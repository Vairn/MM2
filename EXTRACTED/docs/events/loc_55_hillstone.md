# Location 55 — Hillstone

- **event.dat** offset `0x01125F`, length **1463** bytes
- **Map:** map screen **55**; Hillstone
- **Record kind:** `mixed_pool`
- **Triggers:** 72; **script segments:** 43; **strings:** 23

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **18** | ANY_DIR |
| (0,3) | `0x03` | **3** | DIR_W? |
| (0,12) | `0x0C` | **3** | DIR_E? |
| (0,13) | `0x0D` | **18** | ANY_DIR |
| (1,1) | `0x11` | **19** | ANY_DIR |
| (1,8) | `0x18` | **19** | ANY_DIR |
| (1,9) | `0x19` | **19** | ANY_DIR |
| (1,14) | `0x1E` | **19** | ANY_DIR |
| (2,0) | `0x20` | **18** | ANY_DIR |
| (2,5) | `0x25` | **15** | DIR_S? |
| (2,8) | `0x28` | **19** | ANY_DIR |
| (2,9) | `0x29` | **19** | ANY_DIR |
| (2,11) | `0x2B` | **2** | DIR_N? |
| (2,12) | `0x2C` | **13** | DIR_N? |
| (2,13) | `0x2D` | **41** | 0x90 |
| (2,15) | `0x2F` | **18** | ANY_DIR |
| (3,0) | `0x30` | **3** | DIR_S? |
| (3,2) | `0x32` | **12** | DIR_S? |
| (3,5) | `0x35` | **6** | DIR_S? |
| (3,12) | `0x3C` | **37** | ANY_DIR |
| (3,14) | `0x3E` | **39** | ANY_DIR |
| (3,15) | `0x3F` | **3** | DIR_S? |
| (4,8) | `0x48` | **16** | DIR_S? |
| (4,11) | `0x4B` | **17** | DIR_N? |
| (4,12) | `0x4C` | **35** | ANY_DIR |
| (4,14) | `0x4E` | **38** | ANY_DIR |
| (5,5) | `0x55` | **9** | DIR_W? |
| (5,10) | `0x5A` | **18** | ANY_DIR |
| (5,12) | `0x5C` | **33** | ANY_DIR |
| (5,14) | `0x5E` | **36** | ANY_DIR |
| (6,8) | `0x68` | **1** | DIR_S? |
| (6,12) | `0x6C` | **31** | ANY_DIR |
| (6,14) | `0x6E` | **34** | ANY_DIR |
| (7,5) | `0x75` | **9** | DIR_W? |
| (7,9) | `0x79` | **18** | ANY_DIR |
| (7,10) | `0x7A` | **18** | ANY_DIR |
| (7,12) | `0x7C` | **29** | ANY_DIR |
| (7,14) | `0x7E` | **32** | ANY_DIR |
| (8,12) | `0x8C` | **27** | ANY_DIR |
| (8,14) | `0x8E` | **30** | ANY_DIR |
| (9,2) | `0x92` | **9** | DIR_S? |
| (9,3) | `0x93` | **9** | DIR_S? |
| (9,5) | `0x95` | **20** | DIR_S? |
| (9,12) | `0x9C` | **25** | ANY_DIR |
| (9,14) | `0x9E` | **28** | ANY_DIR |
| (10,12) | `0xAC` | **23** | ANY_DIR |
| (10,14) | `0xAE` | **26** | ANY_DIR |
| (11,10) | `0xBA` | **4** | DIR_E? |
| (11,11) | `0xBB` | **5** | DIR_N? |
| (11,14) | `0xBE` | **24** | ANY_DIR |
| (12,0) | `0xC0` | **3** | DIR_N? |
| (12,4) | `0xC4` | **18** | ANY_DIR |
| (12,8) | `0xC8` | **18** | ANY_DIR |
| (12,11) | `0xCB` | **40** | ANY_DIR |
| (12,12) | `0xCC` | **21** | ANY_DIR |
| (12,14) | `0xCE` | **22** | ANY_DIR |
| (12,15) | `0xCF` | **3** | DIR_N? |
| (13,0) | `0xD0` | **18** | ANY_DIR |
| (13,3) | `0xD3` | **14** | DIR_S? |
| (13,5) | `0xD5` | **19** | ANY_DIR |
| (13,7) | `0xD7` | **8** | DIR_S? |
| (13,8) | `0xD8` | **7** | DIR_S? |
| (13,10) | `0xDA` | **10** | 0x30 |
| (13,15) | `0xDF` | **18** | ANY_DIR |
| (14,1) | `0xE1` | **19** | ANY_DIR |
| (14,14) | `0xEE` | **19** | ANY_DIR |
| (15,2) | `0xF2` | **18** | ANY_DIR |
| (15,3) | `0xF3` | **3** | DIR_W? |
| (15,7) | `0xF7` | **11** | DIR_N? |
| (15,8) | `0xF8` | **11** | DIR_N? |
| (15,12) | `0xFC` | **3** | DIR_E? |
| (15,13) | `0xFD` | **18** | ANY_DIR |

