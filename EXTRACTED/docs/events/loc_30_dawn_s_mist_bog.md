# Location 30 — Dawn's Mist Bog

- **event.dat** offset `0x009E98`, length **1445** bytes
- **Map:** map screen **30**; Dawn's Mist Bog
- **Record kind:** `standard`
- **Triggers:** 50; **script segments:** 32; **strings:** 28

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **11** | 0x30 |
| (0,7) | `0x07` | **1** | DIR_S? |
| (0,8) | `0x08` | **1** | DIR_S? |
| (0,9) | `0x09` | **1** | DIR_S? |
| (0,14) | `0x0E` | **25** | DIR_W? |
| (1,1) | `0x11` | **20** | DIR_E? |
| (1,7) | `0x17` | **11** | 0xB0 |
| (2,5) | `0x25` | **11** | 0xB0 |
| (3,0) | `0x30` | **11** | 0xB0 |
| (3,1) | `0x31` | **21** | DIR_E? |
| (3,5) | `0x35` | **11** | 0xB0 |
| (3,11) | `0x3B` | **3** | DIR_N?+DIR_E? |
| (3,15) | `0x3F` | **11** | 0xE0 |
| (4,7) | `0x47` | **11** | 0xB0 |
| (4,9) | `0x49` | **11** | 0xE0 |
| (5,3) | `0x53` | **11** | 0x70 |
| (6,4) | `0x64` | **11** | 0xD0 |
| (6,8) | `0x68` | **10** | DIR_N? |
| (6,11) | `0x6B` | **11** | 0xD0 |
| (6,13) | `0x6D` | **11** | 0xD0 |
| (6,15) | `0x6F` | **11** | 0xE0 |
| (7,0) | `0x70` | **11** | 0xB0 |
| (7,12) | `0x7C` | **8** | 0x30 |
| (8,1) | `0x81` | **22** | DIR_E? |
| (8,2) | `0x82` | **14** | ANY_DIR |
| (8,13) | `0x8D` | **16** | ANY_DIR |
| (8,14) | `0x8E` | **26** | DIR_W? |
| (9,8) | `0x98` | **4** | DIR_S? |
| (9,15) | `0x9F` | **11** | 0xE0 |
| (10,8) | `0xA8` | **9** | DIR_S? |
| (10,13) | `0xAD` | **17** | ANY_DIR |
| (10,14) | `0xAE` | **27** | DIR_W? |
| (11,0) | `0xB0` | **11** | 0xB0 |
| (11,1) | `0xB1` | **23** | DIR_E? |
| (11,2) | `0xB2` | **7** | ANY_DIR |
| (11,4) | `0xB4` | **2** | 0xE0 |
| (11,15) | `0xBF` | **11** | 0xE0 |
| (12,7) | `0xC7` | **13** | ANY_DIR |
| (12,9) | `0xC9` | **13** | ANY_DIR |
| (12,11) | `0xCB` | **6** | ANY_DIR |
| (12,13) | `0xCD` | **18** | ANY_DIR |
| (12,14) | `0xCE` | **28** | DIR_W? |
| (13,15) | `0xDF` | **12** | 0xE0 |
| (14,1) | `0xE1` | **24** | DIR_E? |
| (14,2) | `0xE2` | **15** | ANY_DIR |
| (14,13) | `0xED` | **19** | ANY_DIR |
| (14,14) | `0xEE` | **29** | DIR_W? |
| (15,0) | `0xF0` | **11** | 0xB0 |
| (15,10) | `0xFA` | **5** | 0xD0 |
| (15,15) | `0xFF` | **11** | DIR_N?+DIR_E? |

## Events

**Event 01** — triggers: (0,7)/DIR_S?, (0,8)/DIR_S?, (0,9)/DIR_S?

```hex
16 00 df 11 02 01 01 29 02 02 09 11 01 0c 27 73 0f
```

