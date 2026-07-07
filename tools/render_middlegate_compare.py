#!/usr/bin/env python3
"""Render Middlegate 3D view for PC CGA / EGA PNG exports (same scene as Amiga)."""
from __future__ import annotations

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow")

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from attrib_codec import AttribFile  # noqa: E402
from decode_pc_gfx import parse_wall_sheet, render_wall_frame_rgba  # noqa: E402
from render_view_refs import (  # noqa: E402
    FLOOR_Y,
    ORIGIN_X,
    SKY_Y,
    VIEW_H,
    VIEW_W,
    amiga_frame_mask,
    blit,
)
from view3d_indoor import StitchedVisual, build_scene, load_map  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PC_GFX = ROOT / "EXTRACTED" / "pc_gfx"
OUT = ROOT / "EXTRACTED" / "docs" / "img"
GOG = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")

# Middlegate inn spawn — matches view3d_middlegate_check / game default.
SCREEN, X, Y, FACING = 0, 7, 3, 0

# PC wall stems → Amiga mask source (same frame indices as ``view3d_indoor``).
_AMIGA_MASK_SHEET = {
    "town": "town.32",
    "townf": "townf.32",
    "sky": "sky.32",
    "cave": "cave.32",
    "castle": "castle.32",
}

_SHEET_CACHE: dict[tuple[str, str], dict] = {}
_MASK_CACHE: dict[tuple[str, int], list[list[bool]]] = {}


def _load_sheet_info(variant: str, stem: str) -> dict:
    key = (variant, stem)
    if key in _SHEET_CACHE:
        return _SHEET_CACHE[key]
    ext = ".4" if variant == "cga" else ".16"
    path = GOG / f"{stem.upper()}{ext}"
    if not path.is_file():
        path = ROOT / f"{stem.upper()}{ext}"
    _SHEET_CACHE[key] = parse_wall_sheet(path)
    return _SHEET_CACHE[key]


def _amiga_mask(stem: str, frame: int) -> list[list[bool]] | None:
    sheet = _AMIGA_MASK_SHEET.get(stem)
    if sheet is None or not (ROOT / sheet).is_file():
        return None
    key = (sheet, frame)
    if key not in _MASK_CACHE:
        _MASK_CACHE[key] = amiga_frame_mask(sheet, frame, ROOT)
    return _MASK_CACHE[key]


def load_pc_frame(variant: str, stem: str, frame: int) -> Image.Image:
    """Decode one wall/floor/sky frame with PC driver-style masking."""
    sheet = _load_sheet_info(variant, stem)
    frames = sheet["frames"]
    if frame >= len(frames):
        raise FileNotFoundError(f"{variant}/{stem} frame {frame}")
    fr = frames[frame]
    rgba = render_wall_frame_rgba(
        fr.width,
        fr.height,
        fr.pixels,
        sheet["bpp"],
        cga_palette=1,
        amiga_mask=_amiga_mask(stem, frame),
        frame=frame,
    )
    im = Image.new("RGBA", (fr.width, fr.height))
    im.putdata(rgba)
    return im


def render_pc(variant: str, scene) -> Image.Image:
    cache: dict[tuple[str, int], Image.Image] = {}

    def sprite(stem: str, frame: int) -> Image.Image:
        key = (stem, frame)
        if key not in cache:
            cache[key] = load_pc_frame(variant, stem, frame)
        return cache[key]

    canvas = Image.new("RGBA", (VIEW_W, VIEW_H), (0, 0, 0, 255))
    blit(canvas, sprite("townf", 0), ORIGIN_X, FLOOR_Y)
    blit(canvas, sprite("sky", 0), ORIGIN_X, SKY_Y)
    for b in scene.blits:
        blit(canvas, sprite("town", b.frame), b.x, b.y)
    return canvas


def main() -> int:
    maps = load_map(ROOT / "map.dat")
    AttribFile.load(str(ROOT / "attrib.dat"))
    grid = StitchedVisual(maps, AttribFile.load(str(ROOT / "attrib.dat")), SCREEN)
    scene = build_scene(grid, X, Y, FACING)

    OUT.mkdir(parents=True, exist_ok=True)
    panels: list[tuple[str, Image.Image]] = []
    for variant, label in (("cga", "CGA (.4)"), ("ega", "EGA (.16)")):
        out = OUT / f"middlegate_{variant}.png"
        img = render_pc(variant, scene)
        img.save(out)
        print(f"wrote {out}")
        panels.append((label, img))

    # Side-by-side with Amiga if present.
    amiga_path = OUT / "middlegate_amiga.png"
    if amiga_path.is_file():
        panels.insert(0, ("Amiga (.32)", Image.open(amiga_path).convert("RGBA")))

    scale = 3
    gap = 8
    label_h = 28
    pw = VIEW_W * scale
    ph = VIEW_H * scale + label_h
    total_w = len(panels) * pw + (len(panels) - 1) * gap
    comp = Image.new("RGBA", (total_w, ph), (32, 32, 40, 255))
    from PIL import ImageDraw, ImageFont

    draw = ImageDraw.Draw(comp)
    x = 0
    for label, img in panels:
        up = img.resize((pw, VIEW_H * scale), Image.NEAREST)
        comp.paste(up, (x, label_h), up)
        draw.text((x + 8, 4), f"{label}  screen {SCREEN} ({X},{Y}) N", fill=(220, 220, 230, 255))
        x += pw + gap

    compare = OUT / "middlegate_all3.png"
    comp.save(compare)
    print(f"wrote {compare}")
    print(f"blits ({len(scene.blits)}):")
    for b in scene.blits:
        print(f"  {b.kind:5s} frame={b.frame:2d} @ ({b.x},{b.y})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
