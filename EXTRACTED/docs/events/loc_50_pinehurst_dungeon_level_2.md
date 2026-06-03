# Location 50 — Pinehurst Dungeon Level 2

- **event.dat** offset `0x00F7C6`, length **1190** bytes
- **Map:** map screen **50**; Pinehurst Dungeon Level 2
- **Record kind:** `standard`
- **Triggers:** 37; **script segments:** 14; **strings:** 25

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,13) | `0x0D` | **12** | ENTER |
| (1,2) | `0x12` | **5** | ANY_DIR |
| (1,5) | `0x15` | **8** | 0x90 |
| (1,12) | `0x1C` | **12** | DIR_SPECIAL |
| (1,13) | `0x1D` | **6** | ANY_DIR |
| (1,14) | `0x1E` | **12** | ALWAYS |
| (2,11) | `0x2B` | **9** | ANY_DIR |
| (2,13) | `0x2D` | **12** | DIR_N? |
| (3,0) | `0x30` | **9** | ANY_DIR |
| (3,12) | `0x3C` | **9** | ANY_DIR |
| (3,13) | `0x3D` | **9** | ANY_DIR |
| (3,15) | `0x3F` | **9** | ANY_DIR |
| (4,9) | `0x49` | **9** | ANY_DIR |
| (4,11) | `0x4B` | **9** | ANY_DIR |
| (4,15) | `0x4F` | **9** | ANY_DIR |
| (6,4) | `0x64` | **9** | ANY_DIR |
| (6,8) | `0x68` | **2** | 0x60 |
| (7,1) | `0x71` | **9** | ANY_DIR |
| (7,6) | `0x76` | **12** | ALWAYS |
| (7,7) | `0x77` | **1** | DIR_SPECIAL |
| (8,15) | `0x8F` | **9** | ANY_DIR |
| (9,7) | `0x97` | **9** | ANY_DIR |
| (10,2) | `0xA2` | **9** | ANY_DIR |
| (10,6) | `0xA6` | **9** | ANY_DIR |
| (12,2) | `0xC2` | **10** | ENTER |
| (12,4) | `0xC4` | **9** | ANY_DIR |
| (12,10) | `0xCA` | **9** | ANY_DIR |
| (12,14) | `0xCE` | **11** | ENTER |
| (13,1) | `0xD1` | **10** | DIR_SPECIAL |
| (13,2) | `0xD2` | **3** | ANY_DIR |
| (13,3) | `0xD3` | **10** | ALWAYS |
| (13,13) | `0xDD` | **11** | DIR_SPECIAL |
| (13,14) | `0xDE` | **4** | ANY_DIR |
| (13,15) | `0xDF` | **11** | ALWAYS |
| (14,2) | `0xE2` | **10** | DIR_N? |
| (14,14) | `0xEE` | **11** | DIR_N? |
| (15,4) | `0xF4` | **7** | 0x90 |

## Events

**Event 01** — triggers: (7,7)/DIR_SPECIAL

```hex
01 01 09 11 01 0c 31 21 0f
```

```
00: show_text_basic(str[1] "Stairs leading up. Climb them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x31, 0x21)
04: end_script()
```

**Event 02** — triggers: (6,8)/0x60

```hex
0b 11 00 02 02 0a 11 01 0c 34 2a 0f
```

```
00: set_service_context(str[17] "All are content.", mode=0x00)
01: show_text_block(str[2] ""There's no dungeon that I enjoy / killing in more than level two of / L")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x34, 0x2A)
05: end_script()
```

**Event 03** — triggers: (13,2)/ANY_DIR

```hex
2d 20 00 11 04 01 03 07 0d 09 0c 32 77 02 04 19 01 f6 00 00 10 01 01 16 07 14
```