## Events

**Event 01** — triggers: (6,8)/DIR_S?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Prison")
```

**Event 02** — triggers: (2,11)/DIR_N?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Red Room")
```

**Event 03** — triggers: (0,3)/DIR_W?, (0,12)/DIR_E?, (3,0)/DIR_S?, (3,15)/DIR_S?, (12,0)/DIR_N?, (12,15)/DIR_N?, (15,3)/DIR_W?, (15,12)/DIR_E?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Guard Tower")
```

**Event 04** — triggers: (11,10)/DIR_E?

```hex
04 04
```

```
00: show_text_above_door(str[4] "The Zoo")
```

**Event 05** — triggers: (11,11)/DIR_N?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Zookeeper")
```

**Event 06** — triggers: (3,5)/DIR_S?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Throne Room")
```

**Event 07** — triggers: (13,8)/DIR_S?

```hex
04 07
```

```
00: show_text_above_door(str[7] "No Entry")
```

**Event 08** — triggers: (13,7)/DIR_S?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Slayer's Palace")
```

**Event 09** — triggers: (5,5)/DIR_W?, (7,5)/DIR_W?, (9,2)/DIR_S?, (9,3)/DIR_S?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Grand Hall")
```

**Event 10** — triggers: (13,10)/0x30

```hex
01 0a 09 11 01 0c 2d f8 0f
```

```
00: show_text_basic(str[10] "Stairs to Dungeon. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x2D, 0xF8)
04: end_script()
```

**Event 11** — triggers: (15,7)/DIR_N?, (15,8)/DIR_N?

```hex
01 0b 09 11 01 0c 27 1d 0f
```

```
00: show_text_basic(str[11] "Castle Exit. Leave (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x27, 0x1D)
04: end_script()
```

**Event 12** — triggers: (3,2)/DIR_S?

```hex
01 0c 0d 09 29
```

```
00: show_text_basic(str[12] "You win the Booby Prize!")
01: engine_call(0x09)
02: set_abort_and_exit()
```

**Event 13** — triggers: (2,12)/DIR_N?

```hex
02 0d 09 11 01 18 00 42 00 28 14
```

```
00: show_text_block(str[13] "Food is stacked to the ceiling. Take / some (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: apply_party_masked(count=0x00, set=0x42, and=0x00, or=0x28)
04: clear_current_tile_event_flag()
```

**Event 14** — triggers: (13,3)/DIR_S?

```hex
02 0e 19 01 fe 07 00 10 01 01 0f 07 14
```

```
00: show_text_block(str[14] "You have found an N-19 Capitor.")
01: add_party_entity(0x01, f3a=0xFE, f40=0x07, f46=0x00)
02: if cond: skip_tokens(1)
    # skip -> wait_for_space()
03: show_text_basic(str[15] "*** Backpacks Full ***")
04: wait_for_space()
05: clear_current_tile_event_flag()
```

**Event 15** — triggers: (2,5)/DIR_S?

```hex
0b 0e 00 32 04 11 01 0e ca 02 15 08 0f
```

```
00: service_sign(idx=0x0E -> sign 49 [49.anm], pos=0x00)
01: cond = load_cond_from_var(0x04)
02: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[21] "Lord Slayer will only bestow a / quest unto a Crusader.")
03: exec_selector(0xCA)  # quest_handler_202
04: show_text_block(str[21] "Lord Slayer will only bestow a / quest unto a Crusader.")
05: wait_key()
06: end_script()
```

**Event 16** — triggers: (4,8)/DIR_S?

```hex
17 0b 00 10 01 0e f8 14
```

```
00: cond = load_var8(group=0x0B, index=0x00)
01: if cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
02: exec_selector(0xF8)
03: clear_current_tile_event_flag()
```

**Event 17** — triggers: (4,11)/DIR_N?

```hex
0b 05 00 28 01 71 11 2c 02 13 1f 00 31 04 40 1f 00 15 01 7d 61 1b 61 11 02 1f 01 31 04 a0 86 01 18 01 7d 9e 00 15 02 7d 61 1b 61 11 02 1f 02 31 04 a0 86 01 18 02 7d 9e 00 15 03 7d 61 1b 61 11 02 1f 03 31 04 a0 86 01 18 03 7d 9e 00 15 04 7d 61 1b 61 11 02 1f 04 31 04 a0 86 01 18 04 7d 9e 00 15 05 7d 61 1b 61 11 02 1f 05 31 04 a0 86 01 18 05 7d 9e 00 15 06 7d 61 1b 61 11 02 1f 06 31 04 a0 86 01 18 06 7d 9e 00 15 07 7d 61 1b 61 11 02 1f 07 31 04 a0 86 01 18 07 7d 9e 00 15 08 7d 61 1b 61 11 02 1f 08 31 04 a0 86 01 18 08 7d 9e 00 08 14 02 12 08 0f
```

```
00: service_sign(idx=0x05 -> sign 72 [72.anm], pos=0x00)
01: cond = consume_item(item_id=113, name="Red Key", probe=1)
02: if not cond: skip_tokens(44)
    # skip -> show_text_block(str[18] "The Bishop of Red Battle is locked in / a cage.")
03: show_text_block(str[19] "The Bishop of Red Battle is freed / from imprisonment by your red key / ")
04: party_effect(sel=0x00, 31 04 40 1F 00)
05: apply_party(count=0x01, op=0x7D, val=0x61)
06: cond = (cond >= 0x61)
07: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x02, op=0x7D, val=0x61)
08: party_effect(sel=0x01, 31 04 A0 86 01)
09: apply_party_masked(count=0x01, set=0x7D, and=0x9E, or=0x00)
10: apply_party(count=0x02, op=0x7D, val=0x61)
11: cond = (cond >= 0x61)
12: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x03, op=0x7D, val=0x61)
13: party_effect(sel=0x02, 31 04 A0 86 01)
14: apply_party_masked(count=0x02, set=0x7D, and=0x9E, or=0x00)
15: apply_party(count=0x03, op=0x7D, val=0x61)
16: cond = (cond >= 0x61)
17: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x04, op=0x7D, val=0x61)
18: party_effect(sel=0x03, 31 04 A0 86 01)
19: apply_party_masked(count=0x03, set=0x7D, and=0x9E, or=0x00)
20: apply_party(count=0x04, op=0x7D, val=0x61)
21: cond = (cond >= 0x61)
22: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x05, op=0x7D, val=0x61)
23: party_effect(sel=0x04, 31 04 A0 86 01)
24: apply_party_masked(count=0x04, set=0x7D, and=0x9E, or=0x00)
25: apply_party(count=0x05, op=0x7D, val=0x61)
26: cond = (cond >= 0x61)
27: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x06, op=0x7D, val=0x61)
28: party_effect(sel=0x05, 31 04 A0 86 01)
29: apply_party_masked(count=0x05, set=0x7D, and=0x9E, or=0x00)
30: apply_party(count=0x06, op=0x7D, val=0x61)
31: cond = (cond >= 0x61)
32: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x07, op=0x7D, val=0x61)
33: party_effect(sel=0x06, 31 04 A0 86 01)
34: apply_party_masked(count=0x06, set=0x7D, and=0x9E, or=0x00)
35: apply_party(count=0x07, op=0x7D, val=0x61)
36: cond = (cond >= 0x61)
37: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x08, op=0x7D, val=0x61)
38: party_effect(sel=0x07, 31 04 A0 86 01)
39: apply_party_masked(count=0x07, set=0x7D, and=0x9E, or=0x00)
40: apply_party(count=0x08, op=0x7D, val=0x61)
41: cond = (cond >= 0x61)
42: if not cond: skip_tokens(2)
    # skip -> wait_key()
