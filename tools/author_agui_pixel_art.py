#!/usr/bin/env python3
"""Author Agui sprites ON the final pixel grid (Deluxe Paint style).

Concept sheets are used only for **colour sampling** (skin/hair/armor).
Every output pixel is placed on the target canvas — we do not scale the
concept image down into the pack.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
AGUI = ROOT / "game" / "data" / "ui" / "agui"
ASSETS = Path.home() / ".cursor" / "projects" / "c-20260421-D-REC-development-MM2" / "assets"


def load_palette() -> list[tuple[int, int, int]]:
    data = json.loads((AGUI / "palette.json").read_text(encoding="utf-8"))
    return [tuple(int(c) for c in rgb) for rgb in data["colors"]]


def nearest_pen(rgb: tuple[int, int, int], palette: list[tuple[int, int, int]]) -> int:
    best, best_d = 0, 1 << 30
    for i, p in enumerate(palette):
        d = (rgb[0] - p[0]) ** 2 + (rgb[1] - p[1]) ** 2 + (rgb[2] - p[2]) ** 2
        if d < best_d:
            best, best_d = i, d
    return best


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


def avg_color(arr: np.ndarray) -> tuple[int, int, int]:
    if arr.size == 0:
        return (128, 128, 128)
    m = arr.reshape(-1, 3).mean(axis=0)
    return (int(m[0]), int(m[1]), int(m[2]))


def sample_face_regions(sheet: Image.Image, index: int) -> dict[str, tuple[int, int, int]]:
    """Average colours from concept face #index — reference only, not scaled."""
    arr = np.asarray(sheet.convert("RGB"))
    gray = arr.mean(axis=2)
    mask = gray > 15
    rows = np.where(mask.any(axis=1))[0]
    cols = np.where(mask.any(axis=0))[0]
    strip = arr[rows[0] : rows[-1] + 1, cols[0] : cols[-1] + 1]
    fw = strip.shape[1] // 8
    cell = strip[:, index * fw : (index + 1) * fw]
    h, w, _ = cell.shape
    # Relative regions inside a bust
    hair = cell[int(h * 0.05) : int(h * 0.28), int(w * 0.2) : int(w * 0.8)]
    skin = cell[int(h * 0.30) : int(h * 0.62), int(w * 0.28) : int(w * 0.72)]
    armor = cell[int(h * 0.72) : int(h * 0.95), int(w * 0.15) : int(w * 0.85)]
    return {
        "hair": avg_color(hair),
        "skin": avg_color(skin),
        "armor": avg_color(armor),
    }


def draw_face(palette: list[tuple[int, int, int]], colors: dict[str, tuple[int, int, int]],
              variant: int) -> Image.Image:
    """28×28 intentional portrait pixels."""
    img = new_p(28, 28, palette)
    bg = nearest_pen((24, 20, 16), palette)
    hair = nearest_pen(colors["hair"], palette)
    skin = nearest_pen(colors["skin"], palette)
    armor = nearest_pen(colors["armor"], palette)
    ink = nearest_pen((20, 16, 16), palette)
    white = nearest_pen((224, 224, 192), palette)
    lip = nearest_pen((176, 96, 64), palette)
    hi = nearest_pen((232, 208, 154), palette)

    fill(img, 0, 0, 28, 28, bg)
    # Hair mass
    fill(img, 5, 2, 18, 9, hair)
    if variant in (1, 3, 5):  # longer / side hair
        fill(img, 3, 8, 3, 10, hair)
        fill(img, 22, 8, 3, 10, hair)
    if variant == 2:  # beard
        fill(img, 8, 18, 12, 6, hair)
    if variant == 6:  # hood
        fill(img, 4, 1, 20, 12, nearest_pen((40, 32, 28), palette))
        fill(img, 8, 6, 12, 8, skin)
    else:
        # Face oval-ish
        fill(img, 7, 8, 14, 12, skin)
        fill(img, 8, 7, 12, 2, skin)
        fill(img, 8, 19, 12, 2, skin)

    # Eyes
    put(img, 10, 12, white)
    put(img, 11, 12, ink)
    put(img, 16, 12, white)
    put(img, 17, 12, ink)
    put(img, 10, 13, ink)
    put(img, 16, 13, ink)
    # Brows
    fill(img, 9, 10, 3, 1, hair if variant != 6 else ink)
    fill(img, 15, 10, 3, 1, hair if variant != 6 else ink)
    # Nose hint
    put(img, 13, 14, nearest_pen((min(255, colors["skin"][0] - 30), max(0, colors["skin"][1] - 30),
                                   max(0, colors["skin"][2] - 30)), palette))
    put(img, 14, 15, ink)
    # Mouth
    fill(img, 11, 17, 6, 1, lip)
    put(img, 12, 18, lip)
    put(img, 15, 18, lip)
    # Collar / armor
    fill(img, 5, 21, 18, 5, armor)
    fill(img, 6, 21, 16, 1, hi)
    # Specular on hair
    put(img, 9, 4, hi)
    put(img, 10, 3, hi)
    return img


