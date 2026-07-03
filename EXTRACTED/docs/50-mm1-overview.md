# Might and Magic I (DOS) — overview

This repo’s primary target is **MM2 (Amiga)**, but several **MM1 (DOS)** formats are partially decoded here because the two games share the same overland grid and nearly identical maze geometry. Use this page as the hub for MM1-specific docs, tools, and decode status.

**Platform:** MS-DOS · **Geometry file:** `MAZEDATA.DTA` · **Map scripts:** per-screen `*.OVR` overlays · **Reference:** [ScummVM `engines/mm/mm1`](https://github.com/scummvm/scummvm/tree/master/engines/mm/mm1)

---

## Decode status

| Area | Status | Doc / tool |
|------|--------|------------|
| **Mazes** (`MAZEDATA.DTA`) | **Decoded** — 55 × 512 B, same layout as MM2 `map.dat` | [22-mm1-mazedata-format](22-mm1-mazedata-format.md) · `tools/mm1_maps.py` |
| **Overland → MM2** | **Decoded** — sector grid, collision merge, biome heuristics | [23-mm1-to-mm2-outdoor](23-mm1-to-mm2-outdoor.md) · `tools/mm1_outdoor_codec.py` |
| **Outdoor WALLPIX** | **Partial** — lane ids from `*.OVR`, sheets 7–14 exported | [24-mm1-outdoor-wallpix-by-sector](24-mm1-outdoor-wallpix-by-sector.md) · `tools/mm1_wallpix.py` |
| **Indoor wall art** | **Partial** — lagdotcom `wallNN.png` slice layout | [51-mm1-art-and-graphics](51-mm1-art-and-graphics.md) |
| **Map events** (`*.OVR`) | **Not in repo** — scripts live in OVR code+data segments | ScummVM `maps/maps.cpp` |
| **Items** | **Not decoded** | [52-mm1-items-monsters-events](52-mm1-items-monsters-events.md) |
| **Monsters** | **Not decoded** | [52-mm1-items-monsters-events](52-mm1-items-monsters-events.md) |
| **Spells** | **Not decoded** | [52-mm1-items-monsters-events](52-mm1-items-monsters-events.md) |

---

## All 55 maze screens

Screen order matches `MAZEDATA.DTA` and the slug table in `MM.EXE` @ `0x10C07` (`tools/mm1_maps.py` `MM1_MAP_SLUGS`).

| # | Slug | Title | Kind |
|---|------|-------|------|
| 0 | sorpigal | Sorpigal | town |
| 1 | portsmit | Portsmith | town |
| 2 | algary | Algary | town |
| 3 | dusk | Dusk | town |
| 4 | erliquin | Erliquin | town |
| 5–13 | cave1…cave9 | Caves 1–9 | dungeon |
| 14–17 | areaa1…areaa4 | Overland A1–A4 | outdoor |
| 18–21 | areab1…areab4 | Overland B1–B4 | outdoor |
| 22–25 | areac1…areac4 | Overland C1–C4 | outdoor |
| 26–29 | aread1…aread4 | Overland D1–D4 | outdoor |
| 30–33 | areae1…areae4 | Overland E1–E4 | outdoor |
| 34 | doom | Doom | dungeon |
| 35–36 | blackrn, blackrs | Blackridge North/South | dungeon |
| 37–38 | qvl1, qvl2 | Queen’s Vault 1–2 | dungeon |
| 39–40 | rwl1, rwl2 | Ruby Warlock Lair 1–2 | dungeon |
| 41–42 | enf1, enf2 | Endgame Fight 1–2 | dungeon |
| 43 | whitew | White Wolf | dungeon |
| 44 | dragad | Dragon’s Den | dungeon |
| 45–47 | udrag1…udrag3 | Upper Dragon Caves 1–3 | dungeon |
| 48 | demon | Demon’s Dome | dungeon |
| 49 | alamar | Castle Alamar | dungeon |
| 50–53 | pp1…pp4 | Plane of Power 1–4 | dungeon |
| 54 | astral | Astral Plane | dungeon |

---

## Interactive map walker

First-person **3D** explorer for all 55 screens — indoor frustum uses MM2 `town`/`cave`/`castle` `.32` stand-ins; overland sectors use converted bytes + MM2 outdoor horizon sheets.

| Target | URL / path |
|--------|------------|
| GitHub Pages | [vairn.github.io/MM2/mm1-maze-walker/](https://vairn.github.io/MM2/mm1-maze-walker/) |
| Local bundle | `wiki/mm1-maze-walker/` |

Regenerate (needs GOG `MAZEDATA.DTA`, repo `map.dat` + `attrib.dat`, optional `*.OVR` for outdoor biomes):

```powershell
python tools/export_mm1_map_walker.py
cd wiki/mm1-maze-walker
python -m http.server 8081
```

Controls: **W/S** forward/back, **A/D** turn — same as the [MM2 map walker](../../wiki/README.md#map-walker-interactive).

---

## Tools

| Script | Purpose |
|--------|---------|
| `tools/mm1_maps.py` | Load `MAZEDATA.DTA`, slug/title tables, screen iterator |
| `tools/mm1_outdoor_codec.py` | MM1 `area*` → MM2-compatible visual/collision/attrib |
| `tools/mm1_wallpix.py` | WALLPIX lane lookup, lagdotcom sheet slicing |
| `tools/mm1_outdoor_wallpix_table.py` | Regenerate [24-mm1-outdoor-wallpix-by-sector](24-mm1-outdoor-wallpix-by-sector.md) |
| `tools/export_mm1_map_walker.py` | HTML walker bundle (`wiki/mm1-maze-walker/walker-bundle.js`) |
| `tools/_mm1_wallpix_lookup.py` | Debug OVR → WALLPIX entry trace |

---

## Related MM2 docs

- [21-map-dat-format](21-map-dat-format.md) — MM2 maze pages (same bit layout as MM1)
- [15-3d-view-and-game-screen](15-3d-view-and-game-screen.md) — frustum renderer the walkers reuse
- [07-dat-files-and-formats](07-dat-files-and-formats.md) — MM2 data inventory
