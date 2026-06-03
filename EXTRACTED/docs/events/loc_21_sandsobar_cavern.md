# Location 21 — Sandsobar Cavern

- **event.dat** offset `0x00743D`, length **1418** bytes
- **Map:** map screen **21**; Sandsobar Cavern
- **Record kind:** `mixed_pool`
- **Triggers:** 35; **script segments:** 26; **strings:** 18

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **4** | ANY_DIR |
| (0,5) | `0x05` | **13** | ANY_DIR |
| (0,14) | `0x0E` | **16** | ANY_DIR |
| (0,15) | `0x0F` | **10** | ALWAYS |
| (1,6) | `0x16` | **6** | ANY_DIR |
| (1,15) | `0x1F` | **6** | ANY_DIR |
| (2,4) | `0x24` | **22** | DIR_SPECIAL |
| (2,6) | `0x26` | **14** | ANY_DIR |
| (3,3) | `0x33` | **15** | ANY_DIR |
| (3,12) | `0x3C` | **5** | 0x90 |
| (4,6) | `0x46` | **6** | ANY_DIR |
| (4,15) | `0x4F` | **8** | DIR_SPECIAL |
| (5,2) | `0x52` | **6** | ANY_DIR |
| (5,4) | `0x54` | **6** | ANY_DIR |
| (5,9) | `0x59` | **6** | ANY_DIR |
| (5,11) | `0x5B` | **7** | DIR_N? |
| (6,4) | `0x64` | **12** | ANY_DIR |
| (6,9) | `0x69` | **12** | ANY_DIR |
| (6,12) | `0x6C` | **6** | ANY_DIR |
| (7,12) | `0x7C` | **1** | DIR_SPECIAL |
| (8,5) | `0x85` | **23** | 0x90 |
| (8,10) | `0x8A` | **12** | ANY_DIR |
| (9,3) | `0x93` | **19** | ANY_DIR |
| (9,7) | `0x97` | **11** | ANY_DIR |
| (9,13) | `0x9D` | **3** | DIR_SPECIAL |
| (10,0) | `0xA0` | **6** | ANY_DIR |
| (11,0) | `0xB0` | **24** | ALWAYS |
| (11,2) | `0xB2` | **6** | ANY_DIR |
| (12,0) | `0xC0` | **2** | DIR_N? |
| (12,9) | `0xC9` | **18** | ANY_DIR |
| (12,12) | `0xCC` | **6** | ANY_DIR |
| (13,12) | `0xDC` | **17** | ANY_DIR |
| (14,2) | `0xE2` | **9** | ENTER |
| (14,14) | `0xEE` | **21** | DIR_SPECIAL |
| (15,2) | `0xF2` | **20** | ANY_DIR |

## Events

**Event 01** — triggers: (7,12)/DIR_SPECIAL

```hex
01 01 09 11 01 0c 04 0a 0f
```

```
00: show_text_basic(str[1] "Stairs lead to Sandsobar. Exit (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x04, 0x0A)
04: end_script()
```

**Event 02** — triggers: (12,0)/DIR_N?

```hex
0b 0e 00 02 02 0a 11 2f 24 bc 02 11 2b 15 01 1e 00 11 03 1b 3d 10 01 1f 01 1e 01 0a 00 00 15 02 1e 00 11 03 1b 3d 10 01 1f 02 1e 01 0a 00 00 15 03 1e 00 11 03 1b 3d 10 01 1f 03 1e 01 0a 00 00 15 04 1e 00 11 03 1b 3d 10 01 1f 04 1e 01 0a 00 00 15 05 1e 00 11 03 1b 3d 10 01 1f 05 1e 01 0a 00 00 15 06 1e 00 11 03 1b 3d 10 01 1f 06 1e 01 0a 00 00 15 07 1e 00 11 03 1b 3d 10 01 1f 07 1e 01 0a 00 00 15 08 1e 00 11 03 1b 3d 10 01 1f 08 1e 01 0a 00 00 01 03 08 14 01 04 29 0f
```

