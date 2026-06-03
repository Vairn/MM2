# Location 59 — Castle Xabran

- **event.dat** offset `0x01284F`, length **1428** bytes
- **Map:** map screen **59**; Castle Xabran
- **Record kind:** `standard`
- **Triggers:** 47; **script segments:** 48; **strings:** 47

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,15) | `0x0F` | **2** | 0x60 |
| (2,6) | `0x26` | **3** | DIR_N? |
| (2,9) | `0x29` | **40** | DIR_N? |
| (2,10) | `0x2A` | **41** | DIR_SPECIAL |
| (2,11) | `0x2B` | **42** | DIR_SPECIAL |
| (2,12) | `0x2C` | **43** | DIR_SPECIAL |
| (2,13) | `0x2D` | **44** | DIR_SPECIAL |
| (3,9) | `0x39` | **39** | DIR_N? |
| (4,9) | `0x49` | **38** | DIR_N? |
| (4,13) | `0x4D` | **45** | 0x90 |
| (5,9) | `0x59` | **37** | DIR_N? |
| (6,3) | `0x63` | **9** | DIR_N? |
| (6,4) | `0x64` | **10** | DIR_SPECIAL |
| (6,5) | `0x65` | **32** | DIR_SPECIAL |
| (6,6) | `0x66` | **33** | DIR_SPECIAL |
| (6,7) | `0x67` | **34** | DIR_SPECIAL |
| (6,8) | `0x68` | **35** | DIR_SPECIAL |
| (6,9) | `0x69` | **36** | DIR_SPECIAL |
| (7,3) | `0x73` | **7** | DIR_N? |
| (8,0) | `0x80` | **1** | ALWAYS |
| (9,3) | `0x93` | **6** | ENTER |
| (9,4) | `0x94` | **10** | DIR_SPECIAL |
| (9,5) | `0x95` | **24** | DIR_SPECIAL |
| (9,6) | `0x96` | **25** | DIR_SPECIAL |
| (9,7) | `0x97` | **26** | DIR_SPECIAL |
| (9,8) | `0x98` | **27** | DIR_SPECIAL |
| (9,9) | `0x99` | **28** | DIR_SPECIAL |
| (9,10) | `0x9A` | **29** | DIR_SPECIAL |
| (9,11) | `0x9B` | **30** | DIR_SPECIAL |
| (9,12) | `0x9C` | **31** | DIR_SPECIAL |
| (10,3) | `0xA3` | **8** | ENTER |
| (10,4) | `0xA4` | **10** | DIR_SPECIAL |
| (10,5) | `0xA5` | **12** | DIR_SPECIAL |
| (10,6) | `0xA6` | **13** | DIR_SPECIAL |
| (10,7) | `0xA7` | **14** | DIR_SPECIAL |
| (10,8) | `0xA8` | **15** | DIR_SPECIAL |
| (10,9) | `0xA9` | **16** | DIR_SPECIAL |
| (10,10) | `0xAA` | **17** | DIR_SPECIAL |
| (11,10) | `0xBA` | **18** | ENTER |
| (12,10) | `0xCA` | **19** | ENTER |
| (12,13) | `0xCD` | **46** | 0x30 |
| (13,10) | `0xDA` | **20** | ENTER |
| (14,6) | `0xE6` | **5** | DIR_SPECIAL |
| (14,10) | `0xEA` | **21** | ENTER |
| (14,11) | `0xEB` | **22** | DIR_SPECIAL |
| (14,12) | `0xEC` | **23** | DIR_SPECIAL |
| (15,15) | `0xFF` | **4** | ENTER+SPECIAL |

## Events

**Event 01** — triggers: (8,0)/ALWAYS

```hex
01 01 09 10 01 0f 1a 84 09 0c 0b 8e
```

```
00: show_text_basic(str[1] "Castle exit. Leave (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> store_var8(group=0x84, value=0x09)
03: end_script()
04: store_var8(group=0x84, value=0x09)
05: map_transition(0x0B, 0x8E)
```

**Event 02** — triggers: (0,15)/0x60

