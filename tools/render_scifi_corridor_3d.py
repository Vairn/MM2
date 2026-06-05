#!/usr/bin/env python3
"""Software 3D corridor renderer for MM2 sci-fi wall-set frame extraction.

Renders a first-person sci-fi corridor from the player eye position, producing a
624x360 master image (3x the 208x120 viewport).  make_scifi_two.py slices
engine frames from this render at the exact ASM blit coordinates.

Surfaces use PNG textures from EXTRACTED/gfx_scifi/textures/ when present
(wall, floor, ceiling, door, light); procedural shading is the fallback.

No external 3D deps — pure numpy ray/plane intersection + texture sampling.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow numpy")

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEX_DIR = ROOT / "EXTRACTED" / "gfx_scifi" / "textures"
DEFAULT_CONCEPT = ROOT / "EXTRACTED" / "gfx_scifi" / "concept" / "scifi_town_concept.png"

VIEW_W, VIEW_H = 208, 120
ORIGIN_X, FLOOR_Y = 8, 68


@dataclass
class Hit:
    dist: float
    kind: str  # floor | ceil | left | right | back | door
    u: float
    v: float


@dataclass
class TextureSet:
    wall: np.ndarray | None = None
    floor: np.ndarray | None = None
    ceil: np.ndarray | None = None
    door: np.ndarray | None = None
    light: np.ndarray | None = None

    @property
    def any_loaded(self) -> bool:
        return any(getattr(self, k) is not None for k in ("wall", "floor", "ceil", "door", "light"))


# --- colours (linear 0..1, procedural fallback) ------------------------------
def c(r, g, b):
    return np.array([r, g, b], dtype=np.float64)


COL_FLOOR_DK = c(0.06, 0.08, 0.10)
COL_FLOOR_LT = c(0.14, 0.18, 0.22)
COL_FLOOR_GLOW = c(0.0, 0.55, 0.65)
COL_WALL_DK = c(0.12, 0.14, 0.18)
COL_WALL_LT = c(0.28, 0.32, 0.38)
COL_RIVET = c(0.55, 0.58, 0.62)
COL_CEIL = c(0.04, 0.05, 0.07)
COL_PIPE = c(0.10, 0.12, 0.16)
COL_DOOR = c(0.18, 0.20, 0.24)
COL_DOOR_DK = c(0.10, 0.11, 0.14)
COL_HAZARD_A = c(0.85, 0.55, 0.05)
COL_HAZARD_B = c(0.08, 0.08, 0.08)
COL_SEAM = c(0.0, 0.85, 0.95)
COL_LIGHT = c(0.2, 0.95, 1.0)
COL_VOID = c(0.02, 0.02, 0.03)


def lerp(a, b, t):
    return a + (b - a) * np.clip(t, 0.0, 1.0)


def checker(u, v, scale):
    return (int(u * scale) + int(v * scale)) & 1


def load_tex_array(path: Path) -> np.ndarray | None:
    if not path.exists():
        return None
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def load_textures(tex_dir: Path | None) -> TextureSet:
    if tex_dir is None:
        return TextureSet()
    return TextureSet(
        wall=load_tex_array(tex_dir / "wall.png"),
        floor=load_tex_array(tex_dir / "floor.png"),
        ceil=load_tex_array(tex_dir / "ceiling.png"),
        door=load_tex_array(tex_dir / "door.png"),
        light=load_tex_array(tex_dir / "light.png"),  # optional horizontal strip
    )


def ensure_textures(tex_dir: Path = DEFAULT_TEX_DIR, concept: Path = DEFAULT_CONCEPT) -> TextureSet:
    """Load textures; auto-extract from concept art if the folder is empty."""
    needed = ("wall.png", "floor.png", "ceiling.png", "door.png")
    if not all((tex_dir / n).exists() for n in needed):
        from extract_scifi_textures import extract

        print(f"extracting textures -> {tex_dir}")
        extract(concept, tex_dir)
    return load_textures(tex_dir)


def sample_tex(tex: np.ndarray, u: float, v: float, repeat_u: float = 1.0, repeat_v: float = 1.0) -> np.ndarray:
    """Bilinear sample with repeat wrap."""
    u = (u * repeat_u) % 1.0
    v = (v * repeat_v) % 1.0
    h, w = tex.shape[:2]
    fu = u * w - 0.5
    fv = v * h - 0.5
    x0 = int(math.floor(fu)) % w
    y0 = int(math.floor(fv)) % h
    x1 = (x0 + 1) % w
    y1 = (y0 + 1) % h
    tx = fu - math.floor(fu)
    ty = fv - math.floor(fv)
    c00 = tex[y0, x0]
    c10 = tex[y0, x1]
    c01 = tex[y1, x0]
    c11 = tex[y1, x1]
    return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty)


def apply_fog(col: np.ndarray, dist: float, strength: float = 0.55) -> np.ndarray:
    fog = min(1.0, dist / 14.0)
    return lerp(col, COL_VOID, fog * strength)


def overlay_light_strip(col: np.ndarray, u: float, v: float, z: float) -> np.ndarray:
    """Horizontal cyan band on side walls (torch zone — separate townt sprites)."""
    if not (0.40 < v < 0.54) or not (1.5 < z < 4.5):
        return col
    strength = max(0.0, 1.0 - abs(v - 0.47) / 0.05)
    return lerp(col, COL_LIGHT, strength * 0.55)


# --- procedural fallbacks ----------------------------------------------------
def shade_floor_proc(u, v, dist):
    grate = checker(u, v, 4.0)
    base = lerp(COL_FLOOR_DK, COL_FLOOR_LT, 0.35 if grate else 0.0)
    if abs(u) < 0.15 and checker(u, v, 8.0):
        base = lerp(base, COL_FLOOR_GLOW, 0.45)
    return apply_fog(base, dist)


def shade_wall_proc(u, v):
    seam_y = int(v * 6)
    seam_x = int(u * 4)
    in_seam = (abs(v * 6 - seam_y - 0.5) < 0.04) or (abs(u * 4 - seam_x - 0.5) < 0.03)
    rivet = abs(u * 4 - int(u * 4) - 0.5) < 0.06 and abs(v * 6 - int(v * 6) - 0.5) < 0.06
    base = lerp(COL_WALL_DK, COL_WALL_LT, 0.25 + 0.15 * math.sin(u * 12))
    if in_seam:
        base = lerp(base, COL_WALL_DK, 0.7)
    if rivet:
        base = lerp(base, COL_RIVET, 0.8)
    return base


def shade_door_proc(u, v):
    col = lerp(COL_DOOR, COL_DOOR_DK, 0.3 * math.sin(u * 20))
    if 0.35 < v < 0.55:
        col = COL_HAZARD_A if checker(u, v, 6) else COL_HAZARD_B
    if abs(u - 0.5) < 0.025:
        col = lerp(col, COL_SEAM, 0.9)
    elif abs(u - 0.5) < 0.06:
        col = lerp(col, COL_SEAM, 0.35)
    if 0.58 < v < 0.82 and (0.25 < u < 0.42 or 0.58 < u < 0.75):
        col = lerp(COL_DOOR_DK, COL_SEAM, 0.25)
    return col


def shade_ceil_proc(u, v):
    col = COL_CEIL
    if int(v * 5) % 2 == 0:
        col = lerp(col, COL_PIPE, 0.6)
    return col


# --- textured shading --------------------------------------------------------
def shade_floor(u, v, dist, tex: TextureSet) -> np.ndarray:
    if tex.floor is not None:
        col = sample_tex(tex.floor, u, v, 3.0, 3.0)
        return apply_fog(col, dist)
    return shade_floor_proc(u, v, dist)


def shade_wall(u, v, z, dist, tex: TextureSet) -> np.ndarray:
    if tex.wall is not None:
        col = sample_tex(tex.wall, u, v, 2.5, 1.8)
        col = overlay_light_strip(col, u, v, z)
        return apply_fog(col, dist, 0.35)
    col = shade_wall_proc(u, v)
    if 0.42 < v < 0.52 and 1.5 < z < 4.5:
        glow = max(0.0, 1.0 - abs(v - 0.47) / 0.05)
        col = lerp(col, COL_LIGHT, glow * 0.85)
    return apply_fog(col, dist, 0.35)


def shade_back(u, v, dist, tex: TextureSet) -> np.ndarray:
    if tex.wall is not None:
        col = sample_tex(tex.wall, u, v, 1.5, 1.5)
        return apply_fog(col, dist, 0.4)
    return apply_fog(shade_wall_proc(u, v), dist, 0.4)


def shade_door(u, v, dist, tex: TextureSet) -> np.ndarray:
    if tex.door is not None:
        col = sample_tex(tex.door, u, v, 1.0, 1.0)
        return apply_fog(col, dist, 0.25)
    return apply_fog(shade_door_proc(u, v), dist, 0.25)


def shade_ceil(u, v, dist, tex: TextureSet) -> np.ndarray:
    if tex.ceil is not None:
        col = sample_tex(tex.ceil, u, v, 2.0, 1.0)
        return apply_fog(col, dist, 0.5)
    return apply_fog(shade_ceil_proc(u, v), dist, 0.5)


def cast_ray(ro, rd, corridor_len=7.0, half_w=2.0, ceil_h=3.0):
    """Return closest Hit against corridor box + door opening."""
    best: Hit | None = None

    def try_hit(t, kind, u, v):
        nonlocal best
        if t is None or t < 0.05:
            return
        if best is None or t < best.dist:
            best = Hit(t, kind, u, v)

    if abs(rd[1]) > 1e-6:
        t = -ro[1] / rd[1]
        if t > 0:
            p = ro + rd * t
            if abs(p[0]) <= half_w and -corridor_len <= p[2] <= 0.5:
                try_hit(t, "floor", p[0], -p[2])

    if abs(rd[1]) > 1e-6:
        t = (ceil_h - ro[1]) / rd[1]
        if t > 0:
            p = ro + rd * t
            if abs(p[0]) <= half_w and -corridor_len <= p[2] <= 0.5:
                u = (p[0] + half_w) / (2 * half_w)
                v = (-p[2]) / corridor_len
                try_hit(t, "ceil", u, v)

    if abs(rd[0]) > 1e-6:
        t = (-half_w - ro[0]) / rd[0]
        if t > 0:
            p = ro + rd * t
            if 0 <= p[1] <= ceil_h and -corridor_len <= p[2] <= 0.5:
                u = (-p[2]) / corridor_len
                v = p[1] / ceil_h
                try_hit(t, "left", u, v)

    if abs(rd[0]) > 1e-6:
        t = (half_w - ro[0]) / rd[0]
        if t > 0:
            p = ro + rd * t
            if 0 <= p[1] <= ceil_h and -corridor_len <= p[2] <= 0.5:
                u = (-p[2]) / corridor_len
                v = p[1] / ceil_h
                try_hit(t, "right", u, v)

    if abs(rd[2]) > 1e-6:
        t = (-corridor_len - ro[2]) / rd[2]
        if t > 0:
            p = ro + rd * t
            if abs(p[0]) <= half_w and 0 <= p[1] <= ceil_h:
                u = (p[0] + half_w) / (2 * half_w)
                v = p[1] / ceil_h
                in_door = 0.28 < u < 0.72 and v < 0.88
                if in_door:
                    du = (u - 0.28) / 0.44
                    dv = v / 0.88
                    try_hit(t, "door", du, dv)
                else:
                    try_hit(t, "back", u, v)

    return best


def shade_hit(hit: Hit, z_world: float, tex: TextureSet) -> np.ndarray:
    k, u, v, d = hit.kind, hit.u, hit.v, hit.dist
    if k == "floor":
        return shade_floor(u, v, d, tex)
    if k == "ceil":
        return shade_ceil(u, v, d, tex)
    if k in ("left", "right"):
        return shade_wall(u, v, z_world, d, tex)
    if k == "door":
        return shade_door(u, v, d, tex)
    if k == "back":
        return shade_back(u, v, d, tex)
    return COL_VOID


def render_master(scale: int = 3, tex: TextureSet | None = None, procedural: bool = False) -> Image.Image:
    """Render corridor to RGB PIL image (VIEW_W*scale x VIEW_H*scale)."""
    if procedural:
        textures = TextureSet()
    elif tex is not None:
        textures = tex
    else:
        textures = ensure_textures()

    w, h = VIEW_W * scale, VIEW_H * scale
    img = np.zeros((h, w, 3), dtype=np.float64)

    eye = np.array([0.0, 1.55, 0.8])
    yfov = math.radians(68.0)
    aspect = w / h
    xfov = 2 * math.atan(math.tan(yfov / 2) * aspect)

    mode = "textures" if textures.any_loaded else "procedural"
    if scale >= 3:
        print(f"  render mode: {mode}")

    for py in range(h):
        sv = (py + 0.5) / h
        pitch = (0.5 - sv) * yfov
        cp, sp = math.cos(pitch), math.sin(pitch)
        for px in range(w):
            su = (px + 0.5) / w
            yaw = (su - 0.5) * xfov
            cy, sy = math.cos(yaw), math.sin(yaw)
            rd = np.array([sy * cp, sp, -cy * cp])
            rd /= np.linalg.norm(rd)
            hit = cast_ray(eye, rd)
            if hit is None:
                col = COL_VOID
            else:
                p = eye + rd * hit.dist
                col = shade_hit(hit, -p[2], textures)
            img[py, px] = col

    rgb8 = np.clip(img * 255, 0, 255).astype(np.uint8)
    return Image.fromarray(rgb8, "RGB")


if __name__ == "__main__":
    import argparse

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--procedural", action="store_true", help="skip PNG textures")
    ap.add_argument("--tex-dir", type=Path, default=DEFAULT_TEX_DIR)
    ap.add_argument("--scale", type=int, default=3)
    args = ap.parse_args()

    out = ROOT / "EXTRACTED" / "gfx_scifi" / "art2"
    out.mkdir(parents=True, exist_ok=True)
    tex = TextureSet() if args.procedural else ensure_textures(args.tex_dir)
    im = render_master(args.scale, tex=tex, procedural=args.procedural)
    tag = "proc" if args.procedural else "tex"
    im.save(out / f"master_corridor_3d_{tag}.png")
    im.resize((VIEW_W, VIEW_H), Image.LANCZOS).save(out / f"master_corridor_3d_{tag}_1x.png")
    print(f"wrote {out / f'master_corridor_3d_{tag}.png'} ({im.size[0]}x{im.size[1]})")
