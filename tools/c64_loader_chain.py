#!/usr/bin/env python3
"""Trace T18 sector chains and KERNAL loader stubs on MM2 C64 disks.

Usage:
  python tools/c64_loader_chain.py EXTRACTED/c64/MM2-1.D64
  python tools/c64_loader_chain.py EXTRACTED/c64 --all
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from c64_d64_layout import ROOT, SPT, read_sector, sector_offset

# Known KERNAL entry points (C64)
KERNAL = {
    0xBAFF: "SETLFS",
    0xBDFF: "SETNAM",
    0xD5FF: "LOAD",
    0xE7FF: "OPEN",
    0xB1FF: "CLOSE",
}
KERNAL_SIGS = {
    bytes([0x20, 0xBA, 0xFF]): "SETLFS",
    bytes([0x20, 0xBD, 0xFF]): "SETNAM",
    bytes([0x20, 0xD5, 0xFF]): "LOAD",
}


def t18_chain(data: bytes) -> list[tuple[int, int]]:
    """Follow sector links staying on track 18."""
    chain: list[tuple[int, int]] = []
    seen: set[tuple[int, int]] = set()
    track, sector = 18, 0
    while (track, sector) not in seen and track == 18 and sector < SPT[17]:
        seen.add((track, sector))
        chain.append((track, sector))
        sec = read_sector(data, track, sector)
        ntrack, nsector = sec[0], sec[1]
        if ntrack == 0:
            break
        track, sector = ntrack, nsector
    return chain


def scan_kernal_calls(payload: bytes, org: int = 0x400) -> list[dict]:
    hits = []
    for sig, name in KERNAL_SIGS.items():
        idx = payload.find(sig)
        if idx >= 0:
            hits.append({"offset": org + idx, "kernal": name})
    for i in range(len(payload) - 2):
        if payload[i] == 0x20:
            addr = payload[i + 1] | (payload[i + 2] << 8)
            if addr in KERNAL:
                hits.append({"offset": org + i, "kernal": KERNAL[addr], "addr": hex(addr)})
    return hits


def analyze(path: Path) -> dict:
    data = path.read_bytes()
    chain = t18_chain(data)
    sectors = []
    for track, sector in chain:
        sec = read_sector(data, track, sector)
        payload = sec[2:]
        sectors.append(
            {
                "track": track,
                "sector": sector,
                "offset": sector_offset(track, sector),
                "link": [sec[0], sec[1]],
                "kernal_calls": scan_kernal_calls(payload),
                "petscii_preview": _petscii(payload[:64]),
            }
        )
    return {"source": str(path.name), "t18_chain": [{"track": t, "sector": s} for t, s in chain], "sectors": sectors}


def _petscii(buf: bytes) -> str:
    out = []
    for b in buf:
        if 0x41 <= b <= 0x5A or 0x30 <= b <= 0x39 or b in (0x20, 0x2E, 0x2C, 0x3A):
            out.append(chr(b))
        elif b == 0xA0:
            out.append(" ")
        else:
            out.append(".")
    return "".join(out)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--out", type=Path, default=ROOT / "EXTRACTED" / "c64" / "analysis" / "loader_chain.json")
    args = ap.parse_args()

    paths = sorted(args.input.glob("MM2-*.D64")) if args.all or args.input.is_dir() else [args.input]
    report = [analyze(p) for p in paths]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for r in report:
        chain = " -> ".join(f"T{x['track']}S{x['sector']}" for x in r["t18_chain"])
        print(f"{r['source']}: {chain}")
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
