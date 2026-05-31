"""Map/location labels for MM2 viewers (world sector + tile notation)."""

from __future__ import annotations

from typing import Dict, Optional

# Transcribed from editor/src/core/AreaNames.h
AREA_NAMES: Dict[int, str] = {
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


def tile_notation(x: int, y: int) -> str:
    """16×16 cell as column letter + 1-based row (e.g. 0,0 → a1; 2,2 → c3)."""
    x = max(0, min(15, int(x)))
    y = max(0, min(15, int(y)))
    return f"{chr(ord('a') + x)}{y + 1}"


def outdoor_sector(surface_label_byte: int) -> str:
    """Overland world sector from attrib +0x15 (e.g. 0xA1 → A1, 0xC2 → C2)."""
    b = surface_label_byte & 0xFF
    return f"{b >> 4:X}{b & 0xF:X}"


def area_name(screen: int) -> str:
    return AREA_NAMES.get(screen, f"Area {screen}")


def location_label(
    screen: int,
    x: int,
    y: int,
    *,
    is_outside: bool = False,
    outside_label_byte: int = 0,
) -> str:
    """Human location: overland `C2/c3`, interior `Middlegate/i9`."""
    tile = tile_notation(x, y)
    if is_outside:
        return f"{outdoor_sector(outside_label_byte)}/{tile}"
    name = AREA_NAMES.get(screen)
    if name:
        return f"{name}/{tile}"
    return f"s{screen}/{tile}"


def location_label_from_attrib(screen: int, x: int, y: int, attrib_record) -> str:
    """Build label from attrib_codec.AttribRecord."""
    return location_label(
        screen,
        x,
        y,
        is_outside=attrib_record.is_outside,
        outside_label_byte=attrib_record.raw[0x15],
    )
