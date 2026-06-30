#!/usr/bin/env python3
"""Verify torchBlits for Middlegate door views — mirrors wiki/maze-walker/view3d.js."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from attrib_codec import AttribFile  # noqa: E402
from view3d_indoor import StitchedVisual, build_scene, refresh_hood  # noqa: E402
from view3d_trace import FACE, describe_nibbles, load_map, set_facing, build_frustum, collect_blits  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
FRONT_TORCH_XY = [(105, 44), (108, 52), (107, 60)]


def torch_blit_for_js(b, phase=0):
    """Exact port of view3d.js torchBlitFor."""
    if b.code != 2 or b.depth > 2:
        return None
    flicker = phase % 3
    base = b.frame - 0x10 if b.code == 2 else b.frame
    left_far = {12, 14, 2, 3}
    right_far = {13, 15, 2, 3}
    if b.kind == "front":
        return (0x12 + b.depth * 3 + flicker, FRONT_TORCH_XY[b.depth][0], FRONT_TORCH_XY[b.depth][1])
    mirror = getattr(b, "mirror", False)
    if b.kind == "left":
        if mirror or base in left_far:
            if b.depth == 0:
                return None
            return (0x12 + b.depth * 3 + flicker, (8, 16, 64)[b.depth], (44, 52, 60)[b.depth])
        return (b.depth * 3 + flicker, (8, 43, 73)[b.depth], (49, 55, 59)[b.depth])
    if b.kind == "right":
        if mirror or base in right_far:
            if b.depth == 0:
                return None
            return (0x12 + b.depth * 3 + flicker, (202, 199, 152)[b.depth], (44, 52, 60)[b.depth])
        return (9 + b.depth * 3 + flicker, (196, 166, 142)[b.depth], (49, 55, 59)[b.depth])
    return None


def collect_blits_js(slots):
    """Mirror JS collectBlits return {blits, torchBlits}."""
    blits = collect_blits(slots)
    torch_blits = [b for b in blits if b.code == 2 and b.depth <= 2]
    return blits, torch_blits


def check(maps, attrib, x, y, facing, label, screen=0):
    grid = StitchedVisual(maps, attrib, screen)
    hood = refresh_hood(grid, x, y, facing)
    slots = build_frustum(hood, set_facing(facing))
    blits, torch_blits = collect_blits_js(slots)

    overlays = []
    for b in torch_blits:
        tb = torch_blit_for_js(b)
        if tb:
            overlays.append((b, tb))

    door_torch = []
    for b in blits:
        if b.code == 3 and b.kind == "front" and b.depth <= 2:
            # simulate iterating ALL blits (pre-fix walker.js)
            if b.code == 3:  # old torchBlitFor checked code===3
                door_torch.append(("OLD_BUG", b, FRONT_TORCH_XY[b.depth]))

    print(f"=== {label} screen={screen} ({x},{y}) {FACE[facing]} ===")
    for i, v in enumerate(hood):
        if v:
            print(f"  hood[{i}] 0x{v:02X} {describe_nibbles(v)}")
    nz = [(i, v) for i, v in enumerate(slots) if v]
    print(f"  slots: {[(f'F{20-i}' if False else slots, ) for i,v in nz]}")  # skip
    print(f"  non-zero slots: {nz}")
    print(f"  blits: {len(blits)}")
    for b in blits:
        print(f"    {b.kind:5s} d={b.depth} code={b.code} frame={b.frame} @({b.x},{b.y})")
    print(f"  torchBlits (code=2, depth<=2): {len(torch_blits)}")
    for b in torch_blits:
        print(f"    {b.kind} d={b.depth} frame={b.frame}")
    print(f"  townt.32 overlays from torchBlits: {len(overlays)}")
    for b, tb in overlays:
        print(f"    frame={tb[0]} @({tb[1]},{tb[2]}) from {b.kind} d={b.depth}")
    print(f"  OLD bug (torchBlitFor on door blits): {len(door_torch)}")
    for tag, b, pos in door_torch:
        print(f"    WOULD DRAW @({pos[0]},{pos[1]}) door d={b.depth}")
    print()
    return len(overlays), len(door_torch)


def main() -> int:
    maps = load_map(ROOT / "map.dat")
    attrib = AttribFile.load(str(ROOT / "attrib.dat"))

    fails = 0
    cases = [
        (6, 6, 0, "Gateway approach"),
        (6, 7, 0, "On temple tile"),
        (7, 7, 0, "Alley view"),
        (7, 3, 0, "Inn torch wall (expect 1 overlay)"),
    ]
    for x, y, f, label in cases:
        ov, old = check(maps, attrib, x, y, f, label)
        if label.startswith("Inn"):
            if ov < 1:
                print(f"FAIL: {label} expected torch overlay")
                fails += 1
        else:
            if ov > 0:
                print(f"FAIL: {label} got {ov} torch overlays (expected 0)")
                fails += 1
            # door positions specifically
            grid = StitchedVisual(maps, attrib, 0)
            scene = build_scene(grid, x, y, f)
            for b in scene.blits:
                if b.code == 3 and b.kind == "front":
                    tx, ty = FRONT_TORCH_XY[b.depth]
                    for tb_b in [x for x in scene.blits if x.code == 2]:
                        pass
                    for tb in [torch_blit_for_js(x) for x in collect_blits_js(build_frustum(scene.hood, set_facing(f)))[1]]:
                        if tb and tb[1] == tx and tb[2] == ty:
                            print(f"FAIL: torch overlay at door position ({tx},{ty})")
                            fails += 1

    if fails:
        print(f"FAILED: {fails} checks")
        return 1
    print("OK: Gateway Temple door positions = 0 torch overlays")
    return 0


if __name__ == "__main__":
    sys.exit(main())
