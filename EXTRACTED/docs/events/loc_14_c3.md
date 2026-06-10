# Location 14 — C3

- **event.dat** offset `0x004F60`, length **1458** bytes
- **Map:** map screen **14**; overland sector **C3**
- **Record kind:** `standard`
- **Triggers:** 44; **script segments:** 29; **strings:** 22

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **12** | ANY_DIR |
| (0,4) | `0x04` | **16** | ANY_DIR |
| (0,6) | `0x06` | **15** | ANY_DIR |
| (0,10) | `0x0A` | **17** | ANY_DIR |
| (0,15) | `0x0F` | **5** | ANY_DIR |
| (1,2) | `0x12` | **12** | ANY_DIR |
| (1,7) | `0x17` | **14** | ANY_DIR |
| (1,8) | `0x18` | **16** | ANY_DIR |
| (1,10) | `0x1A` | **14** | ANY_DIR |
| (1,13) | `0x1D` | **14** | ANY_DIR |
| (2,1) | `0x21` | **11** | ANY_DIR |
| (2,6) | `0x26` | **13** | ANY_DIR |
| (2,9) | `0x29` | **13** | ANY_DIR |
| (2,12) | `0x2C` | **13** | ANY_DIR |
| (2,14) | `0x2E` | **13** | ANY_DIR |
| (2,15) | `0x2F` | **15** | ANY_DIR |
| (3,4) | `0x34` | **9** | DIR_W? |
| (3,14) | `0x3E` | **10** | DIR_S? |
| (4,0) | `0x40` | **11** | ANY_DIR |
| (4,2) | `0x42` | **11** | ANY_DIR |
| (5,2) | `0x52` | **11** | ANY_DIR |
| (5,3) | `0x53` | **2** | ANY_DIR |
| (5,7) | `0x57` | **12** | ANY_DIR |
| (6,1) | `0x61` | **1** | DIR_N? |
| (6,9) | `0x69` | **12** | ANY_DIR |
| (7,0) | `0x70` | **6** | ANY_DIR |
| (7,1) | `0x71` | **11** | ANY_DIR |
| (7,3) | `0x73` | **3** | ANY_DIR |
| (7,5) | `0x75` | **11** | ANY_DIR |
| (8,1) | `0x81` | **4** | ANY_DIR |
| (8,11) | `0x8B` | **12** | ANY_DIR |
| (9,1) | `0x91` | **7** | ANY_DIR |
| (9,3) | `0x93` | **11** | ANY_DIR |
| (9,6) | `0x96` | **19** | DIR_W? |
| (9,7) | `0x97` | **18** | DIR_W? |
| (10,6) | `0xA6` | **20** | DIR_W? |
| (11,0) | `0xB0` | **27** | DIR_W? |
| (11,1) | `0xB1` | **26** | DIR_W? |
| (11,2) | `0xB2` | **25** | DIR_W? |
| (11,3) | `0xB3` | **24** | DIR_W? |
| (11,4) | `0xB4` | **23** | DIR_W? |
| (11,5) | `0xB5` | **22** | DIR_W? |
| (11,6) | `0xB6` | **21** | DIR_W? |
| (15,11) | `0xFB` | **8** | ANY_DIR |

## Events

**Event 01** — triggers: (6,1)/DIR_N?

```hex
0e 5b
```

```
00: exec_selector(0x5B)
```

**Event 02** — triggers: (5,3)/ANY_DIR

```hex
01 02 29
```

```
00: show_text_basic(str[2] "Etched onto a huge stone is an "I".")
01: set_abort_and_exit()
```

**Event 03** — triggers: (7,3)/ANY_DIR

```hex
01 03 29
```

```
00: show_text_basic(str[3] "Etched onto a huge stone is a "D".")
01: set_abort_and_exit()
```

**Event 04** — triggers: (8,1)/ANY_DIR

```hex
01 04 29
```

```
00: show_text_basic(str[4] "Etched onto a huge stone is an "S".")
01: set_abort_and_exit()
```

**Event 05** — triggers: (0,15)/ANY_DIR