43: party_effect(sel=0x08, 31 04 A0 86 01)
44: apply_party_masked(count=0x08, set=0x7D, and=0x9E, or=0x00)
45: wait_key()
46: clear_current_tile_event_flag()
47: show_text_block(str[18] "The Bishop of Red Battle is locked in / a cage.")
48: wait_key()
49: end_script()
```

**Event 18** — triggers: (0,2)/ANY_DIR, (0,13)/ANY_DIR, (2,0)/ANY_DIR, (2,15)/ANY_DIR, (5,10)/ANY_DIR, (7,9)/ANY_DIR, (7,10)/ANY_DIR, (12,4)/ANY_DIR, (12,8)/ANY_DIR, (13,0)/ANY_DIR, (13,15)/ANY_DIR, (15,2)/ANY_DIR, (15,13)/ANY_DIR

```hex
2b 01 12 69 69 69 69 69 69 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (1,1)/ANY_DIR, (1,8)/ANY_DIR, (1,9)/ANY_DIR, (1,14)/ANY_DIR, (2,8)/ANY_DIR, (2,9)/ANY_DIR, (13,5)/ANY_DIR, (14,1)/ANY_DIR, (14,14)/ANY_DIR

```hex
2b 01 12 69 69 69 69 69 69 69 69 69 69 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=69 69 69 69 69 69 69 69 69 69, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 20** — triggers: (9,5)/DIR_S?

```hex
0b 0d 00 01 14 0e e2
```

```
00: service_sign(idx=0x0D -> sign 5 [5.anm], pos=0x00)
01: show_text_basic(str[20] "Here's the joke of the day:")
02: exec_selector(0xE2)  # special_226
```

**Event 21** — triggers: (12,12)/ANY_DIR

```hex
2b 01 12 0e 0e 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=0E 0E 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 22** — triggers: (12,14)/ANY_DIR

