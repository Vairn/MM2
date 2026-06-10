# Location 27 — Druid's Cave

- **event.dat** offset `0x008F8B`, length **1466** bytes
- **Map:** map screen **27**; Druid's Cave
- **Record kind:** `mixed_pool`
- **Triggers:** 80; **script segments:** 44; **strings:** 24

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **20** | ANY_DIR |
| (0,3) | `0x03` | **16** | DIR_N? |
| (0,4) | `0x04` | **16** | DIR_N? |
| (0,5) | `0x05` | **16** | DIR_N? |
| (0,6) | `0x06` | **16** | DIR_N? |
| (0,8) | `0x08` | **16** | DIR_E? |
| (0,12) | `0x0C` | **13** | ANY_DIR |
| (1,10) | `0x1A` | **19** | ANY_DIR |
| (1,12) | `0x1C` | **8** | DIR_S? |
| (1,14) | `0x1E` | **16** | DIR_S? |
| (2,1) | `0x21` | **18** | ANY_DIR |
| (2,2) | `0x22` | **13** | ANY_DIR |
| (2,5) | `0x25` | **13** | ANY_DIR |
| (2,8) | `0x28` | **20** | ANY_DIR |
| (2,13) | `0x2D` | **21** | ANY_DIR |
| (2,15) | `0x2F` | **13** | ANY_DIR |
| (3,4) | `0x34` | **17** | ANY_DIR |
| (3,8) | `0x38` | **41** | DIR_N? |
| (3,12) | `0x3C` | **20** | ANY_DIR |
| (4,1) | `0x41` | **19** | ANY_DIR |
| (4,4) | `0x44` | **14** | ANY_DIR |
| (4,8) | `0x48` | **17** | ANY_DIR |
| (4,10) | `0x4A` | **19** | ANY_DIR |
| (5,1) | `0x51` | **38** | ANY_DIR |
| (5,5) | `0x55` | **32** | ANY_DIR |
| (5,6) | `0x56` | **14** | ANY_DIR |
| (6,6) | `0x66` | **16** | DIR_E? |
| (6,10) | `0x6A` | **18** | ANY_DIR |
| (6,13) | `0x6D` | **20** | ANY_DIR |
| (6,14) | `0x6E` | **16** | DIR_E? |
| (7,3) | `0x73` | **32** | ANY_DIR |
| (7,6) | `0x76` | **32** | ANY_DIR |
| (7,8) | `0x78` | **5** | DIR_S? |
| (7,10) | `0x7A` | **19** | ANY_DIR |
| (7,14) | `0x7E` | **12** | ANY_DIR |
| (8,0) | `0x80` | **14** | ANY_DIR |
| (8,1) | `0x81` | **42** | DIR_W? |
| (8,2) | `0x82` | **33** | ANY_DIR |
| (8,4) | `0x84` | **15** | DIR_W? |
| (8,5) | `0x85` | **14** | ANY_DIR |
| (8,7) | `0x87` | **6** | DIR_W? |
| (8,8) | `0x88` | **1** | ANY_DIR |
| (8,9) | `0x89` | **4** | DIR_E? |
| (8,11) | `0x8B` | **12** | ANY_DIR |
| (9,4) | `0x94` | **9** | DIR_W? |
| (9,8) | `0x98` | **3** | DIR_N? |
| (9,11) | `0x9B` | **25** | ANY_DIR |
| (9,12) | `0x9C` | **24** | ANY_DIR |
| (9,14) | `0x9E` | **25** | ANY_DIR |
| (9,15) | `0x9F` | **12** | ANY_DIR |
| (10,2) | `0xA2` | **33** | ANY_DIR |
| (10,8) | `0xA8` | **11** | ANY_DIR |
| (10,12) | `0xAC` | **23** | ANY_DIR |
| (11,0) | `0xB0` | **34** | ANY_DIR |
| (11,4) | `0xB4` | **16** | DIR_W? |
| (11,7) | `0xB7` | **36** | ANY_DIR |
| (11,9) | `0xB9` | **27** | ANY_DIR |
| (11,10) | `0xBA` | **26** | ANY_DIR |
| (11,15) | `0xBF` | **40** | DIR_S? |
| (12,1) | `0xC1` | **39** | ANY_DIR |
| (12,5) | `0xC5` | **16** | DIR_W? |
| (12,9) | `0xC9` | **11** | ANY_DIR |
| (12,10) | `0xCA` | **7** | DIR_N? |
| (12,14) | `0xCE` | **22** | ANY_DIR |
| (13,0) | `0xD0` | **14** | ANY_DIR |
| (13,5) | `0xD5` | **28** | ANY_DIR |
| (13,8) | `0xD8` | **35** | ANY_DIR |
| (13,9) | `0xD9` | **28** | ANY_DIR |
| (13,13) | `0xDD` | **31** | ANY_DIR |
| (13,14) | `0xDE` | **30** | ANY_DIR |
| (14,2) | `0xE2` | **16** | DIR_S? |
| (14,7) | `0xE7` | **16** | DIR_W? |
| (14,11) | `0xEB` | **10** | DIR_N? |
| (14,15) | `0xEF` | **37** | DIR_S? |
| (15,1) | `0xF1` | **2** | DIR_E? |
| (15,2) | `0xF2` | **14** | ANY_DIR |
| (15,4) | `0xF4` | **36** | ANY_DIR |
| (15,8) | `0xF8` | **34** | ANY_DIR |
| (15,10) | `0xFA` | **29** | ANY_DIR |
| (15,11) | `0xFB` | **11** | ANY_DIR |

