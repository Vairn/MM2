#!/usr/bin/env python3
"""Analyze Commodore 64 MM2 D64 images (custom NWC layout, gfx string scan).

All six disks use a custom IEC loader on T18; CBM DOS catalog sectors contain
code, not filenames. This tool dumps confirmed metadata, scans for Amiga gfx
stem strings, PRG signatures, and exports raw sectors for manual RE.

Usage:
  python tools/analyze_c64_disk.py EXTRACTED/c64 --out EXTRACTED/c64/analysis
"""
from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_SPT = [21] * 17 + [19] * 7 + [18] * 6 + [17] * 5
_INTERLEAVE = {
    range(1, 18): [0, 11, 2, 9, 4, 7, 6, 1, 10, 3, 8, 5, 13, 14, 15, 16, 17, 18, 19, 20, 12],
    range(18, 25): [0, 6, 12, 18, 4, 10, 16, 2, 8, 14, 1, 7, 13, 3, 9, 15, 5, 11, 17],
    range(25, 31): [0, 6, 12, 18, 3, 9, 15, 1, 7, 13, 2, 8, 14, 4, 10, 16, 5, 11],
    range(31, 36): [0, 6, 12, 3, 9, 15, 1, 7, 13, 2, 8, 14, 4, 10, 16, 5, 11],
}
_GFX_STEMS = [
    "CASTLE", "CAVE", "TOWN", "THROW", "SKY", "DESERT", "SWAMP", "OCEAN",
    "TUNDRA", "OUTDOOR", "MONSTER", "NWCP", "GLOBE",
]


def _interleave_table(track: int) -> list[int]:
    for rng, table in _INTERLEAVE.items():
        if track in rng:
            return table
    raise ValueError(track)


def sector_offset(track: int, sector: int) -> int:
    base = sum(_SPT[i - 1] * 256 for i in range(1, track))
    il = _interleave_table(track)[: _SPT[track - 1]]
    return base + il[sector] * 256


def read_sector(data: bytes, track: int, sector: int) -> bytes:
    off = sector_offset(track, sector)
    return data[off : off + 256]


def analyze_d64(path: Path) -> dict:
    data = path.read_bytes()
    bam = read_sector(data, 18, 0)
    disk_name = bytes(bam[0x90:0xA0]).split(b"\xa0")[0].decode("latin1", errors="replace")
    t18s1 = read_sector(data, 18, 1)[2:]

    stem_hits = []
    for stem in _GFX_STEMS:
        i = data.find(stem.encode())
        if i >= 0:
            ctx = data[max(0, i - 32) : i + 48]
            stem_hits.append({"stem": stem, "offset": i, "context_ascii": _ascii(ctx)})

    prg_hits = []
    for i in range(len(data) - 4):
        if data[i] == 0x01 and data[i + 1] in (0x08, 0x00, 0x40):
            load = struct.unpack_from("<H", data, i + 2)[0]
            if 0x0800 <= load <= 0xC000:
                prg_hits.append({"offset": i, "load_addr": hex(load)})

    strings = []
    for m in re.finditer(rb"[\x20-\x7e]{8,}", data):
        s = m.group().decode()
        if any(k in s.upper() for k in ("NEW WORLD", "MIGHT", "MAGIC", "COPYRIGHT")):
            strings.append({"offset": m.start(), "text": s[:120]})

    return {
        "source": str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
        "size": len(data),
        "bam_disk_name": disk_name,
        "bam_disk_id": [hex(bam[0xA0]), hex(bam[0xA1])],
        "directory_note": "T18S1+ contain 6502 IEC loader code, not a CBM catalog (all disks)",
        "t18s1_md5": __import__("hashlib").md5(t18s1).hexdigest(),
        "t18_bam_chain": [bam[0], bam[1]],
        "stem_hits": stem_hits,
        "prg_signature_hits": prg_hits[:50],
        "prg_signature_total": len(prg_hits),
        "notable_strings": strings[:30],
        "boot_t1s0_hex": read_sector(data, 1, 0)[:32].hex(),
    }


def _ascii(buf: bytes) -> str:
    return "".join(chr(b) if 32 <= b < 127 else "." for b in buf)


def export_sectors(path: Path, out_dir: Path) -> None:
    data = path.read_bytes()
    sec_dir = out_dir / "sectors" / path.stem.lower()
    sec_dir.mkdir(parents=True, exist_ok=True)
    for track in range(1, 36):
        for sector in range(_SPT[track - 1]):
            sec = read_sector(data, track, sector)
            name = f"t{track:02d}_s{sector:02d}.bin"
            (sec_dir / name).write_bytes(sec)


def main() -> None:
    ap = argparse.ArgumentParser(description="Analyze C64 MM2 D64 disks")
    ap.add_argument("input", type=Path, help="D64 file or directory")
    ap.add_argument("--out", type=Path, default=ROOT / "EXTRACTED" / "c64" / "analysis")
    ap.add_argument("--export-sectors", action="store_true", help="Dump all raw sectors")
    args = ap.parse_args()

    paths = sorted(args.input.glob("*.D64")) if args.input.is_dir() else [args.input]
    args.out.mkdir(parents=True, exist_ok=True)

    report = []
    for p in paths:
        print(f"Analyzing {p.name}...")
        info = analyze_d64(p)
        report.append(info)
        if args.export_sectors:
            export_sectors(p, args.out)

    out_json = args.out / "disk_analysis.json"
    out_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Wrote {out_json}")


if __name__ == "__main__":
    main()
