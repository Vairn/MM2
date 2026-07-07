#!/usr/bin/env python3
"""Capture VICE screenshot + true VIC regs (via I/O bank) of the running crack."""
import socket
import sys
import time
from pathlib import Path

TAG = sys.argv[1] if len(sys.argv) > 1 else "gfx"
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

png = (OUT / f"{TAG}.png").as_posix()
lines = []
lines.append("=== screenshot ===\n" + send(f'screenshot "{png}"', 1.0))
# Force I/O bank so $D0xx / $DDxx read the chips, not RAM under them.
lines.append("=== bank io ===\n" + send("bank io", 0.3))
lines.append("=== VIC $D011-$D02E ===\n" + send("m d011 d02e", 0.6))
lines.append("=== CIA2 $DD00 (VIC bank select) ===\n" + send("m dd00 dd02", 0.4))
lines.append("=== bank ram ===\n" + send("bank ram", 0.3))
(OUT / f"{TAG}_vic.txt").write_text("\n".join(lines), encoding="utf-8")
s.close()
print(f"screenshot={png}")
print("\n".join(lines))
