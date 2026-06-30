# Location 05 — A1

- **event.dat** offset `0x001DDF`, length **1175** bytes
- **Map:** map screen **5**; overland sector **A1**
- **Record kind:** `standard`
- **Triggers:** 27; **script segments:** 15; **strings:** 13

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **7** | ANY_DIR |
| (0,8) | `0x08` | **6** | ANY_DIR |
| (1,5) | `0x15` | **5** | ANY_DIR |
| (1,12) | `0x1C` | **5** | ANY_DIR |
| (2,6) | `0x26` | **11** | ANY_DIR |
| (3,2) | `0x32` | **8** | ANY_DIR |
| (3,8) | `0x38` | **6** | ANY_DIR |
| (3,11) | `0x3B` | **13** | 0xA0 |
| (3,12) | `0x3C` | **1** | DIR_W? |
| (3,14) | `0x3E` | **10** | ANY_DIR |
| (4,6) | `0x46` | **7** | ANY_DIR |
| (5,9) | `0x59` | **11** | ANY_DIR |
| (5,11) | `0x5B` | **5** | ANY_DIR |
| (6,10) | `0x6A` | **10** | ANY_DIR |
| (6,15) | `0x6F` | **5** | ANY_DIR |
| (8,2) | `0x82` | **9** | ANY_DIR |
| (8,8) | `0x88` | **4** | ANY_DIR |
| (8,10) | `0x8A` | **7** | ANY_DIR |
| (8,12) | `0x8C` | **6** | ANY_DIR |
| (9,14) | `0x9E` | **5** | ANY_DIR |
| (10,6) | `0xA6` | **9** | ANY_DIR |
| (10,15) | `0xAF` | **12** | ANY_DIR |
| (11,14) | `0xBE` | **7** | ANY_DIR |
| (13,5) | `0xD5` | **9** | ANY_DIR |
| (14,1) | `0xE1` | **3** | ANY_DIR |
| (14,10) | `0xEA` | **9** | ANY_DIR |
| (15,0) | `0xF0` | **2** | 0x90 |

## Events

**Event 01** — triggers: (3,12)/DIR_W?

```hex
0b 18 00 02 01 0a 11 01 0c 02 bf 0f
```

```
00: service_sign(idx=0x18 -> sign 29 [29.anm], pos=0x00)
01: show_text_block(str[1] "As snow swirls about your party, you / catch a glimpse of the Tundaran t")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x02, 0xBF)
05: end_script()
```

**Event 02** — triggers: (15,0)/0x90

```hex
02 02 09 11 02 1a 84 02 0c 29 f0 0f
```

```
00: show_text_block(str[2] "A violent vortex of swirling wind / creates a passage to the time / of A")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: store_var8(group=0x84, value=0x02)
04: map_transition(0x29, 0xF0)
05: end_script()
```

**Event 03** — triggers: (14,1)/ANY_DIR

```hex
03 03 2e f1 04 07 14
```

```
00: show_text(str[3] "Through your party's harrowing travels / in the Elemental Plane of Air, ")
01: set_party_attr(class=0xF1, bits=0x04)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 04** — triggers: (8,8)/ANY_DIR

```hex
03 04 2e f0 20 07 14
```

```
00: show_text(str[4] "You find a lone, deserted outpost on / the edge of the Elemental Plane o")
01: set_party_attr(class=0xF0, bits=0x20)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 05** — triggers: (1,5)/ANY_DIR, (1,12)/ANY_DIR, (5,11)/ANY_DIR, (6,15)/ANY_DIR, (9,14)/ANY_DIR

```hex
1c 64 1b 1e 10 05 01 05 07 0d 09 31 00 14 00 0c 85 00 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_basic(str[5] "A blizzard blinds the party!")
04: wait_for_space()
05: engine_call(0x09)
06: for_party(mask=0x00): call(0x14,0x00)
07: map_transition(0x85, 0x00)
08: end_script()
```

**Event 06** — triggers: (0,8)/ANY_DIR, (3,8)/ANY_DIR, (8,12)/ANY_DIR

```hex
1c 64 1b 1e 10 05 01 06 07 0d 09 31 00 1e 00 0c 85 00 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_basic(str[6] "A snow drift sweeps the party away!")
04: wait_for_space()
05: engine_call(0x09)
06: for_party(mask=0x00): call(0x1E,0x00)
07: map_transition(0x85, 0x00)
08: end_script()
```

