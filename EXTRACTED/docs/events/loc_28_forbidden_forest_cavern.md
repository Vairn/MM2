# Location 28 — Forbidden Forest Cavern

- **event.dat** offset `0x009545`, length **926** bytes
- **Map:** map screen **28**; Forbidden Forest Cavern
- **Record kind:** `standard`
- **Triggers:** 66; **script segments:** 32; **strings:** 17

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,8) | `0x08` | **14** | DIR_E? |
| (1,3) | `0x13` | **22** | ANY_DIR |
| (1,6) | `0x16` | **24** | ANY_DIR |
| (1,8) | `0x18` | **13** | DIR_E? |
| (1,11) | `0x1B` | **26** | ANY_DIR |
| (1,12) | `0x1C` | **25** | ANY_DIR |
| (1,15) | `0x1F` | **27** | ANY_DIR |
| (2,8) | `0x28` | **12** | DIR_E? |
| (3,0) | `0x30` | **10** | DIR_S? |
| (3,1) | `0x31` | **10** | DIR_S? |
| (4,3) | `0x43` | **19** | ANY_DIR |
| (4,4) | `0x44` | **19** | ANY_DIR |
| (4,5) | `0x45` | **19** | ANY_DIR |
| (4,6) | `0x46` | **19** | ANY_DIR |
| (4,7) | `0x47` | **19** | ANY_DIR |
| (4,8) | `0x48` | **19** | ANY_DIR |
| (4,9) | `0x49` | **19** | ANY_DIR |
| (4,10) | `0x4A` | **19** | ANY_DIR |
| (4,11) | `0x4B` | **19** | ANY_DIR |
| (4,12) | `0x4C` | **19** | ANY_DIR |
| (4,13) | `0x4D` | **19** | ANY_DIR |
| (5,14) | `0x5E` | **6** | DIR_N? |
| (6,4) | `0x64` | **8** | DIR_S? |
| (6,13) | `0x6D` | **20** | ANY_DIR |
| (7,0) | `0x70` | **15** | DIR_W? |
| (7,2) | `0x72` | **21** | ANY_DIR |
| (7,3) | `0x73` | **9** | DIR_W? |
| (7,5) | `0x75` | **16** | ANY_DIR |
| (7,6) | `0x76` | **5** | DIR_W? |
| (7,7) | `0x77` | **1** | DIR_E? |
| (7,11) | `0x7B` | **18** | ANY_DIR |
| (7,13) | `0x7D` | **7** | DIR_W? |
| (8,0) | `0x80` | **15** | DIR_W? |
| (8,2) | `0x82` | **21** | ANY_DIR |
| (8,3) | `0x83` | **9** | DIR_W? |
| (8,5) | `0x85` | **16** | ANY_DIR |
| (8,6) | `0x86` | **5** | DIR_W? |
| (8,7) | `0x87` | **1** | DIR_E? |
| (8,8) | `0x88` | **3** | DIR_S? |
| (8,13) | `0x8D` | **7** | DIR_W? |
| (8,15) | `0x8F` | **4** | ANY_DIR |
| (9,4) | `0x94` | **8** | DIR_N? |
| (9,8) | `0x98` | **2** | DIR_S? |
| (9,13) | `0x9D` | **20** | ANY_DIR |
| (10,14) | `0xAE` | **6** | DIR_S? |
| (11,3) | `0xB3` | **17** | ANY_DIR |
| (11,4) | `0xB4` | **17** | ANY_DIR |
| (11,5) | `0xB5` | **17** | ANY_DIR |
| (11,6) | `0xB6` | **17** | ANY_DIR |
| (11,7) | `0xB7` | **17** | ANY_DIR |
| (11,8) | `0xB8` | **17** | ANY_DIR |
| (11,9) | `0xB9` | **17** | ANY_DIR |
| (11,10) | `0xBA` | **17** | ANY_DIR |
| (11,11) | `0xBB` | **17** | ANY_DIR |
| (11,12) | `0xBC` | **17** | ANY_DIR |
| (11,13) | `0xBD` | **17** | ANY_DIR |
| (12,0) | `0xC0` | **11** | DIR_N? |
| (12,1) | `0xC1` | **11** | DIR_N? |
| (13,8) | `0xD8` | **12** | DIR_E? |
| (14,3) | `0xE3` | **23** | ANY_DIR |
| (14,6) | `0xE6` | **24** | ANY_DIR |
| (14,8) | `0xE8` | **13** | DIR_E? |
| (14,11) | `0xEB` | **29** | ANY_DIR |
| (14,12) | `0xEC` | **28** | ANY_DIR |
| (14,15) | `0xEF` | **30** | ANY_DIR |
| (15,8) | `0xF8` | **14** | DIR_E? |

