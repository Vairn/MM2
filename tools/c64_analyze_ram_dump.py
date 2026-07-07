#!/usr/bin/env python3
"""Summarize key C64 RAM regions from VICE dumps ($4546, $0100, $02A7, …).

Usage:
  python tools/c64_analyze_ram_dump.py low.bin hi.bin
  python tools/c64_analyze_ram_dump.py hi.bin   # $0800-$FFFF only
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from disasm_6502 import disasm  # noqa: E402

REGIONS = (
    (0x0100, 64, "0100"),
    (0x0320, 32, "0320"),
    (0x0334, 32, "0334"),
    (0x0400, 32, "0400"),
    (0x080B, 32, "080B"),
    (0x080D, 32, "080D"),
    (0x02A7, 32, "02A7"),
    (0x02A9, 32, "02A9"),
    (0x0032, 1, "0032"),
    (0x4546, 256, "4546"),
    (0x00CB, 256, "00CB"),
)


def load_mem(paths: list[Path]) -> tuple[bytearray, str]:
    if len(paths) == 1:
        data = paths[0].read_bytes()
        if len(data) == 0xF800:
            mem = bytearray(65536)
            mem[0x0800:0x10000] = data
            return mem, f"{paths[0].name}@$0800"
        if len(data) == 65536:
            return bytearray(data), paths[0].name
        raise SystemExit(f"unexpected size {len(data)} for single file")
    low, hi = paths[0].read_bytes(), paths[1].read_bytes()
    if len(low) != 0x0800 or len(hi) != 0xF800:
        raise SystemExit(f"expected low=2048 hi=63488, got {len(low)}+{len(hi)}")
    mem = bytearray(65536)
    mem[0x0000:0x0800] = low
    mem[0x0800:0x10000] = hi
    return mem, f"{paths[0].name}+{paths[1].name}"


def region_report(mem: bytearray, addr: int, size: int) -> dict:
    chunk = bytes(mem[addr : addr + size])
    nz = sum(1 for b in chunk if b)
    out: dict = {
        "addr": f"${addr:04X}",
        "size": size,
        "nonzero": nz,
        "head_hex": chunk[:32].hex(),
    }
    if size >= 16 and addr in (0x0100, 0x080B, 0x080D, 0x0400, 0x0334):
        out["disasm"] = disasm(chunk[: min(32, size)], addr, min(16, size // 2))
    if addr == 0x02A9:
        end = chunk.find(0)
        if end > 0:
            out["petscii"] = chunk[:end].decode("latin-1", "replace")
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dump", type=Path, nargs="+", help="hi.bin or low.bin hi.bin")
    ap.add_argument("-o", "--out", type=Path)
    args = ap.parse_args()

    mem, source = load_mem(args.dump)
    report = {
        "source": source,
        "regions": {name: region_report(mem, addr, size) for addr, size, name in REGIONS},
    }
    text = json.dumps(report, indent=2)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"Wrote {args.out}")
    else:
        print(text)


if __name__ == "__main__":
    main()
