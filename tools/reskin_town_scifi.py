#!/usr/bin/env python3
"""Phase 3: render the sci-fi palette-swap reskin for review.

Applies the index->material palette swap (tools/scifi_town_common.py) to the
decoded town sheets and writes preview PNGs (per-frame, montage, composited 3D
scenes) plus the new palette swatch into EXTRACTED/gfx_scifi/reskin/.

No pixel indices change; only the 16 used palette slots are recoloured. The 16
free slots stay reserved for .anm overlays.
"""
from __future__ import annotations

import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

from scifi_town_common import (
    ROOT,
    USED_INDICES,
    build_scifi_palette,
    decode_sheet_indexed,
)

OUT = ROOT / "EXTRACTED" / "gfx_scifi" / "reskin"

# 3D viewport blit tables (mirror of dump_town_tiles.py).
FRONT_Y_TABLE = [22, 40, 54, 62]
FRONT_X_TABLE = [32, 64, 88, 104]
FRONT_SCREEN_Y = [y + (1 if i == 0 else 0) for i, y in enumerate(FRONT_Y_TABLE)]
LEFT_NEAR_X = [8, 32, 64, 88]
LEFT_NEAR_Y = [8, 22, 40, 54]
RIGHT_NEAR_X = [192, 160, 136, 120]
RIGHT_NEAR_Y = [8, 22, 40, 54]
VIEW_W, VIEW_H, ORIGIN_X, SKY_Y, FLOOR_Y = 208, 120, 8, 8, 68

SCENES = [
    ("alcove-d0-torch", [
        (4 + 16, LEFT_NEAR_X[0], LEFT_NEAR_Y[0]),
        (8 + 16, RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0]),
        (0 + 16, FRONT_X_TABLE[0], FRONT_SCREEN_Y[0]),
    ]),
    ("corridor-d1-torch", [
        (5 + 16, LEFT_NEAR_X[1], LEFT_NEAR_Y[1]),
        (9 + 16, RIGHT_NEAR_X[1], RIGHT_NEAR_Y[1]),
        (1 + 16, FRONT_X_TABLE[1], FRONT_SCREEN_Y[1]),
    ]),
    ("alcove-d0", [
        (4, LEFT_NEAR_X[0], LEFT_NEAR_Y[0]),
        (8, RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0]),
        (0, FRONT_X_TABLE[0], FRONT_SCREEN_Y[0]),
    ]),
]


def frame_to_rgba(fr, pal) -> Image.Image:
    im = Image.new("RGBA", (fr.w, fr.h))
    px = im.load()
    for y in range(fr.h):
        for x in range(fr.w):
            v = fr.idx[y * fr.w + x]
            r, g, b = pal[v]
            px[x, y] = (r, g, b, 0 if v == 0 else 255)
    return im


def scale(im, s):
    return im.resize((im.width * s, im.height * s), Image.NEAREST)


