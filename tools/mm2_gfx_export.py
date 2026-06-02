#!/usr/bin/env python3
"""Export MM2 sprites for the wiki gallery: monsters, tilesets, cartography, 3D views."""
from __future__ import annotations

import json
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
# Bump when gallery PNG semantics change (wiki CDN caches by URL).
GALLERY_CACHE_VERSION = 16
# Wiki combat GIF frame duration (ms). Original export used 100ms; quarter speed = 400ms.
ANM_GIF_DURATION_MS = 400
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

from attrib_codec import is_outdoor_area  # noqa: E402
from render_view_refs import (  # noqa: E402
    INDOOR_WALLSETS,
    composite_backdrop,
    composite_indoor,
    composite_outdoor,
    composite_sheet_montage,
    composite_view_grid,
    indoor_frame_grid_views,
    load_frame,
    load_sprite,
    read_u16be,
    decode_planes,
)

# ---------------------------------------------------------------------------
# .32 inventory — not all ".32" files are planar gfx (see filename table in
# 06-gfx-loading.md: unified resource list). globe.32 / disk.32 are XOR blobs.
# ---------------------------------------------------------------------------

NON_GFX_32 = frozenset({
    "globe.32",  # copy-protection string tables (tools/decode_globe_amiga.py)
    "disk.32",   # XOR obfuscated data blob, not a tile sheet
})

AREA_NAMES: dict[int, str] = {
    0: "Middlegate",
    1: "Atlantium",
    2: "Tundara",
    3: "Vulcania",
    4: "Sandsobar",
    17: "Middlegate Cavern",
    18: "Atlantium Cavern",
    19: "Tundara Cavern",
    20: "Vulcania Cavern",
    21: "Sandsobar Cavern",
    22: "Corak's Cave",
    23: "Square Lake Cave",
    24: "Ice Cavern",
    25: "Sarakin's Mine",
    26: "Murray's Cave",
    27: "Druid's Cave",
    28: "Forbidden Forest Cavern",
    29: "Dragon's Dominion",
    30: "Dawn's Mist Bog",
    31: "Gemmaker's Cave",
    32: "Nomadic Rift",
    41: "Elemental Plane of Air",
    42: "Elemental Plane of Fire",
    43: "Elemental Plane of Earth",
    44: "Elemental Plane of Water",
    45: "Hillstone Dungeon Level 1",
    46: "Hillstone Dungeon Level 2",
    47: "Woodhaven Dungeon Level 1",
    48: "Woodhaven Dungeon Level 2",
    49: "Pinehurst Dungeon Level 1",
    50: "Pinehurst Dungeon Level 2",
    51: "Luxus Palace Dungeon Level 1",
    52: "Luxus Palace Dungeon Level 2",
    53: "Ancients (Good)",
    54: "Ancients (Evil)",
    55: "Hillstone",
    56: "Woodhaven",
    57: "Pinehurst",
    58: "Luxus Palace Royale",
    59: "Castle Xabran",
}


def area_name(screen: int, attrib: list[bytes] | None = None) -> str:
    if screen in AREA_NAMES:
        return AREA_NAMES[screen]
    if screen in OUTDOOR_SECTOR_BY_SCREEN:
        return OUTDOOR_SECTOR_BY_SCREEN[screen]
    if attrib and screen < len(attrib):
        sec = outdoor_sector(attrib[screen][0x15])
        if sec != "00":
            return sec
    return f"Area {screen}"


def outdoor_sector(label_byte: int) -> str:
    """Overland world sector from attrib +0x15 (e.g. 0xC2 → C2)."""
    b = label_byte & 0xFF
    return f"{b >> 4:X}{b & 0xF:X}"


# Overland map screens 5–16 and 33–40 — sector labels from attrib.dat +0x15.
OUTDOOR_SECTOR_BY_SCREEN: dict[int, str] = {
    5: "A1",
    6: "B1",
    7: "C1",
    8: "D1",
    9: "A2",
    10: "B2",
    11: "C2",
    12: "A3",
    13: "B3",
    14: "C3",
    15: "A4",
    16: "B4",
    33: "E1",
    34: "D2",
    35: "E2",
    36: "D3",
    37: "E3",
    38: "C4",
    39: "D4",
    40: "E4",
}


# Elemental planes on the full 1:1 world map (screen id → grid cell col, row).
# NW Air, NE Fire, SW Water, SE Earth — matches surface portal sectors (B1/E1/A4/E4).
WORLD_MAP_COLS = 7
WORLD_MAP_ROWS = 6
OVERLAND_GRID_ORIGIN = (1, 1)  # top-left cell of the 5×4 sector block
WORLD_OVERLAND_MAP_FILE = "world-map-7x6.png"
ELEMENTAL_PLANE_CORNERS: dict[int, tuple[int, int]] = {
    41: (0, 0),  # Air — NW
    42: (6, 0),  # Fire — NE
    43: (6, 5),  # Earth — SE (portal on E4)
    44: (0, 5),  # Water — SW (portal on A4)
}
WORLD_MAP_CORNER_LABELS: dict[tuple[int, int], str] = {
    (0, 0): "Air (41)",
    (6, 0): "Fire (42)",
    (0, 5): "Water (44)",
    (6, 5): "Earth (43)",
}

# Outdoor overland screens in 5×4 world-sector order (A1..E4, north row first).
OUTDOOR_SECTOR_GRID: list[list[int]] = [
    [5, 6, 7, 8, 33],      # row 1: A1 B1 C1 D1 E1
    [9, 10, 11, 34, 35],   # row 2: A2 … E2
    [12, 13, 14, 36, 37],  # row 3: A3 … E3
    [15, 16, 38, 39, 40],  # row 4: A4 … E4
]

# Overview montage groups for the Map Cartography gallery page.
MAP_OVERVIEW_GROUPS: list[tuple[str, str, list[int], int]] = [
    ("overview-towns", "Towns", list(range(0, 5)), 5),
    ("overview-town-caverns", "Town caverns", list(range(17, 22)), 5),
    ("overview-caves", "Caves", list(range(22, 33)), 4),
    ("overview-outdoor", "Outdoor overland (5×4 world sectors)", sum(OUTDOOR_SECTOR_GRID, []), 5),
    ("overview-planes", "Elemental planes", list(range(41, 45)), 4),
    ("overview-castle-dungeons", "Castle dungeons", list(range(45, 53)), 4),
    ("overview-castles", "Castles", [55, 56, 57, 58, 59], 5),
    ("overview-ancients", "Ancients", [53, 54], 2),
]


# Cartography (from editor/src/core/Cartography.h)
K_CARTO_TILE = [
    0x00, 0x05, 0x06, 0x05, 0x03, 0x0B, 0x0D, 0x0B,
    0x04, 0x0C, 0x0E, 0x0C, 0x03, 0x0B, 0x0D, 0x0B,
    0x01, 0x0F, 0x11, 0x0F, 0x07, 0x13, 0x16, 0x13,
    0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
    0x02, 0x10, 0x12, 0x10, 0x08, 0x14, 0x18, 0x14,
    0x0A, 0x17, 0x1A, 0x17, 0x08, 0x14, 0x18, 0x14,
    0x01, 0x0F, 0x11, 0x0F, 0x07, 0x13, 0x16, 0x13,
    0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
]

CARTO_FRAME_LABELS: dict[int, str] = {
    0x20: "arrow N",
    0x21: "arrow S",
    0x22: "arrow E",
    0x23: "arrow W",
    0x1B: "edge overlay",
    0x1C: "edge overlay",
}


def carto_frame(screen: int, visual: int, outdoor: bool) -> int:
    # Planes 41–44: map bytes 0x28/0x25/0x27/0x26 → outb frames 8/5/7/6 (air/fire/earth/water).
    if 41 <= screen <= 44:
        return visual & 0x1F
    if outdoor:
        return visual & 0x1F
    return K_CARTO_TILE[(visual >> 2) & 0x3F]


def carto_use_outb(screen: int, attrib: list[bytes]) -> bool:
    """Tileset for auto-map: outb.32 for surface + elemental planes."""
    if 41 <= screen <= 44:
        return True
    return is_outdoor_screen(screen, attrib)


# ---------------------------------------------------------------------------
# Data directory resolution
# ---------------------------------------------------------------------------


def resolve_data_dir(preferred: Path | None = None) -> Path:
    candidates = []
    if preferred:
        candidates.append(preferred)
    candidates.extend([ROOT, ROOT / "MM2BOOT", ROOT / "EXTRACTED"])
    for d in candidates:
        if (d / "map.dat").exists() and (d / "monsters.dat").exists():
            return d
    return ROOT


def resolve_asset(data_dir: Path, name: str) -> Path | None:
    for d in (data_dir, ROOT / "MM2BOOT", ROOT):
        p = d / name
        if p.exists() and p.stat().st_size > 0:
            return p
    return None


# ---------------------------------------------------------------------------
# .32 / .anm decode
# ---------------------------------------------------------------------------


