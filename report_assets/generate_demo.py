#!/usr/bin/env python3
import html
import json
import os
import socket
import struct
import subprocess
import tempfile
import threading
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "report_assets"
DNSRELAY = ROOT / "build" / "dnsrelay"


def choose_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]
    finally:
        sock.close()


def encode_name(name):
    if name == ".":
        return b"\x00"
    out = bytearray()
    for label in name.rstrip(".").split("."):
        raw = label.encode("ascii")
        out.append(len(raw))
        out.extend(raw)
    out.append(0)
    return bytes(out)


def query_packet(query_id, name, qtype=1, qclass=1):
    return struct.pack("!HHHHHH", query_id, 0x0100, 1, 0, 0, 0) + encode_name(name) + struct.pack("!HH", qtype, qclass)


def read_name(packet, offset):
    pos = offset
    while True:
        length = packet[pos]
        if length == 0:
            return pos + 1
        if length & 0xC0:
            return pos + 2
        pos += 1 + length


def question_name(packet):
    labels = []
    pos = 12
    while True:
        length = packet[pos]
        if length == 0:
            return "." if not labels else ".".join(labels).lower()
        if length & 0xC0:
            return "<compressed>"
        pos += 1
        labels.append(packet[pos:pos + length].decode("ascii", "replace"))
        pos += length


def upstream_response(query, ip_bytes, ttl=30, name_override=None):
    query_id = query[:2]
    if name_override:
        question = encode_name(name_override) + struct.pack("!HH", 1, 1)
    else:
        question_end = read_name(query, 12) + 4
        question = query[12:question_end]
    return (
        query_id
        + struct.pack("!HHHHH", 0x8180, 1, 1, 0, 0)
        + question
        + struct.pack("!HHHLH", 0xC00C, 1, 1, ttl, 4)
        + ip_bytes
    )


class FakeUpstream:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.port = self.sock.getsockname()[1]
        self.count = 0
        self.queries = []
        self.stopped = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self):
        self.thread.start()

    def run(self):
        self.sock.settimeout(0.1)
        while not self.stopped.is_set():
            try:
                data, addr = self.sock.recvfrom(512)
            except socket.timeout:
                continue
            self.count += 1
            qname = question_name(data)
            qid = struct.unpack("!H", data[:2])[0]
            self.queries.append({"id": qid, "qname": qname})
            if qname == "mismatch.example":
                self.sock.sendto(
                    upstream_response(data, bytes([198, 51, 100, 200]), name_override="wrong-answer.example"),
                    addr,
                )
                time.sleep(0.05)
            self.sock.sendto(upstream_response(data, bytes([203, 0, 113, 9])), addr)

    def close(self):
        self.stopped.set()
        self.thread.join(timeout=1.0)
        self.sock.close()


