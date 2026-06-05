#!/usr/bin/env python3
"""MM2 wall atlas generator — textures → 3D render → per-frame capture.

Workflow inspired by the Dungeon Crawler Atlas Generator:
  https://dungeoncrawlers.org/tools/atlas_generator/

That tool uploads textures, renders a grid dungeon in WebGL, and exports each
wall/floor/ceiling/decal tile into an atlas.  This module does the MM2-specific
equivalent:

  * 208×120 viewport, blit tables from view_3d_master @0x2ECE
  * Silhouette masks from original town.32 / sky.32 / townf.32 / townt.32
  * 32-colour palette with 16 FREE .anm slots preserved
  * Layers: front walls, front+door, side walls (texture UV fill), floor
    (3D slice), ceiling/sky (flat texture — NOT perspective slice), torches
    (decal paint into townt silhouettes)

Usage:
  python tools/mm2_wall_atlas.py --tex-dir EXTRACTED/gfx_scifi/textures
  python tools/mm2_wall_atlas.py --preset EXTRACTED/gfx_scifi/atlas_preset.json
"""
from __future__ import annotations

import argparse
import json
import math
from dataclasses import asdict, dataclass, field
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow numpy")

from encode_image32 import encode_image32
from make_scifi_art import (
    BLACK,
    FIXTURE_IDX,
    LIGHT_IDX,
    TOWN_DOOR_IDX,
    WALL_IDX,
    Canvas,
    build_art_palette,
    mask_of,
    paint_light,
    paint_lights,
    paint_outline,
)
from render_scifi_corridor_3d import (
    DEFAULT_TEX_DIR,
    TextureSet,
    ensure_textures,
    load_textures,
    render_master,
    sample_tex,
)
from scifi_town_common import ROOT, Frame, decode_sheet_indexed

OUT = ROOT / "EXTRACTED" / "gfx_scifi" / "out"
ART = ROOT / "EXTRACTED" / "gfx_scifi" / "art2"
PRESET_PATH = ROOT / "EXTRACTED" / "gfx_scifi" / "atlas_preset.json"

VIEW_W, VIEW_H = 208, 120
ORIGIN_X, SKY_Y, FLOOR_Y = 8, 8, 68
SCALE = 3

FRONT_X = [32, 64, 88, 104]
FRONT_Y = [23, 40, 54, 62]
LEFT_NEAR_X = [8, 32, 64, 88]
LEFT_NEAR_Y = [8, 22, 40, 54]
RIGHT_NEAR_X = [192, 160, 136, 120]
RIGHT_NEAR_Y = [8, 22, 40, 54]

# frame index -> (blit_x, blit_y) for 3D capture from front corridor master
FRAME_BLIT: dict[int, tuple[int, int]] = {}
for d in range(4):
    FRAME_BLIT[d] = (FRONT_X[d], FRONT_Y[d])
    FRAME_BLIT[4 + d] = (LEFT_NEAR_X[d], LEFT_NEAR_Y[d])
    FRAME_BLIT[8 + d] = (RIGHT_NEAR_X[d], RIGHT_NEAR_Y[d])
FRAME_BLIT[12] = (8, 22)
FRAME_BLIT[13] = (192, 22)
FRAME_BLIT[14] = (8, 40)
FRAME_BLIT[15] = (192, 40)
for f in range(16):
    FRAME_BLIT[16 + f] = FRAME_BLIT[f]

# Frames best captured from 3D front corridor render (front + door faces).
CAPTURE_3D = frozenset(range(4)) | frozenset(range(16, 20))

WALL_QUANT = [3, 22, 29, 30, 31]
DOOR_QUANT = [3, 23, 24, 25, 26, 27, 28]
CEIL_QUANT = [3, 22, 29, 30, 31]
BAND_QUANT = [3, 22, 23, 26, 27, 28, 29, 30, 31]
FREE_INDICES = [4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 21]


