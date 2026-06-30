# Location 18 — Atlantium Cavern

- **event.dat** offset `0x0063FE`, length **1420** bytes
- **Map:** map screen **18**; Atlantium Cavern
- **Record kind:** `standard`
- **Triggers:** 27; **script segments:** 23; **strings:** 17

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,5) | `0x15` | **4** | DIR_W? |
| (3,3) | `0x33` | **3** | DIR_N? |
| (5,5) | `0x55` | **17** | DIR_E? |
| (5,14) | `0x5E` | **5** | DIR_N?+DIR_E? |
| (6,9) | `0x69` | **21** | DIR_E? |
| (6,13) | `0x6D` | **9** | DIR_N? |
| (7,1) | `0x71` | **11** | ANY_DIR |
| (7,5) | `0x75` | **10** | ANY_DIR |
| (7,15) | `0x7F` | **18** | DIR_N? |
| (8,0) | `0x80` | **8** | DIR_N? |
| (8,1) | `0x81` | **8** | DIR_S? |
| (8,5) | `0x85` | **8** | DIR_S? |
| (8,6) | `0x86` | **7** | DIR_W? |
| (8,8) | `0x88` | **1** | DIR_E? |
| (9,0) | `0x90` | **12** | ANY_DIR |
| (9,4) | `0x94` | **13** | ANY_DIR |
| (9,5) | `0x95` | **8** | DIR_W? |
| (11,0) | `0xB0` | **8** | DIR_N? |
| (12,0) | `0xC0` | **15** | ANY_DIR |
| (12,4) | `0xC4` | **14** | ANY_DIR |
| (12,5) | `0xC5` | **8** | DIR_W? |
| (13,1) | `0xD1` | **19** | DIR_N?+DIR_E? |
| (13,6) | `0xD6` | **20** | DIR_S? |
| (13,7) | `0xD7` | **8** | DIR_N? |
| (14,0) | `0xE0` | **2** | 0x30 |
| (14,7) | `0xE7` | **16** | ANY_DIR |
| (15,11) | `0xFB` | **6** | DIR_E? |

## Events

**Event 01** — triggers: (8,8)/DIR_E?

```hex
01 01 09 11 01 0c 01 f0 0f
```

```
00: show_text_basic(str[1] "Exit to Atlantium. Ascend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x01, 0xF0)
04: end_script()
```

**Event 02** — triggers: (14,0)/0x30

```hex
02 02 09 10 01 0f 01 03 07 0d 09 0c 0c 48
```

```
00: show_text_block(str[2] "A marble statue of a winged beast has / a button protruding from its / p")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[3] "Poof!")
03: end_script()
04: show_text_basic(str[3] "Poof!")
05: wait_for_space()
06: engine_call(0x09)
07: map_transition(0x0C, 0x48)
```

**Event 03** — triggers: (3,3)/DIR_N?

```hex
2b 06 02 04 09 11 04 02 05 31 00 32 00 13 00 00 00 00 00 00 00 00 00 00 14 0f
```

```
00: skip_tokens(6)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[4] "An old rotting wooden statue appears / unstable. A small but sturdy butt")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(4)
    # skip -> end_script()
04: show_text_block(str[5] "The statue explodes, releasing angry / monsters.")
05: for_party(mask=0x00): call(0x32,0x00)
06: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
07: clear_current_tile_event_flag()
08: end_script()
```

**Event 04** — triggers: (1,5)/DIR_W?

```hex
2b 0c 02 06 09 11 0a 02 07 18 00 22 00 03 18 00 23 00 03 18 00 24 00 03 18 00 2a 00 03 18 00 2b 00 03 18 00 2c 00 03 18 00 2d 00 03 13 00 00 00 00 00 00 00 00 00 00 14 0f
```

