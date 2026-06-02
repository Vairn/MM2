# MM1 overland WALLPIX sheets by sector

Each outdoor map loads **three** `WALLPIX.DTA` entries (near / mid / far horizon lanes).
Values are from each `*.OVR` data segment + ScummVM `TILE_AREA2` lookup (`MAP_1 = 2` for all `area*` maps).

Sheet labels (user-confirmed, `tools/mm1_wallpix.py` `WALLPIX_BIOME`):

| Entry | PNG | Biome |
|-------|-----|-------|
| 7 | wall07.png | trees |
| 8 | wall08.png | mountains |
| 9 | wall09.png | trees |
| 10 | wall10.png | lava |
| 11 | wall11.png | swamp |
| 12 | wall12.png | water |
| 13 | wall13.png | thick forest |
| 14 | wall14.png | mountains |

Per-sector column = lane1 / lane2 / lane3 biomes (near / mid / far horizon).

Lagdotcom exports: `GOG\Might and Magic 1\re\lagdotcom\exported\wallNN.png`

| Sector | MAZEDATA ix | OVR slug | Lane 1 (id → sheet) | Lane 2 | Lane 3 | map_type | surface_id | Lanes (biome) |
|--------|-------------|----------|---------------------|--------|--------|----------|------------|---------------|
| A1 | 14 | `areaa1` | `0B0B` → **wall07** | `0517` → **wall14** | `0B18` → **wall13** | 0 | 0604 | trees / mountains / thick_forest |
| A2 | 15 | `areaa2` | `0B0B` → **wall07** | `0517` → **wall14** | `011A` → **wall12** | 0 | 0604 | trees / mountains / water |
| A3 | 16 | `areaa3` | `0B0B` → **wall07** | `050A` → **wall08** | `011A` → **wall12** | 0 | 0604 | trees / mountains / water |
| A4 | 17 | `areaa4` | `0B0B` → **wall07** | `050A` → **wall08** | `011A` → **wall12** | 0 | 0604 | trees / mountains / water |
| B1 | 18 | `areab1` | `0B0B` → **wall07** | `0517` → **wall14** | `0B18` → **wall13** | 0 | 0604 | trees / mountains / thick_forest |
| B2 | 19 | `areab2` | `0B0B` → **wall07** | `0517` → **wall14** | `0B18` → **wall13** | 0 | 0604 | trees / mountains / thick_forest |
| B3 | 20 | `areab3` | `0B0B` → **wall07** | `050A` → **wall08** | `011A` → **wall12** | 0 | 0604 | trees / mountains / water |
| B4 | 21 | `areab4` | `010D` → **wall09** | `050A` → **wall08** | `011A` → **wall12** | 0 | 0604 | trees / mountains / water |
| C1 | 22 | `areac1` | `0B0B` → **wall07** | `050A` → **wall08** | `010D` → **wall09** | 0 | 0604 | trees / mountains / trees |
| C2 | 23 | `areac2` | `0B0B` → **wall07** | `050A` → **wall08** | `010D` → **wall09** | 0 | 0604 | trees / mountains / trees |
| C3 | 24 | `areac3` | `0B0B` → **wall07** | `050A` → **wall08** | `010D` → **wall09** | 0 | 0604 | trees / mountains / trees |
| C4 | 25 | `areac4` | `010D` → **wall09** | `050A` → **wall08** | `011A` → **wall12** | 0 | 0604 | trees / mountains / water |
| D1 | 26 | `aread1` | `010D` → **wall09** | `050A` → **wall08** | `0F08` → **wall10** | 0 | 0604 | trees / mountains / lava |
| D2 | 27 | `aread2` | `010D` → **wall09** | `050A` → **wall08** | `0F08` → **wall10** | 0 | 0604 | trees / mountains / lava |
| D3 | 28 | `aread3` | `010D` → **wall09** | `050A` → **wall08** | `0907` → **wall11** | 0 | 0604 | trees / mountains / swamp |
| D4 | 29 | `aread4` | `010D` → **wall09** | `0907` → **wall11** | `011A` → **wall12** | 0 | 0604 | trees / swamp / water |
| E1 | 30 | `areae1` | `010D` → **wall09** | `050A` → **wall08** | `0F08` → **wall10** | 0 | 0604 | trees / mountains / lava |
| E2 | 31 | `areae2` | `010D` → **wall09** | `050A` → **wall08** | `0F08` → **wall10** | 0 | 0604 | trees / mountains / lava |
| E3 | 32 | `areae3` | `010D` → **wall09** | `050A` → **wall08** | `0B18` → **wall13** | 0 | 0604 | trees / mountains / thick_forest |
| E4 | 33 | `areae4` | `010D` → **wall09** | `050A` → **wall08** | `0907` → **wall11** | 0 | 0604 | trees / mountains / swamp |

## Unique wall sheets used on overland

| WALLPIX entry | PNG | Used as lane 1 | lane 2 | lane 3 |
|---------------|-----|----------------|--------|--------|
| 7 | wall07.png | A1 L1, A2 L1, A3 L1, A4 L1, B1 L1, B2 L1, B3 L1, C1 L1, C2 L1, C3 L1 |
| 8 | wall08.png | A3 L2, A4 L2, B3 L2, B4 L2, C1 L2, C2 L2, C3 L2, C4 L2, D1 L2, D2 L2, D3 L2, E1 L2, E2 L2, E3 L2, E4 L2 |
| 9 | wall09.png | B4 L1, C1 L3, C2 L3, C3 L3, C4 L1, D1 L1, D2 L1, D3 L1, D4 L1, E1 L1, E2 L1, E3 L1, E4 L1 |
| 10 | wall10.png | D1 L3, D2 L3, E1 L3, E2 L3 |
| 11 | wall11.png | D3 L3, D4 L2, E4 L3 |
| 12 | wall12.png | A2 L3, A3 L3, A4 L3, B3 L3, B4 L3, C4 L3, D4 L3 |
| 13 | wall13.png | A1 L3, B1 L3, B2 L3, E3 L3 |
| 14 | wall14.png | A1 L2, A2 L2, B1 L2, B2 L2 |

## TILE_AREA2 id → entry index (all area* use table 2)

| Id in OVR | WALLPIX entry |
|-----------|---------------|
| `0B0B` | **7** (wall07) |
| `050A` | **8** (wall08) |
| `010D` | **9** (wall09) |
| `0F08` | **10** (wall10) |
| `0907` | **11** (wall11) |
| `011A` | **12** (wall12) |
| `0B18` | **13** (wall13) |
| `0517` | **14** (wall14) |
