# Location 46 — Hillstone Dungeon Level 2

- **event.dat** offset `0x00E4B1`, length **1247** bytes
- **Map:** map screen **46**; Hillstone Dungeon Level 2
- **Record kind:** `standard`
- **Triggers:** 28; **script segments:** 14; **strings:** 20

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **11** | DIR_SPECIAL |
| (0,4) | `0x04` | **3** | 0x60 |
| (1,14) | `0x1E` | **9** | ANY_DIR |
| (4,3) | `0x43` | **9** | ANY_DIR |
| (4,12) | `0x4C` | **9** | ANY_DIR |
| (4,15) | `0x4F` | **9** | ANY_DIR |
| (5,11) | `0x5B` | **9** | ANY_DIR |
| (6,10) | `0x6A` | **12** | ENTER |
| (7,1) | `0x71` | **9** | ANY_DIR |
| (7,7) | `0x77` | **9** | ANY_DIR |
| (7,10) | `0x7A` | **6** | ENTER |
| (7,11) | `0x7B` | **9** | ANY_DIR |
| (8,0) | `0x80` | **7** | 0x90 |
| (9,3) | `0x93` | **8** | 0x60 |
| (10,5) | `0xA5` | **9** | ANY_DIR |
| (10,7) | `0xA7` | **5** | DIR_N? |
| (11,11) | `0xBB` | **9** | ANY_DIR |
| (12,0) | `0xC0` | **1** | ALWAYS |
| (13,1) | `0xD1` | **9** | ANY_DIR |
| (13,4) | `0xD4` | **9** | ANY_DIR |
| (13,7) | `0xD7` | **9** | ANY_DIR |
| (13,12) | `0xDC` | **9** | ANY_DIR |
| (14,2) | `0xE2` | **9** | ANY_DIR |
| (14,13) | `0xED` | **10** | DIR_SPECIAL |
| (15,0) | `0xF0` | **9** | ANY_DIR |
| (15,1) | `0xF1` | **9** | ANY_DIR |
| (15,9) | `0xF9` | **4** | ENTER |
| (15,15) | `0xFF` | **2** | ENTER+SPECIAL |

## Events

**Event 01** — triggers: (12,0)/ALWAYS

```hex
01 01 09 11 01 0c 2d 15 0f
```

```
00: show_text_basic(str[1] "Ascend the stairs (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x2D, 0x15)
04: end_script()
```

**Event 02** — triggers: (15,15)/ENTER+SPECIAL

```hex
2d 27 00 11 04 01 0e 07 0d 09 0c 2e 88 02 02 19 01 ee 00 00 10 01 01 10 07 14
```