```hex
02 05 09 10 01 0f 0c 1c 77
```

```
00: show_text_block(str[5] "A cold draft emerges from the opening / of a web-strewn shaft. Go down (")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x1C, 0x77)
03: end_script()
04: map_transition(0x1C, 0x77)
```

**Event 06** — triggers: (7,0)/ANY_DIR

```hex
32 09 10 02 02 06 29 03 07 29
```

```
00: cond = load_cond_from_var(0x09)
01: if cond: skip_tokens(2)
    # skip -> show_text(str[7] "Strange pagan symbols and runes carved / on the trees of the Oak Grove a")
02: show_text_block(str[6] "Strange pagan symbols and runes carved / on the trees of the Oak Grove b")
03: set_abort_and_exit()
04: show_text(str[7] "Strange pagan symbols and runes carved / on the trees of the Oak Grove a")
05: set_abort_and_exit()
```

**Event 07** — triggers: (9,1)/ANY_DIR

```hex
15 00 77 80 11 01 0e 59 02 0a 29
```

```
00: apply_party(count=0x00, op=0x77, val=0x80)
01: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[10] "An old druid pays you no heed as he / munches enthusiastically on Red Ho")
02: exec_selector(0x59)
03: show_text_block(str[10] "An old druid pays you no heed as he / munches enthusiastically on Red Ho")
04: set_abort_and_exit()
```

**Event 08** — triggers: (15,11)/ANY_DIR

```hex
02 0b 09 10 01 0f 0c 40 00
```

```
00: show_text_block(str[11] "A passage leads behind the falls. / Take it (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x40, 0x00)
03: end_script()
04: map_transition(0x40, 0x00)
```

**Event 09** — triggers: (3,4)/DIR_W?

```hex
06 0c
```

```
00: show_text_popup_style_b(str[12] "Druids / Only!")
```

**Event 10** — triggers: (3,14)/DIR_S?

```hex
06 0d
```

```
00: show_text_popup_style_b(str[13] "<Sandsobar / <Hillstone")
```

**Event 11** — triggers: (2,1)/ANY_DIR, (4,0)/ANY_DIR, (4,2)/ANY_DIR, (5,2)/ANY_DIR, (7,1)/ANY_DIR, (7,5)/ANY_DIR, (9,3)/ANY_DIR

```hex
2b 01 12 4a 4a 4a 4a 4a 4a 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=4A 4A 4A 4A 4A 4A 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (0,0)/ANY_DIR, (1,2)/ANY_DIR, (5,7)/ANY_DIR, (6,9)/ANY_DIR, (8,11)/ANY_DIR

```hex
2b 01 12 9b 9b 9b 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=9B 9B 9B 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (2,6)/ANY_DIR, (2,9)/ANY_DIR, (2,12)/ANY_DIR, (2,14)/ANY_DIR

```hex
2b 01 12 39 39 39 39 39 39 39 39 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=39 39 39 39 39 39 39 39 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (1,7)/ANY_DIR, (1,10)/ANY_DIR, (1,13)/ANY_DIR

```hex
2b 01 12 5b 5b 5b 5b 5b 5b 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=5B 5B 5B 5B 5B 5B 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (0,6)/ANY_DIR, (2,15)/ANY_DIR

```hex
2b 01 12 49 49 49 49 49 49 49 49 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=49 49 49 49 49 49 49 49 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (0,4)/ANY_DIR, (1,8)/ANY_DIR

```hex
2b 01 12 57 57 57 57 57 57 57 57 57 57 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=57 57 57 57 57 57 57 57 57 57, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (0,10)/ANY_DIR

```hex
2b 01 12 39 39 5b 5b 49 49 57 57 57 57 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=39 39 5B 5B 49 49 57 57 57 57, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (9,7)/DIR_W?

```hex
01 08 15 00 7b 04 06 01 10 02 02 10 29 02 11 09 10 01 0f 02 12 1e c8 18 00 7b f3 08 0d 09 0c 0e 96
```

```
00: show_text_basic(str[8] "Cruise to Murray's Resort Isle.")
01: apply_party(count=0x00, op=0x7B, val=0x04)
02: show_text_popup_style_b(str[1] "Must be [ / this tall")
03: if cond: skip_tokens(2)
    # skip -> show_text_block(str[17] ""Welcome aboard folks! I'm Fej, your / cruise director. A boat is just a")
