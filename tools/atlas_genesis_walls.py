#!/usr/bin/env python3
"""Build theme-correct wall atlases for Genesis MM2 env strips.

User-confirmed palettes (CRAM ``0x6FBEA`` + ASM ``0x8140`` / fill ``0xFC56``):

| Theme | Loc ranges (``0xFB86``) | Strip pool | CRAM line |
|-------|-------------------------|------------|-----------|
| Castle 2/5 | 45–59 | ``0x6FFAE``–``0x776F4`` | L0 ``0x6FBEA`` |
| Town 0 | 0–4 | ``0x7780A``–``0x803A8`` | L1 ``0x6FC0A`` |
| Cavern 1 | 17–32 | same art as town | L2 ``0x6FC2A`` |
| Outdoor 3/4/6 | outdoor bands | ``0x9E846``–``0xA5896`` | L3 ``0x6FC4A`` |

Genesis has **no separate cavern strip art** — caverns recolour the town set.
"""

from __future__ import annotations

import importlib.util
import struct
import sys
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw

spec = importlib.util.spec_from_file_location("lz", "tools/genesis_lz_decompress.py")
lz = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lz)

ROM = Path("EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen")
OUT = Path("EXTRACTED/genesis/gfx/catalog/env/atlases")
PAL_BASE = 0x6FBEA

CASTLE_LO, CASTLE_HI = 0x6FFAE, 0x7780A
TOWN_LO, OUTDOOR_LO = 0x7780A, 0x9E000


def cram(word: int) -> tuple[int, int, int]:
    r = (word >> 1) & 7
    g = (word >> 5) & 7
    b = (word >> 9) & 7
    s = lambda v: v * 255 // 7
    return s(r), s(g), s(b)


def pal_line(rom: bytes, n: int) -> list[tuple[int, int, int]]:
    base = PAL_BASE + n * 0x20
    return [cram(struct.unpack_from(">H", rom, base + i)[0]) for i in range(0, 32, 2)]


def collect_ptrs(rom: bytes) -> list[int]:
    ptrs: set[int] = set()
    for at in range(0xFC00, 0x10300):
        if rom[at : at + 2] == b"\x2e\xbc":
            imm = struct.unpack_from(">I", rom, at + 2)[0]
            if 0x6FF00 <= imm <= 0xA6000:
                ptrs.add(imm)
    return sorted(ptrs)


def render(blob: bytes, chunk: int, pal: list, scale: int) -> Image.Image:
    h = len(blob) // chunk
    w = chunk * 2
    img = Image.new("RGB", (w, h), (0, 0, 0))
    px = img.load()
    for y in range(h):
        row = blob[y * chunk : (y + 1) * chunk]
        for x, byte in enumerate(row):
            px[x * 2, y] = pal[byte >> 4]
            px[x * 2 + 1, y] = pal[byte & 0xF]
    return img.resize((img.width * scale, img.height * scale), Image.NEAREST)


def load_items(rom: bytes, offs: list[int]) -> list[tuple[int, int, bytes]]:
    out = []
    for p in offs:
        chunk = struct.unpack_from(">H", rom, p - 4)[0]
        try:
            _, _, blob = lz.read_resource(rom, p)
        except Exception:
            continue
        if not chunk or len(blob) % chunk:
            continue
        out.append((p, chunk, blob))
    return out


