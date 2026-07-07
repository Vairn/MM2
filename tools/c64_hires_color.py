#!/usr/bin/env python3
"""Render a C64 hires bitmap with per-cell colour from screen RAM.

Hires mode: bitmap at <bmp>, screen matrix at <scr>. Per 8x8 cell, screen byte
hi-nibble = colour for bit=1 pixels, lo-nibble = colour for bit=0 pixels.

Usage: c64_hires_color.py mem.bin -o out.png --bmp 0x2000 --scr 0x0400
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

C64 = [
    (0, 0, 0), (255, 255, 255), (136, 0, 0), (170, 255, 238),
    (204, 68, 204), (0, 204, 85), (0, 0, 170), (238, 238, 119),
    (221, 136, 85), (102, 68, 0), (255, 119, 119), (51, 51, 51),
    (119, 119, 119), (170, 255, 102), (0, 136, 255), (187, 187, 187),
]


def render(mem: bytes, bmp: int, scr: int) -> Image.Image:
    img = Image.new("RGB", (320, 200))
    px = img.load()
    for cy in range(25):
        for cx in range(40):
            sb = mem[scr + cy * 40 + cx] if scr + cy * 40 + cx < len(mem) else 0
            fg = C64[(sb >> 4) & 0xF]
            bg = C64[sb & 0xF]
            cell = bmp + (cy * 40 + cx) * 8
            for line in range(8):
                b = mem[cell + line] if cell + line < len(mem) else 0
                y = cy * 8 + line
                for bit in range(8):
                    px[cx * 8 + bit, y] = fg if (b >> (7 - bit)) & 1 else bg
    return img


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("mem", type=Path)
    ap.add_argument("-o", "--out", type=Path, required=True)
    ap.add_argument("--bmp", type=lambda x: int(x, 0), default=0x2000)
    ap.add_argument("--scr", type=lambda x: int(x, 0), default=0x0400)
    args = ap.parse_args()
    mem = args.mem.read_bytes()
    render(mem, args.bmp, args.scr).save(args.out)
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
