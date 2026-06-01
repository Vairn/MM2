# map.dat format

`map.dat` — 60 map screens × 512 bytes = **30720** bytes. Loaded flat into runtime at
`A4-$EEF4` (see `MapFile.cpp`, asm loader near `map.dat` string in resource table).

## Per-screen layout (512 bytes)

| Offset | Size | Page | Purpose |
|--------|------|------|---------|
| `0x000` | 256 | **0 — visual** | 16×16 grid; four **2-bit wall fields** per cell (N/E/S/W). Drives the 3D hood @ `0x2900`. **No event bit.** |
| `0x100` | 256 | **1 — collision** | Same grid geometry; each direction is `(dark<<1)|wall`. Bit **`0x80`** = **event flag** (aligns with `event.dat` triplets). |

Row **0 on disk = south** (bottom row). The MM2ED map editor and wiki grids draw with
**north at the top** (disk row 15 → screen row 0).

## Page 0 — visual byte (3D walls)

Four 2-bit fields per cell (from `MapFile.h`):

| Bits | Direction | Values |
|------|-----------|--------|
| 0–1 | North | `0` open, `1` wall, `2` wall+torch, `3` door |
| 2–3 | East | same |
| 4–5 | South | same |
| 6–7 | West | same |

Decode: `N = byte & 3`, `E = (byte>>2)&3`, `S = (byte>>4)&3`, `W = (byte>>6)&3`.

Outdoor surface cells use the low **5 bits** as terrain id for `outb.32` auto-map
(`visual & 0x1F` when `-$79E2 != 0`). High bits are passability/flags on overland.

## Page 1 — collision byte

Per direction: **`(dark << 1) | wall`** — low bit = wall, high bit = darkness (drains
party light). **West's dark slot is reused:** bit **`0x80`** = event trigger (verified:
every `event.dat` triplet sits on a collision cell with `0x80` set).

| Bits | Meaning |
|------|---------|
| 0–1 | North wall / dark |
| 2–3 | East wall / dark |
| 4–5 | South wall / dark |
| 6 | West wall |
| 7 | **Event flag** (not west-dark) |

Wall-only decode for auto-map: `byte & 0x7F` before `kCartoTile[byte >> 2]` (interior).

## Relationship to other files

| File | Role |
|------|------|
| `attrib.dat` | Per-screen env, neighbours, roof bits, outdoor flag — selects tileset / carto mode |
| `event.dat` | Scripts; triggers align with collision `0x80` |
| `townb.32` / `outb.32` | Auto-map tile sprites (cartography) |

## Editor / codec

- C++: `editor/src/core/MapFile.{h,cpp}` — load/save, 16×16 grids
- UI: `editor/src/sections/MapSection.cpp` — visual, collision, carto, 3D preview
- Python grid dump: `tools/draw_middlegate_grids.py` (Middlegate sample PNGs)
- Wiki gallery: `tools/mm2_gfx_export.py` → `images/gallery/mapdat/` per screen

## See also

- [3D View and Game Screen](3D-View-and-Game-Screen) — hood / frustum reads page 0
- [event.dat Format](event-dat-Format) — trigger alignment with collision page
- [attrib.dat Format](attrib-dat-Format) — outdoor vs interior cartography branch