```hex
02 02 09 11 05 19 01 e7 00 00 10 02 01 06 07 14 0f
```

```
00: show_text_block(str[2] "Atop an aqua pedestal rests a disc. / Take it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: add_party_entity(0x01, f3a=0xE7, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_basic(str[6] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
08: end_script()
```

**Event 03** — triggers: (2,6)/DIR_N?

```hex
02 03 09 11 05 19 01 ea 00 00 10 02 01 06 07 14 0f
```

```
00: show_text_block(str[3] "Atop a brown pedestal rests a disc. / Take it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: add_party_entity(0x01, f3a=0xEA, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_basic(str[6] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
08: end_script()
```

**Event 04** — triggers: (15,15)/ENTER+SPECIAL

```hex
02 04 09 11 05 19 01 e8 00 00 10 02 01 06 07 14 0f
```

```
00: show_text_block(str[4] "Atop a transparent pedestal rests a / disc. Take it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: add_party_entity(0x01, f3a=0xE8, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_basic(str[6] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
08: end_script()
```

**Event 05** — triggers: (14,6)/DIR_SPECIAL

```hex
02 05 09 11 05 19 01 e9 00 00 10 02 01 06 07 14 0f
```

```
00: show_text_block(str[5] "Atop a red pedestal rests a disc. / Take it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: add_party_entity(0x01, f3a=0xE9, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_basic(str[6] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
08: end_script()
```

**Event 06** — triggers: (9,3)/ENTER

```hex
04 07
```

```
00: show_text_above_door(str[7] "Hall of Spells - Key")
```

**Event 07** — triggers: (7,3)/DIR_N?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Hireling Hall - Key")
```

**Event 08** — triggers: (10,3)/ENTER

```hex
0e f4
```

```
00: exec_selector(0xF4)
```

**Event 09** — triggers: (6,3)/DIR_N?

```hex
0e f5
```

```
00: exec_selector(0xF5)
```

**Event 10** — triggers: (6,4)/DIR_SPECIAL, (9,4)/DIR_SPECIAL, (10,4)/DIR_SPECIAL

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Hall of Spells")
```

**Event 12** — triggers: (10,5)/DIR_SPECIAL

```hex
01 0d 29
```

```
00: show_text_basic(str[13] "C2/3 C3 (1,9)")
01: set_abort_and_exit()
```

**Event 13** — triggers: (10,6)/DIR_SPECIAL

```hex
01 0e 29
```

```
00: show_text_basic(str[14] "C3/6 C2 (11,1)")
01: set_abort_and_exit()
```

**Event 14** — triggers: (10,7)/DIR_SPECIAL

```hex
01 0f 29
```

```
00: show_text_basic(str[15] "C4/2 A1 (8,8)")
01: set_abort_and_exit()
```

**Event 15** — triggers: (10,8)/DIR_SPECIAL

```hex
01 10 29
```

```
00: show_text_basic(str[16] "C5/1 A1 (1,14)")
01: set_abort_and_exit()
```

**Event 16** — triggers: (10,9)/DIR_SPECIAL

```hex
01 11 29
```

```
00: show_text_basic(str[17] "C5/3 B4 (8,1)")
01: set_abort_and_exit()
```

**Event 17** — triggers: (10,10)/DIR_SPECIAL

```hex
01 12 29
```

```
00: show_text_basic(str[18] "C6/1 E4 (8,8)")
01: set_abort_and_exit()
```

**Event 18** — triggers: (11,10)/ENTER

```hex
01 13 29
```

```
00: show_text_basic(str[19] "C6/4 A4 (1,1)")
01: set_abort_and_exit()
```

**Event 19** — triggers: (12,10)/ENTER

```hex
01 14 29
```

```
00: show_text_basic(str[20] "C6/5 A4 (8,8)")
01: set_abort_and_exit()
```

**Event 20** — triggers: (13,10)/ENTER

```hex
01 15 29
```

```
00: show_text_basic(str[21] "C7/1 E4 (14,1)")
01: set_abort_and_exit()
```

