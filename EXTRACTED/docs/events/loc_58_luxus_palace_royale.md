# Location 58 — Luxus Palace Royale

- **event.dat** offset `0x0122AB`, length **1444** bytes
- **Map:** map screen **58**; Luxus Palace Royale
- **Record kind:** `standard`
- **Triggers:** 66; **script segments:** 39; **strings:** 27

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **14** | DIR_W? |
| (0,6) | `0x06` | **15** | DIR_S? |
| (0,7) | `0x07` | **15** | DIR_S? |
| (0,8) | `0x08` | **15** | DIR_S? |
| (0,9) | `0x09` | **15** | DIR_S? |
| (2,13) | `0x2D` | **9** | DIR_N? |
| (3,5) | `0x35` | **29** | ANY_DIR |
| (3,6) | `0x36` | **29** | ANY_DIR |
| (3,7) | `0x37` | **2** | DIR_N? |
| (3,8) | `0x38` | **2** | DIR_N? |
| (3,9) | `0x39` | **29** | ANY_DIR |
| (3,10) | `0x3A` | **29** | ANY_DIR |
| (4,12) | `0x4C` | **30** | ANY_DIR |
| (4,14) | `0x4E` | **30** | ANY_DIR |
| (5,2) | `0x52` | **36** | ANY_DIR |
| (5,6) | `0x56` | **26** | ANY_DIR |
| (5,7) | `0x57` | **6** | DIR_W? |
| (5,8) | `0x58` | **6** | DIR_E? |
| (5,9) | `0x59` | **26** | ANY_DIR |
| (6,0) | `0x60` | **17** | DIR_W? |
| (6,4) | `0x64` | **12** | DIR_W? |
| (6,12) | `0x6C` | **31** | ANY_DIR |
| (6,14) | `0x6E` | **31** | ANY_DIR |
| (7,2) | `0x72` | **36** | ANY_DIR |
| (7,6) | `0x76` | **27** | ANY_DIR |
| (7,7) | `0x77` | **25** | DIR_W? |
| (7,8) | `0x78` | **7** | DIR_E? |
| (7,9) | `0x79` | **23** | DIR_E? |
| (8,7) | `0x87` | **21** | DIR_N? |
| (8,8) | `0x88` | **21** | DIR_N? |
| (9,4) | `0x94` | **10** | DIR_N? |
| (9,6) | `0x96` | **24** | DIR_W? |
| (9,7) | `0x97` | **5** | DIR_W? |
| (9,8) | `0x98` | **8** | DIR_E? |
| (9,9) | `0x99` | **28** | ANY_DIR |
| (9,12) | `0x9C` | **30** | ANY_DIR |
| (9,14) | `0x9E` | **31** | ANY_DIR |
| (10,2) | `0xA2` | **32** | ANY_DIR |
| (10,4) | `0xA4` | **32** | ANY_DIR |
| (10,7) | `0xA7` | **20** | DIR_N? |
| (10,8) | `0xA8` | **20** | DIR_N? |
| (11,0) | `0xB0` | **33** | ANY_DIR |
| (11,2) | `0xB2` | **33** | ANY_DIR |
| (11,6) | `0xB6` | **22** | DIR_W? |
| (11,7) | `0xB7` | **5** | DIR_W? |
| (11,8) | `0xB8` | **3** | DIR_E? |
| (11,9) | `0xB9` | **24** | DIR_E? |
| (12,1) | `0xC1` | **32** | ANY_DIR |
| (12,2) | `0xC2` | **11** | DIR_N? |
| (12,3) | `0xC3` | **33** | ANY_DIR |
| (12,7) | `0xC7` | **4** | DIR_N? |
| (12,8) | `0xC8` | **4** | DIR_N? |
| (12,12) | `0xCC` | **34** | ANY_DIR |
| (12,14) | `0xCE` | **34** | ANY_DIR |
| (13,1) | `0xD1` | **35** | ANY_DIR |
| (13,2) | `0xD2` | **35** | ANY_DIR |
| (13,3) | `0xD3` | **35** | ANY_DIR |
| (13,7) | `0xD7` | **18** | DIR_N? |
| (13,8) | `0xD8` | **18** | DIR_N? |
| (14,12) | `0xEC` | **34** | ANY_DIR |
| (14,13) | `0xED` | **1** | DIR_E? |
| (14,14) | `0xEE` | **16** | DIR_E? |
| (15,6) | `0xF6` | **37** | ANY_DIR |
| (15,7) | `0xF7` | **13** | DIR_W? |
| (15,8) | `0xF8` | **13** | DIR_E? |
| (15,9) | `0xF9` | **37** | ANY_DIR |

