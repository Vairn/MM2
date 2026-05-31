#!/usr/bin/env python3
"""Outdoor first-person map viewer (ports outdoor_3d_master @0x18870).

Pipeline (see EXTRACTED/docs/15-3d-view-and-game-screen.md):
  1. Sample three map page-0 rows -> -$55D4 / -$55D0 / -$55CC (hood walk)
  2. @0x9544 terrain lookup -> -$55C6 / -$55C2 / -$55BE
  3. @0x182D8 floor decor on biome sheet (-$7A0A)
  4. @0x1877A horizon blits (L1/L2/L3) from outdoor1/2/3.32

Usage:
  python tools/view3d_outdoor.py map.dat attrib.dat <screen> <x> <y> <facing>
  python tools/view3d_outdoor.py map.dat attrib.dat 11 8 8 0 --out view.png
  python tools/view3d_outdoor.py map.dat attrib.dat 11 8 8 0 --interactive

Facing: 0=N 1=E 2=S 3=W. Requires Pillow; pygame optional for --interactive.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Sequence, Tuple

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

# Reuse verified viewport tables and .32 decoder from render_view_refs.
_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from attrib_codec import AttribFile, AttribRecord  # noqa: E402
from mm2_map_ui import area_name, location_label_from_attrib, tile_notation  # noqa: E402
from view3d_indoor import render_minimap  # noqa: E402
from render_view_refs import (  # noqa: E402
    FRAMES,
    FLOOR_Y,
    ORIGIN_X,
    OUTDOOR_DECOR_X,
    OUTDOOR_DECOR_X_112,
    OUTDOOR_DECOR_X_BDE,
    OUTDOOR_DECOR_Y,
    OUTDOOR_FLOOR_SHEETS,
    OUTDOOR_L1_FRAME,
    OUTDOOR_L1_X,
    OUTDOOR_L1_Y,
    OUTDOOR_L2_FRAME,
    OUTDOOR_L2_X,
    OUTDOOR_L2_Y,
    OUTDOOR_L3_FRAME,
    OUTDOOR_L3_X,
    OUTDOOR_L3_Y,
    VIEW_H,
    SKY_Y,
    VIEW_W,
    blit,
    load_frame,
    load_ppm,
)
from view3d_trace import (  # noqa: E402
    BUNDLE_E,
    BUNDLE_N,
    BUNDLE_S,
    BUNDLE_W,
    MAP_GRID,
    MAP_PAGE_SIZE,
    MAP_SCREENS,
    MAP_SCREEN_SIZE,
    load_map,
)

ROOT = Path(__file__).resolve().parents[1]
DATA_BIN = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
A4 = 0x7FFE

HORIZON_SHEETS = ("outdoor1.32", "outdoor2.32", "outdoor3.32")

# Map page-0 terrain id (byte & $1F): ids 2/3 are water on ocean and coastal maps.
MAP_TERRAIN_ID_BIOME = {
    2: "ocean.32",
    3: "ocean.32",
}

# World sector from attrib +0x15 (e.g. 0xC3 → "C3"). Not the same as surface_flag 0xAA/0xCC.
SECTOR_LABEL_BIOME: dict[int, str] = {
    0xC3: "ocean.32",
}
BIOME_BY_TILESET = {
    0x03: "desert.32",
    0x04: "ocean.32",
    0x06: "tundra.32",
}
BIOME_BY_AREA = {
    41: "swamp.32",
    42: "swamp.32",
    43: "swamp.32",
    44: "swamp.32",
}

BUNDLES = (BUNDLE_N, BUNDLE_E, BUNDLE_S, BUNDLE_W)
FACE = ("N", "E", "S", "W")
STEP_DX = (0, 1, 0, -1)
STEP_DY = (1, 0, -1, 0)


@dataclass
class SpriteBlit:
    sheet: str
    frame: int
    x: int
    y: int


@dataclass
class OutdoorScene:
    rows_raw: Tuple[List[int], List[int], List[int]] = field(
        default_factory=lambda: ([0] * 5, [0] * 5, [0] * 5)
    )
    lane_c6: List[int] = field(default_factory=lambda: [0] * 5)
    lane_c2: List[int] = field(default_factory=lambda: [0] * 5)
    lane_be: List[int] = field(default_factory=lambda: [0] * 5)
    column_biomes: List[str] = field(default_factory=list)
    decor: List[SpriteBlit] = field(default_factory=list)
    horizon: List[SpriteBlit] = field(default_factory=list)


def _read_table_u8(blob: bytes, a4off: int, n: int) -> bytes:
    off = A4 - a4off
    return blob[off : off + n]


def load_terrain_lookup() -> bytes:
    blob = DATA_BIN.read_bytes()
    return _read_table_u8(blob, 0x720A, 256)


_TERRAIN_LOOKUP = None


def terrain_lookup() -> bytes:
    global _TERRAIN_LOOKUP
    if _TERRAIN_LOOKUP is None:
        _TERRAIN_LOOKUP = load_terrain_lookup()
    return _TERRAIN_LOOKUP


def terrain_class(map_byte: int) -> int:
    return terrain_lookup()[map_byte & 0x1F]


class StitchedMap:
    """Center + N/E/S/W visual pages; supports hood steps with x/y < 0 or >= 16."""

    def __init__(
        self,
        maps: Sequence[Tuple[bytes, bytes]],
        screen: int,
        attrib: AttribFile,
    ) -> None:
        self.screen = screen
        self._attrib = attrib
        rec = attrib.records[screen]
        self.neighbor_screen = [
            self._neighbor_id(rec, slot) for slot in range(4)
        ]
        self.page_by_screen: dict[int, bytes] = {screen: maps[screen][0]}
        for slot in range(4):
            n = self.neighbor_screen[slot]
            if 0 <= n < MAP_SCREENS:
                self.page_by_screen[n] = maps[n][0]

    @staticmethod
    def _neighbor_id(rec: AttribRecord, slot: int) -> int:
        n = rec.neighbors[slot]
        return n if 0 <= n < MAP_SCREENS else rec.area_id

    def _neighbor_of(self, screen: int, slot: int) -> int:
        n = self._attrib.records[screen].neighbors[slot]
        return n if 0 <= n < MAP_SCREENS else screen

    def _resolve(self, x: int, y: int) -> Tuple[bytes, int, int, int]:
        screen = self.screen
        lx, ly = x, y
        if lx < 0:
            screen, lx = self._neighbor_of(screen, 3), lx + MAP_GRID
        elif lx >= MAP_GRID:
            screen, lx = self._neighbor_of(screen, 1), lx - MAP_GRID
        # Map y: 0 = south row, 15 = north row (matches minimap).
        if ly < 0:
            screen, ly = self._neighbor_of(screen, 2), ly + MAP_GRID
        elif ly >= MAP_GRID:
            screen, ly = self._neighbor_of(screen, 0), ly - MAP_GRID
        page = self.page_by_screen.get(screen, bytes(MAP_PAGE_SIZE))
        return page, screen, lx, ly

    def screen_id_at(self, x: int, y: int) -> int:
        return self._resolve(x, y)[1]

    def at(self, x: int, y: int) -> int:
        page, _screen, lx, ly = self._resolve(x, y)
        if lx < 0 or ly < 0 or lx >= MAP_GRID or ly >= MAP_GRID:
            return 0
        return page[(ly << 4) | lx]


def _sb(bundle: Sequence[int], i: int) -> int:
    return int.from_bytes(bytes([bundle[i] & 0xFF]), "big", signed=True)


def refresh_outdoor_hood(
    grid: StitchedMap, px: int, py: int, facing: int
) -> List[int]:
    """13-byte hood like map_neighbourhood_refresh @0x19D6 / View3D.cpp."""
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
    row(px + _sb(b, 3), py + _sb(b, 2), 4)
    row(px + _sb(b, 5), py + _sb(b, 4), 8)
    return hood


def refresh_outdoor_rows(
    grid: StitchedMap, px: int, py: int, facing: int
) -> Tuple[List[int], List[int], List[int]]:
    """-$55D4 / -$55D0 / -$55CC slices (5 bytes each) from hood."""
    hood = refresh_outdoor_hood(grid, px, py, facing)
    return hood[0:5], hood[4:9], hood[8:13]


def process_terrain_rows(
    rows: Tuple[List[int], List[int], List[int]],
) -> Tuple[List[int], List[int], List[int]]:
    """@9544: kLookup[map_byte & $1F] into -$55C6 / -$55C2 / -$55BE (no bad mask hack)."""
    lookup = terrain_lookup()
    c6 = [lookup[b & 0x1F] for b in rows[0]]
    c2 = [lookup[b & 0x1F] for b in rows[1]]
    be = [lookup[b & 0x1F] for b in rows[2]]
    return c6, c2, be


def build_sector_label_biomes(attrib: AttribFile) -> dict[int, str]:
    """attrib +0x15 per overland sector (A1, C3, …), keyed by label byte."""
    out = dict(SECTOR_LABEL_BIOME)
    for rec in attrib.records:
        if not rec.is_outside:
            continue
        label = rec.raw[0x15]
        if label in out:
            continue
        sf = rec.surface_flag
        if sf == 0xCC:
            out[label] = "ocean.32"
        elif sf == 0x99:
            out[label] = "tundra.32"
        elif sf == 0xBB:
            out[label] = "swamp.32"
        else:
            out[label] = "desert.32"
    return out


def biome_for_cell(
    map_byte: int, sector_label: int, sector_table: dict[int, str]
) -> str:
    """Floor decor sheet for one hood cell: water ids first, then sector label."""
    tid = map_byte & 0x1F
    if tid in MAP_TERRAIN_ID_BIOME:
        return MAP_TERRAIN_ID_BIOME[tid]
    return sector_table.get(sector_label, OUTDOOR_FLOOR_SHEETS[0])


def biome_for_record(rec: AttribRecord, sector_table: dict[int, str] | None = None) -> str:
    if rec.area_id in BIOME_BY_AREA:
        return BIOME_BY_AREA[rec.area_id]
    if sector_table is not None:
        return sector_table.get(rec.raw[0x15], OUTDOOR_FLOOR_SHEETS[0])
    sf = rec.surface_flag
    if sf == 0xCC:
        return "ocean.32"
    if sf == 0x99:
        return "tundra.32"
    if sf == 0xBB:
        return "swamp.32"
    return BIOME_BY_TILESET.get(rec.tileset, OUTDOOR_FLOOR_SHEETS[0])


def column_biomes(
    grid: StitchedMap,
    px: int,
    py: int,
    facing: int,
    attrib: AttribFile,
    sector_table: dict[int, str],
) -> List[str]:
    """Per decor column: biome from map terrain id, then world sector (+0x15)."""
    b = BUNDLES[facing & 3]
    dx, dy = _sb(b, 0), _sb(b, 1)
    out: List[str] = []
    for col in range(4):
        wx, wy = px + col * dx, py + col * dy
        sid = grid.screen_id_at(wx, wy)
        cell = grid.at(wx, wy)
        label = attrib.records[sid].raw[0x15]
        out.append(biome_for_cell(cell, label, sector_table))
    return out


def build_decor_blits(
    c6: Sequence[int],
    lane_l2: Sequence[int],
    lane_l3: Sequence[int],
    col_biomes: Sequence[str],
) -> List[SpriteBlit]:
    """@182D8 floor decor on -$7A0A (four columns, biome per zone).

    Paint far → near: column 3 (y=68) first, column 0 (y=108) last.
    """
    blits: List[SpriteBlit] = []
    for col in range(3, -1, -1):
        biome = col_biomes[col] if col < len(col_biomes) else OUTDOOR_FLOOR_SHEETS[0]
        main_a = c6[col] > 3
        main_b = lane_l2[col] > 3
        main_c = lane_l3[col] > 3
        y = OUTDOOR_DECOR_Y[col]
        if main_a:
            blits.append(SpriteBlit(biome, col, OUTDOOR_DECOR_X, y))
            if main_b:
                blits.append(SpriteBlit(biome, col + 12, OUTDOOR_DECOR_X, y))
            if main_c:
                blits.append(
                    SpriteBlit(
                        biome,
                        col + 16,
                        OUTDOOR_DECOR_X_BDE[col] - ORIGIN_X,
                        y,
                    )
                )
        else:
            if main_b:
                blits.append(SpriteBlit(biome, col + 4, OUTDOOR_DECOR_X, y))
            if main_c:
                blits.append(
                    SpriteBlit(biome, col + 8, OUTDOOR_DECOR_X_112, y)
                )
    return blits


def _horizon_index(terrain_class: int) -> int:
    """@1877A: (class - 1) -> -$7A16 outdoor1/2/3. Class 4 = open plain, no layer."""
    if terrain_class <= 0 or terrain_class > 3:
        return 0xFF
    return terrain_class - 1


def _horizon_sprite(idx: int, frame: int, x: int, y: int) -> SpriteBlit | None:
    if idx == 0xFF:
        return None
    return SpriteBlit(HORIZON_SHEETS[idx], frame, x, y)


def _horizon_start_col(l1: Sequence[int]) -> int:
    """@1877A: first column with valid L1 terrain, else 3."""
    for col in range(4):
        if l1[col] != 0xFF:
            return col
    return 3


def build_horizon_blits(
    c6: Sequence[int],
    lane_l2: Sequence[int],
    lane_l3: Sequence[int],
) -> List[SpriteBlit]:
    """@1877A order: L1 once, then L2/L3. lane_l2=left coords, lane_l3=right."""
    l1 = [_horizon_index(c6[c]) for c in range(4)]
    l2 = [_horizon_index(lane_l2[c]) for c in range(4)]
    l3 = [_horizon_index(lane_l3[c]) for c in range(4)]
    start = _horizon_start_col(l1)

    blits: List[SpriteBlit] = []
    if l1[start] != 0xFF:
        b = _horizon_sprite(
            l1[start],
            OUTDOOR_L1_FRAME[start],
            OUTDOOR_L1_X[start],
            OUTDOOR_L1_Y[start],
        )
        if b is not None:
            blits.append(b)

    seen_l2: set[int] = set()
    seen_l3: set[int] = set()
    for col in range(3, -1, -1):
        if l2[col] != 0xFF and l2[col] not in seen_l2:
            seen_l2.add(l2[col])
            b = _horizon_sprite(
                l2[col], OUTDOOR_L2_FRAME[col], OUTDOOR_L2_X[col], OUTDOOR_L2_Y[col]
            )
            if b is not None:
                blits.append(b)

    for col in range(3, -1, -1):
        if l3[col] != 0xFF and l3[col] not in seen_l3:
            seen_l3.add(l3[col])
            b = _horizon_sprite(
                l3[col], OUTDOOR_L3_FRAME[col], OUTDOOR_L3_X[col], OUTDOOR_L3_Y[col]
            )
            if b is not None:
                blits.append(b)

    return blits


def build_outdoor_scene(
    grid: StitchedMap,
    px: int,
    py: int,
    facing: int,
    attrib: AttribFile,
) -> OutdoorScene:
    rows = refresh_outdoor_rows(grid, px, py, facing)
    c6, lane_d0, lane_cc = process_terrain_rows(rows)
    # ASM: -$55D0 -> L2 (left), -$55CC -> L3 (right). View mirror: swap at paint.
    lane_l2, lane_l3 = lane_cc, lane_d0
    sector_table = build_sector_label_biomes(attrib)
    col_bio = column_biomes(grid, px, py, facing, attrib, sector_table)
    scene = OutdoorScene(
        rows_raw=rows,
        lane_c6=c6,
        lane_c2=list(lane_l2),
        lane_be=list(lane_l3),
        column_biomes=col_bio,
    )
    scene.decor = build_decor_blits(c6, lane_l2, lane_l3, col_bio)
    scene.horizon = build_horizon_blits(c6, lane_l2, lane_l3)
    return scene


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
    """Single minimap pane (kept here so indoor layout changes cannot break outdoor)."""
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


def load_sprite(sheet: str, frame: int, data_dir: Path) -> Image.Image:
    if (data_dir / sheet).is_file():
        return load_frame(sheet, frame, data_dir)
    ppm = FRAMES / f"{sheet}_f{frame:02d}.ppm"
    if ppm.is_file():
        return load_ppm(ppm)
    raise FileNotFoundError(f"no art for {sheet} frame {frame} (check --data or {FRAMES})")


def draw_location_hud(
    img: Image.Image,
    *,
    screen: int,
    x: int,
    y: int,
    facing: int,
    attrib: AttribFile,
) -> Image.Image:
    """Overlay map sector / tile (e.g. C2/i9) and facing."""
    rec = attrib.records[screen]
    loc = location_label_from_attrib(screen, x, y, rec)
    out = img.copy()
    d = ImageDraw.Draw(out)
    bar_h = 14
    d.rectangle((0, VIEW_H - bar_h, VIEW_W, VIEW_H), fill=(0, 0, 0, 200))
    d.text(
        (4, VIEW_H - bar_h + 1),
        f"{loc}  screen {screen}  ({x},{y})  {FACE[facing & 3]}",
        fill=(240, 240, 250, 255),
    )
    return out


def render_outdoor_view(
    scene: OutdoorScene,
    data_dir: Path,
    *,
    floor_sheet: str = "outf.32",
    sky_sheet: str = "sky.32",
    visual: bytes | None = None,
    collision: bytes | None = None,
    screen: int = 0,
    x: int = 0,
    y: int = 0,
    facing: int = 0,
    map_name: str = "",
    with_minimap: bool = True,
    attrib: AttribFile | None = None,
) -> Image.Image:
    canvas = Image.new("RGBA", (VIEW_W, VIEW_H), (0, 0, 0, 255))
    try:
        blit(canvas, load_sprite(floor_sheet, 0, data_dir), ORIGIN_X, FLOOR_Y)
        blit(canvas, load_sprite(sky_sheet, 0, data_dir), ORIGIN_X, SKY_Y)
    except OSError:
        pass
    for b in scene.decor:
        try:
            blit(canvas, load_sprite(b.sheet, b.frame, data_dir), b.x, b.y)
        except OSError:
            pass
    for b in scene.horizon:
        try:
            blit(canvas, load_sprite(b.sheet, b.frame, data_dir), b.x, b.y)
        except OSError:
            pass
    if attrib is not None and not with_minimap:
        canvas = draw_location_hud(
            canvas, screen=screen, x=x, y=y, facing=facing, attrib=attrib
        )
    if with_minimap and visual is not None and collision is not None:
        minimap = render_minimap(
            visual,
            collision,
            x,
            y,
            facing,
            screen=screen,
            data_dir=data_dir,
            outdoor=True,
        )
        label = map_name or area_name(screen)
        return compose_with_minimap(
            canvas,
            minimap,
            map_name=label,
            screen=screen,
            x=x,
            y=y,
            facing=facing,
        )
    return canvas


def dump_scene_trace(
    scene: OutdoorScene,
    screen: int,
    x: int,
    y: int,
    facing: int,
    attrib: AttribFile,
) -> None:
    rec = attrib.records[screen]
    loc = location_label_from_attrib(screen, x, y, rec)
    print(
        f"{loc}  (screen {screen}, tile {tile_notation(x, y)}, "
        f"xy=({x},{y}), facing {FACE[facing & 3]})"
    )
    print(f"  rows raw:  D4={scene.rows_raw[0]}")
    print(f"             D0={scene.rows_raw[1]}")
    print(f"             CC={scene.rows_raw[2]}")
    print(f"  terrain:   C6={scene.lane_c6[:4]} C2={scene.lane_c2[:4]} BE={scene.lane_be[:4]}")
    if scene.column_biomes:
        print(f"  biomes:    {scene.column_biomes}")
    print(f"  decor blits: {len(scene.decor)}")
    for b in scene.decor:
        print(f"    {b.sheet} f{b.frame} @ ({b.x},{b.y})")
    print(f"  horizon blits: {len(scene.horizon)}")
    for b in scene.horizon:
        print(f"    {b.sheet} f{b.frame} @ ({b.x},{b.y})")


def step_party(
    facing: int, x: int, y: int, screen: int, attrib: AttribFile
) -> Tuple[int, int, int]:
    """Move one tile; cross screen via attrib neighbours."""
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


def run_interactive(
    maps,
    attrib: AttribFile,
    screen: int,
    x: int,
    y: int,
    facing: int,
    data_dir: Path,
    scale: int,
    with_minimap: bool,
) -> int:
    try:
        import pygame
    except ImportError:
        print("pip install pygame for --interactive", file=sys.stderr)
        return 1

    pygame.init()
    grid0 = StitchedMap(maps, screen, attrib)
    scene0 = build_outdoor_scene(grid0, x, y, facing, attrib)
    loc0 = location_label_from_attrib(screen, x, y, attrib.records[screen])
    sample = render_outdoor_view(
        scene0,
        data_dir,
        visual=maps[screen][0],
        collision=maps[screen][1],
        screen=screen,
        x=x,
        y=y,
        facing=facing,
        map_name=loc0,
        with_minimap=with_minimap,
    )
    w, h = sample.width * scale, sample.height * scale
    win = pygame.display.set_mode((w, h))
    clock = pygame.time.Clock()
    cur_screen, cx, cy, cf = screen, x, y, facing

    def redraw() -> pygame.Surface:
        rec = attrib.records[cur_screen]
        if not rec.is_outside:
            surf = pygame.Surface((VIEW_W, VIEW_H))
            surf.fill((80, 0, 0))
            return pygame.transform.scale(surf, (w, h))
        grid = StitchedMap(maps, cur_screen, attrib)
        scene = build_outdoor_scene(grid, cx, cy, cf, attrib)
        loc = location_label_from_attrib(cur_screen, cx, cy, rec)
        pygame.display.set_caption(
            f"MM2 outdoor | {loc} | screen {cur_screen} | {FACE[cf & 3]}"
        )
        img = render_outdoor_view(
            scene,
            data_dir,
            visual=maps[cur_screen][0],
            collision=maps[cur_screen][1],
            screen=cur_screen,
            x=cx,
            y=cy,
            facing=cf,
            map_name=loc,
            with_minimap=with_minimap,
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
                    cur_screen, cx, cy = step_party(cf, cx, cy, cur_screen, attrib)
                elif ev.key == pygame.K_DOWN:
                    cur_screen, cx, cy = step_party((cf + 2) & 3, cx, cy, cur_screen, attrib)
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
    ap.add_argument("--trace", action="store_true", help="print scene tables")
    ap.add_argument("--interactive", action="store_true", help="pygame window")
    ap.add_argument("--scale", type=int, default=3, help="interactive scale")
    ap.add_argument("--no-minimap", action="store_true", help="view only (no minimap pane)")
    args = ap.parse_args(argv)

    if not DATA_BIN.is_file():
        print(f"warning: missing {DATA_BIN} (terrain lookup)", file=sys.stderr)

    maps = load_map(args.map_dat)
    attrib = AttribFile.load(str(args.attrib_dat))
    if not (0 <= args.screen < MAP_SCREENS):
        print(f"screen must be 0..{MAP_SCREENS - 1}", file=sys.stderr)
        return 1
    rec = attrib.records[args.screen]
    if not rec.is_outside:
        print(
            f"warning: screen {args.screen} is not outdoor (surface_flag=0)",
            file=sys.stderr,
        )

    grid = StitchedMap(maps, args.screen, attrib)
    scene = build_outdoor_scene(grid, args.x, args.y, args.facing, attrib)

    if args.trace:
        dump_scene_trace(scene, args.screen, args.x, args.y, args.facing, attrib)

    loc = location_label_from_attrib(args.screen, args.x, args.y, rec)
    print(f"Location: {loc}")

    if args.interactive:
        return run_interactive(
            maps,
            attrib,
            args.screen,
            args.x,
            args.y,
            args.facing,
            args.data,
            args.scale,
            with_minimap=not args.no_minimap,
        )

    img = render_outdoor_view(
        scene,
        args.data,
        visual=maps[args.screen][0],
        collision=maps[args.screen][1],
        screen=args.screen,
        x=args.x,
        y=args.y,
        facing=args.facing,
        map_name=loc,
        with_minimap=not args.no_minimap,
        attrib=attrib if args.no_minimap else None,
    )
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        img.save(args.out)
        print(f"wrote {args.out}")
    else:
        out = ROOT / "EXTRACTED" / "docs" / "img" / "_view3d_outdoor.png"
        out.parent.mkdir(parents=True, exist_ok=True)
        img.save(out)
        print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
