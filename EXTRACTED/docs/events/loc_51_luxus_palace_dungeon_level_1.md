# Location 51 — Luxus Palace Dungeon Level 1

- **event.dat** offset `0x00FC6C`, length **1243** bytes
- **Map:** map screen **51**; Luxus Palace Dungeon Level 1
- **Record kind:** `standard`
- **Triggers:** 57; **script segments:** 18; **strings:** 24

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **5** | DIR_W? |
| (0,1) | `0x01` | **10** | DIR_W? |
| (0,2) | `0x02` | **8** | ANY_DIR |
| (0,5) | `0x05` | **6** | DIR_W? |
| (0,7) | `0x07` | **16** | DIR_W? |
| (0,15) | `0x0F` | **2** | DIR_E? |
| (1,2) | `0x12` | **12** | DIR_S? |
| (3,0) | `0x30` | **15** | DIR_S? |
| (3,2) | `0x32` | **13** | DIR_S? |
| (3,4) | `0x34` | **15** | 0xA0 |
| (3,6) | `0x36` | **15** | 0xA0 |
| (3,8) | `0x38` | **14** | DIR_S? |
| (3,9) | `0x39` | **14** | DIR_S? |
| (3,10) | `0x3A` | **14** | 0xA0 |
| (3,11) | `0x3B` | **14** | 0xA0 |
| (3,12) | `0x3C` | **14** | 0xA0 |
| (3,13) | `0x3D` | **14** | 0xA0 |
| (3,14) | `0x3E` | **14** | 0xA0 |
| (3,15) | `0x3F` | **14** | 0xA0 |
| (5,7) | `0x57` | **14** | DIR_E? |
| (5,13) | `0x5D` | **3** | DIR_S? |
| (6,13) | `0x6D` | **16** | 0x50 |
| (7,7) | `0x77` | **14** | DIR_E? |
| (8,0) | `0x80` | **14** | 0xA0 |
| (8,1) | `0x81` | **14** | DIR_N? |
| (8,2) | `0x82` | **14** | DIR_S? |
| (8,3) | `0x83` | **14** | DIR_N? |
| (8,4) | `0x84` | **14** | DIR_S? |
| (8,5) | `0x85` | **14** | DIR_N? |
| (8,6) | `0x86` | **14** | DIR_S? |
| (8,10) | `0x8A` | **11** | DIR_W? |
| (8,13) | `0x8D` | **16** | 0x50 |
| (9,7) | `0x97` | **12** | DIR_E? |
| (9,11) | `0x9B` | **1** | DIR_N?+DIR_E? |
| (10,0) | `0xA0` | **8** | ANY_DIR |
| (10,1) | `0xA1` | **12** | DIR_W? |
| (11,7) | `0xB7` | **12** | DIR_E? |
| (11,8) | `0xB8` | **8** | ANY_DIR |
| (11,13) | `0xBD` | **16** | 0x50 |
| (12,0) | `0xC0` | **16** | DIR_S? |
| (12,1) | `0xC1` | **15** | DIR_N? |
| (12,2) | `0xC2` | **13** | DIR_N? |
| (12,3) | `0xC3` | **15** | DIR_N? |
| (12,4) | `0xC4` | **15** | DIR_S? |
| (12,5) | `0xC5` | **15** | DIR_N? |
| (12,6) | `0xC6` | **15** | DIR_S? |
| (12,13) | `0xCD` | **13** | DIR_N? |
| (13,7) | `0xD7` | **15** | DIR_E? |
| (13,13) | `0xDD` | **16** | DIR_E? |
| (14,10) | `0xEA` | **11** | DIR_E? |
| (14,11) | `0xEB` | **7** | DIR_E? |
| (14,13) | `0xED` | **16** | DIR_W? |
| (15,0) | `0xF0` | **4** | DIR_W? |
| (15,2) | `0xF2` | **9** | DIR_W? |
| (15,7) | `0xF7` | **15** | 0x50 |
| (15,10) | `0xFA` | **8** | ANY_DIR |
| (15,13) | `0xFD` | **16** | DIR_E? |