@dataclass
class AtlasConfig:
    """Camera / dungeon params (tune like dungeoncrawlers.org atlas tool)."""
    scale: int = 3
    yfov_deg: float = 68.0
    eye_y: float = 1.55
    eye_z: float = 0.8
    corridor_len: float = 7.0
    half_width: float = 2.0
    ceil_height: float = 3.0
    tex_dir: str = str(DEFAULT_TEX_DIR)

    @classmethod
    def load(cls, path: Path) -> AtlasConfig:
        data = json.loads(path.read_text(encoding="utf-8"))
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(asdict(self), indent=2), encoding="utf-8")


def snap4(c: int) -> int:
    return round(c / 255 * 15) * 17


def bbox(coords):
    xs = [c[0] for c in coords]
    ys = [c[1] for c in coords]
    return min(xs), min(ys), max(xs), max(ys)


def rgb_to_idx(rgb, colours: list[tuple[int, tuple[int, int, int]]]) -> int:
    r, g, b = snap4(int(rgb[0])), snap4(int(rgb[1])), snap4(int(rgb[2]))
    best_slot, bestd = colours[0][0], 1 << 30
    for slot, (cr, cg, cb) in colours:
        d = (r - cr) ** 2 + (g - cg) ** 2 + (b - cb) ** 2
        if d < bestd:
            bestd, best_slot = d, slot
    return best_slot


def tex_rgb(tex, u: float, v: float, ru: float = 1.0, rv: float = 1.0):
    return sample_tex(tex, u, v, ru, rv) * 255.0


def slot_colours(pal, slots):
    return [(s, pal[s]) for s in slots]


def build_band_colours(tex: TextureSet, pal) -> list[tuple[int, tuple[int, int, int]]]:
    out: list[tuple[int, tuple[int, int, int]]] = []
    for slot in BAND_QUANT:
        if slot in (22, 29, 30, 31) and tex.wall is not None:
            t = (slot - 22) / max(1, 31 - 22)
            rgb = tex_rgb(tex.wall, 0.25 + t * 0.5, 0.35, 2.0, 2.0)
        elif slot in (23, 26, 27, 28) and tex.floor is not None:
            t = (slot - 23) / 5.0
            rgb = tex_rgb(tex.floor, 0.2 + t * 0.4, 0.55, 2.0, 2.0)
        else:
            rgb = pal[slot]
        out.append((slot, (snap4(int(rgb[0])), snap4(int(rgb[1])), snap4(int(rgb[2])))))
    return out


