# Location 47 — Woodhaven Dungeon Level 1

- **event.dat** offset `0x00E990`, length **896** bytes
- **Map:** map screen **47**; Woodhaven Dungeon Level 1
- **Record kind:** `standard`
- **Triggers:** 26; **script segments:** 13; **strings:** 19

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,6) | `0x06` | **8** | ANY_DIR |
| (0,15) | `0x0F` | **7** | DIR_N? |
| (1,7) | `0x17` | **3** | 0x60 |
| (2,9) | `0x29` | **8** | ANY_DIR |
| (3,6) | `0x36` | **9** | DIR_N? |
| (4,1) | `0x41` | **8** | ANY_DIR |
| (4,4) | `0x44` | **4** | 0x60 |
| (6,0) | `0x60` | **11** | ENTER |
| (6,3) | `0x63` | **10** | DIR_N? |
| (6,4) | `0x64` | **8** | ANY_DIR |
| (7,0) | `0x70` | **5** | ENTER |
| (7,1) | `0x71` | **8** | ANY_DIR |
| (7,9) | `0x79` | **8** | ANY_DIR |
| (8,13) | `0x8D` | **8** | ANY_DIR |
| (9,1) | `0x91` | **6** | 0x30 |
| (9,12) | `0x9C` | **8** | ANY_DIR |
| (10,8) | `0xA8` | **8** | ANY_DIR |
| (12,15) | `0xCF` | **8** | ANY_DIR |
| (13,7) | `0xD7` | **11** | 0x30 |
| (13,8) | `0xD8` | **11** | 0x60 |
| (13,13) | `0xDD` | **2** | DIR_N? |
| (14,5) | `0xE5` | **8** | ANY_DIR |
| (14,8) | `0xE8` | **11** | DIR_SPECIAL |
| (15,0) | `0xF0` | **1** | ALWAYS |
| (15,6) | `0xF6` | **11** | DIR_SPECIAL |
| (15,8) | `0xF8` | **11** | DIR_SPECIAL |

## Events

**Event 01** — triggers: (15,0)/ALWAYS

```hex
01 01 09 11 01 0c 38 9b 0f
```

```
00: show_text_basic(str[1] "Stairs leading up. Climb them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x38, 0x9B)
04: end_script()
```

**Event 02** — triggers: (13,13)/DIR_N?

```hex
01 02 09 11 01 0c 30 f7 0f
```

```
00: show_text_basic(str[2] "Stairs leading down. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x30, 0xF7)
04: end_script()
```

**Event 03** — triggers: (1,7)/0x60

```hex
2d 25 00 11 04 01 06 07 0d 09 0c 2f f0 02 03 19 01 eb 00 00 10 01 01 10 07 14
```

