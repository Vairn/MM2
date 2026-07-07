#!/usr/bin/env python3
"""Gfx heuristics for SNES / Genesis MM2 ROM images."""

from __future__ import annotations

import argparse
import collections
import json
import math
import struct
import sys
from pathlib import Path


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    freq = collections.Counter(data)
    n = len(data)
    return -sum((c / n) * math.log2(c / n) for c in freq.values())


def scan_snes(data: bytes) -> dict:
    ppu = {
        "STZ_2115": bytes([0x9C, 0x15, 0x21]),
        "STZ_212C": bytes([0x9C, 0x2C, 0x21]),
        "STA_2107": bytes([0x8D, 0x07, 0x21]),
        "STA_2118": bytes([0x8D, 0x18, 0x21]),
        "STA_2121": bytes([0x8D, 0x21, 0x21]),
    }
    ppu_hits = {}
    for name, pat in ppu.items():
        offs = [i for i in range(len(data) - len(pat)) if data[i : i + len(pat)] == pat]
        ppu_hits[name] = {"count": len(offs), "offsets": [hex(o) for o in offs[:16]]}

    banks = []
    for b in range(0, len(data), 0x8000):
        chunk = data[b : b + 0x8000]
        if len(chunk) < 0x100:
            continue
        banks.append({"offset": hex(b), "entropy": round(entropy(chunk), 3)})
    banks.sort(key=lambda x: x["entropy"], reverse=True)

    return {
        "ppu_register_hits": ppu_hits,
        "banks_by_entropy": banks[:16],
        "note": "tile_likeness heuristic removed — nibbles<=15 matches any byte stream",
    }


def scan_genesis(data: bytes) -> dict:
    vdp_imm = []
    for i in range(len(data) - 4):
        w = struct.unpack_from(">H", data, i)[0]
        if w in (0xC000, 0xC004):
            vdp_imm.append(i)
    chunks = []
    for b in range(0, len(data), 0x10000):
        chunk = data[b : b + 0x10000]
        if len(chunk) < 0x100:
            continue
        chunks.append({"offset": hex(b), "entropy": round(entropy(chunk), 3)})
    chunks.sort(key=lambda x: x["entropy"], reverse=True)
    return {
        "vdp_port_word_hits": len(vdp_imm),
        "vdp_first_offsets": [hex(o) for o in vdp_imm[:16]],
        "chunks_by_entropy": chunks,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    data = args.rom.read_bytes()
    suffix = args.rom.suffix.lower()
    kind = "snes" if suffix in {".sfc", ".smc"} else "genesis"
    result = {
        "path": str(args.rom),
        "kind": kind,
        "size": len(data),
        "scan": scan_snes(data) if kind == "snes" else scan_genesis(data),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Wrote {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
