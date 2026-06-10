# Location 29 — Dragon's Dominion

- **event.dat** offset `0x0098E3`, length **1461** bytes
- **Map:** map screen **29**; Dragon's Dominion
- **Record kind:** `mixed_pool`
- **Triggers:** 75; **script segments:** 26; **strings:** 18

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **2** | DIR_W? |
| (0,4) | `0x04` | **13** | ANY_DIR |
| (0,6) | `0x06` | **11** | ANY_DIR |
| (0,7) | `0x07` | **15** | ANY_DIR |
| (0,15) | `0x0F` | **11** | ANY_DIR |
| (1,15) | `0x1F` | **11** | ANY_DIR |
| (2,11) | `0x2B` | **3** | DIR_N? |
| (2,15) | `0x2F` | **11** | ANY_DIR |
| (3,0) | `0x30` | **23** | ANY_DIR |
| (3,2) | `0x32` | **11** | ANY_DIR |
| (3,6) | `0x36` | **22** | ANY_DIR |
| (3,9) | `0x39` | **11** | ANY_DIR |
| (3,15) | `0x3F` | **11** | ANY_DIR |
| (4,4) | `0x44` | **22** | ANY_DIR |
| (4,8) | `0x48` | **23** | ANY_DIR |
| (5,0) | `0x50` | **24** | ANY_DIR |
| (5,4) | `0x54` | **11** | ANY_DIR |
| (5,7) | `0x57` | **22** | ANY_DIR |
| (5,11) | `0x5B` | **20** | ANY_DIR |
| (5,15) | `0x5F` | **12** | ANY_DIR |
| (6,3) | `0x63` | **7** | DIR_E? |
| (6,4) | `0x64` | **14** | ANY_DIR |
| (6,5) | `0x65` | **14** | ANY_DIR |
| (6,6) | `0x66` | **14** | ANY_DIR |
| (6,13) | `0x6D` | **5** | DIR_W? |
| (7,0) | `0x70` | **1** | DIR_W? |
| (7,2) | `0x72` | **11** | ANY_DIR |
| (7,3) | `0x73` | **22** | ANY_DIR |
| (7,4) | `0x74` | **11** | ANY_DIR |
| (7,7) | `0x77` | **22** | ANY_DIR |
| (7,9) | `0x79` | **18** | ANY_DIR |
| (7,10) | `0x7A` | **19** | ANY_DIR |
| (7,12) | `0x7C` | **20** | ANY_DIR |
| (7,13) | `0x7D` | **14** | ANY_DIR |
| (8,0) | `0x80` | **1** | DIR_W? |
| (8,1) | `0x81` | **10** | DIR_E? |
| (8,3) | `0x83` | **22** | ANY_DIR |
| (8,4) | `0x84` | **11** | ANY_DIR |
| (8,7) | `0x87` | **22** | ANY_DIR |
| (8,9) | `0x89` | **18** | ANY_DIR |
| (8,11) | `0x8B` | **19** | ANY_DIR |
| (8,13) | `0x8D` | **14** | ANY_DIR |
| (8,15) | `0x8F` | **17** | ANY_DIR |
| (9,0) | `0x90` | **1** | DIR_W? |
| (9,2) | `0x92` | **11** | ANY_DIR |
| (9,3) | `0x93` | **22** | ANY_DIR |
| (9,4) | `0x94` | **11** | ANY_DIR |
| (9,7) | `0x97` | **22** | ANY_DIR |
| (9,9) | `0x99` | **18** | ANY_DIR |
| (9,10) | `0x9A` | **19** | ANY_DIR |
| (9,12) | `0x9C` | **20** | ANY_DIR |
| (9,13) | `0x9D` | **14** | ANY_DIR |
| (10,3) | `0xA3` | **8** | DIR_E? |
| (10,4) | `0xA4` | **14** | ANY_DIR |
| (10,5) | `0xA5` | **14** | ANY_DIR |
| (10,6) | `0xA6` | **14** | ANY_DIR |
| (10,13) | `0xAD` | **6** | DIR_W? |
| (11,0) | `0xB0` | **24** | ANY_DIR |
| (11,4) | `0xB4` | **11** | ANY_DIR |
| (11,7) | `0xB7` | **23** | ANY_DIR |
| (11,11) | `0xBB` | **20** | ANY_DIR |
| (11,15) | `0xBF` | **21** | ANY_DIR |
| (12,2) | `0xC2` | **22** | ANY_DIR |
| (12,4) | `0xC4` | **22** | ANY_DIR |
| (13,0) | `0xD0` | **23** | ANY_DIR |
| (13,6) | `0xD6` | **22** | ANY_DIR |
| (13,8) | `0xD8` | **11** | ANY_DIR |
| (13,13) | `0xDD` | **11** | ANY_DIR |
| (13,15) | `0xDF` | **16** | ANY_DIR |
| (14,3) | `0xE3` | **9** | ANY_DIR |
| (14,7) | `0xE7` | **4** | ANY_DIR |
| (14,13) | `0xED` | **11** | ANY_DIR |
| (14,14) | `0xEE` | **11** | ANY_DIR |
| (15,4) | `0xF4` | **13** | ANY_DIR |
| (15,15) | `0xFF` | **17** | ANY_DIR |