```
00: set_service_context(str[14] "Sharp Tooth Den", mode=0x00)
01: show_text_block(str[2] "The infamous master thief Rinaldo Jr. / will train your thieves in the /")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(47)
    # skip -> end_script()
04: cond = check_gold_at_least(700)
05: if not cond: skip_tokens(43)
    # skip -> show_text_basic(str[4] "Not enough gold!")
06: apply_party(count=0x01, op=0x1E, val=0x00)
07: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x02, op=0x1E, val=0x00)
08: cond = (cond >= 0x3D)
09: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x1E, val=0x00)
10: party_effect(sel=0x01, 1E 01 0A 00 00)
11: apply_party(count=0x02, op=0x1E, val=0x00)
12: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x03, op=0x1E, val=0x00)
13: cond = (cond >= 0x3D)
14: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x1E, val=0x00)
15: party_effect(sel=0x02, 1E 01 0A 00 00)
16: apply_party(count=0x03, op=0x1E, val=0x00)
17: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x04, op=0x1E, val=0x00)
18: cond = (cond >= 0x3D)
19: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x1E, val=0x00)
20: party_effect(sel=0x03, 1E 01 0A 00 00)
21: apply_party(count=0x04, op=0x1E, val=0x00)
22: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x05, op=0x1E, val=0x00)
23: cond = (cond >= 0x3D)
24: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x1E, val=0x00)
25: party_effect(sel=0x04, 1E 01 0A 00 00)
26: apply_party(count=0x05, op=0x1E, val=0x00)
27: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x06, op=0x1E, val=0x00)
28: cond = (cond >= 0x3D)
29: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x1E, val=0x00)
30: party_effect(sel=0x05, 1E 01 0A 00 00)
31: apply_party(count=0x06, op=0x1E, val=0x00)
32: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x07, op=0x1E, val=0x00)
33: cond = (cond >= 0x3D)
34: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x1E, val=0x00)
35: party_effect(sel=0x06, 1E 01 0A 00 00)
36: apply_party(count=0x07, op=0x1E, val=0x00)
37: if not cond: skip_tokens(3)
    # skip -> apply_party(count=0x08, op=0x1E, val=0x00)
38: cond = (cond >= 0x3D)
39: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x1E, val=0x00)
40: party_effect(sel=0x07, 1E 01 0A 00 00)
41: apply_party(count=0x08, op=0x1E, val=0x00)
42: if not cond: skip_tokens(3)
    # skip -> show_text_basic(str[3] "You feel more street smart.")
43: cond = (cond >= 0x3D)
44: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[3] "You feel more street smart.")
45: party_effect(sel=0x08, 1E 01 0A 00 00)
46: show_text_basic(str[3] "You feel more street smart.")
47: wait_key()
48: clear_current_tile_event_flag()
49: show_text_basic(str[4] "Not enough gold!")
50: set_abort_and_exit()
51: end_script()
```

**Event 03** — triggers: (9,13)/DIR_SPECIAL

```hex
0b 0e 00 02 05 0a 11 27 24 fa 00 11 23 15 01 1e 00 1b 06 11 01 20 01 1e 01 05 00 00 15 02 1e 00 1b 06 11 01 20 02 1e 01 05 00 00 15 03 1e 00 1b 06 11 01 20 03 1e 01 05 00 00 15 04 1e 00 1b 06 11 01 20 04 1e 01 05 00 00 15 05 1e 00 1b 06 11 01 20 05 1e 01 05 00 00 15 06 1e 00 1b 06 11 01 20 06 1e 01 05 00 00 15 07 1e 00 1b 06 11 01 20 07 1e 01 05 00 00 15 08 1e 00 1b 06 11 01 20 08 1e 01 05 00 00 02 06 08 0f 01 04 08 0f
```

