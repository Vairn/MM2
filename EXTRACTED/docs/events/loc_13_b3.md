# Location 13 — B3

- **event.dat** offset `0x0049A4`, length **1468** bytes
- **Map:** map screen **13**; overland sector **B3**
- **Record kind:** `standard`
- **Triggers:** 40; **script segments:** 35; **strings:** 19

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,7) | `0x07` | **28** | DIR_W? |
| (1,7) | `0x17` | **27** | DIR_W? |
| (1,13) | `0x1D` | **29** | ANY_DIR |
| (1,15) | `0x1F` | **31** | ANY_DIR |
| (2,4) | `0x24` | **6** | DIR_N? |
| (2,7) | `0x27` | **26** | DIR_W? |
| (2,12) | `0x2C` | **32** | 0x30 |
| (3,7) | `0x37` | **25** | DIR_W? |
| (3,13) | `0x3D` | **5** | ANY_DIR |
| (4,4) | `0x44` | **1** | DIR_N? |
| (4,7) | `0x47` | **24** | DIR_W? |
| (4,15) | `0x4F` | **4** | ANY_DIR |
| (5,7) | `0x57` | **23** | DIR_W? |
| (5,8) | `0x58` | **22** | DIR_W? |
| (5,9) | `0x59` | **21** | DIR_W? |
| (5,10) | `0x5A` | **20** | DIR_W? |
| (5,12) | `0x5C` | **5** | ANY_DIR |
| (6,10) | `0x6A` | **19** | DIR_W? |
| (6,14) | `0x6E` | **3** | ANY_DIR |
| (7,10) | `0x7A` | **18** | DIR_W? |
| (7,12) | `0x7C` | **5** | ANY_DIR |
| (8,10) | `0x8A` | **17** | DIR_W? |
| (8,14) | `0x8E` | **5** | ANY_DIR |
| (8,15) | `0x8F` | **2** | ANY_DIR |
| (9,10) | `0x9A` | **16** | DIR_W? |
| (9,12) | `0x9C` | **33** | 0x90 |
| (10,2) | `0xA2` | **30** | ANY_DIR |
| (10,10) | `0xAA` | **15** | DIR_W? |
| (10,13) | `0xAD` | **5** | ANY_DIR |
| (10,15) | `0xAF` | **30** | ANY_DIR |
| (11,5) | `0xB5` | **7** | ANY_DIR |
| (11,10) | `0xBA` | **14** | DIR_W? |
| (11,11) | `0xBB` | **13** | DIR_W? |
| (11,12) | `0xBC` | **12** | DIR_W? |
| (11,13) | `0xBD` | **11** | DIR_W? |
| (11,14) | `0xBE` | **10** | DIR_W? |
| (11,15) | `0xBF` | **9** | DIR_W? |
| (13,10) | `0xDA` | **29** | ANY_DIR |
| (14,5) | `0xE5` | **8** | 0x50 |
| (14,7) | `0xE7` | **31** | ANY_DIR |

## Events

**Event 01** — triggers: (4,4)/DIR_N?

```hex
0b 07 00 02 01 0a 10 01 0f 2d 04 05 11 01 0c 35 00 02 02 08 14
```

```
00: set_service_context(str[7] "A knight proclaims, / "None shall pass!" Attack (y/n)?", mode=0x00)
01: show_text_block(str[1] "Upon heavy wooden doors of an ancient / fortress is scrawled: Sorcerers ")
02: cond = prompt_yes_no(mode=1)
03: if cond: skip_tokens(1)
    # skip -> cond = check_member_attr(fields=0x04, value=0x05)
04: end_script()
05: cond = check_member_attr(fields=0x04, value=0x05)
06: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[2] "A strange force repels your party.")
07: map_transition(0x35, 0x00)
08: show_text_block(str[2] "A strange force repels your party.")
09: wait_key()
10: clear_current_tile_event_flag()
```

**Event 02** — triggers: (8,15)/ANY_DIR