## Events

**Event 01** — triggers: (7,0)/DIR_W?, (8,0)/DIR_W?, (9,0)/DIR_W?

```hex
01 01 09 10 01 0f 0c 08 ec
```

```
00: show_text_basic(str[1] "Leave the Dragon's Dominion (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x08, 0xEC)
03: end_script()
04: map_transition(0x08, 0xEC)
```

**Event 02** — triggers: (0,0)/DIR_W?

```hex
02 02 09 10 01 0f 24 01 00 10 03 02 03 0d 01 29 15 01 3d 00 10 04 15 01 3c 00 1b c8 10 01 1f 01 3c 02 0a 00 00 15 02 3d 00 10 04 15 02 3c 00 1b c8 10 01 1f 02 3c 02 0a 00 00 15 03 3d 00 10 04 15 03 3c 00 1b c8 10 01 1f 03 3c 02 0a 00 00 15 04 3d 00 10 04 15 04 3c 00 1b c8 10 01 1f 04 3c 02 0a 00 00 15 05 3d 00 10 04 15 05 3c 00 1b c8 10 01 1f 05 3c 02 0a 00 00 15 06 3d 00 10 04 15 06 3c 00 1b c8 10 01 1f 06 3c 02 0a 00 00 15 07 3d 00 10 04 15 07 3c 00 1b c8 10 01 1f 07 3c 02 0a 00 00 15 08 3d 00 10 04 15 08 3c 00 1b c8 10 01 1f 08 3c 02 0a 00 00 02 04 07 14
```

```
00: show_text_block(str[2] "In this small room is a circular, / swirling pool. Pitch a gold (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(1)
03: end_script()
04: cond = check_gold_at_least(1)
05: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x01, op=0x3D, val=0x00)
06: show_text_block(str[3] "Not enough gold!")
07: engine_call(0x01)
08: set_abort_and_exit()
09: apply_party(count=0x01, op=0x3D, val=0x00)
10: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x02, op=0x3D, val=0x00)
11: apply_party(count=0x01, op=0x3C, val=0x00)
12: cond = (cond >= 0xC8)
13: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x3D, val=0x00)
14: party_effect(sel=0x01, 3C 02 0A 00 00)
15: apply_party(count=0x02, op=0x3D, val=0x00)
16: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x03, op=0x3D, val=0x00)
17: apply_party(count=0x02, op=0x3C, val=0x00)
18: cond = (cond >= 0xC8)
19: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x3D, val=0x00)
20: party_effect(sel=0x02, 3C 02 0A 00 00)
21: apply_party(count=0x03, op=0x3D, val=0x00)
22: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x04, op=0x3D, val=0x00)
23: apply_party(count=0x03, op=0x3C, val=0x00)
24: cond = (cond >= 0xC8)
25: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x3D, val=0x00)
26: party_effect(sel=0x03, 3C 02 0A 00 00)
27: apply_party(count=0x04, op=0x3D, val=0x00)
28: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x05, op=0x3D, val=0x00)
29: apply_party(count=0x04, op=0x3C, val=0x00)
30: cond = (cond >= 0xC8)
31: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x3D, val=0x00)
32: party_effect(sel=0x04, 3C 02 0A 00 00)
33: apply_party(count=0x05, op=0x3D, val=0x00)
34: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x06, op=0x3D, val=0x00)
35: apply_party(count=0x05, op=0x3C, val=0x00)
36: cond = (cond >= 0xC8)
37: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x3D, val=0x00)
38: party_effect(sel=0x05, 3C 02 0A 00 00)
39: apply_party(count=0x06, op=0x3D, val=0x00)
40: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x07, op=0x3D, val=0x00)
41: apply_party(count=0x06, op=0x3C, val=0x00)
42: cond = (cond >= 0xC8)
43: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x3D, val=0x00)
44: party_effect(sel=0x06, 3C 02 0A 00 00)
45: apply_party(count=0x07, op=0x3D, val=0x00)
46: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x08, op=0x3D, val=0x00)
47: apply_party(count=0x07, op=0x3C, val=0x00)
48: cond = (cond >= 0xC8)
49: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x3D, val=0x00)
50: party_effect(sel=0x07, 3C 02 0A 00 00)
51: apply_party(count=0x08, op=0x3D, val=0x00)
52: if cond: skip_tokens(4)
    # skip -> show_text_block(str[4] "To all who are needy, +10 Hit Points.")
53: apply_party(count=0x08, op=0x3C, val=0x00)
54: cond = (cond >= 0xC8)
55: if cond: skip_tokens(1)
    # skip -> show_text_block(str[4] "To all who are needy, +10 Hit Points.")
56: party_effect(sel=0x08, 3C 02 0A 00 00)
57: show_text_block(str[4] "To all who are needy, +10 Hit Points.")
58: wait_for_space()
59: clear_current_tile_event_flag()
```

