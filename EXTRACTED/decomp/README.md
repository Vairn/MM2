# MM2 Decomp Scaffold

This directory contains a first-pass C lifting scaffold for the Amiga `mm2` executable.

## Files

- `mm2_types.h`  
  Runtime structs and offset-based field aliases.
- `mm2_gamestate.h`  
  `A4` field constants (`MM2_GS_*`) shared across lifts and codecs.
- `mm2_lift.c`  
  Address-anchored function stubs (`sub_248ae_startup`, `sub_256e_load_screen_state`, etc.).
- `mm2_roster_codec.h` / `mm2_roster_codec.c`  
  Struct-backed loader/saver for `roster.dat` (64 × 130-byte records).
- `mm2_monsters_codec.h` / `mm2_monsters_codec.c`  
  Struct-backed loader/saver for `monsters.dat` (256 × 26-byte records).
- `mm2_event_codec.h` / `mm2_event_codec.c`  
  Struct-backed loader/saver for `event.dat` (71 location records).
- `mm2_found_items.h` / `mm2_found_items.c`  
  Pending loot buffer — OP_2A treasure fill, OP_19 backpack overflow, Search predicate (`A4-$3F10..-$794C`). Python mirror: `tools/mm2_found_items.py`.
- `mm2_town_tables.h` / `mm2_town_tables.c`  
  Per-town commerce constants + blacksmith inventories decoded from `mm2_data_00.bin`. Python mirror: `tools/mm2_town_tables.py`, dump helper: `tools/dump_shop_tables.py`.
- `mm2_encounter.h` / `mm2_encounter.c`  
  Encounter table helpers (random step / arena slots).
- `mm2_anm_preview.h` / `mm2_anm_preview.c`  
  `.anm` planar frame decode for tooling previews.

## Goals

1. Keep control-flow anchored to known 68k addresses.
2. Replace unknowns incrementally without breaking the skeleton.
3. Preserve offset-based memory semantics until field meanings are proven.

## Naming Style

- `sub_<addr>_<purpose>` for recovered routines.
- Comments should include original label/address where possible.
- Do not rename offsets to semantic fields unless confidence is high.

## Next Lifts

- Complete `sub_24928_init_runtime()`.
- Expand `sub_256e_load_screen_state()` to all copied pages.
- Implement `sub_24c4_overland_loop()` key handling and draw calls.
- Model `sub_1280_main_loop()` dispatch table.
- Trace Search payoff @ `0x1B19C` (found-item buffer consumption).
