# event.dat — per-location reference

Decoded script pages for all **71** `event.dat` locations (indices **0..70**).
Each file lists tile triggers, bytecode segments, and string table entries.

| Resource | Doc |
|----------|-----|
| `event.dat` container | [`06-event-dat-format.md`](../06-event-dat-format.md) |
| Opcode reference | [`07-event-script-opcodes.md`](../07-event-script-opcodes.md) — **C++ `dispatchOp` is authoritative** |
| Runtime / collision `0x80` | [`08-event-runtime.md`](../08-event-runtime.md) — **`EventRuntime` port map** |
| 60 `map.dat` screens | [`21-map-dat-format.md`](../21-map-dat-format.md) |
| Wiki hub (numbered) | [`40-events-by-location.md`](../40-events-by-location.md) |
| Class quests | [`37-mount-farview-class-quest-event.md`](../37-mount-farview-class-quest-event.md) |

Map screen names follow `editor/src/core/AreaNames.h`. Locations **60..70** are
overlay banks (quests, HoS, castle blobs, meta) — not `map.dat` screens.

## Regenerate

```powershell
python tools\build_event_location_docs.py
```

Decoder: `tools/decode_event.py` · Editor: [`editor/README.md`](../../editor/README.md) (Events section)


### Towns (map 0–4)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 00 | 0 | Middlegate | [loc_00_middlegate.md](loc_00_middlegate.md) |
| 01 | 1 | Atlantium | [loc_01_atlantium.md](loc_01_atlantium.md) |
| 02 | 2 | Tundara | [loc_02_tundara.md](loc_02_tundara.md) |
| 03 | 3 | Vulcania | [loc_03_vulcania.md](loc_03_vulcania.md) |
| 04 | 4 | Sandsobar | [loc_04_sandsobar.md](loc_04_sandsobar.md) |

### Overland sectors (map 5–16)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 05 | 5 · A1 | A1 | [loc_05_a1.md](loc_05_a1.md) |
| 06 | 6 · B1 | B1 | [loc_06_b1.md](loc_06_b1.md) |
| 07 | 7 · C1 | C1 | [loc_07_c1.md](loc_07_c1.md) |
| 08 | 8 · D1 | D1 | [loc_08_d1.md](loc_08_d1.md) |
| 09 | 9 · A2 | A2 | [loc_09_a2.md](loc_09_a2.md) |
| 10 | 10 · B2 | B2 | [loc_10_b2.md](loc_10_b2.md) |
| 11 | 11 · C2 | C2 | [loc_11_c2.md](loc_11_c2.md) |
| 12 | 12 · A3 | A3 | [loc_12_a3.md](loc_12_a3.md) |
| 13 | 13 · B3 | B3 | [loc_13_b3.md](loc_13_b3.md) |
| 14 | 14 · C3 | C3 | [loc_14_c3.md](loc_14_c3.md) |
| 15 | 15 · A4 | A4 | [loc_15_a4.md](loc_15_a4.md) |
| 16 | 16 · B4 | B4 | [loc_16_b4.md](loc_16_b4.md) |

### Wilderness & dungeons (map 17–32)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 17 | 17 | Middlegate Cavern | [loc_17_middlegate_cavern.md](loc_17_middlegate_cavern.md) |
| 18 | 18 | Atlantium Cavern | [loc_18_atlantium_cavern.md](loc_18_atlantium_cavern.md) |
| 19 | 19 | Tundara Cavern | [loc_19_tundara_cavern.md](loc_19_tundara_cavern.md) |
| 20 | 20 | Vulcania Cavern | [loc_20_vulcania_cavern.md](loc_20_vulcania_cavern.md) |
| 21 | 21 | Sandsobar Cavern | [loc_21_sandsobar_cavern.md](loc_21_sandsobar_cavern.md) |
| 22 | 22 | Corak's Cave | [loc_22_corak_s_cave.md](loc_22_corak_s_cave.md) |
| 23 | 23 | Square Lake Cave | [loc_23_square_lake_cave.md](loc_23_square_lake_cave.md) |
| 24 | 24 | Ice Cavern | [loc_24_ice_cavern.md](loc_24_ice_cavern.md) |
| 25 | 25 | Sarakin's Mine | [loc_25_sarakin_s_mine.md](loc_25_sarakin_s_mine.md) |
| 26 | 26 | Murray's Cave | [loc_26_murray_s_cave.md](loc_26_murray_s_cave.md) |
| 27 | 27 | Druid's Cave | [loc_27_druid_s_cave.md](loc_27_druid_s_cave.md) |
| 28 | 28 | Forbidden Forest Cavern | [loc_28_forbidden_forest_cavern.md](loc_28_forbidden_forest_cavern.md) |
| 29 | 29 | Dragon's Dominion | [loc_29_dragon_s_dominion.md](loc_29_dragon_s_dominion.md) |
| 30 | 30 | Dawn's Mist Bog | [loc_30_dawn_s_mist_bog.md](loc_30_dawn_s_mist_bog.md) |
| 31 | 31 | Gemmaker's Cave | [loc_31_gemmaker_s_cave.md](loc_31_gemmaker_s_cave.md) |
| 32 | 32 | Nomadic Rift | [loc_32_nomadic_rift.md](loc_32_nomadic_rift.md) |

