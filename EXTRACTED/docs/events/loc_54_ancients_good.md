# Location 54 — Ancients (Good)

- **event.dat** offset `0x010CA3`, length **1468** bytes
- **Map:** map screen **54**; Ancients (Good)
- **Record kind:** `standard`
- **Triggers:** 80; **script segments:** 45; **strings:** 48

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,1) | `0x11` | **8** | DIR_N? |
| (2,1) | `0x21` | **32** | DIR_E? |
| (2,4) | `0x24` | **42** | DIR_N? |
| (2,6) | `0x26` | **22** | DIR_E? |
| (2,8) | `0x28` | **22** | DIR_E? |
| (2,12) | `0x2C` | **9** | DIR_N? |
| (3,1) | `0x31` | **10** | DIR_N? |
| (3,2) | `0x32` | **11** | DIR_N? |
| (3,5) | `0x35` | **38** | DIR_N? |
| (3,6) | `0x36` | **23** | DIR_E? |
| (3,8) | `0x38` | **23** | DIR_E? |
| (3,12) | `0x3C` | **3** | DIR_N? |
| (4,3) | `0x43` | **33** | DIR_E? |
| (4,6) | `0x46` | **24** | DIR_E? |
| (4,8) | `0x48` | **24** | DIR_E? |
| (5,1) | `0x51` | **10** | DIR_N? |
| (5,2) | `0x52` | **11** | DIR_N? |
| (5,3) | `0x53` | **12** | DIR_N? |
| (5,4) | `0x54` | **13** | DIR_N? |
| (5,7) | `0x57` | **39** | DIR_N? |
| (5,8) | `0x58` | **25** | DIR_E? |
| (5,11) | `0x5B` | **4** | DIR_N? |
| (5,12) | `0x5C` | **6** | DIR_N? |
| (5,13) | `0x5D` | **5** | DIR_N? |
| (6,5) | `0x65` | **34** | DIR_E? |
| (6,8) | `0x68` | **26** | DIR_E? |
| (6,10) | `0x6A` | **26** | DIR_E? |
| (6,12) | `0x6C` | **26** | DIR_E? |
| (7,1) | `0x71` | **10** | DIR_N? |
| (7,2) | `0x72` | **11** | DIR_N? |
| (7,3) | `0x73` | **12** | DIR_N? |
| (7,4) | `0x74` | **13** | DIR_N? |
| (7,5) | `0x75` | **14** | DIR_N? |
| (7,6) | `0x76` | **15** | DIR_N? |
| (7,9) | `0x79` | **40** | DIR_N? |
| (7,10) | `0x7A` | **27** | DIR_E? |
| (7,12) | `0x7C` | **27** | DIR_E? |
| (8,7) | `0x87` | **35** | DIR_E? |
| (8,10) | `0x8A` | **28** | DIR_E? |
| (8,12) | `0x8C` | **28** | DIR_E? |
| (9,1) | `0x91` | **10** | DIR_N? |
| (9,2) | `0x92` | **11** | DIR_N? |
| (9,3) | `0x93` | **12** | DIR_N? |
| (9,4) | `0x94` | **13** | DIR_N? |
| (9,5) | `0x95` | **14** | DIR_N? |
| (9,6) | `0x96` | **15** | DIR_N? |
| (9,7) | `0x97` | **16** | DIR_N? |
| (9,8) | `0x98` | **17** | DIR_N? |
| (9,11) | `0x9B` | **41** | DIR_N? |
| (9,12) | `0x9C` | **29** | DIR_E? |
| (10,9) | `0xA9` | **36** | DIR_E? |
| (10,12) | `0xAC` | **30** | DIR_E? |
| (11,1) | `0xB1` | **10** | DIR_N? |
| (11,2) | `0xB2` | **11** | DIR_N? |
| (11,3) | `0xB3` | **12** | DIR_N? |
| (11,4) | `0xB4` | **13** | DIR_N? |
| (11,5) | `0xB5` | **14** | DIR_N? |
| (11,6) | `0xB6` | **15** | DIR_N? |
| (11,7) | `0xB7` | **16** | DIR_N? |
| (11,8) | `0xB8` | **17** | DIR_N? |
| (11,9) | `0xB9` | **18** | DIR_N? |
| (11,10) | `0xBA` | **19** | DIR_N? |
| (11,14) | `0xBE` | **43** | ANY_DIR |
| (12,11) | `0xCB` | **37** | DIR_E? |
| (13,1) | `0xD1` | **10** | DIR_N? |
| (13,2) | `0xD2` | **11** | DIR_N? |
| (13,3) | `0xD3` | **12** | DIR_N? |
| (13,4) | `0xD4` | **13** | DIR_N? |
| (13,5) | `0xD5` | **14** | DIR_N? |
| (13,6) | `0xD6` | **15** | DIR_N? |
| (13,7) | `0xD7` | **16** | DIR_N? |
| (13,8) | `0xD8` | **17** | DIR_N? |
| (13,9) | `0xD9` | **18** | DIR_N? |
| (13,10) | `0xDA` | **19** | DIR_N? |
| (13,11) | `0xDB` | **20** | DIR_N? |
| (13,12) | `0xDC` | **21** | DIR_N? |
| (13,15) | `0xDF` | **2** | DIR_S? |
| (14,15) | `0xEF` | **7** | DIR_S? |
| (15,14) | `0xFE` | **31** | DIR_W? |
| (15,15) | `0xFF` | **1** | DIR_N? |