## Events

**Event 01** — triggers: (14,13)/DIR_E?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Black Room")
```

**Event 02** — triggers: (3,7)/DIR_N?, (3,8)/DIR_N?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Great Hall")
```

**Event 03** — triggers: (11,8)/DIR_E?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Court Wizard")
```

**Event 04** — triggers: (12,7)/DIR_N?, (12,8)/DIR_N?

```hex
04 04
```

```
00: show_text_above_door(str[4] "Throne Room")
```

**Event 05** — triggers: (9,7)/DIR_W?, (11,7)/DIR_W?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Corak's Study")
```

**Event 06** — triggers: (5,7)/DIR_W?, (5,8)/DIR_E?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Fool's Room")
```

**Event 07** — triggers: (7,8)/DIR_E?

```hex
04 07
```

```
00: show_text_above_door(str[7] "Alchemist")
```

**Event 08** — triggers: (9,8)/DIR_E?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Apprentice Wizards")
```

**Event 09** — triggers: (2,13)/DIR_N?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Hall of Illusions")
```

**Event 10** — triggers: (9,4)/DIR_N?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Fighting Room")
```

**Event 11** — triggers: (12,2)/DIR_N?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Target Room")
```

**Event 12** — triggers: (6,4)/DIR_W?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Temple")
```