04: show_text_block(str[16] "If you wish to take the cruise, please / make arrangements with travel a")
05: set_abort_and_exit()
06: show_text_block(str[17] ""Welcome aboard folks! I'm Fej, your / cruise director. A boat is just a")
07: cond = prompt_yes_no()
08: if cond: skip_tokens(1)
    # skip -> show_text_block(str[18] "Please remain seated at all times. / This is an E-ticket ride.")
09: end_script()
10: show_text_block(str[18] "Please remain seated at all times. / This is an E-ticket ride.")
11: delay(0xC8)
12: apply_party_masked(count=0x00, set=0x7B, and=0xF3, or=0x08)
13: engine_call(0x09)
14: map_transition(0x0E, 0x96)
```

**Event 19** — triggers: (9,6)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0e a6
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0E, 0xA6)
```

**Event 20** — triggers: (10,6)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0e b6
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0E, 0xB6)
```

**Event 21** — triggers: (11,6)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0e b5
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0E, 0xB5)
```

**Event 22** — triggers: (11,5)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0e b4
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0E, 0xB4)
```

**Event 23** — triggers: (11,4)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0e b3
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0E, 0xB3)
```

**Event 24** — triggers: (11,3)/DIR_W?

```hex
15 00 7b 08 10 01 14 01 13 1e c8 0d 09 0c 0e b2
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[19] "To your left is Druid's Point.")
02: clear_current_tile_event_flag()
03: show_text_basic(str[19] "To your left is Druid's Point.")
04: delay(0xC8)
05: engine_call(0x09)
06: map_transition(0x0E, 0xB2)
```

**Event 25** — triggers: (11,2)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0e b1
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0E, 0xB1)
```

**Event 26** — triggers: (11,1)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0e b0
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0E, 0xB0)
```

**Event 27** — triggers: (11,0)/DIR_W?

```hex
15 00 7b 08 10 01 14 0d 09 0c 0d bf
```

```
00: apply_party(count=0x00, op=0x7B, val=0x08)
01: if cond: skip_tokens(1)
    # skip -> engine_call(0x09)
02: clear_current_tile_event_flag()
03: engine_call(0x09)
04: map_transition(0x0D, 0xBF)
```

## String table

- `[00]` <EMPTY>
- `[01]` Must be [ / this tall
- `[02]` Etched onto a huge stone is an "I".
- `[03]` Etched onto a huge stone is a "D".
- `[04]` Etched onto a huge stone is an "S".
- `[05]` A cold draft emerges from the opening / of a web-strewn shaft. Go down (y/n)?
- `[06]` Strange pagan symbols and runes carved / on the trees of the Oak Grove baffle / the party.
- `[07]` Strange pagan symbols and runes carved / on the trees of the Oak Grove are / deciphered by your Linguist. The / message reads "The Pegasus is called / Meenu."
- `[08]` Cruise to Murray's Resort Isle.
- `[09]` <SENTINEL_Z>
- `[10]` An old druid pays you no heed as he / munches enthusiastically on Red Hot / Wolf Nipple Chips.
- `[11]` A passage leads behind the falls. / Take it (y/n)?
- `[12]` Druids / Only!
- `[13]` <Sandsobar / <Hillstone
- `[14]` A
- `[15]` A
- `[16]` If you wish to take the cruise, please / make arrangements with travel agent.
- `[17]` "Welcome aboard folks! I'm Fej, your / cruise director. A boat is just about / to leave. Would you like to begin / your trip (y/n)?"
- `[18]` Please remain seated at all times. / This is an E-ticket ride.
- `[19]` To your left is Druid's Point.
- `[20]` To your right is Jousters Way.
- `[21]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
