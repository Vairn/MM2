# Location 17 — Middlegate Cavern

- **event.dat** offset `0x005F12`, length **1260** bytes
- **Map:** map screen **17**; Middlegate Cavern
- **Record kind:** `standard`
- **Triggers:** 26; **script segments:** 28; **strings:** 21

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **24** | DIR_S? |
| (0,1) | `0x01` | **2** | DIR_S? |
| (0,9) | `0x09` | **18** | DIR_S? |
| (0,14) | `0x0E` | **22** | DIR_S? |
| (1,2) | `0x12` | **7** | ANY_DIR |
| (2,15) | `0x2F` | **23** | DIR_S? |
| (6,3) | `0x63` | **19** | DIR_E? |
| (6,14) | `0x6E` | **8** | ANY_DIR |
| (7,0) | `0x70` | **3** | 0x30 |
| (8,1) | `0x81` | **5** | ANY_DIR |
| (8,2) | `0x82` | **15** | DIR_W? |
| (8,3) | `0x83` | **14** | ANY_DIR |
| (8,4) | `0x84` | **13** | ANY_DIR |
| (8,6) | `0x86` | **20** | DIR_E? |
| (8,11) | `0x8B` | **26** | DIR_W? |
| (8,12) | `0x8C` | **16** | DIR_W? |
| (8,15) | `0x8F` | **1** | DIR_E? |
| (10,3) | `0xA3` | **21** | DIR_E? |
| (10,14) | `0xAE` | **9** | ANY_DIR |
| (11,0) | `0xB0` | **10** | ANY_DIR |
| (14,5) | `0xE5` | **17** | DIR_N? |
| (14,8) | `0xE8` | **25** | DIR_W? |
| (15,0) | `0xF0` | **4** | DIR_W? |
| (15,1) | `0xF1` | **6** | ANY_DIR |
| (15,3) | `0xF3` | **12** | ANY_DIR |
| (15,5) | `0xF5` | **11** | ANY_DIR |

## Events

**Event 01** — triggers: (8,15)/DIR_E?

```hex
02 01 09 11 01 0c 00 08 0f
```

```
00: show_text_block(str[1] "Stairs lead up to Middlegate. / Ascend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x00, 0x08)
04: end_script()
```

**Event 02** — triggers: (0,1)/DIR_S?

```hex
02 02 2a e8 03 00 00 00 00 00 00 00 00 00 00 00 00 07 14
```

```
00: show_text_block(str[2] "An 'X' marks the spot. Maybe / you should search.")
01: set_treasure(gold/exp=1000, gems=0, items=[0, 0, 0])
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 03** — triggers: (7,0)/0x30

```hex
15 00 76 01 11 06 02 03 19 01 e0 00 00 10 01 01 09 07 14
```

```
00: apply_party(count=0x00, op=0x76, val=0x01)
01: if not cond: skip_tokens(6)
02: show_text_block(str[3] "/ You have found a valuable Gold Goblet!")
03: add_party_entity(0x01, f3a=0xE0, f40=0x00, f46=0x00)
04: if cond: skip_tokens(1)
    # skip -> wait_for_space()
05: show_text_basic(str[9] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
```

**Event 04** — triggers: (15,0)/DIR_W?

```hex
17 00 00 10 06 15 00 76 04 11 04 02 04 07 18 00 76 e0 0a 0f 02 05 29
```

```
00: cond = load_var8(group=0x00, index=0x00)
01: if cond: skip_tokens(6)
    # skip -> show_text_block(str[5] "Empty manacles hang from the wall in / this torture chamber.")
02: apply_party(count=0x00, op=0x76, val=0x04)
03: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[5] "Empty manacles hang from the wall in / this torture chamber.")
04: show_text_block(str[4] "Drog and Sir Hyron thank you and flee.")
05: wait_for_space()
06: apply_party_masked(count=0x00, set=0x76, and=0xE0, or=0x0A)
07: end_script()
08: show_text_block(str[5] "Empty manacles hang from the wall in / this torture chamber.")
09: set_abort_and_exit()
```

**Event 05** — triggers: (8,1)/ANY_DIR

```hex
2b 01 12 11 11 11 11 11 11 1c 05 05 05 05 05 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=11 11 11 11 11 11 1C 05 05 05, flags=05 05)
02: clear_current_tile_event_flag()
```

**Event 06** — triggers: (15,1)/ANY_DIR

```hex
2b 01 12 02 02 2e 2e 03 03 03 03 03 03 03 04 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=02 02 2E 2E 03 03 03 03 03 03, flags=03 04)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (1,2)/ANY_DIR

```hex
2b 01 12 2d 2d 2d 2d 1a 1a 1a 1a 1a 1a 1a 02 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2D 2D 2D 2D 1A 1A 1A 1A 1A 1A, flags=1A 02)
02: clear_current_tile_event_flag()
```

**Event 08** — triggers: (6,14)/ANY_DIR