```
00: set_service_context(str[14] "Sharp Tooth Den", mode=0x00)
01: show_text_block(str[5] "The Thieving Fiend Maxwell will teach / all your thieves the intricacies")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(39)
    # skip -> end_script()
04: cond = check_gold_at_least(250)
05: if not cond: skip_tokens(35)
    # skip -> show_text_basic(str[4] "Not enough gold!")
06: apply_party(count=0x01, op=0x1E, val=0x00)
07: cond = (cond >= 0x06)
08: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x1E, val=0x00)
09: party_effect_b(sel=0x01, 1E 01 05 00 00)
10: apply_party(count=0x02, op=0x1E, val=0x00)
11: cond = (cond >= 0x06)
12: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x1E, val=0x00)
13: party_effect_b(sel=0x02, 1E 01 05 00 00)
14: apply_party(count=0x03, op=0x1E, val=0x00)
15: cond = (cond >= 0x06)
16: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x1E, val=0x00)
17: party_effect_b(sel=0x03, 1E 01 05 00 00)
18: apply_party(count=0x04, op=0x1E, val=0x00)
19: cond = (cond >= 0x06)
20: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x1E, val=0x00)
21: party_effect_b(sel=0x04, 1E 01 05 00 00)
22: apply_party(count=0x05, op=0x1E, val=0x00)
23: cond = (cond >= 0x06)
24: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x1E, val=0x00)
25: party_effect_b(sel=0x05, 1E 01 05 00 00)
26: apply_party(count=0x06, op=0x1E, val=0x00)
27: cond = (cond >= 0x06)
28: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x1E, val=0x00)
29: party_effect_b(sel=0x06, 1E 01 05 00 00)
30: apply_party(count=0x07, op=0x1E, val=0x00)
31: cond = (cond >= 0x06)
32: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x1E, val=0x00)
33: party_effect_b(sel=0x07, 1E 01 05 00 00)
34: apply_party(count=0x08, op=0x1E, val=0x00)
35: cond = (cond >= 0x06)
36: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[6] "The Thieving Fiend got you!")
37: party_effect_b(sel=0x08, 1E 01 05 00 00)
38: show_text_block(str[6] "The Thieving Fiend got you!")
39: wait_key()
40: end_script()
41: show_text_basic(str[4] "Not enough gold!")
42: wait_key()
43: end_script()
```

**Event 04** — triggers: (0,0)/ANY_DIR

```hex
0b 18 00 02 08 08 19 01 c1 00 00 10 02 01 07 08 14
```

```
00: set_service_context(str[24], mode=0x00)
01: show_text_block(str[8] "A foul-smelling zombie hands you an / Admit 8 Pass and says, "This will ")
02: wait_key()
03: add_party_entity(0x01, f3a=0xC1, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_basic(str[7] "*** Backpacks Full ***")
06: wait_key()
07: clear_current_tile_event_flag()
```

**Event 05** — triggers: (3,12)/0x90

```hex
02 09 07 19 01 d7 14 00 10 02 01 07 07 14
```

```
00: show_text_block(str[9] "Hidden amongst the pack's bone cache / is a Dog Whistle.")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0xD7, f40=0x14, f46=0x00)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[7] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 06** — triggers: (1,6)/ANY_DIR, (1,15)/ANY_DIR, (4,6)/ANY_DIR, (5,2)/ANY_DIR, (5,4)/ANY_DIR, (5,9)/ANY_DIR, (6,12)/ANY_DIR, (10,0)/ANY_DIR, (11,2)/ANY_DIR, (12,12)/ANY_DIR

```hex
01 0a 1c 08 31 09 14 00 07 14
```

```
00: show_text_basic(str[10] "You have stepped into an ankle trap!")
01: op_1c_engine_query_to_cond(0x08)
02: for_party(mask=0x09): call(0x14,0x00)
03: wait_for_space()
04: clear_current_tile_event_flag()
```

**Event 07** — triggers: (5,11)/DIR_N?

```hex
06 0b
```

```
00: show_text_popup_style_b(str[11] "Beware of / Vermin!")
```

**Event 08** — triggers: (4,15)/DIR_SPECIAL

```hex
06 0c
```

```
00: show_text_popup_style_b(str[12] "<Master / Thief")
```

**Event 09** — triggers: (14,2)/ENTER

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Thieves' Guild")
```