## Events

**Event 01** — triggers: (9,11)/DIR_N?+DIR_E?

```hex
01 01 09 11 01 0c 3a 00 0f
```

```
00: show_text_basic(str[1] "Stairs going up. Ascend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x3A, 0x00)
04: end_script()
```

**Event 02** — triggers: (0,15)/DIR_E?

```hex
0b 17 00 02 02 0a 11 01 0c 2d f8 0f
```

```
00: set_service_context(str[23] "<EMPTY>", mode=0x00)
01: show_text_block(str[2] ""What's a nice party like you doing / in a drab dungeon like this? Hills")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x2D, 0xF8)
05: end_script()
```

**Event 03** — triggers: (5,13)/DIR_S?

```hex
01 03 09 11 01 0c 34 2a 0f
```

```
00: show_text_basic(str[3] "Stairs down. Take them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x34, 0x2A)
04: end_script()
```

**Event 04** — triggers: (15,0)/DIR_W?

```hex
2d 22 00 11 04 02 04 07 0d 09 0c 33 89 02 07 19 01 f9 00 00 10 01 01 14 07 14
```

```
00: cond = check_member_attr(fields=0x22, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[7] "You have found a Topaz Shard!")
02: show_text_block(str[4] "Shazam! You're sucked away.")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x33, 0x89)
06: show_text_block(str[7] "You have found a Topaz Shard!")
07: add_party_entity(0x01, f3a=0xF9, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[20] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 05** — triggers: (0,0)/DIR_W?

```hex
2d 24 00 11 04 01 05 07 0d 09 0c 33 89 02 06 19 01 ef 00 00 10 01 01 14 07 14
```

```
00: cond = check_member_attr(fields=0x24, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[6] "You have found an Amber Skull!")
02: show_text_basic(str[5] "You are teleported elsewhere.")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x33, 0x89)
06: show_text_block(str[6] "You have found an Amber Skull!")
07: add_party_entity(0x01, f3a=0xEF, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[20] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 06** — triggers: (0,5)/DIR_W?

```hex
02 08 09 11 05 02 09 26 20 09 13 01 05 00 00 11 01 1f 09 44 01 03 00 00 0f
```

```
00: show_text_block(str[8] "Steam rises from a magical well. / Will you sample the water (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_block(str[9] "Who will drink (1-8)?")
04: selected = select_party_member()
05: party_effect_b(sel=0x09, 13 01 05 00 00)
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: party_effect(sel=0x09, 44 01 03 00 00)
08: end_script()
```

**Event 07** — triggers: (14,11)/DIR_E?

```hex
2d a2 00 11 04 01 15 07 0d 09 0c 33 89 17 3d 00 10 06 02 0a 07 1c 05 1f 80 3c 02 00 00 00 1a 3d 01 14 02 16 29
```

```
00: cond = check_member_attr(fields=0xA2, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> cond = load_var8(group=0x3D, index=0x00)
02: show_text_basic(str[21] "The sign said NO DWARVES!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x33, 0x89)
06: cond = load_var8(group=0x3D, index=0x00)
07: if cond: skip_tokens(6)
    # skip -> show_text_block(str[22] "Sorry, benefits are given only once / per moon phase.")
08: show_text_block(str[10] "A shower of soft pellets cascades / upon the party. You feel different.")
09: wait_for_space()
10: op_1c_engine_query_to_cond(0x05)
11: party_effect(sel=0x80, 3C 02 00 00 00)
12: store_var8(group=0x3D, value=0x01)
13: clear_current_tile_event_flag()
14: show_text_block(str[22] "Sorry, benefits are given only once / per moon phase.")
15: set_abort_and_exit()
```

**Event 08** — triggers: (0,2)/ANY_DIR, (10,0)/ANY_DIR, (11,8)/ANY_DIR, (15,10)/ANY_DIR

```hex
02 0b 15 01 01 80 10 03 18 01 3e 00 00 18 01 3f 00 00 18 01 40 00 00 15 02 01 80 10 03 18 02 3e 00 00 18 02 3f 00 00 18 02 40 00 00 15 03 01 80 10 03 18 03 3e 00 00 18 03 3f 00 00 18 03 40 00 00 15 04 01 80 10 03 18 04 3e 00 00 18 04 3f 00 00 18 04 40 00 00 15 05 01 80 10 03 18 05 3e 00 00 18 05 3f 00 00 18 05 40 00 00 15 06 01 80 10 03 18 06 3e 00 00 18 06 3f 00 00 18 06 40 00 00 15 07 01 80 10 03 18 07 3e 00 00 18 07 3f 00 00 18 07 40 00 00 15 08 01 80 10 03 18 08 3e 00 00 18 08 3f 00 00 18 08 40 00 00 07 14
```

```
00: show_text_block(str[11] "You have entered an unauthorized area. / Pay the fine!")
01: apply_party(count=0x01, op=0x01, val=0x80)
02: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x02, op=0x01, val=0x80)
03: apply_party_masked(count=0x01, set=0x3E, and=0x00, or=0x00)
04: apply_party_masked(count=0x01, set=0x3F, and=0x00, or=0x00)
05: apply_party_masked(count=0x01, set=0x40, and=0x00, or=0x00)
06: apply_party(count=0x02, op=0x01, val=0x80)
07: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x03, op=0x01, val=0x80)
08: apply_party_masked(count=0x02, set=0x3E, and=0x00, or=0x00)
09: apply_party_masked(count=0x02, set=0x3F, and=0x00, or=0x00)
10: apply_party_masked(count=0x02, set=0x40, and=0x00, or=0x00)
11: apply_party(count=0x03, op=0x01, val=0x80)
12: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x04, op=0x01, val=0x80)
13: apply_party_masked(count=0x03, set=0x3E, and=0x00, or=0x00)
14: apply_party_masked(count=0x03, set=0x3F, and=0x00, or=0x00)
15: apply_party_masked(count=0x03, set=0x40, and=0x00, or=0x00)
16: apply_party(count=0x04, op=0x01, val=0x80)
17: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x05, op=0x01, val=0x80)
18: apply_party_masked(count=0x04, set=0x3E, and=0x00, or=0x00)
19: apply_party_masked(count=0x04, set=0x3F, and=0x00, or=0x00)
20: apply_party_masked(count=0x04, set=0x40, and=0x00, or=0x00)
21: apply_party(count=0x05, op=0x01, val=0x80)
22: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x06, op=0x01, val=0x80)
23: apply_party_masked(count=0x05, set=0x3E, and=0x00, or=0x00)
24: apply_party_masked(count=0x05, set=0x3F, and=0x00, or=0x00)
25: apply_party_masked(count=0x05, set=0x40, and=0x00, or=0x00)
26: apply_party(count=0x06, op=0x01, val=0x80)
27: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x07, op=0x01, val=0x80)
28: apply_party_masked(count=0x06, set=0x3E, and=0x00, or=0x00)
29: apply_party_masked(count=0x06, set=0x3F, and=0x00, or=0x00)
30: apply_party_masked(count=0x06, set=0x40, and=0x00, or=0x00)
31: apply_party(count=0x07, op=0x01, val=0x80)
32: if cond: skip_tokens(3)
    # skip -> apply_party(count=0x08, op=0x01, val=0x80)
33: apply_party_masked(count=0x07, set=0x3E, and=0x00, or=0x00)
34: apply_party_masked(count=0x07, set=0x3F, and=0x00, or=0x00)
35: apply_party_masked(count=0x07, set=0x40, and=0x00, or=0x00)
36: apply_party(count=0x08, op=0x01, val=0x80)
37: if cond: skip_tokens(3)
    # skip -> wait_for_space()
38: apply_party_masked(count=0x08, set=0x3E, and=0x00, or=0x00)
39: apply_party_masked(count=0x08, set=0x3F, and=0x00, or=0x00)
40: apply_party_masked(count=0x08, set=0x40, and=0x00, or=0x00)
41: wait_for_space()
42: clear_current_tile_event_flag()
```

**Event 09** — triggers: (15,2)/DIR_W?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "No Archers!")
```

**Event 10** — triggers: (0,1)/DIR_W?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "No Sorcerers!")
```

