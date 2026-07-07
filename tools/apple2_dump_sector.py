#!/usr/bin/env python3
"""Dump Apple II DOS-order sectors from decoded .dsk images.

Usage:
  python tools/apple2_dump_sector.py EXTRACTED/apple2/disks/disk_a.dsk 0 0
  python tools/apple2_dump_sector.py EXTRACTED/apple2/disks/disk_a.dsk 17 0 -o EXTRACTED/apple2/asm/disk_a_t17_s00.bin
  python tools/apple2_dump_sector.py EXTRACTED/apple2/disks/disk_a.dsk 0 --all -o EXTRACTED/apple2/asm
"""
from __future__ import annotations

import argparse
from pathlib import Path

# DOS 3.3 physical sector order (logical sector -> index on track)
_PHYS_ORDER = (0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15)


def sector_offset(track: int, sector: int) -> int:
    if not 0 <= track <= 34:
        raise ValueError(f"track out of range: {track}")
    if not 0 <= sector <= 15:
        raise ValueError(f"sector out of range: {sector}")
    phys = _PHYS_ORDER[sector]
    return (track * 16 + phys) * 256


def read_sector(dsk: bytes, track: int, sector: int) -> bytes:
    off = sector_offset(track, sector)
    return dsk[off : off + 256]


def main() -> None:
    ap = argparse.ArgumentParser(description="Dump sectors from Apple II .dsk")
    ap.add_argument("dsk", type=Path, help="Path to 140K .dsk file")
    ap.add_argument("track", type=int, help="Track 0-34")
    ap.add_argument("sector", type=int, nargs="?", help="Logical sector 0-15 (omit with --all)")
    ap.add_argument("-o", "--out", type=Path, help="Output .bin file or directory (--all)")
    ap.add_argument("--all", action="store_true", help="Dump all 16 sectors on track")
    ap.add_argument("--hex", action="store_true", help="Print hex dump to stdout")
    args = ap.parse_args()

    dsk = args.dsk.read_bytes()
    if len(dsk) != 143360:
        raise SystemExit(f"expected 143360-byte DSK, got {len(dsk)}")

    stem = args.dsk.stem  # disk_a
    side = stem.replace("disk_", "")

    if args.all:
        out_dir = args.out or Path(f"EXTRACTED/apple2/asm")
        out_dir.mkdir(parents=True, exist_ok=True)
        for s in range(16):
            data = read_sector(dsk, args.track, s)
            path = out_dir / f"{stem}_t{args.track:02d}_s{s:02d}.bin"
            path.write_bytes(data)
            print(f"T{args.track:02d}S{s:02d} @{sector_offset(args.track, s):05X} -> {path}")
        return

    if args.sector is None:
        raise SystemExit("sector required unless --all")

    data = read_sector(dsk, args.track, args.sector)
    off = sector_offset(args.track, args.sector)
    print(f"T{args.track:02d}S{args.sector:02d} @{off:05X} ({len(data)} bytes)")

    if args.hex:
        for i in range(0, 256, 16):
            chunk = data[i : i + 16]
            hexs = " ".join(f"{b:02X}" for b in chunk)
            print(f"  {i:04X}: {hexs}")

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_bytes(data)
        print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
