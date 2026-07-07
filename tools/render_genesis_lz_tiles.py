#!/usr/bin/env python3
"""Render MM2 Genesis LZ-decoded blobs as 4bpp tiles + CRAM palette.

Decodes a resource table via ``genesis_lz_decompress.decompress_lz`` and
renders the output as standard Genesis/Mega Drive 4bpp planar tiles
(32 bytes/tile, 8x8 px, row = 4 bytes, each byte = 2 pixels high-nibble
left). Palette tables are decoded as CRAM 9-bit BGR words.

Usage:
    python tools/render_genesis_lz_tiles.py
"""

from __future__ import annotations

import importlib.util
import json
import struct
from pathlib import Path

from PIL import Image

ROM = Path("EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen")
OUT_DIR = Path("EXTRACTED/genesis/gfx")

_spec = importlib.util.spec_from_file_location(
    "genesis_lz_decompress", Path(__file__).with_name("genesis_lz_decompress.py")
)
_lz = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_lz)


def cram_to_rgb(word: int) -> tuple[int, int, int]:
    """9-bit BGR CRAM word -> 8-bit RGB. Layout: 0000 BBB0 GGG0 RRR0."""
    r = (word >> 1) & 0x7
    g = (word >> 5) & 0x7
    b = (word >> 9) & 0x7
    scale = lambda v: (v * 255) // 7
    return scale(r), scale(g), scale(b)


def decode_palettes(blob: bytes) -> list[list[tuple[int, int, int]]]:
    """Split a CRAM blob into 16-color palettes."""
    words = [struct.unpack_from(">H", blob, i)[0] for i in range(0, len(blob) & ~1, 2)]
    colors = [cram_to_rgb(w) for w in words]
    pals = [colors[i : i + 16] for i in range(0, len(colors), 16)]
    if pals and len(pals[-1]) < 16:
        pals[-1] += [(0, 0, 0)] * (16 - len(pals[-1]))
    return pals


def render_tiles(blob: bytes, palette: list[tuple[int, int, int]], cols: int = 16) -> Image.Image:
    n_tiles = len(blob) // 32
    if n_tiles == 0:
        return Image.new("RGB", (8, 8), (0, 0, 0))
    rows = (n_tiles + cols - 1) // cols
    img = Image.new("RGB", (cols * 8, rows * 8), (32, 32, 48))
    px = img.load()
    pal = palette + [(0, 0, 0)] * (16 - len(palette))
    for t in range(n_tiles):
        base = t * 32
        tx = (t % cols) * 8
        ty = (t // cols) * 8
        for row in range(8):
            for bcol in range(4):
                byte = blob[base + row * 4 + bcol]
                hi = byte >> 4
                lo = byte & 0x0F
                px[tx + bcol * 2, ty + row] = pal[hi]
                px[tx + bcol * 2 + 1, ty + row] = pal[lo]
    return img


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    rom = ROM.read_bytes()

    # Palette table.
    _, pal_size, pal_blob = _lz.read_resource(rom, 0xB12)
    palettes = decode_palettes(pal_blob)
    pal0 = palettes[0] if palettes else [(0, 0, 0)] * 16

    # Palette swatch strip.
    sw = Image.new("RGB", (16 * 16, len(palettes) * 16), (0, 0, 0))
    swpx = sw.load()
    for pi, pal in enumerate(palettes):
        for ci, col in enumerate(pal):
            for yy in range(16):
                for xx in range(16):
                    swpx[ci * 16 + xx, pi * 16 + yy] = col
    sw.save(OUT_DIR / "palette_B12.png")

    tile_tables = {
        "tiles_C52": 0xC52,
        "tiles_13D8": 0x13D8,
        "tiles_1262": 0x1262,
    }
    results = {}
    for name, off in tile_tables.items():
        _, out_size, blob = _lz.read_resource(rom, off)
        img = render_tiles(blob, pal0)
        path = OUT_DIR / f"{name}.png"
        # 4x upscale for legibility.
        img.resize((img.width * 4, img.height * 4), Image.NEAREST).save(path)
        results[name] = {
            "offset": off,
            "out_size": out_size,
            "decoded": len(blob),
            "tiles": len(blob) // 32,
            "png": str(path).replace("\\", "/"),
        }

    print(json.dumps({"palettes": len(palettes), "tables": results}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
