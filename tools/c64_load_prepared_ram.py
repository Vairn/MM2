#!/usr/bin/env python3
"""Load offline post-decomp RAM into VICE and run from $080B (or $03D4).

Avoids slow in-emulator decompression: uses `c64_prepare_memory.py` output plus
the real T18S1 IEC stub at $0400 (not the pre-decomp copy at $086B).

Usage:
  python tools/c64_prepare_memory.py
  python tools/c64_load_prepared_ram.py
  python tools/c64_load_prepared_ram.py --entry 080b --seconds 90
"""
from __future__ import annotations

import argparse
import json
import re
import socket
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
D64 = ROOT / "EXTRACTED/c64/MM2-1.D64"
PREPARED = ROOT / "EXTRACTED/c64/emulator/ram/mm2-1_prepared_0800.bin"
T18 = ROOT / "EXTRACTED/c64/asm/t18_loader_mm2-1.bin"
OUT = ROOT / "EXTRACTED/c64/emulator"
RAM_DIR = OUT / "ram"
LOG_DIR = OUT / "logs"


def find_x64sc() -> tuple[Path, Path]:
    import shutil

    cmd = shutil.which("x64sc")
    if cmd:
        p = Path(cmd)
        return p, p.parent
    base = Path.home() / "AppData/Local/Microsoft/WinGet/Packages"
    for name in ("GTK3VICE-3.10-win64", "SDL2VICE-3.10-win64"):
        for pkg in base.glob("VICE-Team.VICE.*"):
            for sub in (name, f"{name}/bin"):
                cand = pkg / sub / "x64sc.exe"
                if cand.exists():
                    return cand, cand.parent
    raise FileNotFoundError("x64sc.exe not found")


def make_prg(payload: bytes, load: int) -> bytes:
    end = (load + len(payload)) & 0x10000
    end16 = end & 0xFFFF
    return bytes([load & 0xFF, load >> 8, end16 & 0xFF, end16 >> 8]) + payload


def prg_chunks(payload: bytes, load: int, max_chunk: int = 0x7800) -> list[tuple[int, bytes]]:
    """Split large RAM images into PRG segments (VICE end address is u16)."""
    chunks: list[tuple[int, bytes]] = []
    off = 0
    addr = load
    while off < len(payload):
        n = min(max_chunk, len(payload) - off)
        chunks.append((addr, payload[off : off + n]))
        off += n
        addr += n
    return chunks


def parse_pc(text: str) -> str:
    m = re.search(r"\(C:\$([0-9a-fA-F]+)\)", text)
    return m.group(1).upper() if m else ""


