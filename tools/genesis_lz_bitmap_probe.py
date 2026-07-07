#!/usr/bin/env python3
"""Probe MM2 Genesis LZ blobs as *linear* 4bpp bitmaps (not 8x8 VDP tiles).

The first-block resource sizes cluster on non-32-aligned values (968, 1452,
1936, 2904 ...), which is inconsistent with raw VRAM CHR uploads but consistent
with fixed-size wall/sprite *bitmaps* the engine scales into the 3D view. This
renders a blob at several plausible row strides so a coherent image (if any)
becomes visible.

Usage:
    python tools/genesis_lz_bitmap_probe.py 0x37C00 0x401DA 0x940C2
"""

from __future__ import annotations

import importlib.util
import struct
import sys
from pathlib import Path

from PIL import Image

ROM = Path("EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen")
OUT_DIR = Path("EXTRACTED/genesis/gfx/catalog/probe")

_spec = importlib.util.spec_from_file_location(
    "genesis_lz_decompress", Path(__file__).with_name("genesis_lz_decompress.py")
)
_lz = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_lz)


def cram_to_rgb(word: int):
    r = (word >> 1) & 0x7
    g = (word >> 5) & 0x7
    b = (word >> 9) & 0x7
    s = lambda v: (v * 255) // 7
    return s(r), s(g), s(b)


def load_palette(off: int, idx: int = 0):
    _, _, blob = _lz.read_resource(ROM.read_bytes(), off)
    words = [struct.unpack_from(">H", blob, i)[0] for i in range(0, len(blob) & ~1, 2)]
    cols = [cram_to_rgb(w) for w in words]
    pal = cols[idx * 16 : idx * 16 + 16]
    return pal + [(0, 0, 0)] * (16 - len(pal))


def render_linear(blob: bytes, width_px: int, pal, hi_first: bool = True) -> Image.Image:
    bpr = max(1, width_px // 2)  # bytes per row
    rows = (len(blob) + bpr - 1) // bpr
    img = Image.new("RGB", (width_px, rows), (30, 30, 40))
    px = img.load()
    for i, byte in enumerate(blob):
        y = i // bpr
        x = (i % bpr) * 2
        a, b = (byte >> 4, byte & 0xF) if hi_first else (byte & 0xF, byte >> 4)
        if x < width_px:
            px[x, y] = pal[a]
        if x + 1 < width_px:
            px[x + 1, y] = pal[b]
    return img


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    offs = [int(a, 0) for a in sys.argv[1:]] or [0x37C00]
    rom = ROM.read_bytes()
    pal = load_palette(0x9389E, 0)
    for off in offs:
        _, _, blob = _lz.read_resource(rom, off)
        n = len(blob)
        # Candidate pixel widths (stacked vertically, scaled up, labelled).
        cands = [32, 44, 48, 64, 66, 80, 88, 96, 128, 176]
        from PIL import ImageDraw
        scale = 3
        gap = 10
        panels = []
        for w in cands:
            img = render_linear(blob, w, pal)
            up = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
            panels.append((w, up))
        total_w = max(p.width for _, p in panels) + 60
        total_h = sum(p.height + gap for _, p in panels) + 8
        sheet = Image.new("RGB", (total_w, total_h), (10, 10, 14))
        d = ImageDraw.Draw(sheet)
        y = 4
        for w, up in panels:
            d.text((2, y), f"{w}", fill=(220, 220, 120))
            sheet.paste(up, (56, y))
            y += up.height + gap
        path = OUT_DIR / f"{off:06X}_widths.png"
        sheet.save(path)
        print(f"0x{off:X} bytes={n} -> {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
