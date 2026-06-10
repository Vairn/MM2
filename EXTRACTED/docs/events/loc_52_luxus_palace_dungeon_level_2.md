# Location 52 — Luxus Palace Dungeon Level 2

- **event.dat** offset `0x010147`, length **1440** bytes
- **Map:** map screen **52**; Luxus Palace Dungeon Level 2
- **Record kind:** `standard`
- **Triggers:** 38; **script segments:** 17; **strings:** 24

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **6** | ANY_DIR |
| (0,15) | `0x0F` | **15** | DIR_E? |
| (1,3) | `0x13` | **5** | ANY_DIR |
| (1,10) | `0x1A` | **3** | DIR_S? |
| (1,11) | `0x1B` | **14** | DIR_S? |
| (2,10) | `0x2A` | **13** | DIR_N? |
| (3,2) | `0x32` | **6** | ANY_DIR |
| (3,7) | `0x37` | **6** | ANY_DIR |
| (3,15) | `0x3F` | **6** | ANY_DIR |
| (4,4) | `0x44` | **6** | ANY_DIR |
| (4,7) | `0x47` | **5** | ANY_DIR |
| (4,12) | `0x4C` | **6** | ANY_DIR |
| (7,1) | `0x71` | **5** | ANY_DIR |
| (7,8) | `0x78` | **6** | ANY_DIR |
| (7,14) | `0x7E` | **6** | ANY_DIR |
| (7,15) | `0x7F` | **5** | ANY_DIR |
| (8,4) | `0x84` | **8** | DIR_N? |
| (8,13) | `0x8D` | **5** | ANY_DIR |
| (8,15) | `0x8F` | **5** | ANY_DIR |
| (9,0) | `0x90` | **6** | ANY_DIR |
| (9,8) | `0x98` | **6** | ANY_DIR |
| (10,3) | `0xA3` | **5** | ANY_DIR |
| (10,5) | `0xA5` | **4** | DIR_N? |
| (10,8) | `0xA8` | **3** | DIR_N? |
| (10,10) | `0xAA` | **6** | ANY_DIR |
| (11,5) | `0xB5` | **7** | DIR_N? |
| (11,8) | `0xB8` | **12** | ANY_DIR |
| (11,9) | `0xB9` | **6** | ANY_DIR |
| (12,9) | `0xC9` | **6** | ANY_DIR |
| (12,13) | `0xCD` | **6** | ANY_DIR |
| (13,0) | `0xD0` | **6** | ANY_DIR |
| (13,12) | `0xDC` | **5** | ANY_DIR |
| (13,14) | `0xDE` | **2** | DIR_N? |
| (13,15) | `0xDF` | **10** | DIR_S? |
| (14,3) | `0xE3` | **6** | ANY_DIR |
| (14,14) | `0xEE` | **11** | DIR_N? |
| (14,15) | `0xEF` | **1** | DIR_S? |
| (15,0) | `0xF0` | **9** | 0x90 |

## Events

**Event 01** — triggers: (14,15)/DIR_S?

```hex
06 01
```

```
00: show_text_popup_style_b(str[1] "No / Archers!")
```

**Event 02** — triggers: (13,14)/DIR_N?

```hex
06 02
```

```
00: show_text_popup_style_b(str[2] "No / Sorcerers!")
```

**Event 03** — triggers: (1,10)/DIR_S?, (10,8)/DIR_N?

```hex
06 03
```

```
00: show_text_popup_style_b(str[3] "No / Dwarves!")
```

**Event 04** — triggers: (10,5)/DIR_N?

```hex
06 04
```

```
00: show_text_popup_style_b(str[4] "Fear no / Evil")
```

**Event 05** — triggers: (1,3)/ANY_DIR, (4,7)/ANY_DIR, (7,1)/ANY_DIR, (7,15)/ANY_DIR, (8,13)/ANY_DIR, (8,15)/ANY_DIR, (10,3)/ANY_DIR, (13,12)/ANY_DIR

