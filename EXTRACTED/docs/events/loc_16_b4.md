# Location 16 — B4

- **event.dat** offset `0x005971`, length **1441** bytes
- **Map:** map screen **16**; overland sector **B4**
- **Record kind:** `mixed_pool`
- **Triggers:** 40; **script segments:** 34; **strings:** 23

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,8) | `0x18` | **14** | ANY_DIR |
| (1,10) | `0x1A` | **13** | ANY_DIR |
| (1,12) | `0x1C` | **15** | ANY_DIR |
| (1,13) | `0x1D` | **15** | ANY_DIR |
| (2,2) | `0x22` | **4** | 0x30 |
| (2,3) | `0x23` | **12** | 0x70 |
| (2,4) | `0x24` | **11** | 0x60 |
| (2,12) | `0x2C` | **15** | ANY_DIR |
| (3,2) | `0x32` | **28** | 0xB0 |
| (3,3) | `0x33` | **5** | ANY_DIR |
| (3,4) | `0x34` | **10** | 0xE0 |
| (3,5) | `0x35` | **32** | ALWAYS |
| (3,6) | `0x36` | **31** | ALWAYS |
| (3,7) | `0x37` | **30** | ALWAYS |
| (3,9) | `0x39` | **15** | ANY_DIR |
| (3,11) | `0x3B` | **15** | ANY_DIR |
| (4,2) | `0x42` | **7** | 0x90 |
| (4,3) | `0x43` | **8** | 0xD0 |
| (4,4) | `0x44` | **9** | ENTER+SPECIAL |
| (4,7) | `0x47` | **29** | ALWAYS |
| (5,7) | `0x57` | **27** | ALWAYS |
| (5,14) | `0x5E` | **15** | ANY_DIR |
| (6,7) | `0x67` | **6** | ALWAYS |
| (7,7) | `0x77` | **26** | ALWAYS |
| (8,7) | `0x87` | **25** | ALWAYS |
| (9,7) | `0x97` | **24** | ALWAYS |
| (9,14) | `0x9E` | **16** | ENTER |
| (10,4) | `0xA4` | **1** | DIR_N? |
| (10,7) | `0xA7` | **23** | ALWAYS |
| (11,7) | `0xB7` | **22** | ALWAYS |
| (12,4) | `0xC4` | **2** | DIR_N? |
| (12,7) | `0xC7` | **21** | ALWAYS |
| (12,12) | `0xCC` | **17** | ANY_DIR |
| (13,7) | `0xD7` | **20** | ALWAYS |
| (13,12) | `0xDC` | **17** | ANY_DIR |
| (14,7) | `0xE7` | **19** | ALWAYS |
| (14,12) | `0xEC` | **17** | ANY_DIR |
| (15,4) | `0xF4` | **3** | ANY_DIR |
| (15,7) | `0xF7` | **18** | ALWAYS |
| (15,12) | `0xFC` | **17** | ANY_DIR |

## Events

**Event 01** — triggers: (10,4)/DIR_N?

```hex
0b 07 00 02 01 0a 11 05 2d 04 05 11 01 0c 36 ee 02 02 08 0f
```

```
00: set_service_context(str[7] "Off in the distance is Atlantium.", mode=0x00)
01: show_text_block(str[1] "On heavy wooden doors of an old / fortress is scrawled: Sorcerers ONLY! ")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(5)
    # skip -> end_script()
04: cond = check_member_attr(fields=0x04, value=0x05)
05: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[2] "You are repelled!")
06: map_transition(0x36, 0xEE)
07: show_text_block(str[2] "You are repelled!")
08: wait_key()
09: end_script()
```

**Event 02** — triggers: (12,4)/DIR_N?

```hex
06 03
```

```
00: show_text_popup_style_b(str[3] "Good / Zone")
```

**Event 03** — triggers: (15,4)/ANY_DIR

```hex
06 04
```

```
00: show_text_popup_style_b(str[4] "Neutral / Zone")
```

**Event 04** — triggers: (2,2)/0x30

```hex
02 05 09 11 01 0c 1a ee 0f
```

```
00: show_text_block(str[5] "Weird lights flicker inside Murray's / Cave. Go in (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x1A, 0xEE)
04: end_script()
```

**Event 05** — triggers: (3,3)/ANY_DIR

```hex
02 06 07 14
```

```
00: show_text_block(str[6] "Mosaic tiles of frolicking dolphins / decorate Murray's Resort Spa.")
01: wait_for_space()
02: clear_current_tile_event_flag()
```

