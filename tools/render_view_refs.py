#!/usr/bin/env python3
"""Build accurate 3D-view reference composites from decoded .32 PPM frames.

Uses real blit positions from view_3d_master @0x2ECE / View3D.cpp depth tables.
Run dump_gfx_frame first, or pass --dump to invoke it.
"""
from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "EXTRACTED" / "docs" / "img"
FRAMES = OUT / "frames"

# view_3d_master backdrop + wall tables (capstone 0x2ECE, View3D.cpp)
ORIGIN_X = 8
SKY_Y = 8
FLOOR_Y = 68
VIEW_W = 208
VIEW_H = 120  # sky 60 + floor 60

# A4-$75AE (y) / -$75B6 (x); depth-0 y=22 lands 1px high in composites → 23.
FRONT_Y_TABLE = [22, 40, 54, 62]
FRONT_X_TABLE = [32, 64, 88, 104]
FRONT_SCREEN_Y = [y + (1 if i == 0 else 0) for i, y in enumerate(FRONT_Y_TABLE)]

# --- Outdoor horizon blit (@0x1877A → @0x1844C / @0x184B8 / @0x18620) ---
# Map page-0 rows → -$55C6 / -$55C2 / -$55BE (via @0x9544 + @0x9524 terrain lookup).
# @0x1877A copies those into -$5ADA / -$5AE2 / -$5ADE; each indexes -$7A16[sheet].
# **Any outdoor1/2/3 sheet can land in any lane** — map data picks sheet index;
# the three passes only supply different x/y/frame tables (screen lanes + depth).
#
# Lane 1 pass @1844C (-$5ADA): 6BC2/6BCA/6BCE
OUTDOOR_L1_Y = [21, 21, 42, 50]
OUTDOOR_L1_X = [40, 40, 64, 88]
OUTDOOR_L1_FRAME = [0, 0, 1, 2]  # -$6BCE

# Lane 2 pass @184B8 (-$5AE2): 6BAA/6BB2/6BBA
OUTDOOR_L2_Y = [36, 46, 50, 58]
OUTDOOR_L2_X = [8, 16, 32, 88]
OUTDOOR_L2_FRAME = [4, 5, 2, 3]

# Lane 3 pass @18620 (-$5ADE): 6B96/6B9E/6BA2
OUTDOOR_L3_Y = [36, 46, 50, 58]
OUTDOOR_L3_X = [176, 152, 136, 120]
OUTDOOR_L3_FRAME = [6, 7, 2, 3]

OUTDOOR_LANE_TABLES: list[tuple[str, list[int], list[int], list[int]]] = [
    ("L1", OUTDOOR_L1_FRAME, OUTDOOR_L1_X, OUTDOOR_L1_Y),
    ("L2", OUTDOOR_L2_FRAME, OUTDOOR_L2_X, OUTDOOR_L2_Y),
    ("L3", OUTDOOR_L3_FRAME, OUTDOOR_L3_X, OUTDOOR_L3_Y),
]

# Floor decor @182D8 — sheet -$7A0A (desert/ocean/tundra/swamp), NOT outdoor1/2/3.
# y = $80 - 6BD6[col] → [108,93,78,68]; painted before horizon layers @1877A.
OUTDOOR_DECOR_Y = [0x80 - v for v in (20, 35, 50, 60)]  # A4-$6BD6
OUTDOOR_DECOR_X = 8
OUTDOOR_DECOR_X_112 = 0x70  # hardcoded @183EC branch
OUTDOOR_DECOR_X_BDE = [184, 160, 136, 112]  # A4-$6BDE (x table, branch C)
OUTDOOR_FLOOR_SHEET = "desert.32"  # default biome loaded into -$7A0A
OUTDOOR_FLOOR_SHEETS = ["desert.32", "ocean.32", "tundra.32", "swamp.32"]
LEFT_NEAR_X = [8, 32, 64, 88]
LEFT_NEAR_Y = [8, 22, 40, 54]
LEFT_FAR_X = [8, 8, 40, 88]
LEFT_FAR_Y = [22, 40, 54, 62]
RIGHT_NEAR_X = [192, 160, 136, 120]
RIGHT_NEAR_Y = [8, 22, 40, 54]
RIGHT_FAR_X = [192, 160, 136, 120]
RIGHT_FAR_Y = [22, 40, 54, 62]
LEFT_FAR_FRAME = [12, 14, 2, 3]
RIGHT_FAR_FRAME = [13, 15, 2, 3]
DOOR_FRONT = [4, 5, 6, 7]

# Primary viewport slots: frames 0..15 plus far d2/d3 (reuse front gfx 2/3).
INDOOR_FRAME_VIEWS: list[tuple[int, int, int, str]] = [
    # frame, x, y, caption
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
    # d2/d3 reuse front frames 2/3 at far-wall coords (-$75A6/-$7582 @ col 2..3)
    (LEFT_FAR_FRAME[2], LEFT_FAR_X[2], LEFT_FAR_Y[2], "left far d2"),
    (RIGHT_FAR_FRAME[2], RIGHT_FAR_X[2], RIGHT_FAR_Y[2], "right far d2"),
    (LEFT_FAR_FRAME[3], LEFT_FAR_X[3], LEFT_FAR_Y[3], "left far d3"),
    (RIGHT_FAR_FRAME[3], RIGHT_FAR_X[3], RIGHT_FAR_Y[3], "right far d3"),
]