def make_atlas(
    items: list[tuple[int, int, bytes]],
    pal: list,
    title: str,
    path: Path,
    scale_for_chunk,
) -> None:
    rendered = []
    for off, chunk, blob in items:
        img = render(blob, chunk, pal, scale_for_chunk(chunk))
        rendered.append((off, chunk, img))
    if not rendered:
        return

    by_h: dict[int, list] = defaultdict(list)
    for off, chunk, img in rendered:
        by_h[img.height].append((off, chunk, img))
    rows = [by_h[h] for h in sorted(by_h.keys(), reverse=True)]

    gap = 6
    label_h = 14
    row_heights = []
    row_widths = []
    for row in rows:
        row_heights.append(max(im.height for _, _, im in row) + label_h + gap)
        row_widths.append(sum(im.width + gap for _, _, im in row) + 8)
    W = max(row_widths)
    H = sum(row_heights) + 28
    sheet = Image.new("RGB", (W, H), (12, 12, 16))
    d = ImageDraw.Draw(sheet)
    d.text((6, 6), title, fill=(230, 220, 100))
    blob_h = {off: len(blob) // chunk for off, chunk, blob in items}
    y = 24
    for row in rows:
        x = 6
        for off, chunk, img in row:
            d.text((x, y), f"{off:X} {chunk*2}x{blob_h[off]}", fill=(180, 180, 120))
            sheet.paste(img, (x, y + label_h))
            x += img.width + gap
        y += max(im.height for _, _, im in row) + label_h + gap
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)
    print(f"-> {path} {sheet.size} n={len(rendered)}")


def split(items):
    return {
        "sides": [t for t in items if t[1] in (8, 12, 16)],
        "near": [t for t in items if t[1] in (24, 28, 29, 30, 48)],
        "doors": [t for t in items if t[1] == 80],
        "edges": [t for t in items if t[1] in (13, 17)],
        "big": [t for t in items if t[1] >= 48],
        "all": items,
    }


