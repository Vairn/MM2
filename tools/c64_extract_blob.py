#!/usr/bin/env python3
"""Extract sector chains or whole tracks from C64 D64 images.

Usage:
  python tools/c64_extract_blob.py EXTRACTED/c64/MM2-1.D64 --chain 19 0 -o EXTRACTED/c64/gfx/mm2-1_t19s0_chain.bin
  python tools/c64_extract_blob.py EXTRACTED/c64/MM2-1.D64 --track 11 -o EXTRACTED/c64/gfx/mm2-1_t11.bin
  python tools/c64_extract_blob.py EXTRACTED/c64/MM2-1.D64 --t18-loader -o EXTRACTED/c64/asm/t18_loader_mm2-1.bin
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from c64_d64_layout import ROOT, SPT, read_sector, sector_payload  # noqa: E402
from c64_trace_chains import follow_chain  # noqa: E402

# Loader-related T18 sectors (confirmed IEC / KERNAL stubs on all disks)
T18_LOADER_SECTORS = [1, 6, 15, 16]


def extract_track(data: bytes, track: int) -> bytes:
    return b"".join(sector_payload(data, track, s) for s in range(SPT[track - 1]))


def extract_t18_loader(data: bytes) -> tuple[bytes, list[tuple[int, int]]]:
    parts = []
    order = []
    for s in T18_LOADER_SECTORS:
        parts.append(sector_payload(data, 18, s))
        order.append((18, s))
    return b"".join(parts), order


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("d64", type=Path)
    ap.add_argument("-o", "--out", type=Path, required=True)
    ap.add_argument("--chain", type=int, nargs=2, metavar=("TRACK", "SECTOR"))
    ap.add_argument("--track", type=int)
    ap.add_argument("--t18-loader", action="store_true")
    ap.add_argument("--meta", type=Path, help="Write JSON sidecar")
    args = ap.parse_args()

    data = args.d64.read_bytes()
    meta: dict = {"source": args.d64.name}

    if args.t18_loader:
        blob, order = extract_t18_loader(data)
        meta["kind"] = "t18_loader"
        meta["sectors"] = [list(x) for x in order]
    elif args.chain:
        chain, blob = follow_chain(data, args.chain[0], args.chain[1])
        meta["kind"] = "sector_chain"
        meta["head"] = list(args.chain)
        meta["chain"] = [list(c) for c in chain]
    elif args.track:
        blob = extract_track(data, args.track)
        meta["kind"] = "track"
        meta["track"] = args.track
    else:
        ap.error("specify --chain, --track, or --t18-loader")

    meta["bytes"] = len(blob)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(blob)
    print(f"Wrote {args.out} ({len(blob)} bytes)")

    if args.meta:
        args.meta.write_text(json.dumps(meta, indent=2), encoding="utf-8")
        print(f"Wrote {args.meta}")


if __name__ == "__main__":
    main()