```
00: cond = check_member_attr(fields=0x27, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[2] "You find a Lapis Scarab.")
02: show_text_basic(str[14] "Your party is whisked away.")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2E, 0x88)
06: show_text_block(str[2] "You find a Lapis Scarab.")
07: add_party_entity(0x01, f3a=0xEE, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[16] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 03** — triggers: (0,4)/0x60

```hex
2d 26 00 11 04 01 11 07 0d 09 0c 2e 88 02 03 19 01 f4 00 00 10 01 01 10 07 14
```

```
00: cond = check_member_attr(fields=0x26, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[3] "You find a Ruby Amulet.")
02: show_text_basic(str[17] "The sign said NO NINJAS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2E, 0x88)
06: show_text_block(str[3] "You find a Ruby Amulet.")
07: add_party_entity(0x01, f3a=0xF4, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[16] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 04** — triggers: (15,9)/ENTER

```hex
02 04 09 11 01 0e cc 0f
```

```
00: show_text_block(str[4] "A sign next to a bottomless pit reads, / "A golden experience awaits all")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: exec_selector(0xCC)  # quest_handler_204
04: end_script()
```

**Event 05** — triggers: (10,7)/DIR_N?

```hex
0b 15 00 02 05 0a 11 01 0c 30 f7 0f
```

```
00: set_service_context(str[21], mode=0x00)
01: show_text_block(str[5] ""I can lead you to level two of / Woodhaven Dungeon if you want." / Foll")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x30, 0xF7)
05: end_script()
```

**Event 06** — triggers: (7,10)/ENTER

```hex
2d a4 00 11 04 01 0f 07 0d 09 0c 2e 88 17 3e 00 10 06 02 06 07 1c 0c 1f 80 3c 02 00 00 00 1a 3e 01 14 02 12 29
```

```
00: cond = check_member_attr(fields=0xA4, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> cond = load_var8(group=0x3E, index=0x00)
02: show_text_basic(str[15] "The sign said NO HALF-ORCS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x2E, 0x88)
06: cond = load_var8(group=0x3E, index=0x00)
07: if cond: skip_tokens(6)
    # skip -> show_text_block(str[18] "Sorry, benefits are given only once / per moon phase.")
08: show_text_block(str[6] "An energizing light beams down from / the ceiling, strengthening all.")
09: wait_for_space()
10: op_1c_engine_query_to_cond(0x0C)
11: party_effect(sel=0x80, 3C 02 00 00 00)
12: store_var8(group=0x3E, value=0x01)
13: clear_current_tile_event_flag()
14: show_text_block(str[18] "Sorry, benefits are given only once / per moon phase.")
15: set_abort_and_exit()
```

**Event 07** — triggers: (8,0)/0x90

```hex
02 07 09 11 01 18 00 0c 00 00 0f
```

```
00: show_text_block(str[7] "A magical whirlpool emits a strange / odor. You sense that the waters ar")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x0C, and=0x00, or=0x00)
04: end_script()
```

**Event 08** — triggers: (9,3)/0x60

```hex
03 08 09 11 05 03 09 26 20 09 44 01 05 00 00 11 01 1f 09 11 01 03 00 00 0f
```

```
00: show_text(str[8] "The writings of many symbols and / languages are scrawled into an ancien")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: show_text(str[9] "Who will touch (1-8)?")
04: selected = select_party_member()
05: party_effect_b(sel=0x09, 44 01 05 00 00)
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: party_effect(sel=0x09, 11 01 03 00 00)
08: end_script()
```

**Event 09** — triggers: (1,14)/ANY_DIR, (4,3)/ANY_DIR, (4,12)/ANY_DIR, (4,15)/ANY_DIR, (5,11)/ANY_DIR, (7,1)/ANY_DIR, (7,7)/ANY_DIR, (7,11)/ANY_DIR, (10,5)/ANY_DIR, (11,11)/ANY_DIR, (13,1)/ANY_DIR, (13,4)/ANY_DIR, (13,7)/ANY_DIR, (13,12)/ANY_DIR, (14,2)/ANY_DIR, (15,0)/ANY_DIR, (15,1)/ANY_DIR

```hex
2b 03 02 0a 18 00 43 f7 08 13 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[10] "As you move forward, scores of darts / fly at you from all directions.")
02: apply_party_masked(count=0x00, set=0x43, and=0xF7, or=0x08)
03: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
04: clear_current_tile_event_flag()
```

**Event 10** — triggers: (14,13)/DIR_SPECIAL

```hex
04 0b
```

```
00: show_text_above_door(str[11] "No Barbarians")
```

**Event 11** — triggers: (0,2)/DIR_SPECIAL

```hex
04 0c
```

```
00: show_text_above_door(str[12] "No Ninjas")
```

**Event 12** — triggers: (6,10)/ENTER

```hex
04 0d
```

```
00: show_text_above_door(str[13] "No Half-Orcs")
```

## String table

- `[00]` <EMPTY>
- `[01]` Ascend the stairs (y/n)?
- `[02]` You find a Lapis Scarab.
- `[03]` You find a Ruby Amulet.
- `[04]` A sign next to a bottomless pit reads, / "A golden experience awaits all who / empty their pockets." Toss all of your / gold in (y/n)?
- `[05]` "I can lead you to level two of / Woodhaven Dungeon if you want." / Follow him (y/n)?
- `[06]` An energizing light beams down from / the ceiling, strengthening all.
- `[07]` A magical whirlpool emits a strange / odor. You sense that the waters are / powerful and the effects could be / (ir)reversible. Drink (y/n)?
- `[08]` The writings of many symbols and / languages are scrawled into an ancient / glowing stone. Among the etchings, you / make out, "Knowledge for a price to / those who will touch." / Will you try (y/n)?
- `[09]` Who will touch (1-8)?
- `[10]` As you move forward, scores of darts / fly at you from all directions.
- `[11]` No Barbarians
- `[12]` No Ninjas
- `[13]` No Half-Orcs
- `[14]` Your party is whisked away.
- `[15]` The sign said NO HALF-ORCS!
- `[16]` *** Backpacks Full ***
- `[17]` The sign said NO NINJAS!
- `[18]` Sorry, benefits are given only once / per moon phase.
- `[19]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
