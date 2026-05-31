#!/usr/bin/env python3
"""ASM-driven indoor MM2 3D viewer.

Rebuilt from the runtime flow:
  - map_neighbourhood_refresh @0x19D6 (3x5 hood sampling)
  - frustum_wall_cell_builder @0x2900 (slot extraction + 3->1 normalization)
  - wall_piece_* + view_3d_master @0x2C1A / 0x2F7E (paint tables/order)

This viewer keeps those semantics and only adds a debug minimap/HUD layer.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from attrib_codec import AttribFile  # noqa: E402
from mm2_map_ui import area_name, location_label_from_attrib, tile_notation  # noqa: E402
from render_view_refs import VIEW_H, VIEW_W, blit, composite_backdrop, load_frame  # noqa: E402
from view3d_trace import (  # noqa: E402
    BUNDLE_E,
    BUNDLE_N,
    BUNDLE_S,
    BUNDLE_W,
    FACE,
    MAP_GRID,
    MAP_PAGE_SIZE,
    MAP_SCREENS,
    SLOT_NAMES,
    View3DScene,
    build_frustum,
    collect_blits,
    describe_nibbles,
    load_map,
    set_facing,
)

ROOT = Path(__file__).resolve().parents[1]
BUNDLES = (BUNDLE_N, BUNDLE_E, BUNDLE_S, BUNDLE_W)
STEP_DX = (0, 1, 0, -1)
STEP_DY = (1, 0, -1, 0)
FACING_ARROW = ((0, -1), (1, 0), (0, 1), (-1, 0))
HOOD_LABELS = ("C0", "C1", "C2", "C3", "C4", "L1", "L2", "L3", "R0", "R1", "R2", "R3", "R4")
# Game-facing direction bit placement in page-0 visual byte.
# N/E/S/W map to bit-pairs 6/4/2/0 (masks C0/30/0C/03).
DIR_PAIR_SHIFT = (6, 4, 2, 0)

# editor/src/core/Cartography.h kCartoTile (ASM helper @0x2182)
K_CARTO_TILE = (
    0x00, 0x05, 0x06, 0x05, 0x03, 0x0B, 0x0D, 0x0B,
    0x04, 0x0C, 0x0E, 0x0C, 0x03, 0x0B, 0x0D, 0x0B,
    0x01, 0x0F, 0x11, 0x0F, 0x07, 0x13, 0x16, 0x13,
    0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
    0x02, 0x10, 0x12, 0x10, 0x08, 0x14, 0x18, 0x14,
    0x0A, 0x17, 0x1A, 0x17, 0x08, 0x14, 0x18, 0x14,
    0x01, 0x0F, 0x11, 0x0F, 0x07, 0x13, 0x16, 0x13,
    0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
)


@dataclass
class SheetSet:
    walls: str
    floor: str
    sky: str = "sky.32"


def _sb(bundle: Sequence[int], idx: int) -> int:
    return int.from_bytes(bytes([bundle[idx] & 0xFF]), "big", signed=True)


class StitchedVisual:
    """Page-0 visual sampler with N/E/S/W neighbor screen stitching."""

    def __init__(self, maps, attrib: AttribFile, screen: int) -> None:
        self.screen = screen
        rec = attrib.records[screen]
        self.center = maps[screen][0]
        self.north = self._neighbor(maps, rec.neighbors[0])
        self.east = self._neighbor(maps, rec.neighbors[1])
        self.south = self._neighbor(maps, rec.neighbors[2])
        self.west = self._neighbor(maps, rec.neighbors[3])

    @staticmethod
    def _neighbor(maps, idx: int) -> bytes:
        if 0 <= idx < MAP_SCREENS:
            return maps[idx][0]
        return bytes([0] * MAP_PAGE_SIZE)

    def at(self, x: int, y: int) -> int:
        page = self.center
        lx, ly = x, y
        if x > 0x0F and x < 0x14:
            page, lx = self.east, x - 0x10
        elif x >= 0xFC:
            page, lx = self.west, x - 0xF0
        if y >= MAP_GRID:
            page, ly = self.north, y - MAP_GRID
        elif y < 0:
            page, ly = self.south, y + MAP_GRID
        if lx < 0 or ly < 0 or lx >= MAP_GRID or ly >= MAP_GRID:
            return 0
        return page[(ly << 4) | lx]


class LocalVisual:
    """Single-screen page-0 sampler (no neighbor stitching)."""

    def __init__(self, visual: bytearray) -> None:
        self.visual = visual

    def at(self, x: int, y: int) -> int:
        if x < 0 or y < 0 or x >= MAP_GRID or y >= MAP_GRID:
            return 0
        return self.visual[(y << 4) | x]


def _set_dir(cell: int, d: int, code: int) -> int:
    shift = DIR_PAIR_SHIFT[d & 3]
    cell &= ~(0x03 << shift)
    cell |= (code & 0x03) << shift
    return cell


def _get_dir(cell: int, d: int) -> int:
    return (cell >> DIR_PAIR_SHIFT[d & 3]) & 0x03


def build_sandbox_visual() -> bytearray:
    """16x16 blank map with only outer boundary walls."""
    visual = bytearray([0] * MAP_PAGE_SIZE)
    for y in range(MAP_GRID):
        for x in range(MAP_GRID):
            idx = (y << 4) | x
            c = visual[idx]
            if y == MAP_GRID - 1:
                c = _set_dir(c, 0, 1)  # north edge
            if x == MAP_GRID - 1:
                c = _set_dir(c, 1, 1)  # east edge
            if y == 0:
                c = _set_dir(c, 2, 1)  # south edge
            if x == 0:
                c = _set_dir(c, 3, 1)  # west edge
            visual[idx] = c
    return visual


def place_wall_ahead(visual: bytearray, x: int, y: int, facing: int, code: int = 1) -> None:
    """Set the edge in front of party; mirror to adjacent tile when in-bounds."""
    idx = (y << 4) | x
    visual[idx] = _set_dir(visual[idx], facing, code)
    nx = x + STEP_DX[facing & 3]
    ny = y + STEP_DY[facing & 3]
    if 0 <= nx < MAP_GRID and 0 <= ny < MAP_GRID:
        nidx = (ny << 4) | nx
        visual[nidx] = _set_dir(visual[nidx], (facing + 2) & 3, code)


def step_sandbox(visual: bytearray, facing: int, x: int, y: int) -> Tuple[int, int]:
    """Move one tile if the forward edge is open; stay in-bounds."""
    idx = (y << 4) | x
    if _get_dir(visual[idx], facing) != 0:
        return x, y
    nx = x + STEP_DX[facing & 3]
    ny = y + STEP_DY[facing & 3]
    if nx < 0 or ny < 0 or nx >= MAP_GRID or ny >= MAP_GRID:
        return x, y
    return nx, ny


def refresh_hood(grid: StitchedVisual, px: int, py: int, facing: int) -> List[int]:
    """ASM map_neighbourhood_refresh sampling pattern (3x5 overlap = 13 bytes)."""
    hood = [0] * 13
    b = BUNDLES[facing & 3]
    dx, dy = _sb(b, 0), _sb(b, 1)

    def row(sx: int, sy: int, out_off: int) -> None:
        x, y = sx, sy
        for i in range(5):
            hood[out_off + i] = grid.at(x, y)
            x += dx
            y += dy

    row(px, py, 0)
    row(px + _sb(b, 2), py + _sb(b, 3), 4)
    row(px + _sb(b, 4), py + _sb(b, 5), 8)
    return hood


def build_scene(grid: StitchedVisual, x: int, y: int, facing: int) -> View3DScene:
    scene = View3DScene()
    scene.hood = refresh_hood(grid, x, y, facing)
    scene.slots = build_frustum(scene.hood, set_facing(facing))
    scene.blits = collect_blits(scene.slots)
    return scene


def select_sheets(attrib: AttribFile, screen: int) -> SheetSet:
    env = attrib.records[screen].env_name
    if env == "cavern":
        return SheetSet("cave.32", "cavef.32")
    if env == "castle":
        return SheetSet("castle.32", "castlef.32")
    return SheetSet("town.32", "townf.32")


def carto_frame(screen: int, visual: int, outdoor: bool) -> int:
    if screen == 41:
        return 8
    if screen in (42, 43):
        return 4
    if screen == 44:
        return 5
    if outdoor:
        return visual & 0x1F
    return K_CARTO_TILE[(visual >> 2) & 0x3F]


def render_minimap(visual: bytes, collision: bytes, x: int, y: int, facing: int, *, screen: int,
                   data_dir: Path, outdoor: bool) -> Image.Image:
    tw, th = 14, 11
    w, h = MAP_GRID * tw, MAP_GRID * th
    img = Image.new("RGBA", (w, h), (20, 20, 30, 255))
    d = ImageDraw.Draw(img)

    sheet = "outb.32" if outdoor else "townb.32"
    cache: Dict[int, Image.Image] = {}

    def tile(frame: int) -> Image.Image | None:
        if frame in cache:
            return cache[frame]
        try:
            cache[frame] = load_frame(sheet, frame, data_dir)
            return cache[frame]
        except Exception:
            return None

    for sy in range(MAP_GRID):
        disk_y = MAP_GRID - 1 - sy
        for sx in range(MAP_GRID):
            idx = (disk_y << 4) | sx
            px, py = sx * tw, sy * th
            d.rectangle((px, py, px + tw - 1, py + th - 1), fill=(68, 68, 80, 255))
            spr = tile(carto_frame(screen, visual[idx], outdoor))
            if spr is not None:
                blit(img, spr, px, py)
            if collision[idx] & 0x80:
                d.ellipse((px + tw // 2 - 1, py + th // 2 - 1, px + tw // 2 + 1, py + th // 2 + 1),
                          fill=(255, 65, 65, 220))

    px = x * tw
    py = (MAP_GRID - 1 - y) * th
    d.rectangle((px, py, px + tw - 1, py + th - 1), outline=(255, 220, 0, 210), width=1)
    d.rectangle((px + 1, py + 1, px + tw - 2, py + th - 2), fill=(255, 220, 0, 55))
    dir_frame = (0x20, 0x22, 0x21, 0x23)[facing & 3]
    arr = tile(dir_frame)
    if arr is not None:
        blit(img, arr, px, py)
    d.rectangle((0, 0, w - 1, h - 1), outline=(120, 120, 135, 255), width=1)
    return img


def render_value_minimap(visual: bytes, x: int, y: int, facing: int, *, code: int) -> Image.Image:
    """5x3 hood debug panel: draws edges where field code matches."""
    cw, ch, pad = 20, 20, 6
    w, h = pad * 2 + 5 * cw, pad * 2 + 3 * ch
    img = Image.new("RGBA", (w, h), (20, 20, 30, 255))
    d = ImageDraw.Draw(img)
    edge_col = ((255, 120, 120, 255), (120, 255, 120, 255), (120, 180, 255, 255), (255, 220, 120, 255))

    b = BUNDLES[facing & 3]
    dx, dy = _sb(b, 0), _sb(b, 1)
    starts = ((x, y), (x + _sb(b, 3), y + _sb(b, 2)), (x + _sb(b, 5), y + _sb(b, 4)))

    for ry in range(3):
        sx0, sy0 = starts[ry]
        for rx in range(5):
            mx, my = sx0 + rx * dx, sy0 + rx * dy
            v = visual[((my & 0x0F) << 4) | (mx & 0x0F)]
            px, py = pad + rx * cw, pad + ry * ch
            d.rectangle((px, py, px + cw - 1, py + ch - 1), fill=(66, 66, 80, 255))
            n, e, s, wv = _get_dir(v, 0), _get_dir(v, 1), _get_dir(v, 2), _get_dir(v, 3)
            if n == code:
                d.line((px, py, px + cw - 1, py), fill=edge_col[0], width=2)
            if e == code:
                d.line((px + cw - 1, py, px + cw - 1, py + ch - 1), fill=edge_col[1], width=2)
            if s == code:
                d.line((px, py + ch - 1, px + cw - 1, py + ch - 1), fill=edge_col[2], width=2)
            if wv == code:
                d.line((px, py, px, py + ch - 1), fill=edge_col[3], width=2)
            if (mx & 0x0F) == x and (my & 0x0F) == y:
                d.rectangle((px + 2, py + 2, px + cw - 3, py + ch - 3), outline=(255, 220, 0, 220), width=1)
    d.rectangle((0, 0, w - 1, h - 1), outline=(120, 120, 135, 255), width=1)
    return img


def compose_with_minimap(
    view: Image.Image,
    minimap: Image.Image,
    *,
    map_name: str,
    screen: int,
    x: int,
    y: int,
    facing: int,
) -> Image.Image:
    """Single minimap pane (view3d_outdoor)."""
    pad = 8
    label_h = 50
    out_w = view.width + pad + minimap.width + pad * 2
    out_h = max(view.height, minimap.height + label_h) + pad * 2
    out = Image.new("RGBA", (out_w, out_h), (10, 10, 16, 255))
    out.paste(view, (pad, pad), view)
    mx = pad + view.width + pad
    my = pad + label_h
    out.paste(minimap, (mx, my), minimap)
    d = ImageDraw.Draw(out)
    d.text((mx, pad), map_name, fill=(220, 220, 235, 255))
    d.text((mx, pad + 12), f"Screen: {screen}", fill=(180, 180, 200, 255))
    d.text((mx, pad + 24), f"Facing: {FACE[facing & 3]}", fill=(180, 180, 200, 255))
    d.text(
        (mx, pad + 36),
        f"Tile: {tile_notation(x, y)}  ({x},{y})",
        fill=(180, 180, 200, 255),
    )
    return out


def compose_with_panel(view: Image.Image, n0: Image.Image, c1: Image.Image, c2: Image.Image, c3: Image.Image,
                       *, map_name: str, screen: int, x: int, y: int, facing: int,
                       slots: Sequence[int]) -> Image.Image:
    pad, label_h, gap = 8, 50, 6
    cell_w, cell_h = 112, 88
    panel_w = cell_w * 2 + gap
    panel_h = cell_h * 2 + gap
    out_w = view.width + pad + panel_w + pad * 2
    out_h = max(view.height, panel_h + label_h) + pad * 2
    out = Image.new("RGBA", (out_w, out_h), (10, 10, 16, 255))
    out.paste(view, (pad, pad), view)
    mx, my = pad + view.width + pad, pad + label_h

    def sm(img: Image.Image) -> Image.Image:
        return img.resize((112, 88), Image.NEAREST)

    out.paste(sm(n0), (mx, my), sm(n0))
    out.paste(sm(c1), (mx + cell_w + gap, my), sm(c1))
    out.paste(sm(c2), (mx, my + cell_h + gap), sm(c2))
    out.paste(sm(c3), (mx + cell_w + gap, my + cell_h + gap), sm(c3))

    d = ImageDraw.Draw(out)
    d.text((mx, pad), map_name, fill=(220, 220, 235, 255))
    d.text((mx, pad + 12), f"Screen: {screen}", fill=(180, 180, 200, 255))
    d.text((mx, pad + 24), f"Facing: {FACE[facing & 3]}", fill=(180, 180, 200, 255))
    d.text((mx, pad + 36), f"Tile: {tile_notation(x, y)}  ({x},{y})", fill=(180, 180, 200, 255))
    d.text((mx + 2, my + 2), "normal", fill=(220, 220, 230, 255))
    d.text((mx + cell_w + gap + 2, my + 2), "code=1", fill=(220, 220, 230, 255))
    d.text((mx + 2, my + cell_h + gap + 2), "code=2 door", fill=(220, 220, 230, 255))
    d.text((mx + cell_w + gap + 2, my + cell_h + gap + 2), "code=3", fill=(220, 220, 230, 255))
    nz = [f"{SLOT_NAMES[i]}={v}" for i, v in enumerate(slots) if v]
    d.text((mx, out_h - pad - 22), "Slots:", fill=(180, 180, 200, 255))
    d.text((mx + 40, out_h - pad - 22), " ".join(nz[:4]), fill=(180, 180, 200, 255))
    d.text((mx + 40, out_h - pad - 10), " ".join(nz[4:8]), fill=(180, 180, 200, 255))
    return out


def render_indoor_view(scene: View3DScene, sheets: SheetSet, data_dir: Path, *, visual: bytes,
                       collision: bytes, x: int, y: int, facing: int, screen: int,
                       map_name: str, with_minimap: bool, render_mode: str) -> Image.Image:
    canvas = composite_backdrop(sheets.floor, sheets.sky, data_dir)
    cache: Dict[Tuple[str, int], Image.Image] = {}

    def sprite(sheet: str, frame: int) -> Image.Image:
        key = (sheet, frame)
        if key not in cache:
            cache[key] = load_frame(sheet, frame, data_dir)
        return cache[key]

    draw_blits = scene.blits
    if render_mode == "doors":
        door_slots = [0] * len(scene.slots)
        for i, v in enumerate(scene.slots):
            if v == 2:
                door_slots[i] = 2
        raw = collect_blits(door_slots)
        draw_blits = []
        for b in raw:
            if b.kind == "front" and 0 <= b.frame <= 3:
                continue
            draw_blits.append(b)

    for b in draw_blits:
        blit(canvas, sprite(sheets.walls, b.frame), b.x, b.y)

    if with_minimap:
        normal = render_minimap(visual, collision, x, y, facing, screen=screen, data_dir=data_dir, outdoor=False)
        m1 = render_value_minimap(visual, x, y, facing, code=1)
        m2 = render_value_minimap(visual, x, y, facing, code=2)
        m3 = render_value_minimap(visual, x, y, facing, code=3)
        return compose_with_panel(canvas, normal, m1, m2, m3, map_name=map_name, screen=screen,
                                  x=x, y=y, facing=facing, slots=scene.slots)
    return canvas


def dump_trace(scene: View3DScene, screen: int, x: int, y: int, facing: int, attrib: AttribFile) -> None:
    loc = location_label_from_attrib(screen, x, y, attrib.records[screen])
    print(f"=== {loc}  screen {screen}  ({x},{y})  facing {FACE[facing & 3]} ===")
    print("hood (page-0 visual):")
    for i, v in enumerate(scene.hood):
        print(f"  [{i:2d}] 0x{v:02X}  {describe_nibbles(v)}")
    print("frustum slots:")
    for i, v in enumerate(scene.slots):
        if v:
            print(f"  -${SLOT_NAMES[i]} = {v}")
    print(f"blits: {len(scene.blits)}")
    for b in scene.blits:
        print(f"  {b.kind:5s} depth={b.depth} frame={b.frame} @ ({b.x},{b.y})")


def step_party(facing: int, x: int, y: int, screen: int, attrib: AttribFile) -> Tuple[int, int, int]:
    x += STEP_DX[facing & 3]
    y += STEP_DY[facing & 3]
    rec = attrib.records[screen]
    if x < 0:
        n = rec.neighbors[3]
        if 0 <= n < MAP_SCREENS:
            screen, x = n, 15
    elif x >= MAP_GRID:
        n = rec.neighbors[1]
        if 0 <= n < MAP_SCREENS:
            screen, x = n, 0
    if y < 0:
        n = rec.neighbors[2]
        if 0 <= n < MAP_SCREENS:
            screen, y = n, 15
    elif y >= MAP_GRID:
        n = rec.neighbors[0]
        if 0 <= n < MAP_SCREENS:
            screen, y = n, 0
    return screen, max(0, min(15, x)), max(0, min(15, y))


def run_interactive(maps, attrib: AttribFile, screen: int, x: int, y: int, facing: int, data_dir: Path,
                    scale: int, with_minimap: bool, sandbox: bool) -> int:
    try:
        import pygame
    except ImportError:
        print("pip install pygame for --interactive", file=sys.stderr)
        return 1

    pygame.init()
    sandbox_visual = build_sandbox_visual() if sandbox else None
    grid0 = LocalVisual(sandbox_visual) if sandbox else StitchedVisual(maps, attrib, screen)
    scene0 = build_scene(grid0, x, y, facing)
    vis0 = bytes(sandbox_visual) if sandbox else maps[screen][0]
    col0 = bytes([0] * MAP_PAGE_SIZE) if sandbox else maps[screen][1]
    map_name0 = "Sandbox" if sandbox else area_name(screen)
    sample = render_indoor_view(scene0, select_sheets(attrib, screen), data_dir,
                                visual=vis0, collision=col0,
                                x=x, y=y, facing=facing, screen=screen, map_name=map_name0,
                                with_minimap=with_minimap, render_mode="all")
    w, h = sample.width * scale, sample.height * scale
    win = pygame.display.set_mode((w, h))
    clock = pygame.time.Clock()
    cur_screen, cx, cy, cf = screen, x, y, facing
    render_mode = "all"

    def redraw():
        grid = LocalVisual(sandbox_visual) if sandbox else StitchedVisual(maps, attrib, cur_screen)
        scene = build_scene(grid, cx, cy, cf)
        sheets = select_sheets(attrib, cur_screen)
        visual_page = bytes(sandbox_visual) if sandbox else maps[cur_screen][0]
        collision_page = bytes([0] * MAP_PAGE_SIZE) if sandbox else maps[cur_screen][1]
        map_name = "Sandbox" if sandbox else area_name(cur_screen)
        img = render_indoor_view(scene, sheets, data_dir, visual=visual_page, collision=collision_page,
                                 x=cx, y=cy, facing=cf, screen=cur_screen, map_name=map_name,
                                 with_minimap=with_minimap, render_mode=render_mode)
        loc = f"sandbox ({cx},{cy})" if sandbox else location_label_from_attrib(
            cur_screen, cx, cy, attrib.records[cur_screen]
        )
        door_count = sum(1 for v in scene.slots if v == 2)
        pygame.display.set_caption(
            f"MM2 indoor | {loc} | screen {cur_screen} | {FACE[cf]} | "
            f"{sheets.walls} | mode={render_mode} doorSlots={door_count} "
            f"(A=all,D=doors,Space=place wall,N=next map,P=prev map)"
        )
        surf = pygame.image.fromstring(img.tobytes(), img.size, img.mode)
        return pygame.transform.scale(surf, (w, h))

    frame = redraw()
    running = True
    while running:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            elif ev.type == pygame.KEYDOWN:
                if ev.key in (pygame.K_ESCAPE, pygame.K_q):
                    running = False
                elif ev.key == pygame.K_LEFT:
                    cf = (cf + 3) & 3
                elif ev.key == pygame.K_RIGHT:
                    cf = (cf + 1) & 3
                elif ev.key == pygame.K_UP:
                    if sandbox:
                        cx, cy = step_sandbox(sandbox_visual, cf, cx, cy)
                    else:
                        cur_screen, cx, cy = step_party(cf, cx, cy, cur_screen, attrib)
                elif ev.key == pygame.K_DOWN:
                    if sandbox:
                        cx, cy = step_sandbox(sandbox_visual, (cf + 2) & 3, cx, cy)
                    else:
                        cur_screen, cx, cy = step_party((cf + 2) & 3, cx, cy, cur_screen, attrib)
                elif ev.key == pygame.K_d:
                    render_mode = "doors"
                elif ev.key == pygame.K_a:
                    render_mode = "all"
                elif ev.key == pygame.K_n and not sandbox:
                    cur_screen = (cur_screen + 1) % MAP_SCREENS
                elif ev.key == pygame.K_p and not sandbox:
                    cur_screen = (cur_screen - 1) % MAP_SCREENS
                elif ev.key == pygame.K_SPACE and sandbox:
                    place_wall_ahead(sandbox_visual, cx, cy, cf, code=1)
                frame = redraw()
        win.blit(frame, (0, 0))
        pygame.display.flip()
        clock.tick(30)
    pygame.quit()
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("map_dat", type=Path)
    ap.add_argument("attrib_dat", type=Path)
    ap.add_argument("screen", type=int)
    ap.add_argument("x", type=int)
    ap.add_argument("y", type=int)
    ap.add_argument("facing", type=int, choices=range(4))
    ap.add_argument("--data", type=Path, default=ROOT, help=".32 directory")
    ap.add_argument("--out", type=Path, help="write PNG")
    ap.add_argument("--trace", action="store_true", help="print hood/slots/blits")
    ap.add_argument("--interactive", action="store_true", help="pygame live viewer")
    ap.add_argument("--scale", type=int, default=3, help="interactive scale")
    ap.add_argument("--no-minimap", action="store_true", help="render view only")
    ap.add_argument("--sandbox", action="store_true", help="blank map with boundary walls; Space places wall ahead")
    args = ap.parse_args(argv)

    maps = load_map(args.map_dat)
    attrib = AttribFile.load(str(args.attrib_dat))
    if not (0 <= args.screen < MAP_SCREENS):
        print(f"screen must be 0..{MAP_SCREENS - 1}", file=sys.stderr)
        return 1

    if args.interactive:
        return run_interactive(maps, attrib, args.screen, args.x, args.y, args.facing,
                               args.data, args.scale, with_minimap=not args.no_minimap, sandbox=args.sandbox)

    sandbox_visual = build_sandbox_visual() if args.sandbox else None
    grid = LocalVisual(sandbox_visual) if args.sandbox else StitchedVisual(maps, attrib, args.screen)
    scene = build_scene(grid, args.x, args.y, args.facing)
    sheets = select_sheets(attrib, args.screen)
    if args.trace:
        dump_trace(scene, args.screen, args.x, args.y, args.facing, attrib)
        print(f"sheets: walls={sheets.walls} floor={sheets.floor} sky={sheets.sky}")

    visual_page = bytes(sandbox_visual) if args.sandbox else maps[args.screen][0]
    collision_page = bytes([0] * MAP_PAGE_SIZE) if args.sandbox else maps[args.screen][1]
    map_name = "Sandbox" if args.sandbox else area_name(args.screen)
    img = render_indoor_view(scene, sheets, args.data, visual=visual_page, collision=collision_page,
                             x=args.x, y=args.y, facing=args.facing, screen=args.screen, map_name=map_name,
                             with_minimap=not args.no_minimap, render_mode="all")
    out = args.out or (ROOT / "EXTRACTED" / "docs" / "img" / "_view3d_indoor.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