**Event 11** — triggers: (8,10)/DIR_W?, (14,10)/DIR_E?

```hex
04 0e
```

```
00: show_text_above_door(str[14] "No Dwarves!")
```

**Event 12** — triggers: (1,2)/DIR_S?, (9,7)/DIR_E?, (10,1)/DIR_W?, (11,7)/DIR_E?

```hex
04 0f
```

```
00: show_text_above_door(str[15] "Guards")
```

**Event 13** — triggers: (3,2)/DIR_S?, (12,2)/DIR_N?, (12,13)/DIR_N?

```hex
04 10
```

```
00: show_text_above_door(str[16] "Isolation")
```

**Event 14** — triggers: (3,8)/DIR_S?, (3,9)/DIR_S?, (3,10)/0xA0, (3,11)/0xA0, (3,12)/0xA0, (3,13)/0xA0, (3,14)/0xA0, (3,15)/0xA0, (5,7)/DIR_E?, (7,7)/DIR_E?, (8,0)/0xA0, (8,1)/DIR_N?, (8,2)/DIR_S?, (8,3)/DIR_N?, (8,4)/DIR_S?, (8,5)/DIR_N?, (8,6)/DIR_S?

```hex
04 11
```

```
00: show_text_above_door(str[17] "Bad Guys")
```

