# Location 35 — E2

- **event.dat** offset `0x00B9F4`, length **802** bytes
- **Map:** map screen **35**; overland sector **E2**
- **Record kind:** `standard`
- **Triggers:** 21; **script segments:** 15; **strings:** 11

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,3) | `0x13` | **2** | ANY_DIR |
| (1,10) | `0x1A` | **2** | ANY_DIR |
| (4,5) | `0x45` | **10** | ANY_DIR |
| (4,14) | `0x4E` | **2** | 0xD0 |
| (6,4) | `0x64` | **2** | ANY_DIR |
| (6,11) | `0x6B` | **1** | ANY_DIR |
| (7,3) | `0x73` | **2** | ANY_DIR |
| (9,11) | `0x9B` | **5** | ANY_DIR |
| (12,1) | `0xC1` | **11** | ANY_DIR |
| (12,2) | `0xC2` | **12** | ANY_DIR |
| (12,4) | `0xC4` | **9** | ANY_DIR |
| (12,7) | `0xC7` | **3** | ANY_DIR |
| (13,4) | `0xD4` | **8** | ANY_DIR |
| (13,5) | `0xD5` | **8** | ANY_DIR |
| (13,6) | `0xD6` | **8** | ANY_DIR |
| (13,7) | `0xD7` | **8** | ANY_DIR |
| (14,9) | `0xE9` | **6** | ANY_DIR |
| (14,10) | `0xEA` | **7** | ANY_DIR |
| (14,11) | `0xEB` | **4** | ANY_DIR |
| (14,14) | `0xEE` | **11** | ANY_DIR |
| (15,1) | `0xF1` | **13** | ANY_DIR |

## Events

**Event 01** — triggers: (6,11)/ANY_DIR

```hex
2b 05 15 00 7a 40 11 01 14 02 05 12 f2 00 00 00 00 00 00 00 00 00 00 00 18 00 7a bf 40 14
```

```
00: skip_tokens(5)
    # skip -> apply_party_masked(count=0x00, set=0x7A, and=0xBF, or=0x40)
01: apply_party(count=0x00, op=0x7A, val=0x40)
02: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[5] "The Crawling Envoy of Evil attacks!")
03: clear_current_tile_event_flag()
04: show_text_block(str[5] "The Crawling Envoy of Evil attacks!")
05: encounter_setup(monsters=F2 00 00 00 00 00 00 00 00 00, flags=00 00)
06: apply_party_masked(count=0x00, set=0x7A, and=0xBF, or=0x40)
07: clear_current_tile_event_flag()
```

**Event 02** — triggers: (1,3)/ANY_DIR, (1,10)/ANY_DIR, (4,14)/0xD0, (6,4)/ANY_DIR, (7,3)/ANY_DIR

```hex
02 01 09 11 01 18 00 42 00 28 0f
```

```
00: show_text_block(str[1] "In the midst of the desert is an / oasis. Large fruit trees bear invitin")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x42, and=0x00, or=0x28)
04: end_script()
```

**Event 03** — triggers: (12,7)/ANY_DIR

```hex
06 02
```

```
00: show_text_popup_style_b(str[2] "Dino / Ranch")
```

**Event 04** — triggers: (14,11)/ANY_DIR

```hex
06 03
```

```
00: show_text_popup_style_b(str[3] "Camp / Kill-U")
```

**Event 05** — triggers: (9,11)/ANY_DIR

```hex
02 04 09 11 08 18 00 22 00 c8 18 00 23 00 c8 18 00 24 00 c8 18 00 26 00 32 18 00 2a 00 c8 18 00 2b 00 c8 18 00 2c 00 c8 18 00 2d 00 c8 0f
```

```
00: show_text_block(str[4] "Nestled between tall trees flows / The Greatest Fountain. Drink from its")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(8)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x22, and=0x00, or=0xC8)
04: apply_party_masked(count=0x00, set=0x23, and=0x00, or=0xC8)
05: apply_party_masked(count=0x00, set=0x24, and=0x00, or=0xC8)
06: apply_party_masked(count=0x00, set=0x26, and=0x00, or=0x32)
07: apply_party_masked(count=0x00, set=0x2A, and=0x00, or=0xC8)
08: apply_party_masked(count=0x00, set=0x2B, and=0x00, or=0xC8)
09: apply_party_masked(count=0x00, set=0x2C, and=0x00, or=0xC8)
10: apply_party_masked(count=0x00, set=0x2D, and=0x00, or=0xC8)
11: end_script()
```

