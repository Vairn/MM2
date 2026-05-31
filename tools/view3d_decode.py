#!/usr/bin/env python3
"""MM2 first-person view helper — DEPRECATED edge-walking prototype.

The game uses hood sampling + frustum_wall_cell_builder @0x2900 (ported in
editor/src/core/View3D.cpp). Use editor/tests/view3d_trace.cpp for verification.
See EXTRACTED/docs/15-3d-view-and-game-screen.md.
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import List, Tuple

MAP_SCREENS = 60
MAP_SCREEN_SIZE = 512
MAP_PAGE_SIZE = 256
MAP_GRID = 16
DEPTH_BANDS = 4

# DATA hunk tables (docs/15-3d-view-and-game-screen.md)
FRONT_X = [22, 40, 54, 62]
FRONT_Y = [32, 64, 88, 104]
LEFT_X1 = [8, 32, 64, 88]
LEFT_Y1 = [8, 22, 40, 54]
LEFT_X2 = [8, 8, 40, 88]
LEFT_Y2 = [22, 40, 54, 62]
RIGHT_X1 = [192, 160, 136, 120]
RIGHT_Y1 = [8, 22, 40, 54]
RIGHT_X2 = [192, 160, 136, 120]
RIGHT_Y2 = [22, 40, 54, 62]
LEFT_FRAME = [12, 14, 2, 3]
RIGHT_FRAME = [13, 15, 2, 3]

FACING_MASK = [0xC0, 0x30, 0x0C, 0x03]
FACING_SHIFT = [6, 4, 2, 0]


class MapDir(IntEnum):
    NORTH = 0
    EAST = 1
    SOUTH = 2
    WEST = 3


@dataclass
class View3DCamera:
    x: int = 8
    y: int = 8
    facing: int = 0  # 0=N, 1=E, 2=S, 3=W


@dataclass
class View3DBlit:
    kind: str  # front | left | right
    depth: int
    wall_code: int
    mirror: bool = False
    frame: int = -1
    x: int = 0
    y: int = 0


def load_map(path: Path) -> List[Tuple[bytes, bytes]]:
    data = path.read_bytes()
    if len(data) < MAP_SCREENS * MAP_SCREEN_SIZE:
        raise ValueError(f"map.dat too small: {len(data)} bytes")
    screens = []
    for i in range(MAP_SCREENS):
        off = i * MAP_SCREEN_SIZE
        visual = data[off : off + MAP_PAGE_SIZE]
        collision = data[off + MAP_PAGE_SIZE : off + MAP_SCREEN_SIZE]
        screens.append((visual, collision))
    return screens


def wall_field(cell: int, direction: MapDir) -> int:
    return (cell >> (int(direction) * 2)) & 3


def opposite(d: MapDir) -> MapDir:
    return MapDir((int(d) + 2) & 3)


def forward_delta(facing: int) -> Tuple[int, int]:
    return [(0, -1), (1, 0), (0, 1), (-1, 0)][facing & 3]


def right_delta(facing: int) -> Tuple[int, int]:
    dx, dy = forward_delta((facing + 1) & 3)
    return dx, dy


def left_delta(facing: int) -> Tuple[int, int]:
    dx, dy = right_delta(facing)
    return -dx, -dy


def collision_at(collision: bytes, x: int, y: int) -> int:
    if x < 0 or y < 0 or x >= MAP_GRID or y >= MAP_GRID:
        return 0xFF
    return collision[y * MAP_GRID + x]


def edge_between(collision: bytes, ax: int, ay: int, bx: int, by: int, edge: MapDir) -> int:
    ca = collision_at(collision, ax, ay)
    if ca == 0xFF:
        return 3
    code = wall_field(ca, edge)
    cb = collision_at(collision, bx, by)
    if code == 3 and cb != 0xFF and wall_field(cb, opposite(edge)) == 0:
        code = 1
    return code


def build_scene(collision: bytes, cam: View3DCamera) -> List[View3DBlit]:
    facing = cam.facing & 3
    fdx, fdy = forward_delta(facing)
    ldx, ldy = left_delta(facing)
    rdx, rdy = right_delta(facing)
    fwd = MapDir(facing)
    back = opposite(fwd)
    lft = MapDir((facing + 3) & 3)
    rgt = MapDir((facing + 1) & 3)

    blits: List[View3DBlit] = []
    for depth in range(DEPTH_BANDS - 1, -1, -1):
        dist = depth + 1
        fx = cam.x + fdx * dist
        fy = cam.y + fdy * dist
        lx = cam.x + fdx * dist + ldx
        ly = cam.y + fdy * dist + ldy
        rx = cam.x + fdx * dist + rdx
        ry = cam.y + fdy * dist + rdy

        if dist == 1:
            px, py = cam.x, cam.y
        else:
            px = cam.x + fdx * (dist - 1)
            py = cam.y + fdy * (dist - 1)

        front = edge_between(collision, fx, fy, px, py, back)
        left = edge_between(collision, lx, ly, lx - ldx, ly - ldy, rgt)
        right = edge_between(collision, rx, ry, rx - rdx, ry - rdy, lft)
        far_col = depth >= 2

        for kind, code, mirror in (
            ("left", left, far_col),
            ("right", right, far_col),
            ("front", front, False),
        ):
            if code == 0:
                continue
            blit = View3DBlit(kind, depth, code, mirror)
            blit.frame = wall_frame(blit)
            blit.x, blit.y = blit_pos(blit)
            blits.append(blit)
    return blits


def wall_frame(b: View3DBlit) -> int:
    d = b.depth
    if b.kind == "front":
        frame = d
        if b.wall_code == 2:
            frame += 0x10
        if b.wall_code == 3:
            frame = 1
        return frame
    base = LEFT_FRAME if b.kind == "left" else RIGHT_FRAME
    if b.mirror:
        frame = base[d]
        if b.wall_code == 2:
            return 0x10
        if b.wall_code != 1:
            frame += b.wall_code
        return frame
    frame = base[d] + 4
    if b.wall_code == 2:
        frame += 0x10
    return frame


def blit_pos(b: View3DBlit) -> Tuple[int, int]:
    d = b.depth
    if b.kind == "front":
        y = FRONT_X[d] + (1 if d == 0 else 0)
        return FRONT_Y[d], y
    if b.kind == "left":
        return (LEFT_X2[d], LEFT_Y2[d]) if b.mirror else (LEFT_X1[d], LEFT_Y1[d])
    return (RIGHT_X2[d], RIGHT_Y2[d]) if b.mirror else (RIGHT_X1[d], RIGHT_Y1[d])


def describe_collision(cell: int) -> str:
    parts = []
    for name, d in [("N", MapDir.NORTH), ("E", MapDir.EAST), ("S", MapDir.SOUTH), ("W", MapDir.WEST)]:
        parts.append(f"{name}={wall_field(cell, d)}")
    return " ".join(parts)


def main() -> None:
    ap = argparse.ArgumentParser(description="Decode MM2 3D view frustum from map.dat")
    ap.add_argument("map_dat", type=Path, help="path to map.dat")
    ap.add_argument("screen", type=int, help="screen index 0..59")
    ap.add_argument("--x", type=int, default=8)
    ap.add_argument("--y", type=int, default=8)
    ap.add_argument("--facing", type=int, default=0, choices=range(4), help="0=N 1=E 2=S 3=W")
    ap.add_argument("--cell", action="store_true", help="dump collision byte at camera cell")
    args = ap.parse_args()

    screens = load_map(args.map_dat)
    if not 0 <= args.screen < MAP_SCREENS:
        raise SystemExit(f"screen must be 0..{MAP_SCREENS - 1}")

    _visual, collision = screens[args.screen]
    cam = View3DCamera(args.x, args.y, args.facing)
    cell = collision_at(collision, cam.x, cam.y)
    print(f"screen {args.screen}  camera ({cam.x},{cam.y}) facing {cam.facing}")
    if args.cell:
        print(f"  collision @ camera: 0x{cell:02X}  ({describe_collision(cell)})")

    blits = build_scene(collision, cam)
    print(f"  {len(blits)} blit(s):")
    for b in blits:
        print(
            f"    {b.kind:5s} depth={b.depth} code={b.wall_code} "
            f"mirror={int(b.mirror)} frame={b.frame} @ ({b.x},{b.y})"
        )


if __name__ == "__main__":
    main()
