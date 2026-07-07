#!/usr/bin/env python3
"""One-shot VICE monitor dump for the booted Crazy crack (no poll loop)."""
import socket
import sys
import time
from pathlib import Path

TAG = sys.argv[1] if len(sys.argv) > 1 else "D1A"
OUT = Path(__file__).resolve().parents[1] / "EXTRACTED/c64/crazy_crack/run"
OUT.mkdir(parents=True, exist_ok=True)

s = socket.create_connection(("127.0.0.1", 6510), timeout=5)
s.settimeout(3)


def send(cmd: str, wait: float = 0.6) -> str:
    s.sendall((cmd + "\r\n").encode())
    time.sleep(wait)
    buf = b""
    while True:
        try:
            c = s.recv(65536)
            if not c:
                break
            buf += c
        except socket.timeout:
            break
    return buf.decode("utf-8", "replace")


try:
    s.recv(8192)
except Exception:
    pass

print("=== registers ===")
print(send("r"))
print("=== screen matrix $0400-$07E7 ===")
print(send("m 0400 07e7"))
print("=== VIC $D011/$D016/$D018 ===")
print(send("m d011 d018"))
full = (OUT / f"{TAG}_full.bin").as_posix()
print(send(f'save "{full}" 0 $0000 $ffff', 3.5))
# Do NOT quit — leave VICE running so we can drive it further.
s.close()
print("done; VICE left running")
