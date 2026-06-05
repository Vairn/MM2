#!/usr/bin/env python3
"""Build scifi_two assets via mm2_wall_atlas (textures → 3D → frame capture).

See tools/mm2_wall_atlas.py for the atlas generator (dungeon-crawler style).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("pip install pillow numpy", file=sys.stderr)
    raise

from mm2_wall_atlas import (
    ART,
    FLOOR_Y,
    FRONT_X,
    FRONT_Y,
    LEFT_NEAR_X,
    LEFT_NEAR_Y,
    ORIGIN_X,
    OUT,
    RIGHT_NEAR_X,
    RIGHT_NEAR_Y,
    SKY_Y,
    VIEW_H,
    VIEW_W,
    AtlasConfig,
    build_all,
    write_outputs,
)
from render_scifi_corridor_3d import DEFAULT_TEX_DIR, TextureSet, ensure_textures, load_textures

ROOT = Path(__file__).resolve().parents[1]
MASTER_DIRS = [
    Path.home() / ".cursor" / "projects" / "c-20260421-D-REC-development-MM2" / "assets",
    ROOT / "EXTRACTED" / "gfx_scifi" / "src2",
]


def find_ai_master() -> Path | None:
    for d in MASTER_DIRS:
        p = d / "scifi_two_master_corridor.png"
        if p.exists():
            return p
    return None


def load_ai_master() -> Image.Image | None:
    p = find_ai_master()
    if not p:
        return None
    target = (VIEW_W * 3, VIEW_H * 3)
    m = Image.open(p).convert("RGB")
    return m.resize(target, Image.LANCZOS) if m.size != target else m


def frame_to_rgba(fr, pal):
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


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ai", action="store_true", help="slice AI PNG master instead of 3D render")
    ap.add_argument("--procedural", action="store_true")
    ap.add_argument("--tex-dir", type=Path, default=DEFAULT_TEX_DIR)
    args = ap.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    ART.mkdir(parents=True, exist_ok=True)

    cfg = AtlasConfig(tex_dir=str(args.tex_dir))
    tex = TextureSet() if args.procedural else load_textures(args.tex_dir)
    if not args.procedural and not tex.any_loaded:
        tex = ensure_textures(args.tex_dir)

    master = load_ai_master() if args.ai else None
    tag = "ai" if args.ai and master else ("3d-proc" if args.procedural else "3d-tex")

    town, floor, sky, lights, pal, master_img = build_all(
        tex, cfg, procedural=args.procedural, master=master)
    master_img.save(ART / f"master_corridor_{tag}_3x.png")
    write_outputs(town, floor, sky, lights, pal)

    try:
        from cpp_gfx_preview import montage_from_sheet
        montage_from_sheet(OUT / "scifi_two.32", ART / "montage_scifi_two.png",
                           title="scifi_two.32 — open in MM2ED from gfx_scifi/out/")
        montage_from_sheet(OUT / "scifi_twot.32", ART / "montage_scifi_twot.png",
                           cols=9, title="scifi_twot.32")
        print("montage = C++ Gfx decode (matches MM2ED)")
    except Exception as exc:
        print(f"!! montage: {exc}", file=sys.stderr)

    comp = Image.new("RGBA", (VIEW_W, VIEW_H), (0, 0, 0, 255))
    comp.alpha_composite(frame_to_rgba(sky[0], pal), (ORIGIN_X, SKY_Y))
    comp.alpha_composite(frame_to_rgba(floor[0], pal), (ORIGIN_X, FLOOR_Y))
    for im, x, y in [
        (frame_to_rgba(town[4], pal), LEFT_NEAR_X[0], LEFT_NEAR_Y[0]),
        (frame_to_rgba(town[8], pal), RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0]),
        (frame_to_rgba(town[0], pal), FRONT_X[0], FRONT_Y[0]),
    ]:
        comp.alpha_composite(im, (x, y))
    on_checker(scale(comp, 3)).save(ART / "scene_scifi_two_alcove.png")
    print(f"atlas tool: python tools/mm2_wall_atlas.py --tex-dir {args.tex_dir}")
    print(f"Load walls: {OUT / 'scifi_two.32'}")


if __name__ == "__main__":
    main()