# Sixteen corridor scenes — multi-wall views at increasing depth.
INDOOR_SCENE_VIEWS: list[tuple[str, list[tuple[int, int, int]]]] = [
    ("open", []),
    ("front d0", [(0, FRONT_X_TABLE[0], FRONT_SCREEN_Y[0])]),
    ("front d1", [(1, FRONT_X_TABLE[1], FRONT_SCREEN_Y[1])]),
    ("front d2", [(2, FRONT_X_TABLE[2], FRONT_SCREEN_Y[2])]),
    ("front d3", [(3, FRONT_X_TABLE[3], FRONT_SCREEN_Y[3])]),
    ("left near d0", [(4, LEFT_NEAR_X[0], LEFT_NEAR_Y[0])]),
    ("left near d1", [(5, LEFT_NEAR_X[1], LEFT_NEAR_Y[1])]),
    ("right near d0", [(8, RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0])]),
    ("right near d1", [(9, RIGHT_NEAR_X[1], RIGHT_NEAR_Y[1])]),
    ("left far d0", [(12, LEFT_FAR_X[0], LEFT_FAR_Y[0])]),
    ("right far d0", [(13, RIGHT_FAR_X[0], RIGHT_FAR_Y[0])]),
    ("left far d1", [(14, LEFT_FAR_X[1], LEFT_FAR_Y[1])]),
    ("right far d1", [(15, RIGHT_FAR_X[1], RIGHT_FAR_Y[1])]),
    ("left far d2", [(LEFT_FAR_FRAME[2], LEFT_FAR_X[2], LEFT_FAR_Y[2])]),
    ("right far d2", [(RIGHT_FAR_FRAME[2], RIGHT_FAR_X[2], RIGHT_FAR_Y[2])]),
    ("left far d3", [(LEFT_FAR_FRAME[3], LEFT_FAR_X[3], LEFT_FAR_Y[3])]),
    ("right far d3", [(RIGHT_FAR_FRAME[3], RIGHT_FAR_X[3], RIGHT_FAR_Y[3])]),
    (
        "alcove d0",
        [
            (4, LEFT_NEAR_X[0], LEFT_NEAR_Y[0]),
            (8, RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0]),
            (0, FRONT_X_TABLE[0], FRONT_SCREEN_Y[0]),
        ],
    ),
    (
        "corridor d1",
        [
            (5, LEFT_NEAR_X[1], LEFT_NEAR_Y[1]),
            (9, RIGHT_NEAR_X[1], RIGHT_NEAR_Y[1]),
            (1, FRONT_X_TABLE[1], FRONT_SCREEN_Y[1]),
        ],
    ),
    (
        "corridor d2",
        [
            (6, LEFT_NEAR_X[2], LEFT_NEAR_Y[2]),
            (10, RIGHT_NEAR_X[2], RIGHT_NEAR_Y[2]),
            (2, FRONT_X_TABLE[2], FRONT_SCREEN_Y[2]),
        ],
    ),
    (
        "deep d3",
        [
            (7, LEFT_NEAR_X[3], LEFT_NEAR_Y[3]),
            (11, RIGHT_NEAR_X[3], RIGHT_NEAR_Y[3]),
            (LEFT_FAR_FRAME[3], LEFT_FAR_X[3], LEFT_FAR_Y[3]),
            (3, FRONT_X_TABLE[3], FRONT_SCREEN_Y[3]),
        ],
    ),
    (
        "far corners d0-d1",
        [
            (12, LEFT_FAR_X[0], LEFT_FAR_Y[0]),
            (13, RIGHT_FAR_X[0], RIGHT_FAR_Y[0]),
            (14, LEFT_FAR_X[1], LEFT_FAR_Y[1]),
            (15, RIGHT_FAR_X[1], RIGHT_FAR_Y[1]),
        ],
    ),
    (
        "far corners d2-d3",
        [
            (LEFT_FAR_FRAME[2], LEFT_FAR_X[2], LEFT_FAR_Y[2]),
            (RIGHT_FAR_FRAME[2], RIGHT_FAR_X[2], RIGHT_FAR_Y[2]),
            (LEFT_FAR_FRAME[3], LEFT_FAR_X[3], LEFT_FAR_Y[3]),
            (RIGHT_FAR_FRAME[3], RIGHT_FAR_X[3], RIGHT_FAR_Y[3]),
        ],
    ),
]

