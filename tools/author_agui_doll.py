#!/usr/bin/env python3
"""Author Agui paper-doll chrome ON the final pixel grid (not item icons).

Item sprites are user-supplied (see game/data/ui/agui/ITEM_ICON_PROMPT.md).
This writes only doll/body.png and doll/slot.png.
"""
from __future__ import annotations

import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
AGUI = ROOT / "game" / "data" / "ui" / "agui"


def load_palette() -> list[tuple[int, int, int]]:
    data = json.loads((AGUI / "palette.json").read_text(encoding="utf-8"))
    return [tuple(int(c) for c in rgb) for rgb in data["colors"]]


def new_p(w: int, h: int, palette: list[tuple[int, int, int]]) -> Image.Image:
    img = Image.new("P", (w, h), 0)
    flat: list[int] = []
    for rgb in palette:
        flat.extend(rgb)
    while len(flat) < 256 * 3:
        flat.extend([0, 0, 0])
    img.putpalette(flat)
    return img


def put(img: Image.Image, x: int, y: int, pen: int) -> None:
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), pen)


def fill(img: Image.Image, x: int, y: int, w: int, h: int, pen: int) -> None:
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            put(img, xx, yy, pen)


def draw_body() -> Image.Image:
    """32×48 standing silhouette — slot frames are blitted on top at runtime."""
    palette = load_palette()
    img = new_p(32, 48, palette)
    ink, leather, tan, hi, lo = 7, 22, 20, 24, 19
    # Head
    fill(img, 11, 1, 10, 9, tan)
    fill(img, 12, 0, 8, 1, tan)
    fill(img, 12, 10, 8, 1, tan)
    put(img, 13, 2, hi)
    put(img, 14, 3, ink)
    put(img, 17, 3, ink)
    fill(img, 14, 6, 4, 1, leather)
    # Neck + torso
    fill(img, 13, 11, 6, 3, tan)
    fill(img, 9, 14, 14, 16, leather)
    fill(img, 10, 15, 12, 1, hi)
    fill(img, 10, 28, 12, 1, lo)
    # Arms
    fill(img, 4, 15, 5, 3, tan)
    fill(img, 23, 15, 5, 3, tan)
    fill(img, 3, 18, 4, 10, tan)
    fill(img, 25, 18, 4, 10, tan)
    fill(img, 3, 27, 4, 2, leather)
    fill(img, 25, 27, 4, 2, leather)
    # Legs
    fill(img, 11, 30, 4, 16, leather)
    fill(img, 17, 30, 4, 16, leather)
    fill(img, 10, 44, 6, 3, lo)
    fill(img, 16, 44, 6, 3, lo)
    # Outline ticks
    for x, y in ((10, 1), (21, 1), (8, 14), (23, 14), (10, 30), (21, 30)):
        put(img, x, y, ink)
    return img


def draw_slot() -> Image.Image:
    """12×12 empty socket — transparent interior so body shows through."""
    palette = load_palette()
    img = new_p(12, 12, palette)
    hi, lo, mid = 4, 7, 5
    for i in range(12):
        put(img, i, 0, hi)
        put(img, 0, i, hi)
        put(img, i, 11, lo)
        put(img, 11, i, lo)
    put(img, 0, 0, hi)
    put(img, 11, 11, lo)
    fill(img, 1, 1, 10, 10, 0)
    put(img, 1, 1, mid)
    put(img, 10, 10, mid)
    return img


def main() -> int:
    doll = AGUI / "doll"
    doll.mkdir(parents=True, exist_ok=True)
    draw_body().save(doll / "body.png")
    draw_slot().save(doll / "slot.png")
    print("  doll/body.png 32x48")
    print("  doll/slot.png 12x12")
    print("Done. Pack with: python tools/pack_agui_ui.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
