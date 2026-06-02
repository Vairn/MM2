#!/usr/bin/env python3
"""Might and Magic I (DOS) MAZEDATA.DTA — same 512-byte screen layout as MM2 map.dat."""
from __future__ import annotations

import re
import struct
from pathlib import Path
from typing import Iterator

MM1_MAP_SCREENS = 55
MM1_MAP_SCREEN_SIZE = 512
MM1_MAP_PAGE_SIZE = 256
MM1_MAP_FILE_SIZE = MM1_MAP_SCREENS * MM1_MAP_SCREEN_SIZE

# Ordered filename table in MM.EXE @ ~0x10C07 (one slug per MAZEDATA screen).
MM1_MAP_SLUGS: tuple[str, ...] = (
    "sorpigal",
    "portsmit",
    "algary",
    "dusk",
    "erliquin",
    "cave1",
    "cave2",
    "cave3",
    "cave4",
    "cave5",
    "cave6",
    "cave7",
    "cave8",
    "cave9",
    "areaa1",
    "areaa2",
    "areaa3",
    "areaa4",
    "areab1",
    "areab2",
    "areab3",
    "areab4",
    "areac1",
    "areac2",
    "areac3",
    "areac4",
    "aread1",
    "aread2",
    "aread3",
    "aread4",
    "areae1",
    "areae2",
    "areae3",
    "areae4",
    "doom",
    "blackrn",
    "blackrs",
    "qvl1",
    "qvl2",
    "rwl1",
    "rwl2",
    "enf1",
    "enf2",
    "whitew",
    "dragad",
    "udrag1",
    "udrag2",
    "udrag3",
    "demon",
    "alamar",
    "pp1",
    "pp2",
    "pp3",
    "pp4",
    "astral",
)

# Display titles (slug table is authoritative for screen index; titles are editorial).
MM1_MAP_TITLES: dict[str, str] = {
    "sorpigal": "Sorpigal",
    "portsmit": "Portsmith",
    "algary": "Algary",
    "dusk": "Dusk",
    "erliquin": "Erliquin",
    "doom": "Doom",
    "blackrn": "Black Ridge (north)",
    "blackrs": "Black Ridge (south)",
    "qvl1": "Queen's Pyramids (1)",
    "qvl2": "Queen's Pyramids (2)",
    "rwl1": "Rainbow Road (1)",
    "rwl2": "Rainbow Road (2)",
    "enf1": "Enchanted Forest (1)",
    "enf2": "Enchanted Forest (2)",
    "whitew": "White Wolf",
    "dragad": "Dragadune",
    "udrag1": "Under Dragadune (1)",
    "udrag2": "Under Dragadune (2)",
    "udrag3": "Under Dragadune (3)",
    "demon": "Demon",
    "alamar": "Alamar",
    "pp1": "Protection Point (1)",
    "pp2": "Protection Point (2)",
    "pp3": "Protection Point (3)",
    "pp4": "Protection Point (4)",
    "astral": "Astral Plane",
}


def mm1_map_title(slug: str) -> str:
    if slug in MM1_MAP_TITLES:
        return MM1_MAP_TITLES[slug]
    if slug.startswith("cave") and slug[4:].isdigit():
        return f"Cave {slug[4:]}"
    if re.fullmatch(r"area[a-e][1-4]", slug):
        return f"Overland {slug[4].upper()}{slug[5]}"
    return slug.replace("_", " ").title()


def parse_mm1_map_slugs_from_exe(exe: bytes) -> list[str]:
    """Read the null-terminated slug list starting at the first 'sorpigal' in MM.EXE."""
    start = exe.find(b"sorpigal\x00")
    if start < 0:
        raise ValueError("sorpigal map slug not found in MM.EXE")
    names: list[str] = []
    off = start
    while off < len(exe):
        end = exe.find(b"\x00", off)
        if end <= off:
            break
        raw = exe[off:end]
        if not raw or not all(97 <= c <= 122 or 48 <= c <= 57 for c in raw):
            break
        names.append(raw.decode("ascii"))
        off = end + 1
    if len(names) != MM1_MAP_SCREENS:
        raise ValueError(f"expected {MM1_MAP_SCREENS} map slugs, got {len(names)}")
    return names


def load_mm1_mazedata(path: Path) -> list[tuple[bytes, bytes]]:
    data = path.read_bytes()
    if len(data) < MM1_MAP_FILE_SIZE:
        raise ValueError(f"MAZEDATA.DTA too small: {len(data)} bytes (need {MM1_MAP_FILE_SIZE})")
    screens: list[tuple[bytes, bytes]] = []
    for i in range(MM1_MAP_SCREENS):
        off = i * MM1_MAP_SCREEN_SIZE
        visual = data[off : off + MM1_MAP_PAGE_SIZE]
        collision = data[off + MM1_MAP_PAGE_SIZE : off + MM1_MAP_SCREEN_SIZE]
        screens.append((visual, collision))
    return screens


def collision_blocks(card: int, direction: int) -> bool:
    """Per-direction wall bit (MM2 collision page)."""
    return ((card >> (direction * 2)) & 1) != 0


def find_entry(collision: bytes) -> tuple[int, int]:
    """First fully open cell; fallback (8, 8)."""
    for y in range(16):
        for x in range(16):
            cell = collision[(y << 4) | x]
            if not any(collision_blocks(cell, d) for d in range(4)):
                return x, y
    return 8, 8


_MM1_TOWN_SLUGS = frozenset(
    {
        "sorpigal",
        "portsmit",
        "algary",
        "dusk",
        "erliquin",
        "alamar",
        "blackrn",
        "blackrs",
    }
)
_MM1_CAVERN_SLUGS = frozenset(
    {
        "doom",
        "demon",
        "dragad",
        "astral",
        "whitew",
        "enf1",
        "enf2",
        "qvl1",
        "qvl2",
        "rwl1",
        "rwl2",
        "pp1",
        "pp2",
        "pp3",
        "pp4",
    }
)


def mm1_env_for_slug(slug: str) -> str:
    """MM2 indoor sheet set for walker (town / cavern / castle). Overland uses outdoor flag."""
    if slug.startswith("area"):
        return "outside"
    if slug.startswith("cave") or slug.startswith("udrag") or slug in _MM1_CAVERN_SLUGS:
        return "cavern"
    if slug in _MM1_TOWN_SLUGS:
        return "town"
    return "cavern"


def iter_mm1_screens(
    mazedata: Path,
    *,
    exe: Path | None = None,
) -> Iterator[tuple[int, str, str, bytes, bytes, tuple[int, int], str]]:
    screens = load_mm1_mazedata(mazedata)
    slugs = MM1_MAP_SLUGS
    if exe is not None and exe.is_file():
        parsed = parse_mm1_map_slugs_from_exe(exe.read_bytes())
        if tuple(parsed) != MM1_MAP_SLUGS:
            slugs = tuple(parsed)
    for sid, (slug, (visual, collision)) in enumerate(zip(slugs, screens)):
        yield (
            sid,
            slug,
            mm1_map_title(slug),
            visual,
            collision,
            find_entry(collision),
            mm1_env_for_slug(slug),
        )
