#!/usr/bin/env python3
"""Convert MM1 overland MAZEDATA screens toward MM2 outdoor map.dat encoding.

MM1 and MM2 share the same 5×4 sector grid (A1–E4) and 512-byte MAZEDATA/map.dat
layout, but overland **page-0** bytes differ:

- MM1 (ScummVM MapWalls): four 2-bit wall types per cell (same as dungeon).
- MM2 outdoor: low 5 bits = terrain id for carto / horizon; high bits = flags.

This module derives MM2-style visual bytes from MM1 walls plus an MM2 sector
template, and copies MM2 attrib neighbours for cross-screen walking.
"""
from __future__ import annotations

from pathlib import Path

from attrib_codec import AttribFile, AttribRecord
from mm1_wallpix import biome_label, mm2_floor_for_lanes, outdoor_wall_lanes

# MM2 map.dat open-cell terrain id (low 5 bits) per MM1 WALLPIX far-lane biome.
BIOME_OPEN_TERRAIN: dict[str, int] = {
    "trees": 0,
    "mountains": 0,
    "thick_forest": 0,
    "lava": 0,
    "swamp": 0,
    "water": 2,
}

# MM2 map.dat screen index per overland sector label.
MM2_SCREEN_BY_SECTOR: dict[str, int] = {
    "A1": 5,
    "B1": 6,
    "C1": 7,
    "D1": 8,
    "A2": 9,
    "B2": 10,
    "C2": 11,
    "A3": 12,
    "B3": 13,
    "C3": 14,
    "A4": 15,
    "B4": 16,
    "E1": 33,
    "D2": 34,
    "E2": 35,
    "D3": 36,
    "E3": 37,
    "C4": 38,
    "D4": 39,
    "E4": 40,
}

MM1_AREA_SLUGS: tuple[str, ...] = tuple(
    f"area{letter}{num}" for letter in "abcde" for num in range(1, 5)
)


def sector_from_slug(slug: str) -> str | None:
    if not slug.startswith("area") or len(slug) != 6:
        return None
    return slug[4].upper() + slug[5]


def wall_nibble(cell: int, direction: int) -> int:
    return (cell >> (direction * 2)) & 3


def mm1_cell_open(wall_byte: int) -> bool:
    return all(wall_nibble(wall_byte, d) == 0 for d in range(4))


def mm1_cell_border_void(wall_byte: int) -> bool:
    """MM1 overland often marks map edge with nibble pattern 3 or byte 0xFF."""
    if wall_byte == 0xFF:
        return True
    return all(wall_nibble(wall_byte, d) == 3 for d in range(4))


def convert_visual_page(
    mm1_visual: bytes,
    mm2_template_visual: bytes,
    *,
    open_default: int = 4,
) -> bytes:
    """Build MM2 outdoor visual bytes from MM1 walls + MM2 sector template."""
    if len(mm1_visual) != 256 or len(mm2_template_visual) != 256:
        raise ValueError("expected 256-byte pages")
    out = bytearray(256)
    for i in range(256):
        w1 = mm1_visual[i]
        tmpl = mm2_template_visual[i]
        if mm1_cell_border_void(w1):
            # Prefer template border tile; fallback to common MM2 edge (terrain 12 + flags)
            out[i] = tmpl if (tmpl & 0xE0) else 0xAC
            continue
        if mm1_cell_open(w1):
            terrain = tmpl & 0x1F
            if terrain == 0:
                terrain = open_default
            out[i] = terrain
            continue
        # Blocked: keep template blocked encoding when present, else terrain + wall flag
        if tmpl & 0xE0:
            out[i] = tmpl
        else:
            terrain = tmpl & 0x1F or open_default
            out[i] = 0x60 | terrain
    return bytes(out)


def convert_collision_page(mm1_collision: bytes, mm2_template_collision: bytes) -> bytes:
    """Start from MM2 collision; merge MM1 event (0x80) bits."""
    out = bytearray(mm2_template_collision)
    for i in range(256):
        if mm1_collision[i] & 0x80:
            out[i] |= 0x80
    return bytes(out)


def load_mm2_templates(map_dat: Path, attrib_dat: Path) -> dict[str, tuple[bytes, bytes, AttribRecord]]:
    data = map_dat.read_bytes()
    attrib = AttribFile.decode(attrib_dat.read_bytes())
    templates: dict[str, tuple[bytes, bytes, AttribRecord]] = {}
    for sector, sid in MM2_SCREEN_BY_SECTOR.items():
        off = sid * 512
        visual = data[off : off + 256]
        collision = data[off + 256 : off + 512]
        templates[sector] = (visual, collision, attrib.records[sid])
    return templates


