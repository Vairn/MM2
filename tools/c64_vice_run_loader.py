#!/usr/bin/env python3
"""Boot MM2-1 via extracted T19 loader chain inside VICE (no KERNAL autostart).

Loads the sector chain at $0801, runs entry $0810, breaks on $DD00 IEC access,
then saves RAM $0800-$FFFF.

Usage:
  python tools/c64_vice_run_loader.py
  python tools/c64_vice_run_loader.py --seconds 120 --entry 0x810
"""
from __future__ import annotations

import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHAIN = ROOT / "EXTRACTED/c64/emulator/mm2-1_t19s0_chain.bin"
D64 = ROOT / "EXTRACTED/c64/MM2-1.D64"
OUT_DIR = ROOT / "EXTRACTED/c64/emulator"
RAM_DIR = OUT_DIR / "ram"
LOG_DIR = OUT_DIR / "logs"


def find_x64sc() -> tuple[Path, Path]:
    import shutil

    cmd = shutil.which("x64sc")
    if cmd:
        p = Path(cmd)
        return p, p.parent
    base = Path.home() / "AppData/Local/Microsoft/WinGet/Packages"
    for name in ("GTK3VICE-3.10-win64", "SDL2VICE-3.10-win64"):
        for pkg in base.glob(f"VICE-Team.VICE.*"):
            cand = pkg / name / "bin" / "x64sc.exe"
            if not cand.exists():
                cand = pkg / name / "x64sc.exe"
            if cand.exists():
                return cand, cand.parent
    raise FileNotFoundError("x64sc.exe not found (install winget VICE-Team.VICE.GTK3)")


class RemoteMonitor:
    def __init__(self, host: str = "127.0.0.1", port: int = 6510) -> None:
        self.host, self.port = host, port
        self.sock: socket.socket | None = None

    def connect(self, timeout: float = 60.0) -> None:
        deadline = time.time() + timeout
        last_err: Exception | None = None
        while time.time() < deadline:
            try:
                self.sock = socket.create_connection((self.host, self.port), timeout=2)
                self.sock.settimeout(2)
                try:
                    self.sock.recv(8192)
                except Exception:
                    pass
                return
            except Exception as e:
                last_err = e
                time.sleep(0.25)
        raise TimeoutError(f"remote monitor not ready: {last_err}")

    def cmd(self, line: str, wait: float = 0.4) -> str:
        assert self.sock
        self.sock.sendall((line.rstrip() + "\r\n").encode())
        time.sleep(wait)
        chunks: list[bytes] = []
        while True:
            try:
                c = self.sock.recv(65536)
                if not c:
                    break
                chunks.append(c)
            except Exception:
                break
        return b"".join(chunks).decode("utf-8", "replace")

    def close(self) -> None:
        if self.sock:
            try:
                self.sock.sendall(b"quit\r\n")
            except Exception:
                pass
            self.sock.close()
            self.sock = None


