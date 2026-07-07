#!/usr/bin/env python3
"""Automated VICE boot for MM2-1 C64: breaks, region samples, full RAM save.

Usage:
  python tools/c64_vice_full_boot.py --entry 0x081D --seconds 600 --tag chain
  python tools/c64_vice_full_boot.py --prepared --seconds 90 --tag prepared
  python tools/c64_vice_full_boot.py --t18 --seconds 120 --tag t18
  python tools/c64_vice_full_boot.py --stage --seconds 180 --tag stage
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
PREPARED = ROOT / "EXTRACTED/c64/emulator/ram/mm2-1_prepared_0800.bin"
T18 = ROOT / "EXTRACTED/c64/asm/t18_loader_mm2-1.bin"
OUT = ROOT / "EXTRACTED/c64/emulator"
RAM_DIR = OUT / "ram"
LOG_DIR = OUT / "logs"

BREAKS = ("ffd5", "dd00", "e3bf", "0100", "0400", "0334")
SAMPLE_ADDRS = ("4546", "00cb", "0100", "02a7", "02a9", "32", "0334")


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
    chunks: list[tuple[int, bytes]] = []
    off = 0
    addr = load
    while off < len(payload):
        n = min(max_chunk, len(payload) - off)
        chunks.append((addr, payload[off : off + n]))
        off += n
        addr += n
    return chunks


def write_stage_bins(out: Path, chain: bytes) -> tuple[Path, Path]:
    iec = chain[0x86B - 0x801 : 0x86B - 0x801 + 0x28]
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


def sample_regions(mon: Monitor) -> dict[str, str]:
    return {a: mon.send(f"m {a}", 0.15) for a in SAMPLE_ADDRS}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--prepared", action="store_true", help="Inject offline decomp RAM + T18 IEC")
    ap.add_argument("--t18", action="store_true", help="Prepared RAM then goto $0421 (T18S16 SETNAM path)")
    ap.add_argument("--stage", action="store_true", help="Chain + IEC@$0400 + loader@$03B7")
    ap.add_argument("--entry", type=lambda x: int(x, 0), default=0x081D)
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--tag", default="run")
    ap.add_argument("--port", type=int, default=6510)
    ap.add_argument("--sample-every", type=int, default=30, help="Region sample interval (seconds)")
    args = ap.parse_args()

    RAM_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    loads: list[Path] = []
    entry = args.entry
    mode = "chain"

    if args.prepared or args.t18:
        mode = "t18" if args.t18 else "prepared"
        if not PREPARED.exists():
            raise SystemExit(f"Missing {PREPARED} — run c64_prepare_memory.py")
        ram_payload = PREPARED.read_bytes()
        for addr, chunk in prg_chunks(ram_payload, 0x0800):
            p = OUT / f"_fullboot_{addr:04X}.prg"
            p.write_bytes(make_prg(chunk, addr))
            loads.append(p)
        if T18.exists():
            p = OUT / "_fullboot_iec_0400.prg"
            p.write_bytes(make_prg(T18.read_bytes()[:254], 0x0400))
            loads.append(p)
        entry = 0x0421 if args.t18 else 0x080D
    elif args.stage:
        mode = "stage"
        if not CHAIN.exists():
            raise SystemExit(f"Missing {CHAIN}")
        prg = OUT / "mm2-1_loader.prg"
        prg.write_bytes(make_prg(CHAIN.read_bytes()))
        loads.append(prg)
        loads.extend(write_stage_bins(OUT, CHAIN.read_bytes()))
        entry = 0x03B7
    else:
        if not CHAIN.exists():
            raise SystemExit(f"Missing {CHAIN}")
        prg = OUT / "mm2-1_loader.prg"
        prg.write_bytes(make_prg(CHAIN.read_bytes()))
        loads.append(prg)

    tag = args.tag or mode
    json_out = LOG_DIR / f"full_boot_{tag}.json"
    log_txt = LOG_DIR / f"full_boot_{tag}.log"
    ram_hi = RAM_DIR / f"mm2-1_fullboot_{tag}_hi.bin"
    ram_low = RAM_DIR / f"mm2-1_fullboot_{tag}_low.bin"

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
    dd00_hits = ffd5_hits = 0
    seen_pcs: set[str] = set()
    final_pc = ""

    try:
        mon = Monitor(args.port)
        mon.connect()
        log_lines.append(f"mode={mode} entry=${entry:04X}")

        mon.send("reset 0", 0.8)
        for p in loads:
            cmd = f'l "{p.as_posix()}" 0'
            r = mon.send(cmd, 0.8)
            log_lines.append(f">>> {cmd}\n{r[:400]}")

        for bp in BREAKS:
            mon.send(f"break {bp}", 0.25)

        r = mon.send(f"goto {entry:04X}", 1.0)
        log_lines.append(f">>> goto {entry:04X}\n{r[:400]}")

        t0 = time.time()
        last_sample = t0
        while time.time() - t0 < args.seconds:
            r = mon.send("goto", 1.0)
            pc = parse_pc(r) or parse_pc(mon.send("r", 0.2))

            if pc and pc not in seen_pcs:
                seen_pcs.add(pc)
                events.append({"t": round(time.time() - t0, 2), "pc": pc})
                log_lines.append(f"PC=${pc}")

            if pc.startswith(("DD", "DC", "DE")):
                dd00_hits += 1
                events.append(
                    {"t": round(time.time() - t0, 2), "kind": "dd00", "pc": pc, **sample_regions(mon)}
                )
                mon.send("goto", 0.3)
            elif pc.startswith("FFD"):
                ffd5_hits += 1
                events.append(
                    {"t": round(time.time() - t0, 2), "kind": "ffd5", "pc": pc, **sample_regions(mon)}
                )
                mon.send("goto", 0.3)

            if time.time() - last_sample >= args.sample_every:
                last_sample = time.time()
                snap = sample_regions(mon)
                events.append({"t": round(time.time() - t0, 2), "kind": "sample", **snap})
                log_lines.append(f"--- sample t={round(time.time()-t0,1)} ---")

        final_pc = parse_pc(mon.send("r", 0.3))
        mon.send(f'save "{ram_low.as_posix()}" 0 $0000 $07ff', 2.0)
        mon.send(f'save "{ram_hi.as_posix()}" 0 $0800 $ffff', 3.0)

        report = {
            "mode": mode,
            "entry": f"${entry:04X}",
            "seconds": args.seconds,
            "dd00_hits": dd00_hits,
            "ffd5_hits": ffd5_hits,
            "unique_pcs": len(seen_pcs),
            "final_pc": f"${final_pc}" if final_pc else None,
            "ram_low": str(ram_low),
            "ram_hi": str(ram_hi),
            "events": events[-300:],
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

    print(f"mode={mode} entry=${entry:04X} DD00={dd00_hits} FFD5={ffd5_hits} final=${final_pc}")
    print(f"Log: {json_out}")


if __name__ == "__main__":
    main()