def frame_count_32(sheet: str, data_dir: Path) -> int:
    p = resolve_asset(data_dir, sheet)
    if not p:
        return 0
    b = p.read_bytes()
    if len(b) < 4:
        return 0
    return read_u16be(b, 0)


def load_all_frames(sheet: str, data_dir: Path) -> list[Image.Image]:
    n = frame_count_32(sheet, data_dir)
    frames: list[Image.Image] = []
    for i in range(n):
        try:
            frames.append(load_frame(sheet, i, data_dir))
        except Exception:
            break
    return frames


def find_image_chunk(data: bytes, start: int = 0) -> int | None:
    for i in range(start, len(data) - 10):
        if data[i] == 0xFF and data[i + 1] == 0x00:
            hdr = i + 1
            frames = read_u16be(data, hdr)
            depth = read_u16be(data, hdr + 2)
            w = read_u16be(data, hdr + 4)
            h = read_u16be(data, hdr + 6)
            if 0 < frames < 256 and 0 <= depth < 64 and 0 < w <= 1024 and 0 < h <= 1024:
                return hdr
    return None


@dataclass
class AnmAsset:
    path: Path
    frames: list[Image.Image] = field(default_factory=list)
    prelude: list[dict | None] = field(default_factory=list)
    frame_count: int = 0
    canvas_min_x: int = 0
    canvas_min_y: int = 0
    canvas_w: int = 0
    canvas_h: int = 0


def load_anm_asset(anm_path: Path) -> AnmAsset:
    asset = AnmAsset(path=anm_path)
    data = anm_path.read_bytes()
    if len(data) < 0x34:
        return asset

    prelude: list[dict | None] = []
    for i in range(11):
        off = 4 + i * 4
        entry = data[off : off + 4]
        if entry == b"\xff\xff\xff\xff":
            prelude.append(None)
        else:
            prelude.append(
                {"x": entry[0], "y": entry[1], "w": entry[2], "h": entry[3], "used": True}
            )
    asset.prelude = prelude

    chunk_off = find_image_chunk(data, 0x33)
    if chunk_off is None:
        return asset

    n = read_u16be(data, chunk_off)
    info = chunk_off + 4
    pal_off = info + n * 6
    palette = []
    for i in range(32):
        pw = read_u16be(data, pal_off + i * 2)
        palette.append((((pw >> 8) & 0xF) * 17, ((pw >> 4) & 0xF) * 17, (pw & 0xF) * 17))

    cur = pal_off + 64
    frames: list[Image.Image] = []
    for f in range(n):
        w = read_u16be(data, info + f * 6)
        h = read_u16be(data, info + f * 6 + 2)
        bpr = ((w + 15) >> 3) & 0xFFFE
        rs = h * bpr
        cur, planes = decode_planes(data, cur, 5 * rs)
        im = Image.new("RGBA", (w, h))
        px = im.load()
        for y in range(h):
            for x in range(w):
                idx = 0
                for pl in range(5):
                    bp = pl * rs + y * bpr + (x >> 3)
                    bit = (planes[bp] >> (7 - (x & 7))) & 1
                    idx |= bit << pl
                r, g, b = palette[idx]
                px[x, y] = (r, g, b, 0 if idx == 0 else 255)
        frames.append(im)

    asset.frames = frames
    asset.frame_count = len(frames)
    if frames:
        min_x = min_y = 0
        max_x = frames[0].width
        max_y = frames[0].height
        for i in range(1, len(frames)):
            pe = prelude[i - 1] if i - 1 < len(prelude) else None
            if not pe:
                continue
            fr = frames[i]
            w = pe["w"] if 0 < pe["w"] < fr.width else fr.width
            h = pe["h"] if 0 < pe["h"] < fr.height else fr.height
            x, y = pe["x"], pe["y"]
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x + w)
            max_y = max(max_y, y + h)
        asset.canvas_min_x = min_x
        asset.canvas_min_y = min_y
        asset.canvas_w = max_x - min_x
        asset.canvas_h = max_y - min_y
    return asset


def composite_anm_frame(asset: AnmAsset, frame_idx: int) -> Image.Image | None:
    if not asset.frames or frame_idx < 0 or frame_idx >= len(asset.frames):
        return None
    if asset.canvas_w <= 0 or asset.canvas_h <= 0:
        return asset.frames[0].copy()

    canvas = Image.new("RGBA", (asset.canvas_w, asset.canvas_h), (0, 0, 0, 0))
    base = asset.frames[0]
    canvas.paste(base, (-asset.canvas_min_x, -asset.canvas_min_y), base)

    if frame_idx > 0:
        fr = asset.frames[frame_idx]
        x = -asset.canvas_min_x
        y = -asset.canvas_min_y
        copy_w, copy_h = fr.width, fr.height
        pe = asset.prelude[frame_idx - 1] if frame_idx - 1 < len(asset.prelude) else None
        if pe:
            x = pe["x"] - asset.canvas_min_x
            y = pe["y"] - asset.canvas_min_y
            if 0 < pe["w"] < copy_w:
                copy_w = pe["w"]
            if 0 < pe["h"] < copy_h:
                copy_h = pe["h"]
        clear = Image.new("RGBA", (copy_w, copy_h), (0, 0, 0, 0))
        canvas.paste(clear, (x, y))
        patch = fr.crop((0, 0, copy_w, copy_h))
        canvas.paste(patch, (x, y), patch)

    return canvas


def flatten_rgba(im: Image.Image, bg: tuple[int, int, int] = (13, 3, 3)) -> Image.Image:
    """RGBA → RGB on the wiki background colour."""
    flat = Image.new("RGB", im.size, bg)
    if im.mode == "RGBA":
        flat.paste(im, mask=im.split()[3])
    else:
        flat.paste(im)
    return flat


def save_anm_gif(
    asset: AnmAsset,
    path: Path,
    *,
    scale: int = 3,
    duration_ms: int = ANM_GIF_DURATION_MS,
) -> bool:
    """Animated GIF from composite ANM frames (TV-prelude patching applied)."""
    if not asset.frames:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    rgb_frames: list[Image.Image] = []
    for i in range(asset.frame_count):
        comp = composite_anm_frame(asset, i)
        if not comp:
            continue
        pad = Image.new("RGBA", (comp.width + 4, comp.height + 4), (13, 3, 3, 255))
        pad.paste(comp, (2, 2), comp)
        rgb_frames.append(flatten_rgba(scale_nearest(pad, scale)))
    if not rgb_frames:
        return False
    dur = max(40, min(duration_ms, 2000))
    if len(rgb_frames) == 1:
        rgb_frames[0].save(path, optimize=True)
        return True
    rgb_frames[0].save(
        path,
        save_all=True,
        append_images=rgb_frames[1:],
        duration=dur,
        loop=0,
        disposal=2,
        optimize=True,
    )
    return True


def record_slug(prefix: str, idx: int, name: str) -> str:
    slug = f"{prefix}{idx:03d}-{name.lower().replace(' ', '-').replace('/', '-')}"
    return "".join(c if c.isalnum() or c in "-_" else "" for c in slug)


def load_monster_records(data_dir: Path) -> list[dict]:
    from decode_monsters import (
        PARTY_VERB_NAMES,
        SINGLE_EFFECT_NAMES,
        decode_record,
        enrich_decoded_stats,
    )

    raw = (data_dir / "monsters.dat").read_bytes()
    out: list[dict] = []
    for idx in range(256):
        rec = raw[idx * MONSTER_RECORD : (idx + 1) * MONSTER_RECORD]
        d = decode_record(idx, rec)
        if not d["name"].strip():
            continue
        pic = d["picture"]
        sprite = pic & 0x7F
        friends = (d["add_friends_base"] + 1) * (10 if d["add_friends_x10"] else 1)
        flags: list[str] = []
        if d["undead"]:
            flags.append("undead")
        if d["archer"]:
            flags.append("archer")
        if d["multiplies"]:
            flags.append("multiplies")
        if d["single_misc"]:
            flags.append("single+")
        out.append(
            {
                **enrich_decoded_stats(d),
                "sprite": sprite,
                "large": bool(pic & 0x80),
                "party_verb_name": PARTY_VERB_NAMES[d["party_verb"]]
                if d["party_verb"] < len(PARTY_VERB_NAMES)
                else f"verb#{d['party_verb']}",
                "single_effect_name": SINGLE_EFFECT_NAMES[d["single_effect"]]
                if d["single_effect"] < len(SINGLE_EFFECT_NAMES)
                else f"effect#{d['single_effect']}",
                "add_friends": friends,
                "flags": flags,
            }
        )
    return out


