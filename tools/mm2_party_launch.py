#!/usr/bin/env python3
"""Town inn spawn tables for title Goto Town (ASM @ 0x0E38, data A4-$8990..)."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA_BIN = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
A4_ANCHOR = 0x7FFE

TOWN_NAMES = ["Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"]


@dataclass(frozen=True)
class TownInnSpawn:
    town_filter: int  # 1..5
    area_id: int
    coord_x: int
    coord_y: int
    facing_key: str


def _table_byte(a4_offset: int, index: int) -> int:
    """a4_offset is the positive displacement word (e.g. 0x7670 for lea -$7670(a4))."""
    off = A4_ANCHOR - a4_offset + index
    return DATA_BIN.read_bytes()[off]


def load_tables_from_bin() -> tuple[list[int], list[int], list[str]]:
    xs = [_table_byte(0x7670, i) for i in range(5)]  # A4-$8990 → coord_b / X
    ys = [_table_byte(0x766B, i) for i in range(5)]  # A4-$8995 → coord_a / Y
    faces = [chr(_table_byte(0x7666, i)) for i in range(5)]
    return xs, ys, faces


# Static fallback (matches ghidra/mm2_data_00.bin).
SPAWN_X = [7, 9, 7, 7, 3]
SPAWN_Y = [3, 13, 11, 0, 10]
SPAWN_FACING = ["N", "N", "E", "N", "W"]


def town_inn_spawn(town_filter: int) -> TownInnSpawn:
    idx = max(0, min(4, town_filter - 1))
    return TownInnSpawn(
        town_filter=idx + 1,
        area_id=idx,
        coord_x=SPAWN_X[idx],
        coord_y=SPAWN_Y[idx],
        facing_key=SPAWN_FACING[idx],
    )


def encode_party_launch(town_filter: int, roster_indices: list[int]) -> dict:
    sp = town_inn_spawn(town_filter)
    slots = list(roster_indices[:8]) + [-1] * (8 - len(roster_indices[:8]))
    return {
        "town_filter": sp.town_filter,
        "area_id": sp.area_id,
        "coord_x": sp.coord_x,
        "coord_y": sp.coord_y,
        "facing_key": sp.facing_key,
        "party_count": min(len(roster_indices), 8),
        "roster_slots": slots,
    }


if __name__ == "__main__":
    if DATA_BIN.is_file():
        bx, by, bf = load_tables_from_bin()
        assert bx == SPAWN_X and by == SPAWN_Y and bf == SPAWN_FACING
        print("round-trip OK vs static tables")
    for i in range(1, 6):
        sp = town_inn_spawn(i)
        print(f"{i} {TOWN_NAMES[i-1]:12s} area={sp.area_id} tile=({sp.coord_x},{sp.coord_y}) face={sp.facing_key}")
