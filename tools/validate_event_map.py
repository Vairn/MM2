#!/usr/bin/env python3
"""Validate event.dat triplets against map.dat collision event flags (0x80)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from decode_event import decode_location, read_header  # noqa: E402

MAP_SCREENS = 60
MAP_SCREEN_SIZE = 512
MAP_PAGE_SIZE = 256
GRID = 16
EVENT_FLAG = 0x80


def collision_at(map_data: bytes, screen: int, x: int, y: int) -> int:
    base = screen * MAP_SCREEN_SIZE + MAP_PAGE_SIZE + y * GRID + x
    return map_data[base]


def validate(data_event: bytes, data_map: bytes) -> dict:
    header = read_header(data_event)
    report: dict = {"screens": [], "missing_event_bit": 0, "extra_event_bit": 0}

    for screen in range(min(MAP_SCREENS, len(header))):
        off, length = header[screen]
        blob = data_event[off : off + length]
        loc = decode_location(blob, screen)
        missing: list[str] = []
        extra_cells: list[str] = []

        event_cells = set()
        for pos, _evt, _cond in loc["triplets"]:
            y = (pos >> 4) & 0xF
            x = pos & 0xF
            event_cells.add((x, y))
            cell = collision_at(data_map, screen, x, y)
            if not (cell & EVENT_FLAG):
                missing.append(f"({y},{x}) cell=0x{cell:02X}")

        if screen * MAP_SCREEN_SIZE + MAP_SCREEN_SIZE <= len(data_map):
            page = data_map[
                screen * MAP_SCREEN_SIZE + MAP_PAGE_SIZE : screen * MAP_SCREEN_SIZE + MAP_SCREEN_SIZE
            ]
            for y in range(GRID):
                for x in range(GRID):
                    if (page[y * GRID + x] & EVENT_FLAG) and (x, y) not in event_cells:
                        extra_cells.append(f"({y},{x})")

        report["missing_event_bit"] += len(missing)
        report["extra_event_bit"] += len(extra_cells)
        if missing or extra_cells:
            report["screens"].append(
                {
                    "screen": screen,
                    "triplet_count": len(loc["triplets"]),
                    "missing_collision_flag": missing[:20],
                    "collision_flag_no_triplet": extra_cells[:20],
                }
            )
    return report


def main() -> None:
    event_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "event.dat"
    map_path = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "map.dat"
    ev = event_path.read_bytes()
    mp = map_path.read_bytes()
    rep = validate(ev, mp)
    print(f"event.dat + map.dat validation (screens 0..59)")
    print(f"  triplet cells missing collision 0x80: {rep['missing_event_bit']}")
    print(f"  collision 0x80 cells without triplet: {rep['extra_event_bit']}")
    for row in rep["screens"]:
        print(f"\nScreen {row['screen']:02d}: {row['triplet_count']} triplets")
        for m in row["missing_collision_flag"]:
            print(f"  MISSING 0x80 @ {m}")
        for e in row["collision_flag_no_triplet"]:
            print(f"  EXTRA 0x80 @ {e}")


if __name__ == "__main__":
    main()