**Event 06** — triggers: (14,9)/ANY_DIR

```hex
2b 01 13 dd ca 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup_b(data=DD CA 00 00 00 00 00 00 00 00)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (14,10)/ANY_DIR

```hex
2b 01 13 cc 8c 8c 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup_b(data=CC 8C 8C 00 00 00 00 00 00 00)
02: clear_current_tile_event_flag()
```

**Event 08** — triggers: (13,4)/ANY_DIR, (13,5)/ANY_DIR, (13,6)/ANY_DIR, (13,7)/ANY_DIR

```hex
2b 01 12 b5 95 95 95 95 95 95 95 95 0f 0f 13 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=B5 95 95 95 95 95 95 95 95 0F, flags=0F 13)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (12,4)/ANY_DIR

```hex
2b 01 12 d3 b5 b5 95 95 95 95 95 0f 0f 0f c6 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=D3 B5 B5 95 95 95 95 95 0F 0F, flags=0F C6)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (4,5)/ANY_DIR

```hex
22 07 07 11 0c 2b 06 02 06 09 10 02 02 07 29 12 e9 4b 4b 4b 4b 4b 4b 4b 4b 4b 4b 15 02 08 19 01 e1 00 00 10 01 01 09 07 14
```

```
00: cond = (era in [7..7])
01: if not cond: skip_tokens(12)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(6)
    # skip -> show_text_block(str[8] "You strip the loincloth off his body.")
03: show_text_block(str[6] "The charismatic shaman known as The / Long One kneels before you. He lea")
04: cond = prompt_yes_no()
05: if cond: skip_tokens(2)
    # skip -> encounter_setup(monsters=E9 4B 4B 4B 4B 4B 4B 4B 4B 4B, flags=4B 15)
06: show_text_block(str[7] "They continue to meditate peacefully.")
07: set_abort_and_exit()
08: encounter_setup(monsters=E9 4B 4B 4B 4B 4B 4B 4B 4B 4B, flags=4B 15)
09: show_text_block(str[8] "You strip the loincloth off his body.")
10: add_party_entity(0x01, f3a=0xE1, f40=0x00, f46=0x00)
11: if cond: skip_tokens(1)
    # skip -> wait_for_space()
12: show_text_basic(str[9] "*** Backpacks Full ***")
13: wait_for_space()
14: clear_current_tile_event_flag()
```

**Event 11** — triggers: (12,1)/ANY_DIR, (14,14)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 e0 e1 e1 c2 c2 c1 c2 c2 c2 c2 c1 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=E0 E1 E1 C2 C2 C1 C2 C2 C2 C2, flags=C1 03)
04: clear_current_tile_event_flag()
```

**Event 12** — triggers: (12,2)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 e1 c1 c2 c1 c2 ce c2 c2 ce 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=E1 C1 C2 C1 C2 CE C2 C2 CE 00)
04: clear_current_tile_event_flag()
```

**Event 13** — triggers: (15,1)/ANY_DIR

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

## String table

- `[00]` <EMPTY>
- `[01]` In the midst of the desert is an / oasis. Large fruit trees bear inviting / fruit. Pick (y/n)?
- `[02]` Dino / Ranch
- `[03]` Camp / Kill-U
- `[04]` Nestled between tall trees flows / The Greatest Fountain. Drink from its / renowned waters (y/n)?
- `[05]` The Crawling Envoy of Evil attacks!
- `[06]` The charismatic shaman known as The / Long One kneels before you. He leads / his followers in meditation. Do you / disrupt (y/n)?
- `[07]` They continue to meditate peacefully.
- `[08]` You strip the loincloth off his body.
- `[09]` *** Backpacks Full ***
- `[10]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
