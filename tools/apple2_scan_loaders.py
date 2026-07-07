#!/usr/bin/env python3
"""Scan Apple II .dsk images for loader / DHGR / gfx-candidate signatures.

Confirmed-use only: reports byte offsets and sector locations, no format guessing.

Usage:
  python tools/apple2_scan_loaders.py EXTRACTED/apple2/disks/disk_a.dsk
  python tools/apple2_scan_loaders.py EXTRACTED/apple2/disks/*.dsk -o EXTRACTED/apple2/loader_scan.json
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

_PHYS_ORDER = (0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15)

# Apple IIe DHGR-related soft switches (STA abs)
_DHGR_SWITCHES = {
    "STA_C050_PAGE2": bytes([0x8D, 0x50, 0xC0]),
    "STA_C054_DHGR": bytes([0x8D, 0x54, 0xC0]),
    "STA_C05E_HIRES": bytes([0x8D, 0x5E, 0xC0]),
    "STA_C052_DHGR": bytes([0x8D, 0x52, 0xC0]),
    "STA_C088_AUX": bytes([0x8D, 0x88, 0xC0]),
}

# Loader dispatch addresses seen in disk A T00S0 @ $0800
_LOADER_TARGETS = {
    "JMP_610C": bytes([0x4C, 0x0C, 0x61]),
    "JMP_5F08": bytes([0x4C, 0x08, 0x5F]),
    "JMP_D30B": bytes([0x4C, 0x0B, 0xD3]),
    "JMP_1E0F": bytes([0x4C, 0x0F, 0x1E]),
    "JMP_ind_3E00": bytes([0x6C, 0x3E, 0x00]),
}

_VALID_OPS = frozenset(
    {
        0x00, 0x01, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0D, 0x10, 0x18, 0x20, 0x21,
        0x24, 0x25, 0x29, 0x2C, 0x30, 0x38, 0x4A, 0x4C, 0x4D, 0x50, 0x60, 0x65,
        0x68, 0x69, 0x6C, 0x6D, 0x70, 0x84, 0x85, 0x86, 0x88, 0x8A, 0x8C, 0x8D, 0x8E,
        0x90, 0x91, 0x94, 0x95, 0x99, 0x9A, 0xA0, 0xA2, 0xA4, 0xA5, 0xA6, 0xA8, 0xA9,
        0xAA, 0xAC, 0xAD, 0xAE, 0xB0, 0xB1, 0xB5, 0xB9, 0xBA, 0xBD, 0xC0, 0xC5, 0xC6,
        0xC8, 0xC9, 0xCA, 0xCD, 0xCE, 0xD0, 0xD8, 0xE0, 0xE5, 0xE6, 0xE8, 0xEA, 0xEE, 0xF0,
    }
)


def _sector_loc(offset: int) -> dict:
    sec_idx = offset // 256
    track = sec_idx // 16
    phys = sec_idx % 16
    logical = _PHYS_ORDER.index(phys) if phys in _PHYS_ORDER else -1
    return {
        "offset": offset,
        "hex": hex(offset),
        "track": track,
        "sector": logical,
        "byte_in_sector": offset % 256,
    }


def _entropy(data: bytes) -> float:
    if not data:
        return 0.0
    freq = [0] * 256
    for b in data:
        freq[b] += 1
    n = len(data)
    return -sum((c / n) * math.log2(c / n) for c in freq if c)


def _find_all(data: bytes, pat: bytes, limit: int = 32) -> list[dict]:
    hits: list[dict] = []
    off = 0
    while len(hits) < limit:
        i = data.find(pat, off)
        if i < 0:
            break
        hits.append(_sector_loc(i))
        off = i + 1
    return hits


def scan_dsk(path: Path) -> dict:
    data = path.read_bytes()
    code_sectors: list[dict] = []
    high_entropy: list[dict] = []

    for track in range(35):
        for sector in range(16):
            phys = _PHYS_ORDER[sector]
            off = (track * 16 + phys) * 256
            sec = data[off : off + 256]
            opcode_hits = sum(1 for b in sec[:64] if b in _VALID_OPS)
            ent = _entropy(sec)
            loc = {"track": track, "sector": sector, "offset": off, "hex": hex(off)}
            if opcode_hits >= 40:
                code_sectors.append({**loc, "opcode_hits": opcode_hits})
            if ent >= 7.15 and sum(1 for b in sec if b) >= 200:
                high_entropy.append({**loc, "entropy": round(ent, 3)})

    sta_pages: dict[str, int] = {}
    for i in range(len(data) - 2):
        if data[i] == 0x8D:
            addr = data[i + 1] | (data[i + 2] << 8)
            if addr >= 0x1000:
                page = addr >> 8
                sta_pages[f"{page:02X}"] = sta_pages.get(f"{page:02X}", 0) + 1

    top_sta = sorted(sta_pages.items(), key=lambda kv: -kv[1])[:20]

    return {
        "dsk": str(path),
        "size": len(data),
        "dhgr_switches": {k: _find_all(data, v) for k, v in _DHGR_SWITCHES.items() if _find_all(data, v)},
        "loader_patterns": {k: _find_all(data, v) for k, v in _LOADER_TARGETS.items() if _find_all(data, v)},
        "code_sector_count": len(code_sectors),
        "data_sector_count": 560 - len(code_sectors),
        "code_sectors_sample": code_sectors[:40],
        "high_entropy_sectors": sorted(high_entropy, key=lambda x: -x["entropy"])[:15],
        "sta_abs_top_pages": [{"page": p, "count": c} for p, c in top_sta],
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dsk", type=Path, nargs="+")
    ap.add_argument("-o", "--out", type=Path, help="Write JSON report")
    args = ap.parse_args()

    report = [scan_dsk(p) for p in args.dsk]
    text = json.dumps(report, indent=2)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text + "\n", encoding="utf-8")
        print(f"Wrote {args.out}")
    else:
        print(text)


if __name__ == "__main__":
    main()
