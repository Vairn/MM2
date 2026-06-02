# MM1 MAZEDATA.DTA format (DOS)

Might and Magic I stores maze geometry in **`MAZEDATA.DTA`**: **55** screens × **512** bytes = **28 160** bytes. Layout matches MM2 `map.dat` (see [map.dat format](21-map-dat-format.md)):

| Offset | Size | Page |
|--------|------|------|
| `0x000` | 256 | Visual — four 2-bit wall fields per cell (N/E/S/W): `0` open, `1` wall, `2` torch, `3` door |
| `0x100` | 256 | Collision — `(dark<<1)|wall` per direction; bit `0x80` = event flag |

Overland sectors (`areaa1`…`areae4`) still use **MapWalls** page-0 encoding in the original game (not MM2 terrain ids). For the HTML walker, see [MM1 → MM2 outdoor conversion](23-mm1-to-mm2-outdoor.md).

## Screen index ↔ name

`MM.EXE` holds a null-terminated **slug table** at file offset **`0x10C07`**, in MAZEDATA order (55 entries): `sorpigal`, `portsmit`, `algary`, … `astral`. Each slug matches a companion **`*.OVR`** map script file (events), not embedded in the DTA.

Display titles and loaders: `tools/mm1_maps.py`, `tools/export_mm1_map_walker.py`.

## Walker

Indoor HTML walker: `wiki/mm1-maze-walker/`. Regenerate with `python tools/export_mm1_map_walker.py` (GOG `MAZEDATA.DTA`, MM2 `map.dat`/`attrib.dat` for overland, lagdotcom `re/lagdotcom/exported/wall*.png` for dungeon walls).