**Event 15** — triggers: (3,0)/DIR_S?, (3,4)/0xA0, (3,6)/0xA0, (12,1)/DIR_N?, (12,3)/DIR_N?, (12,4)/DIR_S?, (12,5)/DIR_N?, (12,6)/DIR_S?, (13,7)/DIR_E?, (15,7)/0x50

```hex
04 12
```

```
00: show_text_above_door(str[18] "Really Bad Guys")
```

**Event 16** — triggers: (0,7)/DIR_W?, (6,13)/0x50, (8,13)/0x50, (11,13)/0x50, (12,0)/DIR_S?, (13,13)/DIR_E?, (14,13)/DIR_W?, (15,13)/DIR_E?

```hex
04 13
```

```
00: show_text_above_door(str[19] "Very Bad Guys")
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs going up. Ascend (y/n)?
- `[02]` "What's a nice party like you doing / in a drab dungeon like this? Hillstone / Dungeon is much more exciting. Follow / me there (Y/n)?"
- `[03]` Stairs down. Take them (y/n)?
- `[04]` Shazam! You're sucked away.
- `[05]` You are teleported elsewhere.
- `[06]` You have found an Amber Skull!
- `[07]` You have found a Topaz Shard!
- `[08]` Steam rises from a magical well. / Will you sample the water (y/n)?
- `[09]` Who will drink (1-8)?
- `[10]` A shower of soft pellets cascades / upon the party. You feel different.
- `[11]` You have entered an unauthorized area. / Pay the fine!
- `[12]` No Archers!
- `[13]` No Sorcerers!
- `[14]` No Dwarves!
- `[15]` Guards
- `[16]` Isolation
- `[17]` Bad Guys
- `[18]` Really Bad Guys
- `[19]` Very Bad Guys
- `[20]` *** Backpacks Full ***
- `[21]` The sign said NO DWARVES!
- `[22]` Sorry, benefits are given only once / per moon phase.
- `[23]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