def load_item_records(data_dir: Path) -> list[dict]:
    from decode_items import BONUS_TYPE_NAMES, decode_record

    raw = (data_dir / "items.dat").read_bytes()
    out: list[dict] = []
    for idx in range(256):
        rec = raw[idx * 0x14 : (idx + 1) * 0x14]
        it = decode_record(rec)
        bonus = rec[0x0E]
        out.append(
            {
                "index": idx,
                "name": it.name.strip(),
                "gold": it.gold,
                "damage": it.damage,
                "bonus_type": bonus >> 4,
                "bonus_amount": bonus & 0xF,
                "bonus_text": (
                    f"{BONUS_TYPE_NAMES[bonus >> 4]} +{bonus & 0xF}"
                    if bonus & 0xF
                    else "—"
                ),
                "effect_byte": rec[0x0F],
                "effect_text": it.effect_text,
                "forbidden": it.forbidden_classes,
                "usable": it.usable_classes,
            }
        )
    return out


# ---------------------------------------------------------------------------
# Compositing helpers
# ---------------------------------------------------------------------------


def scale_nearest(im: Image.Image, factor: int) -> Image.Image:
    if factor <= 1:
        return im
    return im.resize((im.width * factor, im.height * factor), Image.NEAREST)


def contact_sheet(
    images: Iterable[Image.Image],
    cols: int = 8,
    pad: int = 2,
    bg: tuple[int, int, int, int] = (13, 3, 3, 255),
    labels: list[str] | None = None,
    label_h: int = 0,
) -> Image.Image:
    imgs = list(images)
    if not imgs:
        return Image.new("RGBA", (64, 64), bg)
    cols = max(1, cols)
    rows = (len(imgs) + cols - 1) // cols
    cell_w = max(im.width for im in imgs) + pad * 2
    cell_h = max(im.height for im in imgs) + pad * 2 + label_h
    out = Image.new("RGBA", (cols * cell_w, rows * cell_h), bg)
    draw = ImageDraw.Draw(out) if labels else None
    for n, im in enumerate(imgs):
        cx, cy = n % cols, n // cols
        out.paste(im, (cx * cell_w + pad, cy * cell_h + pad), im)
        if draw and labels and n < len(labels):
            draw.text((cx * cell_w + pad, cy * cell_h + pad + im.height + 1), labels[n], fill=(242, 237, 237))
    return out


def save_png(im: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path, optimize=True)


# ---------------------------------------------------------------------------
# Attrib / map helpers
# ---------------------------------------------------------------------------


def load_attrib(data_dir: Path) -> list[bytes]:
    p = data_dir / "attrib.dat"
    if not p.exists():
        return [bytes(64)] * 60
    raw = p.read_bytes()
    return [raw[i * 64 : (i + 1) * 64] for i in range(60)]


def is_outdoor_screen(screen: int, attrib: list[bytes]) -> bool:
    if screen < len(attrib):
        return is_outdoor_area(screen, attrib[screen][0x04])
    return screen not in AREA_NAMES


def load_map_screens(data_dir: Path) -> list[tuple[bytes, bytes]]:
    p = data_dir / "map.dat"
    raw = p.read_bytes()
    screens = []
    for i in range(60):
        off = i * 512
        screens.append((raw[off : off + 256], raw[off + 256 : off + 512]))
    return screens


def unpack_map_coord(packed: int) -> tuple[int, int]:
    """Packed (Y<<4)|X from attrib.dat → (x, y) disk row (0=south)."""
    return packed & 0x0F, (packed >> 4) & 0x0F


def triplet_tile(pos_byte: int) -> tuple[int, int]:
    """event.dat triplet byte 0 → (x, disk_y)."""
    return (pos_byte >> 4) & 0x0F, pos_byte & 0x0F


def tile_label(x: int, y: int) -> str:
    """Column a–p, row 1–16 (y=0 is south)."""
    x = max(0, min(15, int(x)))
    y = max(0, min(15, int(y)))
    return f"{chr(ord('a') + x)}{y + 1}"


def map_env_label(attrib_row: bytes, screen: int) -> str:
    if attrib_row[0x04] != 0:
        return "overland"
    if 41 <= screen <= 44:
        return "plane"
    env = attrib_row[0x03]
    if env == 0x11:
        return "town"
    if env == 0x12:
        return "cavern"
    if env in (0x13, 0x14):
        return "castle"
    return "dungeon"


def load_event_triplets(data_dir: Path) -> dict[int, list[tuple[int, int, int]]]:
    """First 60 event.dat locations → map screen id."""
    p = data_dir / "event.dat"
    if not p.exists():
        return {}
    try:
        from decode_event import decode_triplets, read_header
    except ImportError:
        return {}
    data = p.read_bytes()
    header = read_header(data)
    out: dict[int, list[tuple[int, int, int]]] = {}
    for loc_id in range(min(60, len(header))):
        off, length = header[loc_id]
        blob = data[off : off + length]
        triplets, _, _ = decode_triplets(blob)
        out[loc_id] = triplets
    return out


def analyze_map_screen(visual: bytes, collision: bytes) -> dict:
    doors = torches = 0
    for b in visual:
        for shift in (0, 2, 4, 6):
            w = (b >> shift) & 3
            if w == 3:
                doors += 1
            elif w == 2:
                torches += 1
    dark_dirs = 0
    for b in collision:
        for shift in (1, 3, 5):
            if (b >> shift) & 1:
                dark_dirs += 1
    return {
        "events": sum(1 for b in collision if b & 0x80),
        "doors": doors,
        "torches": torches,
        "dark_dirs": dark_dirs,
        "unique_visual": len(set(visual)),
        "unique_collision": len(set(collision)),
        "terrain_ids": sorted(set(b & 0x1F for b in visual)),
        "filled": sum(1 for b in visual if b),
    }


MAP_DAT_REFERENCE_SCREENS = (0, 11, 41)  # town interior, overland hub, elemental plane


def render_event_overlay(
    collision: bytes,
    events: list[tuple[int, int, int]],
    *,
    title: str,
    cell: int = 11,
) -> Image.Image:
    """Compact 16×16 north-up map: event tiles highlighted."""
    gap = 1
    margin = 28
    grid = 16
    w = margin * 2 + grid * cell + (grid - 1) * gap
    h = margin * 2 + grid * cell + (grid - 1) * gap
    img = Image.new("RGBA", (w, h), (13, 3, 3, 255))
    draw = ImageDraw.Draw(img)
    draw.text((6, 6), title, fill=(242, 237, 237, 255))

    event_cells: dict[tuple[int, int], int] = {}
    for pos, evt, _cond in events:
        x, y = triplet_tile(pos)
        event_cells[(x, y)] = evt

    for sy in range(grid):
        disk_y = grid - 1 - sy
        for x in range(grid):
            idx = disk_y * grid + x
            col = collision[idx]
            has_evt = (x, disk_y) in event_cells
            x0 = margin + x * (cell + gap)
            y0 = margin + sy * (cell + gap)
            fill = (235, 56, 56, 255) if has_evt else ((40, 12, 12, 255) if col & 0x80 else (24, 8, 8, 255))
            draw.rectangle((x0, y0, x0 + cell, y0 + cell), fill=fill, outline=(115, 26, 26, 255))
            if has_evt:
                draw.text((x0 + 1, y0 + 1), f"{event_cells[(x, disk_y)]:02X}", fill=(255, 255, 255, 255))

    return img


