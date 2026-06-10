# Location 57 — Pinehurst

- **event.dat** offset `0x011D93`, length **1304** bytes
- **Map:** map screen **57**; Pinehurst
- **Record kind:** `standard`
- **Triggers:** 31; **script segments:** 30; **strings:** 29

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **26** | DIR_E? |
| (2,11) | `0x2B` | **16** | 0x60 |
| (2,13) | `0x2D` | **1** | DIR_N? |
| (3,4) | `0x34` | **27** | DIR_W? |
| (3,5) | `0x35` | **23** | DIR_W? |
| (3,13) | `0x3D` | **24** | DIR_N? |
| (5,2) | `0x52` | **19** | DIR_N? |
| (5,10) | `0x5A` | **8** | DIR_W? |
| (5,11) | `0x5B` | **6** | DIR_S? |
| (5,12) | `0x5C` | **5** | DIR_E? |
| (5,14) | `0x5E` | **21** | DIR_S? |
| (6,2) | `0x62` | **20** | ANY_DIR |
| (6,7) | `0x67` | **22** | DIR_S? |
| (6,10) | `0x6A` | **7** | DIR_W? |
| (6,12) | `0x6C` | **4** | DIR_E? |
| (7,2) | `0x72` | **26** | 0xA0 |
| (7,10) | `0x7A` | **5** | DIR_W? |
| (7,12) | `0x7C` | **15** | DIR_W? |
| (7,13) | `0x7D` | **2** | DIR_W? |
| (7,15) | `0x7F` | **17** | DIR_E? |
| (8,2) | `0x82` | **28** | ANY_DIR |
| (8,10) | `0x8A` | **9** | DIR_W? |
| (8,12) | `0x8C` | **15** | DIR_W? |
| (8,13) | `0x8D` | **3** | DIR_W? |
| (8,15) | `0x8F` | **17** | DIR_E? |
| (9,10) | `0x9A` | **10** | DIR_W? |
| (9,12) | `0x9C` | **14** | DIR_E? |
| (10,10) | `0xAA` | **11** | DIR_W? |
| (10,11) | `0xAB` | **12** | DIR_N? |
| (10,12) | `0xAC` | **13** | DIR_E? |
| (10,14) | `0xAE` | **25** | DIR_N? |

## Events

**Event 01** — triggers: (2,13)/DIR_N?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Yellow Room")
```

**Event 02** — triggers: (7,13)/DIR_W?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Left Door")
```

**Event 03** — triggers: (8,13)/DIR_W?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Right Door")
```

**Event 04** — triggers: (6,12)/DIR_E?

```hex
04 04
```

```
00: show_text_above_door(str[4] "Beware")
```

**Event 05** — triggers: (5,12)/DIR_E?, (7,10)/DIR_W?

```hex
04 05
```

```
00: show_text_above_door(str[5] "The")
```

**Event 06** — triggers: (5,11)/DIR_S?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Time")
```

**Event 07** — triggers: (6,10)/DIR_W?

```hex
04 07
```

```
00: show_text_above_door(str[7] "As")
```

**Event 08** — triggers: (5,10)/DIR_W?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Traps")
```

**Event 09** — triggers: (8,10)/DIR_W?

```hex
04 09
```

```
00: show_text_above_door(str[9] "World")
```

**Event 10** — triggers: (9,10)/DIR_W?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Will")
```

**Event 11** — triggers: (10,10)/DIR_W?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "End")
```

**Event 12** — triggers: (10,11)/DIR_N?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "In The")
```

