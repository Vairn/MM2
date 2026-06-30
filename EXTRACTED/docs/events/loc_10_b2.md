# Location 10 — B2

- **event.dat** offset `0x00393A`, length **1402** bytes
- **Map:** map screen **10**; overland sector **B2**
- **Record kind:** `standard`
- **Triggers:** 43; **script segments:** 25; **strings:** 23

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,11) | `0x0B` | **3** | ANY_DIR |
| (1,2) | `0x12` | **3** | ANY_DIR |
| (1,4) | `0x14` | **10** | ANY_DIR |
| (1,6) | `0x16` | **12** | ANY_DIR |
| (1,8) | `0x18` | **3** | ANY_DIR |
| (1,11) | `0x1B` | **2** | DIR_N? |
| (1,13) | `0x1D` | **3** | ANY_DIR |
| (2,5) | `0x25` | **8** | ANY_DIR |
| (2,9) | `0x29` | **3** | ANY_DIR |
| (2,10) | `0x2A` | **2** | DIR_E? |
| (2,11) | `0x2B` | **17** | 0xA0 |
| (2,12) | `0x2C` | **2** | DIR_W? |
| (2,14) | `0x2E` | **3** | ANY_DIR |
| (3,4) | `0x34` | **9** | ANY_DIR |
| (3,5) | `0x35` | **21** | ANY_DIR |
| (3,6) | `0x36` | **11** | ANY_DIR |
| (3,11) | `0x3B` | **2** | DIR_S? |
| (4,2) | `0x42` | **3** | ANY_DIR |
| (4,10) | `0x4A` | **4** | ANY_DIR |
| (4,12) | `0x4C` | **3** | ANY_DIR |
| (4,14) | `0x4E` | **18** | DIR_S? |
| (5,6) | `0x56` | **3** | ANY_DIR |
| (5,9) | `0x59` | **13** | ANY_DIR |
| (5,14) | `0x5E` | **23** | DIR_N?+DIR_E? |
| (6,3) | `0x63` | **15** | ANY_DIR |
| (7,2) | `0x72` | **19** | ANY_DIR |
| (7,7) | `0x77` | **20** | ANY_DIR |
| (7,13) | `0x7D` | **4** | DIR_W? |
| (7,14) | `0x7E` | **21** | ANY_DIR |
| (8,8) | `0x88` | **5** | ANY_DIR |
| (9,1) | `0x91` | **1** | ANY_DIR |
| (9,5) | `0x95` | **6** | ANY_DIR |
| (9,13) | `0x9D` | **20** | ANY_DIR |
| (9,14) | `0x9E` | **22** | 0x60 |
| (10,14) | `0xAE` | **14** | ANY_DIR |
| (12,1) | `0xC1` | **6** | ANY_DIR |
| (12,7) | `0xC7` | **6** | ANY_DIR |
| (12,12) | `0xCC` | **19** | ANY_DIR |
| (12,13) | `0xCD` | **16** | ANY_DIR |
| (13,3) | `0xD3` | **6** | ANY_DIR |
| (13,5) | `0xD5` | **7** | ANY_DIR |
| (14,10) | `0xEA` | **6** | ANY_DIR |
| (15,1) | `0xF1` | **6** | ANY_DIR |

## Events

**Event 01** — triggers: (9,1)/ANY_DIR

```hex
2b 01 12 d4 d4 d4 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=D4 D4 D4 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 02** — triggers: (1,11)/DIR_N?, (2,10)/DIR_E?, (2,12)/DIR_W?, (3,11)/DIR_S?

```hex
06 01
```

```
00: show_text_popup_style_b(str[1] "Archers / Only")
```

**Event 03** — triggers: (0,11)/ANY_DIR, (1,2)/ANY_DIR, (1,8)/ANY_DIR, (1,13)/ANY_DIR, (2,9)/ANY_DIR, (2,14)/ANY_DIR, (4,2)/ANY_DIR, (4,12)/ANY_DIR, (5,6)/ANY_DIR

```hex
2b 01 12 82 82 82 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=82 82 82 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 04** — triggers: (4,10)/ANY_DIR, (7,13)/DIR_W?