## Events

**Event 01** — triggers: (15,15)/DIR_N?

```hex
02 01 09 11 01 0c 10 a4 0f
```

```
00: show_text_block(str[1] "Stairs lead down to the Isle of the / Ancients. Take them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x10, 0xA4)
04: end_script()
```

**Event 02** — triggers: (13,15)/DIR_S?

```hex
2d 04 05 10 04 01 02 07 0d 09 0c 10 a4 0f
```

```
00: cond = check_member_attr(fields=0x04, value=0x05)
01: if cond: skip_tokens(4)
    # skip -> end_script()
02: show_text_basic(str[2] "Sorcerers only!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x10, 0xA4)
06: end_script()
```

**Event 03** — triggers: (3,12)/DIR_N?

```hex
2d 04 05 11 04 15 00 75 02 10 04 15 00 75 40 10 02 02 03 29 02 28 29
```

```
00: cond = check_member_attr(fields=0x04, value=0x05)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[3] "In the central chamber, locked / in a stasis field, lies / the good wiza")
02: apply_party(count=0x00, op=0x75, val=0x02)
03: if cond: skip_tokens(4)
    # skip -> show_text_block(str[40] "In the central chamber is / an empty stasis field.")
04: apply_party(count=0x00, op=0x75, val=0x40)
05: if cond: skip_tokens(2)
    # skip -> show_text_block(str[40] "In the central chamber is / an empty stasis field.")
06: show_text_block(str[3] "In the central chamber, locked / in a stasis field, lies / the good wiza")
07: set_abort_and_exit()
08: show_text_block(str[40] "In the central chamber is / an empty stasis field.")
09: set_abort_and_exit()
```

**Event 04** — triggers: (5,11)/DIR_N?

```hex
01 29 2f 30 e4 e6 fa fa fa fa fa fa fa fa 10 02 01 2a 29 18 00 75 ef 10 01 2b 07 14
```

```
00: show_text_basic(str[41] "What is the access code?")
01: op_2f_clear_input_buf()
02: cond = check_answer("64")
03: if cond: skip_tokens(2)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xEF, or=0x10)
04: show_text_basic(str[42] "Incorrect!")
05: set_abort_and_exit()
06: apply_party_masked(count=0x00, set=0x75, and=0xEF, or=0x10)
07: show_text_basic(str[43] "Correct!")
08: wait_for_space()
09: clear_current_tile_event_flag()
```

**Event 05** — triggers: (5,13)/DIR_N?

```hex
01 29 2f 30 e7 e8 fa fa fa fa fa fa fa fa 10 02 01 2a 29 18 00 75 df 20 01 2b 07 14
```

```
00: show_text_basic(str[41] "What is the access code?")
01: op_2f_clear_input_buf()
02: cond = check_answer("32")
03: if cond: skip_tokens(2)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xDF, or=0x20)
04: show_text_basic(str[42] "Incorrect!")
05: set_abort_and_exit()
06: apply_party_masked(count=0x00, set=0x75, and=0xDF, or=0x20)
07: show_text_basic(str[43] "Correct!")
08: wait_for_space()
09: clear_current_tile_event_flag()
```

**Event 06** — triggers: (5,12)/DIR_N?

```hex
2d 04 05 11 09 15 00 75 02 10 02 15 00 75 40 11 02 02 28 29 15 00 75 30 1b 30 10 02 02 03 29 01 2c 15 00 75 80 10 03 02 2d 18 00 75 8f 40 29 18 00 75 00 03 02 2e 29
```