## Events

**Event 01** — triggers: (7,7)/DIR_E?, (8,7)/DIR_E?

```hex
02 01 09 10 01 0f 0c 0e 0f
```

```
00: show_text_block(str[1] "A reassuring light streams from the / exit to the outside. Go through (y")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x0E, 0x0F)
03: end_script()
04: map_transition(0x0E, 0x0F)
```

**Event 02** — triggers: (9,8)/DIR_S?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Paladins Only!")
```

**Event 03** — triggers: (8,8)/DIR_S?

```hex
2b 03 2d 01 05 11 05 12 94 00 00 00 00 00 00 00 00 00 00 00 18 00 75 fe 01 02 0f 07 14 02 03 07 0d 09 0c 1c 8f
```

```
00: skip_tokens(3)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
01: cond = check_member_attr(fields=0x01, value=0x05)
02: if not cond: skip_tokens(5)
    # skip -> show_text_block(str[3] "The sign says, "Paladins Only!" Can't / you read!")
03: encounter_setup(monsters=94 00 00 00 00 00 00 00 00 00, flags=00 00)
04: apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
05: show_text_block(str[15] "You have proven yourself worthy. Now, / return to the jurors of Mount Fa")
06: wait_for_space()
07: clear_current_tile_event_flag()
08: show_text_block(str[3] "The sign says, "Paladins Only!" Can't / you read!")
09: wait_for_space()
10: engine_call(0x09)
11: map_transition(0x1C, 0x8F)
```

**Event 04** — triggers: (8,15)/ANY_DIR

```hex
2b 01 13 b4 22 32 44 47 51 54 65 67 6e 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup_b(data=B4 22 32 44 47 51 54 65 67 6E)
02: clear_current_tile_event_flag()
```

**Event 05** — triggers: (7,6)/DIR_W?, (8,6)/DIR_W?

```hex
04 04
```

```
00: show_text_above_door(str[4] "Army Barracks")
```

**Event 06** — triggers: (5,14)/DIR_N?, (10,14)/DIR_S?

```hex
04 05
```

```
00: show_text_above_door(str[5] "G.H.Q.")
```

**Event 07** — triggers: (7,13)/DIR_W?, (8,13)/DIR_W?

```hex
04 06
```

```
00: show_text_above_door(str[6] "War Room")
```

**Event 08** — triggers: (6,4)/DIR_S?, (9,4)/DIR_N?

```hex
04 07
```

```
00: show_text_above_door(str[7] "Infantry")
```

**Event 09** — triggers: (7,3)/DIR_W?, (8,3)/DIR_W?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Legions")
```

**Event 10** — triggers: (3,0)/DIR_S?, (3,1)/DIR_S?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Seventh Legion")
```

**Event 11** — triggers: (12,0)/DIR_N?, (12,1)/DIR_N?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Fifth Legion")
```

**Event 12** — triggers: (2,8)/DIR_E?, (13,8)/DIR_E?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "M.P.'s")
```

**Event 13** — triggers: (1,8)/DIR_E?, (14,8)/DIR_E?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "The Sarge")
```

**Event 14** — triggers: (0,8)/DIR_E?, (15,8)/DIR_E?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Command")
```

**Event 15** — triggers: (7,0)/DIR_W?, (8,0)/DIR_W?

```hex
05 0e
```

```
00: show_text_popup_style_a(str[14] "No tasteless Orc / jokes allowed... /      - The Sarge")
```

**Event 16** — triggers: (7,5)/ANY_DIR, (8,5)/ANY_DIR

```hex
2b 01 12 44 44 44 44 44 44 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=44 44 44 44 44 44 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (11,3)/ANY_DIR, (11,4)/ANY_DIR, (11,5)/ANY_DIR, (11,6)/ANY_DIR, (11,7)/ANY_DIR, (11,8)/ANY_DIR, (11,9)/ANY_DIR, (11,10)/ANY_DIR, (11,11)/ANY_DIR, (11,12)/ANY_DIR, (11,13)/ANY_DIR