**Event 13** — triggers: (15,7)/DIR_W?, (15,8)/DIR_E?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Funny Room")
```

**Event 14** — triggers: (0,0)/DIR_W?

```hex
01 0e 09 11 01 0c 33 9b 0f
```

```
00: show_text_basic(str[14] "Stairs to Dungeon. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x33, 0x9B)
04: end_script()
```

**Event 15** — triggers: (0,6)/DIR_S?, (0,7)/DIR_S?, (0,8)/DIR_S?, (0,9)/DIR_S?

```hex
01 0f 09 11 01 0c 22 ee 0f
```

```
00: show_text_basic(str[15] "Palace exit. Leave (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x22, 0xEE)
04: end_script()
```

**Event 16** — triggers: (14,14)/DIR_E?

```hex
0b 05 00 28 01 72 11 34 02 11 1f 00 31 04 10 27 00 15 01 7d 0e 1b 0e 11 03 1f 01 31 04 40 0d 03 18 01 7d f1 00 18 01 7e df 20 15 02 7d 0e 1b 0e 11 03 1f 02 31 04 40 0d 03 18 02 7d f1 00 18 02 7e df 20 15 03 7d 0e 1b 0e 11 03 1f 03 31 04 40 0d 03 18 03 7d f1 00 18 03 7e df 20 15 04 7d 0e 1b 0e 11 03 1f 04 31 04 40 0d 03 18 04 7d f1 00 18 04 7e df 20 15 05 7d 0e 1b 0e 11 03 1f 05 31 04 40 0d 03 18 05 7d f1 00 18 05 7e df 20 15 06 7d 0e 1b 0e 11 03 1f 06 31 04 40 0d 03 18 06 7d f1 00 18 06 7e df 20 15 07 7d 0e 1b 0e 11 03 1f 07 31 04 40 0d 03 18 07 7d f1 00 18 07 7e df 20 15 08 7d 0e 1b 0e 11 03 1f 08 31 04 40 0d 03 18 08 7d f1 00 18 08 7e df 20 08 14 02 10 08 0f
```

```
00: set_service_context(str[5] "Corak's Study", mode=0x00)
01: cond = consume_item(item_id=114, name="Black Key", probe=1)
02: if not cond: skip_tokens(52)
    # skip -> show_text_block(str[16] "The Bishop of Black Battle is locked / in a cage.")
03: show_text_block(str[17] "The Bishop of Black Battle is freed / from imprisonment by your black ke")
04: party_effect(sel=0x00, 31 04 10 27 00)
05: apply_party(count=0x01, op=0x7D, val=0x0E)
06: cond = (cond >= 0x0E)
07: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x02, op=0x7D, val=0x0E)
08: party_effect(sel=0x01, 31 04 40 0D 03)
09: apply_party_masked(count=0x01, set=0x7D, and=0xF1, or=0x00)
10: apply_party_masked(count=0x01, set=0x7E, and=0xDF, or=0x20)
11: apply_party(count=0x02, op=0x7D, val=0x0E)
12: cond = (cond >= 0x0E)
13: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x03, op=0x7D, val=0x0E)
14: party_effect(sel=0x02, 31 04 40 0D 03)
15: apply_party_masked(count=0x02, set=0x7D, and=0xF1, or=0x00)
16: apply_party_masked(count=0x02, set=0x7E, and=0xDF, or=0x20)
17: apply_party(count=0x03, op=0x7D, val=0x0E)
18: cond = (cond >= 0x0E)
19: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x04, op=0x7D, val=0x0E)
20: party_effect(sel=0x03, 31 04 40 0D 03)
21: apply_party_masked(count=0x03, set=0x7D, and=0xF1, or=0x00)
22: apply_party_masked(count=0x03, set=0x7E, and=0xDF, or=0x20)
23: apply_party(count=0x04, op=0x7D, val=0x0E)
24: cond = (cond >= 0x0E)
25: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x05, op=0x7D, val=0x0E)
26: party_effect(sel=0x04, 31 04 40 0D 03)
27: apply_party_masked(count=0x04, set=0x7D, and=0xF1, or=0x00)
28: apply_party_masked(count=0x04, set=0x7E, and=0xDF, or=0x20)
29: apply_party(count=0x05, op=0x7D, val=0x0E)
30: cond = (cond >= 0x0E)
31: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x06, op=0x7D, val=0x0E)
32: party_effect(sel=0x05, 31 04 40 0D 03)
33: apply_party_masked(count=0x05, set=0x7D, and=0xF1, or=0x00)
34: apply_party_masked(count=0x05, set=0x7E, and=0xDF, or=0x20)
35: apply_party(count=0x06, op=0x7D, val=0x0E)
36: cond = (cond >= 0x0E)
37: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x07, op=0x7D, val=0x0E)
38: party_effect(sel=0x06, 31 04 40 0D 03)
39: apply_party_masked(count=0x06, set=0x7D, and=0xF1, or=0x00)
40: apply_party_masked(count=0x06, set=0x7E, and=0xDF, or=0x20)
41: apply_party(count=0x07, op=0x7D, val=0x0E)
42: cond = (cond >= 0x0E)
43: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x08, op=0x7D, val=0x0E)
44: party_effect(sel=0x07, 31 04 40 0D 03)
45: apply_party_masked(count=0x07, set=0x7D, and=0xF1, or=0x00)
46: apply_party_masked(count=0x07, set=0x7E, and=0xDF, or=0x20)
47: apply_party(count=0x08, op=0x7D, val=0x0E)
48: cond = (cond >= 0x0E)
49: if not cond: skip_tokens(3)
    # skip -> wait_key()
