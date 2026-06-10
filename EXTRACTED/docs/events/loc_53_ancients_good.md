# Location 53 — Ancients (Good)

- **event.dat** offset `0x0106E7`, length **1468** bytes
- **Map:** map screen **53**; Ancients (Good)
- **Record kind:** `standard`
- **Triggers:** 80; **script segments:** 44; **strings:** 47

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **1** | 0x30 |
| (0,1) | `0x01` | **31** | DIR_E? |
| (1,0) | `0x10` | **7** | DIR_N? |
| (2,0) | `0x20` | **2** | DIR_N? |
| (2,3) | `0x23` | **20** | DIR_S? |
| (2,4) | `0x24` | **21** | DIR_S? |
| (2,5) | `0x25` | **19** | DIR_S? |
| (2,6) | `0x26` | **18** | DIR_S? |
| (2,7) | `0x27` | **17** | DIR_S? |
| (2,8) | `0x28` | **16** | DIR_S? |
| (2,9) | `0x29` | **15** | DIR_S? |
| (2,10) | `0x2A` | **14** | DIR_S? |
| (2,11) | `0x2B` | **13** | DIR_S? |
| (2,12) | `0x2C` | **12** | DIR_S? |
| (2,13) | `0x2D` | **11** | DIR_S? |
| (2,14) | `0x2E` | **10** | DIR_S? |
| (3,4) | `0x34` | **37** | DIR_W? |
| (4,1) | `0x41` | **42** | ANY_DIR |
| (4,5) | `0x45` | **19** | DIR_S? |
| (4,6) | `0x46` | **18** | DIR_S? |
| (4,7) | `0x47` | **17** | DIR_S? |
| (4,8) | `0x48` | **16** | DIR_S? |
| (4,9) | `0x49` | **15** | DIR_S? |
| (4,10) | `0x4A` | **14** | DIR_S? |
| (4,11) | `0x4B` | **13** | DIR_S? |
| (4,12) | `0x4C` | **12** | DIR_S? |
| (4,13) | `0x4D` | **11** | DIR_S? |
| (4,14) | `0x4E` | **10** | DIR_S? |
| (5,3) | `0x53` | **30** | DIR_W? |
| (5,6) | `0x56` | **36** | DIR_W? |
| (6,3) | `0x63` | **29** | DIR_W? |
| (6,4) | `0x64` | **41** | DIR_S? |
| (6,7) | `0x67` | **17** | DIR_S? |
| (6,8) | `0x68` | **16** | DIR_S? |
| (6,9) | `0x69` | **15** | DIR_S? |
| (6,10) | `0x6A` | **14** | DIR_S? |
| (6,11) | `0x6B` | **13** | DIR_S? |
| (6,12) | `0x6C` | **12** | DIR_S? |
| (6,13) | `0x6D` | **11** | DIR_S? |
| (6,14) | `0x6E` | **10** | DIR_S? |
| (7,3) | `0x73` | **28** | DIR_W? |
| (7,5) | `0x75` | **28** | DIR_W? |
| (7,8) | `0x78` | **35** | DIR_W? |
| (8,3) | `0x83` | **27** | DIR_W? |
| (8,5) | `0x85` | **27** | DIR_W? |
| (8,6) | `0x86` | **40** | DIR_S? |
| (8,9) | `0x89` | **15** | DIR_S? |
| (8,10) | `0x8A` | **14** | DIR_S? |
| (8,11) | `0x8B` | **13** | DIR_S? |
| (8,12) | `0x8C` | **12** | DIR_S? |
| (8,13) | `0x8D` | **11** | DIR_S? |
| (8,14) | `0x8E` | **10** | DIR_S? |
| (9,3) | `0x93` | **26** | DIR_W? |
| (9,5) | `0x95` | **26** | DIR_W? |
| (9,7) | `0x97` | **26** | DIR_W? |
| (9,10) | `0x9A` | **34** | DIR_W? |
| (10,2) | `0xA2` | **4** | DIR_S? |
| (10,3) | `0xA3` | **6** | DIR_S? |
| (10,4) | `0xA4` | **5** | DIR_S? |
| (10,7) | `0xA7` | **25** | DIR_W? |
| (10,8) | `0xA8` | **39** | DIR_S? |
| (10,11) | `0xAB` | **13** | DIR_S? |
| (10,12) | `0xAC` | **12** | DIR_S? |
| (10,13) | `0xAD` | **11** | DIR_S? |
| (10,14) | `0xAE` | **10** | DIR_S? |
| (11,7) | `0xB7` | **24** | DIR_W? |
| (11,9) | `0xB9` | **24** | DIR_W? |
| (11,12) | `0xBC` | **33** | DIR_W? |
| (12,3) | `0xC3` | **3** | DIR_S? |
| (12,7) | `0xC7` | **23** | DIR_W? |
| (12,9) | `0xC9` | **23** | DIR_W? |
| (12,10) | `0xCA` | **39** | DIR_S? |
| (12,13) | `0xCD` | **11** | DIR_S? |
| (12,14) | `0xCE` | **10** | DIR_S? |
| (13,3) | `0xD3` | **9** | DIR_S? |
| (13,7) | `0xD7` | **22** | DIR_W? |
| (13,9) | `0xD9` | **22** | DIR_W? |
| (13,11) | `0xDB` | **38** | DIR_S? |
| (13,14) | `0xDE` | **32** | DIR_W? |
| (14,14) | `0xEE` | **8** | DIR_S? |

