#!/usr/bin/env python3
"""Paint NEW sci-fi art into the town tileset silhouettes.

Unlike the palette-swap reskin, this REDRAWS the interior of every frame:
metal panel walls, sliding blast doors with hazard stripes + glowing seam,
cyan light tubes (the "torches"), and a grated tech-deck floor.

What is preserved:
  * each frame's exact opaque MASK (silhouette / alpha) -> engine perspective
    geometry still fits pixel-for-pixel
  * the semantic REGION each pixel belongs to (wall / door / light / fixture /
    floor), classified from the original palette index, so doors stay where
    doors were and lights stay where torches were
  * the 16 FREE palette slots {4..17,20,21} (reserved for .anm overlays) keep
    their original RGB; only the 16 USED slots are repainted

Outputs (drop-in .32 assets, original game files untouched):
  EXTRACTED/gfx_scifi/out/scifi.32    (was town.32  - wall set)
  EXTRACTED/gfx_scifi/out/scifif.32   (was townf.32 - floor)
  EXTRACTED/gfx_scifi/out/scifit.32   (was townt.32 - light fixtures)
plus preview montages + a 3D corridor scene in EXTRACTED/gfx_scifi/art/.
"""
from __future__ import annotations

import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

from encode_image32 import encode_image32
from scifi_town_common import ROOT, Frame, decode_sheet_indexed

OUT = ROOT / "EXTRACTED" / "gfx_scifi" / "out"
ART = ROOT / "EXTRACTED" / "gfx_scifi" / "art"

# ---------------------------------------------------------------------------
# Art palette: only the 16 USED indices are (re)defined here. FREE indices
# {4..17,20,21} keep their original town-palette RGB (applied at build time).
# All values are on the Amiga 4-bit/channel grid (multiples of 17).
# ---------------------------------------------------------------------------
TRANSPARENT = 0
BLACK = 3          # outline / deepest shadow
SPEC = 1           # near-white highlight / rivet glint
M_HI = 31          # metal highlight
M_MID = 30         # metal light
M_MID2 = 29        # metal mid
M_DARK = 22        # metal dark
M_SHADOW = 28      # metal seam shadow
C_CORE = 2         # brightest cyan (light core)
C_BRIGHT = 19      # bright cyan
C_MID = 18         # mid cyan
C_DARK = 27        # deep cyan / recess
AMBER = 25         # hazard amber
AMBER_DK = 24      # hazard dark amber
DECK_DK = 26       # deck/grate dark
DECK_MID = 23      # deck/grate mid

ART_PALETTE: dict[int, tuple[int, int, int]] = {
    BLACK: (0, 0, 0),
    SPEC: (238, 255, 255),
    M_HI: (187, 204, 221),
    M_MID: (136, 153, 170),
    M_MID2: (102, 119, 136),
    M_DARK: (51, 68, 85),
    M_SHADOW: (34, 34, 51),
    C_CORE: (0, 238, 255),
    C_BRIGHT: (0, 187, 238),
    C_MID: (0, 136, 187),
    C_DARK: (0, 68, 102),
    AMBER: (255, 170, 17),
    AMBER_DK: (187, 102, 0),
    DECK_DK: (34, 51, 51),
    DECK_MID: (68, 85, 85),
}

# Original-index -> region, per sheet (used pixels only; 0 stays transparent).
WALL_IDX = {22, 29, 30, 31}
TOWN_DOOR_IDX = {19, 23, 24, 25, 26, 27, 28}
LIGHT_IDX = {1, 2, 18, 19}
FIXTURE_IDX = {23, 24, 25, 26, 27, 28}


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


class Canvas:
    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.px = [TRANSPARENT] * (w * h)

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.px[y * self.w + x]
        return -1

    def set(self, x, y, v):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = v


def mask_of(fr: Frame):
    return [fr.idx[i] != 0 for i in range(fr.w * fr.h)]


def bbox(coords):
    xs = [c[0] for c in coords]
    ys = [c[1] for c in coords]
    return min(xs), min(ys), max(xs), max(ys)