```hex
01 03 29
```

```
00: show_text_basic(str[3] "Etched onto a huge stone is a "U".")
01: set_abort_and_exit()
```

**Event 03** — triggers: (6,14)/ANY_DIR

```hex
01 04 29
```

```
00: show_text_basic(str[4] "Etched onto a huge stone is an "R".")
01: set_abort_and_exit()
```

**Event 04** — triggers: (4,15)/ANY_DIR

```hex
01 05 29
```

```
00: show_text_basic(str[5] "Etched onto a huge stone is a "D".")
01: set_abort_and_exit()
```

**Event 05** — triggers: (3,13)/ANY_DIR, (5,12)/ANY_DIR, (7,12)/ANY_DIR, (8,14)/ANY_DIR, (10,13)/ANY_DIR

```hex
2b 01 12 4a 4a 4a 4a 4a 4a 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=4A 4A 4A 4A 4A 4A 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 06** — triggers: (2,4)/DIR_N?

```hex
06 06
```

```
00: show_text_popup_style_b(str[6] "Evil / Zone")
```

**Event 07** — triggers: (11,5)/ANY_DIR

```hex
2b 05 02 07 09 10 01 0c 0d ef 12 c9 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(5)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[7] "A knight proclaims, / "None shall pass!" Attack (y/n)?")
02: cond = prompt_yes_no()
03: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=C9 00 00 00 00 00 00 00 00 00, flags=00 00)
04: map_transition(0x0D, 0xEF)
05: encounter_setup(monsters=C9 00 00 00 00 00 00 00 00 00, flags=00 00)
06: clear_current_tile_event_flag()
```

**Event 08** — triggers: (14,5)/0x50

```hex
2b 09 2d 00 05 10 02 02 08 29 02 09 07 02 0a 07 12 ef 18 00 00 00 00 00 00 00 00 00 00 18 00 75 fe 01 02 11 07 14
```

```
00: skip_tokens(9)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
01: cond = check_member_attr(fields=0x00, value=0x05)
02: if cond: skip_tokens(2)
    # skip -> show_text_block(str[9] "A flurry of motion pervades Jouster's / Way. Banners fly and trumpets bl")
03: show_text_block(str[8] "The Jouster will only battle Knights!")
04: set_abort_and_exit()
05: show_text_block(str[9] "A flurry of motion pervades Jouster's / Way. Banners fly and trumpets bl")
06: wait_for_space()
07: show_text_block(str[10] "Riding a restless stallion, the Dread / Knight gallops toward your knigh")
08: wait_for_space()
09: encounter_setup(monsters=EF 18 00 00 00 00 00 00 00 00, flags=00 00)
10: apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
11: show_text_block(str[17] "You have proven your worthiness by / slaying the Dread Knight. The Juror")
12: wait_for_space()
13: clear_current_tile_event_flag()
```

**Event 09** — triggers: (11,15)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d be
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0xBE)
```

**Event 10** — triggers: (11,14)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d bd
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0xBD)
```

**Event 11** — triggers: (11,13)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d bc
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0xBC)
```

**Event 12** — triggers: (11,12)/DIR_W?

```hex
15 00 7b 08 10 01 14 01 0b 1e c8 0d 09 0c 0d bb
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[11] "To your right is Jouster's Way.")
02: clear_current_tile_event_flag()
03: show_text_basic(str[11] "To your right is Jouster's Way.")
04: delay(0xC8)
05: engine_call(0x09)
06: map_transition(0x0D, 0xBB)
```

**Event 13** — triggers: (11,11)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d ba
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0xBA)
```

**Event 14** — triggers: (11,10)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d aa
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0xAA)
```

**Event 15** — triggers: (10,10)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 9a
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x9A)
```

**Event 16** — triggers: (9,10)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 8a
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x8A)
```

**Event 17** — triggers: (8,10)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 7a
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x7A)
```

**Event 18** — triggers: (7,10)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 6a
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x6A)
```