### Overland sectors (map 33–40)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 33 | 33 · E1 | E1 | [loc_33_e1.md](loc_33_e1.md) |
| 34 | 34 · D2 | D2 | [loc_34_d2.md](loc_34_d2.md) |
| 35 | 35 · E2 | E2 | [loc_35_e2.md](loc_35_e2.md) |
| 36 | 36 · D3 | D3 | [loc_36_d3.md](loc_36_d3.md) |
| 37 | 37 · E3 | E3 | [loc_37_e3.md](loc_37_e3.md) |
| 38 | 38 · C4 | C4 | [loc_38_c4.md](loc_38_c4.md) |
| 39 | 39 · D4 | D4 | [loc_39_d4.md](loc_39_d4.md) |
| 40 | 40 · E4 | E4 | [loc_40_e4.md](loc_40_e4.md) |

### Elemental planes (map 41–44)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 41 | 41 | Elemental Plane of Air | [loc_41_elemental_plane_of_air.md](loc_41_elemental_plane_of_air.md) |
| 42 | 42 | Elemental Plane of Fire | [loc_42_elemental_plane_of_fire.md](loc_42_elemental_plane_of_fire.md) |
| 43 | 43 | Elemental Plane of Earth | [loc_43_elemental_plane_of_earth.md](loc_43_elemental_plane_of_earth.md) |
| 44 | 44 | Elemental Plane of Water | [loc_44_elemental_plane_of_water.md](loc_44_elemental_plane_of_water.md) |

### Castle basements (map 45–52)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 45 | 45 | Hillstone Dungeon Level 1 | [loc_45_hillstone_dungeon_level_1.md](loc_45_hillstone_dungeon_level_1.md) |
| 46 | 46 | Hillstone Dungeon Level 2 | [loc_46_hillstone_dungeon_level_2.md](loc_46_hillstone_dungeon_level_2.md) |
| 47 | 47 | Woodhaven Dungeon Level 1 | [loc_47_woodhaven_dungeon_level_1.md](loc_47_woodhaven_dungeon_level_1.md) |
| 48 | 48 | Woodhaven Dungeon Level 2 | [loc_48_woodhaven_dungeon_level_2.md](loc_48_woodhaven_dungeon_level_2.md) |
| 49 | 49 | Pinehurst Dungeon Level 1 | [loc_49_pinehurst_dungeon_level_1.md](loc_49_pinehurst_dungeon_level_1.md) |
| 50 | 50 | Pinehurst Dungeon Level 2 | [loc_50_pinehurst_dungeon_level_2.md](loc_50_pinehurst_dungeon_level_2.md) |
| 51 | 51 | Luxus Palace Dungeon Level 1 | [loc_51_luxus_palace_dungeon_level_1.md](loc_51_luxus_palace_dungeon_level_1.md) |
| 52 | 52 | Luxus Palace Dungeon Level 2 | [loc_52_luxus_palace_dungeon_level_2.md](loc_52_luxus_palace_dungeon_level_2.md) |

### Ancients & castles (map 53–59)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 53 | 53 | Ancients (Evil) | [loc_53_ancients_evil.md](loc_53_ancients_evil.md) |
| 54 | 54 | Ancients (Good) | [loc_54_ancients_good.md](loc_54_ancients_good.md) |
| 55 | 55 | Hillstone | [loc_55_hillstone.md](loc_55_hillstone.md) |
| 56 | 56 | Woodhaven | [loc_56_woodhaven.md](loc_56_woodhaven.md) |
| 57 | 57 | Pinehurst | [loc_57_pinehurst.md](loc_57_pinehurst.md) |
| 58 | 58 | Luxus Palace Royale | [loc_58_luxus_palace_royale.md](loc_58_luxus_palace_royale.md) |
| 59 | 59 | Castle Xabran | [loc_59_castle_xabran.md](loc_59_castle_xabran.md) |

### Overlay banks (loc 60–70, not map screens)

| Loc | Map | Area | Doc |
|-----|-----|------|-----|
| 60 | — | Quest: Nordon/Nordonna/Corak | [loc_60_quest_nordon_nordonna_corak.md](loc_60_quest_nordon_nordonna_corak.md) |
| 61 | — | Spell/hireling index tables | [loc_61_spell_hireling_index_tables.md](loc_61_spell_hireling_index_tables.md) |
| 62 | — | Side quests (Chris, Gertrude) | [loc_62_side_quests_chris_gertrude.md](loc_62_side_quests_chris_gertrude.md) |
| 63 | — | Castle blob A | [loc_63_castle_blob_a.md](loc_63_castle_blob_a.md) |
| 64 | — | Lord Haart heirloom | [loc_64_lord_haart_heirloom.md](loc_64_lord_haart_heirloom.md) |
| 65 | — | Castle blob B | [loc_65_castle_blob_b.md](loc_65_castle_blob_b.md) |
| 66 | — | Endgame Corak/Murray/Horvath | [loc_66_endgame_corak_murray_horvath.md](loc_66_endgame_corak_murray_horvath.md) |
| 67 | — | Hall of Spells pool | [loc_67_hall_of_spells_pool.md](loc_67_hall_of_spells_pool.md) |
| 68 | — | Castle blob C | [loc_68_castle_blob_c.md](loc_68_castle_blob_c.md) |
| 69 | — | Queen Lamanda (Luxus) | [loc_69_queen_lamanda_luxus.md](loc_69_queen_lamanda_luxus.md) |
| 70 | — | Meta bank (HoS, bishops, puzzles) | [loc_70_meta_bank_hos_bishops_puzzles.md](loc_70_meta_bank_hos_bishops_puzzles.md) |