**Event 13** — triggers: (10,12)/DIR_E?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Year")
```

**Event 14** — triggers: (9,12)/DIR_E?

```hex
04 0e
```

```
00: show_text_above_door(str[14] "1000")
```

**Event 15** — triggers: (7,12)/DIR_W?, (8,12)/DIR_W?

```hex
06 0f
```

```
00: show_text_popup_style_b(str[15] "Unmarked / are deadly")
```

**Event 16** — triggers: (2,11)/0x60

```hex
01 10 09 11 01 0c 31 f0 0f
```

```
00: show_text_basic(str[16] "Stairs to Dungeon. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x31, 0xF0)
04: end_script()
```

**Event 17** — triggers: (7,15)/DIR_E?, (8,15)/DIR_E?

```hex
01 11 09 11 01 0c 09 11 0f
```

```
00: show_text_basic(str[17] "Castle Exit. Leave (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x09, 0x11)
04: end_script()
```

**Event 19** — triggers: (5,2)/DIR_N?

```hex
0e cf
```

```
00: exec_selector(0xCF)  # quest_handler_207
```

**Event 20** — triggers: (6,2)/ANY_DIR

```hex
06 1a 07 0d 09 0c 39 4f
```

```
00: show_text_popup_style_b(str[26] "See you / later!")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x39, 0x4F)
```

**Event 21** — triggers: (5,14)/DIR_S?

```hex
06 1a 07 0d 09 0c 39 7b
```

```
00: show_text_popup_style_b(str[26] "See you / later!")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x39, 0x7B)
```

**Event 22** — triggers: (6,7)/DIR_S?

```hex
02 16 09 10 01 0f 19 01 fb 00 00 10 02 02 1b 07 14
```

```
00: show_text_block(str[22] "Nestled under a pile of dust, you see / what was once described to you a")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> add_party_entity(0x01, f3a=0xFB, f40=0x00, f46=0x00)
03: end_script()
04: add_party_entity(0x01, f3a=0xFB, f40=0x00, f46=0x00)
05: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
06: show_text_block(str[27] "*** Backpacks Full ***")
07: wait_for_space()
08: clear_current_tile_event_flag()
```

**Event 23** — triggers: (3,5)/DIR_W?

```hex
04 18
```

```
00: show_text_above_door(str[24] "Throne Room")
```

**Event 24** — triggers: (3,13)/DIR_N?

```hex
0b 05 00 28 00 70 11 2c 02 13 1f 00 31 04 88 13 00 15 01 7c 38 1b 38 11 02 1f 01 31 04 50 c3 00 18 01 7c c7 00 15 02 7c 38 1b 38 11 02 1f 02 31 04 50 c3 00 18 02 7c c7 00 15 03 7c 38 1b 38 11 02 1f 03 31 04 50 c3 00 18 03 7c c7 00 15 04 7c 38 1b 38 11 02 1f 04 31 04 50 c3 00 18 04 7c c7 00 15 05 7c 38 1b 38 11 02 1f 05 31 04 50 c3 00 18 05 7c c7 00 15 06 7c 38 1b 38 11 02 1f 06 31 04 50 c3 00 18 06 7c c7 00 15 07 7c 38 1b 38 11 02 1f 07 31 04 50 c3 00 18 07 7c c7 00 15 08 7c 38 1b 38 11 02 1f 08 31 04 50 c3 00 18 08 7c c7 00 08 14 02 12 08 0f
```

```
00: set_service_context(str[5] "The", mode=0x00)
01: cond = consume_item(item_id=112, name="Yellow Key", probe=0)
02: if not cond: skip_tokens(44)
    # skip -> show_text_block(str[18] "The Bishop of Yellow Battle is locked / in a cage.")
03: show_text_block(str[19] "The Bishop of Yellow Battle is freed / from imprisonment by your yellow ")
04: party_effect(sel=0x00, 31 04 88 13 00)
05: apply_party(count=0x01, op=0x7C, val=0x38)
06: cond = (cond >= 0x38)
07: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x02, op=0x7C, val=0x38)
08: party_effect(sel=0x01, 31 04 50 C3 00)
09: apply_party_masked(count=0x01, set=0x7C, and=0xC7, or=0x00)
10: apply_party(count=0x02, op=0x7C, val=0x38)
11: cond = (cond >= 0x38)
12: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x03, op=0x7C, val=0x38)
13: party_effect(sel=0x02, 31 04 50 C3 00)
14: apply_party_masked(count=0x02, set=0x7C, and=0xC7, or=0x00)
15: apply_party(count=0x03, op=0x7C, val=0x38)
16: cond = (cond >= 0x38)
17: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x04, op=0x7C, val=0x38)
18: party_effect(sel=0x03, 31 04 50 C3 00)
19: apply_party_masked(count=0x03, set=0x7C, and=0xC7, or=0x00)
20: apply_party(count=0x04, op=0x7C, val=0x38)
21: cond = (cond >= 0x38)
22: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x05, op=0x7C, val=0x38)
23: party_effect(sel=0x04, 31 04 50 C3 00)
24: apply_party_masked(count=0x04, set=0x7C, and=0xC7, or=0x00)
25: apply_party(count=0x05, op=0x7C, val=0x38)
26: cond = (cond >= 0x38)
27: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x06, op=0x7C, val=0x38)
28: party_effect(sel=0x05, 31 04 50 C3 00)
29: apply_party_masked(count=0x05, set=0x7C, and=0xC7, or=0x00)
30: apply_party(count=0x06, op=0x7C, val=0x38)
31: cond = (cond >= 0x38)
32: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x07, op=0x7C, val=0x38)
33: party_effect(sel=0x06, 31 04 50 C3 00)
34: apply_party_masked(count=0x06, set=0x7C, and=0xC7, or=0x00)
35: apply_party(count=0x07, op=0x7C, val=0x38)
36: cond = (cond >= 0x38)
37: if not cond: skip_tokens(2)
    # skip -> apply_party(count=0x08, op=0x7C, val=0x38)