**Event 19** — triggers: (6,10)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 5a
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x5A)
```

**Event 20** — triggers: (5,10)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 59
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x59)
```

**Event 21** — triggers: (5,9)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 58
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x58)
```

**Event 22** — triggers: (5,8)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 57
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x57)
```

**Event 23** — triggers: (5,7)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 47
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x47)
```

**Event 24** — triggers: (4,7)/DIR_W?

```hex
15 00 7b 08 10 01 14 01 0c 1e c8 0d 09 0c 0d 37
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[12] "Isle of the Ancients, Evil Zone.")
02: clear_current_tile_event_flag()
03: show_text_basic(str[12] "Isle of the Ancients, Evil Zone.")
04: delay(0xC8)
05: engine_call(0x09)
06: map_transition(0x0D, 0x37)
```

**Event 25** — triggers: (3,7)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 27
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x27)
```

**Event 26** — triggers: (2,7)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 17
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x17)
```

**Event 27** — triggers: (1,7)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d 07
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0x07)
```

**Event 28** — triggers: (0,7)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 10 f7
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x10, 0xF7)
```

**Event 29** — triggers: (1,13)/ANY_DIR, (13,10)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 c2 c2 e0 e1 e1 c2 c2 c2 c2 c1 c1 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=C2 C2 E0 E1 E1 C2 C2 C2 C2 C1, flags=C1 03)
04: clear_current_tile_event_flag()
```

**Event 30** — triggers: (10,2)/ANY_DIR, (10,15)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 c2 c1 c1 c2 e1 ce ce c2 c2 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=C2 C1 C1 C2 E1 CE CE C2 C2 00)
04: clear_current_tile_event_flag()
```

**Event 31** — triggers: (1,15)/ANY_DIR, (14,7)/ANY_DIR

```hex
22 07 07 11 02 2b 01 13 ce ce ce ce 00 00 00 00 00 00 14
```

```
00: cond = (era in [7..7])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE CE CE CE 00 00 00 00 00 00)
04: clear_current_tile_event_flag()
```

**Event 32** — triggers: (2,12)/0x30

```hex
06 0d 02 0e 29
```

```
00: show_text_popup_style_b(str[13] "Green / Message 2")
01: show_text_block(str[14] "F e  iuteeustj  ceetuF / i virs ri sAb aaoritrs")
02: set_abort_and_exit()
```

**Event 33** — triggers: (9,12)/0x90

```hex
06 0f 02 10 29
```

```
00: show_text_popup_style_b(str[15] "Green / Message 4")
01: show_text_block(str[16] "h dd bmr   y irdao v / .a gpfprea orn  ew ia")
02: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` Upon heavy wooden doors of an ancient / fortress is scrawled: Sorcerers ONLY! / Enter (y/n)?
- `[02]` A strange force repels your party.
- `[03]` Etched onto a huge stone is a "U".
- `[04]` Etched onto a huge stone is an "R".
- `[05]` Etched onto a huge stone is a "D".
- `[06]` Evil / Zone
- `[07]` A knight proclaims, / "None shall pass!" Attack (y/n)?
- `[08]` The Jouster will only battle Knights!
- `[09]` A flurry of motion pervades Jouster's / Way. Banners fly and trumpets blare as / ladies and nobles view the tournament / from colorful tents.
- `[10]` Riding a restless stallion, the Dread / Knight gallops toward your knight, / lance braced ready at his side.
- `[11]` To your right is Jouster's Way.
- `[12]` Isle of the Ancients, Evil Zone.
- `[13]` Green / Message 2
- `[14]` F e  iuteeustj  ceetuF / i virs ri sAb aaoritrs
- `[15]` Green / Message 4
- `[16]`   h dd bmr   y irdao v / .a gpfprea orn  ew ia 
- `[17]` You have proven your worthiness by / slaying the Dread Knight. The Jurors / atop Mount Farview await you with / your reward.
- `[18]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