**Event 21** — triggers: (14,10)/ENTER

```hex
01 16 29
```

```
00: show_text_basic(str[22] "C8/1 E1 (14,14)")
01: set_abort_and_exit()
```

**Event 22** — triggers: (14,11)/DIR_SPECIAL

```hex
01 17 29
```

```
00: show_text_basic(str[23] "C8/2 E1 (8,8)")
01: set_abort_and_exit()
```

**Event 23** — triggers: (14,12)/DIR_SPECIAL

```hex
01 18 29
```

```
00: show_text_basic(str[24] "C9/1 Druid's Cave (14,14)")
01: set_abort_and_exit()
```

**Event 24** — triggers: (9,5)/DIR_SPECIAL

```hex
01 19 29
```

```
00: show_text_basic(str[25] "C9/2 C1 (5,5) South")
01: set_abort_and_exit()
```

**Event 25** — triggers: (9,6)/DIR_SPECIAL

```hex
01 1a 29
```

```
00: show_text_basic(str[26] "S2/1 Middlegate (10,2)")
01: set_abort_and_exit()
```

**Event 26** — triggers: (9,7)/DIR_SPECIAL

```hex
01 1b 29
```

```
00: show_text_basic(str[27] "S2/6 Corak's Cave (7,11)")
01: set_abort_and_exit()
```

**Event 27** — triggers: (9,8)/DIR_SPECIAL

```hex
01 1c 29
```

```
00: show_text_basic(str[28] "S3/6 Sandsobar (7,4)")
01: set_abort_and_exit()
```

**Event 28** — triggers: (9,9)/DIR_SPECIAL

```hex
01 1d 29
```

```
00: show_text_basic(str[29] "S5/2 C1 (1,8)")
01: set_abort_and_exit()
```

**Event 29** — triggers: (9,10)/DIR_SPECIAL

```hex
01 1e 29
```

```
00: show_text_basic(str[30] "S7/1 A2 (15,11)")
01: set_abort_and_exit()
```

**Event 30** — triggers: (9,11)/DIR_SPECIAL

```hex
01 1f 29
```

```
00: show_text_basic(str[31] "S9/3 D1 (5,6)")
01: set_abort_and_exit()
```

**Event 31** — triggers: (9,12)/DIR_SPECIAL

```hex
01 20 29
```

```
00: show_text_basic(str[32] "S9/4 Gemmaker's Cave (3,3)")
01: set_abort_and_exit()
```

**Event 32** — triggers: (6,5)/DIR_SPECIAL

```hex
01 09 29
```

```
00: show_text_basic(str[9] "Cavern under Middlegate (0,15) 1")
01: set_abort_and_exit()
```

**Event 33** — triggers: (6,6)/DIR_SPECIAL

```hex
01 0a 29
```

```
00: show_text_basic(str[10] "Sandsobar (4,10)5")
01: set_abort_and_exit()
```

**Event 34** — triggers: (6,7)/DIR_SPECIAL

```hex
01 21 29
```

```
00: show_text_basic(str[33] "Vulcania (4,2) 4")
01: set_abort_and_exit()
```

**Event 35** — triggers: (6,8)/DIR_SPECIAL

```hex
01 22 29
```

```
00: show_text_basic(str[34] "Atlantium (0,14) 2")
01: set_abort_and_exit()
```

**Event 36** — triggers: (6,9)/DIR_SPECIAL

```hex
01 23 29
```

```
00: show_text_basic(str[35] "Cavern under Vulcania (1,14) 1")
01: set_abort_and_exit()
```

**Event 37** — triggers: (5,9)/DIR_N?

```hex
01 24 29
```

```
00: show_text_basic(str[36] "Tundara (15,10) 3")
01: set_abort_and_exit()
```

**Event 38** — triggers: (4,9)/DIR_N?

```hex
01 25 29
```

```
00: show_text_basic(str[37] "Castle Hillstone (8,4) 5")
01: set_abort_and_exit()
```

**Event 39** — triggers: (3,9)/DIR_N?

```hex
01 26 29
```