def is_boundary(mask, w, h, x, y):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = x + dx, y + dy
        if nx < 0 or ny < 0 or nx >= w or ny >= h or not mask[ny * w + nx]:
            return True
    return False


def paint_wall(cv: Canvas, mask, w, h, pixels):
    """Metal panel wall: vertical gradient + embossed panel grid + rivets."""
    if not pixels:
        return
    bx0, by0, bx1, by1 = bbox(pixels)
    bw = max(1, bx1 - bx0)
    bh = max(1, by1 - by0)
    seam_h = clamp(round(bh / 4), 4, 22)
    seam_w = clamp(round(bw / 4), 4, 26)
    pixset = set(pixels)

    for (x, y) in pixels:
        ty = (y - by0) / bh
        if ty < 0.16:
            base = M_HI
        elif ty < 0.45:
            base = M_MID
        elif ty < 0.78:
            base = M_MID2
        else:
            base = M_DARK
        cv.set(x, y, base)

    # Embossed seams: highlight row/col then shadow line on top.
    for (x, y) in pixels:
        on_h = ((y - by0) % seam_h) == 0
        on_v = ((x - bx0) % seam_w) == 0
        if on_h or on_v:
            if (x, y - 1) in pixset and not (((y - 1 - by0) % seam_h) == 0):
                cv.set(x, y - 1, M_HI)
            if (x - 1, y) in pixset and not (((x - 1 - bx0) % seam_w) == 0):
                cv.set(x - 1, y, M_HI)
            cv.set(x, y, M_SHADOW)

    # Rivets at panel intersections.
    gy = by0
    while gy <= by1:
        gx = bx0
        while gx <= bx1:
            if (gx + 1, gy + 1) in pixset:
                cv.set(gx + 1, gy + 1, SPEC)
            gx += seam_w
        gy += seam_h


def paint_outline(cv: Canvas, mask, w, h):
    for y in range(h):
        for x in range(w):
            if mask[y * w + x] and is_boundary(mask, w, h, x, y):
                cv.set(x, y, BLACK)


