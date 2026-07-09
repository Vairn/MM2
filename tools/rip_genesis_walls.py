#!/usr/bin/env python3
"""Rip Genesis MM2 first-person wall / door / outdoor strips.

ASM-confirmed pipeline (``$66(A5)`` = ``0x29A22`` banked LZ):

- Env table fill @ ``0xFC40`` / jump table ``0xFC56`` stores ROM pointers
  into ``-$35D2(A5)`` …
- Each resource is prefixed with ``u16 chunk_bytes`` + ``u16 row_count``
  immediately before the standard 8-byte LZ header.
- ``chunk_bytes`` = bytes per scanline (4bpp → width = chunk*2 pixels).

Palette (ASM @ ``0x8140``, user-confirmed):

| Theme | Strip pool | CRAM line |
|-------|------------|-----------|
| Castle (2/5) | ``0x6FFAE``–``0x776F4`` | L0 ``0x6FBEA`` |
| Town (0) | ``0x7780A``–``0x803A8`` | L1 ``0x6FC0A`` |
| Cavern (1) | same as town | L2 ``0x6FC2A`` |
| Outdoor (3/4/6) | ``0x9E846``–``0xA5896`` | L3 ``0x6FC4A`` |

Outputs under ``EXTRACTED/genesis/gfx/catalog/env/``.
"""

from __future__ import annotations

import importlib.util
import json
import struct
import sys
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw

spec = importlib.util.spec_from_file_location("lz", "tools/genesis_lz_decompress.py")
lz = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lz)

ROM = Path("EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen")
OUT = Path("EXTRACTED/genesis/gfx/catalog/env")
ANALYSIS = Path("EXTRACTED/genesis/analysis/env_lz_catalog.json")

PAL_BASE = 0x6FBEA
PAL_LINE_STRIDE = 0x20
FILL_RANGES = [(0xFC00, 0x10300)]

CASTLE_LO, CASTLE_HI = 0x6FFAE, 0x7780A
TOWN_LO, OUTDOOR_LO = 0x7780A, 0x9E000

THEME_NAMES = {
    0: "castle",
    1: "town",
    2: "cavern",
    3: "outdoor",
}


def cram(word: int) -> tuple[int, int, int]:
    r = (word >> 1) & 7
    g = (word >> 5) & 7
    b = (word >> 9) & 7
    s = lambda v: v * 255 // 7
    return s(r), s(g), s(b)


def pal_line(rom: bytes, n: int) -> list[tuple[int, int, int]]:
    base = PAL_BASE + n * PAL_LINE_STRIDE
    return [cram(struct.unpack_from(">H", rom, base + i)[0]) for i in range(0, 32, 2)]


def theme_for_offset(off: int) -> tuple[str, list[int]]:
    """Return (primary_theme_name, list of (line, tag) to rip)."""
    if CASTLE_LO <= off < CASTLE_HI:
        return "castle", [(0, "L0")]
    if TOWN_LO <= off < OUTDOOR_LO:
        # Same art: town uses L1, cavern uses L2
        return "town_cavern", [(1, "L1_town"), (2, "L2_cavern")]
    if off >= OUTDOOR_LO:
        return "outdoor", [(3, "L3")]
    return "other", [(0, "L0")]


def collect_env_ptrs(rom: bytes) -> list[int]:
    ptrs: set[int] = set()
    for lo, hi in FILL_RANGES:
        for at in range(lo, hi - 6):
            if rom[at : at + 2] == b"\x2e\xbc":
                imm = struct.unpack_from(">I", rom, at + 2)[0]
                if 0x6FF00 <= imm <= 0xA6000:
                    ptrs.add(imm)
    return sorted(ptrs)


def read_env_resource(rom: bytes, off: int) -> dict:
    chunk = struct.unpack_from(">H", rom, off - 4)[0]
    rows = struct.unpack_from(">H", rom, off - 2)[0]
    meta0, meta1, blob = lz.read_resource(rom, off)
    return {
        "offset": off,
        "offset_hex": hex(off),
        "chunk_bytes": chunk,
        "row_count": rows,
        "comp_len": meta0,
        "out_size": meta1,
        "decoded_len": len(blob),
        "width_px": chunk * 2,
        "height_px": len(blob) // chunk if chunk else 0,
        "size_ok": chunk > 0 and len(blob) == meta1 and (len(blob) % chunk == 0),
        "blob": blob,
    }