```
00: show_text_basic(str[38] "D1 (14,1) 4")
01: set_abort_and_exit()
```

**Event 40** — triggers: (2,9)/DIR_N?

```hex
01 27 29
```

```
00: show_text_basic(str[39] "B4 (10,1) 2")
01: set_abort_and_exit()
```

**Event 41** — triggers: (2,10)/DIR_SPECIAL

```hex
01 28 29
```

```
00: show_text_basic(str[40] "A3 (8,1) 2")
01: set_abort_and_exit()
```

**Event 42** — triggers: (2,11)/DIR_SPECIAL

```hex
01 29 29
```

```
00: show_text_basic(str[41] "Sarakin's Mine (7,2) 3")
01: set_abort_and_exit()
```

**Event 43** — triggers: (2,12)/DIR_SPECIAL

```hex
01 2a 29
```

```
00: show_text_basic(str[42] "Dawn's Cavern (4,11) 2")
01: set_abort_and_exit()
```

**Event 44** — triggers: (2,13)/DIR_SPECIAL

```hex
01 2b 29
```

```
00: show_text_basic(str[43] "D3 (1,14) 5")
01: set_abort_and_exit()
```

**Event 45** — triggers: (4,13)/0x90

```hex
02 2c 29
```

```
00: show_text_block(str[44] "Red Interleave - one letter, then two, / and finally three letters. Repe")
01: set_abort_and_exit()
```

**Event 46** — triggers: (12,13)/0x30

```hex
0e f6
```

```
00: exec_selector(0xF6)
```

## String table

- `[00]` <EMPTY>
- `[01]` Castle exit. Leave (y/n)?
- `[02]` Atop an aqua pedestal rests a disc. / Take it (y/n)?
- `[03]` Atop a brown pedestal rests a disc. / Take it (y/n)?
- `[04]` Atop a transparent pedestal rests a / disc. Take it (y/n)?
- `[05]` Atop a red pedestal rests a disc. / Take it (y/n)?
- `[06]` *** Backpacks Full ***
- `[07]` Hall of Spells - Key
- `[08]` Hireling Hall - Key
- `[09]` Cavern under Middlegate (0,15) 1
- `[10]` Sandsobar (4,10)5
- `[11]` Hall of Spells
- `[12]` Hireling Hall
- `[13]` C2/3 C3 (1,9)
- `[14]` C3/6 C2 (11,1)
- `[15]` C4/2 A1 (8,8)
- `[16]` C5/1 A1 (1,14)
- `[17]` C5/3 B4 (8,1)
- `[18]` C6/1 E4 (8,8)
- `[19]` C6/4 A4 (1,1)
- `[20]` C6/5 A4 (8,8)
- `[21]` C7/1 E4 (14,1)
- `[22]` C8/1 E1 (14,14)
- `[23]` C8/2 E1 (8,8)
- `[24]` C9/1 Druid's Cave (14,14)
- `[25]` C9/2 C1 (5,5) South
- `[26]` S2/1 Middlegate (10,2)
- `[27]` S2/6 Corak's Cave (7,11)
- `[28]` S3/6 Sandsobar (7,4)
- `[29]` S5/2 C1 (1,8)
- `[30]` S7/1 A2 (15,11)
- `[31]` S9/3 D1 (5,6)
- `[32]` S9/4 Gemmaker's Cave (3,3)
- `[33]` Vulcania (4,2) 4
- `[34]` Atlantium (0,14) 2
- `[35]` Cavern under Vulcania (1,14) 1
- `[36]` Tundara (15,10) 3
- `[37]` Castle Hillstone (8,4) 5
- `[38]` D1 (14,1) 4
- `[39]` B4 (10,1) 2
- `[40]` A3 (8,1) 2
- `[41]` Sarakin's Mine (7,2) 3
- `[42]` Dawn's Cavern (4,11) 2
- `[43]` D3 (1,14) 5
- `[44]` Red Interleave - one letter, then two, / and finally three letters. Repeat. Use / this order: 8-5-2-6-1-3-9-7-4.
- `[45]` Y
- `[46]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