50: party_effect(sel=0x08, 31 04 40 0D 03)
51: apply_party_masked(count=0x08, set=0x7D, and=0xF1, or=0x00)
52: apply_party_masked(count=0x08, set=0x7E, and=0xDF, or=0x20)
53: wait_key()
54: clear_current_tile_event_flag()
55: show_text_block(str[16] "The Bishop of Black Battle is locked / in a cage.")
56: wait_key()
57: end_script()
```

**Event 17** — triggers: (6,0)/DIR_W?

```hex
02 12 09 11 05 19 01 fd 00 00 10 02 02 14 07 14 0f
```

```
00: show_text_block(str[18] "A little princess plays with an / A1-Todilor. Take it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: add_party_entity(0x01, f3a=0xFD, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_block(str[20] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
08: end_script()
```

**Event 18** — triggers: (13,7)/DIR_N?, (13,8)/DIR_N?

```hex
15 00 7f 10 10 02 0b 14 00 0e e3 15 00 7f 02 10 02 0b 14 00 0e e4 0b 0e 00 0e e5
```

```
00: apply_party(count=0x00, op=0x7F, val=0x10)
01: if cond: skip_tokens(2)
    # skip -> apply_party(count=0x00, op=0x7F, val=0x02)
02: set_service_context(str[20] "*** Backpacks Full ***", mode=0x00)
03: exec_selector(0xE3)
04: apply_party(count=0x00, op=0x7F, val=0x02)
05: if cond: skip_tokens(2)
    # skip -> set_service_context(str[14] "Stairs to Dungeon. Descend (y/n)?", mode=0x00)
06: set_service_context(str[20] "*** Backpacks Full ***", mode=0x00)
07: exec_selector(0xE4)
08: set_service_context(str[14] "Stairs to Dungeon. Descend (y/n)?", mode=0x00)
09: exec_selector(0xE5)
```

**Event 20** — triggers: (10,7)/DIR_N?, (10,8)/DIR_N?

```hex
0b 0d 00 01 15 0e e2
```

```
00: set_service_context(str[13] "Funny Room", mode=0x00)
01: show_text_basic(str[21] "Here's the joke of the day:")
02: exec_selector(0xE2)  # special_226
```

**Event 21** — triggers: (8,7)/DIR_N?, (8,8)/DIR_N?

```hex
02 16 09 11 01 18 00 42 00 28 0f
```

```
00: show_text_block(str[22] "A sumptuous feast is laid upon the / huge banquet table. Take some (y/n)")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x42, and=0x00, or=0x28)
04: end_script()
```

**Event 22** — triggers: (11,6)/DIR_W?

```hex
17 3b 00 10 02 0b 17 00 0e f2 01 17 29
```

```
00: cond = load_var8(group=0x3B, index=0x00)
01: if cond: skip_tokens(2)
    # skip -> show_text_basic(str[23] "Dust covers everything.")
02: set_service_context(str[23] "Dust covers everything.", mode=0x00)
03: exec_selector(0xF2)
04: show_text_basic(str[23] "Dust covers everything.")
05: set_abort_and_exit()
```

**Event 23** — triggers: (7,9)/DIR_E?

```hex
2b 06 02 13 09 10 01 0f 02 19 12 69 69 69 69 69 69 69 69 69 69 00 00 2a e8 03 00 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(6)
    # skip -> set_treasure(gold/exp=1000, gems=0, items=[0, 0, 0])
01: show_text_block(str[19] "The palace alchemist chamber contains / piles of gold made for the treas")
02: cond = prompt_yes_no()
03: if cond: skip_tokens(1)
    # skip -> show_text_block(str[25] "Castle Guards intervene!")
