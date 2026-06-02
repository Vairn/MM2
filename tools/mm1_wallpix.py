#!/usr/bin/env python3
"""MM1 WALLPIX.DTA slices — layout from lagdotcom/common.py (496×128 composites)."""
from __future__ import annotations

import io
import re
import struct
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore

# ScummVM engines/mm/mm1/maps/maps.cpp COLOR_OFFSET[mapIndex]
MM1_COLOR_OFFSET: tuple[int, ...] = (
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
)

DEFAULT_WALLPIX_EXPORT = Path(
    r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 1\re\lagdotcom\exported"
)

# Overland WALLPIX entries 7–14 only (lagdotcom wall07.png … wall14.png).
# wall14 = mountains (mid-horizon on A1/A2/B1/B2), not forest.
WALLPIX_BIOME: dict[int, str] = {
    7: "trees",
    8: "mountains",
    9: "trees",
    10: "lava",
    11: "swamp",
    12: "water",
    13: "thick_forest",
    14: "mountains",
}

# MM2 outdoor floor sheets used by wiki/maze-walker/outdoor3d.js
MM2_FLOOR_BY_BIOME: dict[str, str] = {
    "water": "ocean_32",
    "swamp": "swamp_32",
    "lava": "desert_32",
    "mountains": "tundra_32",
    "trees": "desert_32",
    "thick_forest": "swamp_32",
}


def biome_label(entry: int) -> str:
    return WALLPIX_BIOME.get(entry, "unknown")


def mm2_floor_for_biome(label: str) -> str:
    return MM2_FLOOR_BY_BIOME.get(label, "desert_32")


def mm2_floor_for_lanes(lanes: tuple[int, int, int]) -> str:
    """Pick MM2 outdoor floor/horizon sheet from all three WALLPIX lane biomes."""
    labels = [biome_label(e) for e in lanes]
    if "water" in labels:
        return mm2_floor_for_biome("water")
    if labels[2] == "swamp" or labels[1] == "swamp":
        return mm2_floor_for_biome("swamp")
    if labels[2] == "lava" or labels[1] == "lava":
        return mm2_floor_for_biome("lava")
    if labels[1] == "mountains" or labels[2] == "mountains":
        return mm2_floor_for_biome("mountains")
    if labels[2] == "thick_forest":
        return mm2_floor_for_biome("thick_forest")
    return mm2_floor_for_biome(labels[1])


def format_lane_biomes(lanes: tuple[int | None, int | None, int | None]) -> str:
    parts = [biome_label(e) for e in lanes if e]
    return " / ".join(parts)


# OVR data offsets (ScummVM mm1/maps/maps.cpp) for overland WALLPIX lane ids.
_OVR_MAP_1, _OVR_MAP_2, _OVR_MAP_4, _OVR_MAP_6 = 1, 2, 4, 6
_TILE_AREA1 = [0x10D, 0x0B0B, 0x50A, 0x11A, 0x0B18, 0x517]
_TILE_AREA2 = [0x0B0B, 0x50A, 0x10D, 0x0F08, 0x907, 0x11A, 0x0B18, 0x517]
_TILE_AREA3 = [0x0B0B, 0x10D, 0x517, 0x0B18, 0x11A, 0x50A]
_TILE_AREAS = [_TILE_AREA1, _TILE_AREA2, _TILE_AREA3]
_TILE_OFFSET = [1, 7, 15]


def gog_dir_from_mazedata(mazedata: Path) -> Path | None:
    """Directory containing MAZEDATA.DTA and area*.OVR."""
    parent = mazedata.resolve().parent
    if (parent / "areaa1.OVR").is_file() or (parent / "AREAA1.OVR").is_file():
        return parent
    return None


def _wallpix_entry(area_table: int, load_id: int) -> int | None:
    if area_table < 1 or area_table > 3:
        return None
    arr = _TILE_AREAS[area_table - 1]
    ctr = _TILE_OFFSET[area_table - 1]
    for v in arr:
        if v == load_id:
            return ctr
        ctr += 1
    return None


def parse_ovr_data(ovr: bytes) -> dict:
    code_sz = struct.unpack_from("<H", ovr, 4)[0]
    data_sz = struct.unpack_from("<H", ovr, 8)[0]
    data = ovr[14 + code_sz : 14 + code_sz + data_sz]

    def w(off: int) -> int:
        return data[off] | (data[off + 1] << 8)

    return {
        "area_table": data[_OVR_MAP_1],
        "id_lane1": w(_OVR_MAP_2),
        "id_lane2": w(_OVR_MAP_4),
        "id_lane3": w(_OVR_MAP_6),
    }


def outdoor_wall_lanes(slug: str, gog_dir: Path) -> tuple[int, int, int] | None:
    """WALLPIX entry indices (7–14) for near / mid / far horizon lanes."""
    if not slug.startswith("area"):
        return None
    ovr_path = gog_dir / f"{slug.upper()}.OVR"
    if not ovr_path.is_file():
        return None
    d = parse_ovr_data(ovr_path.read_bytes())
    area = d["area_table"]
    lanes: list[int] = []
    for key in ("id_lane1", "id_lane2", "id_lane3"):
        entry = _wallpix_entry(area, d[key])
        if entry is None:
            return None
        lanes.append(entry)
    return lanes[0], lanes[1], lanes[2]


