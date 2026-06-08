import os
import socket
import struct
import subprocess
import sys
import threading
import time


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


def compressed_test1_query(query_id):
    return (
        struct.pack("!HHHHHH", query_id, 0x0100, 1, 0, 0, 0)
        + b"\xc0\x12"
        + struct.pack("!HH", 1, 1)
        + b"\x05test1\x00"
    )


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
            return "." if not labels else ".".join(labels)
        if length & 0xC0:
            return "<compressed>"
        pos += 1
        labels.append(packet[pos:pos + length].decode("ascii").lower())
        pos += length


def upstream_response(query, ip_bytes):
    query_id = query[:2]
    question_end = read_name(query, 12) + 4
    return (
        query_id
        + struct.pack("!HHHHH", 0x8180, 1, 1, 0, 0)
        + query[12:question_end]
        + struct.pack("!HHHLH", 0xC00C, 1, 1, 30, 4)
        + ip_bytes
    )


class FakeUpstream:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.port = self.sock.getsockname()[1]
        self.count = 0
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
            if question_name(data) == "mismatch.example":
                wrong_query = query_packet(struct.unpack("!H", data[:2])[0], "wrong-answer.example")
                self.sock.sendto(upstream_response(wrong_query, bytes([198, 51, 100, 200])), addr)
                time.sleep(0.05)
            self.sock.sendto(upstream_response(data, bytes([203, 0, 113, 9])), addr)

    def close(self):
        self.stopped.set()
        self.thread.join(timeout=1.0)
        self.sock.close()


def exchange(port, packet, timeout=2.0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    try:
        sock.sendto(packet, ("127.0.0.1", port))
        return sock.recvfrom(512)[0]
    finally:
        sock.close()


def ask(port, query_id, name, qtype=1, timeout=2.0):
    return exchange(port, query_packet(query_id, name, qtype), timeout)


def wait_for_server(proc):
    deadline = time.time() + 3.0
    lines = []
    while time.time() < deadline:
        line = proc.stdout.readline()
        if line:
            lines.append(line)
            if "dnsrelay started" in line:
                return
        if proc.poll() is not None:
            raise RuntimeError("dnsrelay exited early:\n" + "".join(lines))
    raise RuntimeError("dnsrelay did not start:\n" + "".join(lines))


def main():
    exe = sys.argv[1]
    source_dir = sys.argv[2]
    bind_port = choose_port()
    upstream = FakeUpstream()
    upstream.start()
    env = os.environ.copy()
    env["DNSRELAY_BIND_PORT"] = str(bind_port)
    env["DNSRELAY_UPSTREAM_PORT"] = str(upstream.port)

    proc = subprocess.Popen(
        [exe, "-dd", "127.0.0.1", os.path.join(source_dir, "dnsrelay.txt")],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=env,
    )
    try:
        wait_for_server(proc)

        local = ask(bind_port, 0x1111, "test1")
        local_header = struct.unpack("!HHHHHH", local[:12])
        assert local_header[0] == 0x1111 and local_header[1] & 0xF == 0 and local_header[3] == 1
        assert local[-4:] == bytes([11, 111, 11, 111])

        blocked = ask(bind_port, 0x2222, "www.5dsoft.com")
        blocked_header = struct.unpack("!HHHHHH", blocked[:12])
        assert blocked_header[0] == 0x2222 and blocked_header[1] & 0xF == 3 and blocked_header[3] == 0

        blocked_aaaa = ask(bind_port, 0x2225, "www.5dsoft.com", qtype=28)
        blocked_aaaa_header = struct.unpack("!HHHHHH", blocked_aaaa[:12])
        assert blocked_aaaa_header[0] == 0x2225 and blocked_aaaa_header[1] & 0xF == 3 and blocked_aaaa_header[3] == 0
        assert upstream.count == 0

        compressed = exchange(bind_port, compressed_test1_query(0x2223))
        compressed_header = struct.unpack("!HHHHHH", compressed[:12])
        assert compressed_header[0] == 0x2223 and compressed_header[1] & 0xF == 0 and compressed_header[3] == 1
        assert compressed[12] == 5 and compressed[13:18] == b"test1" and compressed[-4:] == bytes([11, 111, 11, 111])

        root = ask(bind_port, 0x2224, ".")
        root_header = struct.unpack("!HHHHHH", root[:12])
        assert root_header[0] == 0x2224 and root_header[1] & 0xF == 0 and root_header[3] == 1
        assert upstream.count == 1

        relayed = ask(bind_port, 0x3333, "relay-only.example")
        relayed_header = struct.unpack("!HHHHHH", relayed[:12])
        assert relayed_header[0] == 0x3333 and relayed_header[1] & 0xF == 0 and relayed[-4:] == bytes([203, 0, 113, 9])
        assert upstream.count == 2

        mismatch = ask(bind_port, 0x3335, "mismatch.example")
        mismatch_header = struct.unpack("!HHHHHH", mismatch[:12])
        assert mismatch_header[0] == 0x3335 and mismatch_header[1] & 0xF == 0
        assert question_name(mismatch) == "mismatch.example"
        assert mismatch[-4:] == bytes([203, 0, 113, 9])
        assert upstream.count == 3

        cached = ask(bind_port, 0x3334, "relay-only.example")
        cached_header = struct.unpack("!HHHHHH", cached[:12])
        assert cached_header[0] == 0x3334 and cached[-4:] == bytes([203, 0, 113, 9])
        time.sleep(0.2)
        assert upstream.count == 3
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2.0)
        upstream.close()


if __name__ == "__main__":
    main()