```
00: cond = check_member_attr(fields=0x20, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[4] "You have found a Ruby Tiara!")
02: show_text_basic(str[3] "The sign said NO KNIGHTS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x32, 0x77)
06: show_text_block(str[4] "You have found a Ruby Tiara!")
07: add_party_entity(0x01, f3a=0xF6, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[22] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 04** — triggers: (13,14)/ANY_DIR

```hex
2d 21 00 11 04 01 05 07 0d 09 0c 32 77 02 06 19 01 f2 00 00 10 01 01 16 07 14
```

```
00: cond = check_member_attr(fields=0x21, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[6] "You have found an Opal Pendant!")
02: show_text_basic(str[5] "The sign said NO PALADINS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x32, 0x77)
06: show_text_block(str[6] "You have found an Opal Pendant!")
07: add_party_entity(0x01, f3a=0xF2, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[22] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 05** — triggers: (1,2)/ANY_DIR

```hex
0b 16 00 0e f9
```

```
00: set_service_context(str[22] "*** Backpacks Full ***", mode=0x00)
01: exec_selector(0xF9)
```

**Event 06** — triggers: (1,13)/ANY_DIR

```hex
2d a0 00 11 04 01 0c 07 0d 09 0c 32 77 17 3e 00 10 06 01 0d 07 1c 0c 1f 80 3c 02 00 00 00 1a 3e 01 14 02 17 29
```

```
00: cond = check_member_attr(fields=0xA0, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> cond = load_var8(group=0x3E, index=0x00)
02: show_text_basic(str[12] "The sign said NO HUMANS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x32, 0x77)
06: cond = load_var8(group=0x3E, index=0x00)
07: if cond: skip_tokens(6)
    # skip -> show_text_block(str[23] "Sorry, benefits are given only once / per moon phase.")
08: show_text_basic(str[13] "You realize a new strength!")
09: wait_for_space()
10: op_1c_engine_query_to_cond(0x0C)
11: party_effect(sel=0x80, 3C 02 00 00 00)
12: store_var8(group=0x3E, value=0x01)
13: clear_current_tile_event_flag()
14: show_text_block(str[23] "Sorry, benefits are given only once / per moon phase.")
15: set_abort_and_exit()
```

**Event 07** — triggers: (15,4)/0x90

```hex
02 0e 09 11 05 02 0f 26 20 09 12 01 05 00 00 11 01 1f 09 15 01 03 00 00 0f
```

```
00: show_text_block(str[14] "Thick syrup drips from the walls. You / consider sampling it, but you've")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_block(str[15] "Who will sample (1-8)?")
04: selected = select_party_member()
05: party_effect_b(sel=0x09, 12 01 05 00 00)
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: party_effect(sel=0x09, 15 01 03 00 00)
08: end_script()
```

**Event 08** — triggers: (1,5)/0x90

```hex
02 10 09 11 03 18 00 25 00 00 02 11 07 0f
```

```
00: show_text_block(str[16] "A sign above a fountain reads, / "Good Water." Will you drink (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(3)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x25, and=0x00, or=0x00)
04: show_text_block(str[17] "All are content.")
05: wait_for_space()
06: end_script()
```

**Event 09** — triggers: (2,11)/ANY_DIR, (3,0)/ANY_DIR, (3,12)/ANY_DIR, (3,13)/ANY_DIR, (3,15)/ANY_DIR, (4,9)/ANY_DIR, (4,11)/ANY_DIR, (4,15)/ANY_DIR, (6,4)/ANY_DIR, (7,1)/ANY_DIR, (8,15)/ANY_DIR, (9,7)/ANY_DIR, (10,2)/ANY_DIR, (10,6)/ANY_DIR, (12,4)/ANY_DIR, (12,10)/ANY_DIR

```hex
2b 1b 02 12 20 01 28 02 32 00 00 10 01 18 01 28 00 00 20 02 28 02 32 00 00 10 01 18 02 28 00 00 20 03 28 02 32 00 00 10 01 18 03 28 00 00 20 04 28 02 32 00 00 10 01 18 04 28 00 00 20 05 28 02 32 00 00 10 01 18 05 28 00 00 20 06 28 02 32 00 00 10 01 18 06 28 00 00 20 07 28 02 32 00 00 10 01 18 07 28 00 00 20 08 28 02 32 00 00 10 01 18 08 28 00 00 07 13 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(27)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[18] "You are struck by bolts of electricity / which drain your magic.")
02: party_effect_b(sel=0x01, 28 02 32 00 00)
03: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x02, 28 02 32 00 00)
04: apply_party_masked(count=0x01, set=0x28, and=0x00, or=0x00)
05: party_effect_b(sel=0x02, 28 02 32 00 00)
06: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x03, 28 02 32 00 00)
07: apply_party_masked(count=0x02, set=0x28, and=0x00, or=0x00)
08: party_effect_b(sel=0x03, 28 02 32 00 00)
09: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x04, 28 02 32 00 00)
10: apply_party_masked(count=0x03, set=0x28, and=0x00, or=0x00)
11: party_effect_b(sel=0x04, 28 02 32 00 00)
12: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x05, 28 02 32 00 00)
13: apply_party_masked(count=0x04, set=0x28, and=0x00, or=0x00)
14: party_effect_b(sel=0x05, 28 02 32 00 00)
15: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x06, 28 02 32 00 00)
16: apply_party_masked(count=0x05, set=0x28, and=0x00, or=0x00)
17: party_effect_b(sel=0x06, 28 02 32 00 00)
18: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x07, 28 02 32 00 00)
19: apply_party_masked(count=0x06, set=0x28, and=0x00, or=0x00)
20: party_effect_b(sel=0x07, 28 02 32 00 00)
21: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x08, 28 02 32 00 00)
22: apply_party_masked(count=0x07, set=0x28, and=0x00, or=0x00)
23: party_effect_b(sel=0x08, 28 02 32 00 00)
24: if cond: skip_tokens(1)
    # skip -> wait_for_space()
25: apply_party_masked(count=0x08, set=0x28, and=0x00, or=0x00)
26: wait_for_space()
27: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
28: clear_current_tile_event_flag()
```

**Event 10** — triggers: (12,2)/ENTER, (13,1)/DIR_SPECIAL, (13,3)/ALWAYS, (14,2)/DIR_N?

```hex
06 13
```

```
00: show_text_popup_style_b(str[19] "No / Knights!")
```

**Event 11** — triggers: (12,14)/ENTER, (13,13)/DIR_SPECIAL, (13,15)/ALWAYS, (14,14)/DIR_N?

```hex
06 14
```

```
00: show_text_popup_style_b(str[20] "No / Paladins!")
```

**Event 12** — triggers: (0,13)/ENTER, (1,12)/DIR_SPECIAL, (1,14)/ALWAYS, (2,13)/DIR_N?, (7,6)/ALWAYS

```hex
06 15
```

```
00: show_text_popup_style_b(str[21] "No / Humans!")
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs leading up. Climb them (y/n)?
- `[02]` "There's no dungeon that I enjoy / killing in more than level two of / Luxus Palace Dungeon. Would you like / me to send you there (y/n)?"
- `[03]` The sign said NO KNIGHTS!
- `[04]` You have found a Ruby Tiara!
- `[05]` The sign said NO PALADINS!
- `[06]` You have found an Opal Pendant!
- `[07]` A
- `[08]` "
- `[09]` a
- `[10]` "
- `[11]` T
- `[12]` The sign said NO HUMANS!
- `[13]` You realize a new strength!
- `[14]` Thick syrup drips from the walls. You / consider sampling it, but you've got / to ask yourself one question. Do I / feel lucky? Well, do you (y/n)?
- `[15]` Who will sample (1-8)?
- `[16]` A sign above a fountain reads, / "Good Water." Will you drink (y/n)?
- `[17]` All are content.
- `[18]` You are struck by bolts of electricity / which drain your magic.
- `[19]` No / Knights!
- `[20]` No / Paladins!
- `[21]` No / Humans!
- `[22]` *** Backpacks Full ***
- `[23]` Sorry, benefits are given only once / per moon phase.
- `[24]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
