# Location 45 — Hillstone Dungeon Level 1

- **event.dat** offset `0x00E04A`, length **1127** bytes
- **Map:** map screen **45**; Hillstone Dungeon Level 1
- **Record kind:** `standard`
- **Triggers:** 28; **script segments:** 14; **strings:** 21

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **11** | DIR_S? |
| (0,1) | `0x01` | **5** | ANY_DIR |
| (0,8) | `0x08` | **5** | ANY_DIR |
| (0,15) | `0x0F` | **5** | ANY_DIR |
| (1,5) | `0x15` | **10** | DIR_E? |
| (1,11) | `0x1B` | **6** | DIR_W? |
| (2,1) | `0x21` | **5** | ANY_DIR |
| (2,7) | `0x27` | **5** | ANY_DIR |
| (2,15) | `0x2F` | **5** | ANY_DIR |
| (3,9) | `0x39` | **5** | ANY_DIR |
| (4,14) | `0x4E` | **5** | ANY_DIR |
| (5,3) | `0x53` | **5** | ANY_DIR |
| (5,5) | `0x55` | **5** | ANY_DIR |
| (5,7) | `0x57` | **7** | DIR_S? |
| (5,9) | `0x59` | **7** | DIR_S? |
| (5,11) | `0x5B` | **12** | DIR_N? |
| (6,8) | `0x68` | **5** | ANY_DIR |
| (7,6) | `0x76` | **7** | DIR_W? |
| (7,10) | `0x7A` | **7** | DIR_E? |
| (9,13) | `0x9D` | **5** | ANY_DIR |
| (12,1) | `0xC1` | **9** | DIR_N? |
| (12,3) | `0xC3` | **5** | ANY_DIR |
| (12,8) | `0xC8` | **5** | ANY_DIR |
| (12,15) | `0xCF` | **8** | DIR_N? |
| (14,1) | `0xE1` | **4** | DIR_N? |
| (14,15) | `0xEF` | **3** | DIR_N? |
| (15,8) | `0xF8` | **1** | DIR_N? |
| (15,15) | `0xFF` | **2** | DIR_E? |

## Events

**Event 01** — triggers: (15,8)/DIR_N?

```hex
01 01 09 11 01 0c 37 da 0f
```

```
00: show_text_basic(str[1] "Stairs lead up. Climb them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x37, 0xDA)
04: end_script()
```

**Event 02** — triggers: (15,15)/DIR_E?

```hex
0b 0d 00 03 02 0a 11 01 0c 2f e0 0f
```

```
00: set_service_context(str[13] "No Barbarians!", mode=0x00)
01: show_text(str[2] ""I told one bad orc joke too many, and / Lord Slayer threw me in the dun")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x2F, 0xE0)
05: end_script()
```

**Event 03** — triggers: (14,15)/DIR_N?

```hex
2d 26 00 11 04 01 03 07 0d 09 0c 2d f8 02 04 19 01 f3 00 00 10 01 01 05 07 14
```