**Event 06** — triggers: (6,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 01 07 1e c8 0d 09 0c 10 57
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[7] "Off in the distance is Atlantium.")
02: clear_current_tile_event_flag()
03: show_text_basic(str[7] "Off in the distance is Atlantium.")
04: delay(0xC8)
05: engine_call(0x09)
06: map_transition(0x10, 0x57)
```

**Event 07** — triggers: (4,2)/0x90

```hex
0e 57
```

```
00: exec_selector(0x57)
```

**Event 08** — triggers: (4,3)/0xD0

```hex
2b 04 01 0a 09 11 02 12 73 73 73 73 73 73 73 73 73 73 73 02 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_basic(str[10] "Giants playing volleyball. Join (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=73 73 73 73 73 73 73 73 73 73, flags=73 02)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 09** — triggers: (4,4)/ENTER+SPECIAL

```hex
02 0b 09 11 09 15 00 7b 20 11 05 18 00 20 00 c8 18 00 21 00 00 18 00 3a 00 c8 18 00 3b 00 00 14 18 00 43 f7 08 14 0f
```

```
00: show_text_block(str[11] "Juice Bar. Drink (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(9)
    # skip -> end_script()
03: apply_party(count=0x00, op=0x7B, val=0x20)
04: if not cond: skip_tokens(5)
    # skip -> apply_party_masked(count=0x00, set=0x43, and=0xF7, or=0x08)
05: apply_party_masked(count=0x00, set=0x20, and=0x00, or=0xC8)
06: apply_party_masked(count=0x00, set=0x21, and=0x00, or=0x00)
07: apply_party_masked(count=0x00, set=0x3A, and=0x00, or=0xC8)
08: apply_party_masked(count=0x00, set=0x3B, and=0x00, or=0x00)
09: clear_current_tile_event_flag()
10: apply_party_masked(count=0x00, set=0x43, and=0xF7, or=0x08)
11: clear_current_tile_event_flag()
12: end_script()
```

**Event 10** — triggers: (3,4)/0xE0

```hex
2b 09 01 0c 09 11 07 1c 14 1f 80 22 01 00 00 00 1c 64 1b 32 11 01 13 01 00 00 00 00 00 00 00 00 00 14 0f
```

```
00: skip_tokens(9)
    # skip -> clear_current_tile_event_flag()
01: show_text_basic(str[12] "Murray's Gym. Work out (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(7)
    # skip -> end_script()
04: op_1c_engine_query_to_cond(0x14)
05: party_effect(sel=0x80, 22 01 00 00 00)
06: op_1c_engine_query_to_cond(0x64)
07: cond = (cond >= 0x32)
08: if not cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
09: encounter_setup_b(data=01 00 00 00 00 00 00 00 00 00)
10: clear_current_tile_event_flag()
11: end_script()
```

**Event 11** — triggers: (2,4)/0x60

```hex
01 0d 09 11 05 15 00 7b 20 10 04 01 0e 07 31 00 1e 00 0f 18 00 23 00 41 0f
```

```
00: show_text_basic(str[13] "A marble swimming pool. Jump in (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: apply_party(count=0x00, op=0x7B, val=0x20)
04: if cond: skip_tokens(4)
    # skip -> apply_party_masked(count=0x00, set=0x23, and=0x00, or=0x41)
05: show_text_basic(str[14] "Someone has poured acid in the pool!")
06: wait_for_space()
07: for_party(mask=0x00): call(0x1E,0x00)
08: end_script()
09: apply_party_masked(count=0x00, set=0x23, and=0x00, or=0x41)
10: end_script()
```

**Event 12** — triggers: (2,3)/0x70

```hex
01 0f 09 11 01 18 00 2c 00 4b 0f
```

```
00: show_text_basic(str[15] "Massage Therapy. Want a rub down(y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x2C, and=0x00, or=0x4B)
04: end_script()
```

**Event 13** — triggers: (1,10)/ANY_DIR

```hex
2b 03 17 0f 00 10 02 12 8b 8b 8b 8b 8b 8b 8b 8b 8b 8b 00 00 0e 5c 14
```

```
00: skip_tokens(3)
    # skip -> exec_selector(0x5C)
01: cond = load_var8(group=0x0F, index=0x00)
02: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=8B 8B 8B 8B 8B 8B 8B 8B 8B 8B, flags=00 00)
04: exec_selector(0x5C)
05: clear_current_tile_event_flag()
```

**Event 14** — triggers: (1,8)/ANY_DIR

```hex
2b 01 12 4b 4b 4b 4b 4b 4b 4b 4b 4b 4b 4b 05 2e f1 10 02 12 07 14
```

```
00: skip_tokens(1)
    # skip -> set_party_attr(class=0xF1, bits=0x10)
01: encounter_setup(monsters=4B 4B 4B 4B 4B 4B 4B 4B 4B 4B, flags=4B 05)
02: set_party_attr(class=0xF1, bits=0x10)
03: show_text_block(str[18] "The hyperactive fighting style of the / crazed natives has influenced yo")
04: wait_for_space()
05: clear_current_tile_event_flag()
```

**Event 15** — triggers: (1,12)/ANY_DIR, (1,13)/ANY_DIR, (2,12)/ANY_DIR, (3,9)/ANY_DIR, (3,11)/ANY_DIR, (5,14)/ANY_DIR

```hex
2b 01 12 4b 4b 4b 4b 4b 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=4B 4B 4B 4B 4B 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (9,14)/ENTER

```hex
01 13 09 11 02 1c c8 18 80 22 00 00 0f
```

```
00: show_text_basic(str[19] "A mineral water fountain. Drink (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: op_1c_engine_query_to_cond(0xC8)
04: apply_party_masked(count=0x80, set=0x22, and=0x00, or=0x00)
05: end_script()
```

**Event 17** — triggers: (12,12)/ANY_DIR, (13,12)/ANY_DIR, (14,12)/ANY_DIR, (15,12)/ANY_DIR

```hex
2b 07 01 14 1f 00 38 02 05 00 00 07 1c 64 1b 0b 10 01 12 e6 e6 e6 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(7)
    # skip -> clear_current_tile_event_flag()
01: show_text_basic(str[20] "Gems wash ashore!")
02: party_effect(sel=0x00, 38 02 05 00 00)
03: wait_for_space()
04: op_1c_engine_query_to_cond(0x64)
05: cond = (cond >= 0x0B)
06: if cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
07: encounter_setup(monsters=E6 E6 E6 00 00 00 00 00 00 00, flags=00 00)
08: clear_current_tile_event_flag()
```

**Event 18** — triggers: (15,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 e7
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0xE7)
```

**Event 19** — triggers: (14,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 d7
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0xD7)
```

**Event 20** — triggers: (13,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 c7
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0xC7)
```

**Event 21** — triggers: (12,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 b7
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0xB7)
```

**Event 22** — triggers: (11,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 a7
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0xA7)
```

**Event 23** — triggers: (10,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 01 09 1e c8 0d 09 0c 10 97
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[9] "Isle of the Ancients, Good Zone.")
02: clear_current_tile_event_flag()
03: show_text_basic(str[9] "Isle of the Ancients, Good Zone.")
04: delay(0xC8)
05: engine_call(0x09)
06: map_transition(0x10, 0x97)
```

**Event 24** — triggers: (9,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 87
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0x87)
```

**Event 25** — triggers: (8,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 77
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0x77)
```

**Event 26** — triggers: (7,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 67
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0x67)
```

**Event 27** — triggers: (5,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 47
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0x47)
```

**Event 28** — triggers: (3,2)/0xB0

```hex
0e 58
```

```
00: exec_selector(0x58)
```

**Event 29** — triggers: (4,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 37
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0x37)
```

**Event 30** — triggers: (3,7)/ALWAYS

```hex
15 00 7b 08 10 01 14 01 08 1e c8 0d 09 0c 10 36
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[8] "Coming up, Murray's Resort Isle.")
02: clear_current_tile_event_flag()
03: show_text_basic(str[8] "Coming up, Murray's Resort Isle.")
04: delay(0xC8)
05: engine_call(0x09)
06: map_transition(0x10, 0x36)
```

**Event 31** — triggers: (3,6)/ALWAYS

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 35
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0x35)
```

**Event 32** — triggers: (3,5)/ALWAYS

```hex
15 00 7b 08 10 01 14 02 15 07 18 00 7b e3 10 0c 10 33
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> show_text_block(str[21] "I hope you have enjoyed your cruise to / Murray's Resort Isle.")
02: clear_current_tile_event_flag()
03: show_text_block(str[21] "I hope you have enjoyed your cruise to / Murray's Resort Isle.")
04: wait_for_space()
05: apply_party_masked(count=0x00, set=0x7B, and=0xE3, or=0x10)
06: map_transition(0x10, 0x33)
```

## String table

- `[00]` <EMPTY>
- `[01]` On heavy wooden doors of an old / fortress is scrawled: Sorcerers ONLY! / Enter (y/n)?
- `[02]` You are repelled!
- `[03]` Good / Zone
- `[04]` Neutral / Zone
- `[05]` Weird lights flicker inside Murray's / Cave. Go in (y/n)?
- `[06]` Mosaic tiles of frolicking dolphins / decorate Murray's Resort Spa.
- `[07]` Off in the distance is Atlantium.
- `[08]` Coming up, Murray's Resort Isle.
- `[09]` Isle of the Ancients, Good Zone.
- `[10]` Giants playing volleyball. Join (y/n)?
- `[11]` Juice Bar. Drink (y/n)?
- `[12]` Murray's Gym. Work out (y/n)?
- `[13]` A marble swimming pool. Jump in (y/n)?
- `[14]` Someone has poured acid in the pool!
- `[15]` Massage Therapy. Want a rub down(y/n)?
- `[16]` You feel rejuvenated.
- `[17]` "
- `[18]` The hyperactive fighting style of the / crazed natives has influenced your / Clerics to concoct a Frenzy Spell.
- `[19]` A mineral water fountain. Drink (y/n)?
- `[20]` Gems wash ashore!
- `[21]` I hope you have enjoyed your cruise to / Murray's Resort Isle.
- `[22]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
