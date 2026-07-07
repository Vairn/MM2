#!/usr/bin/env python3
"""Preview raw C64 hires bitmaps from binary blobs.

Usage:
  python tools/c64_bitmap_preview.py blob.bin -o out.png --offset 0
"""
from __future__ import annotations

import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow")

PAL = [(0, 0, 0), (255, 255, 255)]


def render_hires(data: bytes, offset: int = 0) -> Image.Image | None:
    if offset + 8000 > len(data):
        return None
    bmp = data[offset : offset + 8000]
    img = Image.new("RGB", (320, 200))
    px = img.load()
    for y in range(200):
        char_row = y // 8
        line = y & 7
        for x in range(320):
            col_idx = x // 8
            off = char_row * 320 + col_idx * 8 + line
            if off >= len(bmp):
                continue
            b = bmp[off]
            bit = (b >> (7 - (x & 7))) & 1
            px[x, y] = PAL[bit]
    return img


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("blob", type=Path)
    ap.add_argument("-o", "--out", type=Path, required=True)
    ap.add_argument("--offset", type=lambda x: int(x, 0), default=0)
    args = ap.parse_args()
    img = render_hires(args.blob.read_bytes(), args.offset)
    if img is None:
        raise SystemExit("need >= 8000 bytes from offset")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.out)
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
