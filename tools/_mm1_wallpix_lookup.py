#!/usr/bin/env python3
"""Trace MM1 WALLPIX entry selection from *.OVR data (ScummVM loadTile)."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from mm1_maps import MM1_MAP_SLUGS  # noqa: E402

GOG = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 1")

MAP_1, MAP_2, MAP_4, MAP_6 = 1, 2, 4, 6

TILE_AREA1 = [0x10D, 0x0B0B, 0x50A, 0x11A, 0x0B18, 0x517]
TILE_AREA2 = [0x0B0B, 0x50A, 0x10D, 0x0F08, 0x907, 0x11A, 0x0B18, 0x517]
TILE_AREA3 = [0x0B0B, 0x10D, 0x517, 0x0B18, 0x11A, 0x50A]
TILE_AREAS = [TILE_AREA1, TILE_AREA2, TILE_AREA3]
TILE_OFFSET = [1, 7, 15]


def parse_ovr_data(slug: str) -> dict:
    ovr = (GOG / f"{slug.upper()}.OVR").read_bytes()
    code_sz = struct.unpack_from("<H", ovr, 4)[0]
    data_sz = struct.unpack_from("<H", ovr, 8)[0]
    data = ovr[14 + code_sz : 14 + code_sz + data_sz]

    def w(off: int) -> int:
        return data[off] | (data[off + 1] << 8)

    return {
        "map_id": w(0),
        "area": data[MAP_1],
        "id2": w(MAP_2),
        "id4": w(MAP_4),
        "id6": w(MAP_6),
        "map_type": data[37] if len(data) > 37 else -1,
    }


def wallpix_entry(area: int, load_id: int) -> int | None:
    """1-based WALLPIX.DTA entry index (lagdotcom wallNN.png = entry N)."""
    if area < 1 or area > 3:
        return None
    arr = TILE_AREAS[area - 1]
    ctr = TILE_OFFSET[area - 1]
    for v in arr:
        if v == load_id:
            return ctr
        ctr += 1
    return None


def main() -> None:
    print("MM1 WALLPIX selection (ScummVM Maps::loadTile)")
    print("OVR +1=area table 1..3; +2/+4/+6 = three load ids -> 3 wall sets per map\n")
    print(f"{'slug':12} {'type':4} {'area':4} {'id@2':6} {'id@4':6} {'id@6':6}  -> wall entries (horizon lanes)")
    for slug in MM1_MAP_SLUGS:
        d = parse_ovr_data(slug)
        area = d["area"]
        e2 = wallpix_entry(area, d["id2"])
        e4 = wallpix_entry(area, d["id4"])
        e6 = wallpix_entry(area, d["id6"])
        tag = "OUT" if slug.startswith("area") else "in"
        ent = f"{e2},{e4},{e6}"
        names = ""
        if e2 and e4 and e6:
            names = f"  wall{e2:02d}/wall{e4:02d}/wall{e6:02d}"
        print(
            f"{slug:12} {tag:4} {area:4} {d['id2']:04x}   {d['id4']:04x}   {d['id6']:04x}   -> {ent}{names}"
        )


if __name__ == "__main__":
    main()