def draw_icon(palette: list[tuple[int, int, int]], kind: str) -> Image.Image:
    """12×12 intentional icons — gold on dark stone."""
    img = new_p(12, 12, palette)
    lo, mid, hi = 7, 5, 4
    gold, gold_hi = 2, 3
    cream = 1
    teal = 9
    red = 12
    fill(img, 0, 0, 12, 12, lo)
    fill(img, 1, 1, 10, 10, mid)
    # top-left bevel
    for i in range(12):
        put(img, i, 0, hi)
        put(img, 0, i, hi)

    def line(x0, y0, x1, y1, pen):
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(steps + 1):
            t = i / steps
            put(img, int(round(x0 + (x1 - x0) * t)), int(round(y0 + (y1 - y0) * t)), pen)

    if kind == "cast":
        line(3, 9, 8, 3, gold)
        put(img, 8, 2, gold_hi)
        put(img, 9, 3, cream)
        put(img, 7, 3, cream)
        put(img, 8, 4, cream)
    elif kind == "shoot":
        line(2, 6, 9, 6, gold)
        line(8, 4, 10, 6, gold_hi)
        line(8, 8, 10, 6, gold_hi)
        put(img, 2, 5, cream)
    elif kind == "unlock":
        fill(img, 4, 3, 4, 3, gold)
        fill(img, 5, 4, 2, 1, mid)
        fill(img, 5, 6, 2, 4, gold)
        put(img, 6, 8, mid)
    elif kind == "bash":
        fill(img, 3, 2, 3, 7, gold)
        fill(img, 6, 4, 4, 3, cream)
        put(img, 5, 8, gold_hi)
    elif kind == "rest":
        put(img, 3, 3, cream)
        put(img, 4, 4, cream)
        put(img, 7, 5, cream)
        put(img, 8, 6, cream)
        fill(img, 4, 8, 4, 2, teal)
    elif kind == "search":
        fill(img, 3, 3, 5, 5, gold)
        fill(img, 4, 4, 3, 3, mid)
        line(7, 7, 10, 10, gold_hi)
    elif kind == "map":
        fill(img, 2, 2, 8, 8, teal)
        fill(img, 3, 3, 6, 6, mid)
        put(img, 5, 5, cream)
        put(img, 4, 6, gold)
        put(img, 6, 4, gold)
    elif kind == "quick":
        fill(img, 3, 2, 2, 8, gold)
        fill(img, 5, 2, 4, 2, gold)
        fill(img, 5, 5, 3, 2, gold)
    elif kind == "attack":
        line(5, 1, 5, 10, cream)
        line(3, 3, 8, 3, gold)
        put(img, 5, 10, gold)
    elif kind == "fight":
        line(3, 9, 8, 2, gold)
        line(8, 9, 3, 2, gold_hi)
    elif kind == "run":
        line(2, 6, 8, 6, cream)
        line(7, 4, 10, 6, gold_hi)
        line(7, 8, 10, 6, gold_hi)
    elif kind == "block":
        fill(img, 3, 3, 6, 6, gold)
        fill(img, 4, 4, 4, 4, mid)
        put(img, 5, 5, cream)
        put(img, 6, 6, cream)
    elif kind == "use":
        fill(img, 4, 2, 4, 7, red)
        fill(img, 5, 3, 2, 2, cream)
        fill(img, 5, 8, 2, 2, gold)
    elif kind.startswith("dpad_"):
        fill(img, 1, 1, 10, 10, mid)
        if kind == "dpad_n":
            fill(img, 5, 2, 2, 6, gold_hi)
            fill(img, 3, 4, 6, 2, gold_hi)
        elif kind == "dpad_s":
            fill(img, 5, 4, 2, 6, gold_hi)
            fill(img, 3, 6, 6, 2, gold_hi)
        elif kind == "dpad_w":
            fill(img, 2, 5, 6, 2, gold_hi)
            fill(img, 4, 3, 2, 6, gold_hi)
        elif kind == "dpad_e":
            fill(img, 4, 5, 6, 2, gold_hi)
            fill(img, 6, 3, 2, 6, gold_hi)
        else:
            fill(img, 4, 4, 4, 4, gold)
    return img