**Event 03** — triggers: (2,11)/DIR_N?

```hex
02 05 09 10 01 0f 02 06 1f 00 3c 02 19 00 00 07 14
```

```
00: show_text_block(str[5] "A blue-watered fountain beckons to / you. Drink from it's waters (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_block(str[6] "You feel much healthier but not / fulfilled. As you look back toward the")
03: end_script()
04: show_text_block(str[6] "You feel much healthier but not / fulfilled. As you look back toward the")
05: party_effect(sel=0x00, 3C 02 19 00 00)
06: wait_for_space()
07: clear_current_tile_event_flag()
```

**Event 04** — triggers: (14,7)/ANY_DIR

```hex
0e 69
```

```
00: exec_selector(0x69)
```

**Event 05** — triggers: (6,13)/DIR_W?

```hex
02 09 19 01 59 c8 07 19 01 41 c8 07 19 01 6e c8 07 10 02 01 08 0d 09 07 14
```

```
00: show_text_block(str[9] "You have found a Titan's Pike, an / Ancient Bow, and a Photon Blade.")
01: add_party_entity(0x01, f3a=0x59, f40=0xC8, f46=0x07)
02: add_party_entity(0x01, f3a=0x41, f40=0xC8, f46=0x07)
03: add_party_entity(0x01, f3a=0x6E, f40=0xC8, f46=0x07)
04: if cond: skip_tokens(2)
    # skip -> wait_for_space()
05: show_text_basic(str[8] "*** Backpacks Full ***")
06: engine_call(0x09)
07: wait_for_space()
08: clear_current_tile_event_flag()
```

**Event 06** — triggers: (10,13)/DIR_W?

```hex
0e 78
```

```
00: exec_selector(0x78)
```

**Event 07** — triggers: (6,3)/DIR_E?

```hex
02 0b 19 01 17 00 05 19 01 61 00 05 19 01 4e 00 05 10 02 01 08 0d 09 07 14
```

```
00: show_text_block(str[11] "You add a Flamberge, Broad Sword and a / Great Bow to your collection.")
01: add_party_entity(0x01, f3a=0x17, f40=0x00, f46=0x05)
02: add_party_entity(0x01, f3a=0x61, f40=0x00, f46=0x05)
03: add_party_entity(0x01, f3a=0x4E, f40=0x00, f46=0x05)
04: if cond: skip_tokens(2)
    # skip -> wait_for_space()
05: show_text_basic(str[8] "*** Backpacks Full ***")
06: engine_call(0x09)
07: wait_for_space()
08: clear_current_tile_event_flag()
```

**Event 08** — triggers: (10,3)/DIR_E?

```hex
02 0c 19 01 75 00 05 19 01 9b 00 05 19 01 85 00 05 10 02 01 08 0d 09 07 14
```

```
00: show_text_block(str[12] "In the midst of dragon dung you find / Plate Armor, a Great Shield, and ")
01: add_party_entity(0x01, f3a=0x75, f40=0x00, f46=0x05)
02: add_party_entity(0x01, f3a=0x9B, f40=0x00, f46=0x05)
03: add_party_entity(0x01, f3a=0x85, f40=0x00, f46=0x05)
04: if cond: skip_tokens(2)
    # skip -> wait_for_space()
05: show_text_basic(str[8] "*** Backpacks Full ***")
06: engine_call(0x09)
07: wait_for_space()
08: clear_current_tile_event_flag()
```

**Event 09** — triggers: (14,3)/ANY_DIR