```hex
06 05 07 0d 09 0c b4 00
```

```
00: show_text_popup_style_b(str[5] "See you / later")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0xB4, 0x00)
```

**Event 06** — triggers: (0,2)/ANY_DIR, (3,2)/ANY_DIR, (3,7)/ANY_DIR, (3,15)/ANY_DIR, (4,4)/ANY_DIR, (4,12)/ANY_DIR, (7,8)/ANY_DIR, (7,14)/ANY_DIR, (9,0)/ANY_DIR, (9,8)/ANY_DIR, (10,10)/ANY_DIR, (11,9)/ANY_DIR, (12,9)/ANY_DIR, (12,13)/ANY_DIR, (13,0)/ANY_DIR, (14,3)/ANY_DIR

```hex
2b 08 1c 64 1b 32 11 04 06 06 07 0d 09 0c b4 00 13 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(8)
    # skip -> clear_current_tile_event_flag()
01: op_1c_engine_query_to_cond(0x64)
02: cond = (cond >= 0x32)
03: if not cond: skip_tokens(4)
    # skip -> encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
04: show_text_popup_style_b(str[6] "Have a / nice day!")
05: wait_for_space()
06: engine_call(0x09)
07: map_transition(0xB4, 0x00)
08: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
09: clear_current_tile_event_flag()
```

**Event 07** — triggers: (11,5)/DIR_N?

```hex
02 07 09 11 01 18 00 25 00 02 0f
```

```
00: show_text_block(str[7] "There is a giant lever on the wall / labeled, "AE." Pull it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x25, and=0x00, or=0x02)
04: end_script()
```

**Event 08** — triggers: (8,4)/DIR_N?

```hex
02 08 09 11 01 18 00 0c 00 01 0f
```

```
00: show_text_block(str[8] "The sign above a magical / fountain reads, "eraweb selam" / Drink from i")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x0C, and=0x00, or=0x01)
04: end_script()
```

**Event 09** — triggers: (15,0)/0x90

```hex
0b 0d 00 02 0a 08 02 09 0a 10 01 0f 0e ce
```

```
00: set_service_context(str[13] "The sign said NO SORCERERS!", mode=0x00)
01: show_text_block(str[10] ""Looking for some help? Hey, I know / that we have all gotten a raw deal")
02: wait_key()
03: show_text_block(str[9] "one million gold, I'll sell you all / the hit points you SHOULD have." /")
04: cond = prompt_yes_no(mode=1)
05: if cond: skip_tokens(1)
    # skip -> exec_selector(0xCE)  # quest_handler_206
06: end_script()
07: exec_selector(0xCE)  # quest_handler_206
```

**Event 10** — triggers: (13,15)/DIR_S?

```hex
2d 22 00 11 04 01 0b 07 0d 09 0c 34 00 02 0c 19 01 fa 00 00 10 01 01 15 07 14
```

