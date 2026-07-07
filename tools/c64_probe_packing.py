#!/usr/bin/env python3
"""Rule out trivial (non-LZ) packing on C64 MM2 blobs.

The boot payload is proven to be packed (entropy ~7.0, only embedded code
fragments). If the packing were a simple reversible transform (XOR constant,
running XOR, nibble swap, bit reverse) we should be able to recover low-entropy
6502 code. Try each and report the best entropy / opcode-ratio achieved.

Run: python tools/c64_probe_packing.py
"""
from __future__ import annotations

import math
from collections import Counter
from pathlib import Path

from c64_d64_layout import KNOWN_OPS, ROOT

BITREV = [int(f"{b:08b}"[::-1], 2) for b in range(256)]


def ent(b: bytes) -> float:
    if not b:
        return 0.0
    c = Counter(b)
    n = len(b)
    return -sum((v / n) * math.log2(v / n) for v in c.values())


def opratio(b: bytes) -> float:
    if not b:
        return 0.0
    return sum(x in KNOWN_OPS for x in b) / len(b)


def transforms(data: bytes):
    yield "identity", data
    # XOR constant
    best_k, best_e = 0, 9.0
    for k in range(256):
        t = bytes(x ^ k for x in data)
        e = ent(t)
        if e < best_e:
            best_e, best_k = e, k
    yield f"xor#${best_k:02X}", bytes(x ^ best_k for x in data)
    # running xor (delta decode)
    prev = 0
    out = bytearray()
    for x in data:
        out.append(x ^ prev)
        prev = x
    yield "running-xor", bytes(out)
    # add/sub rolling
    out = bytearray()
    prev = 0
    for x in data:
        out.append((x - prev) & 0xFF)
        prev = x
    yield "delta-sub", bytes(out)
    # nibble swap
    yield "nibble-swap", bytes(((x >> 4) | (x << 4)) & 0xFF for x in data)
    # bit reverse
    yield "bit-reverse", bytes(BITREV[x] for x in data)
    # xor with position (weak cipher)
    yield "xor-index", bytes(x ^ (i & 0xFF) for i, x in enumerate(data))


def probe(label: str, data: bytes) -> None:
    base_e = ent(data)
    print(f"\n### {label}  len={len(data)} base_entropy={base_e:.2f}")
    for name, t in transforms(data):
        print(f"  {name:14} entropy={ent(t):.2f}  op_ratio={opratio(t):.2f}")


def main() -> None:
    reloc = (ROOT / "EXTRACTED/c64/emulator/mm2-1_relocated.bin").read_bytes()
    probe("boot packed payload $0820-$18FF", reloc[0x20:0x1100])

    track = (ROOT / "EXTRACTED/c64/gfx/tracks/mm2-1/t08.bin").read_bytes()
    probe("data track MM2-1 T08 (payload region)", track[0x200:0x1200])

    track3 = (ROOT / "EXTRACTED/c64/gfx/tracks/mm2-2/t07.bin").read_bytes()
    probe("data track MM2-2 T07", track3[0x200:0x1200])


if __name__ == "__main__":
    main()