**Event 07** — triggers: (0,2)/ANY_DIR, (4,6)/ANY_DIR, (8,10)/ANY_DIR, (11,14)/ANY_DIR

```hex
1c 64 1b 1e 10 05 01 07 07 0d 09 31 00 32 00 0c 85 00 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_basic(str[7] "Avalanche!")
04: wait_for_space()
05: engine_call(0x09)
06: for_party(mask=0x00): call(0x32,0x00)
07: map_transition(0x85, 0x00)
08: end_script()
```

**Event 08** — triggers: (3,2)/ANY_DIR

```hex
02 08 09 11 01 18 00 26 00 19 0f
```

```
00: show_text_block(str[8] "A frozen fountain's water swirls / under a bed of ice. Break and / drink")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x26, and=0x00, or=0x19)
04: end_script()
```

**Event 09** — triggers: (8,2)/ANY_DIR, (10,6)/ANY_DIR, (13,5)/ANY_DIR, (14,10)/ANY_DIR

```hex
2b 02 02 09 12 c2 c2 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[9] "/         Air Elementals Attack!")
02: encounter_setup(monsters=C2 C2 00 00 00 00 00 00 00 00, flags=00 00)
03: clear_current_tile_event_flag()
```

**Event 10** — triggers: (3,14)/ANY_DIR, (6,10)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 e0 e1 e1 c1 c1 c1 c1 c2 c2 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=E0 E1 E1 C1 C1 C1 C1 C2 C2 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 11** — triggers: (2,6)/ANY_DIR, (5,9)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 e1 c1 c1 c2 c2 c2 c2 ce ce 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=E1 C1 C1 C2 C2 C2 C2 CE CE 00)
04: clear_current_tile_event_flag()
```

**Event 12** — triggers: (10,15)/ANY_DIR

```hex
22 07 07 11 02 2b 01 13 ce ce ce ce 00 00 00 00 00 00 14
```

```
00: cond = (era in [7..7])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE CE CE CE 00 00 00 00 00 00)
04: clear_current_tile_event_flag()
```

**Event 13** — triggers: (3,11)/0xA0

```hex
22 06 06 11 07 2b 04 02 0a 09 11 04 12 ea 76 76 76 76 76 76 00 00 00 00 00 2a 00 00 00 00 00 c8 00 00 64 00 00 05 00 00 18 00 76 7f 80 14 02 0b 29
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(7)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(4)
    # skip -> set_treasure(gold/exp=0, gems=0, items=[200, 0, 0])
03: show_text_block(str[10] "Unconquerable Spaz Twit and his mighty / phaser own this area. Do you ch")
04: cond = prompt_yes_no()
05: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[11] "You are wise.")
06: encounter_setup(monsters=EA 76 76 76 76 76 76 00 00 00, flags=00 00)
07: set_treasure(gold/exp=0, gems=0, items=[200, 0, 0])
08: apply_party_masked(count=0x00, set=0x76, and=0x7F, or=0x80)
09: clear_current_tile_event_flag()
10: show_text_block(str[11] "You are wise.")
11: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` As snow swirls about your party, you / catch a glimpse of the Tundaran town / gates. Enter (y/n)?
- `[02]` A violent vortex of swirling wind / creates a passage to the time / of Air. Enter (y/n)?
- `[03]` Through your party's harrowing travels / in the Elemental Plane of Air, the / clerical spell casters have come to / an understanding of the area and have / formulated the spell, Air Encasement.
- `[04]` You find a lone, deserted outpost on / the edge of the Elemental Plane of / Air. As you sift through the ruins, / an ancient scroll is found bearing the / lost spell, Air Transmutation.
- `[05]` A blizzard blinds the party!
- `[06]` A snow drift sweeps the party away!
- `[07]` Avalanche!
- `[08]` A frozen fountain's water swirls / under a bed of ice. Break and / drink (y/n)?
- `[09]`  /         Air Elementals Attack!
- `[10]` Unconquerable Spaz Twit and his mighty / phaser own this area. Do you challenge / their dominion (y/n)?
- `[11]` You are wise.
- `[12]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
