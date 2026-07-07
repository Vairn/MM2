#!/usr/bin/env python3
"""Render every ROM->VRAM CHR source from the SNES MM2 DMA table.

For triage we render each region with a 16-level GRAYSCALE ramp so tile
structure is visible regardless of the (unknown) real palette. A separate pass
can re-render coherent regions with a real BGR555 palette via snes_decompress.py.

Source of truth = EXTRACTED/snes/analysis/dma_table.json (produced by the
corrected extract_snes_dma.py: source addr = A1B0:A1T0H:A1T0L, LoROM).
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from snes_decompress import decode_4bpp_tile, bgr555_to_rgb, read_palette_words  # noqa: E402


def gray_ramp(levels: int = 16) -> list[tuple[int, int, int]]:
    return [(v, v, v) for v in [round(i * 255 / (levels - 1)) for i in range(levels)]]


def decode_8bpp_tile(block: bytes) -> list[list[int]]:
    """8x8 palette indices from a 64-byte SNES 8bpp tile (planes 0-7)."""
    px = [[0] * 8 for _ in range(8)]
    for y in range(8):
        # 8bpp = 4 bitplane-pairs: pair k at bytes 16*k+2*y (plane 2k) / +1 (plane 2k+1).
        p = [block[16 * (pp // 2) + y * 2 + (pp % 2)] for pp in range(8)]
        for x in range(8):
            bit = 7 - x
            val = 0
            for pl in range(8):
                val |= ((p[pl] >> bit) & 1) << pl
            px[y][x] = val
    return px


def render(chr_bytes: bytes, pal: list[tuple[int, int, int]], cols: int, scale: int,
           bpp: int = 4) -> Image.Image:
    tsize = 32 if bpp == 4 else 64
    dec = decode_4bpp_tile if bpp == 4 else decode_8bpp_tile
    ntiles = len(chr_bytes) // tsize
    rows = max(1, (ntiles + cols - 1) // cols)
    img = Image.new("RGB", (cols * 8, rows * 8), (32, 0, 32))
    px = img.load()
    for t in range(ntiles):
        tile = dec(chr_bytes[t * tsize:t * tsize + tsize])
        ox, oy = (t % cols) * 8, (t // cols) * 8
        for y in range(8):
            for x in range(8):
                px[ox + x, oy + y] = pal[tile[y][x]]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--table", type=Path, required=True)
    ap.add_argument("--outdir", type=Path, required=True)
    ap.add_argument("--cols", type=int, default=16)
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--bpp", type=int, default=4, choices=(4, 8))
    ap.add_argument("--palette-offset", type=lambda x: int(x, 0), default=None,
                    help="Optional BGR555 palette file offset (else grayscale)")
    ap.add_argument("--sub", type=int, default=0)
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    d = json.loads(args.table.read_text())
    args.outdir.mkdir(parents=True, exist_ok=True)

    ncolors = 16 if args.bpp == 4 else 256
    if args.palette_offset is not None:
        words = read_palette_words(rom, args.palette_offset, ncolors)
        pal = [bgr555_to_rgb(w) for w in words]
        if args.bpp == 4:
            pal = [bgr555_to_rgb(w) for w in
                   read_palette_words(rom, args.palette_offset + args.sub * 32, 16)]
        tag = f"pal{args.palette_offset:X}s{args.sub}_{args.bpp}bpp"
    else:
        pal = gray_ramp(ncolors)
        tag = f"gray_{args.bpp}bpp"

    seen = set()
    index = []
    for s in d["all_setups"]:
        if s.get("kind") != "rom" or s["bbad"] == "0x22":
            continue
        fo = int(s["file_offset"], 16)
        size = s["size_bytes"]
        key = (fo, size)
        if key in seen:
            continue
        seen.add(key)
        chr_bytes = rom[fo:fo + size]
        img = render(chr_bytes, pal, args.cols, args.scale, args.bpp)
        name = f"{s['snes_addr'].replace('$','').replace(':','_')}_{fo:06X}_{size}_{tag}.png"
        out = args.outdir / name
        img.save(out)
        entry = {
            "snes_addr": s["snes_addr"],
            "file_offset": s["file_offset"],
            "size_bytes": size,
            "tiles": size // 32,
            "dest_vram": s["dest_vram"],
            "setup_offset": s["setup_offset"],
            "png": str(out).replace("\\", "/"),
        }
        index.append(entry)
        print(f"{s['snes_addr']:>10}  file={s['file_offset']:>9}  {size:>6}B  "
              f"{size//32:>4} tiles  -> {out}")

    (args.outdir / "catalog_index.json").write_text(json.dumps(index, indent=2))
    print(f"\n{len(index)} regions -> {args.outdir}/catalog_index.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