# lagdotcom WALLSLICES — one 496×128 sheet, 12 sub-images.
@dataclass(frozen=True)
class WallSlice:
    name: str
    w: int
    h: int
    x: int
    y: int


WALLSLICES: tuple[WallSlice, ...] = (
    WallSlice("left1", 32, 128, 0, 0),
    WallSlice("left2", 40, 96, 32, 16),
    WallSlice("left3", 24, 64, 72, 32),
    WallSlice("left4", 16, 32, 96, 48),
    WallSlice("right1", 32, 128, 192, 0),
    WallSlice("right2", 40, 96, 152, 16),
    WallSlice("right3", 24, 64, 128, 32),
    WallSlice("right4", 16, 32, 112, 48),
    WallSlice("mid2", 176, 96, 224, 0),
    WallSlice("mid3", 96, 64, 400, 0),
    WallSlice("mid4", 48, 32, 400, 80),
    WallSlice("mid5", 16, 16, 460, 80),
)

# MM2 collectBlits frame index → slice (depth ordering aligned with view3d.js tables).
SLICE_BY_FRAME: tuple[str, ...] = (
    "mid2",   # 0 front d0
    "mid3",   # 1
    "mid4",   # 2
    "mid5",   # 3
    "left1",  # 4 left near d0
    "left2",  # 5
    "left3",  # 6
    "left4",  # 7
    "right1", # 8 right near d0
    "right2", # 9
    "right3", # 10
    "right4", # 11
    "left2",  # 12 left far d0
    "right2", # 13 right far d0
    "left3",  # 14
    "right3", # 15
    "mid2",   # 16 door front d0 (+0x10 in engine)
    "mid3",   # 17
    "mid4",   # 18
    "mid5",   # 19
    "left1",  # 20
    "left2",  # 21
    "left3",  # 22
    "right1", # 23
)

SLICE_NAME_TO_INDEX: dict[str, int] = {s.name: i for i, s in enumerate(WALLSLICES)}


def wall_set_for_screen(screen_id: int, slug: str) -> int:
    """Pick wallNN.png (1-based) for a MAZEDATA screen."""
    if slug.startswith("area"):
        letter = slug[4].lower()
        return {"a": 5, "b": 6, "c": 7, "d": 8, "e": 9}.get(letter, 5)
    if screen_id < len(MM1_COLOR_OFFSET) and MM1_COLOR_OFFSET[screen_id] == 0:
        return 1
    if slug.startswith("cave") or slug.startswith("udrag"):
        return 1
    if slug in {"doom", "demon", "astral", "dragad", "whitew"}:
        return 13
    if slug.startswith("enf") or slug.startswith("qvl") or slug.startswith("rwl"):
        return 10
    if slug.startswith("pp"):
        return 14
    # towns
    return 2


def wall_sheet_path(export_dir: Path, set_index: int) -> Path:
    return export_dir / f"wall{set_index:02d}.png"


def slice_wall_sheet(sheet: Image.Image) -> list[Image.Image]:
    """12 slice images in WALLSLICES order."""
    out: list[Image.Image] = []
    for sl in WALLSLICES:
        crop = sheet.crop((sl.x, sl.y, sl.x + sl.w, sl.y + sl.h))
        out.append(crop.convert("RGBA"))
    return out


def build_frame_atlas(set_index: int, export_dir: Path) -> tuple[dict[str, dict], list[Image.Image]]:
    """Frame metadata for walker manifest + 24 MM2-indexed frames (RGBA)."""
    path = wall_sheet_path(export_dir, set_index)
    if not path.is_file():
        raise FileNotFoundError(path)
    if Image is None:
        raise RuntimeError("pip install pillow")
    sheet = Image.open(path)
    slices = slice_wall_sheet(sheet)
    frames: list[Image.Image] = []
    frames_meta: dict[str, dict] = {}
    for fi, slice_name in enumerate(SLICE_BY_FRAME):
        si = SLICE_NAME_TO_INDEX[slice_name]
        img = slices[si]
        frames.append(img)
        frames_meta[str(fi)] = {"w": img.width, "h": img.height, "slice": slice_name}
    return frames_meta, frames


def export_wallpix_to_manifest(
    export_dir: Path,
    *,
    sets: range | None = None,
) -> tuple[dict, dict[str, str]]:
    """Return (manifest.sheets fragment, sprites data-URL table)."""
    import base64

    if sets is None:
        sets = range(1, 19)
    sheets: dict = {}
    sprites: dict[str, str] = {}
    for n in sets:
        key = f"wall{n:02d}"
        try:
            meta, frames = build_frame_atlas(n, export_dir)
        except FileNotFoundError:
            continue
        sheets[key] = {"file": f"{key}.png", "source": "lagdotcom", "frames": meta}
        for fi, img in enumerate(frames):
            buf = io.BytesIO()
            img.save(buf, format="PNG")
            b64 = base64.b64encode(buf.getvalue()).decode("ascii")
            sprites[f"{key}:{fi}"] = f"data:image/png;base64,{b64}"
    return sheets, sprites


def list_available_wall_sets(export_dir: Path) -> list[int]:
    found: list[int] = []
    for p in export_dir.glob("wall*.png"):
        m = re.fullmatch(r"wall(\d+)\.png", p.name, re.I)
        if m:
            found.append(int(m.group(1)))
    return sorted(found)