```hex
06 02
```

```
00: show_text_popup_style_b(str[2] "Circus / <-This way")
```

**Event 05** — triggers: (8,8)/ANY_DIR

```hex
02 03 09 11 01 18 00 2b 00 41 14
```

```
00: show_text_block(str[3] "A great fruit tree rises majestically / above you. The sun beams off a h")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: apply_party_masked(count=0x00, set=0x2B, and=0x00, or=0x41)
04: clear_current_tile_event_flag()
```

**Event 06** — triggers: (9,5)/ANY_DIR, (12,1)/ANY_DIR, (12,7)/ANY_DIR, (13,3)/ANY_DIR, (14,10)/ANY_DIR, (15,1)/ANY_DIR

```hex
1c 64 1b 1e 10 03 01 04 0d 09 31 00 1e 00 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(3)
    # skip -> end_script()
03: show_text_basic(str[4] "A snowdrift engulfs your party.")
04: engine_call(0x09)
05: for_party(mask=0x00): call(0x1E,0x00)
06: end_script()
```

**Event 07** — triggers: (13,5)/ANY_DIR

```hex
02 05 09 10 01 0f 19 01 51 0a 02 10 02 01 0d 07 14
```

```
00: show_text_block(str[5] "Something is buried in the snow. / Get it (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> add_party_entity(0x01, f3a=0x51, f40=0x0A, f46=0x02)
03: end_script()
04: add_party_entity(0x01, f3a=0x51, f40=0x0A, f46=0x02)
05: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
06: show_text_basic(str[13] "*** Backpacks Full ***")
07: wait_for_space()
08: clear_current_tile_event_flag()
```

**Event 08** — triggers: (2,5)/ANY_DIR

```hex
2b 01 12 37 37 37 37 37 37 37 37 37 37 37 14 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=37 37 37 37 37 37 37 37 37 37, flags=37 14)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (3,4)/ANY_DIR

```hex
01 06 07 0d 09 0c 2d f8
```

```
00: show_text_basic(str[6] "Poof! You are displaced.")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x2D, 0xF8)
```

**Event 10** — triggers: (1,4)/ANY_DIR

```hex
01 07 07 0d 09 0c 2f e0
```

```
00: show_text_basic(str[7] "Shazam! You are teleported!")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x2F, 0xE0)
```

**Event 11** — triggers: (3,6)/ANY_DIR

```hex
01 08 07 0d 09 0c 31 21
```

```
00: show_text_basic(str[8] "Foop! You are ejected!")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x31, 0x21)
```

**Event 12** — triggers: (1,6)/ANY_DIR

```hex
01 09 07 0d 09 0c 33 89
```

```
00: show_text_basic(str[9] "Zap! Where am I?")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x33, 0x89)
```

**Event 13** — triggers: (5,9)/ANY_DIR

```hex
2b 04 02 0a 09 11 02 12 08 08 08 08 08 08 08 08 08 08 08 f0 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[10] "The Grand Order of Merchants is / holding a meeting. Disrupt (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=08 08 08 08 08 08 08 08 08 08, flags=08 F0)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 14** — triggers: (10,14)/ANY_DIR

```hex
2b 04 02 0b 09 11 02 12 11 11 11 11 11 11 11 11 11 11 11 f0 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[11] "Orcon Convention. Disrupt / the unwashed Orcs (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=11 11 11 11 11 11 11 11 11 11, flags=11 F0)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 15** — triggers: (6,3)/ANY_DIR

```hex
2b 04 02 0c 09 11 02 12 e7 e7 00 00 00 00 00 00 00 00 00 00 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[12] "Cosmic Sludge drips from the sky. / Investigate (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=E7 E7 00 00 00 00 00 00 00 00, flags=00 00)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 16** — triggers: (12,13)/ANY_DIR

