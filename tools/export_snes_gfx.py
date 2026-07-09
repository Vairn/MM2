#!/usr/bin/env python3
"""Export SNES MM2 graphics for PC/Amiga/JS rewrites.

Produces EXTRACTED/snes/gfx/export/ with:

  README.txt
  manifest.json          panel rects + Amiga/PC frame hints
  atlases/               labeled contact sheets (preview scale)
  titles/ explore/ monsters/   (unchanged preview assets)
  walls/
    sheets_native/       8 px/tile sheets (160×128) — match Amiga front width
    sheets_x2/           16 px/tile sheets (320×256) — hi-DPI preview
    panels_native/       cropped panels, transparent bg, 8 px/tile
    panels_x2/           same crops at 2×
    floors_native/       indoor + outdoor floors at native scale
    floors_x2/
    sky_native/ sky_x2/

Native scale (1) matches Amiga/PC pixel grid: front wall width 160 px,
viewport 208×120. SNES near-front panel is 128×88 (Amiga frame 0 is 160×92);
side near strips are 24×120 (Amiga 24×118). Use panels_* for per-depth blits;
use sheets_* when you want the full composed face sheet.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from compose_snes_scenes import (  # noqa: E402
    _face_row_tables,
    _floor_pal_no_torch,
    blit_face_table,
    load_chr,
    read_pal,
    render_chr_grid,
    sheet_to_png,
)
from snes_decompress import decode_4bpp_tile, lorom_to_file  # noqa: E402

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow")

SRC = ROOT / "EXTRACTED" / "snes" / "gfx"
DEFAULT_OUT = SRC / "export"
ROM_PATH = ROOT / "EXTRACTED" / "SNES" / "Might and Magic II (Europe).sfc"

# Preview allow-lists (copied from existing compose/atlas outputs)
ATLASES = [
    "atlas_master.png",
    "atlas_titles.png",
    "atlas_wall_faces.png",
    "atlas_outdoor_faces.png",
    "atlas_env_faces.png",
    "atlas_doors_floors.png",
    "atlas_env_sheets.png",
    "atlas_explore.png",
    "atlas_monsters.png",
    "atlas_monster_anims.png",
]
TITLES = ["title_screen.png", "boot_title_screen.png"]
EXPLORE = ["explore_A2_0.png", "explore_A2_2.png"]

# Face tables → rewrite-friendly names
FACE_SETS = [
    # name, bank, addr, n_panels, pal_bank, pal_addr, notes
    ("town_cavern_wall", 0x11, 0xFD6C, 20, 0x13, 0x8034, "A=0/1 wall"),
    ("town_cavern_wall_night", 0x11, 0xFD6C, 20, 0x1D, 0x808A, "A=0/1 night"),
    ("town_cavern_door", 0x11, 0xFDA8, 20, 0x13, 0x8034, "A=0/1 door"),
    ("town_cavern_door_night", 0x11, 0xFDA8, 20, 0x1D, 0x808A, "A=0/1 door night"),
    ("castle_wall", 0x11, 0xFCB8, 20, 0x13, 0x8014, "A=2/5 wall"),
    ("castle_wall_night", 0x11, 0xFCB8, 20, 0x15, 0x8000, "A=2/5 night"),
    ("castle_door", 0x11, 0xFCF4, 20, 0x13, 0x8014, "A=2/5 door"),
    ("outdoor_mountains", 0x11, 0xFE20, 15, 0x13, 0x8034, "terrain 1/2"),
    ("outdoor_trees_A", 0x11, 0xFE7A, 15, 0x13, 0x8054, "terrain 3"),
    ("outdoor_trees_B", 0x11, 0xFEA7, 15, 0x13, 0x8054, "terrain 4"),
]
for _t in range(5, 13):
    FACE_SETS.append(
        (
            f"outdoor_floor_terr{_t}",
            0x11,
            0xFED4 + (_t - 5) * 0x24,
            12,
            0x13,
            0x8054,
            f"terrain {_t}",
        )
    )

# Best-effort SNES panel index → Amiga/PC town.32 frame index.
# SNES paint order ≠ Amiga 0..15 depth ladder; these are size/role hints only.
AMIGA_FRAME_HINT = {
    # near front (128×88) ≈ Amiga frame 0 (160×92)
    16: 0,
    # mid front (80×64) ≈ frame 1 (96×55)
    11: 1,
    # mid (48×32) ≈ frame 2 (48×27)
    6: 2,
    # far (16×16) ≈ frame 3 (16×10)
    0: 3,
    1: 3,
    2: 3,
    # left near tall (24×120) ≈ frame 4 (24×118)
    18: 4,
    # right near tall
    19: 8,
    # left mid (24×88)
    15: 5,
    13: 5,
    # right mid
    17: 9,
    14: 9,
}

SHEET_W, SHEET_H = 20, 16  # tiles
BG_OLIVE = (0x58, 0x70, 0x58)


def copy_named(src_dir: Path, names: list[str], dest: Path) -> int:
    dest.mkdir(parents=True, exist_ok=True)
    n = 0
    for name in names:
        src = src_dir / name
        if src.is_file():
            shutil.copy2(src, dest / name)
            n += 1
        else:
            print(f"  missing: {src}")
    return n


def copy_glob(src_dir: Path, pattern: str, dest: Path) -> int:
    dest.mkdir(parents=True, exist_ok=True)
    n = 0
    for src in sorted(src_dir.glob(pattern)):
        if src.is_file():
            shutil.copy2(src, dest / src.name)
            n += 1
    return n


def panel_rects(rom: bytes, table_bank: int, table_addr: int, n: int) -> list[dict]:
    fo = lorom_to_file(table_bank, table_addr)
    out: list[dict] = []
    for i in range(n):
        b0, b1, b2 = rom[fo + i * 3], rom[fo + i * 3 + 1], rom[fo + i * 3 + 2]
        addr, bank = b0 | (b1 << 8), b2
        if bank < 0x0E or bank > 0x1F:
            out.append({"index": i, "valid": False})
            continue
        pfo = lorom_to_file(bank, addr)
        x, y, w, h = rom[pfo], rom[pfo + 1], rom[pfo + 2], rom[pfo + 3]
        out.append(
            {
                "index": i,
                "valid": True,
                "rom": f"{bank:02X}:{addr:04X}",
                "tile": {"x": x, "y": y, "w": w, "h": h},
                "px": {"x": x * 8, "y": y * 8, "w": w * 8, "h": h * 8},
                "amiga_frame_hint": AMIGA_FRAME_HINT.get(i),
            }
        )
    return out


def sheet_rgba(
    sheet: bytearray,
    pal: list[tuple[int, int, int]],
    *,
    scale: int,
    transparent0: bool,
) -> Image.Image:
    """Render WRAM sheet to RGBA; color 0 → transparent when transparent0."""
    img = Image.new("RGBA", (SHEET_W * 8, SHEET_H * 8), (0, 0, 0, 0))
    px = img.load()
    for ty in range(SHEET_H):
        for tx in range(SHEET_W):
            off = ty * 640 + tx * 32
            tile = decode_4bpp_tile(bytes(sheet[off : off + 32]))
            for y in range(8):
                for x in range(8):
                    i = tile[y][x]
                    if transparent0 and i == 0:
                        continue
                    r, g, b = pal[i]
                    px[tx * 8 + x, ty * 8 + y] = (r, g, b, 255)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def crop_panels(
    sheet_img: Image.Image,
    rects: list[dict],
    dest: Path,
    stem: str,
    scale: int,
) -> int:
    dest.mkdir(parents=True, exist_ok=True)
    n = 0
    for r in rects:
        if not r.get("valid"):
            continue
        px = r["px"]
        box = (
            px["x"] * scale,
            px["y"] * scale,
            (px["x"] + px["w"]) * scale,
            (px["y"] + px["h"]) * scale,
        )
        crop = sheet_img.crop(box)
        # Drop fully-empty crops
        if crop.getbbox() is None:
            continue
        hint = r.get("amiga_frame_hint")
        suffix = f"_amiga{hint}" if hint is not None else ""
        name = f"{stem}_p{r['index']:02d}{suffix}_{px['w']*scale}x{px['h']*scale}.png"
        crop.save(dest / name)
        n += 1
    return n


def export_rewrite_walls(rom: bytes, out: Path) -> dict[str, int]:
    counts: dict[str, int] = {}
    d403, d439 = _face_row_tables(rom)
    manifest_faces: list[dict] = []

    for name, tbank, taddr, n, pb, pa, notes in FACE_SETS:
        pal = read_pal(rom, lorom_to_file(pb, pa), 16)
        sheet = blit_face_table(rom, tbank, taddr, n, d403, d439)
        rects = panel_rects(rom, tbank, taddr, n)

        for scale, sheet_dir, panel_dir in (
            (1, "sheets_native", "panels_native"),
            (2, "sheets_x2", "panels_x2"),
        ):
            # Opaque olive sheet (preview / atlas-compatible)
            opaque = out / "walls" / sheet_dir
            opaque.mkdir(parents=True, exist_ok=True)
            sheet_to_png(
                sheet,
                pal,
                opaque / f"{name}.png",
                scale=scale,
                bg=BG_OLIVE,
            )
            # Transparent sheet + panel crops for rewrite blits
            rgba = sheet_rgba(sheet, pal, scale=scale, transparent0=True)
            rgba.save(opaque / f"{name}_alpha.png")
            n_panels = crop_panels(
                rgba, rects, out / "walls" / panel_dir / name, name, scale
            )
            key = f"walls/{panel_dir}/{name}"
            counts[key] = n_panels

        counts.setdefault("walls/sheets_native", 0)
        counts["walls/sheets_native"] += 2  # .png + _alpha.png
        counts.setdefault("walls/sheets_x2", 0)
        counts["walls/sheets_x2"] += 2

        manifest_faces.append(
            {
                "name": name,
                "table": f"{tbank:02X}:{taddr:04X}",
                "n_panels": n,
                "palette": f"{pb:02X}:{pa:04X}",
                "notes": notes,
                "sheet_native_px": [SHEET_W * 8, SHEET_H * 8],
                "sheet_x2_px": [SHEET_W * 16, SHEET_H * 16],
                "panels": rects,
            }
        )

    # Floors + sky at both scales
    wall_a = load_chr(rom, lorom_to_file(0x0D, 0xBFE0), 140 * 32)
    wall_b = load_chr(rom, lorom_to_file(0x0D, 0xD160), 140 * 32)
    sky = load_chr(rom, lorom_to_file(0x0D, 0x8000), 4288)
    floor_specs = [
        ("floor_town_cavern_day", wall_a, read_pal(rom, lorom_to_file(0x13, 0x8034), 16), 20, 7),
        ("floor_town_cavern_night", wall_a, read_pal(rom, lorom_to_file(0x1D, 0x808A), 16), 20, 7),
        (
            "floor_castle_day",
            wall_b,
            _floor_pal_no_torch(rom, 0x13, 0x8014, {13, 14, 15}),
            20,
            7,
        ),
        ("floor_castle_night", wall_b, read_pal(rom, lorom_to_file(0x15, 0x8000), 16), 20, 7),
        ("sky_day", sky, read_pal(rom, lorom_to_file(0x1B, 0x8000), 16), 11, 11),
        ("sky_dusk", sky, read_pal(rom, lorom_to_file(0x1D, 0x806A), 16), 11, 11),
    ]
    for scale, sub in ((1, "native"), (2, "x2")):
        dest = out / "walls" / f"floors_{sub}"
        sky_dest = out / "walls" / f"sky_{sub}"
        dest.mkdir(parents=True, exist_ok=True)
        sky_dest.mkdir(parents=True, exist_ok=True)
        n_f = n_s = 0
        for name, tiles, pal, cols, rows in floor_specs:
            img = render_chr_grid(tiles, pal, cols, rows, scale)
            if name.startswith("sky"):
                img.save(sky_dest / f"{name}.png")
                n_s += 1
            else:
                img.save(dest / f"{name}.png")
                n_f += 1
        counts[f"walls/floors_{sub}"] = n_f
        counts[f"walls/sky_{sub}"] = n_s

    # Viewport reference (208×120) — empty canvas for rewrite compositors
    for scale, sub in ((1, "native"), (2, "x2")):
        vp = Image.new("RGBA", (208 * scale, 120 * scale), (0, 0, 0, 0))
        # Mark sky/floor bands
        draw_bg = Image.new("RGBA", vp.size, (40, 40, 50, 255))
        sky_band = Image.new("RGBA", (208 * scale, 60 * scale), (60, 90, 140, 80))
        floor_band = Image.new("RGBA", (208 * scale, 60 * scale), (50, 50, 40, 80))
        draw_bg.paste(sky_band, (0, 0), sky_band)
        draw_bg.paste(floor_band, (0, 60 * scale), floor_band)
        ref_dir = out / "walls" / f"viewport_{sub}"
        ref_dir.mkdir(parents=True, exist_ok=True)
        draw_bg.save(ref_dir / "viewport_208x120.png")
        counts[f"walls/viewport_{sub}"] = 1

    manifest = {
        "source_rom": "EXTRACTED/SNES/Might and Magic II (Europe).sfc",
        "exported": date.today().isoformat(),
        "amiga_pc_viewport": {"w": 208, "h": 120, "origin": [8, 8]},
        "amiga_front_ladder": [
            {"frame": 0, "w": 160, "h": 92, "xy": [32, 23]},
            {"frame": 1, "w": 96, "h": 55, "xy": [64, 40]},
            {"frame": 2, "w": 48, "h": 27, "xy": [88, 54]},
            {"frame": 3, "w": 16, "h": 10, "xy": [104, 62]},
        ],
        "snes_sheet_tiles": [SHEET_W, SHEET_H],
        "native_px_per_tile": 8,
        "note": (
            "SNES panels are not 1:1 with Amiga town.32 frames. "
            "amiga_frame_hint is a size/role guess for rewrite mapping. "
            "Prefer panels_* crops; sheets are full 20×16 WRAM face sheets."
        ),
        "faces": manifest_faces,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return counts


README = """SNES MM2 graphics export (rewrite-ready)
========================================
Generated by tools/export_snes_gfx.py