def sample_master(master: Image.Image, sx: int, sy: int, scale: int):
    mx, my = sx * scale, sy * scale
    r = g = b = 0
    n = 0
    w, h = master.size
    for dy in range(scale):
        for dx in range(scale):
            px, py = mx + dx, my + dy
            if 0 <= px < w and 0 <= py < h:
                pr, pg, pb = master.getpixel((px, py))
                r += pr
                g += pg
                b += pb
                n += 1
    return (r // n, g // n, b // n) if n else (0, 0, 0)


def capture_frame(mask: Frame, blit_x: int, blit_y: int, master: Image.Image,
                  colours, scale: int) -> Frame:
    """Grab one engine frame from a 3D master at ASM blit coords."""
    idxs = [0] * (mask.w * mask.h)
    for y in range(mask.h):
        for x in range(mask.w):
            if mask.idx[y * mask.w + x] == 0:
                continue
            idxs[y * mask.w + x] = rgb_to_idx(
                sample_master(master, blit_x + x, blit_y + y, scale), colours)
    return Frame(mask.w, mask.h, mask.flags, idxs)


def texture_fill_wall(mask: Frame, tex: TextureSet, pal) -> Frame:
    """Side walls / door sides: tile PNG textures into silhouette (UV fill)."""
    wall_px: list[tuple[int, int]] = []
    door_px: list[tuple[int, int]] = []
    light_px: list[tuple[int, int]] = []
    fixture_px: list[tuple[int, int]] = []

    for y in range(mask.h):
        for x in range(mask.w):
            v = mask.idx[y * mask.w + x]
            if v == 0:
                continue
            p = (x, y)
            if v in LIGHT_IDX:
                light_px.append(p)
            elif v in TOWN_DOOR_IDX:
                door_px.append(p)
            elif v in FIXTURE_IDX:
                fixture_px.append(p)
            elif v in WALL_IDX or v == BLACK:
                wall_px.append(p)

    lightset = set(light_px)
    fixtureset = set(fixture_px)
    wall_px = [p for p in wall_px if p not in lightset and p not in fixtureset]

    cv = Canvas(mask.w, mask.h)
    wall_q = slot_colours(pal, WALL_QUANT)
    door_q = slot_colours(pal, DOOR_QUANT)

    if wall_px and tex.wall is not None:
        x0, y0, x1, y1 = bbox(wall_px)
        bw, bh = max(1, x1 - x0), max(1, y1 - y0)
        for x, y in wall_px:
            if mask.idx[y * mask.w + x] == BLACK:
                cv.set(x, y, BLACK)
                continue
            cv.set(x, y, rgb_to_idx(
                tex_rgb(tex.wall, (x - x0) / bw, (y - y0) / bh, 2.5, 2.0), wall_q))

    if door_px and tex.door is not None:
        x0, y0, x1, y1 = bbox(door_px)
        bw, bh = max(1, x1 - x0), max(1, y1 - y0)
        for x, y in door_px:
            cv.set(x, y, rgb_to_idx(
                tex_rgb(tex.door, (x - x0) / bw, (y - y0) / bh, 1.0, 1.0), door_q))

    paint_light(cv, mask_of(mask), mask.w, mask.h, light_px, fixture_px)
    paint_outline(cv, mask_of(mask), mask.w, mask.h)
    return Frame(mask.w, mask.h, mask.flags, cv.px)


def paint_sky_frames(sky_masks: list[Frame], tex: TextureSet, pal) -> list[Frame]:
    """Flat ceiling into sky.32 silhouettes (replaces clouds with hull plating)."""
    ceil_q = slot_colours(pal, CEIL_QUANT)
    out: list[Frame] = []
    for fr in sky_masks:
        idxs = [0] * (fr.w * fr.h)
        for y in range(fr.h):
            for x in range(fr.w):
                if fr.idx[y * fr.w + x] == 0:
                    continue
                u = x / max(1, fr.w - 1)
                v = y / max(1, fr.h - 1)
                if tex.ceil is not None:
                    rgb = tex_rgb(tex.ceil, u, v, 2.0, 0.8)
                else:
                    rgb = pal[29]
                # Brighter near horizon (bottom of band), darker at top — not perspective ribs
                horizon = 0.72 + 0.28 * (y / max(1, fr.h - 1))
                rgb = tuple(min(255, int(c * horizon + 40 * (1 - v))) for c in rgb)
                # Horizontal panel seams
                if int(v * 8) % 2 == 0 and tex.ceil is not None:
                    rgb = tuple(max(0, int(c * 0.82)) for c in rgb)
                idxs[y * fr.w + x] = rgb_to_idx(rgb, ceil_q)
        out.append(Frame(fr.w, fr.h, fr.flags, idxs))
    return out


def build_town_sheet(
    town_masks: list[Frame],
    master: Image.Image,
    tex: TextureSet,
    pal,
    band_colours,
    scale: int,
) -> list[Frame]:
    """Layer: front walls + doors from 3D capture; sides from texture fill."""
    out: list[Frame] = []
    for i, mask in enumerate(town_masks):
        if i in CAPTURE_3D:
            bx, by = FRAME_BLIT.get(i, (104, 40))
            out.append(capture_frame(mask, bx, by, master, band_colours, scale))
        else:
            out.append(texture_fill_wall(mask, tex, pal))
    return out


def build_all(
    tex: TextureSet,
    cfg: AtlasConfig,
    *,
    procedural: bool = False,
    master: Image.Image | None = None,
) -> tuple[list[Frame], list[Frame], list[Frame], list[Frame], list[tuple[int, int, int]], Image.Image]:
    town_masks, town_pal, town_depth = decode_sheet_indexed(ROOT / "town.32")
    floor_masks, _, floor_depth = decode_sheet_indexed(ROOT / "townf.32")
    light_masks, _, light_depth = decode_sheet_indexed(ROOT / "townt.32")
    sky_masks, _, sky_depth = decode_sheet_indexed(ROOT / "sky.32")

    pal = build_art_palette(town_pal)
    band = build_band_colours(tex, pal)
    scale = cfg.scale

    if master is None:
        if procedural:
            master = render_master(scale, procedural=True)
        else:
            master = render_master(scale, tex=tex)

    town_art = build_town_sheet(town_masks, master, tex, pal, band, scale)
    floor_art = [
        capture_frame(f, ORIGIN_X, FLOOR_Y, master, band, scale) for f in floor_masks]
    sky_art = paint_sky_frames(sky_masks, tex, pal)
    light_art = paint_lights(light_masks)

    return town_art, floor_art, sky_art, light_art, pal, master


def write_outputs(
    town_art, floor_art, sky_art, light_art, pal,
    *,
    out_dir: Path = OUT,
) -> None:
    _, _, town_depth = decode_sheet_indexed(ROOT / "town.32")
    _, _, floor_depth = decode_sheet_indexed(ROOT / "townf.32")
    _, _, light_depth = decode_sheet_indexed(ROOT / "townt.32")
    _, _, sky_depth = decode_sheet_indexed(ROOT / "sky.32")

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "scifi_two.32").write_bytes(encode_image32(town_art, pal, town_depth))
    (out_dir / "scifi_twof.32").write_bytes(encode_image32(floor_art, pal, floor_depth))
    (out_dir / "scifi_twot.32").write_bytes(encode_image32(light_art, pal, light_depth))
    (out_dir / "scifi_twos.32").write_bytes(encode_image32(sky_art, pal, sky_depth))

    free_ok = all(pal[i] == decode_sheet_indexed(ROOT / "town.32")[1][i] for i in FREE_INDICES)
    print(f"wrote scifi_two/twof/twot/twos.32 -> {out_dir}")
    print(f"FREE .anm slots preserved: {free_ok}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tex-dir", type=Path, default=DEFAULT_TEX_DIR)
    ap.add_argument("--preset", type=Path, default=None, help="JSON camera preset")
    ap.add_argument("--save-preset", type=Path, default=None)
    ap.add_argument("--procedural", action="store_true")
    args = ap.parse_args()

    cfg = AtlasConfig.load(args.preset) if args.preset else AtlasConfig(tex_dir=str(args.tex_dir))
    if args.tex_dir != DEFAULT_TEX_DIR:
        cfg.tex_dir = str(args.tex_dir)
    if args.save_preset:
        cfg.save(args.save_preset)
        print(f"saved preset -> {args.save_preset}")
        return

    tex = TextureSet() if args.procedural else load_textures(Path(cfg.tex_dir))
    if not args.procedural and not tex.any_loaded:
        tex = ensure_textures(Path(cfg.tex_dir))

    town, floor, sky, lights, pal, master = build_all(tex, cfg, procedural=args.procedural)
    ART.mkdir(parents=True, exist_ok=True)
    master.save(ART / "atlas_master_3x.png")
    write_outputs(town, floor, sky, lights, pal)

    try:
        from cpp_gfx_preview import montage_from_sheet
        montage_from_sheet(OUT / "scifi_two.32", ART / "montage_scifi_two.png",
                           title="scifi_two.32 (MM2ED decode)")
        montage_from_sheet(OUT / "scifi_twot.32", ART / "montage_scifi_twot.png",
                           cols=9, title="scifi_twot.32")
    except Exception as exc:
        print(f"montage: {exc}", file=__import__("sys").stderr)

    if not PRESET_PATH.exists():
        cfg.save(PRESET_PATH)
        print(f"default preset -> {PRESET_PATH}")


if __name__ == "__main__":
    main()