```
00: cond = check_member_attr(fields=0x22, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[12] "You have found a Sun Crown!")
02: show_text_basic(str[11] "The sign said NO ARCHERS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x34, 0x00)
06: show_text_block(str[12] "You have found a Sun Crown!")
07: add_party_entity(0x01, f3a=0xFA, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[21] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 11** — triggers: (14,14)/DIR_N?

```hex
2d 24 00 11 04 01 0d 07 0d 09 0c 34 00 02 0e 19 01 f0 00 00 10 01 01 15 07 14
```

```
00: cond = check_member_attr(fields=0x24, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[14] "You have found a Quartz Skull!")
02: show_text_basic(str[13] "The sign said NO SORCERERS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x34, 0x00)
06: show_text_block(str[14] "You have found a Quartz Skull!")
07: add_party_entity(0x01, f3a=0xF0, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[21] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 12** — triggers: (11,8)/ANY_DIR

```hex
2d a2 00 11 04 01 0f 07 0d 09 0c 34 00 17 3e 00 10 06 02 10 07 1c 0c 1f 80 3c 02 00 00 00 1a 3e 01 14 02 16 29
```

```
00: cond = check_member_attr(fields=0xA2, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> cond = load_var8(group=0x3E, index=0x00)
02: show_text_basic(str[15] "The sign said NO DWARVES!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x34, 0x00)
06: cond = load_var8(group=0x3E, index=0x00)
07: if cond: skip_tokens(6)
    # skip -> show_text_block(str[22] "Sorry, benefits are given only once / per moon phase.")
08: show_text_block(str[16] "The spirits of Cron grant / you increased strength.")
09: wait_for_space()
10: op_1c_engine_query_to_cond(0x0C)
11: party_effect(sel=0x80, 3C 02 00 00 00)
12: store_var8(group=0x3E, value=0x01)
13: clear_current_tile_event_flag()
14: show_text_block(str[22] "Sorry, benefits are given only once / per moon phase.")
15: set_abort_and_exit()
```

**Event 13** — triggers: (2,10)/DIR_N?

```hex
01 11 09 11 01 0c 33 5d 0f
```

```
00: show_text_basic(str[17] "Stairs leading up. Ascend them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x33, 0x5D)
04: end_script()
```

**Event 14** — triggers: (1,11)/DIR_S?

```hex
0b 10 00 02 12 0a 11 01 0c 2e c0 0f
```

```
00: set_service_context(str[16] "The spirits of Cron grant / you increased strength.", mode=0x00)
01: show_text_block(str[18] ""Hey! I just saw Sir Elvis in / Hillstone Dungeon, level two. I knew / t")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x2E, 0xC0)
05: end_script()
```

**Event 15** — triggers: (0,15)/DIR_E?

```hex
02 13 09 11 05 02 14 26 20 09 10 01 05 00 00 11 01 1f 09 13 01 03 00 00 0f
```

```
00: show_text_block(str[19] "A mysterious molecule chamber appears / before you. Inside, bolts of lig")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_block(str[20] "The chamber is only big enough for / one. Who will enter (1-8)?")
04: selected = select_party_member()
05: party_effect_b(sel=0x09, 10 01 05 00 00)
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: party_effect(sel=0x09, 13 01 03 00 00)
08: end_script()
```

## String table

- `[00]` <EMPTY>
- `[01]` No / Archers!
- `[02]` No / Sorcerers!
- `[03]` No / Dwarves!
- `[04]` Fear no / Evil
- `[05]` See you / later
- `[06]` Have a / nice day!
- `[07]` There is a giant lever on the wall / labeled, "AE." Pull it (y/n)?
- `[08]` The sign above a magical / fountain reads, "eraweb selam" / Drink from it (y/n)?
- `[09]` one million gold, I'll sell you all / the hit points you SHOULD have." / Will you pay (y/n)?
- `[10]` "Looking for some help? Hey, I know / that we have all gotten a raw deal. I / don't think that a fair shake is too / much to ask for, do you? So, for just
- `[11]` The sign said NO ARCHERS!
- `[12]` You have found a Sun Crown!
- `[13]` The sign said NO SORCERERS!
- `[14]` You have found a Quartz Skull!
- `[15]` The sign said NO DWARVES!
- `[16]` The spirits of Cron grant / you increased strength.
- `[17]` Stairs leading up. Ascend them (y/n)?
- `[18]` "Hey! I just saw Sir Elvis in / Hillstone Dungeon, level two. I knew / that whole death thing was nothing / but a hoax." Go there (y/n)?
- `[19]` A mysterious molecule chamber appears / before you. Inside, bolts of light / move frantically about. Will you / enter the chamber (y/n)?
- `[20]` The chamber is only big enough for / one. Who will enter (1-8)?
- `[21]` *** Backpacks Full ***
- `[22]` Sorry, benefits are given only once / per moon phase.
- `[23]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