def draw_frame(palette: list[tuple[int, int, int]]) -> Image.Image:
    """216×128 stone frame with transparent 208×120 hood."""
    img = new_p(216, 128, palette)
    stone, stone_hi, stone_lo = 16, 15, 19
    fill(img, 0, 0, 216, 128, stone)
    # outer bevel
    for i in range(216):
        put(img, i, 0, stone_hi)
        put(img, i, 127, stone_lo)
    for i in range(128):
        put(img, 0, i, stone_hi)
        put(img, 215, i, stone_lo)
    fill(img, 2, 2, 212, 124, nearest_pen((96, 72, 48), palette))
    # transparent hood
    fill(img, 4, 4, 208, 120, 0)
    # inner rim
    for i in range(208):
        put(img, 4 + i, 4, stone_lo)
        put(img, 4 + i, 123, stone_hi)
    for i in range(120):
        put(img, 4, 4 + i, stone_lo)
        put(img, 211, 4 + i, stone_hi)
    return img


def draw_gem(palette: list[tuple[int, int, int]], on: bool) -> Image.Image:
    img = new_p(6, 6, palette)
    pen = 9 if on else 7
    for y in range(5):
        half = 2 - abs(y - 2)
        for x in range(3 - half, 3 + half + 1):
            put(img, x, y, pen)
    if on:
        put(img, 3, 2, 3)
    return img


def draw_face_gem(palette: list[tuple[int, int, int]]) -> Image.Image:
    img = new_p(18, 10, palette)
    fill(img, 0, 0, 18, 10, 27)
    for i in range(18):
        put(img, i, 0, 4)
        put(img, i, 9, 7)
    for i in range(10):
        put(img, 0, i, 4)
        put(img, 17, i, 7)
    return img


def main() -> int:
    palette = load_palette()
    sheet_path = ASSETS / "agui-faces-native-sheet.png"
    if not sheet_path.is_file():
        print(f"WARN: missing {sheet_path} — using palette defaults for face colours", file=sys.stderr)
        sheet = None
    else:
        sheet = Image.open(sheet_path)

    print("Authoring native-grid Agui art (no image scale into pack)")
    faces = AGUI / "faces"
    faces.mkdir(parents=True, exist_ok=True)
    defaults = [
        {"hair": (60, 40, 28), "skin": (200, 170, 130), "armor": (140, 120, 70)},
        {"hair": (30, 20, 18), "skin": (210, 180, 150), "armor": (80, 60, 100)},
        {"hair": (90, 70, 40), "skin": (180, 140, 100), "armor": (100, 80, 50)},
        {"hair": (200, 180, 100), "skin": (220, 190, 160), "armor": (70, 110, 90)},
        {"hair": (50, 35, 25), "skin": (190, 150, 110), "armor": (120, 70, 40)},
        {"hair": (220, 200, 140), "skin": (230, 210, 190), "armor": (90, 70, 120)},
        {"hair": (25, 20, 18), "skin": (170, 130, 100), "armor": (40, 35, 30)},
        {"hair": (180, 160, 90), "skin": (210, 175, 145), "armor": (150, 130, 80)},
    ]
    for i in range(8):
        colors = sample_face_regions(sheet, i) if sheet is not None else defaults[i]
        img = draw_face(palette, colors, i)
        img.save(faces / f"face_{i:02d}.png")
        print(f"  faces/face_{i:02d}.png 28x28")

    icons = AGUI / "icons"
    icons.mkdir(parents=True, exist_ok=True)
    for name in (
        "cast", "shoot", "unlock", "bash", "rest", "search", "map", "quick",
        "attack", "fight", "run", "block", "use",
        "dpad_n", "dpad_s", "dpad_w", "dpad_e", "dpad_wait",
    ):
        draw_icon(palette, name).save(icons / f"{name}.png")
        print(f"  icons/{name}.png 12x12")

    chrome = AGUI / "chrome"
    chrome.mkdir(parents=True, exist_ok=True)
    draw_frame(palette).save(chrome / "frame.png")
    draw_gem(palette, True).save(chrome / "gem_on.png")
    draw_gem(palette, False).save(chrome / "gem_off.png")
    draw_face_gem(palette).save(chrome / "face_gem.png")
    print("  chrome/* native")
    print("Done. Pack with: python tools/pack_agui_ui.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
