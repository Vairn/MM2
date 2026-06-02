#!/usr/bin/env python3
"""Print MM1 overland WALLPIX sheet usage per area* map (from *.OVR + ScummVM tables)."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from mm1_maps import MM1_MAP_SLUGS  # noqa: E402
from mm1_wallpix import (  # noqa: E402
    WALLPIX_BIOME,
    format_lane_biomes,
    outdoor_wall_lanes,
    parse_ovr_data,
)

GOG = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 1")

MAP_TYPE, MAP_SURFACE_ID = 37, 39


def parse_ovr(slug: str) -> dict:
    ovr = (GOG / f"{slug.upper()}.OVR").read_bytes()
    code_sz = struct.unpack_from("<H", ovr, 4)[0]
    data_sz = struct.unpack_from("<H", ovr, 8)[0]
    data = ovr[14 + code_sz : 14 + code_sz + data_sz]
    d = parse_ovr_data(ovr)

    def w(off: int) -> int:
        return data[off] | (data[off + 1] << 8)

    d["map_id"] = w(0)
    d["map_type"] = data[MAP_TYPE] if len(data) > MAP_TYPE else -1
    d["surface_id"] = w(MAP_SURFACE_ID) if len(data) > MAP_SURFACE_ID + 1 else 0
    return d


def sector_label(slug: str) -> str:
    return slug[4].upper() + slug[5]


def main() -> None:
    rows: list[dict] = []
    for slug in MM1_MAP_SLUGS:
        if not slug.startswith("area"):
            continue
        d = parse_ovr(slug)
        lanes = outdoor_wall_lanes(slug, GOG)
        e1, e2, e3 = lanes if lanes else (None, None, None)
        rows.append(
            {
                "sector": sector_label(slug),
                "slug": slug,
                "screen_ix": MM1_MAP_SLUGS.index(slug),
                "area_table": a,
                "lane1_id": f"{d['id_lane1']:04X}",
                "lane2_id": f"{d['id_lane2']:04X}",
                "lane3_id": f"{d['id_lane3']:04X}",
                "wall_lane1": f"wall{e1:02d}" if e1 else "?",
                "wall_lane2": f"wall{e2:02d}" if e2 else "?",
                "wall_lane3": f"wall{e3:02d}" if e3 else "?",
                "entry_lane1": e1,
                "entry_lane2": e2,
                "entry_lane3": e3,
                "map_type": d["map_type"],
                "surface_id": f"{d['surface_id']:04X}",
                "biome_guess": format_lane_biomes((e1, e2, e3)),
            }
        )

    out_md = ROOT / "EXTRACTED" / "docs" / "24-mm1-outdoor-wallpix-by-sector.md"
    lines = [
        "# MM1 overland WALLPIX sheets by sector",
        "",
        "Each outdoor map loads **three** `WALLPIX.DTA` entries (near / mid / far horizon lanes).",
        "Values are from each `*.OVR` data segment + ScummVM `TILE_AREA2` lookup (`MAP_1 = 2` for all `area*` maps).",
        "",
        "Sheet labels (user-confirmed, `tools/mm1_wallpix.py` `WALLPIX_BIOME`):",
        "",
        "| Entry | PNG | Biome |",
        "|-------|-----|-------|",
    ]
    for entry in sorted(WALLPIX_BIOME):
        label = WALLPIX_BIOME[entry].replace("_", " ")
        lines.append(f"| {entry} | wall{entry:02d}.png | {label} |")
    lines.extend(
        [
            "",
            "Per-sector column = lane1 / lane2 / lane3 biomes (near / mid / far horizon).",
            "",
            "Lagdotcom exports: `GOG\\Might and Magic 1\\re\\lagdotcom\\exported\\wallNN.png`",
            "",
            "| Sector | MAZEDATA ix | OVR slug | Lane 1 (id → sheet) | Lane 2 | Lane 3 | map_type | surface_id | Lanes (biome) |",
            "|--------|-------------|----------|---------------------|--------|--------|----------|------------|---------------|",
        ]
    )
    for r in rows:
        lines.append(
            f"| {r['sector']} | {r['screen_ix']} | `{r['slug']}` | "
            f"`{r['lane1_id']}` → **{r['wall_lane1']}** | "
            f"`{r['lane2_id']}` → **{r['wall_lane2']}** | "
            f"`{r['lane3_id']}` → **{r['wall_lane3']}** | "
            f"{r['map_type']} | {r['surface_id']} | {r['biome_guess']} |"
        )

    lines.extend(
        [
            "",
            "## Unique wall sheets used on overland",
            "",
            "| WALLPIX entry | PNG | Used as lane 1 | lane 2 | lane 3 |",
            "|---------------|-----|----------------|--------|--------|",
        ]
    )
    used: dict[int, list[str]] = {}
    for r in rows:
        for lane, key in [(1, "entry_lane1"), (2, "entry_lane2"), (3, "entry_lane3")]:
            e = r[key]
            if e:
                used.setdefault(e, []).append(f"{r['sector']} L{lane}")

    for entry in sorted(used):
        sectors = ", ".join(used[entry])
        lines.append(f"| {entry} | wall{entry:02d}.png | {sectors} |")

    lines.extend(
        [
            "",
            "## TILE_AREA2 id → entry index (all area* use table 2)",
            "",
            "| Id in OVR | WALLPIX entry |",
            "|-----------|---------------|",
        ]
    )
    arr = TILE_AREA2
    ctr = TILE_OFFSET[1]
    for v in arr:
        lines.append(f"| `{v:04X}` | **{ctr}** (wall{ctr:02d}) |")
        ctr += 1

    out_md.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {out_md}")

    # Console table for chat
    print("\nSector  L1      L2      L3      (three wallNN.png per map)\n")
    for r in rows:
        print(
            f"{r['sector']:4}  {r['wall_lane1']:8} {r['wall_lane2']:8} {r['wall_lane3']:8}  "
            f"({r['lane1_id']},{r['lane2_id']},{r['lane3_id']})"
        )


if __name__ == "__main__":
    main()
