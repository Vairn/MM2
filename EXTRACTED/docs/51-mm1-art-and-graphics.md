# MM1 art and graphics

MM1 DOS graphics are **not** the same format as MM2 Amiga `.32` / `.anm` planar chunks. What we have decoded or exported in this repo is listed below.

---

## Decode status

| Asset | File | Status | Notes |
|-------|------|--------|-------|
| **Horizon WALLPIX** | `WALLPIX.DTA` | **Partial** | Entries 7–14 = overland biomes; lagdotcom PNG exports |
| **Indoor wall sets** | `WALLPIX.DTA` | **Partial** | `wall01.png`…`wall14.png` composites (496×128, 12 slices each) |
| **Floor / object pix** | `FLOORPIX.DTA`, `OBJPIX.DTA` | **Not decoded** | ScummVM loaders only |
| **Monster portraits** | `MONPIX.DTA` | **Not decoded** | Combat UI sprites |
| **Character faces** | in `MM.EXE` / data | **Not decoded** | — |
| **Walker rendering** | — | **MM2 stand-ins** | Indoor walls from MM2 `town`/`cave`/`castle`; outdoor horizons from MM2 `desert`/`ocean`/etc. |

---

## WALLPIX.DTA layout (lagdotcom)

Each **wall set** is one **496×128** indexed-color composite. The 3D engine slices it into 12 sub-images for the frustum (`tools/mm1_wallpix.py` `WALLSLICES`):

| Slice | Size | Role |
|-------|------|------|
| left1…left4 | 32×128 … 16×32 | Left wall column, near → far |
| right1…right4 | mirror of left | Right wall column |
| mid2…mid5 | 176×96 … 16×16 | Front wall column |

Slice order matches MM2’s `collectBlits` depth table (`SLICE_BY_FRAME` in `mm1_wallpix.py`).

**Export path (GOG):** `Might and Magic 1\re\lagdotcom\exported\wallNN.png`  
Regenerate from game files with lagdotcom’s RE tools (not shipped in this repo).

---

## Overland horizon sheets (entries 7–14)

Outdoor maps load **three** WALLPIX entries (near / mid / far lanes). Per-sector assignments are in [24-mm1-outdoor-wallpix-by-sector](24-mm1-outdoor-wallpix-by-sector.md).

| Entry | PNG | Biome label |
|-------|-----|-------------|
| 7 | wall07.png | trees |
| 8 | wall08.png | mountains |
| 9 | wall09.png | trees |
| 10 | wall10.png | lava |
| 11 | wall11.png | swamp |
| 12 | wall12.png | water |
| 13 | wall13.png | thick forest |
| 14 | wall14.png | mountains |

Lane ids come from each `area*.OVR` data segment (ScummVM `Maps::loadTile` / `TILE_AREA2` table) — see `tools/mm1_wallpix.py` `outdoor_wall_lanes()`.

---

## Indoor wall set selection

`MM1_COLOR_OFFSET` (from ScummVM `maps.cpp`) plus slug heuristics pick which `wallNN.png` a dungeon/town uses (`wall_set_for_screen()`):

| Screen kind | Typical wall set |
|-------------|------------------|
| Caves, upper dragon | wall01 |
| Towns (default) | wall02 |
| Overland A–E | wall05–wall09 (by sector letter) |
| Endgame fights | wall10 |
| Doom / demon / astral / dragon | wall13 |
| Plane of Power | wall14 |

The HTML walker does **not** render these PNGs yet — it uses MM2 `.32` sheets for a consistent preview pipeline.

---

## COLOR_OFFSET per screen

55-byte table (one byte per MAZEDATA screen) toggles palette variant for wall sets — `MM1_COLOR_OFFSET` in `tools/mm1_wallpix.py` (from ScummVM `COLOR_OFFSET[mapIndex]`).

---

## Wiki gallery

Rendered PNGs (regenerate anytime):

```powershell
python tools/export_mm1_gallery.py          # 55 maps + WALLPIX sheets → wiki/public/gallery/mm1/
python wiki/scripts/export-gfx-gallery.py # MM2 maps, monsters, tilesets → wiki/public/gallery/
```

Browse locally: `cd wiki && npm run dev` → [MM1 maps](/docs/gallery/mm1-maps), [MM1 walls](/docs/gallery/mm1-walls).

When lagdotcom exports are present, wall sheets include labeled **slice atlases** (12 frustum pieces per `wallNN.png`).

---

## See also

- [50-mm1-overview](50-mm1-overview.md) — MM1 hub
- [22-mm1-mazedata-format](22-mm1-mazedata-format.md) — maze geometry
- [23-mm1-to-mm2-outdoor](23-mm1-to-mm2-outdoor.md) — how WALLPIX biomes drive the outdoor walker
- [06-gfx-loading](06-gfx-loading.md) — MM2 `.32` format (walker stand-ins)
