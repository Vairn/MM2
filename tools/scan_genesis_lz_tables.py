#!/usr/bin/env python3
"""Scan the MM2 Genesis ROM for 8-byte-header + inline-LZ resource tables.

A valid table (per resource_29954.asm) has an 8-byte big-endian header:
    +0  u32  compressed payload length (bytes consumed from stream)
    +4  u32  decompressed output length (D0 counter)
    +8  inline LZSS bitstream

The decisive signal is that decoding exactly ``out_size`` bytes consumes
exactly ``metadata`` bytes of the stream (token parsing lands on the recorded
compressed size). Random data almost never satisfies this. We also require an
entropy drop (compressed stream more random than decoded output).
"""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path

RING_MASK = 0xFFF
RING_START = 0xFEE


def _decode_track(src: bytes, base: int, out_size: int, meta: int):
    """Decode, returning (produced, consumed) or None if consumed overruns meta."""
    i = base + 8
    end = base + 8 + meta
    out = bytearray()
    ring = bytearray(0x1000)
    for j in range(RING_START):
        ring[j] = 0x20
    r = RING_START
    flags = 0
    n = len(src)
    while len(out) < out_size:
        if i > end + 2 or i >= n:
            return None
        flags >>= 1
        if (flags & 0x100) == 0:
            flags = src[i] | 0xFF00
            i += 1
        if flags & 1:
            b = src[i]
            i += 1
            out.append(b)
            ring[r] = b
            r = (r + 1) & RING_MASK
        else:
            b0 = src[i]
            b1 = src[i + 1]
            i += 2
            offset = b0 | ((b1 & 0xF0) << 4)
            length = (b1 & 0x0F) + 3
            for k in range(length):
                if len(out) >= out_size:
                    break
                b = ring[(offset + k) & RING_MASK]
                out.append(b)
                ring[r] = b
                r = (r + 1) & RING_MASK
    return bytes(out), i - (base + 8)


def _entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = [0] * 256
    for b in data:
        counts[b] += 1
    n = len(data)
    h = 0.0
    for c in counts:
        if c:
            p = c / n
            h -= p * math.log2(p)
    return h


def scan(rom: bytes, start: int, end: int, min_out: int, max_out: int):
    hits = []
    for off in range(start, min(end, len(rom) - 8)):
        meta, osz = struct.unpack_from(">II", rom, off)
        if not (4 <= meta <= 0x10000):
            continue
        if not (min_out <= osz <= max_out):
            continue
        if osz < meta:  # LZSS: decompressed >= compressed
            continue
        if off + 8 + meta > len(rom):
            continue
        res = _decode_track(rom, off, osz, meta)
        if res is None:
            continue
        decoded, consumed = res
        if consumed != meta or len(decoded) != osz:
            continue
        comp_h = _entropy(rom[off + 8 : off + 8 + meta])
        dec_h = _entropy(decoded)
        if comp_h - dec_h < 0.3:  # require a real entropy drop
            continue
        hits.append(
            {
                "offset": off,
                "offset_hex": f"0x{off:X}",
                "compressed_len": meta,
                "out_size": osz,
                "ratio": round(osz / meta, 2),
                "comp_entropy": round(comp_h, 3),
                "dec_entropy": round(dec_h, 3),
            }
        )
    return hits


def dedupe(hits):
    """Greedily keep tables that do not start inside another's [off, off+8+meta)."""
    kept = []
    covered_end = -1
    for h in sorted(hits, key=lambda x: x["offset"]):
        if h["offset"] <= covered_end:
            continue
        kept.append(h)
        covered_end = h["offset"] + 8 + h["compressed_len"] - 1
    return kept


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--start", type=lambda x: int(x, 0), default=0x30000)
    ap.add_argument("--end", type=lambda x: int(x, 0), default=0xC0000)
    ap.add_argument("--min-out", type=lambda x: int(x, 0), default=0x40)
    ap.add_argument("--max-out", type=lambda x: int(x, 0), default=0x8000)
    ap.add_argument("-o", "--output", type=Path)
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    hits = scan(rom, args.start, args.end, args.min_out, args.max_out)
    kept = dedupe(hits)
    result = {
        "range": [f"0x{args.start:X}", f"0x{args.end:X}"],
        "count_raw": len(hits),
        "count": len(kept),
        "tables": kept,
    }
    text = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(text)
    print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