def build_map_dat_wiki_markdown(
    attrib: list[bytes],
    screens: list[tuple[bytes, bytes]],
    events_by_screen: dict[int, list[tuple[int, int, int]]],
    *,
    links: dict[str, str],
    img,
) -> list[str]:
    """Rich map.dat reference page (index tables, events, adjacency — not 60 PNG dumps)."""
    fmt = links.get("map_format", "map-dat-Format")
    attrib_fmt = links.get("attrib_format", "attrib-dat-Format")
    event_fmt = links.get("event_format", "event-dat-Format")
    carto = links.get("maps", "Map-Cartography")

    lines = [
        "# map.dat reference",
        "",
        "Per-screen **512-byte** records in `map.dat` (60 screens × visual + collision pages).",
        "For rendered auto-maps see [Map Cartography](%s); byte layout in [map.dat Format](%s);"
        % (carto, fmt),
        "neighbours and spawn/recall in [attrib.dat Format](%s); scripts in [event.dat Format](%s)."
        % (attrib_fmt, event_fmt),
        "",
        "## File layout",
        "",
        "| Offset | Size | Page | Role |",
        "|--------|------|------|------|",
        "| `+0x000` | 256 | **0 — visual** | 3D wall fields (N/E/S/W × 2 bits): `0` open, `1` wall, `2` torch, `3` door |",
        "| `+0x100` | 256 | **1 — collision** | Per-dir `(dark<<1)|wall`; bit **`0x80`** = event trigger (pairs with `event.dat`) |",
        "",
        "Disk **row 0 = south**; tables below use **column a–p, row 1–16** (`a1` = SW corner).",
        "",
        "## Screen index",
        "",
        "| # | Name | Env | Evts | `event.dat` | Doors | Entry | Recall → | N/E/S/W |",
        "|---|------|-----|------|-------------|-------|-------|----------|---------|",
    ]

    screen_stats: list[dict] = []
    for screen in range(60):
        visual, collision = screens[screen]
        a = attrib[screen]
        stats = analyze_map_screen(visual, collision)
        evts = events_by_screen.get(screen, [])
        name = area_name(screen, attrib)
        entry_x, entry_y = unpack_map_coord(a[0x0E])
        recall_x, recall_y = unpack_map_coord(a[0x16])
        recall_scr = a[0x18]
        if a[0x04] != 0:
            n, e, s, w = (a[0x05], a[0x06], a[0x07], a[0x08])
            neigh = f"{n:02d}/{e:02d}/{s:02d}/{w:02d}"
        else:
            neigh = "—"
        recall = (
            f"{recall_scr:02d} {tile_label(recall_x, recall_y)}"
            if recall_scr != screen or recall_x or recall_y
            else "self"
        )
        lines.append(
            f"| {screen:02d} | {name} | {map_env_label(a, screen)} | "
            f"{stats['events']} | {len(evts)} | {stats['doors']} | "
            f"{tile_label(entry_x, entry_y)} | {recall} | {neigh} |"
        )
        screen_stats.append(
            {
                "screen": screen,
                "name": name,
                "stats": stats,
                "events": evts,
                "attrib": a,
            }
        )

    lines.extend(
        [
            "",
            "**Evts** = collision cells with `0x80` set. **`event.dat`** = triplet count for that",
            "screen index. **Entry** = party spawn (`attrib` +0x0E). **Recall** = cross-map",
            "teleport target (+0x16/+0x18). **N/E/S/W** = overland neighbour screen ids",
            "(interior maps use `—`).",
            "",
            "## Overland connectivity",
            "",
            "Surface sectors (**A1–E4**) from `OUTDOOR_SECTOR_GRID`; elemental planes sit on",
            "the [world map corners](%s) (Air NW, Fire NE, Water SW, Earth SE)."
            % carto,
            "",
            "| Sector | Screen | N | E | S | W |",
            "|--------|--------|---|---|---|---|",
        ]
    )

    for row in OUTDOOR_SECTOR_GRID:
        for screen in row:
            a = attrib[screen]
            sec = area_name(screen, attrib)
            n, e, s, w = a[0x05], a[0x06], a[0x07], a[0x08]
            lines.append(f"| {sec} | {screen:02d} | {n:02d} | {e:02d} | {s:02d} | {w:02d} |")

    for screen, label in ((41, "Air"), (42, "Fire"), (44, "Water"), (43, "Earth")):
        a = attrib[screen]
        n, e, s, w = a[0x05], a[0x06], a[0x07], a[0x08]
        lines.append(f"| Plane {label} | {screen:02d} | {n:02d} | {e:02d} | {s:02d} | {w:02d} |")

    lines.extend(
        [
            "",
            "## Event trigger map (all screens)",
            "",
            "Red cells = `event.dat` triplet position; label = handler id (hex).",
            "Collision `0x80` should cover every triplet (verified on retail `map.dat`).",
            "",
            f"![Event overview]({img('mapdat/event-overview.png')})",
            "",
            "## Reference decode grids",
            "",
            "Full hex + N/E/S/W decode for three representative screens (not all 60):",
            "",
        ]
    )

    ref_labels = {
        0: "town interior (dual page decode)",
        11: "overland hub C2 / Middlegate surface",
        41: "elemental plane (uniform terrain bytes)",
    }
    for screen in MAP_DAT_REFERENCE_SCREENS:
        slug = screen_slug(screen, attrib)
        name = area_name(screen, attrib)
        lines.append(f"### {screen:02d} — {name} ({ref_labels[screen]})")
        lines.append("")
        lines.append(f"![{name} decode]({img(f'mapdat/ref-{slug}.png')})")
        lines.append("")

    lines.extend(
        [
            "## Per-screen detail",
            "",
            "Expand a screen for **event tile list**, terrain ids (overland), and stats.",
            "",
        ]
    )

    for item in screen_stats:
        screen = item["screen"]
        name = item["name"]
        stats = item["stats"]
        evts = item["events"]
        a = item["attrib"]
        entry_x, entry_y = unpack_map_coord(a[0x0E])
        lines.append(f"<details><summary><strong>{screen:02d} — {name}</strong> "
                     f"({len(evts)} events, {stats['doors']} doors)</summary>")
        lines.append("")
        lines.append(
            f"- **Env:** {map_env_label(a, screen)} · **Unique visual bytes:** {stats['unique_visual']} · "
            f"**Torches:** {stats['torches']} · **Dark dirs:** {stats['dark_dirs']}"
        )
        if a[0x04] != 0:
            terr = ", ".join(f"`0x{v:02X}`" for v in stats["terrain_ids"])
            lines.append(f"- **Terrain ids** (`visual & 0x1F`): {terr}")
        lines.append(f"- **Entry:** {tile_label(entry_x, entry_y)} · **Era gate:** `0x{a[0x0F]:02X}`")
        if evts:
            lines.append("")
            lines.append("| Tile | Handler | Cond | Coll `0x80` |")
            lines.append("|------|---------|------|-------------|")
            _, collision = screens[screen]
            for pos, evt, cond in evts[:80]:
                x, y = triplet_tile(pos)
                idx = y * 16 + x
                flag = "yes" if collision[idx] & 0x80 else "**no**"
                lines.append(f"| {tile_label(x, y)} | `{evt:02X}` | `{cond:02X}` | {flag} |")
            if len(evts) > 80:
                lines.append(f"| … | +{len(evts) - 80} more triplets | | |")
        else:
            lines.append("- *No tile-event triplets in `event.dat` for this screen.*")
        lines.append("")
        lines.append("</details>")
        lines.append("")

    return lines


def screen_slug(screen: int, attrib: list[bytes] | None = None) -> str:
    slug = f"{screen:02d}-{area_name(screen, attrib).lower().replace(' ', '-').replace('/', '-')}"
    return "".join(c if c.isalnum() or c in "-_" else "" for c in slug)


def render_map_page_decoded(
    page: bytes,
    *,
    collision: bool,
    title: str,
    cell: int = 26,
) -> Image.Image:
    """16×16 grid with hex + per-direction decode (north-up)."""
    grid = 16
    gap = 2
    margin = 36
    w = margin * 2 + grid * cell + (grid - 1) * gap
    h = margin * 2 + grid * cell + (grid - 1) * gap
    img = Image.new("RGBA", (w, h), (13, 3, 3, 255))
    draw = ImageDraw.Draw(img)
    draw.text((8, 8), title, fill=(242, 237, 237, 255))

    for x in range(grid):
        px = margin + x * (cell + gap) + 8
        draw.text((px, margin - 18), f"{x:X}", fill=(180, 160, 160, 255))
    for sy in range(grid):
        py = margin + sy * (cell + gap) + 8
        map_y = grid - 1 - sy
        draw.text((8, py), f"{map_y:X}", fill=(180, 160, 160, 255))

    for sy in range(grid):
        map_y = grid - 1 - sy
        for x in range(grid):
            v = page[map_y * grid + x]
            n = (v >> 0) & 0x3
            e = (v >> 2) & 0x3
            s = (v >> 4) & 0x3
            wv = (v >> 6) & 0x3
            x0 = margin + x * (cell + gap)
            y0 = margin + sy * (cell + gap)
            x1 = x0 + cell
            y1 = y0 + cell
            fill = (56, 10, 10, 255) if v else (24, 8, 8, 255)
            draw.rectangle((x0, y0, x1, y1), fill=fill, outline=(115, 26, 26, 255), width=1)
            if collision and (v & 0x80):
                draw.ellipse((x1 - 8, y0 + 2, x1 - 2, y0 + 8), fill=(235, 56, 56, 255))
            draw.text((x0 + 2, y0 + 2), f"{v:02X}", fill=(242, 237, 237, 255))
            draw.text((x0 + 2, y0 + 13), f"N{n}E{e}", fill=(190, 220, 255, 255))
            draw.text((x0 + 2, y0 + 22), f"S{s}W{wv}", fill=(190, 220, 255, 255))
            if collision:
                draw.text((x0 + 2, y0 + 31), f"ev:{1 if v & 0x80 else 0}", fill=(255, 160, 160, 255))

    return img


def export_map_dat_grids(data_dir: Path, out: Path) -> None:
    """Event overview montage + a few full decode reference grids (not 60 dumps)."""
    mapdat_dir = out / "mapdat"
    attrib = load_attrib(data_dir)
    screens = load_map_screens(data_dir)
    events_by_screen = load_event_triplets(data_dir)

    overlays: list[Image.Image] = []
    labels: list[str] = []
    for screen in range(60):
        visual, collision = screens[screen]
        name = area_name(screen, attrib)
        evts = events_by_screen.get(screen, [])
        overlays.append(
            render_event_overlay(
                collision,
                evts,
                title=f"{screen:02d}",
                cell=9,
            )
        )
        labels.append(f"{screen:02d} {name[:10]}")

    save_png(
        contact_sheet(overlays, cols=10, pad=4, labels=labels, label_h=12, bg=(13, 3, 3, 255)),
        mapdat_dir / "event-overview.png",
    )
    print(f"  map.dat event overview: 60 screens")

    for screen in MAP_DAT_REFERENCE_SCREENS:
        visual, collision = screens[screen]
        name = area_name(screen, attrib)
        top = render_map_page_decoded(
            visual, collision=False, title=f"{screen:02d} {name} — visual (page 0)"
        )
        bot = render_map_page_decoded(
            collision, collision=True, title=f"{screen:02d} {name} — collision (page 1)"
        )
        combined = Image.new(
            "RGBA",
            (max(top.width, bot.width), top.height + bot.height + 6),
            (13, 3, 3, 255),
        )
        combined.paste(top, (0, 0))
        combined.paste(bot, (0, top.height + 6))
        slug = screen_slug(screen, attrib)
        save_png(combined, mapdat_dir / f"ref-{slug}.png")
        print(f"  map.dat reference grid {screen:02d} {name}")