## Events

**Event 01** — triggers: (8,8)/ANY_DIR

```hex
01 01 09 11 01 0c 0e 61 0f
```

```
00: show_text_basic(str[1] "Stairs leading up. Take them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x0E, 0x61)
04: end_script()
```

**Event 02** — triggers: (15,1)/DIR_E?

```hex
02 02 09 11 22 15 01 10 00 1b 32 10 01 1f 01 10 01 0a 00 00 15 02 10 00 1b 32 10 01 1f 02 10 01 0a 00 00 15 03 10 00 1b 32 10 01 1f 03 10 01 0a 00 00 15 04 10 00 1b 32 10 01 1f 04 10 01 0a 00 00 15 05 10 00 1b 32 10 01 1f 05 10 01 0a 00 00 15 06 10 00 1b 32 10 01 1f 06 10 01 0a 00 00 15 07 10 00 1b 32 10 01 1f 07 10 01 0a 00 00 15 08 10 00 1b 32 10 01 1f 08 10 01 0a 00 00 02 03 07 14
```

```
00: show_text_block(str[2] "Cans of spinach are everywhere. / Eat some (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(34)
    # skip -> clear_current_tile_event_flag()
03: apply_party(count=0x01, op=0x10, val=0x00)
04: cond = (cond >= 0x32)
05: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x10, val=0x00)
06: party_effect(sel=0x01, 10 01 0A 00 00)
07: apply_party(count=0x02, op=0x10, val=0x00)
08: cond = (cond >= 0x32)
09: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x10, val=0x00)
10: party_effect(sel=0x02, 10 01 0A 00 00)
11: apply_party(count=0x03, op=0x10, val=0x00)
12: cond = (cond >= 0x32)
13: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x10, val=0x00)
14: party_effect(sel=0x03, 10 01 0A 00 00)
15: apply_party(count=0x04, op=0x10, val=0x00)
16: cond = (cond >= 0x32)
17: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x10, val=0x00)
18: party_effect(sel=0x04, 10 01 0A 00 00)
19: apply_party(count=0x05, op=0x10, val=0x00)
20: cond = (cond >= 0x32)
21: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x10, val=0x00)
22: party_effect(sel=0x05, 10 01 0A 00 00)
23: apply_party(count=0x06, op=0x10, val=0x00)
24: cond = (cond >= 0x32)
25: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x10, val=0x00)
26: party_effect(sel=0x06, 10 01 0A 00 00)
27: apply_party(count=0x07, op=0x10, val=0x00)
28: cond = (cond >= 0x32)
29: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x10, val=0x00)
30: party_effect(sel=0x07, 10 01 0A 00 00)
31: apply_party(count=0x08, op=0x10, val=0x00)
32: cond = (cond >= 0x32)
33: if cond: skip_tokens(1)
    # skip -> show_text_block(str[3] "You're strong to the finish / 'cause you ate your spinach.")
34: party_effect(sel=0x08, 10 01 0A 00 00)
35: show_text_block(str[3] "You're strong to the finish / 'cause you ate your spinach.")
36: wait_for_space()
37: clear_current_tile_event_flag()
```

**Event 03** — triggers: (9,8)/DIR_N?

```hex
06 04
```

```
00: show_text_popup_style_b(str[4] "Water / Druids")
```

**Event 04** — triggers: (8,9)/DIR_E?

```hex
06 05
```

```
00: show_text_popup_style_b(str[5] "Air / Druids")
```

**Event 05** — triggers: (7,8)/DIR_S?

```hex
06 06
```

```
00: show_text_popup_style_b(str[6] "Fire / Druids")
```

**Event 06** — triggers: (8,7)/DIR_W?

```hex
06 07
```

```
00: show_text_popup_style_b(str[7] "Earth / Druids")
```

**Event 07** — triggers: (12,10)/DIR_N?

```hex
02 08 07 19 01 47 00 02 10 02 01 13 07 14
```

