#!/usr/bin/env python3
"""Scan extracted C64 track blobs for gfx (PC LZW, Amiga .32, C64 hires).

Usage:
  python tools/c64_scan_tracks.py EXTRACTED/c64/gfx/tracks
  python tools/c64_scan_tracks.py EXTRACTED/c64/gfx/tracks --try-lzw
"""
from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from c64_d64_layout import KNOWN_OPS  # noqa: E402

AMIGA = {
    "castle.32": 57098,
    "town.32": 57125,
    "cave.32": 57205,
    "throw.32": 14639,
    "sky.32": 6535,
    "ocean.32": 10973,
}

C64_SIZES = {
    "hires_bitmap": 8000,
    "hires_screen": 1000,
    "charset_8x8": 2048,
    "multicolor_pack": 3048,
}


def shannon_entropy(buf: bytes) -> float:
    if not buf:
        return 0.0
    n = len(buf)
    counts = Counter(buf)
    return round(-sum((c / n) * math.log2(c / n) for c in counts.values()), 3)


def ascii_ratio(buf: bytes) -> float:
    if not buf:
        return 0.0
    return round(sum(32 <= b < 127 for b in buf) / len(buf), 3)


def op_ratio(buf: bytes) -> float:
    if not buf:
        return 0.0
    return round(sum(b in KNOWN_OPS for b in buf) / len(buf), 3)


def bitmap_heuristic(buf: bytes) -> dict | None:
    """Best 8000-byte window: low ASCII, moderate entropy (not uniform noise)."""
    if len(buf) < 8000:
        return None
    best: tuple[float, int] | None = None
    step = 256
    for off in range(0, len(buf) - 8000 + 1, step):
        win = buf[off : off + 8000]
        ar = ascii_ratio(win)
        ent = shannon_entropy(win)
        if ar > 0.15 or ent < 4.5 or ent > 7.8:
            continue
        score = ent - ar * 2
        if best is None or score > best[0]:
            best = (score, off)
    if best is None:
        return None
    off = best[1]
    win = buf[off : off + 8000]
    return {
        "offset": off,
        "entropy": shannon_entropy(win),
        "ascii_ratio": ascii_ratio(win),
        "unique_bytes": len(set(win)),
    }


def scan_file(path: Path, try_lzw: bool) -> dict:
    data = path.read_bytes()
    hits: list[dict] = []

    for name, size in AMIGA.items():
        sig_path = ROOT / name
        if sig_path.exists():
            sig = sig_path.read_bytes()[:24]
            j = data.find(sig)
            if j >= 0:
                hits.append({"kind": "amiga_sig", "file": name, "offset": j})

        needle = struct.pack("<I", size)
        j = 0
        while True:
            j = data.find(needle, j)
            if j < 0:
                break
            entry: dict = {"kind": "u32_size", "target": name, "size": size, "offset": j}
            if try_lzw and j + 8 <= len(data):
                try:
                    from mm2_lzw import lzw_decompress

                    dec = lzw_decompress(data[j + 4 : j + 4 + 65536])
                    entry["lzw_len"] = len(dec)
                    entry["lzw_match"] = len(dec) == size
                    if entry["lzw_match"]:
                        entry["kind"] = "pc_lzw_confirmed"
                except Exception as ex:
                    entry["lzw_error"] = str(ex)[:80]
            hits.append(entry)
            j += 1

    for name, size in C64_SIZES.items():
        needle = struct.pack("<H", size)
        j = data.find(needle)
        if j >= 0 and j + 2 + size <= len(data):
            payload = data[j + 2 : j + 2 + size]
            if ascii_ratio(payload) < 0.4 and op_ratio(payload[:256]) < 0.35:
                hits.append(
                    {
                        "kind": "u16_size_header",
                        "target": name,
                        "size": size,
                        "offset": j,
                        "entropy": shannon_entropy(payload[:512]),
                    }
                )

    bm = bitmap_heuristic(data)
    return {
        "file": path.name,
        "bytes": len(data),
        "entropy": shannon_entropy(data),
        "ascii_ratio": ascii_ratio(data),
        "op_ratio": op_ratio(data[:512]),
        "bitmap_window": bm,
        "hits": hits[:40],
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("tracks", type=Path, nargs="?", default=ROOT / "EXTRACTED/c64/gfx/tracks")
    ap.add_argument("--try-lzw", action="store_true")
    ap.add_argument("-o", "--out", type=Path, default=ROOT / "EXTRACTED/c64/analysis/track_gfx_scan.json")
    args = ap.parse_args()

    report = []
    for disk in sorted(args.tracks.glob("mm2-*")):
        tracks = [scan_file(tb, args.try_lzw) for tb in sorted(disk.glob("t*.bin"))]
        report.append(
            {
                "disk": disk.name,
                "tracks": tracks,
                "confirmed_lzw": sum(
                    1 for t in tracks for h in t["hits"] if h.get("kind") == "pc_lzw_confirmed"
                ),
            }
        )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    confirmed = sum(d["confirmed_lzw"] for d in report)
    bm = sum(1 for d in report for t in d["tracks"] if t.get("bitmap_window"))
    print(f"Wrote {args.out} ({len(report)} disks, {confirmed} LZW, {bm} bitmap windows)")


if __name__ == "__main__":
    main()