```
00: cond = check_monster_present(0x00, 0xDF)
01: if not cond: skip_tokens(2)
    # skip -> show_text_block(str[2] "Stairs lead to the Quagmire of Doom. / Climb them (y/n)?")
02: show_text_basic(str[1] "The Orb denies you exit!")
03: set_abort_and_exit()
04: show_text_block(str[2] "Stairs lead to the Quagmire of Doom. / Climb them (y/n)?")
05: cond = prompt_yes_no()
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: map_transition(0x27, 0x73)
08: end_script()
```

**Event 02** — triggers: (11,4)/0xE0

```hex
17 15 00 11 01 14 0e 6b
```

```
00: cond = load_var8(group=0x15, index=0x00)
01: if not cond: skip_tokens(1)
    # skip -> exec_selector(0x6B)
02: clear_current_tile_event_flag()
03: exec_selector(0x6B)
```

**Event 03** — triggers: (3,11)/DIR_N?+DIR_E?

```hex
16 00 df 11 02 01 04 29 02 05 09 11 01 0c 1a 80 0f
```

```
00: cond = check_monster_present(0x00, 0xDF)
01: if not cond: skip_tokens(2)
    # skip -> show_text_block(str[5] "A brightly glowing portal leads to / Murray's Resort Isle. Take it (y/n)")
02: show_text_basic(str[4] "The Orb denies you exit!")
03: set_abort_and_exit()
04: show_text_block(str[5] "A brightly glowing portal leads to / Murray's Resort Isle. Take it (y/n)")
05: cond = prompt_yes_no()
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: map_transition(0x1A, 0x80)
08: end_script()
```

**Event 04** — triggers: (9,8)/DIR_S?

```hex
2b 01 12 f9 ac ac 00 00 00 00 00 00 00 00 00 18 00 7b df 20 2d 06 05 10 01 14 18 00 75 fe 01 02 1a 07 14
```

```
00: skip_tokens(1)
    # skip -> apply_party_masked(count=0x00, set=0x7B, and=0xDF, or=0x20)
01: encounter_setup(monsters=F9 AC AC 00 00 00 00 00 00 00, flags=00 00)
02: apply_party_masked(count=0x00, set=0x7B, and=0xDF, or=0x20)
03: cond = check_member_attr(fields=0x06, value=0x05)
04: if cond: skip_tokens(1)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
05: clear_current_tile_event_flag()
06: apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
07: show_text_block(str[26] "You feel much better after the / successful assassination of that / wick")
08: wait_for_space()
09: clear_current_tile_event_flag()
```

**Event 05** — triggers: (15,10)/0xD0

```hex
16 00 df 11 03 02 08 07 0f 02 06 09 10 01 0f 0e 73
```

```
00: cond = check_monster_present(0x00, 0xDF)
01: if not cond: skip_tokens(3)
    # skip -> show_text_block(str[6] "King Kalohn's powerful Element Orb / rests majestically upon a decorativ")
02: show_text_block(str[8] "You see an empty pedestal.")
03: wait_for_space()
04: end_script()
05: show_text_block(str[6] "King Kalohn's powerful Element Orb / rests majestically upon a decorativ")
06: cond = prompt_yes_no()
07: if cond: skip_tokens(1)
    # skip -> exec_selector(0x73)
08: end_script()
09: exec_selector(0x73)
```

**Event 06** — triggers: (12,11)/ANY_DIR

```hex
2b 01 12 f8 f8 f8 f8 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=F8 F8 F8 F8 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (11,2)/ANY_DIR

```hex
2b 03 17 15 00 10 01 0e 6c 14
```

```
00: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
01: cond = load_var8(group=0x15, index=0x00)
02: if cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: exec_selector(0x6C)
04: clear_current_tile_event_flag()
```

**Event 08** — triggers: (7,12)/0x30

```hex
02 0a 09 11 22 15 01 15 00 1b 32 10 01 1f 01 15 01 0a 00 00 15 02 15 00 1b 32 10 01 1f 02 15 01 0a 00 00 15 03 15 00 1b 32 10 01 1f 03 15 01 0a 00 00 15 04 15 00 1b 32 10 01 1f 04 15 01 0a 00 00 15 05 15 00 1b 32 10 01 1f 05 15 01 0a 00 00 15 06 15 00 1b 32 10 01 1f 06 15 01 0a 00 00 15 07 15 00 1b 32 10 01 1f 07 15 01 0a 00 00 15 08 15 00 1b 32 10 01 1f 08 15 01 0a 00 00 02 0b 07 14
```

```
00: show_text_block(str[10] "The room is blanketed with a multitude / of various lucky charms. / Roll")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(34)
    # skip -> clear_current_tile_event_flag()