```hex
2b 01 12 14 14 14 14 14 14 14 14 14 14 14 32 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=14 14 14 14 14 14 14 14 14 14, flags=14 32)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (10,14)/ANY_DIR

```hex
2b 01 12 15 15 15 15 15 15 15 15 36 36 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=15 15 15 15 15 15 15 15 36 36, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (11,0)/ANY_DIR

```hex
2b 01 12 44 44 03 03 03 03 05 05 05 05 05 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=44 44 03 03 03 03 05 05 05 05, flags=05 00)
02: clear_current_tile_event_flag()
```

**Event 11** — triggers: (15,5)/ANY_DIR

```hex
2b 01 12 03 03 03 03 02 02 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=03 03 03 03 02 02 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (15,3)/ANY_DIR

```hex
2b 01 12 03 03 03 03 03 03 03 03 02 02 02 02 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=03 03 03 03 03 03 03 03 02 02, flags=02 02)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (8,4)/ANY_DIR

```hex
2b 01 12 05 05 05 05 05 05 05 05 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=05 05 05 05 05 05 05 05 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (8,3)/ANY_DIR

```hex
2b 01 12 05 05 05 05 05 05 05 05 05 05 11 04 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=05 05 05 05 05 05 05 05 05 05, flags=11 04)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (8,2)/DIR_W?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Goblin Lair")
```

**Event 16** — triggers: (8,12)/DIR_W?

```hex
06 07
```

```
00: show_text_popup_style_b(str[7] "DANGER! / BEWARE!")
```

**Event 17** — triggers: (14,5)/DIR_N?

```hex
06 08
```

```
00: show_text_popup_style_b(str[8] "Kobold HQ / Keep Out!")
```

**Event 18** — triggers: (0,9)/DIR_S?

```hex
05 0c
```

```
00: show_text_popup_style_a(str[12] "There are only 180 / days per year.")
```

**Event 19** — triggers: (6,3)/DIR_E?

```hex
05 0d
```

```
00: show_text_popup_style_a(str[13] "Win the Blackest / battles and you are / halfway to an / audience with Q")
```

**Event 20** — triggers: (8,6)/DIR_E?

```hex
05 0a
```

```
00: show_text_popup_style_a(str[10] "Lloyd, of Lloyd's / Beacon fame, was / last seen in Corak's / Cave at 7,")
```

**Event 21** — triggers: (10,3)/DIR_E?

```hex
05 0b
```

```
00: show_text_popup_style_a(str[11] "Seek Earth / Encasement at 14,1 / in the proper plane. / Do walk about f")
```

**Event 22** — triggers: (0,14)/DIR_S?

```hex
05 0e
```

```
00: show_text_popup_style_a(str[14] "Lord Haart's famous / ancestor, The Long / One, hangs out in / the 8th c")
```

**Event 23** — triggers: (2,15)/DIR_S?

```hex
02 0f 29
```

```
00: show_text_block(str[15] "A message appears briefly on the / floor: "The Water Disc rests at 15,0 ")
01: set_abort_and_exit()
```

**Event 24** — triggers: (0,0)/DIR_S?

```hex
05 10
```

```
00: show_text_popup_style_a(str[16] "Castle Pinehurst / keeps a multitude / of J-26 Fluxers / at 7,6.")
```

**Event 25** — triggers: (14,8)/DIR_W?

```hex
01 11 02 12 29
```

```
00: show_text_basic(str[17] "Green Interleave")
01: show_text_block(str[18] "One letter after another 2-1-3-4.")
02: set_abort_and_exit()
```

**Event 26** — triggers: (8,11)/DIR_W?

```hex
05 13
```

```
00: show_text_popup_style_a(str[19] "The moon phase / of Cron lasts / sixty days.")
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs lead up to Middlegate. / Ascend (y/n)?
- `[02]` An 'X' marks the spot. Maybe / you should search.
- `[03]`  / You have found a valuable Gold Goblet!
- `[04]` Drog and Sir Hyron thank you and flee.
- `[05]` Empty manacles hang from the wall in / this torture chamber.
- `[06]` Goblin Lair
- `[07]` DANGER! / BEWARE!
- `[08]` Kobold HQ / Keep Out!
- `[09]` *** Backpacks Full ***
- `[10]` Lloyd, of Lloyd's / Beacon fame, was / last seen in Corak's / Cave at 7,11.
- `[11]` Seek Earth / Encasement at 14,1 / in the proper plane. / Do walk about first.
- `[12]` There are only 180 / days per year.
- `[13]` Win the Blackest / battles and you are / halfway to an / audience with Queen / Lamanda.
- `[14]` Lord Haart's famous / ancestor, The Long / One, hangs out in / the 8th century in / E2 at 5,4.
- `[15]` A message appears briefly on the / floor: "The Water Disc rests at 15,0 / within Castle Xabran."
- `[16]` Castle Pinehurst / keeps a multitude / of J-26 Fluxers / at 7,6.
- `[17]` Green Interleave
- `[18]`   One letter after another 2-1-3-4.
- `[19]` The moon phase / of Cron lasts / sixty days.
- `[20]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
