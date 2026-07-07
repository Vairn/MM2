#!/usr/bin/env python3
"""Preview Genesis 4bpp tiles (standard interleaved nibble format)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore

# Default Genesis CRAM-like preview palette (9-bit BGR expanded)
DEFAULT_PAL = [
    (0, 0, 0),
    (0, 0, 170),
    (0, 170, 0),
    (0, 170, 170),
    (170, 0, 0),
    (170, 0, 170),
    (170, 85, 0),
    (170, 170, 170),
    (85, 85, 85),
    (85, 85, 255),
    (85, 255, 85),
    (85, 255, 255),
    (255, 85, 85),
    (255, 85, 255),
    (255, 255, 85),
    (255, 255, 255),
]


def genesis_color9(w: int) -> tuple[int, int, int]:
    r = ((w >> 1) & 7) * 36
    g = ((w >> 5) & 7) * 36
    b = ((w >> 9) & 7) * 36
    return min(r, 255), min(g, 255), min(b, 255)


def decode_tile(block: bytes, pal: list[tuple[int, int, int]]) -> Image.Image:
    if Image is None:
        raise SystemExit("pip install pillow")
    px = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    for y in range(8):
        for x in range(4):
            b = block[y * 4 + x // 2] if x < 8 else 0
            if x % 2 == 0:
                p0, p1 = (b >> 4) & 0xF, b & 0xF
            else:
                continue
            # Genesis stores 4 bytes per row = 8 pixels as packed nibbles
        for x in range(8):
            byte = block[y * 4 + x // 2]
            p = (byte >> 4) & 0xF if x % 2 == 0 else byte & 0xF
            c = pal[p] if p < len(pal) else (255, 0, 255)
            px.putpixel((x, y), (*c, 255) if p else (0, 0, 0, 0))
    return px


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--offset", type=lambda x: int(x, 0), default=0xB0000)
    ap.add_argument("--tiles", type=int, default=128)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    if Image is None:
        raise SystemExit("pip install pillow")

    rom = args.rom.read_bytes()
    cols = 16
    rows = (args.tiles + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * 8, rows * 8), (32, 32, 32, 255))
    for t in range(args.tiles):
        off = args.offset + t * 32
        if off + 32 > len(rom):
            break
        tile = decode_tile(rom[off : off + 32], DEFAULT_PAL)
        sheet.paste(tile, ((t % cols) * 8, (t // cols) * 8))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"Wrote {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