def make_prg(payload: bytes, load_addr: int = 0x0801) -> bytes:
    end = load_addr + len(payload)
    return bytes([load_addr & 0xFF, load_addr >> 8, end & 0xFF, end >> 8]) + payload


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=90, help="Run time before RAM save")
    ap.add_argument("--entry", type=lambda x: int(x, 0), default=0x081D,
                    help="6502 entry (081D init; 081E is LDA #$93 immediate → JAM)")
    ap.add_argument("--p", type=lambda x: int(x, 0), default=0x40,
                    help="P register at entry (0x40 = V set for 0810 path)")
    ap.add_argument("--port", type=int, default=6510)
    args = ap.parse_args()

    if not CHAIN.exists():
        raise SystemExit(f"Missing chain blob — run c64_extract_blob first: {CHAIN}")
    if not D64.exists():
        raise SystemExit(f"Missing {D64}")

    RAM_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    payload = CHAIN.read_bytes()
    prg = make_prg(payload)
    prg_path = OUT_DIR / "mm2-1_loader.prg"
    prg_path.write_bytes(prg)
    prg_posix = prg_path.as_posix()

    ram_out = (RAM_DIR / "mm2-1_loader_run.bin").as_posix()
    log_path = LOG_DIR / "loader_trace.log"
    mon_path = OUT_DIR / "scripts" / "loader_run.mon"
    mon_path.parent.mkdir(parents=True, exist_ok=True)
    mon_path.write_text("x\n", encoding="ascii")  # leave monitor; breaks handled remotely

    x64sc, vice_dir = find_x64sc()
    d64_posix = D64.as_posix()

    vice_args = [
        str(x64sc),
        "-8",
        d64_posix,
        "-drive8type",
        "1541",
        "-drive8truedrive",
        "+sound",
        "-warp",
        "-moncommands",
        str(mon_path),
        "-remotemonitor",
        "-remotemonitoraddress",
        f"127.0.0.1:{args.port}",
        "+confirmonexit",
        "-jamaction",
        "1",
    ]

    print(f"VICE: {x64sc}")
    print(f"PRG:  {prg_path} ({len(prg)} bytes, load $0801, entry ${args.entry:04X})")
    proc = subprocess.Popen(vice_args, cwd=vice_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    log_lines: list[str] = []

    try:
        mon = RemoteMonitor(port=args.port)
        mon.connect(timeout=45)
        log_lines.append("=== connected ===")

        # Reset to clean state, load PRG from host filesystem (device 0).
        for c in ("reset 0", f'l "{prg_posix}" 0'):
            resp = mon.cmd(c, wait=0.8)
            log_lines.append(f">>> {c}\n{resp}")

        reg = f"r pc = ${args.entry:04x}, fl = ${args.p:02x}"
        resp = mon.cmd(reg, wait=0.5)
        log_lines.append(f">>> {reg}\n{resp}")
        mon.cmd("break dd00", wait=0.3)
        log_lines.append(">>> break dd00 set")
        resp = mon.cmd(f"goto {args.entry:04x}", wait=2.0)
        log_lines.append(f">>> goto {args.entry:04x}\n{resp}")

        t0 = time.time()
        dd00_hits = 0
        last_pc = ""
        while time.time() - t0 < args.seconds:
            # Keep CPU running (remote monitor returns after each goto/step batch).
            mon.cmd("step 500", wait=0.8)
            resp = mon.cmd("r", wait=0.2)
            pc = ""
            if "(C:$" in resp:
                try:
                    pc = resp.split("(C:$")[1].split(")")[0].upper()
                except IndexError:
                    pass
            if pc and pc != last_pc:
                log_lines.append(f"PC=${pc}")
                last_pc = pc
            if pc.startswith("DD") or pc.startswith("DC") or pc.startswith("DE"):
                dd00_hits += 1
                port = mon.cmd("m dd00", wait=0.2)
                log_lines.append(f"--- DD00 hit #{dd00_hits} PC=${pc} ---\n{port}")
            time.sleep(0.3)
        log_lines.append(f"Total DD00-region stops: {dd00_hits}")

        save_resp = mon.cmd(f'save "{ram_out}" 0 $0800 $ffff', wait=3.0)
        log_lines.append(f">>> save RAM\n{save_resp}")

        # Sample IEC port and key vectors.
        for addr in ("dd00", "a702", "a704", "01"):
            log_lines.append(f">>> mem {addr}\n{mon.cmd(f'm {addr}', wait=0.3)}")

        mon.close()
    finally:
        time.sleep(1)
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

    log_path.write_text("\n".join(log_lines), encoding="utf-8")
    ram_file = RAM_DIR / "mm2-1_loader_run.bin"
    if ram_file.exists():
        print(f"RAM dump: {ram_file} ({ram_file.stat().st_size} bytes)")
    else:
        print("WARNING: RAM dump missing")
    print(f"Trace log: {log_path}")


if __name__ == "__main__":
    main()