```
00: cond = check_member_attr(fields=0x25, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[3] "You have found a Sapphire Pin!")
02: show_text_basic(str[6] "The sign said NO ROBBERS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2F, 0xF0)
06: show_text_block(str[3] "You have found a Sapphire Pin!")
07: add_party_entity(0x01, f3a=0xEB, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[16] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 04** — triggers: (4,4)/0x60

```hex
2d 23 00 11 04 01 07 07 0d 09 0c 2f f0 02 04 19 01 f7 00 00 10 01 01 10 07 14
```

```
00: cond = check_member_attr(fields=0x23, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[4] "You have found an Onyx Effigy!")
02: show_text_basic(str[7] "The sign said NO CLERICS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2F, 0xF0)
06: show_text_block(str[4] "You have found an Onyx Effigy!")
07: add_party_entity(0x01, f3a=0xF7, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[16] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 05** — triggers: (7,0)/ENTER

```hex
2d a1 00 11 04 01 0f 07 0d 09 0c 2f e0 17 3d 00 10 06 01 05 07 1c 05 1f 80 3c 02 00 00 00 1a 3d 01 14 02 11 29
```

```
00: cond = check_member_attr(fields=0xA1, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> cond = load_var8(group=0x3D, index=0x00)
02: show_text_basic(str[15] "The sign said NO ELVES!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2F, 0xE0)
06: cond = load_var8(group=0x3D, index=0x00)
07: if cond: skip_tokens(6)
    # skip -> show_text_block(str[17] "Sorry, benefits are given only once / per moon phase.")
08: show_text_basic(str[5] "A sudden puff of smoke energizes you.")
09: wait_for_space()
10: op_1c_engine_query_to_cond(0x05)
11: party_effect(sel=0x80, 3C 02 00 00 00)
12: store_var8(group=0x3D, value=0x01)
13: clear_current_tile_event_flag()
14: show_text_block(str[17] "Sorry, benefits are given only once / per moon phase.")
15: set_abort_and_exit()
```

**Event 06** — triggers: (9,1)/0x30

```hex
0b 03 00 02 08 0a 11 01 0c 31 f0 0f
```

```
00: set_service_context(str[3] "You have found a Sapphire Pin!", mode=0x00)
01: show_text_block(str[8] ""There's a secret passage to / Pinehurst Dungeon here. If you / haven't ")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x31, 0xF0)
05: end_script()
```

**Event 07** — triggers: (0,15)/DIR_N?

```hex
02 09 09 11 05 01 0a 26 20 09 12 01 05 00 00 11 01 1f 09 10 01 03 00 00 0f
```

```
00: show_text_block(str[9] "A sign above a well reads: Ogre Ale. / Strength that will lighten your h")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_basic(str[10] "Who will drink (1-8)?")
04: selected = select_party_member()
05: party_effect_b(sel=0x09, 12 01 05 00 00)
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: party_effect(sel=0x09, 10 01 03 00 00)
08: end_script()
```

**Event 08** — triggers: (0,6)/ANY_DIR, (2,9)/ANY_DIR, (4,1)/ANY_DIR, (6,4)/ANY_DIR, (7,1)/ANY_DIR, (7,9)/ANY_DIR, (8,13)/ANY_DIR, (9,12)/ANY_DIR, (10,8)/ANY_DIR, (12,15)/ANY_DIR, (14,5)/ANY_DIR

```hex
02 0b 07 31 00 14 00 0f
```

```
00: show_text_block(str[11] "Darts fly and hit you from behind!")
01: wait_for_space()
02: for_party(mask=0x00): call(0x14,0x00)
03: end_script()
```

**Event 09** — triggers: (3,6)/DIR_N?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "No Robbers!")
```

**Event 10** — triggers: (6,3)/DIR_N?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "No Clerics!")
```

**Event 11** — triggers: (6,0)/ENTER, (13,7)/0x30, (13,8)/0x60, (14,8)/DIR_SPECIAL, (15,6)/DIR_SPECIAL, (15,8)/DIR_SPECIAL

```hex
04 0e
```

```
00: show_text_above_door(str[14] "No Elves!")
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs leading up. Climb them (y/n)?
- `[02]` Stairs leading down. Descend (y/n)?
- `[03]` You have found a Sapphire Pin!
- `[04]` You have found an Onyx Effigy!
- `[05]` A sudden puff of smoke energizes you.
- `[06]` The sign said NO ROBBERS!
- `[07]` The sign said NO CLERICS!
- `[08]` "There's a secret passage to / Pinehurst Dungeon here. If you / haven't been, you really must go." / Take his advice (y/n)?
- `[09]` A sign above a well reads: Ogre Ale. / Strength that will lighten your head / to all who drink. Taste it (y/n)?
- `[10]` Who will drink (1-8)?
- `[11]` Darts fly and hit you from behind!
- `[12]` No Robbers!
- `[13]` No Clerics!
- `[14]` No Elves!
- `[15]` The sign said NO ELVES!
- `[16]` *** Backpacks Full ***
- `[17]` Sorry, benefits are given only once / per moon phase.
- `[18]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