def _build_outdoor_sheet_frame_slots() -> list[tuple[str, int, int, int]]:
    """Every sheet uses the same lane×column slots — map picks sheet, not filename."""
    slots: list[tuple[str, int, int, int]] = []
    for prefix, frames, xs, ys in OUTDOOR_LANE_TABLES:
        for c in range(4):
            slots.append((f"{prefix} c{c} f{frames[c]}", frames[c], xs[c], ys[c]))
    while len(slots) < 16:
        slots.append(("open", 0, 0, 0))
    return slots[:16]


_OUTDOOR_FRAME_SLOTS = _build_outdoor_sheet_frame_slots()

OUTDOOR_SHEET_SLOTS: dict[str, list[tuple[str, int, int, int]]] = {
    "outdoor1.32": _OUTDOOR_FRAME_SLOTS,
    "outdoor2.32": _OUTDOOR_FRAME_SLOTS,
    "outdoor3.32": _OUTDOOR_FRAME_SLOTS,
}


def _lane_sprite_layers(sheet: str) -> list[tuple[str, int, int, int]]:
    layers: list[tuple[str, int, int, int]] = []
    for _prefix, frames, xs, ys in OUTDOOR_LANE_TABLES:
        for c in range(4):
            layers.append((sheet, frames[c], xs[c], ys[c]))
    return layers


def _lane_layers(prefix: str) -> list[tuple[int, int, int]]:
    for name, frames, xs, ys in OUTDOOR_LANE_TABLES:
        if name == prefix:
            return [(frames[c], xs[c], ys[c]) for c in range(4)]
    return []


def _l1_layers() -> list[tuple[int, int, int]]:
    return _lane_layers("L1")


def _l2_layers() -> list[tuple[int, int, int]]:
    return _lane_layers("L2")


def _l3_layers() -> list[tuple[int, int, int]]:
    return _lane_layers("L3")


def outdoor_combined_frame_grid_views(sheet: str = "outdoor1.32") -> list[tuple[str, list[tuple[str, int, int, int]]]]:
    """One sheet at all three lane tables — col N stacks L1+L2+L3 (full viewport)."""
    views: list[tuple[str, list[tuple[str, int, int, int]]]] = []
    for c in range(4):
        layers: list[tuple[str, int, int, int]] = []
        for prefix, frames, xs, ys in OUTDOOR_LANE_TABLES:
            layers.append((sheet, frames[c], xs[c], ys[c]))
        views.append((f"col{c} L1+L2+L3", layers))
    views.append(("L1 all", [(sheet, f, x, y) for f, x, y in _l1_layers()]))
    views.append(("L2 all", [(sheet, f, x, y) for f, x, y in _l2_layers()]))
    views.append(("L3 all", [(sheet, f, x, y) for f, x, y in _l3_layers()]))
    views.append(("all lanes", _lane_sprite_layers(sheet)))
    while len(views) < 16:
        views.append(("open", []))
    return views[:16]


def outdoor_frame_grid_views(sheet: str) -> list[tuple[str, list[tuple[int, int, int]]]]:
    """16 mini-views: each frame at its real ASM slot for this sheet."""
    views: list[tuple[str, list[tuple[int, int, int]]]] = []
    for cap, frame, x, y in OUTDOOR_SHEET_SLOTS[sheet]:
        if cap == "open":
            views.append((cap, []))
        else:
            views.append((cap, [(frame, x, y)]))
    return views[:16]


def outdoor_scene_grid_views(sheet: str) -> list[tuple[str, list[tuple[int, int, int]]]]:
    """Per-lane scene composites for one art sheet (same coords for all sheets)."""
    l1 = _l1_layers()
    l2 = _l2_layers()
    l3 = _l3_layers()
    return [
        ("open", []),
        ("L1 col0", [l1[0]]),
        ("L1 col1", [l1[1]]),
        ("L1 col2", [l1[2]]),
        ("L1 col3", [l1[3]]),
        ("L1 all", l1),
        ("L2 all", l2),
        ("L3 all", l3),
        ("col0 lanes", [l1[0], l2[0], l3[0]]),
        ("col1 lanes", [l1[1], l2[1], l3[1]]),
        ("col2 lanes", [l1[2], l2[2], l3[2]]),
        ("col3 lanes", [l1[3], l2[3], l3[3]]),
        ("L1+L2", l1 + l2),
        ("L1+L2+L3", l1 + l2 + l3),
        ("L2 c0", [l2[0]]),
        ("L3 c0", [l3[0]]),
    ]


def _decor_branch_a() -> list[tuple[str, int, int, int]]:
    return [
        (OUTDOOR_FLOOR_SHEET, col, OUTDOOR_DECOR_X, OUTDOOR_DECOR_Y[col])
        for col in range(4)
    ]


def _decor_branch_b() -> list[tuple[str, int, int, int]]:
    """-$55C2 alt @183C6: frames col+4 → desert f4-7 (left wedge sprites)."""
    return [
        (OUTDOOR_FLOOR_SHEET, col + 4, OUTDOOR_DECOR_X, OUTDOOR_DECOR_Y[col])
        for col in range(4)
    ]