04: end_script()
05: show_text_block(str[25] "Castle Guards intervene!")
06: encounter_setup(monsters=69 69 69 69 69 69 69 69 69 69, flags=00 00)
07: set_treasure(gold/exp=1000, gems=0, items=[0, 0, 0])
08: clear_current_tile_event_flag()
```

**Event 24** — triggers: (9,6)/DIR_W?, (11,9)/DIR_E?

```hex
01 17 29 0e e2 02 16 09
```

```
00: show_text_basic(str[23] "Dust covers everything.")
01: set_abort_and_exit()
02: exec_selector(0xE2)  # special_226
03: show_text_block(str[22] "A sumptuous feast is laid upon the / huge banquet table. Take some (y/n)")
04: cond = prompt_yes_no()
```

**Event 25** — triggers: (7,7)/DIR_W?

```hex
04 18
```

```
00: show_text_above_door(str[24] "Barracks")
```

**Event 26** — triggers: (5,6)/ANY_DIR, (5,9)/ANY_DIR

```hex
2b 01 0e e6 14 11 01
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xE6)
02: clear_current_tile_event_flag()
03: if not cond: skip_tokens(1)
```

**Event 27** — triggers: (7,6)/ANY_DIR

```hex
2b 01 0e e7 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xE7)
02: clear_current_tile_event_flag()
```

**Event 28** — triggers: (9,9)/ANY_DIR

```hex
2b 01 0e e8 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xE8)
02: clear_current_tile_event_flag()
```

**Event 29** — triggers: (3,5)/ANY_DIR, (3,6)/ANY_DIR, (3,9)/ANY_DIR, (3,10)/ANY_DIR

```hex
2b 01 0e e9 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xE9)
02: clear_current_tile_event_flag()
```

**Event 30** — triggers: (4,12)/ANY_DIR, (4,14)/ANY_DIR, (9,12)/ANY_DIR

```hex
2b 01 0e ea 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xEA)
02: clear_current_tile_event_flag()
```

**Event 31** — triggers: (6,12)/ANY_DIR, (6,14)/ANY_DIR, (9,14)/ANY_DIR

```hex
2b 01 0e eb 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xEB)
02: clear_current_tile_event_flag()
```

**Event 32** — triggers: (10,2)/ANY_DIR, (10,4)/ANY_DIR, (12,1)/ANY_DIR

```hex
2b 01 0e ec 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xEC)
02: clear_current_tile_event_flag()
```

**Event 33** — triggers: (11,0)/ANY_DIR, (11,2)/ANY_DIR, (12,3)/ANY_DIR

```hex
2b 01 0e ed 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xED)
02: clear_current_tile_event_flag()
```

**Event 34** — triggers: (12,12)/ANY_DIR, (12,14)/ANY_DIR, (14,12)/ANY_DIR

```hex
2b 01 0e ee 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xEE)
02: clear_current_tile_event_flag()
```

**Event 35** — triggers: (13,1)/ANY_DIR, (13,2)/ANY_DIR, (13,3)/ANY_DIR

```hex
2b 01 0e ef 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xEF)
02: clear_current_tile_event_flag()
```

**Event 36** — triggers: (5,2)/ANY_DIR, (7,2)/ANY_DIR

```hex
2b 01 0e f0 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xF0)
02: clear_current_tile_event_flag()
```

**Event 37** — triggers: (15,6)/ANY_DIR, (15,9)/ANY_DIR

```hex
2b 01 0e f1 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0xF1)
02: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` Black Room
- `[02]` Great Hall
- `[03]` Court Wizard
- `[04]` Throne Room
- `[05]` Corak's Study
- `[06]` Fool's Room
- `[07]` Alchemist
- `[08]` Apprentice Wizards
- `[09]` Hall of Illusions
- `[10]` Fighting Room
- `[11]` Target Room
- `[12]` Temple
- `[13]` Funny Room
- `[14]` Stairs to Dungeon. Descend (y/n)?
- `[15]` Palace exit. Leave (y/n)?
- `[16]` The Bishop of Black Battle is locked / in a cage.
- `[17]` The Bishop of Black Battle is freed / from imprisonment by your black key. / (+10,000 exp). He grants all black / triple crown winners 200,000 exp.
- `[18]` A little princess plays with an / A1-Todilor. Take it (y/n)?
- `[19]` The palace alchemist chamber contains / piles of gold made for the treasury. / Steal some (y/n)?
- `[20]` *** Backpacks Full ***
- `[21]` Here's the joke of the day:
- `[22]` A sumptuous feast is laid upon the / huge banquet table. Take some (y/n)?
- `[23]` Dust covers everything.
- `[24]` Barracks
- `[25]` Castle Guards intervene!
- `[26]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
