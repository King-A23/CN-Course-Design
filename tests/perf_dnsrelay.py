import argparse
import concurrent.futures
import json
import os
import socket
import struct
import subprocess
import sys
import threading
import time


LOCAL_IP = bytes([11, 111, 11, 111])
UPSTREAM_IP = bytes([203, 0, 113, 42])


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
    packet = bytearray()
    for label in name.rstrip(".").split("."):
        packet.append(len(label))
        packet.extend(label.encode("ascii"))
    packet.append(0)
    return bytes(packet)


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


def upstream_response(query, ip_bytes):
    query_id = query[:2]
    question_end = read_name(query, 12) + 4
    return (
        query_id
        + struct.pack("!HHHHH", 0x8180, 1, 1, 0, 0)
        + query[12:question_end]
        + struct.pack("!HHHLH", 0xC00C, 1, 1, 60, 4)
        + ip_bytes
    )


class BenchmarkError(Exception):
    pass


class FakeUpstream:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.port = self.sock.getsockname()[1]
        self.count = 0
        self.lock = threading.Lock()
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
            with self.lock:
                self.count += 1
            self.sock.sendto(upstream_response(data, UPSTREAM_IP), addr)

    def request_count(self):
        with self.lock:
            return self.count

    def close(self):
        self.stopped.set()
        self.thread.join(timeout=1.0)
        self.sock.close()


def exchange(port, packet, timeout=1.0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    try:
        sock.sendto(packet, ("127.0.0.1", port))
        return sock.recvfrom(512)[0]
    finally:
        sock.close()


def validate_a_response(packet, query_id, expected_ip):
    if len(packet) < 12:
        raise BenchmarkError("response is shorter than DNS header")
    header = struct.unpack("!HHHHHH", packet[:12])
    rcode = header[1] & 0x000F
    if header[0] != query_id:
        raise BenchmarkError(f"response id mismatch: got {header[0]}, expected {query_id}")
    if rcode != 0:
        raise BenchmarkError(f"response rcode is {rcode}, expected NOERROR")
    if header[3] < 1:
        raise BenchmarkError("response has no A answer")
    if packet[-4:] != expected_ip:
        got = ".".join(str(part) for part in packet[-4:])
        expected = ".".join(str(part) for part in expected_ip)
        raise BenchmarkError(f"response IP mismatch: got {got}, expected {expected}")


def wait_for_server(proc, port):
    deadline = time.time() + 3.0
    last_error = None
    while time.time() < deadline:
        if proc.poll() is not None:
            output = proc.stdout.read() if proc.stdout is not None else ""
            raise RuntimeError(f"dnsrelay exited early with code {proc.returncode}:\n{output}")
        try:
            query_id = 0xCAFE
            response = exchange(port, query_packet(query_id, "test1"), timeout=0.05)
            validate_a_response(response, query_id, LOCAL_IP)
            return
        except (BenchmarkError, OSError, socket.timeout) as exc:
            last_error = exc
            time.sleep(0.05)
    raise RuntimeError(f"dnsrelay did not become ready: {last_error}")


def start_dnsrelay(exe, source_dir, bind_port, upstream_port):
    env = os.environ.copy()
    env["DNSRELAY_BIND_PORT"] = str(bind_port)
    env["DNSRELAY_UPSTREAM_PORT"] = str(upstream_port)
    proc = subprocess.Popen(
        [exe, "127.0.0.1", os.path.join(source_dir, "dnsrelay.txt")],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=env,
    )
    wait_for_server(proc, bind_port)
    return proc


def stop_dnsrelay(proc):
    proc.terminate()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2.0)


def percentile(values, percentile_value):
    if not values:
        return 0.0
    ordered = sorted(values)
    rank = int((percentile_value / 100.0) * (len(ordered) - 1))
    return ordered[rank]


def summarize(name, requests, clients, duration_s, latencies_ms, upstream_delta):
    return {
        "scenario": name,
        "requests": requests,
        "clients": clients,
        "duration_s": duration_s,
        "qps": requests / duration_s if duration_s > 0 else 0.0,
        "avg_ms": sum(latencies_ms) / len(latencies_ms),
        "p50_ms": percentile(latencies_ms, 50),
        "p95_ms": percentile(latencies_ms, 95),
        "p99_ms": percentile(latencies_ms, 99),
        "max_ms": max(latencies_ms),
        "upstream_requests": upstream_delta,
    }


def run_worker(port, worker_id, total_requests, clients, qname_for, expected_ip, timeout):
    latencies = []
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    try:
        for index in range(worker_id, total_requests, clients):
            query_id = (index % 65535) + 1
            packet = query_packet(query_id, qname_for(index))
            start = time.perf_counter()
            sock.sendto(packet, ("127.0.0.1", port))
            response = sock.recvfrom(512)[0]
            latencies.append((time.perf_counter() - start) * 1000.0)
            validate_a_response(response, query_id, expected_ip)
    finally:
        sock.close()
    return latencies


