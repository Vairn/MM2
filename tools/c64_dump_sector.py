#!/usr/bin/env python3
"""Dump D64 sectors and optionally disassemble for C64 MM2 RE.

Usage:
  python tools/c64_dump_sector.py EXTRACTED/c64/MM2-1.D64 18 1 -o EXTRACTED/c64/asm/mm2_1_t18_s01.bin
  python tools/c64_dump_sector.py EXTRACTED/c64/MM2-1.D64 18 1 --disasm --org 0x400
  python tools/c64_dump_sector.py EXTRACTED/c64/MM2-1.D64 --tracks 1,18 --out-dir EXTRACTED/c64/asm
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from c64_d64_layout import LOADER_ORG, ROOT, read_sector

DISASM = ROOT / "tools" / "disasm_6502.py"


def dump_sector(d64: Path, track: int, sector: int, out: Path | None, disasm: bool, org: int) -> None:
    data = d64.read_bytes()
    sec = read_sector(data, track, sector)
    stem = f"{d64.stem.lower()}_t{track:02d}_s{sector:02d}"
    if out is None:
        out = Path(f"{stem}.bin")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(sec)
    print(f"Wrote {out} ({len(sec)} bytes, link T{sec[0]} S{sec[1]})")

    if disasm:
        asm_path = out.with_suffix(".asm")
        payload = sec[2:]
        # Write payload-only for disasm (code starts at sector+2)
        payload_path = out.with_name(out.stem + "_payload.bin")
        payload_path.write_bytes(payload)
        cmd = [
            sys.executable,
            str(DISASM),
            str(payload_path),
            "--org",
            hex(org),
            "-o",
            str(asm_path),
            "--limit",
            "800",
        ]
        subprocess.run(cmd, check=True)


def main() -> None:
    ap = argparse.ArgumentParser(description="Dump/disasm C64 D64 sectors")
    ap.add_argument("d64", type=Path)
    ap.add_argument("track", type=int, nargs="?", default=0)
    ap.add_argument("sector", type=int, nargs="?", default=0)
    ap.add_argument("-o", "--out", type=Path)
    ap.add_argument("--out-dir", type=Path)
    ap.add_argument("--tracks", type=str, help="Comma-separated tracks to dump all sectors")
    ap.add_argument("--disasm", action="store_true")
    ap.add_argument("--org", type=lambda x: int(x, 0), default=LOADER_ORG)
    args = ap.parse_args()

    if args.tracks and args.out_dir:
        for t in [int(x) for x in args.tracks.split(",")]:
            from c64_d64_layout import SPT

            for s in range(SPT[t - 1]):
                out = args.out_dir / f"{args.d64.stem.lower()}_t{t:02d}_s{s:02d}.bin"
                dump_sector(args.d64, t, s, out, args.disasm, args.org)
    else:
        dump_sector(args.d64, args.track, args.sector, args.out, args.disasm, args.org)


if __name__ == "__main__":
    main()
