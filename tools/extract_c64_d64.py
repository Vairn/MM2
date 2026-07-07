#!/usr/bin/env python3
"""Extract files from Commodore 64 D64 disk images (CBM DOS 2.x).

Usage:
  python tools/extract_c64_d64.py EXTRACTED/c64/MM2-1.D64 -o EXTRACTED/c64/extracted/disk1
  python tools/extract_c64_d64.py EXTRACTED/c64 --batch -o EXTRACTED/c64/extracted
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, asdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Tracks 1-35 sector counts (standard 1541 layout)
_SPT = [21] * 17 + [19] * 7 + [18] * 6 + [17] * 5

_INTERLEAVE = {
    range(1, 18): [0, 11, 2, 9, 4, 7, 6, 1, 10, 3, 8, 5, 13, 14, 15, 16, 17, 18, 19, 20, 12],
    range(18, 25): [0, 6, 12, 18, 4, 10, 16, 2, 8, 14, 1, 7, 13, 3, 9, 15, 5, 11, 17],
    range(25, 31): [0, 6, 12, 18, 3, 9, 15, 1, 7, 13, 2, 8, 14, 4, 10, 16, 5, 11],
    range(31, 36): [0, 6, 12, 3, 9, 15, 1, 7, 13, 2, 8, 14, 4, 10, 16, 5, 11],
}

_FILE_TYPES = {
    0x80: "DEL",
    0x81: "SEQ",
    0x82: "PRG",
    0x83: "USR",
    0x84: "REL",
}


def _interleave_table(track: int) -> list[int]:
    for rng, table in _INTERLEAVE.items():
        if track in rng:
            return table
    raise ValueError(track)


def _track_base(track: int) -> int:
    return sum(_SPT[i - 1] * 256 for i in range(1, track))


def sector_offset(track: int, sector: int) -> int:
    """Byte offset for 1-based track, 0-based logical sector."""
    il = _interleave_table(track)[: _SPT[track - 1]]
    return _track_base(track) + il[sector] * 256


def read_sector(data: bytes, track: int, sector: int) -> bytes:
    off = sector_offset(track, sector)
    return data[off : off + 256]


@dataclass
class DirEntry:
    name: str
    file_type: str
    closed: bool
    start_track: int
    start_sector: int
    size_blocks: int


def _petscii_name(raw: bytes) -> str:
    out = []
    for b in raw:
        if b == 0xA0:
            break
        if 0x41 <= b <= 0x5A:
            out.append(chr(b))
        elif 0x30 <= b <= 0x39:
            out.append(chr(b))
        elif b in (0x20, 0x2E, 0x2D, 0x2B):
            out.append(chr(b))
        else:
            out.append(f"\\x{b:02x}")
    return "".join(out)


def parse_directory(data: bytes) -> tuple[str, list[DirEntry]]:
    bam = read_sector(data, 18, 0)
    disk_name = _petscii_name(bam[0x90:0xA0])
    entries: list[DirEntry] = []
    for block in (1, 2):
        sec = read_sector(data, 18, block)
        next_track, next_sector = sec[0], sec[1]
        for i in range(8):
            e = sec[2 + i * 32 : 2 + (i + 1) * 32]
            ftype = e[0]
            if ftype == 0:
                continue
            closed = bool(ftype & 0x40)
            base_type = ftype & 0x07
            entries.append(
                DirEntry(
                    name=_petscii_name(e[3:19]),
                    file_type=_FILE_TYPES.get(base_type | 0x80, f"0x{ftype:02x}"),
                    closed=closed,
                    start_track=e[1],
                    start_sector=e[2],
                    size_blocks=e[28] | (e[29] << 8),
                )
            )
        if next_track == 0:
            break
    return disk_name, entries


def read_file(data: bytes, start_track: int, start_sector: int) -> bytes:
    out = bytearray()
    track, sector = start_track, start_sector
    while track != 0:
        sec = read_sector(data, track, sector)
        link_track, link_sector = sec[0], sec[1]
        out.extend(sec[2:])
        track, sector = link_track, link_sector
    return bytes(out)


def extract_d64(path: Path, out_dir: Path) -> dict:
    data = path.read_bytes()
    if len(data) != 174848:
        raise ValueError(f"{path}: expected 174848 bytes, got {len(data)}")

    out_dir.mkdir(parents=True, exist_ok=True)
    disk_name, entries = parse_directory(data)
    manifest = {
        "source": str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
        "disk_name": disk_name,
        "files": [],
    }

    for ent in entries:
        if not ent.closed or ent.start_track == 0:
            continue
        if ent.start_track > 35 or ent.start_sector >= _SPT[ent.start_track - 1]:
            continue
        try:
            payload = read_file(data, ent.start_track, ent.start_sector)
        except (IndexError, ValueError):
            continue
        # REL/SEQ sizes differ; PRG uses block count * 254 - last sector padding
        if ent.file_type == "PRG":
            expected = ent.size_blocks * 254
            payload = payload[:expected]
        safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in ent.name)
        out_path = out_dir / f"{safe}.{ent.file_type.lower()}"
        out_path.write_bytes(payload)
        manifest["files"].append({**asdict(ent), "path": out_path.name, "bytes": len(payload)})

    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return manifest


def main() -> None:
    ap = argparse.ArgumentParser(description="Extract CBM files from D64 images")
    ap.add_argument("input", type=Path, help="D64 file or directory of D64s")
    ap.add_argument("-o", "--out", type=Path, required=True)
    ap.add_argument("--batch", action="store_true")
    args = ap.parse_args()

    if args.batch or args.input.is_dir():
        src = args.input if args.input.is_dir() else args.input.parent
        disks = sorted(src.glob("*.D64")) + sorted(src.glob("*.d64"))
        for d in disks:
            stem = d.stem.lower().replace(" ", "_")
            extract_d64(d, args.out / stem)
            print(f"{d.name}: extracted -> {args.out / stem}")
    else:
        manifest = extract_d64(args.input, args.out)
        print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
