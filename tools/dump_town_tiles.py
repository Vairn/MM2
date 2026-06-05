#!/usr/bin/env python3
"""Decode the MM2 "town" environment sheets to PNGs for sci-fi reskin work.

Dumps per-frame PNGs, a labelled montage, composited 3D corridor scenes, and
the 32-colour palette for:

  town.32   wall set      (32 frames; 0-15 walls/doors, 16-31 wall+torch)
  townf.32  floor backdrop (1 frame, 208x60)
  townt.32  ceiling pieces (27 frames)

Self-contained planar .32 decoder (mirrors decode_planes/load_frame in
tools/render_view_refs.py). Output -> EXTRACTED/gfx_scifi/original/.
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "EXTRACTED" / "gfx_scifi" / "original"

# 3D viewport blit tables (from view_3d_master @0x2ECE / render_view_refs.py).
FRONT_Y_TABLE = [22, 40, 54, 62]
FRONT_X_TABLE = [32, 64, 88, 104]
FRONT_SCREEN_Y = [y + (1 if i == 0 else 0) for i, y in enumerate(FRONT_Y_TABLE)]
LEFT_NEAR_X = [8, 32, 64, 88]
LEFT_NEAR_Y = [8, 22, 40, 54]
RIGHT_NEAR_X = [192, 160, 136, 120]
RIGHT_NEAR_Y = [8, 22, 40, 54]
LEFT_FAR_X = [8, 8, 40, 88]
LEFT_FAR_Y = [22, 40, 54, 62]
RIGHT_FAR_X = [192, 160, 136, 120]
RIGHT_FAR_Y = [22, 40, 54, 62]

# (frame, x, y, caption) — primary 16 viewport slots + reused far d2/d3.
INDOOR_FRAME_VIEWS: list[tuple[int, int, int, str]] = [
    (0, FRONT_X_TABLE[0], FRONT_SCREEN_Y[0], "front d0"),
    (1, FRONT_X_TABLE[1], FRONT_SCREEN_Y[1], "front d1"),
    (2, FRONT_X_TABLE[2], FRONT_SCREEN_Y[2], "front d2"),
    (3, FRONT_X_TABLE[3], FRONT_SCREEN_Y[3], "front d3"),
    (4, LEFT_NEAR_X[0], LEFT_NEAR_Y[0], "left near d0"),
    (5, LEFT_NEAR_X[1], LEFT_NEAR_Y[1], "left near d1"),
    (6, LEFT_NEAR_X[2], LEFT_NEAR_Y[2], "left near d2"),
    (7, LEFT_NEAR_X[3], LEFT_NEAR_Y[3], "left near d3"),
    (8, RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0], "right near d0"),
    (9, RIGHT_NEAR_X[1], RIGHT_NEAR_Y[1], "right near d1"),
    (10, RIGHT_NEAR_X[2], RIGHT_NEAR_Y[2], "right near d2"),
    (11, RIGHT_NEAR_X[3], RIGHT_NEAR_Y[3], "right near d3"),
    (12, LEFT_FAR_X[0], LEFT_FAR_Y[0], "left far d0"),
    (13, RIGHT_FAR_X[0], RIGHT_FAR_Y[0], "right far d0"),
    (14, LEFT_FAR_X[1], LEFT_FAR_Y[1], "left far d1"),
    (15, RIGHT_FAR_X[1], RIGHT_FAR_Y[1], "right far d1"),
]

# Representative corridor scenes (subset of INDOOR_SCENE_VIEWS).
SCENES: list[tuple[str, list[tuple[int, int, int]]]] = [
    ("alcove-d0", [
        (4, LEFT_NEAR_X[0], LEFT_NEAR_Y[0]),
        (8, RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0]),
        (0, FRONT_X_TABLE[0], FRONT_SCREEN_Y[0]),
    ]),
    ("corridor-d1", [
        (5, LEFT_NEAR_X[1], LEFT_NEAR_Y[1]),
        (9, RIGHT_NEAR_X[1], RIGHT_NEAR_Y[1]),
        (1, FRONT_X_TABLE[1], FRONT_SCREEN_Y[1]),
    ]),
    ("corridor-d2", [
        (6, LEFT_NEAR_X[2], LEFT_NEAR_Y[2]),
        (10, RIGHT_NEAR_X[2], RIGHT_NEAR_Y[2]),
        (2, FRONT_X_TABLE[2], FRONT_SCREEN_Y[2]),
    ]),
    # Torch variants (+0x10): same geometry, frames 16+.
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
]

VIEW_W = 208
VIEW_H = 120
ORIGIN_X = 8
SKY_Y = 8
FLOOR_Y = 68


def u16be(b: bytes, off: int) -> int:
    return (b[off] << 8) | b[off + 1]


def decode_planes(data: bytes, off: int, frame_bytes: int) -> tuple[int, bytes]:
    out = bytearray()
    have = False
    pending = 0
    cur = off
    while len(out) < frame_bytes and cur < len(data):
        p = data[cur]
        cur += 1
        cmd = p & 0xF0
        if cmd in (0x00, 0xF0):
            nib = (p >> 4) & 0xF
            times = (p & 0xF) + 1
            for _ in range(times):
                if len(out) >= frame_bytes:
                    break
                if not have:
                    pending = nib
                    have = True
                else:
                    out.append((pending << 4) | nib)
                    have = False
        else:
            for nib in ((p >> 4) & 0xF, p & 0xF):
                if len(out) >= frame_bytes:
                    break
                if not have:
                    pending = nib
                    have = True
                else:
                    out.append((pending << 4) | nib)
                    have = False
    return cur, bytes(out)


def read_palette(b: bytes) -> list[tuple[int, int, int]]:
    n = u16be(b, 0)
    pal_off = 4 + n * 6
    pal = []
    for i in range(32):
        pw = u16be(b, pal_off + i * 2)
        pal.append((((pw >> 8) & 0xF) * 17, ((pw >> 4) & 0xF) * 17, (pw & 0xF) * 17))
    return pal


def decode_sheet(path: Path) -> tuple[list[Image.Image], list[tuple[int, int, int]]]:
    b = path.read_bytes()
    n = u16be(b, 0)
    info = 4
    pal = read_palette(b)
    cur = pal_off = info + n * 6 + 64
    frames: list[Image.Image] = []
    for f in range(n):
        w = u16be(b, info + f * 6)
        h = u16be(b, info + f * 6 + 2)
        bpr = ((w + 15) >> 3) & 0xFFFE
        rs = h * bpr
        cur, planes = decode_planes(b, cur, 5 * rs)
        im = Image.new("RGBA", (w, h))
        px = im.load()
        for y in range(h):
            for x in range(w):
                idx = 0
                for pl in range(5):
                    bp = pl * rs + y * bpr + (x >> 3)
                    bit = (planes[bp] >> (7 - (x & 7))) & 1
                    idx |= bit << pl
                r, g, bl = pal[idx]
                px[x, y] = (r, g, bl, 0 if idx == 0 else 255)
        frames.append(im)
    return frames, pal


def scale(im: Image.Image, s: int) -> Image.Image:
    return im.resize((im.width * s, im.height * s), Image.NEAREST)


def checker_bg(w: int, h: int, cell: int = 8) -> Image.Image:
    bg = Image.new("RGBA", (w, h), (40, 40, 48, 255))
    d = ImageDraw.Draw(bg)
    for y in range(0, h, cell):
        for x in range(0, w, cell):
            if ((x // cell) + (y // cell)) & 1:
                d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(56, 56, 66, 255))
    return bg


def flatten_on_checker(im: Image.Image) -> Image.Image:
    bg = checker_bg(im.width, im.height)
    bg.alpha_composite(im)
    return bg


def montage(frames: list[Image.Image], cols: int, s: int, title: str) -> Image.Image:
    pad = 6
    label_h = 12
    cw = max(f.width for f in frames) * s
    ch = max(f.height for f in frames) * s
    cell_w = cw + pad * 2
    cell_h = ch + pad * 2 + label_h
    rows = (len(frames) + cols - 1) // cols
    W = cols * cell_w
    H = rows * cell_h + 18
    canvas = Image.new("RGBA", (W, H), (24, 24, 30, 255))
    d = ImageDraw.Draw(canvas)
    d.text((6, 4), title, fill=(220, 220, 230, 255))
    for i, fr in enumerate(frames):
        c = i % cols
        r = i // cols
        cx = c * cell_w
        cy = r * cell_h + 18
        sf = scale(fr, s)
        tile = checker_bg(cw, ch)
        tile.alpha_composite(sf, ((cw - sf.width) // 2, (ch - sf.height) // 2))
        canvas.alpha_composite(tile, (cx + pad, cy + pad + label_h))
        d.text((cx + pad, cy + 1), f"{i}", fill=(180, 200, 255, 255))
    return canvas


def build_scene(
    walls: list[tuple[int, int, int]],
    town_frames: list[Image.Image],
    floor: Image.Image | None,
    sky: Image.Image | None,
) -> Image.Image:
    canvas = Image.new("RGBA", (VIEW_W + ORIGIN_X * 2, VIEW_H + 16), (8, 8, 12, 255))
    if sky is not None:
        canvas.alpha_composite(sky, (ORIGIN_X, SKY_Y))
    if floor is not None:
        canvas.alpha_composite(floor, (ORIGIN_X, FLOOR_Y))
    for frame, x, y in walls:
        if 0 <= frame < len(town_frames):
            canvas.alpha_composite(town_frames[frame], (x, y))
    return canvas


def palette_swatch(pal: list[tuple[int, int, int]], title: str) -> Image.Image:
    cell = 28
    cols = 8
    rows = 4
    W = cols * cell
    H = rows * cell + 16
    im = Image.new("RGBA", (W, H), (24, 24, 30, 255))
    d = ImageDraw.Draw(im)
    d.text((4, 3), title, fill=(220, 220, 230, 255))
    for i, (r, g, b) in enumerate(pal):
        cx = (i % cols) * cell
        cy = (i // cols) * cell + 16
        d.rectangle([cx, cy, cx + cell - 1, cy + cell - 1], fill=(r, g, b, 255))
        d.rectangle([cx, cy, cx + cell - 1, cy + cell - 1], outline=(60, 60, 70, 255))
        if i == 0:
            d.line([cx, cy, cx + cell - 1, cy + cell - 1], fill=(255, 0, 0, 255))
    return im


def dump_sheet_frames(name: str, frames: list[Image.Image], scale_to: int) -> None:
    sub = OUT / name.replace(".32", "")
    sub.mkdir(parents=True, exist_ok=True)
    for i, fr in enumerate(frames):
        flatten_on_checker(scale(fr, scale_to)).save(sub / f"{name.replace('.32','')}_f{i:02d}.png")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    sheets = {
        "town.32": 3,
        "townf.32": 2,
        "townt.32": 4,
    }
    decoded: dict[str, tuple[list[Image.Image], list]] = {}
    palettes: dict[str, list] = {}
    for name, s in sheets.items():
        path = ROOT / name
        if not path.exists():
            print(f"!! missing {path}")
            continue
        frames, pal = decode_sheet(path)
        decoded[name] = (frames, pal)
        palettes[name] = pal
        print(f"{name}: {len(frames)} frames, {len(pal)} colours")
        dump_sheet_frames(name, frames, s)
        montage(frames, cols=8, s=s, title=f"{name} ({len(frames)} frames)").save(
            OUT / f"montage_{name.replace('.32','')}.png"
        )
        palette_swatch(pal, f"{name} palette (idx0 = transparent)").save(
            OUT / f"palette_{name.replace('.32','')}.png"
        )

    # Composited 3D corridor scenes (sky + floor + walls).
    town_frames = decoded.get("town.32", ([], []))[0]
    floor = decoded.get("townf.32", ([None], []))[0]
    floor_img = floor[0] if floor else None
    sky_path = ROOT / "sky.32"
    sky_img = None
    if sky_path.exists():
        sky_frames, _ = decode_sheet(sky_path)
        if sky_frames:
            sky_img = sky_frames[0]
    if town_frames:
        scenes_dir = OUT / "scenes"
        scenes_dir.mkdir(parents=True, exist_ok=True)
        for cap, walls in SCENES:
            scene = build_scene(walls, town_frames, floor_img, sky_img)
            flatten_on_checker(scale(scene, 3)).save(scenes_dir / f"scene_{cap}.png")

    # Palette JSON (all sheets).
    pal_json = {
        name: [[r, g, b] for (r, g, b) in pal] for name, pal in palettes.items()
    }
    (OUT / "palettes.json").write_text(json.dumps(pal_json, indent=2))
    print(f"\nWrote reference PNGs + palettes to {OUT}")


if __name__ == "__main__":
    main()
