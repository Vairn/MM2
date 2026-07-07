#!/usr/bin/env python3
"""Disassemble all sectors on a track (code-heavy sectors only by default).

Usage:
  python tools/apple2_disasm_track.py EXTRACTED/apple2/disks/disk_a.dsk 0 --org 0x800
  python tools/apple2_disasm_track.py EXTRACTED/apple2/disks/disk_a.dsk 0 --org 0xB800 --sectors 8
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
_DISASM = ROOT / "tools" / "disasm_6502.py"
_PHYS_ORDER = (0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15)


def sector_bytes(dsk: bytes, track: int, sector: int) -> bytes:
    phys = _PHYS_ORDER[sector]
    off = (track * 16 + phys) * 256
    return dsk[off : off + 256]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dsk", type=Path)
    ap.add_argument("track", type=int)
    ap.add_argument("--org", type=lambda x: int(x, 0), default=0x800)
    ap.add_argument("--sectors", type=str, default="all", help="Comma list or 'all'")
    ap.add_argument("-o", "--out-dir", type=Path, default=Path("EXTRACTED/apple2/asm"))
    ap.add_argument("--limit", type=int, default=300)
    args = ap.parse_args()

    dsk = args.dsk.read_bytes()
    stem = args.dsk.stem
    args.out_dir.mkdir(parents=True, exist_ok=True)

    if args.sectors == "all":
        sectors = list(range(16))
    else:
        sectors = [int(s.strip()) for s in args.sectors.split(",")]

    for s in sectors:
        data = sector_bytes(dsk, args.track, s)
        bin_path = args.out_dir / f"{stem}_t{args.track:02d}_s{s:02d}.bin"
        asm_path = args.out_dir / f"{stem}_t{args.track:02d}_s{s:02d}.asm"
        bin_path.write_bytes(data)
        subprocess.run(
            [
                sys.executable,
                str(_DISASM),
                str(bin_path),
                "--org",
                hex(args.org),
                "-o",
                str(asm_path),
                "--limit",
                str(args.limit),
            ],
            check=True,
        )
        print(f"T{args.track:02d}S{s:02d} -> {asm_path}")


if __name__ == "__main__":
    main()
