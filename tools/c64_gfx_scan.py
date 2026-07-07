#!/usr/bin/env python3
"""Scan C64 D64 images for candidate graphics blobs and size matches.

Looks for:
- Exact/near size matches to Amiga .32 / C64 hires (8000) / multicolor screen+charset
- High-entropy contiguous regions without ASCII/code signatures
- u16/u32 size headers before large payloads

Usage:
  python tools/c64_gfx_scan.py EXTRACTED/c64 --out EXTRACTED/c64/analysis
  python tools/c64_gfx_scan.py EXTRACTED/c64/MM2-1.D64 --extract EXTRACTED/c64/gfx
"""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

from c64_d64_layout import KNOWN_OPS, ROOT, SPT, offset_to_track_sector, read_sector, sector_offset

# Reference sizes (Amiga originals in repo root)
AMIGA_SIZES = {
    "castle.32": 57098,
    "town.32": 57125,
    "throw.32": 14639,
    "sky.32": 6535,
}

C64_CANDIDATE_SIZES = {
    "hires_bitmap": 8000,
    "hires_screen": 1000,
    "charset_8x8": 2048,
    "hires_full": 9000,  # screen + bitmap common load
    "multicolor_screen_charset": 3048,  # 1000 + 2048
}


def _ascii_ratio(buf: bytes) -> float:
    if not buf:
        return 0.0
    return sum(32 <= b < 127 for b in buf) / len(buf)


def _op_ratio(buf: bytes) -> float:
    if not buf:
        return 0.0
    return sum(b in KNOWN_OPS for b in buf) / len(buf)


def scan_size_matches(data: bytes) -> list[dict]:
    hits = []
    all_targets = {**AMIGA_SIZES, **C64_CANDIDATE_SIZES}
    for name, size in all_targets.items():
        # u16 LE size header immediately before payload
        needle = struct.pack("<H", size)
        start = 0
        while True:
            i = data.find(needle, start)
            if i < 0:
                break
            start = i + 1
            if i + 2 + size > len(data):
                continue
            payload = data[i + 2 : i + 2 + size]
            if _ascii_ratio(payload) > 0.5:
                continue
            ts = offset_to_track_sector(len(data), i)
            hits.append(
                {
                    "kind": "u16_size_header",
                    "target": name,
                    "size": size,
                    "header_offset": i,
                    "track_sector": list(ts) if ts else None,
                    "ascii_ratio": round(_ascii_ratio(payload), 3),
                    "op_ratio": round(_op_ratio(payload[:256]), 3),
                }
            )
    return hits[:100]


def scan_contiguous_data_tracks(data: bytes) -> list[dict]:
    """Find tracks dominated by non-text non-code payloads (gfx candidates)."""
    from c64_d64_layout import classify_payload, sector_payload

    candidates = []
    for track in range(1, 36):
        payloads = [sector_payload(data, track, s) for s in range(SPT[track - 1])]
        classes = [classify_payload(p) for p in payloads]
        data_count = sum(1 for c in classes if c == "data")
        if data_count >= max(3, SPT[track - 1] // 2):
            off = sector_offset(track, 0)
            concat = b"".join(payloads)
            candidates.append(
                {
                    "track": track,
                    "data_sectors": data_count,
                    "total_sectors": SPT[track - 1],
                    "offset": off,
                    "bytes": len(concat),
                    "ascii_ratio": round(_ascii_ratio(concat), 3),
                    "op_ratio": round(_op_ratio(concat[:512]), 3),
                }
            )
    return candidates


def scan_disk(path: Path) -> dict:
    data = path.read_bytes()
    return {
        "source": str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
        "size_matches": scan_size_matches(data),
        "data_tracks": scan_contiguous_data_tracks(data),
    }


def extract_data_track(path: Path, track: int, out_dir: Path) -> Path:
    from c64_d64_layout import sector_payload

    data = path.read_bytes()
    payloads = [sector_payload(data, track, s) for s in range(SPT[track - 1])]
    blob = b"".join(payloads)
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{path.stem.lower()}_t{track:02d}.bin"
    out_path.write_bytes(blob)
    return out_path


def main() -> None:
    ap = argparse.ArgumentParser(description="C64 MM2 graphics candidate scan")
    ap.add_argument("input", type=Path)
    ap.add_argument("--out", type=Path, default=ROOT / "EXTRACTED" / "c64" / "analysis")
    ap.add_argument("--extract", type=Path, help="Extract data-dominant tracks here")
    ap.add_argument("--extract-track", type=int, action="append", default=[], help="Track to extract")
    args = ap.parse_args()

    paths = sorted(args.input.glob("*.D64")) if args.input.is_dir() else [args.input]
    args.out.mkdir(parents=True, exist_ok=True)

    report = [scan_disk(p) for p in paths]
    out_json = args.out / "gfx_scan.json"
    out_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Wrote {out_json}")

    if args.extract:
        for disk in report:
            src = ROOT / disk["source"] if not Path(disk["source"]).is_absolute() else Path(disk["source"])
            tracks = args.extract_track or [t["track"] for t in disk["data_tracks"][:5]]
            for tr in tracks:
                out = extract_data_track(src, tr, args.extract)
                print(f"Extracted {out}")


if __name__ == "__main__":
    main()