def run_scenario(name, port, upstream, requests, clients, qname_for, expected_ip, timeout):
    before = upstream.request_count()
    start = time.perf_counter()
    latencies = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=clients) as executor:
        futures = [
            executor.submit(run_worker, port, worker_id, requests, clients, qname_for, expected_ip, timeout)
            for worker_id in range(clients)
        ]
        for future in concurrent.futures.as_completed(futures):
            latencies.extend(future.result())
    duration_s = time.perf_counter() - start
    after = upstream.request_count()
    if len(latencies) != requests:
        raise BenchmarkError(f"{name}: got {len(latencies)} successful responses, expected {requests}")
    return summarize(name, requests, clients, duration_s, latencies, after - before)


def print_table(results):
    headers = (
        "scenario",
        "requests",
        "clients",
        "qps",
        "avg_ms",
        "p50_ms",
        "p95_ms",
        "p99_ms",
        "max_ms",
        "upstream",
    )
    print(
        f"{headers[0]:<18} {headers[1]:>8} {headers[2]:>7} {headers[3]:>10} "
        f"{headers[4]:>9} {headers[5]:>9} {headers[6]:>9} {headers[7]:>9} "
        f"{headers[8]:>9} {headers[9]:>9}"
    )
    for result in results:
        print(
            f"{result['scenario']:<18} {result['requests']:>8} {result['clients']:>7} "
            f"{result['qps']:>10.1f} {result['avg_ms']:>9.3f} {result['p50_ms']:>9.3f} "
            f"{result['p95_ms']:>9.3f} {result['p99_ms']:>9.3f} {result['max_ms']:>9.3f} "
            f"{result['upstream_requests']:>9}"
        )


def check_thresholds(results, min_qps, max_p95_ms):
    failures = []
    for result in results:
        if min_qps > 0.0 and result["qps"] < min_qps:
            failures.append(f"{result['scenario']} qps {result['qps']:.1f} < {min_qps:.1f}")
        if max_p95_ms > 0.0 and result["p95_ms"] > max_p95_ms:
            failures.append(f"{result['scenario']} p95 {result['p95_ms']:.3f} ms > {max_p95_ms:.3f} ms")
    if failures:
        raise BenchmarkError("; ".join(failures))


def parse_args():
    parser = argparse.ArgumentParser(description="Run dnsrelay performance scenarios against a local fake upstream.")
    parser.add_argument("dnsrelay_exe", help="Path to the dnsrelay executable")
    parser.add_argument("source_dir", help="Repository source directory containing dnsrelay.txt")
    parser.add_argument("--requests", type=int, default=1000, help="Requests per local/cache scenario")
    parser.add_argument("--relay-requests", type=int, default=300, help="Requests for the relay-miss scenario")
    parser.add_argument("--clients", type=int, default=8, help="Concurrent client sockets")
    parser.add_argument("--timeout", type=float, default=2.0, help="Per-query timeout in seconds")
    parser.add_argument("--min-qps", type=float, default=0.0, help="Optional minimum QPS threshold for every scenario")
    parser.add_argument("--max-p95-ms", type=float, default=0.0, help="Optional maximum p95 latency threshold for every scenario")
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON instead of a table")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.requests <= 0 or args.relay_requests <= 0 or args.clients <= 0:
        raise BenchmarkError("requests, relay-requests, and clients must be positive")

    bind_port = choose_port()
    upstream = FakeUpstream()
    upstream.start()
    proc = start_dnsrelay(args.dnsrelay_exe, args.source_dir, bind_port, upstream.port)
    try:
        warm_id = 0xBEEF
        warm_response = exchange(bind_port, query_packet(warm_id, "perf-cache.example"), timeout=args.timeout)
        validate_a_response(warm_response, warm_id, UPSTREAM_IP)

        results = [
            run_scenario(
                "local_table_hit",
                bind_port,
                upstream,
                args.requests,
                args.clients,
                lambda _index: "test1",
                LOCAL_IP,
                args.timeout,
            ),
            run_scenario(
                "cache_hit",
                bind_port,
                upstream,
                args.requests,
                args.clients,
                lambda _index: "perf-cache.example",
                UPSTREAM_IP,
                args.timeout,
            ),
            run_scenario(
                "relay_miss",
                bind_port,
                upstream,
                args.relay_requests,
                args.clients,
                lambda index: f"relay-{index}.perf.example",
                UPSTREAM_IP,
                args.timeout,
            ),
        ]
        check_thresholds(results, args.min_qps, args.max_p95_ms)
        if args.json:
            print(json.dumps(results, indent=2, sort_keys=True))
        else:
            print_table(results)
    finally:
        stop_dnsrelay(proc)
        upstream.close()


if __name__ == "__main__":
    main()