```hex
2b 01 12 37 37 37 37 37 37 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=37 37 37 37 37 37 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (2,11)/0xA0

```hex
2b 07 2d 02 05 10 03 02 0e 07 14 02 0f 12 f0 00 00 00 00 00 00 00 00 00 00 00 18 00 75 fe 01 02 15 07 14
```

```
00: skip_tokens(7)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
01: cond = check_member_attr(fields=0x02, value=0x05)
02: if cond: skip_tokens(3)
    # skip -> show_text_block(str[15] "Ruthless Baron Wilfrey trains a / young falcon. He throws his leather / ")
03: show_text_block(str[14] "Baron Wilfrey refuses to be bothered / by any except archers.")
04: wait_for_space()
05: clear_current_tile_event_flag()
06: show_text_block(str[15] "Ruthless Baron Wilfrey trains a / young falcon. He throws his leather / ")
07: encounter_setup(monsters=F0 00 00 00 00 00 00 00 00 00, flags=00 00)
08: apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
09: show_text_block(str[21] "You have saved Falcon Forest from / ruthless Baron Wilfrey. Now, return ")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 18** — triggers: (4,14)/DIR_S?

```hex
23 8c aa 11 01 0e 64 01 10 29
```

```
00: cond = check_day_of_year(0x8C, 0xAA)
01: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[16] "Looks like the circus was here!")
02: exec_selector(0x64)  # portal_travel_100
03: show_text_basic(str[16] "Looks like the circus was here!")
04: set_abort_and_exit()
```

**Event 19** — triggers: (7,2)/ANY_DIR, (12,12)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 e0 e1 e1 c1 c2 c1 c2 c1 c1 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=E0 E1 E1 C1 C2 C1 C2 C1 C1 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 20** — triggers: (7,7)/ANY_DIR, (9,13)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 ce c2 ce c2 e1 c1 c1 c2 c2 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE C2 CE C2 E1 C1 C1 C2 C2 00)
04: clear_current_tile_event_flag()
```

**Event 21** — triggers: (3,5)/ANY_DIR, (7,14)/ANY_DIR

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

**Event 22** — triggers: (9,14)/0x60

```hex
06 11 02 12 29
```

```
00: show_text_popup_style_b(str[17] "Green / Message 1")
01: show_text_block(str[18] "o a ivaoc eehu whss na / eTeno aoacs bciltsth k")
02: set_abort_and_exit()
```

**Event 23** — triggers: (5,14)/DIR_N?+DIR_E?

```hex
06 13 02 14 29
```

```
00: show_text_popup_style_b(str[19] "Green / Message 3")
01: show_text_block(str[20] "r c nil ot,eer h i Mtr / wrl uopptl.Readlh het.")
02: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` Archers / Only
- `[02]` Circus / <-This way
- `[03]` A great fruit tree rises majestically / above you. The sun beams off a huge, / golden mango. Pick and eat (y/n)?
- `[04]` A snowdrift engulfs your party.
- `[05]` Something is buried in the snow. / Get it (y/n)?
- `[06]` Poof! You are displaced.
- `[07]` Shazam! You are teleported!
- `[08]` Foop! You are ejected!
- `[09]` Zap! Where am I?
- `[10]` The Grand Order of Merchants is / holding a meeting. Disrupt (y/n)?
- `[11]` Orcon Convention. Disrupt / the unwashed Orcs (y/n)?
- `[12]` Cosmic Sludge drips from the sky. / Investigate (y/n)?
- `[13]` *** Backpacks Full ***
- `[14]` Baron Wilfrey refuses to be bothered / by any except archers.
- `[15]` Ruthless Baron Wilfrey trains a / young falcon. He throws his leather / glove at your archer's foot!
- `[16]` Looks like the circus was here!
- `[17]` Green / Message 1
- `[18]` o a ivaoc eehu whss na / eTeno aoacs bciltsth k
- `[19]` Green / Message 3
- `[20]` r c nil ot,eer h i Mtr / wrl uopptl.Readlh het.
- `[21]` You have saved Falcon Forest from / ruthless Baron Wilfrey. Now, return / to the Jurors for your reward.
- `[22]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