# ---------------------------------------------------------------------------
# Export sections
# ---------------------------------------------------------------------------


MONSTER_RECORD = 0x1A
MONSTER_PICTURE = 0x15

# Non-monster `.anm` files: NPC portraits (41, 56) and town/overland (61–74).
# 41/56 are the only unused combat-range indices; used for castle king/queen events.
WORLD_SPRITES: list[tuple[int, str, str]] = [
    (41, "Queen (castle NPC)", "npc"),
    (56, "King (castle NPC)", "npc"),
    (61, "City (overland map)", "overland"),
    (69, "Castle (overland map)", "overland"),
    (62, "Blacksmith", "town"),
    (63, "Training guild", "town"),
    (65, "Guild statue", "town"),
    (66, "Tavern / food", "town"),
    (67, "Temple / healer", "town"),
    (68, "Arena / joust", "town"),
    (70, "Treasure chest (blue)", "chest"),
    (72, "Treasure chest (wood)", "chest"),
    (73, "Treasure chest (wood, variant)", "chest"),
    (74, "Treasure chest (wood, animated)", "chest"),
]

WORLD_SPRITE_SECTIONS = {
    "npc": "Castle NPCs (not in monsters.dat)",
    "overland": "Overland map icons",
    "town": "Town services",
    "chest": "Treasure chests",
}


def decode_monster_name(rec: bytes) -> str:
    return "".join(chr((b - 128) & 0x7F) for b in rec[:14]).rstrip(" \x00")


def export_anm_gallery_files(asset: AnmAsset, dest_dir: Path, key: str) -> bool:
    """Write composite thumb PNG, contact sheet, and GIF for one `.anm`. Returns True if GIF written."""
    dest_dir.mkdir(parents=True, exist_ok=True)
    thumb = composite_anm_frame(asset, 0)
    if thumb:
        pad = Image.new("RGBA", (thumb.width + 4, thumb.height + 4), (13, 3, 3, 255))
        pad.paste(thumb, (2, 2), thumb)
        save_png(scale_nearest(pad, 3), dest_dir / f"{key}.png")
        composites = [composite_anm_frame(asset, i) for i in range(asset.frame_count)]
        composites = [c for c in composites if c]
        if composites:
            padded = []
            for c in composites:
                p = Image.new("RGBA", (c.width + 4, c.height + 4), (13, 3, 3, 255))
                p.paste(c, (2, 2), c)
                padded.append(scale_nearest(p, 2))
            save_png(contact_sheet(padded, cols=6, pad=4), dest_dir / f"{key}-sheet.png")
    return save_anm_gif(asset, dest_dir / f"{key}.gif", scale=3)