For PC / Amiga / JS maze-walker rewrites, start here:
  walls/panels_native/   cropped face panels @ 8 px/tile (Amiga/PC pixel grid)
  walls/sheets_native/   full 160×128 face sheets (+ _alpha.png)
  walls/floors_native/   indoor floors 160×56
  walls/sky_native/      sky 88×88
  walls/viewport_native/ empty 208×120 viewport reference
  manifest.json          panel rects + Amiga frame hints

Scale guide
-----------
  native (×1)  8 px/tile   sheet 160×128   matches Amiga front width 160
  x2     (×2) 16 px/tile   sheet 320×256   hi-DPI preview only

Amiga/PC indoor front ladder (for comparison):
  frame 0  160×92 @ (32,23)
  frame 1   96×55 @ (64,40)
  frame 2   48×27 @ (88,54)
  frame 3   16×10 @ (104,62)
SNES near-front panel is typically 128×88; side near strips 24×120.

Palette lock (indoor walls = CGRAM $40):
  town/cavern day   $13:8034
  town/cavern night $1D:808A
  castle day        $13:8014  (floor remaps torch idxs 13-15)
  castle night      $15:8000
  outdoor mountains $13:8034
  outdoor trees/floors $13:8054

Also included
-------------
  atlases/ titles/ explore/ monsters/   preview contact sheets

