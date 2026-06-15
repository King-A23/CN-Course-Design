#!/usr/bin/env python3
import os
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DNSRELAY = ROOT / "build" / "dnsrelay"
TABLE = ROOT / "dnsrelay.txt"


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


def upstream_response(query, ip_bytes, ttl=30):
    question_end = read_name(query, 12) + 4
    return (
        query[:2]
        + struct.pack("!HHHHH", 0x8180, 1, 1, 0, 0)
        + query[12:question_end]
        + struct.pack("!HHHLH", 0xC00C, 1, 1, ttl, 4)
        + ip_bytes
    )


class FakeUpstream:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.port = self.sock.getsockname()[1]
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
            qname = question_name(data)
            print(f"[fake-upstream] query {qname}", flush=True)
            self.sock.sendto(upstream_response(data, bytes([203, 0, 113, 9])), addr)

    def close(self):
        self.stopped.set()
        self.thread.join(timeout=1.0)
        self.sock.close()


def main():
    if not DNSRELAY.exists():
        print("build/dnsrelay does not exist. Run: cmake -S . -B build -DDNSRELAY_DEV_PORT=8053 && cmake --build build", file=sys.stderr)
        return 1

    upstream = FakeUpstream()
    upstream.start()

    env = os.environ.copy()
    env["DNSRELAY_BIND_PORT"] = env.get("DNSRELAY_BIND_PORT", "8053")
    env["DNSRELAY_UPSTREAM_PORT"] = str(upstream.port)

    print(f"[demo] dnsrelay bind port: {env['DNSRELAY_BIND_PORT']}", flush=True)
    print(f"[demo] fake upstream: 127.0.0.1:{upstream.port}", flush=True)
    print("[demo] query from another terminal, for example:", flush=True)
    print(f"       dig @127.0.0.1 -p {env['DNSRELAY_BIND_PORT']} relay-only.example A +comments +noall +answer", flush=True)
    print("[demo] press Ctrl+C here to stop", flush=True)

    proc = subprocess.Popen(
        [str(DNSRELAY), "-dd", "127.0.0.1", str(TABLE)],
        cwd=str(ROOT),
        env=env,
    )
    try:
        return proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2)
        return 0
    finally:
        upstream.close()


if __name__ == "__main__":
    raise SystemExit(main())
