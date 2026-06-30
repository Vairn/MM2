#!/usr/bin/env python3
"""Verify torch overlay eligibility at Middlegate door positions."""

from __future__ import annotations

import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from view3d_trace import build_scene, load_map, FACE  # noqa: E402

# Torch overlay positions (front wall, depth 1) — townt.32 centered on door
FRONT_TORCH_XY = (105, 52)


def torch_blit_for(kind: str, depth: int, code: int, frame: int, mirror: bool = False):
    """Mirror wiki/maze-walker/view3d.js torchBlitFor (fixed: code must be 2)."""
    if code != 2 or depth > 2:
        return None
    flicker = 0
    base = frame - 0x10 if code == 2 else frame
    left_far = {12, 14, 2, 3}
    right_far = {13, 15, 2, 3}
    if kind == "front":
        return (0x12 + depth * 3 + flicker, (105, 108, 107)[depth], (44, 52, 60)[depth])
    if kind == "left":
        if mirror or base in left_far:
            if depth == 0:
                return None
            return (0x12 + depth * 3 + flicker, (8, 16, 64)[depth], (44, 52, 60)[depth])
        return (depth * 3 + flicker, (8, 43, 73)[depth], (49, 55, 59)[depth])
    if kind == "right":
        if mirror or base in right_far:
            if depth == 0:
                return None
            return (0x12 + depth * 3 + flicker, (202, 199, 152)[depth], (44, 52, 60)[depth])
        return (9 + depth * 3 + flicker, (196, 166, 142)[depth], (49, 55, 59)[depth])
    return None


def old_torch_blit_for(kind: str, depth: int, code: int, frame: int):
    """Pre-fix bug: torchBlitFor required code === 3 (door)."""
    if code != 3 or depth > 2:
        return None
    if kind == "front":
        return (0x12 + depth * 3, (105, 108, 107)[depth], (44, 52, 60)[depth])
    return ("side", depth, code)


def check_pos(visual, x, y, facing, label: str) -> None:
    scene = build_scene(visual, x, y, facing)
    print(f"=== {label} ({x},{y}) facing {FACE[facing & 3]} ===")
    door_front = [b for b in scene.blits if b.kind == "front" and b.code == 3]
    torch_slots = [b for b in scene.blits if b.code == 2]
    old_on_door = []
    new_on_door = []
    for b in scene.blits:
        tb_old = old_torch_blit_for(b.kind, b.depth, b.code, b.frame)
        tb_new = torch_blit_for(b.kind, b.depth, b.code, b.frame, mirror=False)
        if b.code == 3 and b.kind == "front" and tb_old:
            old_on_door.append((b, tb_old))
        if b.code == 3 and b.kind == "front" and tb_new:
            new_on_door.append((b, tb_new))
    print(f"  front door blits: {len(door_front)}")
    for b in door_front:
        print(f"    depth={b.depth} frame={b.frame} @ ({b.x},{b.y}) code={b.code}")
    print(f"  torch wall slots (code=2): {len(torch_slots)}")
    for b in torch_slots:
        print(f"    {b.kind} depth={b.depth} frame={b.frame} code={b.code}")
    print(f"  OLD bug torch on door: {len(old_on_door)}")
    for b, tb in old_on_door:
        print(f"    BUG depth={b.depth} torch @ {tb[1:]}")
    print(f"  FIXED torch on door: {len(new_on_door)}")
    print()


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    visual, _ = load_map(root / "map.dat")[0]
    # Gateway Temple door cell (6,7); event DIR_N
    cell = visual[(7 << 4) | 6]
    print(f"map screen 0 cell (6,7) page-0 = 0x{cell:02X}")
    print(f"  N={cell & 3} E={(cell >> 2) & 3} S={(cell >> 4) & 3} W={(cell >> 6) & 3}")
    print()
    check_pos(visual, 6, 6, 0, "Approach temple from S")
    check_pos(visual, 6, 7, 0, "On temple tile")
    check_pos(visual, 7, 7, 0, "Alley (7,7)")
    check_pos(visual, 7, 3, 0, "Inn spawn (7,3)")


if __name__ == "__main__":
    main()
