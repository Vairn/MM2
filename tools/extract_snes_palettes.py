#!/usr/bin/env python3
"""Extract inline SNES CGRAM (palette) upload sequences from ROM."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path


def extract_cgram_uploads(data: bytes) -> list[dict]:
    uploads = []
    i = 0
    while i < len(data) - 6:
        # STA $2121 (CGADD)
        if data[i : i + 3] == bytes([0x8D, 0x21, 0x21]):
            # preceding LDA #imm for CGADD
            cgadd = None
            if i >= 2 and data[i - 2] == 0xA9:
                cgadd = data[i - 1]
            colors: list[int] = []
            j = i + 3
            while j < len(data) - 3:
                if data[j] == 0xA9 and data[j + 2 : j + 5] == bytes([0x8D, 0x22, 0x21]):
                    colors.append(data[j + 1])
                    j += 5
                elif data[j : j + 3] == bytes([0x8D, 0x22, 0x21]):
                    j += 3
                else:
                    break
            if len(colors) >= 4:
                uploads.append(
                    {
                        "cgadd_setup_offset": hex(i),
                        "cgadd": cgadd,
                        "color_bytes": [hex(c) for c in colors],
                        "count": len(colors),
                    }
                )
            i = j
        else:
            i += 1
    return uploads


def snes15_to_rgb(b: int) -> tuple[int, int, int]:
    # 8-bit SNES CGRAM upload uses high byte of 15-bit color in some loaders
    w = b | (b << 8)  # expand 8-bit to duplicated low/high - rough preview
    r = (w & 0x1F) << 3
    g = ((w >> 5) & 0x1F) << 3
    b_ = ((w >> 10) & 0x1F) << 3
    return r | (r >> 5), g | (g >> 5), b_ | (b_ >> 5)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    data = args.rom.read_bytes()
    uploads = extract_cgram_uploads(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(uploads[:30], indent=2), encoding="utf-8")
    print(f"Found {len(uploads)} CGRAM upload sequences", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