03: apply_party(count=0x01, op=0x15, val=0x00)
04: cond = (cond >= 0x32)
05: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x15, val=0x00)
06: party_effect(sel=0x01, 15 01 0A 00 00)
07: apply_party(count=0x02, op=0x15, val=0x00)
08: cond = (cond >= 0x32)
09: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x15, val=0x00)
10: party_effect(sel=0x02, 15 01 0A 00 00)
11: apply_party(count=0x03, op=0x15, val=0x00)
12: cond = (cond >= 0x32)
13: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x15, val=0x00)
14: party_effect(sel=0x03, 15 01 0A 00 00)
15: apply_party(count=0x04, op=0x15, val=0x00)
16: cond = (cond >= 0x32)
17: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x15, val=0x00)
18: party_effect(sel=0x04, 15 01 0A 00 00)
19: apply_party(count=0x05, op=0x15, val=0x00)
20: cond = (cond >= 0x32)
21: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x15, val=0x00)
22: party_effect(sel=0x05, 15 01 0A 00 00)
23: apply_party(count=0x06, op=0x15, val=0x00)
24: cond = (cond >= 0x32)
25: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x15, val=0x00)
26: party_effect(sel=0x06, 15 01 0A 00 00)
27: apply_party(count=0x07, op=0x15, val=0x00)
28: cond = (cond >= 0x32)
29: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x15, val=0x00)
30: party_effect(sel=0x07, 15 01 0A 00 00)
31: apply_party(count=0x08, op=0x15, val=0x00)
32: cond = (cond >= 0x32)
33: if cond: skip_tokens(1)
    # skip -> show_text_block(str[11] "More luck to those who are worthy.")
34: party_effect(sel=0x08, 15 01 0A 00 00)
35: show_text_block(str[11] "More luck to those who are worthy.")
36: wait_for_space()
37: clear_current_tile_event_flag()
```

**Event 09** — triggers: (10,8)/DIR_S?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Dawn's Throne Room")
```

**Event 10** — triggers: (6,8)/DIR_N?

```hex
05 0d
```

```
00: show_text_popup_style_a(str[13] "Dawn's Mist Bog - / where monsters come / to relax.")
```

**Event 11** — triggers: (0,0)/0x30, (1,7)/0xB0, (2,5)/0xB0, (3,0)/0xB0, (3,5)/0xB0, (3,15)/0xE0, (4,7)/0xB0, (4,9)/0xE0, (5,3)/0x70, (6,4)/0xD0, (6,11)/0xD0, (6,13)/0xD0, (6,15)/0xE0, (7,0)/0xB0, (9,15)/0xE0, (11,0)/0xB0, (11,15)/0xE0, (15,0)/0xB0, (15,15)/DIR_N?+DIR_E?

```hex
01 0e 29
```

```
00: show_text_basic(str[14] "Rubbish lines the walls.")
01: set_abort_and_exit()
```

**Event 12** — triggers: (13,15)/0xE0

```hex
01 0e 07 16 00 d9 10 05 02 0f 19 01 d9 00 00 10 01 01 03 07 0f
```

```
00: show_text_basic(str[14] "Rubbish lines the walls.")
01: wait_for_space()
02: cond = check_monster_present(0x00, 0xD9)
03: if cond: skip_tokens(5)
    # skip -> end_script()
04: show_text_block(str[15] "You have found a Monster Tome.")
05: add_party_entity(0x01, f3a=0xD9, f40=0x00, f46=0x00)
06: if cond: skip_tokens(1)
    # skip -> wait_for_space()
07: show_text_basic(str[3] "*** Backpacks Full ***")
08: wait_for_space()
09: end_script()
```

