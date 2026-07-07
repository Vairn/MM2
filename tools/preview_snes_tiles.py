#!/usr/bin/env python3
"""Preview SNES 4bpp tiles from ROM using a BGR555 palette."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore


def snes_color(w: int) -> tuple[int, int, int]:
    r = (w & 0x1F) << 3
    g = ((w >> 5) & 0x1F) << 3
    b = ((w >> 10) & 0x1F) << 3
    return r | (r >> 5), g | (g >> 5), b | (b >> 5)


def decode_tile(block: bytes, pal: list[tuple[int, int, int]]) -> Image.Image:
    if Image is None:
        raise SystemExit("pip install pillow")
    px = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    # SNES 4bpp: 4 bitplanes, 8 bytes each for rows 0-7
    planes = [block[i * 8 : (i + 1) * 8] for i in range(4)]
    for y in range(8):
        for x in range(8):
            bit = 7 - x
            mask = 1 << bit
            color = 0
            for p in range(4):
                if planes[p][y] & mask:
                    color |= 1 << p
            px.putpixel((x, y), (*pal[color], 255) if color else (0, 0, 0, 0))
    return px


def load_palette(rom: bytes, off: int, count: int = 16) -> list[tuple[int, int, int]]:
    pal = [(0, 0, 0)]
    for i in range(1, count):
        w = struct.unpack_from("<H", rom, off + i * 2)[0]
        pal.append(snes_color(w))
    while len(pal) < 16:
        pal.append((0, 0, 0))
    return pal


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--offset", type=lambda x: int(x, 0), default=0x20000)
    ap.add_argument("--palette-offset", type=lambda x: int(x, 0), default=0x6490)
    ap.add_argument("--tiles", type=int, default=64)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    if Image is None:
        raise SystemExit("pip install pillow")

    rom = args.rom.read_bytes()
    pal = load_palette(rom, args.palette_offset)
    cols = 16
    rows = (args.tiles + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * 8, rows * 8), (32, 32, 32, 255))
    for t in range(args.tiles):
        off = args.offset + t * 32
        if off + 32 > len(rom):
            break
        tile = decode_tile(rom[off : off + 32], pal)
        x = (t % cols) * 8
        y = (t // cols) * 8
        sheet.paste(tile, (x, y))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"Wrote {args.output} ({args.tiles} tiles @ 0x{args.offset:X}, pal @ 0x{args.palette_offset:X})", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
