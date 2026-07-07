#!/usr/bin/env python3
"""Probe MM2 Genesis LZ gfx blobs as VDP *sprites* (column-major tile order).

Mega Drive sprite cell data is stored column-major: for a sprite W tiles wide
and H tiles tall, cells run top->bottom of column 0, then column 1, etc. Raw
row-major tile grids therefore scramble sprites. This renders a blob at several
sprite tile-grid widths using column-major cell order so a coherent sprite (a
wall block, monster, portrait) becomes visible if that is the true layout.

Usage:
    python tools/genesis_lz_sprite_probe.py 0x37C00 0x401DA
"""

from __future__ import annotations

import importlib.util
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

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


def load_palette(rom: bytes, off: int, idx: int = 0):
    _, _, blob = _lz.read_resource(rom, off)
    words = [struct.unpack_from(">H", blob, i)[0] for i in range(0, len(blob) & ~1, 2)]
    cols = [cram_to_rgb(w) for w in words]
    pal = cols[idx * 16 : idx * 16 + 16]
    return pal + [(0, 0, 0)] * (16 - len(pal))


def draw_tile(px, blob, base, x0, y0, pal):
    for row in range(8):
        for bcol in range(4):
            if base + row * 4 + bcol >= len(blob):
                return
            byte = blob[base + row * 4 + bcol]
            px[x0 + bcol * 2, y0 + row] = pal[byte >> 4]
            px[x0 + bcol * 2 + 1, y0 + row] = pal[byte & 0x0F]


def render_sprite(blob, tiles_w, pal, column_major=True):
    n = len(blob) // 32
    tiles_h = (n + tiles_w - 1) // tiles_w
    img = Image.new("RGB", (tiles_w * 8, tiles_h * 8), (30, 30, 40))
    px = img.load()
    for t in range(n):
        if column_major:
            col = t // tiles_h
            row = t % tiles_h
        else:
            col = t % tiles_w
            row = t // tiles_w
        draw_tile(px, blob, t * 32, col * 8, row * 8, pal)
    return img


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    offs = [int(a, 0) for a in sys.argv[1:]] or [0x37C00]
    rom = ROM.read_bytes()
    pal = load_palette(rom, 0x9389E, 0)
    for off in offs:
        _, _, blob = _lz.read_resource(rom, off)
        n = len(blob) // 32
        cands = [w for w in (2, 3, 4, 5, 6, 8, 10, 11, 12, 16) if w <= max(2, n)]
        scale = 4
        gap = 12
        panels = []
        for w in cands:
            img = render_sprite(blob, w, pal, column_major=True)
            up = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
            panels.append((w, up))
        total_w = sum(p.width + gap for _, p in panels) + 8
        total_h = max(p.height for _, p in panels) + 18
        sheet = Image.new("RGB", (total_w, total_h), (10, 10, 14))
        d = ImageDraw.Draw(sheet)
        x = 6
        for w, up in panels:
            d.text((x, 2), f"{w}w", fill=(220, 220, 120))
            sheet.paste(up, (x, 16))
            x += up.width + gap
        path = OUT_DIR / f"{off:06X}_sprite.png"
        sheet.save(path)
        print(f"0x{off:X} tiles={n} -> {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
