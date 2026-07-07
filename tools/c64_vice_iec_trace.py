#!/usr/bin/env python3
"""VICE IEC trace: run MM2-1 T19 loader, break on $DD00, log PC/ports/vectors.

Usage:
  python tools/c64_vice_iec_trace.py
  python tools/c64_vice_iec_trace.py --entry 0xb08 --seconds 120
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
CHAIN = ROOT / "EXTRACTED/c64/emulator/mm2-1_t19s0_chain.bin"
D64 = ROOT / "EXTRACTED/c64/MM2-1.D64"
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
            for sub in (name, name.replace("bin", "")):
                cand = pkg / name / "bin" / "x64sc.exe"
                if not cand.exists():
                    cand = pkg / name / "x64sc.exe"
                if cand.exists():
                    return cand, cand.parent
    raise FileNotFoundError("x64sc.exe not found")


def make_prg(payload: bytes, load: int = 0x0801) -> bytes:
    end = load + len(payload)
    return bytes([load & 0xFF, load >> 8, end & 0xFF, end >> 8]) + payload


def write_stage_bins(out: Path, chain: bytes) -> tuple[Path, Path]:
    iec = chain[0x86B - 0x801 : 0x86B - 0x801 + 0x28]
    # Decomp routine must start at SEI ($0847); $0845..$0846 are JMP tail bytes.
    loader = chain[0x847 - 0x801 : 0x847 - 0x801 + 0xE0]
    p400 = out / "stage_0400.prg"
    p3b7 = out / "stage_03b7.prg"
    p400.write_bytes(make_prg(iec, 0x0400))
    p3b7.write_bytes(make_prg(loader, 0x03B7))
    return p400, p3b7


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
                except Exception:
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
            except socket.timeout:
                break
            except OSError:
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
    ap.add_argument("--stage", action="store_true",
                    help="Load IEC@$0400 + loader@$03B7, run from $03B7")
    ap.add_argument("--skip-decomp", action="store_true",
                    help="Jump to $03D4 (post-decomp exit) instead of running $03B7")
    ap.add_argument("--entry", type=lambda x: int(x, 0), default=0x081D,
                    help="6502 entry (081D init; 081E is LDA #$93 immediate → JAM)")
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--port", type=int, default=6510)
    args = ap.parse_args()

    RAM_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    prg = OUT / "mm2-1_loader.prg"
    prg.write_bytes(make_prg(CHAIN.read_bytes()))
    prg_posix = prg.as_posix()
    stage_400 = OUT / "stage_0400.prg"
    stage_3b7 = OUT / "stage_03b7.prg"
    if args.stage:
        write_stage_bins(OUT, CHAIN.read_bytes())
        if args.skip_decomp:
            args.entry = 0x03D4
        elif args.entry in (0x081D, 0x081E):
            args.entry = 0x03B7
    ram_out = (RAM_DIR / "mm2-1_iec_trace.bin").as_posix()
    json_out = LOG_DIR / "iec_trace.json"
    log_txt = LOG_DIR / "iec_trace.log"

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

    try:
        mon = Monitor(args.port)
        mon.connect()
        log_lines.append("connected")

        for cmd in (
            "reset 0",
            f'l "{prg_posix}" 0',
        ):
            r = mon.send(cmd, 0.7)
            log_lines.append(f">>> {cmd}\n{r}")

        if args.stage:
            for path in (stage_400, stage_3b7):
                r = mon.send(f'l "{path.as_posix()}" 0', 0.8)
                log_lines.append(f">>> load stage {path.name}\n{r}")

        for cmd in ("break dd00",):
            r = mon.send(cmd, 0.7)
            log_lines.append(f">>> {cmd}\n{r}")

        r = mon.send(f"goto {args.entry:04X}", 1.0)
        log_lines.append(f">>> goto {args.entry:04X}\n{r}")
        pc_check = parse_pc(r)
        if pc_check:
            log_lines.append(f"PC after goto: ${pc_check}")

        if pc_check and pc_check.startswith("DD"):
            dd00_hits += 1
            log_lines.append(f"--- initial DD00 stop PC=${pc_check} ---")

        if not pc_check or pc_check.startswith("E5"):
            mon.send(f"#{args.entry:04X}", 0.4)
            mon.send("goto", 0.5)
        t0 = time.time()
        last_pc = parse_pc(log_lines[-1])

        while time.time() - t0 < args.seconds:
            # Advance emulation in chunks.
            r = mon.send("step 2000", 1.2)
            pc = parse_pc(r)
            if not pc:
                r2 = mon.send("r", 0.3)
                pc = parse_pc(r2)
                r = r + r2

            if pc and pc not in seen_pcs:
                seen_pcs.add(pc)
                events.append({"t": round(time.time() - t0, 2), "pc": pc, "kind": "pc"})
                log_lines.append(f"PC=${pc}")

            if pc.startswith("DD") or pc.startswith("DC") or pc.startswith("DE"):
                dd00_hits += 1
                snap = {
                    "t": round(time.time() - t0, 2),
                    "pc": pc,
                    "kind": "dd00",
                    "dd00": mon.send("m dd00", 0.25),
                    "dd01": mon.send("m dd01", 0.2),
                    "zp02a7": mon.send("m 02a7", 0.2),
                    "fc": mon.send("m fc", 0.2),
                }
                events.append(snap)
                log_lines.append(f"--- DD00 #{dd00_hits} PC=${pc} ---")
                mon.send("goto", 0.4)

            # Stuck at entry — nudge with goto.
            if pc in ("081E", "0810", "0805") and time.time() - t0 > 5:
                mon.send("goto", 0.5)

        final_r = mon.send("r", 0.3)
        final_pc = parse_pc(final_r)
        mon.send(f'save "{ram_out}" 0 $0800 $ffff', 3.0)

        report = {
            "entry": f"${args.entry:04X}",
            "stage": args.stage,
            "skip_decomp": args.skip_decomp,
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

    print(f"DD00 hits: {dd00_hits}, unique PCs: {len(seen_pcs)}, final PC: ${final_pc}")
    print(f"Log: {json_out}")
    print(f"RAM: {RAM_DIR / 'mm2-1_iec_trace.bin'}")


if __name__ == "__main__":
    main()
