#!/usr/bin/env python3
"""Drive the booted Crazy crack: inject keys, sample screen, dump RAM.

Leaves VICE running. Usage: c64_crack_drive.py <tag> [keys]
"""
import socket
import sys
import time
from pathlib import Path

TAG = sys.argv[1] if len(sys.argv) > 1 else "state"
KEYS = sys.argv[2] if len(sys.argv) > 2 else ""
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

if KEYS:
    # keybuf injects into the KERNAL keyboard buffer
    send(f'keybuf {KEYS}', 0.4)
    send("x", 0.3)          # exit monitor -> resume emulation
    time.sleep(6)           # let it run (warp)
    # re-enter monitor
    s.sendall(b"\r\n")
    time.sleep(0.5)

reg = send("r", 0.4)
scr = send("m 0400 07e7", 0.6)
vic = send("m d011 d018", 0.4)
full = (OUT / f"{TAG}_full.bin").as_posix()
save = send(f'save "{full}" 0 $0000 $ffff', 3.5)

report = OUT / f"{TAG}.txt"
report.write_text(
    f"=== reg ===\n{reg}\n=== screen ===\n{scr}\n=== vic ===\n{vic}\n=== save ===\n{save}\n",
    encoding="utf-8",
)
s.close()
pc = reg.split("(C:$")[1][:4] if "(C:$" in reg else "?"
print(f"PC=${pc}  report={report}")