**Event 13** — triggers: (12,7)/ANY_DIR, (12,9)/ANY_DIR

```hex
2b 01 12 10 10 10 10 10 10 10 10 10 10 10 be 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=10 10 10 10 10 10 10 10 10 10, flags=10 BE)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (8,2)/ANY_DIR

```hex
2b 01 12 5f 5f 5f 5f 5f 5f 5f 5f 5f 5f 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=5F 5F 5F 5F 5F 5F 5F 5F 5F 5F, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (14,2)/ANY_DIR

```hex
2b 01 12 44 44 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=44 44 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (8,13)/ANY_DIR

```hex
2b 01 12 f6 f6 f6 f6 f6 f6 f6 f6 f6 f6 f6 03 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=F6 F6 F6 F6 F6 F6 F6 F6 F6 F6, flags=F6 03)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (10,13)/ANY_DIR

```hex
2b 01 12 73 73 73 73 73 73 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=73 73 73 73 73 73 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (12,13)/ANY_DIR

```hex
2b 01 12 85 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=85 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (14,13)/ANY_DIR

```hex
2b 01 12 72 72 72 72 72 72 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=72 72 72 72 72 72 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 20** — triggers: (1,1)/DIR_E?

```hex
04 10
```

```
00: show_text_above_door(str[16] "Playroom")
```

**Event 21** — triggers: (3,1)/DIR_E?

```hex
04 11
```

```
00: show_text_above_door(str[17] "Lounge")
```

**Event 22** — triggers: (8,1)/DIR_E?

```hex
04 12
```

```
00: show_text_above_door(str[18] "The Blue Room")
```

**Event 23** — triggers: (11,1)/DIR_E?

```hex
04 13
```

```
00: show_text_above_door(str[19] "Target Practice")
```

**Event 24** — triggers: (14,1)/DIR_E?

```hex
04 14
```

```
00: show_text_above_door(str[20] "Employees Only!")
```

**Event 25** — triggers: (0,14)/DIR_W?

```hex
04 15
```

```
00: show_text_above_door(str[21] "Spa")
```

**Event 26** — triggers: (8,14)/DIR_W?

```hex
04 16
```

```
00: show_text_above_door(str[22] "Top Dog Kennel")
```

**Event 27** — triggers: (10,14)/DIR_W?

```hex
04 17
```

```
00: show_text_above_door(str[23] "Weight Room")
```

**Event 28** — triggers: (12,14)/DIR_W?

```hex
04 18
```

```
00: show_text_above_door(str[24] "Steam Room")
```

**Event 29** — triggers: (14,14)/DIR_W?

```hex
04 19
```

```
00: show_text_above_door(str[25] "Cafeteria")
```

## String table

- `[00]` <EMPTY>
- `[01]` The Orb denies you exit!
- `[02]` Stairs lead to the Quagmire of Doom. / Climb them (y/n)?
- `[03]` *** Backpacks Full ***
- `[04]` The Orb denies you exit!
- `[05]` A brightly glowing portal leads to / Murray's Resort Isle. Take it (y/n)?
- `[06]` King Kalohn's powerful Element Orb / rests majestically upon a decorative / pedestal. Do you have what it / takes to get it (y/n)?
- `[07]` Y
- `[08]` You see an empty pedestal.
- `[09]` A
- `[10]` The room is blanketed with a multitude / of various lucky charms. / Roll in them (y/n)?
- `[11]` More luck to those who are worthy.
- `[12]` Dawn's Throne Room
- `[13]` Dawn's Mist Bog - / where monsters come / to relax.
- `[14]` Rubbish lines the walls.
- `[15]` You have found a Monster Tome.
- `[16]` Playroom
- `[17]` Lounge
- `[18]` The Blue Room
- `[19]` Target Practice
- `[20]` Employees Only!
- `[21]` Spa
- `[22]` Top Dog Kennel
- `[23]` Weight Room
- `[24]` Steam Room
- `[25]` Cafeteria
- `[26]` You feel much better after the / successful assassination of that / wicked woman from hell, Dawn. Return / to Mount Farview for your reward.
- `[27]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