def _decor_branch_b_main() -> list[tuple[str, int, int, int]]:
    """-$55C2 main @18370 (55C6 set): frames col+12 (desert f12-15)."""
    return [
        (OUTDOOR_FLOOR_SHEET, col + 12, OUTDOOR_DECOR_X, OUTDOOR_DECOR_Y[col])
        for col in range(4)
    ]


def _decor_branch_c() -> list[tuple[str, int, int, int]]:
    """-$55BE alt @183E8: frames col+8 → desert f8-11 (right wedge sprites)."""
    return [
        (OUTDOOR_FLOOR_SHEET, col + 8, OUTDOOR_DECOR_X_112, OUTDOOR_DECOR_Y[col])
        for col in range(4)
    ]


def _decor_branch_c_main() -> list[tuple[str, int, int, int]]:
    """-$55BE main @18394 (55C6 set): frames col+16 (f16-19) at 6BDE x."""
    return [
        (
            OUTDOOR_FLOOR_SHEET,
            col + 16,
            OUTDOOR_DECOR_X_BDE[col] - ORIGIN_X,
            OUTDOOR_DECOR_Y[col],
        )
        for col in range(4)
    ]


def _decor_branch_d() -> list[tuple[str, int, int, int]]:
    """Alias for _decor_branch_c (same @183E8 blit)."""
    return _decor_branch_c()


def _l1_sprite_layers(sheet: str = "outdoor1.32") -> list[tuple[str, int, int, int]]:
    return [(sheet, OUTDOOR_L1_FRAME[c], OUTDOOR_L1_X[c], OUTDOOR_L1_Y[c]) for c in range(4)]


def _l2_sprite_layers(sheet: str = "outdoor1.32") -> list[tuple[str, int, int, int]]:
    return [(sheet, OUTDOOR_L2_FRAME[c], OUTDOOR_L2_X[c], OUTDOOR_L2_Y[c]) for c in range(4)]


def _l3_sprite_layers(sheet: str = "outdoor1.32") -> list[tuple[str, int, int, int]]:
    return [(sheet, OUTDOOR_L3_FRAME[c], OUTDOOR_L3_X[c], OUTDOOR_L3_Y[c]) for c in range(4)]


def outdoor_mountain_grid_views() -> list[tuple[str, list[tuple[str, int, int, int]]]]:
    """One art sheet at every lane slot (runtime picks sheet from map)."""
    sheet = "outdoor1.32"
    p1 = _l1_sprite_layers(sheet)
    p2 = _l2_sprite_layers(sheet)
    p3 = _l3_sprite_layers(sheet)
    return [
        ("open", []),
        ("L1 c0", [p1[0]]),
        ("L1 c1", [p1[1]]),
        ("L1 c2", [p1[2]]),
        ("L1 c3", [p1[3]]),
        ("L2 c0", [p2[0]]),
        ("L2 c1", [p2[1]]),
        ("L2 c2", [p2[2]]),
        ("L2 c3", [p2[3]]),
        ("L3 c0", [p3[0]]),
        ("L3 c1", [p3[1]]),
        ("L3 c2", [p3[2]]),
        ("L3 c3", [p3[3]]),
        ("col0 lanes", [p1[0], p2[0], p3[0]]),
        ("col2 lanes", [p1[2], p2[2], p3[2]]),
        ("all lanes", p1 + p2 + p3),
    ]


def outdoor_floor_grid_views() -> list[tuple[str, list[tuple[str, int, int, int]]]]:
    """Floor / horizon-strip decor on -$7A0A (desert.32 etc.), not outdoor1 mountains."""
    da = _decor_branch_a()
    db = _decor_branch_b()
    dc = _decor_branch_c()
    dcm = _decor_branch_c_main()
    return [
        ("outf only", []),
        ("decor A c0", [da[0]]),
        ("decor A c1", [da[1]]),
        ("decor A c2", [da[2]]),
        ("decor A c3", [da[3]]),
        ("decor A all", da),
        ("decor B c0", [db[0]]),
        ("decor B c1", [db[1]]),
        ("decor B c2", [db[2]]),
        ("decor B c3", [db[3]]),
        ("decor B all", db),
        ("decor C c0", [dc[0]]),
        ("decor C c1", [dc[1]]),
        ("decor C c2", [dc[2]]),
        ("decor C c3", [dc[3]]),
        ("decor C all", dc),
        ("C main c0", [dcm[0]]),
        ("C main c3", [dcm[3]]),
        ("C main all", dcm),
        ("A+B all", da + db),
        ("A+B+C", da + db + dc),
    ]


INDOOR_WALLSETS: dict[str, tuple[str, str]] = {
    "town.32": ("townf.32", "sky.32"),
    "cave.32": ("cavef.32", "sky.32"),
    "castle.32": ("castlef.32", "sky.32"),
}


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