```hex
06 0d 2a a8 61 00 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: show_text_popup_style_b(str[13] "Search / && Here &&")
01: set_treasure(gold/exp=25000, gems=0, items=[0, 0, 0])
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (8,1)/DIR_E?

```hex
06 0e
```

```
00: show_text_popup_style_b(str[14] "$ Dragon $ / Kingdom")
```

**Event 11** — triggers: (0,6)/ANY_DIR, (0,15)/ANY_DIR, (1,15)/ANY_DIR, (2,15)/ANY_DIR, (3,2)/ANY_DIR, (3,9)/ANY_DIR, (3,15)/ANY_DIR, (5,4)/ANY_DIR, (7,2)/ANY_DIR, (7,4)/ANY_DIR, (8,4)/ANY_DIR, (9,2)/ANY_DIR, (9,4)/ANY_DIR, (11,4)/ANY_DIR, (13,8)/ANY_DIR, (13,13)/ANY_DIR, (14,13)/ANY_DIR, (14,14)/ANY_DIR

```hex
02 0f 0d 09 02 10 29
```

```
00: show_text_block(str[15] "SQUISH!")
01: engine_call(0x09)
02: show_text_block(str[16] "SQUISH! The party is up to its waist / in dragon dung!")
03: set_abort_and_exit()
```

**Event 12** — triggers: (5,15)/ANY_DIR

```hex
2b 01 12 b4 a6 94 85 75 74 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=B4 A6 94 85 75 74 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (0,4)/ANY_DIR, (15,4)/ANY_DIR

```hex
2b 01 12 75 75 75 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=75 75 75 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (6,4)/ANY_DIR, (6,5)/ANY_DIR, (6,6)/ANY_DIR, (7,13)/ANY_DIR, (8,13)/ANY_DIR, (9,13)/ANY_DIR, (10,4)/ANY_DIR, (10,5)/ANY_DIR, (10,6)/ANY_DIR

```hex
2b 01 12 74 74 74 74 74 74 74 74 74 74 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=74 74 74 74 74 74 74 74 74 74, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (0,7)/ANY_DIR

```hex
2b 01 12 b4 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=B4 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (13,15)/ANY_DIR

```hex
2b 01 12 c4 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=C4 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (8,15)/ANY_DIR, (15,15)/ANY_DIR

```hex
2b 01 12 d2 00 00 00 00 00 00 00 00 00 00 00 2a 80 96 98 e8 03 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> set_treasure(gold/exp=10000000, gems=1000, items=[0, 0, 0])
01: encounter_setup(monsters=D2 00 00 00 00 00 00 00 00 00, flags=00 00)
02: set_treasure(gold/exp=10000000, gems=1000, items=[0, 0, 0])
03: clear_current_tile_event_flag()
```

**Event 18** — triggers: (7,9)/ANY_DIR, (8,9)/ANY_DIR, (9,9)/ANY_DIR

```hex
2b 01 12 75 75 75 75 75 75 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=75 75 75 75 75 75 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (7,10)/ANY_DIR, (8,11)/ANY_DIR, (9,10)/ANY_DIR

```hex
2b 01 12 85 85 85 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=85 85 85 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 20** — triggers: (5,11)/ANY_DIR, (7,12)/ANY_DIR, (9,12)/ANY_DIR, (11,11)/ANY_DIR

```hex
2b 01 12 94 94 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=94 94 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 21** — triggers: (11,15)/ANY_DIR

```hex
2b 01 12 c4 a6 94 85 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=C4 A6 94 85 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 22** — triggers: (3,6)/ANY_DIR, (4,4)/ANY_DIR, (5,7)/ANY_DIR, (7,3)/ANY_DIR, (7,7)/ANY_DIR, (8,3)/ANY_DIR, (8,7)/ANY_DIR, (9,3)/ANY_DIR, (9,7)/ANY_DIR, (12,2)/ANY_DIR, (12,4)/ANY_DIR, (13,6)/ANY_DIR

```hex
2b 01 12 74 74 75 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=74 74 75 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 23** — triggers: (3,0)/ANY_DIR, (4,8)/ANY_DIR, (11,7)/ANY_DIR, (13,0)/ANY_DIR

```hex
2b 01 12 75 85 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=75 85 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 24** — triggers: (5,0)/ANY_DIR, (11,0)/ANY_DIR

```hex
2b 01 12 74 75 94 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=74 75 94 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` Leave the Dragon's Dominion (y/n)?
- `[02]` In this small room is a circular, / swirling pool. Pitch a gold (y/n)?
- `[03]` Not enough gold!
- `[04]` To all who are needy, +10 Hit Points.
- `[05]` A blue-watered fountain beckons to / you. Drink from it's waters (y/n)?
- `[06]` You feel much healthier but not / fulfilled. As you look back toward the / fountain it vanishes.
- `[07]` A
- `[08]` *** Backpacks Full ***
- `[09]` You have found a Titan's Pike, an / Ancient Bow, and a Photon Blade.
- `[10]` S
- `[11]` You add a Flamberge, Broad Sword and a / Great Bow to your collection.
- `[12]` In the midst of dragon dung you find / Plate Armor, a Great Shield, and a / Helm.
- `[13]` Search / && Here &&
- `[14]` $ Dragon $ / Kingdom
- `[15]` SQUISH!
- `[16]` SQUISH! The party is up to its waist / in dragon dung!
- `[17]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
