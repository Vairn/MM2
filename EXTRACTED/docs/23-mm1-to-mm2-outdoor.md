# MM1 overland → MM2 outdoor compatibility

## Same world grid

Both games use the same **5×4 overland sector grid** (`A1`–`E4`). MM1 `areaa1`…`areae4` in `MAZEDATA.DTA` match MM2 `map.dat` screens 5–16 and 33–40 (see `tools/mm2_gfx_export.py` `OUTDOOR_SECTOR_BY_SCREEN`).

| | MM1 | MM2 |
|---|-----|-----|
| Geometry file | `MAZEDATA.DTA` (55×512) | `map.dat` (60×512) |
| Per-sector script | `*.OVR` data segment | `event.dat` + `attrib.dat` |
| Cross-sector links | OVR `MAP_*_EXIT_ID` + section (16-bit id) | `attrib.dat` +0x05..+0x08 (screen index) |

MM1 exit ids (e.g. `areaa1` → north `0x0F01` section 2) are **map ids**, not MAZEDATA indices; the walker remaps via the shared sector → MM2 screen → MM1 index table in `tools/mm1_outdoor_codec.py`.

## Page encoding (why you cannot memcpy)

**MM1 overland page 0** is still **MapWalls** (ScummVM): four 2-bit fields per cell (`WALL_NONE`…`WALL_DOOR`), same as dungeons.

**MM2 outdoor page 0** uses **terrain ids** in the low 5 bits (`visual & 0x1F` → `outb.32`), with high bits as passability/flags. The indoor wall frustum and the outdoor horizon renderer read the same page differently (`-$79E2` outdoor flag).

Direct copy of MM1 `areaa*` bytes into MM2 `map.dat` breaks outdoor 3D and cartography (~4% low-nibble match vs MM2 `A1` on identical geography).

**Page 1 (collision)** is largely compatible: `(dark<<1)|wall` per direction and `0x80` event flag. MM1 desert cells use `states` dark bit `0x20` similarly.

## Conversion strategy (implemented)

`tools/mm1_outdoor_codec.py`:

1. Load MM2 **template** visual/collision/attrib per sector from local `map.dat` + `attrib.dat`.
2. Read each `area*.OVR` **WALLPIX lane triple** (near / mid / far) and map entries 7–14 to biomes (`tools/mm1_wallpix.py` `WALLPIX_BIOME`; see [24-mm1-outdoor-wallpix-by-sector.md](24-mm1-outdoor-wallpix-by-sector.md)).
3. For each MM1 `area*` screen:
   - **Open** MM1 cell → MM2 terrain id from far-lane biome (e.g. water → id `2` for ocean decor) instead of only the MM2 template nibble.
   - **Blocked** MM1 cell → template blocked byte, or `0x60 | terrain` wall flag.
   - **Border** (`0xFF` or all nibbles 3) → template edge tile (e.g. `0xAC`).
   - **`surface`** / walker **`biome`** (`desert_32`, `ocean_32`, `tundra_32`, `swamp_32`) from lane biomes (water → `0xCC`, mountains → `0x99`, swamp/thick forest → `0xBB`, etc.).
4. Merge MM1 collision **event** bits (`0x80`) into template collision.
5. Set walker **neighbors** from MM2 attrib, remapped to MM1 screen indices 14–33.

Export sets `"conversion": "mm1_walls+mm2_template+wallpix_biome"` when GOG `*.OVR` sit beside `MAZEDATA.DTA`. Without OVR, falls back to template-only `mm1_walls+mm2_template`.

This preserves **MM1 walkable layout** while producing bytes the **MM2 outdoor walker** understands. Horizon/floor art follows MM1 sector biomes, not MM2’s per-sector `map.dat` paint.

## Limits

| Topic | Status |
|--------|--------|
| Walker outdoor 3D / minimap | Works with converted bytes + MM2 outdoor `.32` sheets |
| Towns / caves / dungeons | MM2 `town` / `cave` / `castle` `.32` frustum (all MM1 towns are interior) |
| MM1 WALLPIX wall art | Not used in walker (labels only drive overland biomes via OVR) |
| Exact MM2 `map.dat` round-trip | Not lossless; templates + heuristics |
| Towns / dungeons on overland | Still separate MM1 screens (not `area*`) |
| OVR events on overland | Not wired to MM2 `event.dat` |

## Usage

```powershell
python tools/export_mm1_map_walker.py   # needs GOG MAZEDATA + repo map.dat/attrib.dat
```

Overland screens export with `"outdoor": true`, optional `"wallLanes"` / `"wallBiomes"` / `"biome"`, and `"conversion": "mm1_walls+mm2_template+wallpix_biome"`. Re-open `wiki/mm1-maze-walker/` — `areaa1` etc. use MM2 horizon view (ocean/tundra/swamp/desert `.32` sheets) and can step to adjacent sectors when neighbors resolve.

## See also

- [MM1 MAZEDATA](22-mm1-mazedata-format.md)
- [map.dat format](21-map-dat-format.md)
- [attrib.dat format](12-attrib-dat-format.md)
- [3D view / outdoor branch](15-3d-view-and-game-screen.md)