## Events

**Event 01** — triggers: (0,0)/0x30

```hex
02 01 09 11 01 0c 0d 44 0f
```

```
00: show_text_block(str[1] "Stairs lead back to the Isle of / the Ancients. Follow (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x0D, 0x44)
04: end_script()
```

**Event 02** — triggers: (2,0)/DIR_N?

```hex
2d 04 05 10 04 01 05 07 0d 09 0c 0d 44 0f
```

```
00: cond = check_member_attr(fields=0x04, value=0x05)
01: if cond: skip_tokens(4)
    # skip -> end_script()
02: show_text_basic(str[5] "Sorcerers Only!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x0D, 0x44)
06: end_script()
```

**Event 03** — triggers: (12,3)/DIR_S?

```hex
2d 04 05 11 04 15 00 75 02 10 04 15 00 75 80 10 02 02 04 29 02 27 29
```

```
00: cond = check_member_attr(fields=0x04, value=0x05)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[4] "In the central chamber, locked / in a stasis field, lies the / evil wiza")
02: apply_party(count=0x00, op=0x75, val=0x02)
03: if cond: skip_tokens(4)
    # skip -> show_text_block(str[39] "In the central chamber is an / empty stasis field.")
04: apply_party(count=0x00, op=0x75, val=0x80)
05: if cond: skip_tokens(2)
    # skip -> show_text_block(str[39] "In the central chamber is an / empty stasis field.")
06: show_text_block(str[4] "In the central chamber, locked / in a stasis field, lies the / evil wiza")
07: set_abort_and_exit()
08: show_text_block(str[39] "In the central chamber is an / empty stasis field.")
09: set_abort_and_exit()
```

**Event 04** — triggers: (10,2)/DIR_S?

```hex
01 28 2f 30 e6 e4 fa fa fa fa fa fa fa fa 10 02 01 29 29 18 00 75 fb 04 01 2a 07 14
```

```
00: show_text_basic(str[40] "What is the access code?")
01: op_2f_clear_input_buf()
02: cond = check_answer("46")
03: if cond: skip_tokens(2)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xFB, or=0x04)
04: show_text_basic(str[41] "Incorrect!")
05: set_abort_and_exit()
06: apply_party_masked(count=0x00, set=0x75, and=0xFB, or=0x04)
07: show_text_basic(str[42] "Correct!")
08: wait_for_space()
09: clear_current_tile_event_flag()
```

**Event 05** — triggers: (10,4)/DIR_S?

```hex
01 28 2f 30 e8 e7 fa fa fa fa fa fa fa fa 10 02 01 29 29 18 00 75 f7 08 01 2a 07 14
```

```
00: show_text_basic(str[40] "What is the access code?")
01: op_2f_clear_input_buf()
02: cond = check_answer("23")
03: if cond: skip_tokens(2)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xF7, or=0x08)
04: show_text_basic(str[41] "Incorrect!")
05: set_abort_and_exit()
06: apply_party_masked(count=0x00, set=0x75, and=0xF7, or=0x08)
07: show_text_basic(str[42] "Correct!")
08: wait_for_space()
09: clear_current_tile_event_flag()
```

**Event 06** — triggers: (10,3)/DIR_S?

```hex
2d 04 05 11 09 15 00 75 02 10 02 15 00 75 80 11 02 02 27 29 15 00 75 0c 1b 0c 10 02 02 04 29 01 2b 15 00 75 40 10 03 02 2c 18 00 75 73 80 29 02 2d 18 00 75 00 03 29
```