def main() -> int:
    rom = ROM.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)

    # Remove probe / wrong-palette leftovers
    for pat in (
        "cmp_*",
        "theme_*",
        "*_wrong*",
        "*_compare*",
        "atlas_dungeon_*",
        "atlas_sides_*",
        "palettes_L0_L3.png",
    ):
        for p in OUT.glob(pat):
            p.unlink()
            print(f"rm {p.name}")

    ptrs = collect_ptrs(rom)
    castle = [p for p in ptrs if CASTLE_LO <= p < CASTLE_HI]
    town = [p for p in ptrs if TOWN_LO <= p < OUTDOOR_LO]
    outdoor = [p for p in ptrs if p >= OUTDOOR_LO]

    pal0 = pal_line(rom, 0)
    pal1 = pal_line(rom, 1)
    pal2 = pal_line(rom, 2)
    pal3 = pal_line(rom, 3)

    c_items = load_items(rom, castle)
    t_items = load_items(rom, town)
    o_items = load_items(rom, outdoor)
    cs, ts, os_ = split(c_items), split(t_items), split(o_items)

    def sc_side(c: int) -> int:
        return 3 if c <= 16 else 2

    def sc_big(c: int) -> int:
        return 2

    # Castle — L0
    make_atlas(cs["sides"], pal0, "Genesis MM2 — CASTLE sides (L0 @ 0x6FBEA)", OUT / "atlas_castle_sides_L0.png", sc_side)
    make_atlas(cs["near"], pal0, "Genesis MM2 — CASTLE near faces (L0)", OUT / "atlas_castle_near_L0.png", sc_big)
    make_atlas(cs["doors"], pal0, "Genesis MM2 — CASTLE doors / panels (L0)", OUT / "atlas_castle_doors_L0.png", sc_big)
    make_atlas(cs["edges"], pal0, "Genesis MM2 — CASTLE edges (L0)", OUT / "atlas_castle_edges_L0.png", sc_side)
    make_atlas(cs["all"], pal0, "Genesis MM2 — CASTLE all strips (L0)", OUT / "atlas_castle_all_L0.png", sc_big)

    # Town — L1 (user-confirmed)
    make_atlas(ts["sides"], pal1, "Genesis MM2 — TOWN sides (L1 @ 0x6FC0A)", OUT / "atlas_town_sides_L1.png", sc_side)
    make_atlas(ts["near"], pal1, "Genesis MM2 — TOWN near faces (L1)", OUT / "atlas_town_near_L1.png", sc_big)
    make_atlas(ts["doors"], pal1, "Genesis MM2 — TOWN doors / panels (L1)", OUT / "atlas_town_doors_L1.png", sc_big)
    make_atlas(ts["edges"], pal1, "Genesis MM2 — TOWN edges (L1)", OUT / "atlas_town_edges_L1.png", sc_side)
    make_atlas(ts["all"], pal1, "Genesis MM2 — TOWN all strips (L1)", OUT / "atlas_town_all_L1.png", sc_big)

    # Cavern — L2, same art (user-confirmed)
    make_atlas(ts["sides"], pal2, "Genesis MM2 — CAVERN sides (L2 @ 0x6FC2A, same art as town)", OUT / "atlas_cavern_sides_L2.png", sc_side)
    make_atlas(ts["near"], pal2, "Genesis MM2 — CAVERN near faces (L2)", OUT / "atlas_cavern_near_L2.png", sc_big)
    make_atlas(ts["doors"], pal2, "Genesis MM2 — CAVERN doors / panels (L2)", OUT / "atlas_cavern_doors_L2.png", sc_big)
    make_atlas(ts["edges"], pal2, "Genesis MM2 — CAVERN edges (L2)", OUT / "atlas_cavern_edges_L2.png", sc_side)
    make_atlas(ts["all"], pal2, "Genesis MM2 — CAVERN all strips (L2, same art as town)", OUT / "atlas_cavern_all_L2.png", sc_big)

    # Outdoor — L3 (user-confirmed)
    make_atlas(os_["all"], pal3, "Genesis MM2 — OUTDOOR strips (L3 @ 0x6FC4A)", OUT / "atlas_outdoor_all_L3.png", sc_side)
    make_atlas(os_["big"], pal3, "Genesis MM2 — OUTDOOR large panels (L3)", OUT / "atlas_outdoor_large_L3.png", sc_big)
    make_atlas(os_["sides"], pal3, "Genesis MM2 — OUTDOOR sides (L3)", OUT / "atlas_outdoor_sides_L3.png", sc_side)

    # Master contact sheet: one door/panel per theme
    doors = []
    for name, items, pal, line in (
        ("CASTLE L0", cs["doors"], pal0, 0),
        ("TOWN L1", ts["doors"], pal1, 1),
        ("CAVERN L2", ts["doors"], pal2, 2),
        ("OUTDOOR L3", [t for t in o_items if t[1] == 80] or os_["big"][:2], pal3, 3),
    ):
        for off, chunk, blob in items[:2]:
            doors.append((name, off, render(blob, chunk, pal, 2)))
    if doors:
        gap = 8
        label_h = 28
        W = sum(im.width + gap for _, _, im in doors) + 8
        H = max(im.height for _, _, im in doors) + label_h + 16
        sheet = Image.new("RGB", (W, H), (12, 12, 16))
        d = ImageDraw.Draw(sheet)
        d.text((6, 4), "Genesis MM2 — theme doors (real palettes L0/L1/L2/L3)", fill=(230, 220, 100))
        x = 6
        for name, off, im in doors:
            d.text((x, 18), f"{name}  {off:X}", fill=(180, 180, 120))
            sheet.paste(im, (x, label_h))
            x += im.width + gap
        sheet.save(OUT / "atlas_themes_doors.png")
        print(f"-> {OUT / 'atlas_themes_doors.png'} {sheet.size}")

    # Palette swatches
    sw = Image.new("RGB", (16 * 28 + 100, 4 * 28 + 40), (20, 20, 24))
    d = ImageDraw.Draw(sw)
    d.text((2, 2), "Env CRAM 0x6FBEA — real palettes (user-confirmed)", fill=(230, 220, 100))
    for n, (pal, label) in enumerate(
        ((pal0, "L0 castle"), (pal1, "L1 town"), (pal2, "L2 cavern"), (pal3, "L3 outdoor"))
    ):
        d.text((2, 18 + n * 28), label, fill=(200, 200, 160))
        for i, c in enumerate(pal):
            d.rectangle([90 + i * 28, 16 + n * 28, 90 + i * 28 + 26, 16 + n * 28 + 26], fill=c)
    sw.save(OUT / "palettes_L0_L1_L2_L3.png")
    print(f"atlases in {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