38: party_effect(sel=0x07, 31 04 50 C3 00)
39: apply_party_masked(count=0x07, set=0x7C, and=0xC7, or=0x00)
40: apply_party(count=0x08, op=0x7C, val=0x38)
41: cond = (cond >= 0x38)
42: if not cond: skip_tokens(2)
    # skip -> wait_key()
43: party_effect(sel=0x08, 31 04 50 C3 00)
44: apply_party_masked(count=0x08, set=0x7C, and=0xC7, or=0x00)
45: wait_key()
46: clear_current_tile_event_flag()
47: show_text_block(str[18] "The Bishop of Yellow Battle is locked / in a cage.")
48: wait_key()
49: end_script()
```

**Event 25** — triggers: (10,14)/DIR_N?

```hex
05 19
```

```
00: show_text_popup_style_a(str[25] "Find my mirror image / and you will find / the way out.")
```

**Event 26** — triggers: (0,2)/DIR_E?, (7,2)/0xA0

```hex
03 17 2c 01 1f 00 2f 01 01 00 00 07 0f
```

```
00: show_text(str[23] "Suddenly your party is moving very / slowly, yet everything around them ")
01: adjust_state(+0x01)
02: party_effect(sel=0x00, 2F 01 01 00 00)
03: wait_for_space()
04: end_script()
```

**Event 27** — triggers: (3,4)/DIR_W?

```hex
0b 0e 00 0e f3
```

```
00: set_service_context(str[14] "1000", mode=0x00)
01: exec_selector(0xF3)
```

**Event 28** — triggers: (8,2)/ANY_DIR

```hex
02 14 29
```

```
00: show_text_block(str[20] "To win the Queen's Triple Crown, take / a Black Ticket to the Arena, Mon")
01: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` Yellow Room
- `[02]` Left Door
- `[03]` Right Door
- `[04]` Beware
- `[05]` The    
- `[06]` Time
- `[07]` As  
- `[08]` Traps
- `[09]` World
- `[10]` Will
- `[11]` End  
- `[12]` In The
- `[13]` Year
- `[14]` 1000
- `[15]` Unmarked / are deadly
- `[16]` Stairs to Dungeon. Descend (y/n)?
- `[17]` Castle Exit. Leave (y/n)?
- `[18]` The Bishop of Yellow Battle is locked / in a cage.
- `[19]` The Bishop of Yellow Battle is freed / from imprisonment by your yellow key / (+5000 exp). He grants all Yellow / Triple Crown winners 50,000 exp.
- `[20]` To win the Queen's Triple Crown, take / a Black Ticket to the Arena, Monster / Bowl, and Colosseum. After victory has / been achieved, return for your reward.
- `[21]` I
- `[22]` Nestled under a pile of dust, you see / what was once described to you as a / "J-26 Fluxer". Take it (y/n)?
- `[23]` Suddenly your party is moving very / slowly, yet everything around them is / moving very fast. By the time you / notice what has happened, a year has / passed.  
- `[24]` Throne Room
- `[25]` Find my mirror image / and you will find / the way out.
- `[26]` See you / later!
- `[27]` *** Backpacks Full ***
- `[28]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