```
00: cond = check_member_attr(fields=0x04, value=0x05)
01: if not cond: skip_tokens(9)
    # skip -> show_text_block(str[4] "In the central chamber, locked / in a stasis field, lies the / evil wiza")
02: apply_party(count=0x00, op=0x75, val=0x02)
03: if cond: skip_tokens(2)
    # skip -> show_text_block(str[39] "In the central chamber is an / empty stasis field.")
04: apply_party(count=0x00, op=0x75, val=0x80)
05: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x00, op=0x75, val=0x0C)
06: show_text_block(str[39] "In the central chamber is an / empty stasis field.")
07: set_abort_and_exit()
08: apply_party(count=0x00, op=0x75, val=0x0C)
09: cond = (cond >= 0x0C)
10: if cond: skip_tokens(2)
    # skip -> show_text_basic(str[43] "The field deactivates and Ybmug rises.")
11: show_text_block(str[4] "In the central chamber, locked / in a stasis field, lies the / evil wiza")
12: set_abort_and_exit()
13: show_text_basic(str[43] "The field deactivates and Ybmug rises.")
14: apply_party(count=0x00, op=0x75, val=0x40)
15: if cond: skip_tokens(3)
    # skip -> show_text_block(str[45] "Thank you for freeing Yekop and me. / Now you must return to the Jurors.")
16: show_text_block(str[44] ""Equilibrium is essential. You must / free my counterpart, Yekop, and / ")
17: apply_party_masked(count=0x00, set=0x75, and=0x73, or=0x80)
18: set_abort_and_exit()
19: show_text_block(str[45] "Thank you for freeing Yekop and me. / Now you must return to the Jurors.")
20: apply_party_masked(count=0x00, set=0x75, and=0x00, or=0x03)
21: set_abort_and_exit()
```

**Event 07** — triggers: (1,0)/DIR_N?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Sorcerers Only!")
```

**Event 08** — triggers: (14,14)/DIR_S?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Despair")
```

**Event 09** — triggers: (13,3)/DIR_S?

```hex
04 07
```

```
00: show_text_above_door(str[7] "Death's Doorway")
```

**Event 10** — triggers: (2,14)/DIR_S?, (4,14)/DIR_S?, (6,14)/DIR_S?, (8,14)/DIR_S?, (10,14)/DIR_S?, (12,14)/DIR_S?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Door #1")
```

**Event 11** — triggers: (2,13)/DIR_S?, (4,13)/DIR_S?, (6,13)/DIR_S?, (8,13)/DIR_S?, (10,13)/DIR_S?, (12,13)/DIR_S?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Door #2")
```

**Event 12** — triggers: (2,12)/DIR_S?, (4,12)/DIR_S?, (6,12)/DIR_S?, (8,12)/DIR_S?, (10,12)/DIR_S?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Door #3")
```

**Event 13** — triggers: (2,11)/DIR_S?, (4,11)/DIR_S?, (6,11)/DIR_S?, (8,11)/DIR_S?, (10,11)/DIR_S?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Door #4")
```

**Event 14** — triggers: (2,10)/DIR_S?, (4,10)/DIR_S?, (6,10)/DIR_S?, (8,10)/DIR_S?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Door #5")
```

**Event 15** — triggers: (2,9)/DIR_S?, (4,9)/DIR_S?, (6,9)/DIR_S?, (8,9)/DIR_S?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Door #6")
```