```hex
2b 01 12 05 05 05 05 05 05 05 05 05 05 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=05 05 05 05 05 05 05 05 05 05, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (7,11)/ANY_DIR

```hex
2b 01 12 67 67 67 67 67 67 47 47 47 47 47 0b 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=67 67 67 67 67 67 47 47 47 47, flags=47 0B)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (4,3)/ANY_DIR, (4,4)/ANY_DIR, (4,5)/ANY_DIR, (4,6)/ANY_DIR, (4,7)/ANY_DIR, (4,8)/ANY_DIR, (4,9)/ANY_DIR, (4,10)/ANY_DIR, (4,11)/ANY_DIR, (4,12)/ANY_DIR, (4,13)/ANY_DIR

```hex
2b 01 12 11 11 11 11 11 11 11 11 11 11 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=11 11 11 11 11 11 11 11 11 11, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 20** — triggers: (6,13)/ANY_DIR, (9,13)/ANY_DIR

```hex
2b 01 12 65 65 65 6e 6e 6e 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=65 65 65 6E 6E 6E 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 21** — triggers: (7,2)/ANY_DIR, (8,2)/ANY_DIR

```hex
2b 01 12 32 32 32 32 32 32 32 32 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=32 32 32 32 32 32 32 32 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 22** — triggers: (1,3)/ANY_DIR

```hex
2b 01 12 2e 2e 2e 2e 2e 2e 2e 2e 2e 03 03 ef 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2E 2E 2E 2E 2E 2E 2E 2E 2E 03, flags=03 EF)
02: clear_current_tile_event_flag()
```

**Event 23** — triggers: (14,3)/ANY_DIR

```hex
2b 01 12 05 05 05 05 05 05 05 05 05 05 05 f0 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=05 05 05 05 05 05 05 05 05 05, flags=05 F0)
02: clear_current_tile_event_flag()
```

**Event 24** — triggers: (1,6)/ANY_DIR, (14,6)/ANY_DIR

```hex
2b 01 12 59 59 59 59 11 11 11 11 11 11 11 f4 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=59 59 59 59 11 11 11 11 11 11, flags=11 F4)
02: clear_current_tile_event_flag()
```

**Event 25** — triggers: (1,12)/ANY_DIR

```hex
2b 01 12 22 22 22 22 22 22 22 22 22 22 22 14 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=22 22 22 22 22 22 22 22 22 22, flags=22 14)
02: clear_current_tile_event_flag()
```

**Event 26** — triggers: (1,11)/ANY_DIR

```hex
2b 01 12 cc 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=CC 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 27** — triggers: (1,15)/ANY_DIR

```hex
2b 01 12 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 0f 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6E 6E 6E 6E 6E 6E 6E 6E 6E 6E, flags=6E 0F)
02: clear_current_tile_event_flag()
```

**Event 28** — triggers: (14,12)/ANY_DIR

```hex
2b 01 12 51 51 51 51 51 51 51 51 51 51 51 0f 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=51 51 51 51 51 51 51 51 51 51, flags=51 0F)
02: clear_current_tile_event_flag()
```

**Event 29** — triggers: (14,11)/ANY_DIR

```hex
2b 01 12 ca 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=CA 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 30** — triggers: (14,15)/ANY_DIR

```hex
2b 01 12 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 0f 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6E 6E 6E 6E 6E 6E 6E 6E 6E 6E, flags=6E 0F)
02: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` A reassuring light streams from the / exit to the outside. Go through (y/n)?
- `[02]` Paladins Only!
- `[03]` The sign says, "Paladins Only!" Can't / you read!
- `[04]` Army Barracks
- `[05]` G.H.Q.
- `[06]` War Room
- `[07]` Infantry
- `[08]` Legions
- `[09]` Seventh Legion
- `[10]` Fifth Legion
- `[11]` M.P.'s
- `[12]` The Sarge
- `[13]` Command
- `[14]` No tasteless Orc / jokes allowed... /      - The Sarge
- `[15]` You have proven yourself worthy. Now, / return to the jurors of Mount Farview.
- `[16]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