def amiga_frame_mask(sheet: str, frame: int, data_dir: Path) -> list[list[bool]]:
    """Pen-0 transparency mask for one ``.32`` frame (True = transparent)."""
    b = (data_dir / sheet).read_bytes()
    n = read_u16be(b, 0)
    info = 4
    w = read_u16be(b, info + frame * 6)
    h = read_u16be(b, info + frame * 6 + 2)
    cur = info + n * 6 + 64
    bpr = ((w + 15) >> 3) & 0xFFFE
    rs = h * bpr
    for f in range(frame):
        fw = read_u16be(b, info + f * 6)
        fh = read_u16be(b, info + f * 6 + 2)
        rs2 = fh * (((fw + 15) >> 3) & 0xFFFE)
        cur, _ = decode_planes(b, cur, 5 * rs2)
    _, planes = decode_planes(b, cur, 5 * rs)
    mask: list[list[bool]] = []
    for y in range(h):
        row: list[bool] = []
        for x in range(w):
            idx = 0
            for pl in range(5):
                bp = pl * rs + y * bpr + (x >> 3)
                bit = (planes[bp] >> (7 - (x & 7))) & 1
                idx |= bit << pl
            row.append(idx == 0)
        mask.append(row)
    return mask


def load_frame(sheet: str, frame: int, data_dir: Path) -> Image.Image:
    """Decode one .32 frame; only pen index 0 is transparent (blit mask)."""
    b = (data_dir / sheet).read_bytes()
    n = read_u16be(b, 0)
    info = 4
    w = read_u16be(b, info + frame * 6)
    h = read_u16be(b, info + frame * 6 + 2)
    pal_off = info + n * 6
    palette = []
    for i in range(32):
        pw = read_u16be(b, pal_off + i * 2)
        palette.append((((pw >> 8) & 0xF) * 17, ((pw >> 4) & 0xF) * 17, (pw & 0xF) * 17))
    cur = pal_off + 64
    bpr = ((w + 15) >> 3) & 0xFFFE
    rs = h * bpr
    for f in range(frame):
        fw = read_u16be(b, info + f * 6)
        fh = read_u16be(b, info + f * 6 + 2)
        rs2 = fh * (((fw + 15) >> 3) & 0xFFFE)
        cur, _ = decode_planes(b, cur, 5 * rs2)
    _, planes = decode_planes(b, cur, 5 * rs)
    im = Image.new("RGBA", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            idx = 0
            for pl in range(5):
                bp = pl * rs + y * bpr + (x >> 3)
                bit = (planes[bp] >> (7 - (x & 7))) & 1
                idx |= bit << pl
            r, g, bl = palette[idx]
            px[x, y] = (r, g, bl, 0 if idx == 0 else 255)
    return im


def read_u16be(b: bytes, off: int) -> int:
    return (b[off] << 8) | b[off + 1]


def load_ppm(path: Path) -> Image.Image:
    """Legacy PPM loader — prefer load_frame() which masks pen 0 only."""
    with open(path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6":
            raise ValueError(path)
        while True:
            line = f.readline()
            if not line.startswith(b"#"):
                break
        w, h = map(int, line.split())
        maxv = int(f.readline().strip())
        assert maxv == 255
        raw = f.read(w * h * 3)
    return Image.frombytes("RGB", (w, h), raw).convert("RGBA")


def blit(canvas: Image.Image, sprite: Image.Image, x: int, y: int) -> None:
    canvas.paste(sprite, (x, y), sprite)


def frame_path(sheet: str, idx: int) -> Path:
    return FRAMES / f"{sheet}_f{idx:02d}.ppm"


def load_sprite(sheet: str, frame: int, data_dir: Path) -> Image.Image:
    return load_frame(sheet, frame, data_dir)


def dump_frames(data_dir: Path, dumper: Path) -> None:
    FRAMES.mkdir(parents=True, exist_ok=True)
    sheets = [
        "sky.32", "townf.32", "outf.32", "town.32",
        "outdoor1.32", "outdoor2.32", "outdoor3.32",
        "desert.32", "ocean.32", "tundra.32", "swamp.32",
    ]
    for name in sheets:
        src = data_dir / name
        if not src.exists():
            print(f"skip missing {src}")
            continue
        subprocess.run([str(dumper), str(src), str(FRAMES)], check=True)


def indoor_wall_layers() -> list[tuple[int, int, int]]:
    """All indoor wall frames at their viewport slots, far → near paint order."""
    layers: list[tuple[int, int, int]] = []
    for d in range(3, -1, -1):
        layers.append((LEFT_FAR_FRAME[d], LEFT_FAR_X[d], LEFT_FAR_Y[d]))
        layers.append((RIGHT_FAR_FRAME[d], RIGHT_FAR_X[d], RIGHT_FAR_Y[d]))
        layers.append((d, FRONT_X_TABLE[d], FRONT_SCREEN_Y[d]))
        layers.append((d + 4, LEFT_NEAR_X[d], LEFT_NEAR_Y[d]))
        layers.append((d + 8, RIGHT_NEAR_X[d], RIGHT_NEAR_Y[d]))
    return layers


def composite_backdrop(floor_sheet: str, sky_sheet: str, data_dir: Path) -> Image.Image:
    canvas = Image.new("RGBA", (VIEW_W, VIEW_H), (0, 0, 0, 255))
    blit(canvas, load_sprite(floor_sheet, 0, data_dir), ORIGIN_X, FLOOR_Y)
    blit(canvas, load_sprite(sky_sheet, 0, data_dir), ORIGIN_X, SKY_Y)
    return canvas


def composite_viewport(
    wall_sheet: str,
    data_dir: Path,
    layers: list[tuple[int, int, int]],
    *,
    floor_sheet: str,
    sky_sheet: str,
) -> Image.Image:
    canvas = composite_backdrop(floor_sheet, sky_sheet, data_dir)
    for frame, x, y in layers:
        try:
            blit(canvas, load_sprite(wall_sheet, frame, data_dir), x, y)
        except Exception:
            pass
    return canvas


def composite_wallset_layout(
    wall_sheet: str,
    data_dir: Path,
    *,
    floor_sheet: str = "townf.32",
    sky_sheet: str = "sky.32",
    layers: list[tuple[int, int, int]] | None = None,
) -> Image.Image:
    """Legacy single-canvas stack of all layers (kept for debugging)."""
    if layers is None:
        layers = indoor_wall_layers()
    return composite_viewport(wall_sheet, data_dir, layers, floor_sheet=floor_sheet, sky_sheet=sky_sheet)


def composite_view_grid(
    wall_sheet: str,
    data_dir: Path,
    views: list[tuple[str, list[tuple[int, int, int]]]],
    *,
    floor_sheet: str,
    sky_sheet: str,
    cols: int = 4,
    pad: int = 6,
    draw_labels: bool = True,
) -> Image.Image:
    """4×4 (or cols×rows) grid of separate mini-viewports."""
    try:
        from PIL import ImageDraw, ImageFont
    except ImportError:
        ImageDraw = None  # type: ignore
        ImageFont = None  # type: ignore

    cells: list[Image.Image] = []
    for _caption, layers in views:
        cells.append(
            composite_viewport(
                wall_sheet, data_dir, layers, floor_sheet=floor_sheet, sky_sheet=sky_sheet
            )
        )

    rows = (len(cells) + cols - 1) // cols
    label_h = 14 if draw_labels else 0
    gw = cols * VIEW_W + (cols + 1) * pad
    gh = rows * (VIEW_H + label_h) + (rows + 1) * pad
    grid = Image.new("RGBA", (gw, gh), (14, 14, 22, 255))

    font = None
    if draw_labels and ImageFont is not None:
        try:
            font = ImageFont.truetype("arial.ttf", 11)
        except OSError:
            font = ImageFont.load_default()

    draw = ImageDraw.Draw(grid) if draw_labels and ImageDraw is not None else None
    for i, cell in enumerate(cells):
        cx, cy = i % cols, i // cols
        ox = pad + cx * (VIEW_W + pad)
        oy = pad + cy * (VIEW_H + label_h + pad)
        if draw and font and i < len(views):
            draw.text((ox + 2, oy), views[i][0], fill=(180, 180, 200, 255), font=font)
        grid.paste(cell, (ox, oy + label_h), cell)
    return grid


def indoor_frame_grid_views() -> list[tuple[str, list[tuple[int, int, int]]]]:
    return [(cap, [(fr, x, y)]) for fr, x, y, cap in INDOOR_FRAME_VIEWS]


# Viewport lattice: lateral x = -2..+2, row = -3..0 (depth d3..d0).
# x=-2 far L, x=-1 near L, x=0 front, x=+1 near R, x=+2 far R (ASM wall tables).
INDOOR_COORD_COLS = (-2, -1, 0, 1, 2)
INDOOR_COORD_ROLES = ("far L", "near L", "front", "near R", "far R")


def indoor_coord_layers(col: int, row: int) -> list[tuple[int, int, int]]:
    """Map (x, row) to wall blit(s); empty list = open backdrop only."""
    d = -row  # depth 0..3
    if d < 0 or d >= 4:
        return []
    if col == -2:
        return [(LEFT_FAR_FRAME[d], LEFT_FAR_X[d], LEFT_FAR_Y[d])]
    if col == -1:
        return [(d + 4, LEFT_NEAR_X[d], LEFT_NEAR_Y[d])]
    if col == 0:
        return [(d, FRONT_X_TABLE[d], FRONT_SCREEN_Y[d])]
    if col == 1:
        return [(d + 8, RIGHT_NEAR_X[d], RIGHT_NEAR_Y[d])]
    if col == 2:
        return [(RIGHT_FAR_FRAME[d], RIGHT_FAR_X[d], RIGHT_FAR_Y[d])]
    return []


def indoor_coord_grid_views() -> list[tuple[str, list[tuple[int, int, int]]]]:
    views: list[tuple[str, list[tuple[int, int, int]]]] = []
    for row in range(-3, 1):
        for col, role in zip(INDOOR_COORD_COLS, INDOOR_COORD_ROLES):
            cap = f"({col},{row}) {role}"
            views.append((cap, indoor_coord_layers(col, row)))
    return views


def composite_indoor(walls: str = "town.32", data_dir: Path = ROOT) -> Image.Image:
    """Dead-end corridor: one front wall + near left/right (View3D depth 0)."""
    canvas = composite_backdrop("townf.32", "sky.32", data_dir)
    layers = [
        (8, RIGHT_NEAR_X[0], RIGHT_NEAR_Y[0]),
        (4, LEFT_NEAR_X[0], LEFT_NEAR_Y[0]),
        (0, FRONT_X_TABLE[0], FRONT_SCREEN_Y[0]),
    ]
    for fr, x, y in layers:
        blit(canvas, load_sprite(walls, fr, data_dir), x, y)
    return canvas


def composite_outdoor_sprites(
    data_dir: Path,
    layers: list[tuple[str, int, int, int]],
    *,
    floor_sheet: str = "outf.32",
    sky_sheet: str = "sky.32",
) -> Image.Image:
    """Paint outdoor sprite layers (sheet, frame, x, y) over outf+sky."""
    canvas = composite_backdrop(floor_sheet, sky_sheet, data_dir)
    for sheet, frame, x, y in layers:
        try:
            blit(canvas, load_sprite(sheet, frame, data_dir), x, y)
        except Exception:
            pass
    return canvas


def composite_outdoor_floor_grid(
    data_dir: Path,
    views: list[tuple[str, list[tuple[str, int, int, int]]]],
    *,
    cols: int = 4,
    pad: int = 6,
) -> Image.Image:
    """Grid of floor/decor mini-views (-$7A0A biome strips)."""
    try:
        from PIL import ImageDraw, ImageFont
    except ImportError:
        ImageDraw = None  # type: ignore
        ImageFont = None  # type: ignore

    cells = [composite_outdoor_sprites(data_dir, layers) for _cap, layers in views]
    rows = (len(cells) + cols - 1) // cols
    label_h = 14
    gw = cols * VIEW_W + (cols + 1) * pad
    gh = rows * (VIEW_H + label_h) + (rows + 1) * pad
    grid = Image.new("RGBA", (gw, gh), (14, 14, 22, 255))
    font = None
    if ImageFont is not None:
        try:
            font = ImageFont.truetype("arial.ttf", 11)
        except OSError:
            font = ImageFont.load_default()
    draw = ImageDraw.Draw(grid) if ImageDraw is not None else None
    for i, cell in enumerate(cells):
        cx, cy = i % cols, i // cols
        ox = pad + cx * (VIEW_W + pad)
        oy = pad + cy * (VIEW_H + label_h + pad)
        if draw and font and i < len(views):
            draw.text((ox + 2, oy), views[i][0], fill=(180, 180, 200, 255), font=font)
        grid.paste(cell, (ox, oy + label_h), cell)
    return grid


def composite_outdoor_mountain_grid(
    data_dir: Path,
    views: list[tuple[str, list[tuple[str, int, int, int]]]],
    *,
    cols: int = 4,
    pad: int = 6,
) -> Image.Image:
    """Grid of horizon mountain/tree layers (outdoor1/2/3 @ 6BC2/6BCA)."""
    return composite_outdoor_floor_grid(data_dir, views, cols=cols, pad=pad)


def composite_outdoor(terrain_frame: int = 0, layer: int = 0, data_dir: Path = ROOT) -> Image.Image:
    """Outdoor first-person: outf floor + sky + one horizon layer from outdoor1/2/3.

    ASM @0x18870: outf + sky, then @0x182D8 floor decor (-$7A0A), then @0x1877A
    horizon layers (-$7A16 → outdoor1/2/3). This helper shows wall layers only.
    """
    canvas = composite_backdrop("outf.32", "sky.32", data_dir)

    sheet = f"outdoor{layer + 1}.32"
    if layer == 0:
        frame = OUTDOOR_L1_FRAME[terrain_frame]
        x = OUTDOOR_L1_X[terrain_frame]
        y = OUTDOOR_L1_Y[terrain_frame]
    elif layer == 1:
        frame = OUTDOOR_L2_FRAME[terrain_frame]
        x = OUTDOOR_L2_X[terrain_frame]
        y = OUTDOOR_L2_Y[terrain_frame]
    else:
        frame = OUTDOOR_L3_FRAME[terrain_frame]
        x = OUTDOOR_L3_X[terrain_frame]
        y = OUTDOOR_L3_Y[terrain_frame]
    blit(canvas, load_sprite(sheet, frame, data_dir), x, y)
    return canvas


def composite_sheet_montage(sheet: str, max_frames: int = 16, data_dir: Path = ROOT) -> Image.Image:
    """Thumbnail grid of all frames in a wall set."""
    imgs: list[Image.Image] = []
    i = 0
    while i < max_frames:
        try:
            imgs.append(load_sprite(sheet, i, data_dir))
        except Exception:
            break
        i += 1
    if not imgs:
        return Image.new("RGBA", (64, 64), (64, 0, 0, 255))
    cols = 4
    rows = (len(imgs) + cols - 1) // cols
    cell_w = max(im.width for im in imgs) + 4
    cell_h = max(im.height for im in imgs) + 4
    out = Image.new("RGBA", (cols * cell_w, rows * cell_h), (20, 20, 30, 255))
    for n, im in enumerate(imgs):
        cx, cy = n % cols, n // cols
        out.paste(im, (cx * cell_w + 2, cy * cell_h + 2), im)
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, default=ROOT)
    ap.add_argument("--dump", action="store_true")
    args = ap.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    dumper = ROOT / "tools" / "dump_gfx_frame.exe"
    if args.dump or not FRAMES.exists():
        if not dumper.exists():
            subprocess.run(
                [
                    "g++", "-std=c++17", f"-I{ROOT}/editor/src",
                    str(ROOT / "tools" / "dump_gfx_frame.cpp"),
                    str(ROOT / "editor/src/core/Gfx.cpp"),
                    str(ROOT / "editor/src/core/ByteIO.cpp"),
                    "-o", str(dumper),
                ],
                check=True,
            )
        dump_frames(args.data, dumper)

    composite_indoor(data_dir=args.data).save(OUT / "ref-indoor-corridor.png")
    composite_outdoor(0, 0, data_dir=args.data).save(OUT / "ref-outdoor-terrain0.png")
    composite_outdoor(1, 0, data_dir=args.data).save(OUT / "ref-outdoor-terrain1.png")

    # Backdrop-only (what picture 1 should show)
    composite_backdrop("townf.32", "sky.32", args.data).save(OUT / "ref-backdrop-indoor.png")
    composite_backdrop("outf.32", "sky.32", args.data).save(OUT / "ref-backdrop-outdoor.png")

    composite_sheet_montage("town.32", 16, args.data).save(OUT / "ref-town32-frames.png")
    composite_sheet_montage("outdoor1.32", 8, args.data).save(OUT / "ref-outdoor1-frames.png")

    composite_outdoor_floor_grid(
        args.data, outdoor_floor_grid_views()
    ).save(OUT / "ref-outdoor-floors.png")
    composite_outdoor_mountain_grid(
        args.data, outdoor_mountain_grid_views()
    ).save(OUT / "ref-outdoor-mountains.png")
    composite_outdoor_mountain_grid(
        args.data, outdoor_combined_frame_grid_views("outdoor1.32")
    ).save(OUT / "ref-layout-outdoor-combined-frames.png")

    # Wall-set reference grids: 16 separate mini-views per sheet
    for wall_sheet, (floor_sheet, sky_sheet) in INDOOR_WALLSETS.items():
        if not (args.data / wall_sheet).exists():
            continue
        stem = wall_sheet.replace(".32", "")
        composite_view_grid(
            wall_sheet,
            args.data,
            indoor_frame_grid_views(),
            floor_sheet=floor_sheet,
            sky_sheet=sky_sheet,
        ).save(OUT / f"ref-layout-{stem}-frames.png")
        composite_view_grid(
            wall_sheet,
            args.data,
            INDOOR_SCENE_VIEWS,
            floor_sheet=floor_sheet,
            sky_sheet=sky_sheet,
        ).save(OUT / f"ref-layout-{stem}-scenes.png")
        composite_view_grid(
            wall_sheet,
            args.data,
            indoor_coord_grid_views(),
            floor_sheet=floor_sheet,
            sky_sheet=sky_sheet,
            cols=5,
        ).save(OUT / f"ref-layout-{stem}-coords.png")

    for wall_sheet in OUTDOOR_SHEET_SLOTS:
        if not (args.data / wall_sheet).exists():
            continue
        stem = wall_sheet.replace(".32", "")
        composite_view_grid(
            wall_sheet,
            args.data,
            outdoor_frame_grid_views(wall_sheet),
            floor_sheet="outf.32",
            sky_sheet="sky.32",
        ).save(OUT / f"ref-layout-{stem}-frames.png")
        composite_view_grid(
            wall_sheet,
            args.data,
            outdoor_scene_grid_views(wall_sheet),
            floor_sheet="outf.32",
            sky_sheet="sky.32",
        ).save(OUT / f"ref-layout-{stem}-scenes.png")

    # Side-by-side: same viewport layout, different art + renderer path (NOT an infographic)
    pad = 8
    a = composite_indoor(data_dir=args.data)
    b = composite_outdoor(0, 0, data_dir=args.data)
    label_h = 0
    combined = Image.new(
        "RGBA",
        (a.width * 2 + pad * 3, a.height + label_h + pad * 2),
        (16, 16, 24, 255),
    )
    combined.paste(a, (pad, pad))
    combined.paste(b, (pad * 2 + a.width, pad))
    combined.save(OUT / "ref-indoor-vs-outdoor.png")

    print(f"wrote references to {OUT}")


if __name__ == "__main__":
    main()