```hex
2b 01 12 1d 1d 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=1D 1D 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 23** — triggers: (10,12)/ANY_DIR

```hex
2b 01 12 2e 2e 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=2E 2E 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 24** — triggers: (11,14)/ANY_DIR

```hex
2b 01 12 3d 3d 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=3D 3D 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 25** — triggers: (9,12)/ANY_DIR

```hex
2b 01 12 44 44 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=44 44 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 26** — triggers: (10,14)/ANY_DIR

```hex
2b 01 12 47 47 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=47 47 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 27** — triggers: (8,12)/ANY_DIR

```hex
2b 01 12 52 52 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=52 52 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 28** — triggers: (9,14)/ANY_DIR

```hex
2b 01 12 5a 5a 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=5A 5A 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 29** — triggers: (7,12)/ANY_DIR

```hex
2b 01 12 5c 5c 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=5C 5C 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 30** — triggers: (8,14)/ANY_DIR

```hex
2b 01 12 5d 5d 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=5D 5D 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 31** — triggers: (6,12)/ANY_DIR

```hex
2b 01 12 5f 5f 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=5F 5F 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 32** — triggers: (7,14)/ANY_DIR

```hex
2b 01 12 6d 6d 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=6D 6D 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 33** — triggers: (5,12)/ANY_DIR

```hex
2b 01 12 71 71 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=71 71 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 34** — triggers: (6,14)/ANY_DIR

```hex
2b 01 12 73 73 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=73 73 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 35** — triggers: (4,12)/ANY_DIR

```hex
2b 01 12 7e 7e 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=7E 7E 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 36** — triggers: (5,14)/ANY_DIR

```hex
2b 01 12 7a 7a 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=7A 7A 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 37** — triggers: (3,12)/ANY_DIR

```hex
2b 01 12 8a 8a 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=8A 8A 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 38** — triggers: (4,14)/ANY_DIR

```hex
2b 01 12 8c 8c 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=8C 8C 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 39** — triggers: (3,14)/ANY_DIR

```hex
2b 01 12 87 87 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=87 87 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 40** — triggers: (12,11)/ANY_DIR

```hex
2b 01 12 9d 9d 00 00 00 00 00 00 00 00 00 00 0e fa
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0xFA)
01: encounter_setup(monsters=9D 9D 00 00 00 00 00 00 00 00, flags=00 00)
02: exec_selector(0xFA)
```

**Event 41** — triggers: (2,13)/0x90

```hex
0e fb
```

```
00: exec_selector(0xFB)
```

## String table

- `[00]` <EMPTY>
- `[01]` Prison
- `[02]` Red Room
- `[03]` Guard Tower
- `[04]` The Zoo
- `[05]` Zookeeper
- `[06]` Throne Room
- `[07]` No Entry
- `[08]` Slayer's Palace
- `[09]` Grand Hall
- `[10]` Stairs to Dungeon. Descend (y/n)?
- `[11]` Castle Exit. Leave (y/n)?
- `[12]` You win the Booby Prize!
- `[13]` Food is stacked to the ceiling. Take / some (y/n)?
- `[14]` You have found an N-19 Capitor.
- `[15]` *** Backpacks Full ***
- `[16]` E
- `[17]` T
- `[18]` The Bishop of Red Battle is locked in / a cage.
- `[19]` The Bishop of Red Battle is freed / from imprisonment by your red key / (+8000 exp). He grants all red / triple crown winners 100,000 exp.
- `[20]` Here's the joke of the day:
- `[21]` Lord Slayer will only bestow a / quest unto a Crusader.
- `[22]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