class Monitor:
    def __init__(self, port: int) -> None:
        self.port = port
        self.sock: socket.socket | None = None

    def connect(self, timeout: float = 45.0) -> None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=2)
                self.sock.settimeout(3)
                try:
                    self.sock.recv(8192)
                except OSError:
                    pass
                return
            except OSError:
                time.sleep(0.2)
        raise TimeoutError("remote monitor not ready")

    def send(self, line: str, wait: float = 0.5) -> str:
        assert self.sock
        self.sock.sendall((line.strip() + "\r\n").encode())
        time.sleep(wait)
        buf = b""
        while True:
            try:
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                buf += chunk
            except (socket.timeout, OSError):
                break
        return buf.decode("utf-8", "replace")

    def close(self) -> None:
        if self.sock:
            try:
                self.sock.sendall(b"quit\r\n")
            except OSError:
                pass
            self.sock.close()
            self.sock = None


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--prepared", type=Path, default=PREPARED)
    ap.add_argument("--entry", type=lambda x: int(x, 0), default=0x080D,
                    help="Post-decomp entry ($080D if PRG-loaded; $080B in offline image)")
    ap.add_argument("--seconds", type=int, default=90)
    ap.add_argument("--port", type=int, default=6510)
    ap.add_argument("--no-iec", action="store_true", help="Skip T18 IEC stub @ $0400")
    args = ap.parse_args()

    if not args.prepared.exists():
        raise SystemExit(f"Missing {args.prepared} — run c64_prepare_memory.py first")

    RAM_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    ram_payload = args.prepared.read_bytes()
    if len(ram_payload) != 0xF800:
        raise SystemExit(f"Expected 63488 bytes ($0800-$FFFF), got {len(ram_payload)}")

    prg_paths: list[Path] = []
    for i, (addr, chunk) in enumerate(prg_chunks(ram_payload, 0x0800)):
        p = OUT / f"_prepared_{addr:04X}.prg"
        p.write_bytes(make_prg(chunk, addr))
        prg_paths.append(p)

    prg_iec: Path | None = None
    if not args.no_iec:
        if not T18.exists():
            raise SystemExit(f"Missing {T18}")
        # T18S1 IEC bit-bang (254-byte payload, org $0400 when on disk track 18)
        iec = T18.read_bytes()[:254]
        prg_iec = OUT / "_iec_0400.prg"
        prg_iec.write_bytes(make_prg(iec, 0x0400))

    ram_out = (RAM_DIR / "mm2-1_prepared_run.bin").as_posix()
    json_out = LOG_DIR / "prepared_run.json"
    log_txt = LOG_DIR / "prepared_run.log"

    x64sc, vice_dir = find_x64sc()
    mon_exit = OUT / "scripts" / "mon_exit.mon"
    mon_exit.parent.mkdir(parents=True, exist_ok=True)
    mon_exit.write_text("x\n", encoding="ascii")

    proc = subprocess.Popen(
        [
            str(x64sc),
            "-8",
            D64.as_posix(),
            "-drive8type",
            "1541",
            "-drive8truedrive",
            "+sound",
            "-warp",
            "-moncommands",
            str(mon_exit),
            "-remotemonitor",
            "-remotemonitoraddress",
            f"127.0.0.1:{args.port}",
            "+confirmonexit",
            "-jamaction",
            "1",
        ],
        cwd=vice_dir,
    )

    events: list[dict] = []
    log_lines: list[str] = []
    dd00_hits = 0
    seen_pcs: set[str] = set()
    final_pc = ""

    try:
        mon = Monitor(args.port)
        mon.connect()
        log_lines.append("connected")

        r = mon.send("reset 0", 0.8)
        log_lines.append(f">>> reset 0\n{r}")
        for p in prg_paths:
            cmd = f'l "{p.as_posix()}" 0'
            r = mon.send(cmd, 0.8)
            log_lines.append(f">>> {cmd}\n{r}")

        if prg_iec:
            r = mon.send(f'l "{prg_iec.as_posix()}" 0', 0.8)
            log_lines.append(f">>> load IEC @ $0400\n{r}")

        for cmd in ("break dd00", "break e3bf", "break 0100", "break 0400"):
            r = mon.send(cmd, 0.4)
            log_lines.append(f">>> {cmd}\n{r}")

        r = mon.send(f"goto {args.entry:04X}", 1.0)
        log_lines.append(f">>> goto {args.entry:04X}\n{r}")

        t0 = time.time()
        while time.time() - t0 < args.seconds:
            r = mon.send("goto", 1.0)
            pc = parse_pc(r)
            if not pc:
                pc = parse_pc(mon.send("r", 0.25))

            if pc and pc not in seen_pcs:
                seen_pcs.add(pc)
                events.append({"t": round(time.time() - t0, 2), "pc": pc})
                log_lines.append(f"PC=${pc}")

            if pc.startswith(("DD", "DC", "DE")):
                dd00_hits += 1
                snap = {
                    "t": round(time.time() - t0, 2),
                    "pc": pc,
                    "kind": "dd00",
                    "dd00": mon.send("m dd00", 0.2),
                    "02a7": mon.send("m 02a7", 0.2),
                    "00cb": mon.send("m 00cb", 0.2),
                    "4546": mon.send("m 4546", 0.2),
                }
                events.append(snap)
                log_lines.append(f"--- DD00 #{dd00_hits} ---")
                mon.send("goto", 0.4)

            if pc in ("0100", "0101", "0102", "081F", "080B"):
                log_lines.append(f"--- reached $0100 stub ---\n{mon.send('m 0100', 0.3)}")

        final_r = mon.send("r", 0.3)
        final_pc = parse_pc(final_r)
        low_ram_out = (RAM_DIR / "mm2-1_prepared_run_low.bin").as_posix()
        mon.send(f'save "{low_ram_out}" 0 $0000 $07ff', 2.0)
        mon.send(f'save "{ram_out}" 0 $0800 $ffff', 3.0)

        report = {
            "entry": args.entry,
            "iec_at_0400": not args.no_iec,
            "seconds": args.seconds,
            "dd00_hits": dd00_hits,
            "unique_pcs": len(seen_pcs),
            "final_pc": f"${final_pc}" if final_pc else None,
            "events": events[-200:],
        }
        json_out.write_text(json.dumps(report, indent=2), encoding="utf-8")
        log_txt.write_text("\n".join(log_lines), encoding="utf-8")
        mon.close()
    finally:
        time.sleep(0.5)
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(5)
            except subprocess.TimeoutExpired:
                proc.kill()

    print(f"DD00 hits: {dd00_hits}, unique PCs: {len(seen_pcs)}, final: ${final_pc}")
    print(f"Log: {json_out}")


if __name__ == "__main__":
    main()
