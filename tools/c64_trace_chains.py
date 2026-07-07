#!/usr/bin/env python3
"""Follow valid CBM sector chains (track 1-35) and report blob sizes.

Usage:
  python tools/c64_trace_chains.py EXTRACTED/c64/MM2-2.D64
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from c64_d64_layout import ROOT, SPT, read_sector, sector_offset  # noqa: E402

KERNAL_SIGS = {
    bytes([0x20, 0xBA, 0xFF]): "SETLFS",
    bytes([0x20, 0xBD, 0xFF]): "SETNAM",
    bytes([0x20, 0xD5, 0xFF]): "LOAD",
}


def valid_link(track: int, sector: int) -> bool:
    return 1 <= track <= 35 and sector < SPT[track - 1]


def follow_chain(data: bytes, start_t: int, start_s: int) -> tuple[list[tuple[int, int]], bytes]:
    chain: list[tuple[int, int]] = []
    seen: set[tuple[int, int]] = set()
    payload = bytearray()
    t, s = start_t, start_s
    while (t, s) not in seen:
        seen.add((t, s))
        chain.append((t, s))
        sec = read_sector(data, t, s)
        payload.extend(sec[2:])
        nt, ns = sec[0], sec[1]
        if nt == 0:
            break
        if not valid_link(nt, ns):
            break
        t, s = nt, ns
    return chain, bytes(payload)


def find_chain_starts(data: bytes) -> list[tuple[int, int]]:
    """Sectors referenced as next-link targets are not chain heads."""
    referenced: set[tuple[int, int]] = set()
    for t in range(1, 36):
        for s in range(SPT[t - 1]):
            sec = read_sector(data, t, s)
            nt, ns = sec[0], sec[1]
            if valid_link(nt, ns):
                referenced.add((nt, ns))
    heads = []
    for t in range(1, 36):
        for s in range(SPT[t - 1]):
            sec = read_sector(data, t, s)
            nt, ns = sec[0], sec[1]
            if valid_link(nt, ns) and (t, s) not in referenced:
                heads.append((t, s))
    return heads


def analyze(path: Path, min_sectors: int = 2) -> dict:
    data = path.read_bytes()
    chains = []
    for head in find_chain_starts(data):
        chain, payload = follow_chain(data, head[0], head[1])
        if len(chain) < min_sectors:
            continue
        ascii_ratio = sum(32 <= b < 127 for b in payload) / max(1, len(payload))
        prg = payload[:2] == bytes([0x01, 0x08]) if len(payload) >= 2 else False
        load_addr = payload[2] | (payload[3] << 8) if prg and len(payload) >= 4 else None
        kernal = [n for sig, n in KERNAL_SIGS.items() if sig in payload[:128]]
        chains.append(
            {
                "head": list(head),
                "sectors": len(chain),
                "bytes": len(payload),
                "ascii_ratio": round(ascii_ratio, 3),
                "prg_header": prg,
                "load_addr": hex(load_addr) if load_addr else None,
                "kernal_in_head": kernal,
                "chain": [list(c) for c in chain[:20]],
            }
        )
    chains.sort(key=lambda x: -x["bytes"])
    return {"source": path.name, "chains": chains}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--min-sectors", type=int, default=2)
    ap.add_argument("--out", type=Path, default=ROOT / "EXTRACTED" / "c64" / "analysis" / "sector_chains.json")
    args = ap.parse_args()

    paths = sorted(args.input.glob("MM2-*.D64")) if args.all or args.input.is_dir() else [args.input]
    report = [analyze(p, args.min_sectors) for p in paths]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for r in report:
        print(f"{r['source']}: {len(r['chains'])} chains")
        for c in r["chains"][:5]:
            print(f"  T{c['head'][0]}S{c['head'][1]} -> {c['sectors']} sectors, {c['bytes']} bytes, ascii={c['ascii_ratio']}")
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