```
00: skip_tokens(12)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[6] "A metallic statue of an Atlantian / deity stares at you intensely. / Tou")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(10)
    # skip -> end_script()
04: show_text_block(str[7] "Its eyes appear to come alive and you / feel drained.")
05: apply_party_masked(count=0x00, set=0x22, and=0x00, or=0x03)
06: apply_party_masked(count=0x00, set=0x23, and=0x00, or=0x03)
07: apply_party_masked(count=0x00, set=0x24, and=0x00, or=0x03)
08: apply_party_masked(count=0x00, set=0x2A, and=0x00, or=0x03)
09: apply_party_masked(count=0x00, set=0x2B, and=0x00, or=0x03)
10: apply_party_masked(count=0x00, set=0x2C, and=0x00, or=0x03)
11: apply_party_masked(count=0x00, set=0x2D, and=0x00, or=0x03)
12: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
13: clear_current_tile_event_flag()
14: end_script()
```

**Event 05** — triggers: (5,14)/DIR_N?+DIR_E?

```hex
02 08 09 10 01 0f 01 03 07 0d 09 0c 92 00
```

```
00: show_text_block(str[8] "Everytime you look at this statue it / appears to be moving. Touch it (y")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[3] "Poof!")
03: end_script()
04: show_text_basic(str[3] "Poof!")
05: wait_for_space()
06: engine_call(0x09)
07: map_transition(0x92, 0x00)
```

**Event 06** — triggers: (15,11)/DIR_E?

```hex
02 09 09 11 23 02 0a 15 01 11 00 1b 32 10 01 1f 01 11 01 0a 00 00 15 02 11 00 1b 32 10 01 1f 02 11 01 0a 00 00 15 03 11 00 1b 32 10 01 1f 03 11 01 0a 00 00 15 04 11 00 1b 32 10 01 1f 04 11 01 0a 00 00 15 05 11 00 1b 32 10 01 1f 05 11 01 0a 00 00 15 06 11 00 1b 32 10 01 1f 06 11 01 0a 00 00 15 07 11 00 1b 32 10 01 1f 07 11 01 0a 00 00 15 08 11 00 1b 32 10 01 1f 08 11 01 0a 00 00 07 14 0f
```

```
00: show_text_block(str[9] "A statue of a renowned scholar kneels / with a book in hand. Read pages ")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(35)
    # skip -> end_script()
03: show_text_block(str[10] "After reading the pages you are / enlightened.")
04: apply_party(count=0x01, op=0x11, val=0x00)
05: cond = (cond >= 0x32)
06: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x11, val=0x00)
07: party_effect(sel=0x01, 11 01 0A 00 00)
08: apply_party(count=0x02, op=0x11, val=0x00)
09: cond = (cond >= 0x32)
10: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x11, val=0x00)
11: party_effect(sel=0x02, 11 01 0A 00 00)
12: apply_party(count=0x03, op=0x11, val=0x00)
13: cond = (cond >= 0x32)
14: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x11, val=0x00)
15: party_effect(sel=0x03, 11 01 0A 00 00)
16: apply_party(count=0x04, op=0x11, val=0x00)
17: cond = (cond >= 0x32)
18: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x11, val=0x00)
19: party_effect(sel=0x04, 11 01 0A 00 00)
20: apply_party(count=0x05, op=0x11, val=0x00)
21: cond = (cond >= 0x32)
22: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x11, val=0x00)
23: party_effect(sel=0x05, 11 01 0A 00 00)
24: apply_party(count=0x06, op=0x11, val=0x00)
25: cond = (cond >= 0x32)
26: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x11, val=0x00)
27: party_effect(sel=0x06, 11 01 0A 00 00)
28: apply_party(count=0x07, op=0x11, val=0x00)
29: cond = (cond >= 0x32)
30: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x11, val=0x00)
31: party_effect(sel=0x07, 11 01 0A 00 00)
32: apply_party(count=0x08, op=0x11, val=0x00)
33: cond = (cond >= 0x32)
34: if cond: skip_tokens(1)
    # skip -> wait_for_space()
35: party_effect(sel=0x08, 11 01 0A 00 00)
36: wait_for_space()
37: clear_current_tile_event_flag()
38: end_script()
```

**Event 07** — triggers: (8,6)/DIR_W?

```hex
06 0b
```

```
00: show_text_popup_style_b(str[11] "No Entry!")
```

