#!/usr/bin/env python3
"""Extract data-class tracks from C64 MM2 D64 images for offline gfx RE.

Usage:
  python tools/c64_extract_data_tracks.py EXTRACTED/c64 --out EXTRACTED/c64/gfx/tracks
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from c64_d64_layout import ROOT, SPT, classify_payload, sector_payload  # noqa: E402


def extract_disk(path: Path, out_dir: Path) -> dict:
    data = path.read_bytes()
    name = path.stem.lower()
    disk_out = out_dir / name
    disk_out.mkdir(parents=True, exist_ok=True)
    tracks: list[int] = []
    for t in range(1, 36):
        kinds = [classify_payload(sector_payload(data, t, s)) for s in range(SPT[t - 1])]
        if sum(1 for k in kinds if k == "data") >= max(6, SPT[t - 1] // 2):
            blob = b"".join(sector_payload(data, t, s) for s in range(SPT[t - 1]))
            out = disk_out / f"t{t:02d}.bin"
            out.write_bytes(blob)
            tracks.append(t)
    meta = {"source": path.name, "tracks": tracks, "bytes": sum((disk_out / f"t{t:02d}.bin").stat().st_size for t in tracks)}
    (disk_out / "manifest.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    return meta


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path, default=ROOT / "EXTRACTED/c64")
    ap.add_argument("--out", type=Path, default=ROOT / "EXTRACTED/c64/gfx/tracks")
    args = ap.parse_args()

    paths = sorted(args.input.glob("MM2-*.D64")) if args.input.is_dir() else [args.input]
    report = [extract_disk(p, args.out) for p in paths]
    summary = args.out / "summary.json"
    summary.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for r in report:
        print(f"{r['source']}: {len(r['tracks'])} tracks, {r['bytes']} bytes")
    print(f"Wrote {summary}")


if __name__ == "__main__":
    main()