```
00: cond = check_member_attr(fields=0x04, value=0x05)
01: if not cond: skip_tokens(9)
    # skip -> show_text_block(str[3] "In the central chamber, locked / in a stasis field, lies / the good wiza")
02: apply_party(count=0x00, op=0x75, val=0x02)
03: if cond: skip_tokens(2)
    # skip -> show_text_block(str[40] "In the central chamber is / an empty stasis field.")
04: apply_party(count=0x00, op=0x75, val=0x40)
05: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x00, op=0x75, val=0x30)
06: show_text_block(str[40] "In the central chamber is / an empty stasis field.")
07: set_abort_and_exit()
08: apply_party(count=0x00, op=0x75, val=0x30)
09: cond = (cond >= 0x30)
10: if cond: skip_tokens(2)
    # skip -> show_text_basic(str[44] "The field deactivates and Yekop rises.")
11: show_text_block(str[3] "In the central chamber, locked / in a stasis field, lies / the good wiza")
12: set_abort_and_exit()
13: show_text_basic(str[44] "The field deactivates and Yekop rises.")
14: apply_party(count=0x00, op=0x75, val=0x80)
15: if cond: skip_tokens(3)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0x00, or=0x03)
16: show_text_block(str[45] ""Equilibrium is essential. You must / free my counterpart, Ybmug, / and ")
17: apply_party_masked(count=0x00, set=0x75, and=0x8F, or=0x40)
18: set_abort_and_exit()
19: apply_party_masked(count=0x00, set=0x75, and=0x00, or=0x03)
20: show_text_block(str[46] ""Thank you for freeing Ybmug and me. / Now you must return to the Jurors")
21: set_abort_and_exit()
```

**Event 07** — triggers: (14,15)/DIR_S?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Sorcerers only!")
```

**Event 08** — triggers: (1,1)/DIR_N?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Illumination")
```

**Event 09** — triggers: (2,12)/DIR_N?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Death's Doorway")
```

**Event 10** — triggers: (3,1)/DIR_N?, (5,1)/DIR_N?, (7,1)/DIR_N?, (9,1)/DIR_N?, (11,1)/DIR_N?, (13,1)/DIR_N?

```hex
04 07
```

```
00: show_text_above_door(str[7] "Door #1")
```

**Event 11** — triggers: (3,2)/DIR_N?, (5,2)/DIR_N?, (7,2)/DIR_N?, (9,2)/DIR_N?, (11,2)/DIR_N?, (13,2)/DIR_N?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Door #2")
```

**Event 12** — triggers: (5,3)/DIR_N?, (7,3)/DIR_N?, (9,3)/DIR_N?, (11,3)/DIR_N?, (13,3)/DIR_N?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Door #3")
```

**Event 13** — triggers: (5,4)/DIR_N?, (7,4)/DIR_N?, (9,4)/DIR_N?, (11,4)/DIR_N?, (13,4)/DIR_N?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Door #4")
```

**Event 14** — triggers: (7,5)/DIR_N?, (9,5)/DIR_N?, (11,5)/DIR_N?, (13,5)/DIR_N?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Door #5")
```

**Event 15** — triggers: (7,6)/DIR_N?, (9,6)/DIR_N?, (11,6)/DIR_N?, (13,6)/DIR_N?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Door #6")
```