```
00: cond = check_member_attr(fields=0x26, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[4] "You have found a Crystal Vial.")
02: show_text_basic(str[3] "The sign said NO NINJAS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2D, 0xF8)
06: show_text_block(str[4] "You have found a Crystal Vial.")
07: add_party_entity(0x01, f3a=0xF3, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[5] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 04** — triggers: (14,1)/DIR_N?

```hex
2d 27 00 11 04 01 06 07 0d 09 0c 2d f8 02 07 19 01 ed 00 00 10 01 01 05 07 14
```

```
00: cond = check_member_attr(fields=0x27, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[7] "You have found a Coral Brooch!")
02: show_text_basic(str[6] "The sign said NO BARBARIANS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2D, 0xF8)
06: show_text_block(str[7] "You have found a Coral Brooch!")
07: add_party_entity(0x01, f3a=0xED, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[5] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 05** — triggers: (0,1)/ANY_DIR, (0,8)/ANY_DIR, (0,15)/ANY_DIR, (2,1)/ANY_DIR, (2,7)/ANY_DIR, (2,15)/ANY_DIR, (3,9)/ANY_DIR, (4,14)/ANY_DIR, (5,3)/ANY_DIR, (5,5)/ANY_DIR, (6,8)/ANY_DIR, (9,13)/ANY_DIR, (12,3)/ANY_DIR, (12,8)/ANY_DIR

```hex
2b 03 02 08 31 00 1e 00 13 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[8] "A trap! Spikes hit you from behind!")
02: for_party(mask=0x00): call(0x1E,0x00)
03: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
04: clear_current_tile_event_flag()
```

**Event 06** — triggers: (1,11)/DIR_W?

```hex
2d a4 00 11 04 01 09 07 0d 09 0c 2d f8 17 3d 00 10 06 01 0a 07 1c 05 1f 80 3c 02 00 00 00 1a 3d 01 14 02 13 29
```

```
00: cond = check_member_attr(fields=0xA4, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> cond = load_var8(group=0x3D, index=0x00)
02: show_text_basic(str[9] "The sign said NO HALF-ORCS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2D, 0xF8)
06: cond = load_var8(group=0x3D, index=0x00)
07: if cond: skip_tokens(6)
    # skip -> show_text_block(str[19] "Sorry, benefits are given only once / per moon phase.")
08: show_text_basic(str[10] "A calm melody strengthens your party.")
09: wait_for_space()
10: op_1c_engine_query_to_cond(0x05)
11: party_effect(sel=0x80, 3C 02 00 00 00)
12: store_var8(group=0x3D, value=0x01)
13: clear_current_tile_event_flag()
14: show_text_block(str[19] "Sorry, benefits are given only once / per moon phase.")
15: set_abort_and_exit()
```

**Event 07** — triggers: (5,7)/DIR_S?, (5,9)/DIR_S?, (7,6)/DIR_W?, (7,10)/DIR_E?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "No Half-Orcs")
```

**Event 08** — triggers: (12,15)/DIR_N?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "No Ninjas!")
```

**Event 09** — triggers: (12,1)/DIR_N?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "No Barbarians!")
```

**Event 10** — triggers: (1,5)/DIR_E?

```hex
01 0e 09 11 01 0c 2e c0 0f
```

```
00: show_text_basic(str[14] "Stairs leading down. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x2E, 0xC0)
04: end_script()
```

**Event 11** — triggers: (0,0)/DIR_S?

```hex
0b 0b 00 03 0f 0a 11 07 03 10 27 20 09 11 01 05 00 00 11 03 1f 09 10 01 03 00 00 03 11 08 0f
```

```
00: set_service_context(str[11] "No Half-Orcs", mode=0x00)
01: show_text(str[15] ""Everybody's always picking on me / because I am as stupid as an Orc. / ")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(7)
    # skip -> end_script()
04: show_text(str[16] "Who will trade (1-8)?")
05: selected = select_party_member(mode=1)
06: party_effect_b(sel=0x09, 11 01 05 00 00)
07: if not cond: skip_tokens(3)
    # skip -> end_script()
08: party_effect(sel=0x09, 10 01 03 00 00)
09: show_text(str[17] "Thanks!")
10: wait_key()
11: end_script()
```

**Event 12** — triggers: (5,11)/DIR_N?

```hex
02 12 09 11 01 0e cb 0f
```

```
00: show_text_block(str[18] "A giant coin slot is labeled, / "Insert gold here." Do you (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: exec_selector(0xCB)  # quest_handler_203
04: end_script()
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs lead up. Climb them (y/n)?
- `[02]` "I told one bad orc joke too many, and / Lord Slayer threw me in the dungeon. / I've worked some rough rooms, but this / is ridiculous. But seriously folks, I / can send you to Woodhaven Dungeon just / for the halibut." Go (y/n)?
- `[03]` The sign said NO NINJAS!
- `[04]` You have found a Crystal Vial.
- `[05]` *** Backpacks Full ***
- `[06]` The sign said NO BARBARIANS!
- `[07]` You have found a Coral Brooch!
- `[08]` A trap! Spikes hit you from behind!
- `[09]` The sign said NO HALF-ORCS!
- `[10]` A calm melody strengthens your party.
- `[11]` No Half-Orcs
- `[12]` No Ninjas!
- `[13]` No Barbarians!
- `[14]` Stairs leading down. Descend (y/n)?
- `[15]` "Everybody's always picking on me / because I am as stupid as an Orc. / So, I'll trade what I got for what / you got. OK (y/n)?"
- `[16]` Who will trade (1-8)?
- `[17]` Thanks!
- `[18]` A giant coin slot is labeled, / "Insert gold here." Do you (y/n)?
- `[19]` Sorry, benefits are given only once / per moon phase.
- `[20]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