**Event 08** — triggers: (8,0)/DIR_N?, (8,1)/DIR_S?, (8,5)/DIR_S?, (9,5)/DIR_W?, (11,0)/DIR_N?, (12,5)/DIR_W?, (13,7)/DIR_N?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Private - Keep Out!")
```

**Event 09** — triggers: (6,13)/DIR_N?

```hex
06 0d
```

```
00: show_text_popup_style_b(str[13] "Dead End / and Death")
```

**Event 10** — triggers: (7,5)/ANY_DIR

```hex
2b 01 12 50 50 50 50 50 50 50 50 50 50 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=50 50 50 50 50 50 50 50 50 50, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 11** — triggers: (7,1)/ANY_DIR

```hex
2b 01 12 70 70 70 70 70 70 7e 7e 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=70 70 70 70 70 70 7E 7E 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (9,0)/ANY_DIR

```hex
2b 01 12 77 77 77 77 77 77 77 77 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=77 77 77 77 77 77 77 77 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (9,4)/ANY_DIR

```hex
2b 01 12 5e 5e 5e 5e 5e 5e 8a 5e 5e 5e 5e 11 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=5E 5E 5E 5E 5E 5E 8A 5E 5E 5E, flags=5E 11)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (12,4)/ANY_DIR

```hex
2b 01 12 64 64 64 64 64 64 52 64 64 64 64 1e 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=64 64 64 64 64 64 52 64 64 64, flags=64 1E)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (12,0)/ANY_DIR

```hex
2b 01 12 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 14 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6E 6E 6E 6E 6E 6E 6E 6E 6E 6E, flags=6E 14)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (14,7)/ANY_DIR

```hex
2b 01 12 6a 6a 6a 6a 6a 6a 6a 6a 6a 6a 6a 0a 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6A 6A 6A 6A 6A 6A 6A 6A 6A 6A, flags=6A 0A)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (5,5)/DIR_E?

```hex
05 0e 29
```

```
00: show_text_popup_style_a(str[14] "Buy tickets to the / Arena, the Monster / Bowl and the / Colosseum at yo")
01: set_abort_and_exit()
```

**Event 18** — triggers: (7,15)/DIR_N?

```hex
02 0f 29
```

```
00: show_text_block(str[15] "Buy a key from a locksmith and win a / battle of the same color in each ")
01: set_abort_and_exit()
```

**Event 19** — triggers: (13,1)/DIR_N?+DIR_E?

```hex
0e 70
```

```
00: exec_selector(0x70)
```

**Event 20** — triggers: (13,6)/DIR_S?

```hex
0e 75
```

```
00: exec_selector(0x75)
```

**Event 21** — triggers: (6,9)/DIR_E?

```hex
0e 76
```

```
00: exec_selector(0x76)
```

## String table

- `[00]` <EMPTY>
- `[01]` Exit to Atlantium. Ascend (y/n)?
- `[02]` A marble statue of a winged beast has / a button protruding from its / pedestal. Press it (y/n)?
- `[03]` Poof!
- `[04]` An old rotting wooden statue appears / unstable. A small but sturdy button / extends from the nose. Press it (y/n)?
- `[05]` The statue explodes, releasing angry / monsters.
- `[06]` A metallic statue of an Atlantian / deity stares at you intensely. / Touch it (y/n)?
- `[07]` Its eyes appear to come alive and you / feel drained.
- `[08]` Everytime you look at this statue it / appears to be moving. Touch it (y/n)?
- `[09]` A statue of a renowned scholar kneels / with a book in hand. Read pages (y/n)?
- `[10]` After reading the pages you are / enlightened.
- `[11]` No Entry!
- `[12]` Private - Keep Out!
- `[13]` Dead End / and Death
- `[14]` Buy tickets to the / Arena, the Monster / Bowl and the / Colosseum at your / local blacksmith. / Hurry, limited / time offer.
- `[15]` Buy a key from a locksmith and win a / battle of the same color in each / forum. Go then to the castles and find / a bishop of that key color's battle.
- `[16]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