```
00: show_text_block(str[8] "You have found a +2 Trident!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x47, f40=0x00, f46=0x02)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[19] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 08** — triggers: (1,12)/DIR_S?

```hex
01 09 07 19 01 61 00 02 10 02 01 13 07 14
```

```
00: show_text_basic(str[9] "You have found a +2 Great Bow!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x61, f40=0x00, f46=0x02)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[19] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 09** — triggers: (9,4)/DIR_W?

```hex
01 0a 07 19 01 24 05 02 10 02 01 13 07 14
```

```
00: show_text_basic(str[10] "You have found a +2 Fiery Spear!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x24, f40=0x05, f46=0x02)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[19] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 10** — triggers: (14,11)/DIR_N?

```hex
01 0b 07 19 01 8f 00 02 10 02 01 13 07 14
```

```
00: show_text_basic(str[11] "You have found +2 Silver Chain Mail!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x8F, f40=0x00, f46=0x02)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[19] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 11** — triggers: (10,8)/ANY_DIR, (12,9)/ANY_DIR, (15,11)/ANY_DIR

```hex
01 0c 0d 09 31 00 05 00 29
```

```
00: show_text_basic(str[12] "Water Snare!")
01: engine_call(0x09)
02: for_party(mask=0x00): call(0x05,0x00)
03: set_abort_and_exit()
```

**Event 12** — triggers: (7,14)/ANY_DIR, (8,11)/ANY_DIR, (9,15)/ANY_DIR

```hex
01 0d 0d 09 31 00 0a 00 29
```

```
00: show_text_basic(str[13] "Air Snare!")
01: engine_call(0x09)
02: for_party(mask=0x00): call(0x0A,0x00)
03: set_abort_and_exit()
```

**Event 13** — triggers: (0,12)/ANY_DIR, (2,2)/ANY_DIR, (2,5)/ANY_DIR, (2,15)/ANY_DIR

```hex
01 0e 0d 09 31 00 0f 00 29
```

```
00: show_text_basic(str[14] "Fire Snare!")
01: engine_call(0x09)
02: for_party(mask=0x00): call(0x0F,0x00)
03: set_abort_and_exit()
```

**Event 14** — triggers: (4,4)/ANY_DIR, (5,6)/ANY_DIR, (8,0)/ANY_DIR, (8,5)/ANY_DIR, (13,0)/ANY_DIR, (15,2)/ANY_DIR

```hex
01 0f 0d 09 31 00 14 00 29
```

```
00: show_text_basic(str[15] "Earth Snare!")
01: engine_call(0x09)
02: for_party(mask=0x00): call(0x14,0x00)
03: set_abort_and_exit()
```

**Event 15** — triggers: (8,4)/DIR_W?

```hex
04 10
```

```
00: show_text_above_door(str[16] "Coven Headquarters")
```

**Event 16** — triggers: (0,3)/DIR_N?, (0,4)/DIR_N?, (0,5)/DIR_N?, (0,6)/DIR_N?, (0,8)/DIR_E?, (1,14)/DIR_S?, (6,6)/DIR_E?, (6,14)/DIR_E?, (11,4)/DIR_W?, (12,5)/DIR_W?, (14,2)/DIR_S?, (14,7)/DIR_W?

```hex
04 11
```

```
00: show_text_above_door(str[17] "Meditation Room")
```

**Event 17** — triggers: (3,4)/ANY_DIR, (4,8)/ANY_DIR

```hex
2b 01 12 42 4a 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=42 4A 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (2,1)/ANY_DIR, (6,10)/ANY_DIR

```hex
2b 01 12 42 42 4a 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=42 42 4A 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (1,10)/ANY_DIR, (4,1)/ANY_DIR, (4,10)/ANY_DIR, (7,10)/ANY_DIR

```hex
2b 01 12 42 42 42 4a 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=42 42 42 4A 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 20** — triggers: (0,2)/ANY_DIR, (2,8)/ANY_DIR, (3,12)/ANY_DIR, (6,13)/ANY_DIR

```hex
2b 01 12 42 42 42 42 4a 4a 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=42 42 42 42 4A 4A 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 21** — triggers: (2,13)/ANY_DIR

```hex
2b 01 12 68 68 4a 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=68 68 4A 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 22** — triggers: (12,14)/ANY_DIR

```hex
2b 01 12 14 14 14 14 14 14 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=14 14 14 14 14 14 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 23** — triggers: (10,12)/ANY_DIR

```hex
2b 01 12 1e 1e 1e 1e 1e 1e 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1E 1E 1E 1E 1E 1E 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 24** — triggers: (9,12)/ANY_DIR

```hex
2b 01 12 14 14 14 14 1e 1e 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=14 14 14 14 1E 1E 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 25** — triggers: (9,11)/ANY_DIR, (9,14)/ANY_DIR

```hex
2b 01 12 1e 1e 1e 1e 14 14 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1E 1E 1E 1E 14 14 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 26** — triggers: (11,10)/ANY_DIR

```hex
2b 01 12 1e 1e 1e 1e 14 14 14 14 4a 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1E 1E 1E 1E 14 14 14 14 4A 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 27** — triggers: (11,9)/ANY_DIR

```hex
2b 01 12 02 02 02 02 02 02 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=02 02 02 02 02 02 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 28** — triggers: (13,5)/ANY_DIR, (13,9)/ANY_DIR

```hex
2b 01 12 02 02 13 13 13 13 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=02 02 13 13 13 13 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 29** — triggers: (15,10)/ANY_DIR

```hex
2b 01 12 02 02 02 13 13 13 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=02 02 02 13 13 13 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 30** — triggers: (13,14)/ANY_DIR

```hex
2b 01 12 02 02 02 02 13 13 13 13 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=02 02 02 02 13 13 13 13 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 31** — triggers: (13,13)/ANY_DIR

```hex
2b 01 12 02 02 02 13 13 13 13 13 4a 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=02 02 02 13 13 13 13 13 4A 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 32** — triggers: (5,5)/ANY_DIR, (7,3)/ANY_DIR, (7,6)/ANY_DIR

```hex
2b 01 12 35 35 35 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=35 35 35 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 33** — triggers: (8,2)/ANY_DIR, (10,2)/ANY_DIR

```hex
2b 01 12 36 36 35 35 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=36 36 35 35 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 34** — triggers: (11,0)/ANY_DIR, (15,8)/ANY_DIR

```hex
2b 01 12 36 36 36 36 35 35 35 35 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=36 36 36 36 35 35 35 35 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 35** — triggers: (13,8)/ANY_DIR

```hex
2b 01 12 44 44 36 36 35 35 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=44 44 36 36 35 35 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 36** — triggers: (11,7)/ANY_DIR, (15,4)/ANY_DIR

```hex
2b 01 12 6c 4a 4a 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6C 4A 4A 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 37** — triggers: (14,15)/DIR_S?

```hex
0e 67
```

```
00: exec_selector(0x67)
```

**Event 38** — triggers: (5,1)/ANY_DIR

```hex
2b 08 15 00 7e 08 10 04 01 14 07 0d 09 0c 1b 88 02 15 12 f7 00 00 00 00 00 00 00 00 00 00 00 18 00 7e fb 04 14
```

```
00: skip_tokens(8)
    # skip -> apply_party_masked(count=0x00, set=0x7E, and=0xFB, or=0x04)
01: apply_party(count=0x00, op=0x7E, val=0x08)
02: if cond: skip_tokens(4)
    # skip -> show_text_block(str[21] ""I swear it wasn't my fault," cries / the once-mighty Horvath. Out of / ")
03: show_text_basic(str[20] ""It's not my fault - Leave me alone!"")
04: wait_for_space()
05: engine_call(0x09)
06: map_transition(0x1B, 0x88)
07: show_text_block(str[21] ""I swear it wasn't my fault," cries / the once-mighty Horvath. Out of / ")
08: encounter_setup(monsters=F7 00 00 00 00 00 00 00 00 00, flags=00 00)
09: apply_party_masked(count=0x00, set=0x7E, and=0xFB, or=0x04)
10: clear_current_tile_event_flag()
```

**Event 39** — triggers: (12,1)/ANY_DIR

```hex
0e 68
```

```
00: exec_selector(0x68)
```

**Event 40** — triggers: (11,15)/DIR_S?

```hex
0e 6e
```

```
00: exec_selector(0x6E)
```

**Event 41** — triggers: (3,8)/DIR_N?

```hex
0e 6f
```

```
00: exec_selector(0x6F)
```

**Event 42** — triggers: (8,1)/DIR_W?

```hex
0e 72
```

```
00: exec_selector(0x72)
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs leading up. Take them (y/n)?
- `[02]` Cans of spinach are everywhere. / Eat some (y/n)?
- `[03]` You're strong to the finish / 'cause you ate your spinach.
- `[04]` Water / Druids
- `[05]` Air / Druids
- `[06]` Fire / Druids
- `[07]` Earth / Druids
- `[08]` You have found a +2 Trident!
- `[09]` You have found a +2 Great Bow!
- `[10]` You have found a +2 Fiery Spear!
- `[11]` You have found +2 Silver Chain Mail!
- `[12]` Water Snare!
- `[13]` Air Snare!
- `[14]` Fire Snare!
- `[15]` Earth Snare!
- `[16]` Coven Headquarters
- `[17]` Meditation Room
- `[18]` K
- `[19]` *** Backpacks Full ***
- `[20]` "It's not my fault - Leave me alone!"
- `[21]` "I swear it wasn't my fault," cries / the once-mighty Horvath. Out of / mercy, your party attacks it.
- `[22]` m
- `[23]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