def paint_door(cv: Canvas, mask, w, h, pixels):
    """Sliding blast door: two leaves, glowing centre seam, hazard stripes."""
    if not pixels:
        return
    dx0, dy0, dx1, dy1 = bbox(pixels)
    dh = max(1, dy1 - dy0)
    rows: dict[int, list[int]] = {}
    for (x, y) in pixels:
        rows.setdefault(y, []).append(x)

    # Base metal door fill (leaves slightly different value).
    for (x, y) in pixels:
        xs = rows[y]
        left, right = min(xs), max(xs)
        mid = (left + right) // 2
        cv.set(x, y, M_MID2 if x <= mid else M_DARK)

    # Hazard stripe bands (amber / black diagonal).
    band_h = max(2, round(dh * 0.13))
    stripe_w = max(2, round((dx1 - dx0) / 12))
    for band_top in (dy0 + round(dh * 0.20), dy0 + round(dh * 0.58)):
        for y in range(band_top, band_top + band_h):
            for x in rows.get(y, []):
                cv.set(x, y, AMBER if ((x + y) // stripe_w) % 2 == 0 else BLACK)

    # Glowing central seam (follows per-row skew).
    for y, xs in rows.items():
        left, right = min(xs), max(xs)
        mid = (left + right) // 2
        cv.set(mid, y, C_CORE)
        if (mid - 1) >= left:
            cv.set(mid - 1, y, C_MID)
        if (mid + 1) <= right:
            cv.set(mid + 1, y, C_MID)

    # Door edge outline against the wall.
    pixset = set(pixels)
    for (x, y) in pixels:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (x + dx, y + dy) not in pixset:
                cv.set(x, y, BLACK)
                break


def paint_light(cv: Canvas, mask, w, h, light_pixels, fixture_pixels):
    """Cyan energy light: glowing core in the flame area + metal bracket."""
    # Fixture bracket first (so light glow sits on top).
    if fixture_pixels:
        fx0, fy0, fx1, fy1 = bbox(fixture_pixels)
        cxf = (fx0 + fx1) // 2
        for (x, y) in fixture_pixels:
            cv.set(x, y, M_DARK)
        # Vertical light tube down the bracket centre.
        for (x, y) in fixture_pixels:
            if abs(x - cxf) <= 0:
                cv.set(x, y, C_MID)
        # Amber mounting bolts at top & bottom.
        for (x, y) in fixture_pixels:
            if y <= fy0 + 1 or y >= fy1 - 1:
                cv.set(x, y, AMBER)

    if light_pixels:
        cx = sum(p[0] for p in light_pixels) / len(light_pixels)
        cy = sum(p[1] for p in light_pixels) / len(light_pixels)
        maxr = max(1.0, max(((p[0] - cx) ** 2 + (p[1] - cy) ** 2) ** 0.5
                            for p in light_pixels))
        top = min(light_pixels, key=lambda p: p[1])
        for (x, y) in light_pixels:
            r = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 / maxr
            if r < 0.35:
                cv.set(x, y, C_CORE)
            elif r < 0.7:
                cv.set(x, y, C_BRIGHT)
            else:
                cv.set(x, y, C_MID)
        cv.set(top[0], top[1], SPEC)


def paint_floor(cv: Canvas, mask, w, h, pixels):
    """Grated tech-deck: perspective slats + cyan seam accents."""
    if not pixels:
        return
    bx0, by0, bx1, by1 = bbox(pixels)
    rows: dict[int, list[int]] = {}
    for (x, y) in pixels:
        rows.setdefault(y, []).append(x)

    for (x, y) in pixels:
        cv.set(x, y, DECK_DK)

    # Horizontal slats, spacing grows toward the bottom (nearer the viewer).
    pos = by1
    band = 0
    while pos > by0:
        for x in rows.get(pos, []):
            cv.set(x, pos, BLACK)
        for x in rows.get(pos - 1, []):
            cv.set(x, pos - 1, C_MID if band % 3 == 0 else DECK_MID)
        depth = (by1 - pos) / max(1, (by1 - by0))
        gap = max(3, round(3 + depth * 7))
        pos -= gap
        band += 1

    # Faint vertical seams.
    seam_w = max(6, round((bx1 - bx0) / 6))
    for (x, y) in pixels:
        if (x - bx0) % seam_w == 0:
            if cv.get(x, y) == DECK_DK:
                cv.set(x, y, M_DARK)


def build_art_palette(orig_pal):
    pal = list(orig_pal)
    for i, rgb in ART_PALETTE.items():
        pal[i] = rgb
    return pal


def paint_town(frames):
    out = []
    for fr in frames:
        mask = mask_of(fr)
        wall = [(i % fr.w, i // fr.w) for i in range(fr.w * fr.h)
                if fr.idx[i] in WALL_IDX or (fr.idx[i] == BLACK)]
        door = [(i % fr.w, i // fr.w) for i in range(fr.w * fr.h)
                if fr.idx[i] in TOWN_DOOR_IDX]
        # Black pixels that are inside the door cluster should not be walls.
        doorset = set(door)
        wall = [p for p in wall if p not in doorset]
        cv = Canvas(fr.w, fr.h)
        paint_wall(cv, mask, fr.w, fr.h, wall)
        paint_door(cv, mask, fr.w, fr.h, door)
        paint_outline(cv, mask, fr.w, fr.h)
        out.append(Frame(fr.w, fr.h, fr.flags, cv.px))
    return out


def paint_lights(frames):
    out = []
    for fr in frames:
        mask = mask_of(fr)
        light = [(i % fr.w, i // fr.w) for i in range(fr.w * fr.h)
                 if fr.idx[i] in LIGHT_IDX]
        lightset = set(light)
        fixture = [(i % fr.w, i // fr.w) for i in range(fr.w * fr.h)
                   if fr.idx[i] in FIXTURE_IDX and (i % fr.w, i // fr.w) not in lightset]
        wall = [(i % fr.w, i // fr.w) for i in range(fr.w * fr.h)
                if fr.idx[i] in WALL_IDX or fr.idx[i] == BLACK]
        wall = [p for p in wall if p not in lightset and p not in set(fixture)]
        cv = Canvas(fr.w, fr.h)
        paint_wall(cv, mask, fr.w, fr.h, wall)
        paint_light(cv, mask, fr.w, fr.h, light, fixture)
        paint_outline(cv, mask, fr.w, fr.h)
        out.append(Frame(fr.w, fr.h, fr.flags, cv.px))
    return out


def paint_floor_sheet(frames):
    out = []
    for fr in frames:
        mask = mask_of(fr)
        floor = [(i % fr.w, i // fr.w) for i in range(fr.w * fr.h) if fr.idx[i] != 0]
        cv = Canvas(fr.w, fr.h)
        paint_floor(cv, mask, fr.w, fr.h, floor)
        out.append(Frame(fr.w, fr.h, fr.flags, cv.px))
    return out


# ---------------------------------------------------------------------------
# Preview rendering
# ---------------------------------------------------------------------------
FRONT_X = [32, 64, 88, 104]
FRONT_Y = [23, 40, 54, 62]
LN_X = [8, 32, 64, 88]
LN_Y = [8, 22, 40, 54]
RN_X = [192, 160, 136, 120]
RN_Y = [8, 22, 40, 54]
VIEW_W, VIEW_H, ORIGIN_X, FLOOR_Y = 208, 120, 8, 68


def frame_to_rgba(fr: Frame, pal):
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


def scene(town_imgs, floor_img, light_imgs):
    cv = Image.new("RGBA", (VIEW_W + ORIGIN_X * 2, VIEW_H + 16), (8, 8, 12, 255))
    if floor_img is not None:
        cv.alpha_composite(floor_img, (ORIGIN_X, FLOOR_Y))
    # near alcove: left/right near d0 + front door d0, plus lights.
    walls = [
        (town_imgs[4], LN_X[0], LN_Y[0]),
        (town_imgs[8], RN_X[0], RN_Y[0]),
        (town_imgs[16], FRONT_X[0], FRONT_Y[0]),  # front door (torch variant)
    ]
    for im, x, y in walls:
        cv.alpha_composite(im, (x, y))
    # light fixtures (townt frame 18 vertical, near depth) on side walls.
    if light_imgs:
        cv.alpha_composite(light_imgs[18], (LN_X[0] + 2, LN_Y[0] + 6))
        cv.alpha_composite(light_imgs[18], (RN_X[0] + 2, RN_Y[0] + 6))
    return cv


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    ART.mkdir(parents=True, exist_ok=True)

    town_frames, town_pal, town_depth = decode_sheet_indexed(ROOT / "town.32")
    floor_frames, floor_pal, floor_depth = decode_sheet_indexed(ROOT / "townf.32")
    light_frames, light_pal, light_depth = decode_sheet_indexed(ROOT / "townt.32")

    pal = build_art_palette(town_pal)  # shared 32-colour palette

    town_art = paint_town(town_frames)
    floor_art = paint_floor_sheet(floor_frames)
    light_art = paint_lights(light_frames)

    (OUT / "scifi.32").write_bytes(encode_image32(town_art, pal, town_depth))
    (OUT / "scifif.32").write_bytes(encode_image32(floor_art, pal, floor_depth))
    (OUT / "scifit.32").write_bytes(encode_image32(light_art, pal, light_depth))
    print(f"Wrote scifi.32 / scifif.32 / scifit.32 -> {OUT}")

    montage(town_art, pal, 8, 3, "scifi.32 wall set (new art, 32 frames)").save(
        ART / "montage_scifi.png")
    montage(light_art, pal, 8, 4, "scifit.32 lights (new art, 27 frames)").save(
        ART / "montage_scifit.png")
    montage(floor_art, pal, 1, 2, "scifif.32 floor (new art)").save(
        ART / "montage_scifif.png")

    town_imgs = [frame_to_rgba(f, pal) for f in town_art]
    floor_img = frame_to_rgba(floor_art[0], pal)
    light_imgs = [frame_to_rgba(f, pal) for f in light_art]
    on_checker(scale(scene(town_imgs, floor_img, light_imgs), 3)).save(
        ART / "scene_scifi_alcove.png")
    print(f"Wrote previews -> {ART}")


if __name__ == "__main__":
    main()