Regenerate
----------
  python tools/compose_snes_scenes.py "EXTRACTED/SNES/Might and Magic II (Europe).sfc" ^
      --outdir EXTRACTED/snes/gfx/scenes --scale 2
  python tools/make_snes_atlases.py
  python tools/export_snes_gfx.py
"""


def export(out: Path) -> None:
    if not ROM_PATH.is_file():
        raise SystemExit(f"ROM not found: {ROM_PATH}")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    rom = ROM_PATH.read_bytes()
    counts: dict[str, int] = {}

    # Preview copies from prior compose/atlas run
    counts["atlases"] = copy_named(SRC / "atlases", ATLASES, out / "atlases")
    counts["titles"] = copy_named(SRC / "scenes", TITLES, out / "titles")
    counts["explore"] = copy_named(SRC / "scenes", EXPLORE, out / "explore")
    mon = SRC / "monsters_assembled"
    counts["monsters/sprites"] = copy_glob(mon, "monster_[0-9][0-9].png", out / "monsters" / "sprites")
    counts["monsters/frames"] = copy_glob(
        mon, "monster_[0-9][0-9]_frames.png", out / "monsters" / "frames"
    )

    # Rewrite-scaled walls (recompose from ROM)
    counts.update(export_rewrite_walls(rom, out))

    readme = README + f"\nExported: {date.today().isoformat()}\n"
    (out / "README.txt").write_text(readme, encoding="utf-8")

    total = sum(counts.values())
    print(f"Exported {total} files -> {out}")
    for k in sorted(counts):
        print(f"  {k:40s} {counts[k]:4d}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--outdir", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()
    dest = args.outdir.resolve() if args.outdir.is_absolute() else (ROOT / args.outdir).resolve()
    export(dest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
