#!/usr/bin/env python3
"""Cross-check outdoor blit tables vs capstone/mm2.asm and mm2_data_00.bin."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
A4 = 0x7FFE

EXPECTED = {
    "6BC2 L1 y": (0x6BC2, [21, 21, 42, 50]),
    "6BCA L1 x": (0x6BCA, [40, 40, 64, 88]),
    "6BCE L1 fr": (0x6BCE, [0, 0, 1, 2], "u8"),
    "6BAA L2 y": (0x6BAA, [36, 46, 50, 58]),
    "6BB2 L2 x": (0x6BB2, [8, 16, 32, 88]),
    "6BBA L2 fr": (0x6BBA, [4, 5, 2, 3]),
    "6B96 L3 y": (0x6B96, [36, 46, 50, 58]),
    "6B9E L3 x": (0x6B9E, [176, 152, 136, 120]),
    "6BA2 L3 fr": (0x6BA2, [6, 7, 2, 3], "u8"),
    "6BD6": (0x6BD6, [20, 35, 50, 60]),
    "6BDE": (0x6BDE, [184, 160, 136, 112]),
}


def read_table(blob: bytes, a4off: int, n: int, kind: str) -> list[int]:
    off = A4 - a4off
    if kind == "u8":
        return list(blob[off : off + n])
    return [struct.unpack_from(">h", blob, off + i * 2)[0] for i in range(n)]


def main() -> int:
    blob = DATA.read_bytes()
    ok = True
    for name, spec in EXPECTED.items():
        a4off, want = spec[0], spec[1]
        kind = spec[2] if len(spec) > 2 else "u16"
        got = read_table(blob, a4off, len(want), kind)
        if got != want:
            print(f"MISMATCH {name}: disk={got} want={want}")
            ok = False
        else:
            print(f"OK {name}: {got}")
    decor_y = [0x80 - v for v in read_table(blob, 0x6BD6, 4, "u16")]
    print(f"decor y ($80-6BD6): {decor_y}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