**Event 16** — triggers: (2,8)/DIR_S?, (4,8)/DIR_S?, (6,8)/DIR_S?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Door #7")
```

**Event 17** — triggers: (2,7)/DIR_S?, (4,7)/DIR_S?, (6,7)/DIR_S?

```hex
04 0e
```

```
00: show_text_above_door(str[14] "Door #8")
```

**Event 18** — triggers: (2,6)/DIR_S?, (4,6)/DIR_S?

```hex
04 0f
```

```
00: show_text_above_door(str[15] "Door #9")
```

**Event 19** — triggers: (2,5)/DIR_S?, (4,5)/DIR_S?

```hex
04 10
```

```
00: show_text_above_door(str[16] "Door #10")
```

**Event 20** — triggers: (2,3)/DIR_S?

```hex
04 11
```

```
00: show_text_above_door(str[17] "Door #11")
```

**Event 21** — triggers: (2,4)/DIR_S?

```hex
04 12
```

```
00: show_text_above_door(str[18] "Door #12")
```

**Event 22** — triggers: (13,7)/DIR_W?, (13,9)/DIR_W?

```hex
04 13
```

```
00: show_text_above_door(str[19] "Door A")
```

**Event 23** — triggers: (12,7)/DIR_W?, (12,9)/DIR_W?

```hex
04 14
```

```
00: show_text_above_door(str[20] "Door B")
```

**Event 24** — triggers: (11,7)/DIR_W?, (11,9)/DIR_W?

```hex
04 15
```

```
00: show_text_above_door(str[21] "Door C")
```

**Event 25** — triggers: (10,7)/DIR_W?

```hex
04 16
```

```
00: show_text_above_door(str[22] "Door D")
```

**Event 26** — triggers: (9,3)/DIR_W?, (9,5)/DIR_W?, (9,7)/DIR_W?

```hex
04 17
```

```
00: show_text_above_door(str[23] "Door E")
```

**Event 27** — triggers: (8,3)/DIR_W?, (8,5)/DIR_W?

```hex
04 18
```

```
00: show_text_above_door(str[24] "Door F")
```

**Event 28** — triggers: (7,3)/DIR_W?, (7,5)/DIR_W?

```hex
04 19
```

```
00: show_text_above_door(str[25] "Door G")
```

**Event 29** — triggers: (6,3)/DIR_W?

```hex
04 1a
```

```
00: show_text_above_door(str[26] "Door H")
```

**Event 30** — triggers: (5,3)/DIR_W?

```hex
04 1b
```

```
00: show_text_above_door(str[27] "Door I")
```

**Event 31** — triggers: (0,1)/DIR_E?

```hex
05 1c
```

```
00: show_text_popup_style_a(str[28] "All that are even / are less but odd / All that are odd / become more od")
```

**Event 32** — triggers: (13,14)/DIR_W?

```hex
05 1d
```

```
00: show_text_popup_style_a(str[29] "It's not one")
```

**Event 33** — triggers: (11,12)/DIR_W?

```hex
05 1e
```

```
00: show_text_popup_style_a(str[30] "It is one")
```

**Event 34** — triggers: (9,10)/DIR_W?

```hex
05 1f
```

```
00: show_text_popup_style_a(str[31] "It's larger than one / but less than / a crowd")
```

**Event 35** — triggers: (7,8)/DIR_W?

```hex
05 20
```

```
00: show_text_popup_style_a(str[32] "Octal is the one")
```

**Event 36** — triggers: (5,6)/DIR_W?

```hex
05 21
```

```
00: show_text_popup_style_a(str[33] "The largest prime")
```

**Event 37** — triggers: (3,4)/DIR_W?

```hex
05 22
```

```
00: show_text_popup_style_a(str[34] "Biggest is the best")
```

**Event 38** — triggers: (13,11)/DIR_S?

```hex
05 23
```

```
00: show_text_popup_style_a(str[35] "The lettered doors / should be read / in reverse")
```

**Event 39** — triggers: (10,8)/DIR_S?, (12,10)/DIR_S?

```hex
05 24
```

```
00: show_text_popup_style_a(str[36] "Do you see the light")
```

**Event 40** — triggers: (8,6)/DIR_S?

```hex
05 25
```

```
00: show_text_popup_style_a(str[37] "Vowels are winners")
```

**Event 41** — triggers: (6,4)/DIR_S?

```hex
05 26
```

```
00: show_text_popup_style_a(str[38] "The first letter / in eye")
```

**Event 42** — triggers: (4,1)/ANY_DIR

```hex
2b 01 12 6d 6d 6d 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6D 6D 6D 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs lead back to the Isle of / the Ancients. Follow (y/n)?
- `[02]` S
- `[03]` Door #1
- `[04]` In the central chamber, locked / in a stasis field, lies the / evil wizard Ybmug.
- `[05]` Sorcerers Only!
- `[06]` Despair
- `[07]` Death's Doorway
- `[08]` Door #2
- `[09]` Door #3
- `[10]` Door #4
- `[11]` Door #5
- `[12]` Door #6
- `[13]` Door #7
- `[14]` Door #8
- `[15]` Door #9
- `[16]` Door #10
- `[17]` Door #11
- `[18]` Door #12
- `[19]` Door A
- `[20]` Door B
- `[21]` Door C
- `[22]` Door D
- `[23]` Door E
- `[24]` Door F
- `[25]` Door G
- `[26]` Door H
- `[27]` Door I
- `[28]` All that are even / are less but odd / All that are odd / become more odd
- `[29]` It's not one
- `[30]` It is one
- `[31]` It's larger than one / but less than / a crowd
- `[32]` Octal is the one
- `[33]` The largest prime
- `[34]` Biggest is the best
- `[35]` The lettered doors / should be read / in reverse
- `[36]` Do you see the light
- `[37]` Vowels are winners
- `[38]` The first letter / in eye
- `[39]` In the central chamber is an / empty stasis field.
- `[40]` What is the access code?
- `[41]` Incorrect!
- `[42]` Correct!
- `[43]` The field deactivates and Ybmug rises.
- `[44]` "Equilibrium is essential. You must / free my counterpart, Yekop, and / return to the Jurors."
- `[45]` Thank you for freeing Yekop and me. / Now you must return to the Jurors.
- `[46]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