def checker(w, h, cell=8):
    bg = Image.new("RGBA", (w, h), (40, 40, 48, 255))
    d = ImageDraw.Draw(bg)
    for y in range(0, h, cell):
        for x in range(0, w, cell):
            if ((x // cell) + (y // cell)) & 1:
                d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(56, 56, 66, 255))
    return bg


def on_checker(im):
    bg = checker(im.width, im.height)
    bg.alpha_composite(im)
    return bg


def montage(frames, pal, cols, s, title):
    pad, label_h = 6, 12
    imgs = [frame_to_rgba(f, pal) for f in frames]
    cw = max(i.width for i in imgs) * s
    ch = max(i.height for i in imgs) * s
    cell_w, cell_h = cw + pad * 2, ch + pad * 2 + label_h
    rows = (len(imgs) + cols - 1) // cols
    canvas = Image.new("RGBA", (cols * cell_w, rows * cell_h + 18), (24, 24, 30, 255))
    d = ImageDraw.Draw(canvas)
    d.text((6, 4), title, fill=(220, 220, 230, 255))
    for i, im in enumerate(imgs):
        cx = (i % cols) * cell_w
        cy = (i // cols) * cell_h + 18
        sf = scale(im, s)
        tile = checker(cw, ch)
        tile.alpha_composite(sf, ((cw - sf.width) // 2, (ch - sf.height) // 2))
        canvas.alpha_composite(tile, (cx + pad, cy + pad + label_h))
        d.text((cx + pad, cy + 1), f"{i}", fill=(180, 200, 255, 255))
    return canvas


def palette_swatch(pal, used, title):
    cell, cols = 28, 8
    rows = 4
    im = Image.new("RGBA", (cols * cell, rows * cell + 16), (24, 24, 30, 255))
    d = ImageDraw.Draw(im)
    d.text((4, 3), title, fill=(220, 220, 230, 255))
    for i, (r, g, b) in enumerate(pal):
        cx, cy = (i % cols) * cell, (i // cols) * cell + 16
        d.rectangle([cx, cy, cx + cell - 1, cy + cell - 1], fill=(r, g, b, 255))
        outline = (0, 200, 90, 255) if i not in used else (90, 90, 110, 255)
        d.rectangle([cx, cy, cx + cell - 1, cy + cell - 1], outline=outline)
        if i == 0:
            d.line([cx, cy, cx + cell - 1, cy + cell - 1], fill=(255, 0, 0, 255))
    return im


def build_scene(walls, town_imgs, floor_img, sky_img):
    canvas = Image.new("RGBA", (VIEW_W + ORIGIN_X * 2, VIEW_H + 16), (8, 8, 12, 255))
    if sky_img is not None:
        canvas.alpha_composite(sky_img, (ORIGIN_X, SKY_Y))
    if floor_img is not None:
        canvas.alpha_composite(floor_img, (ORIGIN_X, FLOOR_Y))
    for frame, x, y in walls:
        if 0 <= frame < len(town_imgs):
            canvas.alpha_composite(town_imgs[frame], (x, y))
    return canvas


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    sheets = {"town.32": 3, "townf.32": 2, "townt.32": 4}
    town_imgs = []
    floor_img = None
    scifi_pal = None
    for name, s in sheets.items():
        frames, orig_pal, _ = decode_sheet_indexed(ROOT / name)
        pal = build_scifi_palette(orig_pal)
        scifi_pal = pal
        sub = OUT / name.replace(".32", "")
        sub.mkdir(parents=True, exist_ok=True)
        for i, fr in enumerate(frames):
            on_checker(scale(frame_to_rgba(fr, pal), s)).save(
                sub / f"{name.replace('.32','')}_f{i:02d}.png"
            )
        montage(frames, pal, 8, s, f"{name} sci-fi reskin ({len(frames)} frames)").save(
            OUT / f"montage_{name.replace('.32','')}.png"
        )
        if name == "town.32":
            town_imgs = [frame_to_rgba(f, pal) for f in frames]
        if name == "townf.32":
            floor_img = frame_to_rgba(frames[0], pal)

    # Sky stays the original (global sheet, not reskinned).
    sky_img = None
    sky_path = ROOT / "sky.32"
    if sky_path.exists():
        sky_frames, sky_pal, _ = decode_sheet_indexed(sky_path)
        if sky_frames:
            sky_img = frame_to_rgba(sky_frames[0], sky_pal)

    scenes_dir = OUT / "scenes"
    scenes_dir.mkdir(parents=True, exist_ok=True)
    for cap, walls in SCENES:
        on_checker(scale(build_scene(walls, town_imgs, floor_img, sky_img), 3)).save(
            scenes_dir / f"scene_{cap}.png"
        )

    if scifi_pal is not None:
        palette_swatch(scifi_pal, USED_INDICES, "sci-fi palette (green outline = FREE for .anm)").save(
            OUT / "scifi_palette_used.png"
        )
    print(f"Sci-fi reskin previews written to {OUT}")


if __name__ == "__main__":
    main()