def export_monsters(data_dir: Path, out: Path) -> dict:
    raw = (data_dir / "monsters.dat").read_bytes()
    by_sprite: dict[int, list[dict]] = {}
    for idx in range(256):
        rec = raw[idx * MONSTER_RECORD : (idx + 1) * MONSTER_RECORD]
        name = decode_monster_name(rec)
        if not name.strip():
            continue
        pic = rec[MONSTER_PICTURE]
        sprite_idx = pic & 0x7F
        if sprite_idx == 0:
            continue
        by_sprite.setdefault(sprite_idx, []).append(
            {
                "index": idx,
                "name": name,
                "picture": pic,
                "large": bool(pic & 0x80),
            }
        )

    monster_dir = out / "monsters"
    meta: dict = {"sprites": {}, "monsters": load_monster_records(data_dir)}

    for sprite_idx in sorted(by_sprite.keys()):
        anm_name = f"{sprite_idx:02d}.anm"
        anm_path = resolve_asset(data_dir, anm_name)
        key = f"{sprite_idx:02d}"
        entry: dict = {
            "file": anm_name,
            "monsters": by_sprite[sprite_idx],
            "frames": 0,
            "thumbnail": f"/gallery/monsters/{key}.png",
            "gif": f"/gallery/monsters/{key}.gif",
            "sheet": f"/gallery/monsters/{key}-sheet.png",
        }
        if not anm_path:
            entry["error"] = "missing"
            meta["sprites"][key] = entry
            continue

        asset = load_anm_asset(anm_path)
        entry["frames"] = asset.frame_count
        if export_anm_gallery_files(asset, monster_dir, key):
            entry["has_gif"] = True
        meta["sprites"][key] = entry
        print(f"  monster {anm_name}: {asset.frame_count} frames, {len(by_sprite[sprite_idx])} names")

    (monster_dir / "index.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    return meta


def export_world_sprites(data_dir: Path, out: Path) -> dict:
    """Export NPC / town / overland / chest `.anm` files not indexed by monsters.dat."""
    world_dir = out / "world"
    meta: dict = {"sprites": {}}
    for sprite_idx, label, category in WORLD_SPRITES:
        anm_name = f"{sprite_idx:02d}.anm"
        anm_path = resolve_asset(data_dir, anm_name)
        key = f"{sprite_idx:02d}"
        entry: dict = {
            "file": anm_name,
            "label": label,
            "category": category,
            "frames": 0,
            "thumbnail": f"/gallery/world/{key}.png",
            "gif": f"/gallery/world/{key}.gif",
            "sheet": f"/gallery/world/{key}-sheet.png",
        }
        if not anm_path:
            entry["error"] = "missing"
            meta["sprites"][key] = entry
            continue
        asset = load_anm_asset(anm_path)
        entry["frames"] = asset.frame_count
        if export_anm_gallery_files(asset, world_dir, key):
            entry["has_gif"] = True
        meta["sprites"][key] = entry
        print(f"  world {anm_name}: {label} ({asset.frame_count} frames)")
    (world_dir / "index.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    return meta


def build_monster_sprites_wiki_markdown(monster_meta: dict, *, links: dict[str, str], img) -> list[str]:
    """`.anm` combat sprite animations — GIFs and contact sheets."""
    fmt = links.get("monsters_format", "monsters-dat-Format")
    catalog = links.get("monsters", "Monsters")
    world = links.get("world_sprites", "World-Sprites")
    lines = [
        "# Monster sprites (`.anm`)",
        "",
        "Combat animation files referenced by `monsters.dat` byte **`0x15 & 0x7F`**. "
        "GIFs use TV-prelude composite frames at **quarter speed** (same compositing as MM2ED). "
        f"Per-monster stats: [{catalog}]({catalog}). NPC / town / overland sprites "
        f"(incl. king `56.anm`, queen `41.anm`): [{world}]({world}). "
        f"Byte layout: [monsters.dat Format]({fmt}).",
        "",
    ]
    sprites = monster_meta.get("sprites", {})
    for key in sorted(sprites.keys(), key=lambda k: int(k)):
        e = sprites[key]
        if e.get("error"):
            continue
        names = ", ".join(m["name"] for m in e.get("monsters", []))
        lines.append(f"### `{e['file']}` — {names}")
        lines.append("")
        if e.get("has_gif"):
            lines.append(f"![{e['file']} animation]({img(f'monsters/{key}.gif', bust=True)})")
        else:
            lines.append(f"![{e['file']}]({img(f'monsters/{key}.png', bust=True)})")
        lines.append("")
        lines.append(f"![{e['file']} contact sheet]({img(f'monsters/{key}-sheet.png', bust=True)})")
        lines.append("")

    return lines


def build_world_sprites_wiki_markdown(world_meta: dict, *, links: dict[str, str], img) -> list[str]:
    """Town, overland, and chest `.anm` sprites (not in monsters.dat)."""
    fmt = links.get("anm_format", "ANM-TV-Format")
    monsters = links.get("monster_sprites", "Monster-Sprites")
    lines = [
        "# World sprites (`.anm`)",
        "",
        "Non-combat and story `.anm` files: **castle king/queen** (`41.anm`, `56.anm` — "
        "the only `01`–`60` sprites not referenced by `monsters.dat`), **overland map icons**, "
        "**town service screens** (blacksmith, tavern, temple, training, arena), and "
        "**treasure chests**. "
        f"Combat monsters: [{monsters}]({monsters}). Format: [ANM TV Format]({fmt}).",
        "",
    ]
    sprites = world_meta.get("sprites", {})
    by_cat: dict[str, list[tuple[str, dict]]] = {}
    for key, entry in sprites.items():
        cat = entry.get("category", "other")
        by_cat.setdefault(cat, []).append((key, entry))
    for cat in ("npc", "overland", "town", "chest"):
        section = WORLD_SPRITE_SECTIONS.get(cat, cat)
        entries = by_cat.get(cat, [])
        if not entries:
            continue
        lines.append(f"## {section}")
        lines.append("")
        for key, e in sorted(entries, key=lambda t: int(t[0])):
            if e.get("error"):
                lines.append(f"### `{e['file']}` — {e.get('label', '?')} *(missing)*")
                lines.append("")
                continue
            label = e.get("label", key)
            lines.append(f"### `{e['file']}` — {label}")
            lines.append("")
            if e.get("has_gif"):
                lines.append(f"![{label}]({img(f'world/{key}.gif', bust=True)})")
            else:
                lines.append(f"![{label}]({img(f'world/{key}.png', bust=True)})")
            lines.append("")
            lines.append(f"![{label} contact sheet]({img(f'world/{key}-sheet.png', bust=True)})")
            lines.append("")

    return lines


def build_monsters_catalog_wiki_markdown(monster_meta: dict, *, links: dict[str, str], img) -> list[str]:
    """`monsters.dat` catalog — one index table with sprite thumbs and decoded stats."""
    fmt = links.get("monsters_format", "monsters-dat-Format")
    sprites_page = links.get("monster_sprites", "Monster-Sprites")
    lines = [
        "# Monsters (`monsters.dat`)",
        "",
        "All named combat monsters from **`monsters.dat`** (256 × 26 bytes). "
        f"Animated sprites: [{sprites_page}]({sprites_page}). "
        f"Record layout: [monsters.dat Format]({fmt}).",
        "",
        "HP / XP / AC / damage decoded with MM2ED formulas (unpacker asm `0x4C8E`). "
        "Treasure columns are from byte `0x10` (gold **tier**, gem flag, item-drop level — "
        "actual gp uses RNG at asm `0x10B74`).",
        "",
        "## Monster index",
        "",
        "| # | Sprite | Name | HP | XP | AC | Dmg | Gems | Gold tier | Item drop | Large | "
        "Group attack | Single effect | Friends | Flags |",
        "|---|--------|------|----|----|----|-----|------|-----------|-----------|-------|"
        "--------------|---------------|---------|-------|",
    ]
    sprites = monster_meta.get("sprites", {})
    for m in monster_meta.get("monsters", []):
        key = f"{m['sprite']:02d}" if m["sprite"] else None
        sp = sprites.get(key or "", {})
        if key and not sp.get("error"):
            sprite_cell = f"![{m['name']}]({img(f'monsters/{key}.png', bust=True)})"
        else:
            sprite_cell = "—"
        flags = ", ".join(m["flags"]) if m["flags"] else "—"
        gem = "yes" if m["drops_gems"] else "no"
        lines.append(
            f"| {m['index']} | {sprite_cell} | {m['name']} | "
            f"{m['hp']} | {m['xp']} | {m['ac_val']} | {m['damage_val']} | "
            f"{gem} | {m['gold_tier']} | {m['item_drop_level']} | "
            f"{'yes' if m['large'] else 'no'} | "
            f"{m['party_verb_name']} | {m['single_effect_name']} | "
            f"{m['add_friends']} | {flags} |"
        )

    return lines


def build_items_wiki_markdown(items: list[dict], *, links: dict[str, str], img) -> list[str]:
    fmt = links.get("items_format", "items-dat-Format")
    spells = links.get("spells_format", "Spells-and-Item-Use")

    lines = [
        "# Items catalog",
        "",
        "All **256** `items.dat` records (20 bytes each). Use-power bytes map to spells via",
        f"[Spells and Item Use]({spells}); record layout in [items.dat Format]({fmt}).",
        "",
        "## Item index",
        "",
        "| # | Name | Gold | Dmg | Equipped bonus | Use power | Forbidden classes |",
        "|---|------|------|-----|----------------|-----------|-------------------|",
    ]
    for it in items:
        name = it["name"] if it["name"] else "*(empty)*"
        forbidden = ", ".join(it["forbidden"][:3])
        if len(it["forbidden"]) > 3:
            forbidden += f" (+{len(it['forbidden']) - 3})"
        if not forbidden:
            forbidden = "—"
        lines.append(
            f"| {it['index']} | {name} | {it['gold']} | {it['damage']} | "
            f"{it['bonus_text']} | {it['effect_text']} | {forbidden} |"
        )

    named = [it for it in items if it["name"]]
    lines.append("")
    lines.append(f"*({len(named)} named items, {256 - len(named)} empty slots.)*")
    return lines

def export_tilesets(data_dir: Path, out: Path) -> list[str]:
    tile_dir = out / "tilesets"
    exported: list[str] = []
    sheets = sorted(p.name for p in data_dir.glob("*.32"))
    for sheet in sheets:
        if sheet in NON_GFX_32:
            print(f"  skip {sheet}: not planar gfx (XOR data blob)")
            continue
        if not resolve_asset(data_dir, sheet):
            continue
        n = frame_count_32(sheet, data_dir)
        if n <= 0 or n > 256:
            print(f"  skip {sheet}: not a planar image chunk (frame_count={n})")
            continue
        try:
            montage = composite_sheet_montage(sheet, max_frames=n, data_dir=data_dir)
        except Exception as exc:
            print(f"  skip {sheet}: {exc}")
            continue
        stem = sheet.replace(".32", "")
        save_png(montage, tile_dir / f"{stem}-sheet.png")
        exported.append(sheet)
        print(f"  tileset {sheet}: {n} frames")
    return exported


CARTO_SHEETS = ("townb.32", "outb.32")


def export_carto_tiles(data_dir: Path, out: Path) -> None:
    carto_dir = out / "carto"
    for sheet in CARTO_SHEETS:
        frames = load_all_frames(sheet, data_dir)
        if not frames:
            continue
        labels = [CARTO_FRAME_LABELS.get(i, str(i)) for i in range(len(frames))]
        scaled = [scale_nearest(f, 3) for f in frames]
        stem = sheet.replace(".32", "")
        save_png(
            contact_sheet(scaled, cols=6, pad=4, labels=labels, label_h=10),
            carto_dir / f"{stem}-frames.png",
        )
        print(f"  carto {sheet}: {len(frames)} tiles")


def render_carto_screen(
    screen: int,
    visual: bytes,
    *,
    outdoor: bool,
    tiles: list[Image.Image],
    scale: int = 1,
) -> Image.Image:
    """16×16 auto-map at native carto tile size × scale (14×11 px per tile at scale 1)."""
    tw, th = 14 * scale, 11 * scale
    grid = Image.new("RGBA", (16 * tw, 16 * th), (13, 3, 3, 255))
    for sy in range(16):  # screen row 0 = north; disk row 0 = south
        disk_row = 15 - sy
        for col in range(16):
            cell = visual[disk_row * 16 + col]
            frame = carto_frame(screen, cell, outdoor)
            if frame >= len(tiles):
                continue
            tile = scale_nearest(tiles[frame], scale) if scale != 1 else tiles[frame]
            grid.paste(tile, (col * tw, sy * th), tile)
    return grid


def _native_carto_sector(
    screen: int,
    screens: list[tuple[bytes, bytes]],
    outb: list[Image.Image],
    cache: dict[int, Image.Image] | None,
) -> Image.Image:
    if cache and screen in cache:
        return cache[screen]
    visual, _ = screens[screen]
    return render_carto_screen(screen, visual, outdoor=True, tiles=outb, scale=1)


def export_world_overland_map(
    screens: list[tuple[bytes, bytes]],
    outb: list[Image.Image],
    out: Path,
    *,
    screen_grids: dict[int, Image.Image] | None = None,
) -> None:
    """Stitch overland A1–E4 (center 5×4) + elemental planes at corners, 1:1 native scale."""
    maps_dir = out / "maps"
    sw, sh = 16 * 14, 16 * 11
    bg = (13, 3, 3, 255)
    world = Image.new("RGBA", (WORLD_MAP_COLS * sw, WORLD_MAP_ROWS * sh), bg)

    ox, oy = OVERLAND_GRID_ORIGIN
    for row_idx, row in enumerate(OUTDOOR_SECTOR_GRID):
        for col_idx, screen in enumerate(row):
            sector = _native_carto_sector(screen, screens, outb, screen_grids)
            world.paste(sector, ((ox + col_idx) * sw, (oy + row_idx) * sh), sector)

    for screen, (cx, cy) in ELEMENTAL_PLANE_CORNERS.items():
        sector = _native_carto_sector(screen, screens, outb, screen_grids)
        world.paste(sector, (cx * sw, cy * sh), sector)

    draw = ImageDraw.Draw(world)
    for (cx, cy), label in WORLD_MAP_CORNER_LABELS.items():
        x, y = cx * sw + 6, cy * sh + 6
        tw = len(label) * 6 + 8
        draw.rectangle([x - 4, y - 2, x + tw, y + 11], fill=(13, 3, 3, 220))
        draw.text((x, y), label, fill=(242, 237, 237, 255))

    out_path = maps_dir / WORLD_OVERLAND_MAP_FILE
    legacy = maps_dir / "world-overland-1x1.png"
    if legacy.exists():
        legacy.unlink()
    save_png(world, out_path)
    print(
        f"  world overland 1:1: {world.width}×{world.height} px "
        f"({WORLD_MAP_COLS}×{WORLD_MAP_ROWS} cells, planes at corners)"
    )


def export_map_cartography(data_dir: Path, out: Path, attrib: list[bytes]) -> None:
    maps_dir = out / "maps"
    screens = load_map_screens(data_dir)
    townb = load_all_frames("townb.32", data_dir)
    outb = load_all_frames("outb.32", data_dir)
    scale = 2
    thumb_w, thumb_h = 16 * 14, 16 * 11

    thumbs: dict[int, Image.Image] = {}
    native_outdoor: dict[int, Image.Image] = {}

    def overview_label(screen: int) -> str:
        return f"{screen:02d} {area_name(screen, attrib)}"

    for screen in range(60):
        visual, _collision = screens[screen]
        outdoor = is_outdoor_screen(screen, attrib)
        use_outb = carto_use_outb(screen, attrib)
        tiles = outb if use_outb else townb
        if not tiles:
            continue

        grid = render_carto_screen(
            screen, visual, outdoor=use_outb, tiles=tiles, scale=scale
        )
        if use_outb:
            native_outdoor[screen] = render_carto_screen(
                screen, visual, outdoor=True, tiles=outb, scale=1
            )

        save_png(grid, maps_dir / f"{screen_slug(screen, attrib)}.png")
        thumbs[screen] = grid.resize((thumb_w, thumb_h), Image.NEAREST)

        print(f"  map screen {screen:02d} {area_name(screen, attrib)} ({'outdoor' if outdoor else 'indoor'})")

    export_world_overland_map(screens, outb, out, screen_grids=native_outdoor)

    for slug, title, screen_list, cols in MAP_OVERVIEW_GROUPS:
        cells = [thumbs[s] for s in screen_list if s in thumbs]
        labels = [overview_label(s) for s in screen_list if s in thumbs]
        if not cells:
            continue
        save_png(
            contact_sheet(cells, cols=cols, pad=2, labels=labels, label_h=11),
            maps_dir / f"{slug}.png",
        )
        print(f"  overview {slug}: {len(cells)} screens ({title})")


def export_3d_views(data_dir: Path, out: Path) -> None:
    views_dir = out / "views"
    jobs = [
        ("indoor-corridor", lambda: composite_indoor(data_dir=data_dir)),
        ("backdrop-indoor", lambda: composite_backdrop("townf.32", "sky.32", data_dir)),
        ("backdrop-outdoor", lambda: composite_backdrop("outf.32", "sky.32", data_dir)),
        ("outdoor-terrain0", lambda: composite_outdoor(0, 0, data_dir=data_dir)),
        ("outdoor-terrain1", lambda: composite_outdoor(1, 0, data_dir=data_dir)),
    ]
    for name, fn in jobs:
        try:
            save_png(fn(), views_dir / f"{name}.png")
            print(f"  view {name}")
        except Exception as exc:
            print(f"  skip view {name}: {exc}")

    for wall_sheet, (floor_sheet, sky_sheet) in INDOOR_WALLSETS.items():
        if not resolve_asset(data_dir, wall_sheet):
            continue
        stem = wall_sheet.replace(".32", "")
        try:
            grid = composite_view_grid(
                wall_sheet,
                data_dir,
                indoor_frame_grid_views(),
                floor_sheet=floor_sheet,
                sky_sheet=sky_sheet,
            )
            save_png(grid, views_dir / f"{stem}-layout.png")
            print(f"  view {stem}-layout")
        except Exception as exc:
            print(f"  skip {stem}-layout: {exc}")

    for sheet in ("outdoor1.32", "outdoor2.32", "outdoor3.32"):
        if not resolve_asset(data_dir, sheet):
            continue
        stem = sheet.replace(".32", "")
        try:
            save_png(composite_sheet_montage(sheet, 8, data_dir), views_dir / f"{stem}-frames.png")
            print(f"  view {stem}-frames")
        except Exception as exc:
            print(f"  skip {stem}: {exc}")


# ---------------------------------------------------------------------------
# Wiki markdown generation
# ---------------------------------------------------------------------------


def write_gallery_markdown(
    out_docs: Path,
    monster_meta: dict,
    world_meta: dict,
    tilesets: list[str],
    *,
    image_prefix: str = "/gallery",
    use_frontmatter: bool = True,
    page_names: dict[str, str] | None = None,
    page_links: dict[str, str] | None = None,
    attrib: list[bytes] | None = None,
) -> None:
    """Write gallery markdown pages. Use page_names/page_links for GitHub Wiki output."""
    out_docs.mkdir(parents=True, exist_ok=True)
    if attrib is None:
        attrib = load_attrib(resolve_data_dir())

    def img(rel: str, *, bust: bool = False) -> str:
        path = f"{image_prefix.rstrip('/')}/{rel.lstrip('/')}"
        if bust:
            path += f"?v={GALLERY_CACHE_VERSION}"
        return path

    files = {
        "index": "index.md",
        "monster_sprites": "monster-sprites.md",
        "world_sprites": "world-sprites.md",
        "monsters": "monsters.md",
        "items": "items.md",
        "tilesets": "tilesets.md",
        "maps": "maps.md",
        "map_dat": "map-dat.md",
        "views": "views.md",
        **(page_names or {}),
    }
    links = {
        "monster_sprites": "/docs/gallery/monster-sprites",
        "world_sprites": "/docs/gallery/world-sprites",
        "monsters": "/docs/gallery/monsters",
        "items": "/docs/gallery/items",
        "tilesets": "/docs/gallery/tilesets",
        "maps": "/docs/gallery/maps",
        "map_dat": "/docs/gallery/map-dat",
        "views": "/docs/gallery/views",
        "map_format": "map-dat-Format",
        "attrib_format": "attrib-dat-Format",
        "event_format": "event-dat-Format",
        "monsters_format": "monsters-dat-Format",
        "anm_format": "ANM-TV-Format",
        "items_format": "items-dat-Format",
        "spells_format": "Spells-and-Item-Use",
        **(page_links or {}),
    }

    def fm(title: str) -> list[str]:
        if not use_frontmatter:
            return []
        return ["---", f"title: {title}", "---", ""]

    # Monster sprites + monsters catalog (separate wiki pages)
    sprite_lines = build_monster_sprites_wiki_markdown(monster_meta, links=links, img=img)
    if use_frontmatter:
        sprite_lines = [*fm("Monster Sprites"), *sprite_lines]
    (out_docs / files["monster_sprites"]).write_text("\n".join(sprite_lines), encoding="utf-8")

    world_lines = build_world_sprites_wiki_markdown(world_meta, links=links, img=img)
    if use_frontmatter:
        world_lines = [*fm("World Sprites"), *world_lines]
    (out_docs / files["world_sprites"]).write_text("\n".join(world_lines), encoding="utf-8")

    catalog_lines = build_monsters_catalog_wiki_markdown(monster_meta, links=links, img=img)
    if use_frontmatter:
        catalog_lines = [*fm("Monsters"), *catalog_lines]
    (out_docs / files["monsters"]).write_text("\n".join(catalog_lines), encoding="utf-8")

    # Items page
    items = load_item_records(resolve_data_dir())
    item_lines = build_items_wiki_markdown(items, links=links, img=img)
    if use_frontmatter:
        item_lines = [*fm("Items Catalog"), *item_lines]
    (out_docs / files["items"]).write_text("\n".join(item_lines), encoding="utf-8")

    # Tilesets page
    lines = [
        *fm("Tilesets (.32)"),
        "# Graphics tilesets (`.32`)",
        "",
        "Planar 32-color image-chunk sheets (walls, floors, UI sprites, auto-map",
        "tiles). **Not every file with a `.32` extension is graphics** — the runtime",
        "filename table is a unified resource list. These are **excluded** here:",
        "",
        "| File | Actual contents |",
        "|------|-----------------|",
        "| `globe.32` | XOR-obfuscated **copy-protection string tables** (manual page prompts); see `tools/decode_globe_amiga.py` and `20-copy-protection-table.md` |",
        "| `disk.32` | XOR data blob (not a tile sheet) |",
        "",
        "Each montage below shows every frame in file order.",
        "",
    ]
    for sheet in tilesets:
        stem = sheet.replace(".32", "")
        n = frame_count_32(sheet, ROOT)
        lines.append(f"## `{sheet}` ({n} frames)")
        lines.append("")
        lines.append(f"![{sheet}]({img(f'tilesets/{stem}-sheet.png')})")
        lines.append("")
    (out_docs / files["tilesets"]).write_text("\n".join(lines), encoding="utf-8")

    # Maps page
    lines = [
        *fm("Map Cartography"),
        "# Auto-map cartography",
        "",
        "Each screen is a **16×16** auto-map render using `map.dat` page 0 (visual tiles)",
        "and the cartography rules from `Cartography.h` / asm `@0x2182`.",
        "Outdoor/surface screens use **`outb.32`**; interiors use **`townb.32`**.",
        "",
        "**North is at the top** (disk row 15); row 0 on disk is south — same as MM2ED",
        "and the in-game Location spell map. Raw bytes: [map.dat grids]({})".format(
            links.get("map_dat", "/docs/gallery/map-dat")
        ),
        "",
        "## Overview by area type",
        "",
        "All **60** map screens (`map.dat` indices 0–59), grouped like the game world.",
        "",
        "### Towns (0–4)",
        "",
        f"![Towns]({img('maps/overview-towns.png', bust=True)})",
        "",
        "### Town caverns (17–21)",
        "",
        f"![Town caverns]({img('maps/overview-town-caverns.png', bust=True)})",
        "",
        "### Caves (22–32)",
        "",
        f"![Caves]({img('maps/overview-caves.png', bust=True)})",
        "",
        "### Outdoor overland — 5×4 sectors (A1–E4)",
        "",
        "Twenty surface maps in **column A–E, row 1–4** order (north at top), named",
        "by world sector (**A1** … **E4** from `attrib.dat` +0x15). **C2** is Middlegate",
        "overland; town maps are separate interior screens 0–4.",
        "",
        f"![Outdoor overland]({img('maps/overview-outdoor.png', bust=True)})",
        "",
        "### World overland map (1:1)",
        "",
        "Full **7×6** stitch at native auto-map resolution (**14×11** px per carto tile):",
        "center **5×4** surface sectors (**A1–E4**) plus **elemental planes** in the corners",
        "— **Air** NW, **Fire** NE, **Water** SW, **Earth** SE (**1568×1056** px, **112×96** cells).",
        "Town interiors (screens 0–4) appear as terrain icons on the center block.",
        "",
        f"![World overland 1:1]({img(f'maps/{WORLD_OVERLAND_MAP_FILE}', bust=True)})",
        "",
        "### Elemental planes (41–44)",
        "",
        f"![Elemental planes]({img('maps/overview-planes.png', bust=True)})",
        "",
        "### Castle dungeons (45–52)",
        "",
        "Two basement levels per castle: Hillstone, Woodhaven, Pinehurst, Luxus Palace.",
        "",
        f"![Castle dungeons]({img('maps/overview-castle-dungeons.png', bust=True)})",
        "",
        "### Castles (55–59)",
        "",
        f"![Castles]({img('maps/overview-castles.png', bust=True)})",
        "",
        "### Ancients (53–54)",
        "",
        f"![Ancients]({img('maps/overview-ancients.png', bust=True)})",
        "",
        "## Carto tilesets",
        "",
        "### `townb.32` (interior dungeon auto-map, 36 frames)",
        "",
        f"![townb frames]({img('carto/townb-frames.png')})",
        "",
        "### `outb.32` (outdoor surface auto-map, 36 frames)",
        "",
        f"![outb frames]({img('carto/outb-frames.png')})",
        "",
        "## Per-screen maps",
        "",
    ]
    for screen in range(60):
        slug = screen_slug(screen, attrib)
        name = area_name(screen, attrib)
        lines.append(f"### {screen:02d} — {name}")
        lines.append("")
        lines.append(f"![{name}]({img(f'maps/{slug}.png', bust=True)})")
        lines.append("")
    (out_docs / files["maps"]).write_text("\n".join(lines), encoding="utf-8")

    # map.dat reference (tables + event maps, not 60 full decode PNGs)
    data_dir = resolve_data_dir()
    map_dat_lines = build_map_dat_wiki_markdown(
        attrib,
        load_map_screens(data_dir),
        load_event_triplets(data_dir),
        links=links,
        img=img,
    )
    if use_frontmatter:
        map_dat_lines = [*fm("map.dat Reference"), *map_dat_lines]
    (out_docs / files["map_dat"]).write_text("\n".join(map_dat_lines), encoding="utf-8")

    # Views page
    lines = [
        *fm("3D View Sprites"),
        "# First-person view graphics",
        "",
        "Reference composites built with the same blit tables as `render_view_refs.py`",
        "and the MM2ED `View3D` preview.",
        "",
        "## Indoor corridor (town walls)",
        "",
        f"![indoor corridor]({img('views/indoor-corridor.png')})",
        "",
        "## Backdrops",
        "",
        "| Indoor (townf + sky) | Outdoor (outf + sky) |",
        "|----------------------|----------------------|",
        f"| ![indoor]({img('views/backdrop-indoor.png')}) | ![outdoor]({img('views/backdrop-outdoor.png')}) |",
        "",
        "## Wall-set layout grids",
        "",
    ]
    for stem in ("town", "cave", "castle"):
        lines.append(f"### `{stem}.32` viewport slots")
        lines.append("")
        lines.append(f"![{stem} layout]({img(f'views/{stem}-layout.png')})")
        lines.append("")
    lines.extend([
        "## Outdoor horizon layers",
        "",
    ])
    for stem in ("outdoor1", "outdoor2", "outdoor3"):
        lines.append(f"### `{stem}.32`")
        lines.append("")
        lines.append(f"![{stem}]({img(f'views/{stem}-frames.png')})")
        lines.append("")
    lines.extend([
        "## Outdoor terrain samples",
        "",
        "| Terrain 0 | Terrain 1 |",
        "|-----------|-----------|",
        f"| ![t0]({img('views/outdoor-terrain0.png')}) | ![t1]({img('views/outdoor-terrain1.png')}) |",
        "",
    ])
    (out_docs / files["views"]).write_text("\n".join(lines), encoding="utf-8")

    # Index
    index = [
        *fm("Sprite Gallery"),
        "# Sprite gallery",
        "",
        "Decoded game graphics exported from original `.32` / `.anm` assets.",
        "",
        "| Section | Contents |",
        "|---------|----------|",
        f"| [Monster sprites]({links['monster_sprites']}) | `.anm` combat animations (GIF) |",
        f"| [World sprites]({links['world_sprites']}) | NPC king/queen, town, overland, chest `.anm` |",
        f"| [Monsters]({links['monsters']}) | Full `monsters.dat` catalog |",
        f"| [Items]({links['items']}) | All 256 `items.dat` records |",
        f"| [Tilesets]({links['tilesets']}) | Planar `.32` image sheets (not `globe.32`) |",
        f"| [Maps]({links['maps']}) | Auto-map cartography for 60 screens |",
        f"| [map.dat]({links['map_dat']}) | Screen index, events, adjacency, decode refs |",
        f"| [3D Views]({links['views']}) | First-person wall/floor/sky compositing |",
        "",
    ]
    (out_docs / files["index"]).write_text("\n".join(index), encoding="utf-8")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def export_all(
    gallery_out: Path,
    docs_out: Path | None = None,
    data_dir: Path | None = None,
    *,
    image_prefix: str = "/gallery",
    gallery_md_opts: dict | None = None,
) -> None:
    data = resolve_data_dir(data_dir)
    print(f"data dir: {data}")
    print(f"gallery out: {gallery_out}")

    if gallery_out.exists():
        import shutil
        shutil.rmtree(gallery_out)
    gallery_out.mkdir(parents=True)

    print("Exporting monsters…")
    monster_meta = export_monsters(data, gallery_out)

    print("Exporting world sprites…")
    world_meta = export_world_sprites(data, gallery_out)

    print("Exporting tilesets…")
    tilesets = export_tilesets(data, gallery_out)

    print("Exporting carto tiles…")
    export_carto_tiles(data, gallery_out)

    print("Exporting map cartography…")
    attrib = load_attrib(data)
    export_map_cartography(data, gallery_out, attrib)

    print("Exporting map.dat grids…")
    export_map_dat_grids(data, gallery_out)

    print("Exporting 3D views…")
    export_3d_views(data, gallery_out)

    if docs_out:
        print("Writing gallery markdown…")
        opts = {"image_prefix": image_prefix, "attrib": attrib, **(gallery_md_opts or {})}
        write_gallery_markdown(docs_out, monster_meta, world_meta, tilesets, **opts)

    print("Done.")


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Export MM2 sprite gallery for the wiki")
    ap.add_argument("--data", type=Path, default=None, help="Directory with .dat/.32/.anm")
    ap.add_argument(
        "--out",
        type=Path,
        default=ROOT / "wiki" / "public" / "gallery",
        help="Output directory for PNG assets",
    )
    ap.add_argument(
        "--docs",
        type=Path,
        default=ROOT / "wiki" / "docs" / "gallery",
        help="Output directory for generated markdown pages",
    )
    ap.add_argument("--no-docs", action="store_true", help="Skip markdown generation")
    args = ap.parse_args()

    export_all(
        args.out,
        docs_out=None if args.no_docs else args.docs,
        data_dir=args.data,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