**Event 16** — triggers: (9,7)/DIR_N?, (11,7)/DIR_N?, (13,7)/DIR_N?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Door #7")
```

**Event 17** — triggers: (9,8)/DIR_N?, (11,8)/DIR_N?, (13,8)/DIR_N?

```hex
04 0e
```

```
00: show_text_above_door(str[14] "Door #8")
```

**Event 18** — triggers: (11,9)/DIR_N?, (13,9)/DIR_N?

```hex
04 0f
```

```
00: show_text_above_door(str[15] "Door #9")
```

**Event 19** — triggers: (11,10)/DIR_N?, (13,10)/DIR_N?

```hex
04 10
```

```
00: show_text_above_door(str[16] "Door #10")
```

**Event 20** — triggers: (13,11)/DIR_N?

```hex
04 11
```

```
00: show_text_above_door(str[17] "Door #11")
```

**Event 21** — triggers: (13,12)/DIR_N?

```hex
04 12
```

```
00: show_text_above_door(str[18] "Door #12")
```

**Event 22** — triggers: (2,6)/DIR_E?, (2,8)/DIR_E?

```hex
04 13
```

```
00: show_text_above_door(str[19] "Door A")
```

**Event 23** — triggers: (3,6)/DIR_E?, (3,8)/DIR_E?

```hex
04 14
```

```
00: show_text_above_door(str[20] "Door B")
```

**Event 24** — triggers: (4,6)/DIR_E?, (4,8)/DIR_E?

```hex
04 15
```

```
00: show_text_above_door(str[21] "Door C")
```

**Event 25** — triggers: (5,8)/DIR_E?

```hex
04 16
```

```
00: show_text_above_door(str[22] "Door D")
```

**Event 26** — triggers: (6,8)/DIR_E?, (6,10)/DIR_E?, (6,12)/DIR_E?

```hex
04 17
```

```
00: show_text_above_door(str[23] "Door E")
```

**Event 27** — triggers: (7,10)/DIR_E?, (7,12)/DIR_E?

```hex
04 18
```

```
00: show_text_above_door(str[24] "Door F")
```

**Event 28** — triggers: (8,10)/DIR_E?, (8,12)/DIR_E?

```hex
04 19
```

```
00: show_text_above_door(str[25] "Door G")
```

**Event 29** — triggers: (9,12)/DIR_E?

```hex
04 1a
```

```
00: show_text_above_door(str[26] "Door H")
```

**Event 30** — triggers: (10,12)/DIR_E?

```hex
04 1b
```

```
00: show_text_above_door(str[27] "Door I")
```

**Event 31** — triggers: (15,14)/DIR_W?

```hex
05 1c
```

```
00: show_text_popup_style_a(str[28] "All that are odd / are more but even / All that are even / become less e")
```

**Event 32** — triggers: (2,1)/DIR_E?

```hex
05 1d
```

```
00: show_text_popup_style_a(str[29] "This one's easy")
```

**Event 33** — triggers: (4,3)/DIR_E?

```hex
05 1e
```

```
00: show_text_popup_style_a(str[30] "Rhymes with door")
```

**Event 34** — triggers: (6,5)/DIR_E?

```hex
05 1f
```

```
00: show_text_popup_style_a(str[31] "The sick sheik's / sixth sheep ...")
```

**Event 35** — triggers: (8,7)/DIR_E?

```hex
05 20
```

```
00: show_text_popup_style_a(str[32] "Twice the first / two's double")
```

**Event 36** — triggers: (10,9)/DIR_E?

```hex
05 21
```

```
00: show_text_popup_style_a(str[33] "Sides of a pentagram")
```

**Event 37** — triggers: (12,11)/DIR_E?

```hex
05 22
```

```
00: show_text_popup_style_a(str[34] "Almost a dozen")
```

**Event 38** — triggers: (3,5)/DIR_N?

```hex
05 23
```

```
00: show_text_popup_style_a(str[35] "It's Z end")
```

**Event 39** — triggers: (5,7)/DIR_N?

```hex
05 24
```

```
00: show_text_popup_style_a(str[36] "You and you")
```

**Event 40** — triggers: (7,9)/DIR_N?

```hex
05 25
```

```
00: show_text_popup_style_a(str[37] "No, just you")
```

**Event 41** — triggers: (9,11)/DIR_N?

```hex
05 26
```

```
00: show_text_popup_style_a(str[38] "The first on / the right")
```

**Event 42** — triggers: (2,4)/DIR_N?

```hex
05 27
```

```
00: show_text_popup_style_a(str[39] "The alphabet is reversed")
```

**Event 43** — triggers: (11,14)/ANY_DIR

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
- `[01]` Stairs lead down to the Isle of the / Ancients. Take them (y/n)?
- `[02]` Sorcerers only!
- `[03]` In the central chamber, locked / in a stasis field, lies / the good wizard Yekop.
- `[04]` S
- `[05]` Illumination
- `[06]` Death's Doorway
- `[07]` Door #1
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
- `[28]` All that are odd / are more but even / All that are even / become less even
- `[29]` This one's easy
- `[30]` Rhymes with door
- `[31]` The sick sheik's / sixth sheep ...
- `[32]` Twice the first / two's double
- `[33]` Sides of a pentagram
- `[34]` Almost a dozen
- `[35]` It's Z end
- `[36]` You and you
- `[37]` No, just you
- `[38]` The first on / the right
- `[39]` The alphabet is reversed
- `[40]` In the central chamber is / an empty stasis field.
- `[41]` What is the access code?
- `[42]` Incorrect!
- `[43]` Correct!
- `[44]` The field deactivates and Yekop rises.
- `[45]` "Equilibrium is essential. You must / free my counterpart, Ybmug, / and return to the Jurors."
- `[46]` "Thank you for freeing Ybmug and me. / Now you must return to the Jurors."
- `[47]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