def render_4bpp_strip(blob: bytes, row_bytes: int, pal: list, scale: int = 2) -> Image.Image:
    h = len(blob) // row_bytes
    w = row_bytes * 2
    img = Image.new("RGB", (w, h), (0, 0, 0))
    px = img.load()
    for y in range(h):
        row = blob[y * row_bytes : (y + 1) * row_bytes]
        for x, byte in enumerate(row):
            px[x * 2, y] = pal[byte >> 4]
            px[x * 2 + 1, y] = pal[byte & 0xF]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def main() -> int:
    rom = ROM.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)

    # Swatch all 4 CRAM lines with theme labels
    sw = Image.new("RGB", (16 * 28 + 120, 4 * 28 + 28), (20, 20, 24))
    d = ImageDraw.Draw(sw)
    d.text((2, 2), "CRAM 0x6FBEA — real env palettes", fill=(220, 220, 100))
    labels = ["L0 castle", "L1 town", "L2 cavern", "L3 outdoor"]
    lines = []
    for n in range(4):
        pal = pal_line(rom, n)
        lines.append(pal)
        for i, c in enumerate(pal):
            d.rectangle([i * 28, 20 + n * 28, i * 28 + 26, 20 + n * 28 + 26], fill=c)
        d.text(
            (16 * 28 + 4, 24 + n * 28),
            f"{labels[n]} {hex(PAL_BASE + n * PAL_LINE_STRIDE)}",
            fill=(180, 180, 100),
        )
    sw.save(OUT / "pal_env_6FBEA_lines.png")

    ptrs = collect_env_ptrs(rom)
    entries = []
    by_theme_chunk: dict[tuple[str, int], list] = defaultdict(list)

    for off in ptrs:
        rec = read_env_resource(rom, off)
        blob = rec.pop("blob")
        theme, rips = theme_for_offset(off)
        rec["theme"] = theme
        entries.append(rec)
        if not rec["size_ok"]:
            print(f"SKIP 0x{off:X}")
            continue
        chunk = rec["chunk_bytes"]
        scale = 2 if chunk >= 40 else 3
        for line_i, tag in rips:
            img = render_4bpp_strip(blob, chunk, lines[line_i], scale=scale)
            img.save(OUT / f"strip_{off:06X}_c{chunk}_{tag}.png")
            by_theme_chunk[(tag, chunk)].append((off, img))
        print(f"0x{off:X} {rec['width_px']}x{rec['height_px']} chunk={chunk} {theme}")

    ANALYSIS.parent.mkdir(parents=True, exist_ok=True)
    ANALYSIS.write_text(
        json.dumps(
            {
                "palette_base": hex(PAL_BASE),
                "line_stride": PAL_LINE_STRIDE,
                "lines": {
                    "L0": "castle grey brick (theme 2/5)",
                    "L1": "town bluish stone (theme 0) — user-confirmed",
                    "L2": "cavern grey (theme 1, same art as town) — user-confirmed",
                    "L3": "outdoor greens (theme 3/4/6) — user-confirmed",
                },
                "pools": {
                    "castle": f"{hex(CASTLE_LO)}..{hex(CASTLE_HI - 1)}",
                    "town_cavern": f"{hex(TOWN_LO)}..{hex(OUTDOOR_LO - 1)}",
                    "outdoor": f"{hex(OUTDOOR_LO)}+",
                },
                "count": len(entries),
                "tables": entries,
            },
            indent=2,
        )
    )

    for (tag, chunk), items in sorted(by_theme_chunk.items()):
        gap = 4
        max_h = max(im.height for _, im in items)
        total_w = sum(im.width + gap for _, im in items) + 8
        sheet = Image.new("RGB", (total_w, max_h + 20), (10, 10, 14))
        d = ImageDraw.Draw(sheet)
        x = 4
        for off, im in items:
            d.text((x, 2), f"{off:X}", fill=(220, 220, 100))
            sheet.paste(im, (x, 16))
            x += im.width + gap
        path = OUT / f"_strips_chunk{chunk}_{tag}.png"
        sheet.save(path)
        print("->", path, f"n={len(items)}")

    print(f"catalog {ANALYSIS} ({len(entries)} resources)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