**Event 10** — triggers: (0,15)/ALWAYS

```hex
04 0e
```

```
00: show_text_above_door(str[14] "Sharp Tooth Den")
```

**Event 11** — triggers: (9,7)/ANY_DIR

```hex
2b 01 12 27 27 27 27 27 27 35 35 35 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=27 27 27 27 27 27 35 35 35 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (6,4)/ANY_DIR, (6,9)/ANY_DIR, (8,10)/ANY_DIR

```hex
2b 01 12 30 30 30 30 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=30 30 30 30 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (0,5)/ANY_DIR

```hex
2b 01 12 45 45 45 45 45 45 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=45 45 45 45 45 45 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (2,6)/ANY_DIR

```hex
2b 01 12 34 34 34 34 34 34 34 34 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=34 34 34 34 34 34 34 34 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (3,3)/ANY_DIR

```hex
2b 01 12 53 53 53 53 53 53 53 53 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=53 53 53 53 53 53 53 53 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (0,14)/ANY_DIR

```hex
2b 01 12 55 55 55 55 55 55 55 55 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=55 55 55 55 55 55 55 55 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (13,12)/ANY_DIR

```hex
2b 01 12 56 56 56 56 56 56 56 56 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=56 56 56 56 56 56 56 56 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (12,9)/ANY_DIR

```hex
2b 01 12 21 21 21 21 21 21 21 21 21 21 21 02 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=21 21 21 21 21 21 21 21 21 21, flags=21 02)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (9,3)/ANY_DIR

```hex
2b 01 12 29 29 29 29 29 29 29 29 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=29 29 29 29 29 29 29 29 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 20** — triggers: (15,2)/ANY_DIR

```hex
2b 01 12 38 38 38 38 38 38 38 38 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=38 38 38 38 38 38 38 38 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 21** — triggers: (14,14)/DIR_SPECIAL

```hex
02 0f 29
```

```
00: show_text_block(str[15] "The mystic Thaumaturge of Good can be / freed if you enter Right 32 and ")
01: set_abort_and_exit()
```

**Event 22** — triggers: (2,4)/DIR_SPECIAL

```hex
02 10 29
```

```
00: show_text_block(str[16] "Seek the N-19 Capitor at 3,13 in / Castle Hillstone.")
01: set_abort_and_exit()
```

**Event 23** — triggers: (8,5)/0x90

```hex
0e 74
```

```
00: exec_selector(0x74)
```

**Event 24** — triggers: (11,0)/ALWAYS

```hex
0e 79
```

```
00: exec_selector(0x79)
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs lead to Sandsobar. Exit (y/n)?
- `[02]` The infamous master thief Rinaldo Jr. / will train your thieves in the / finer points of pilfering for / only 700 gold (y/n)?
- `[03]` You feel more street smart.
- `[04]` Not enough gold!
- `[05]` The Thieving Fiend Maxwell will teach / all your thieves the intricacies of / his trade for only 250 gold. / Accept (y/n)?
- `[06]` The Thieving Fiend got you!
- `[07]` *** Backpacks Full ***
- `[08]` A foul-smelling zombie hands you an / Admit 8 Pass and says, "This will / help you in Corak's Cavern."
- `[09]` Hidden amongst the pack's bone cache / is a Dog Whistle.
- `[10]` You have stepped into an ankle trap!
- `[11]` Beware of / Vermin!
- `[12]` <Master / Thief
- `[13]` Thieves' Guild
- `[14]` Sharp Tooth Den
- `[15]` The mystic Thaumaturge of Good can be / freed if you enter Right 32 and Left / 64.
- `[16]` Seek the N-19 Capitor at 3,13 in / Castle Hillstone.
- `[17]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