def mm1_index_by_sector(slugs: tuple[str, ...]) -> dict[str, int]:
    out: dict[str, int] = {}
    for i, slug in enumerate(slugs):
        sec = sector_from_slug(slug)
        if sec:
            out[sec] = i
    return out


def mm2_neighbors_to_mm1_indices(
    mm2_neighbors: list[int],
    mm2_screen_to_mm1: dict[int, int],
) -> list[int]:
    """Map MM2 overland screen ids to MM1 MAZEDATA indices for the walker."""
    out: list[int] = []
    for sid in mm2_neighbors:
        out.append(mm2_screen_to_mm1.get(sid, -1))
    return out


def build_mm2_screen_to_mm1(slugs: tuple[str, ...]) -> dict[int, int]:
    by_sector = mm1_index_by_sector(slugs)
    return {
        mm2_sid: by_sector[sector]
        for sector, mm2_sid in MM2_SCREEN_BY_SECTOR.items()
        if sector in by_sector
    }


def mm2_surface_from_wall_lanes(lanes: tuple[int, int, int]) -> int:
    """MM2 attrib surface_flag class from MM1 OVR WALLPIX lane biomes."""
    labels = [biome_label(e) for e in lanes]
    if "water" in labels:
        return 0xCC
    if labels[2] == "swamp" or labels[1] == "swamp":
        return 0xBB
    if labels[1] == "mountains" or labels[2] == "mountains":
        return 0x99
    if labels[2] == "thick_forest":
        return 0xBB
    if labels[2] == "lava" or labels[1] == "lava":
        return 0xAA
    return 0xAA


def open_terrain_from_wall_lanes(lanes: tuple[int, int, int]) -> int:
    labels = [biome_label(e) for e in lanes]
    if labels[2] == "water" or (labels[1] == "water" and labels[2] != "mountains"):
        return BIOME_OPEN_TERRAIN["water"]
    return BIOME_OPEN_TERRAIN.get(labels[2], 0)


def outdoor_style_from_wall_lanes(lanes: tuple[int, int, int]) -> dict:
    labels = [biome_label(e) for e in lanes]
    return {
        "wallLanes": list(lanes),
        "wallBiomes": labels,
        "biome": mm2_floor_for_lanes(lanes),
        "surface": mm2_surface_from_wall_lanes(lanes),
        "openDefault": open_terrain_from_wall_lanes(lanes),
    }


def convert_mm1_area_screen(
    slug: str,
    mm1_visual: bytes,
    mm1_collision: bytes,
    templates: dict[str, tuple[bytes, bytes, AttribRecord]],
    *,
    mm2_screen_to_mm1: dict[int, int] | None = None,
    gog_dir: Path | None = None,
) -> tuple[bytes, bytes, dict] | None:
    """Return (visual, collision, walker_meta) or None if not an area slug."""
    sector = sector_from_slug(slug)
    if sector is None or sector not in templates:
        return None
    tv, tc, rec = templates[sector]
    lanes: tuple[int, int, int] | None = None
    if gog_dir is not None:
        lanes = outdoor_wall_lanes(slug, gog_dir)
    open_default = 4
    surface = rec.surface_flag
    conversion = "mm1_walls+mm2_template"
    style: dict | None = None
    if lanes is not None:
        style = outdoor_style_from_wall_lanes(lanes)
        open_default = style["openDefault"]
        surface = style["surface"]
        conversion = "mm1_walls+mm2_template+wallpix_biome"
    visual = convert_visual_page(mm1_visual, tv, open_default=open_default)
    collision = convert_collision_page(mm1_collision, tc)
    mm2_nbs = list(rec.raw[5:9])
    mm1_nbs = (
        mm2_neighbors_to_mm1_indices(mm2_nbs, mm2_screen_to_mm1)
        if mm2_screen_to_mm1
        else [-1, -1, -1, -1]
    )
    meta = {
        "outdoor": True,
        "env": "outside",
        "neighbors": mm1_nbs,
        "sector": rec.raw[0x15],
        "surface": surface,
        "mm2_template_screen": MM2_SCREEN_BY_SECTOR[sector],
        "mm2_neighbors": mm2_nbs,
        "conversion": conversion,
    }
    if style is not None:
        meta.update(style)
    return visual, collision, meta