def run_capture(args, env=None, timeout=20):
    result = subprocess.run(
        args,
        cwd=str(ROOT),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    return result.returncode, result.stdout


def write_text(name, text):
    path = ASSETS / name
    path.write_text(text.rstrip() + "\n", encoding="utf-8")
    return path


def terminal_svg(title, text, out_name, width=1180):
    lines = []
    for raw in text.rstrip().splitlines():
        if len(raw) <= 112:
            lines.append(raw)
            continue
        start = 0
        while start < len(raw):
            lines.append(raw[start:start + 112])
            start += 112
    lines = lines[:28]
    line_height = 22
    height = 92 + max(1, len(lines)) * line_height
    body = []
    for index, line in enumerate(lines):
        y = 70 + index * line_height
        body.append(f'<text x="28" y="{y}" class="term">{html.escape(line)}</text>')
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <rect x="0" y="0" width="{width}" height="{height}" rx="14" fill="#1f2329"/>
  <rect x="0" y="0" width="{width}" height="42" rx="14" fill="#303741"/>
  <circle cx="28" cy="21" r="7" fill="#ff5f57"/>
  <circle cx="50" cy="21" r="7" fill="#ffbd2e"/>
  <circle cx="72" cy="21" r="7" fill="#28c840"/>
  <text x="96" y="27" fill="#d7dce2" font-family="Menlo, Consolas, monospace" font-size="15">{html.escape(title)}</text>
  <style>.term {{ fill: #f0f3f6; font-family: Menlo, Consolas, monospace; font-size: 15px; }}</style>
  {''.join(body)}
</svg>
'''
    path = ASSETS / out_name
    path.write_text(svg, encoding="utf-8")
    return path


def dot_escape(text):
    return text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def write_dot_svg(title, nodes, edges, out_name, same_rank_groups=(), invisible_edges=()):
    lines = [
        "digraph G {",
        f'  graph [rankdir=LR, bgcolor="#F8FAFC", pad=0.28, nodesep=0.55, ranksep=0.9, splines=polyline, label="{dot_escape(title)}", labelloc=t, fontname="PingFang SC", fontsize=30, fontcolor="#0F172A"];',
        '  node [shape=box, style="rounded,filled", fillcolor="white", color="#64748B", penwidth=1.5, fontname="PingFang SC", fontsize=18, margin="0.24,0.16"];',
        '  edge [color="#475569", penwidth=1.8, arrowsize=0.8, fontname="PingFang SC", fontsize=14];',
    ]
    for name, label in nodes:
        lines.append(f'  {name} [label="{dot_escape(label)}"];')
    for group in same_rank_groups:
        members = "; ".join(group)
        lines.append(f"  {{ rank=same; {members}; }}")
    for start, end in invisible_edges:
        lines.append(f"  {start} -> {end} [style=invis, weight=8];")
    for edge in edges:
        if len(edge) == 3:
            start, end, label = edge
            extra = ""
        else:
            start, end, label, extra = edge
        attrs = []
        if label:
            attrs.append(f'label="{dot_escape(label)}"')
        if extra:
            attrs.append(extra)
        if attrs:
            lines.append(f"  {start} -> {end} [{', '.join(attrs)}];")
        else:
            lines.append(f"  {start} -> {end};")
    lines.append("}")

    path = ASSETS / out_name
    with tempfile.NamedTemporaryFile("w", suffix=".dot", delete=False, encoding="utf-8") as handle:
        handle.write("\n".join(lines))
        dot_path = Path(handle.name)
    try:
        subprocess.run(
            ["dot", "-Tsvg", str(dot_path), "-o", str(path)],
            cwd=str(ROOT),
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    finally:
        dot_path.unlink(missing_ok=True)
    return path


def main():
    ASSETS.mkdir(exist_ok=True)

    ctest_code, ctest_output = run_capture(["ctest", "--test-dir", "build", "--output-on-failure"], timeout=60)
    write_text("ctest_output.txt", ctest_output)
    terminal_svg("ctest --test-dir build --output-on-failure", ctest_output, "screenshot_ctest.svg")

    upstream = FakeUpstream()
    upstream.start()
    bind_port = choose_port()
    env = os.environ.copy()
    env["DNSRELAY_BIND_PORT"] = str(bind_port)
    env["DNSRELAY_UPSTREAM_PORT"] = str(upstream.port)
    proc = subprocess.Popen(
        [str(DNSRELAY), "-dd", "127.0.0.1", str(ROOT / "dnsrelay.txt")],
        cwd=str(ROOT),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    logs = []
    started = threading.Event()

    def read_logs():
        assert proc.stdout is not None
        for line in proc.stdout:
            logs.append(line.rstrip())
            if "dnsrelay started" in line:
                started.set()

    reader = threading.Thread(target=read_logs, daemon=True)
    reader.start()
    if not started.wait(timeout=5):
        proc.terminate()
        upstream.close()
        raise RuntimeError("dnsrelay did not start")

    def dig(name, qtype="A", status=False):
        args = ["dig", f"@127.0.0.1", "-p", str(bind_port), name, qtype, "+comments"]
        if not status:
            args.extend(["+noall", "+answer"])
        code, output = run_capture(args, timeout=10)
        cmd_text = "$ " + " ".join(args) + "\n" + output
        return code, cmd_text

    time.sleep(0.2)
    _, local_a = dig("test1", "A")
    _, blocked_a = dig("www.5dsoft.com", "A", status=True)
    _, blocked_aaaa = dig("www.5dsoft.com", "AAAA", status=True)
    _, relay_1 = dig("relay-only.example", "A")
    time.sleep(1.1)
    _, relay_2 = dig("relay-only.example", "A")
    _, mismatch = dig("mismatch.example", "A")

    time.sleep(0.4)
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2)
    upstream.close()
    reader.join(timeout=1)

    write_text("local_a_result.txt", local_a)
    write_text("blocked_result.txt", blocked_a + "\n" + blocked_aaaa)
    write_text("relay_cache_result.txt", relay_1 + "\n" + relay_2)
    write_text("mismatch_result.txt", mismatch)
    server_log = "\n".join(logs)
    write_text("server_log_excerpt.txt", server_log)

    terminal_svg("dig: 本地 test1 A 解析", local_a, "screenshot_local_a.svg")
    terminal_svg("dig: 本地 0.0.0.0 拦截 NXDOMAIN", blocked_a + "\n" + blocked_aaaa, "screenshot_blocked.svg")
    terminal_svg("dig: 上游中继与 Cache 命中", relay_1 + "\n" + relay_2, "screenshot_relay_cache.svg")
    terminal_svg("dnsrelay -dd 运行日志", server_log, "screenshot_server_log.svg")

    write_dot_svg(
        "DNS Relay 总体架构",
        [
            ("client", "客户端\nResolver"),
            ("server", "dnsrelay\n单 socket"),
            ("table", "local_table"),
            ("cache", "cache"),
            ("pending", "pending_map"),
            ("upstream", "上游 DNS"),
        ],
        [
            ("client", "server", "UDP 查询/响应"),
            ("server", "table", "本地优先"),
            ("server", "cache", "未命中查缓存"),
            ("server", "pending", "转发前登记"),
            ("pending", "upstream", "改写 ID 转发"),
            ("upstream", "server", "响应恢复 ID"),
        ],
        "flow_architecture.svg",
    )
    write_dot_svg(
        "客户端查询处理流程",
        [
            ("recv", "收到客户端报文"),
            ("parse", "解析 Header\n/ Question"),
            ("local", "本地表命中"),
            ("cache", "Cache 命中"),
            ("alloc", "分配上游 ID"),
            ("reply", "直接返回客户端"),
            ("forward", "转发上游 DNS"),
        ],
        [
            ("recv", "parse", ""),
            ("parse", "local", "A/IN 或拦截"),
            ("parse", "cache", "本地未命中"),
            ("local", "reply", "普通 IP / NXDOMAIN"),
            ("cache", "reply", "命中"),
            ("cache", "alloc", "未命中"),
            ("alloc", "forward", "登记后转发"),
        ],
        "flow_client.svg",
        same_rank_groups=(("recv", "parse"), ("local", "cache", "alloc"), ("reply", "forward")),
        invisible_edges=(("local", "cache"), ("cache", "alloc"), ("reply", "forward")),
    )
    write_dot_svg(
        "上游响应处理流程",
        [
            ("recv", "收到上游响应"),
            ("pending", "按 upstream ID\n查 pending"),
            ("validate", "校验 Question"),
            ("cache", "写入 Cache"),
            ("restore", "恢复 client ID"),
            ("reply", "回发客户端"),
            ("drop", "未知或不匹配\n直接丢弃"),
        ],
        [
            ("recv", "pending", ""),
            ("pending", "validate", "找到"),
            ("pending", "drop", "找不到"),
            ("validate", "cache", "匹配且可缓存"),
            ("validate", "drop", "不匹配"),
            ("cache", "restore", ""),
            ("restore", "reply", "回发客户端"),
        ],
        "flow_upstream.svg",
    )

    summary = {
        "ctest_exit_code": ctest_code,
        "bind_port": bind_port,
        "fake_upstream_port": upstream.port,
        "fake_upstream_count": upstream.count,
        "fake_upstream_queries": upstream.queries,
        "assets": {
            "ctest": "report_assets/screenshot_ctest.svg",
            "local_a": "report_assets/screenshot_local_a.svg",
            "blocked": "report_assets/screenshot_blocked.svg",
            "relay_cache": "report_assets/screenshot_relay_cache.svg",
            "server_log": "report_assets/screenshot_server_log.svg",
            "architecture": "report_assets/flow_architecture.svg",
            "client_flow": "report_assets/flow_client.svg",
            "upstream_flow": "report_assets/flow_upstream.svg",
        },
    }
    (ASSETS / "demo_summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
