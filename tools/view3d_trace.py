#!/usr/bin/env python3
"""ASM-faithful 3D frustum trace (ports editor/src/core/View3D.cpp).

Usage:
  python tools/view3d_trace.py map.dat <screen> <x> <y> <facing>
  python tools/view3d_trace.py map.dat --matrix          # known test cases
  python tools/view3d_trace.py --dump-bundles            # DATA hunk bundles
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, List, Sequence, Tuple

MAP_SCREENS = 60
MAP_SCREEN_SIZE = 512
MAP_PAGE_SIZE = 256
MAP_GRID = 16
DEPTH_BANDS = 4
FRUSTUM_SLOTS = 20

A4 = 0x7FFE
DATA_BIN = Path(__file__).resolve().parents[1] / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"

# Direction bundles @ A4-$7646 (N), -$7640 (S), -$763A (E), -$7634 (W)
BUNDLE_N = (0, 1, 255, 0, 1, 0)
BUNDLE_S = (0, 255, 1, 0, 255, 0)
BUNDLE_E = (1, 0, 0, 1, 0, 255)
BUNDLE_W = (255, 0, 0, 255, 0, 1)

SLOT_NAMES = (
    "F20", "F1F", "F1E", "F1D", "F1C", "F1B", "F1A", "F19",
    "F18", "F17", "F16", "F15", "F14", "F13", "F12", "F11",
    "F10", "F0F", "F0E", "F0D",
)

S_F20, S_F1F, S_F1E, S_F1D, S_F1C = 0, 1, 2, 3, 4
S_F1B, S_F1A, S_F19 = 5, 6, 7
S_F18, S_F17, S_F16, S_F15 = 8, 9, 10, 11
S_F14, S_F13, S_F12, S_F11 = 12, 13, 14, 15
S_F10, S_F0F, S_F0E, S_F0D = 16, 17, 18, 19

FACE = ("N", "E", "S", "W")
DIR_MASK = (0xC0, 0x30, 0x0C, 0x03)


@dataclass
class FacingCtx:
    mask: int = 0xC0
    shift: int = 6
    mask_d4: int = 0
    mask_d5: int = 0
    shift_d4: int = 0
    shift_d5: int = 0


@dataclass
class View3DBlit:
    kind: str
    depth: int
    frame: int
    x: int
    y: int
    code: int


@dataclass
class View3DScene:
    hood: List[int] = field(default_factory=lambda: [0] * 13)
    slots: List[int] = field(default_factory=lambda: [0] * FRUSTUM_SLOTS)
    blits: List[View3DBlit] = field(default_factory=list)


def load_map(path: Path) -> List[Tuple[bytes, bytes]]:
    data = path.read_bytes()
    if len(data) < MAP_SCREENS * MAP_SCREEN_SIZE:
        raise ValueError(f"map.dat too small: {len(data)} bytes")
    out: List[Tuple[bytes, bytes]] = []
    for i in range(MAP_SCREENS):
        off = i * MAP_SCREEN_SIZE
        out.append((data[off : off + MAP_PAGE_SIZE], data[off + MAP_PAGE_SIZE : off + MAP_SCREEN_SIZE]))
    return out


def set_facing(facing: int) -> FacingCtx:
    f = FacingCtx()
    i = facing & 3
    d3 = DIR_MASK[i]
    f.mask = d3
    f.shift = 0 if d3 == 0x03 else (6 - 2 * i)
    f.shift_d4 = (f.shift + 2) & 7
    f.shift_d5 = 6 if f.shift == 0 else (f.shift - 2)
    f.mask_d5 = 0xC0 if d3 == 0x03 else (d3 >> 2)
    f.mask_d4 = 0x03 if d3 == 0xC0 else ((d3 << 2) & 0xFF)
    return f


def direction_bundle(facing: int) -> Sequence[int]:
    return (BUNDLE_N, BUNDLE_E, BUNDLE_S, BUNDLE_W)[facing & 3]


def sample_visual(visual: bytes, x: int, y: int) -> int:
    x &= 0x0F
    y &= 0x0F
    if x < 0 or y < 0 or x >= MAP_GRID or y >= MAP_GRID:
        return 0
    return visual[(y << 4) | x]


def refresh_neighbourhood(visual: bytes, px: int, py: int, facing: int) -> List[int]:
    hood = [0] * 13
    b = direction_bundle(facing)
    dx, dy = int.from_bytes(bytes([b[0] & 0xFF]), "big", signed=True), int.from_bytes(bytes([b[1] & 0xFF]), "big", signed=True)

    def row_sampler(sx: int, sy: int, out_off: int) -> None:
        x, y = sx, sy
        for i in range(5):
            hood[out_off + i] = sample_visual(visual, x, y)
            x += dx
            y += dy

    def sb(i: int) -> int:
        return int.from_bytes(bytes([b[i] & 0xFF]), "big", signed=True)

    row_sampler(px, py, 0)
    row_sampler(px + sb(2), py + sb(3), 4)
    row_sampler(px + sb(4), py + sb(5), 8)
    return hood


def put_slot(slots: List[int], idx: int, cell: int, mask: int, shift: int) -> None:
    if (cell & mask) == 0:
        return
    # ASM stores the post-shift byte as-is (no &3 clamp).
    code = (cell >> shift) & 3
    if code == 0:
        return
    slots[idx] = code


def build_frustum(hood: Sequence[int], f: FacingCtx) -> List[int]:
    """Port of editor/src/core/View3D.cpp buildFrustum (asm @0x2900..0x2BBC)."""
    slots = [0] * FRUSTUM_SLOTS
    d1, d4, d5 = f.mask, f.mask_d4, f.mask_d5
    sh_d4, sh_d5, sh_fwd = f.shift_d4, f.shift_d5, f.shift

    def b(idx: int) -> int:
        return hood[idx] if 0 <= idx < len(hood) else 0

    put_slot(slots, S_F20, b(0), d4, sh_d4)
    if slots[S_F20] == 0:
        put_slot(slots, S_F14, b(4), d1, sh_fwd)
    put_slot(slots, S_F1C, b(0), d5, sh_d5)
    if slots[S_F1C] == 0:
        put_slot(slots, S_F10, b(8), d1, sh_fwd)

    if b(0) & d1:
        put_slot(slots, S_F18, b(0), d1, sh_fwd)
        if slots[S_F20] == 0 and slots[S_F14] == 0:
            put_slot(slots, S_F13, b(5), d1, sh_fwd)
        if slots[S_F1C] == 0 and slots[S_F10] == 0:
            put_slot(slots, S_F0F, b(9), d1, sh_fwd)
    else:
        if b(1) & d4:
            put_slot(slots, S_F1F, b(1), d4, sh_d4)
        else:
            put_slot(slots, S_F13, b(5), d1, sh_fwd)
        if b(1) & d5:
            put_slot(slots, S_F1B, b(1), d5, sh_d5)
        else:
            put_slot(slots, S_F0F, b(9), d1, sh_fwd)
        put_slot(slots, S_F17, b(1), d1, sh_fwd)
        if slots[S_F1F] == 0 and slots[S_F13] == 0:
            put_slot(slots, S_F12, b(6), d1, sh_fwd)
        if slots[S_F1B] == 0 and slots[S_F0F] == 0:
            put_slot(slots, S_F0E, b(10), d1, sh_fwd)

        if (b(1) & d1) == 0:
            if b(2) & d4:
                put_slot(slots, S_F1E, b(2), d4, sh_d4)
            else:
                put_slot(slots, S_F12, b(6), d1, sh_fwd)
            if b(2) & d5:
                put_slot(slots, S_F1A, b(2), d5, sh_d5)
            else:
                put_slot(slots, S_F0E, b(10), d1, sh_fwd)
            put_slot(slots, S_F16, b(2), d1, sh_fwd)

            if (b(2) & d1) == 0:
                if b(3) & d4:
                    put_slot(slots, S_F1D, b(3), d4, sh_d4)
                else:
                    put_slot(slots, S_F11, b(7), d1, sh_fwd)
                if b(3) & d5:
                    put_slot(slots, S_F19, b(3), d5, sh_d5)
                else:
                    put_slot(slots, S_F0D, b(11), d1, sh_fwd)
                put_slot(slots, S_F15, b(3), d1, sh_fwd)

    def norm(a: int, bb: int) -> None:
        # Door (3) beside an open side → plain wall (1) @0x2B6A..0x2BBC.
        if slots[a] != 0 and slots[bb] == 3:
            slots[bb] = 1

    norm(S_F20, S_F13)
    norm(S_F1C, S_F0F)
    norm(S_F1F, S_F12)
    norm(S_F1B, S_F0E)
    return slots


def collect_blits(slots: Sequence[int]) -> List[View3DBlit]:
    # Paint order @0x2F7E — positions from DATA tables (same as View3D.cpp)
    front_y, front_x = [22, 40, 54, 62], [32, 64, 88, 104]
    left_x1, left_y1 = [8, 32, 64, 88], [8, 22, 40, 54]
    left_x2, left_y2 = [8, 8, 40, 88], [22, 40, 54, 62]
    right_x1, right_y1 = [192, 160, 136, 120], [8, 22, 40, 54]
    right_x2, right_y2 = [192, 160, 136, 120], [22, 40, 54, 62]
    left_base, right_base = [12, 14, 2, 3], [13, 15, 2, 3]

    blits: List[View3DBlit] = []

    def paint(kind: str, paint_depth: int, code: int) -> None:
        if code == 0:
            return
        depth = (paint_depth & 0x7F) - 1
        if depth < 0 or depth >= DEPTH_BANDS:
            return
        mirror = paint_depth >= 0x80
        is_door = (code == 2)
        if kind == "front":
            x = front_x[depth]
            y = front_y[depth] + (1 if depth == 0 else 0)
            frame = depth + (0x10 if is_door else 0)
            blits.append(View3DBlit("front", depth, frame, x, y, code))
        elif kind == "left":
            if mirror:
                base = left_base[depth]
                frame = base + (0x10 if is_door else 0)
                blits.append(View3DBlit("left", depth, frame, left_x2[depth], left_y2[depth], code))
            else:
                base = depth + 4
                frame = base + (0x10 if is_door else 0)
                blits.append(View3DBlit("left", depth, frame, left_x1[depth], left_y1[depth], code))
        else:
            if mirror:
                base = right_base[depth]
                frame = base + (0x10 if is_door else 0)
                blits.append(View3DBlit("right", depth, frame, right_x2[depth], right_y2[depth], code))
            else:
                base = depth + 8
                frame = base + (0x10 if is_door else 0)
                blits.append(View3DBlit("right", depth, frame, right_x1[depth], right_y1[depth], code))

    s = slots
    paint("left", 4, s[S_F1D]); paint("left", 0x84, s[S_F11])
    paint("right", 4, s[S_F19]); paint("right", 0x84, s[S_F0D])
    paint("front", 4, s[S_F15])
    paint("left", 3, s[S_F1E]); paint("left", 0x83, s[S_F12])
    paint("right", 3, s[S_F1A]); paint("right", 0x83, s[S_F0E])
    paint("front", 3, s[S_F16])
    paint("left", 2, s[S_F1F]); paint("left", 0x82, s[S_F13])
    paint("right", 2, s[S_F1B]); paint("right", 0x82, s[S_F0F])
    paint("front", 2, s[S_F17])
    paint("left", 1, s[S_F20]); paint("left", 0x81, s[S_F14])
    paint("right", 1, s[S_F1C]); paint("right", 0x81, s[S_F10])
    paint("front", 1, s[S_F18])
    return blits


def torch_overlay_for(b: View3DBlit, phase: int = 0) -> tuple[int, int, int] | None:
    """``townt.32`` flame overlay for map field **2** torch walls (``view3dTorchBlitFor`` / -$4F62).

    Returns ``(frame, x, y)`` or ``None``. Field 3 doors must not get overlays.
    """
    if b.code != 2 or b.depth > 2:
        return None
    flicker = phase % 3
    if b.kind == "front":
        return (
            0x12 + b.depth * 3 + flicker,
            (105, 108, 107)[b.depth],
            (44, 52, 60)[b.depth],
        )
    left_far = {12, 14, 2, 3}
    right_far = {13, 15, 2, 3}
    if b.kind == "left":
        if b.frame in left_far:
            if b.depth == 0:
                return None
            return (
                0x12 + b.depth * 3 + flicker,
                (8, 16, 64)[b.depth],
                (44, 52, 60)[b.depth],
            )
        return (
            b.depth * 3 + flicker,
            (8, 43, 73)[b.depth],
            (49, 55, 59)[b.depth],
        )
    if b.kind == "right":
        if b.frame in right_far:
            if b.depth == 0:
                return None
            return (
                0x12 + b.depth * 3 + flicker,
                (202, 199, 152)[b.depth],
                (44, 52, 60)[b.depth],
            )
        return (
            9 + b.depth * 3 + flicker,
            (196, 166, 142)[b.depth],
            (49, 55, 59)[b.depth],
        )
    return None


def build_scene(visual: bytes, x: int, y: int, facing: int) -> View3DScene:
    scene = View3DScene()
    scene.hood = refresh_neighbourhood(visual, x, y, facing)
    f = set_facing(facing)
    scene.slots = build_frustum(scene.hood, f)
    scene.blits = collect_blits(scene.slots)
    return scene


def describe_nibbles(cell: int) -> str:
    """Page-0 visual: 2 bits per side, 0-3 wall codes only (no event bit)."""
    return (f"N={cell & 3} E={(cell >> 2) & 3} S={(cell >> 4) & 3} "
            f"W={(cell >> 6) & 3}")


WALL_SYM = {0: ".", 1: "#", 2: "D", 3: "T"}


def slots_to_lattice(slots: Sequence[int]) -> dict[tuple[int, int], str]:
    """Map frustum slots to (latX, latRow) like View3D.cpp collectBlits."""
    lat: dict[tuple[int, int], str] = {}
    paints = (
        (-1, 4, S_F1D), (-2, 0x84, S_F11), (1, 4, S_F19), (2, 0x84, S_F0D), (0, 4, S_F15),
        (-1, 3, S_F1E), (-2, 0x83, S_F12), (1, 3, S_F1A), (2, 0x83, S_F0E), (0, 3, S_F16),
        (-1, 2, S_F1F), (-2, 0x82, S_F13), (1, 2, S_F1B), (2, 0x82, S_F0F), (0, 2, S_F17),
        (-1, 1, S_F20), (-2, 0x81, S_F14), (1, 1, S_F1C), (2, 0x81, S_F10), (0, 1, S_F18),
    )
    for lat_x, paint_depth, slot in paints:
        code = slots[slot]
        if not code:
            continue
        depth = (paint_depth & 0x7F) - 1
        row = -depth
        lat[(lat_x, row)] = WALL_SYM.get(code, "?")
    return lat


def print_lattice(slots: Sequence[int]) -> None:
    lat = slots_to_lattice(slots)
    cols = (-2, -1, 0, 1, 2)
    print("  lattice (latX columns, row -3=far .. 0=near):")
    print("        " + "".join(f"{c:>4d}" for c in cols))
    for row in (-3, -2, -1, 0):
        cells = []
        for c in cols:
            cells.append(f"  {lat.get((c, row), ' .')}")
        print(f"  {row:2d}   " + "".join(cells))


def hood_coord(cam_x: int, cam_y: int, facing: int, idx: int) -> Tuple[int, int]:
    b = direction_bundle(facing)
    dx = int.from_bytes(bytes([b[0] & 0xFF]), "big", signed=True)
    dy = int.from_bytes(bytes([b[1] & 0xFF]), "big", signed=True)
    x, y = cam_x, cam_y
    def sb(i: int) -> int:
        return int.from_bytes(bytes([b[i] & 0xFF]), "big", signed=True)

    if idx >= 8:
        x += sb(5)
        y += sb(4)
        idx -= 8
    elif idx >= 4:
        x += sb(3)
        y += sb(2)
        idx -= 4
    return x + idx * dx, y + idx * dy


def dump_trace(visual: bytes, collision: bytes, screen: int, x: int, y: int, facing: int) -> None:
    scene = build_scene(visual, x, y, facing)
    f = set_facing(facing)
    print(f"=== screen {screen}  ({x},{y}) facing {FACE[facing & 3]} ===")
    print(f"masks: d1=0x{f.mask:02X} shift={f.shift}  d4=0x{f.mask_d4:02X} sh={f.shift_d4}  "
          f"d5=0x{f.mask_d5:02X} sh={f.shift_d5}")
    print("hood (page-0 visual):")
    for i, cell in enumerate(scene.hood):
        hx, hy = hood_coord(x, y, facing, i)
        coll = sample_visual(collision, hx, hy)
        print(f"  [{i:2d}] 0x{cell:02X} vis  0x{coll:02X} coll  ({hx},{hy})  {describe_nibbles(cell)}")
    print("frustum slots:")
    for i, v in enumerate(scene.slots):
        if v:
            print(f"  -${SLOT_NAMES[i]} = {v}")
    print_lattice(scene.slots)
    print(f"{len(scene.blits)} blit(s):")
    for b in scene.blits:
        print(f"  {b.kind:5s} depth={b.depth} frame={b.frame} @ ({b.x},{b.y})")
    print()


def dump_map_window(visual: bytes, cx: int, cy: int, radius: int = 3) -> None:
    print(f"  page-0 window (data x, y; y+ = north):")
    for y in range(cy + radius, cy - radius - 1, -1):
        row = []
        for x in range(cx - radius, cx + radius + 1):
            if x == cx and y == cy:
                row.append(" @ ")
            else:
                v = sample_visual(visual, x, y)
                row.append(f"{v:02X}")
        print(f"    y={y:2d}  " + " ".join(row))
    print(f"  @ = player; visual 2b/side N@0 E@2 S@4 W@6  (0-3)")


def dump_data_tables() -> None:
    if not DATA_BIN.is_file():
        print(f"DATA hunk not found: {DATA_BIN}", file=sys.stderr)
        return
    blob = DATA_BIN.read_bytes()

    def a4_off(base: int) -> int:
        return A4 - base

    def dump8(base: int, n: int, label: str) -> None:
        off = a4_off(base)
        vals = list(blob[off : off + n])
        print(f"{label} @ A4-${base:04X}: {vals}")

    print("Direction bundles (map_neighbourhood_refresh @0x19D6):")
    dump8(0x7646, 6, "BundleN")
    dump8(0x7640, 6, "BundleS")
    dump8(0x763A, 6, "BundleE")
    dump8(0x7634, 6, "BundleW")
    print()
    print("Python constants:")
    for name, base in [("BUNDLE_N", 0x7646), ("BUNDLE_S", 0x7640), ("BUNDLE_E", 0x763A), ("BUNDLE_W", 0x7634)]:
        vals = tuple(blob[a4_off(base) : a4_off(base) + 6])
        print(f"  {name} = {vals}")


TEST_MATRIX = (
    (0, 8, 8, 0, "Middlegate (8,8) N"),
    (0, 8, 8, 3, "Middlegate (8,8) W"),
    (0, 8, 8, 2, "Middlegate (8,8) S"),
    (0, 7, 5, 0, "Middlegate (7,5) N"),
    (0, 7, 5, 1, "Middlegate (7,5) E"),
    (0, 7, 5, 2, "Middlegate (7,5) S"),
    (0, 7, 5, 3, "Middlegate (7,5) W"),
    (0, 8, 5, 0, "Middlegate corridor (8,5) N"),
    (0, 7, 8, 2, "Middlegate (7,8) S open"),
    (1, 8, 6, 0, "Atlantium (8,6) N"),
)


def run_matrix(map_path: Path) -> None:
    screens = load_map(map_path)
    print(f"map.dat: {map_path}  ({len(screens)} screens)\n")
    for screen, x, y, facing, label in TEST_MATRIX:
        print(f"--- {label} ---")
        visual, collision = screens[screen]
        dump_trace(visual, collision, screen, x, y, facing)


def main() -> None:
    ap = argparse.ArgumentParser(description="ASM-faithful MM2 3D view trace (Python)")
    ap.add_argument("map_dat", nargs="?", type=Path, help="path to map.dat")
    ap.add_argument("screen", nargs="?", type=int)
    ap.add_argument("x", nargs="?", type=int)
    ap.add_argument("y", nargs="?", type=int)
    ap.add_argument("facing", nargs="?", type=int, help="0=N 1=E 2=S 3=W")
    ap.add_argument("--matrix", action="store_true", help="run known test positions")
    ap.add_argument("--dump-bundles", action="store_true", help="dump DATA hunk direction bundles")
    args = ap.parse_args()

    if args.dump_bundles:
        dump_data_tables()
        return

    root = Path(__file__).resolve().parents[1]
    map_path = args.map_dat or (root / "map.dat")
    if not map_path.is_file():
        ap.error(f"map.dat not found: {map_path}")

    if args.matrix or args.screen is None:
        run_matrix(map_path)
        return

    screens = load_map(map_path)
    if not 0 <= args.screen < MAP_SCREENS:
        raise SystemExit(f"screen must be 0..{MAP_SCREENS - 1}")
    visual, collision = screens[args.screen]
    print(f"map window around ({args.x},{args.y}):")
    dump_map_window(visual, args.x, args.y)
    print()
    dump_trace(visual, collision, args.screen, args.x, args.y, args.facing)


if __name__ == "__main__":
    main()
