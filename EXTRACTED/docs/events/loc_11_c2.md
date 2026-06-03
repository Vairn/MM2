# Location 11 — C2

- **event.dat** offset `0x003EB4`, length **1475** bytes
- **Map:** map screen **11**; overland sector **C2**
- **Record kind:** `standard`
- **Triggers:** 25; **script segments:** 24; **strings:** 21

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,2) | `0x12` | **12** | 0x30 |
| (1,3) | `0x13` | **21** | ANY_DIR |
| (1,11) | `0x1B` | **5** | ALWAYS |
| (3,2) | `0x32` | **20** | ANY_DIR |
| (3,7) | `0x37` | **1** | DIR_N? |
| (4,1) | `0x41` | **20** | ANY_DIR |
| (4,2) | `0x42` | **20** | ANY_DIR |
| (4,4) | `0x44` | **14** | 0x30 |
| (4,7) | `0x47` | **4** | ENTER |
| (4,10) | `0x4A` | **6** | DIR_SPECIAL |
| (4,12) | `0x4C` | **8** | ALWAYS |
| (4,14) | `0x4E` | **18** | DIR_SPECIAL |
| (4,15) | `0x4F` | **19** | DIR_SPECIAL |
| (5,2) | `0x52` | **20** | ANY_DIR |
| (6,2) | `0x62` | **10** | DIR_N? |
| (7,7) | `0x77` | **15** | ENTER |
| (7,10) | `0x7A` | **2** | ENTER |
| (8,14) | `0x8E` | **22** | ENTER+SPECIAL |
| (10,14) | `0xAE` | **13** | DIR_SPECIAL |
| (11,5) | `0xB5` | **3** | ENTER |
| (12,7) | `0xC7` | **16** | ENTER |
| (12,10) | `0xCA` | **7** | DIR_SPECIAL |
| (12,12) | `0xCC` | **9** | ALWAYS |
| (13,1) | `0xD1` | **11** | ENTER |
| (13,3) | `0xD3` | **17** | ALWAYS |

## Events

**Event 01** — triggers: (3,7)/DIR_N?

```hex
0b 18 00 02 01 0a 11 01 0c 00 f5 0f
```

```
00: set_service_context(str[24], mode=0x00)
01: show_text_block(str[1] "A giant portcullis slowly raises, / offering admittance to Middlegate. /")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x00, 0xF5)
05: end_script()
```

**Event 02** — triggers: (7,10)/ENTER

```hex
02 02 09 11 05 15 00 7f 01 10 02 01 03 29 0c 17 00 0f
```

```
00: show_text_block(str[2] "A sign above this cave reads, / "Chosen Ones only." Enter (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: apply_party(count=0x00, op=0x7F, val=0x01)
04: if cond: skip_tokens(2)
    # skip -> map_transition(0x17, 0x00)
05: show_text_basic(str[3] "An invisible force repels you.")
06: set_abort_and_exit()
07: map_transition(0x17, 0x00)
08: end_script()
```

**Event 03** — triggers: (11,5)/ENTER

```hex
02 04 09 11 01 0c 16 d7 0f
```

```
00: show_text_block(str[4] "Revealed before you is the partially / collapsed entrance to legendary /")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x16, 0xD7)
04: end_script()
```

**Event 04** — triggers: (4,7)/ENTER

```hex
15 00 74 40 10 04 0b 0e 00 03 05 18 00 74 bf 40 08 14
```

```
00: apply_party(count=0x00, op=0x74, val=0x40)
01: if cond: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
02: set_service_context(str[14] "<- Castle / Pinehurst", mode=0x00)
03: show_text(str[5] ""Greetings! I'm your Guardian Pegasus. / Welcome to the outer world of C")
04: apply_party_masked(count=0x00, set=0x74, and=0xBF, or=0x40)
05: wait_key()
06: clear_current_tile_event_flag()
```

**Event 05** — triggers: (1,11)/ALWAYS

```hex
02 06 09 11 03 24 32 00 11 02 2e f0 08 14 01 07 29
```

```
00: show_text_block(str[6] "A curious looking fellow walks easily / over the turbulent river, yet no")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
03: cond = check_gold_at_least(50)
04: if not cond: skip_tokens(2)
    # skip -> show_text_basic(str[7] "Not enough gold!")
05: set_party_attr(class=0xF0, bits=0x08)
06: clear_current_tile_event_flag()
07: show_text_basic(str[7] "Not enough gold!")
08: set_abort_and_exit()
```

**Event 06** — triggers: (4,10)/DIR_SPECIAL

```hex
02 08 09 11 05 24 05 00 11 01 0c 0b 4c 01 07 29 0f
```

```
00: show_text_block(str[8] "Ferry boat crossing. 5 Gold one way. / Buy ticket (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: cond = check_gold_at_least(5)
04: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[7] "Not enough gold!")
05: map_transition(0x0B, 0x4C)
06: show_text_basic(str[7] "Not enough gold!")
07: set_abort_and_exit()
08: end_script()
```

**Event 07** — triggers: (12,10)/DIR_SPECIAL

```hex
02 08 09 11 05 24 05 00 11 01 0c 0b cc 01 07 29 0f
```

```
00: show_text_block(str[8] "Ferry boat crossing. 5 Gold one way. / Buy ticket (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: cond = check_gold_at_least(5)
04: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[7] "Not enough gold!")
05: map_transition(0x0B, 0xCC)
06: show_text_basic(str[7] "Not enough gold!")
07: set_abort_and_exit()
08: end_script()
```

**Event 08** — triggers: (4,12)/ALWAYS

```hex
02 08 09 11 05 24 05 00 11 01 0c 0b 4a 01 07 29 0f
```

```
00: show_text_block(str[8] "Ferry boat crossing. 5 Gold one way. / Buy ticket (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: cond = check_gold_at_least(5)
04: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[7] "Not enough gold!")
05: map_transition(0x0B, 0x4A)
06: show_text_basic(str[7] "Not enough gold!")
07: set_abort_and_exit()
08: end_script()
```

**Event 09** — triggers: (12,12)/ALWAYS

```hex
02 08 09 11 05 24 05 00 11 01 0c 0b ca 01 07 29 0f
```

```
00: show_text_block(str[8] "Ferry boat crossing. 5 Gold one way. / Buy ticket (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: cond = check_gold_at_least(5)
04: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[7] "Not enough gold!")
05: map_transition(0x0B, 0xCA)
06: show_text_basic(str[7] "Not enough gold!")
07: set_abort_and_exit()
08: end_script()
```

**Event 10** — triggers: (6,2)/DIR_N?

```hex
01 09 29
```

```
00: show_text_basic(str[9] "Orc foot prints are everywhere.")
01: set_abort_and_exit()
```

**Event 11** — triggers: (13,1)/ENTER

```hex
02 0a 09 11 01 18 00 24 00 28 0f
```

```
00: show_text_block(str[10] "A bubbling fountain stands before you. / Drink from it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x24, and=0x00, or=0x28)
04: end_script()
```

**Event 12** — triggers: (1,2)/0x30

```hex
02 0b 09 11 02 1c 0f 18 80 26 00 00 0f
```

```
00: show_text_block(str[11] "A moss-covered fountain stands before / you. Drink from it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: op_1c_engine_query_to_cond(0x0F)
04: apply_party_masked(count=0x80, set=0x26, and=0x00, or=0x00)
05: end_script()
```

**Event 13** — triggers: (10,14)/DIR_SPECIAL

```hex
02 0c 09 11 01 18 00 28 00 14 0f
```

```
00: show_text_block(str[12] "A mist rises from a 3-tiered / fountain. Drink from it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x28, and=0x00, or=0x14)
04: end_script()
```

**Event 14** — triggers: (4,4)/0x30

```hex
01 0d 09 11 01 13 00 00 00 00 00 00 00 00 00 00 0f
```

```
00: show_text_basic(str[13] "Magic Monster Pit. Jump in (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
04: end_script()
```

**Event 15** — triggers: (7,7)/ENTER

```hex
06 0e
```

```
00: show_text_popup_style_b(str[14] "<- Castle / Pinehurst")
```

**Event 16** — triggers: (12,7)/ENTER

```hex
06 0f
```

```
00: show_text_popup_style_b(str[15] "Vulcania / -[")
```

**Event 17** — triggers: (13,3)/ALWAYS

```hex
06 10
```

```
00: show_text_popup_style_b(str[16] "Tundara  [ / Woodhaven[")
```

**Event 18** — triggers: (4,14)/DIR_SPECIAL

```hex
06 11
```

```
00: show_text_popup_style_b(str[17] "Sandsobar[ / Hillstone[")
```

**Event 19** — triggers: (4,15)/DIR_SPECIAL

```hex
06 12
```

```
00: show_text_popup_style_b(str[18] "Luxus $ / Palace")
```

**Event 20** — triggers: (3,2)/ANY_DIR, (4,1)/ANY_DIR, (4,2)/ANY_DIR, (5,2)/ANY_DIR

```hex
2b 01 12 11 11 11 11 11 11 11 11 11 11 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=11 11 11 11 11 11 11 11 11 11, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 21** — triggers: (1,3)/ANY_DIR

```hex
2b 01 12 11 11 11 11 34 34 34 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=11 11 11 11 34 34 34 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 22** — triggers: (8,14)/ENTER+SPECIAL

```hex
22 08 08 11 03 2b 07 0b 07 00 0e 5a 22 09 09 10 01 14 02 13 29 0c 3b 80
```

```
00: cond = (era in [8..8])
01: if not cond: skip_tokens(3)
    # skip -> cond = (era in [9..9])
02: skip_tokens(7)
    # skip -> map_transition(0x3B, 0x80)
03: set_service_context(str[7] "Not enough gold!", mode=0x00)
04: exec_selector(0x5A)
05: cond = (era in [9..9])
06: if cond: skip_tokens(1)
    # skip -> show_text_block(str[19] "Ruins of a mighty castle are strewn / across the area.")
07: clear_current_tile_event_flag()
08: show_text_block(str[19] "Ruins of a mighty castle are strewn / across the area.")
09: set_abort_and_exit()
10: map_transition(0x3B, 0x80)
```

## String table

- `[00]` <EMPTY>
- `[01]` A giant portcullis slowly raises, / offering admittance to Middlegate. / Enter (y/n)?
- `[02]` A sign above this cave reads, / "Chosen Ones only." Enter (y/n)?
- `[03]` An invisible force repels you.
- `[04]` Revealed before you is the partially / collapsed entrance to legendary / Corak's Cave. Pass through (y/n)?
- `[05]` "Greetings! I'm your Guardian Pegasus. / Welcome to the outer world of Cron. / Beyond these town walls lie dangers / and wonders! Should you discover my / name, I will help you once per year. / Seek me out in the Ice Tundra."
- `[06]` A curious looking fellow walks easily / over the turbulent river, yet no / bridge is visible! "I'll teach you my / secret ways for 50 gold. Pay (y/n)?"
- `[07]` Not enough gold!
- `[08]` Ferry boat crossing. 5 Gold one way. / Buy ticket (y/n)?
- `[09]` Orc foot prints are everywhere.
- `[10]` A bubbling fountain stands before you. / Drink from it (y/n)?
- `[11]` A moss-covered fountain stands before / you. Drink from it (y/n)?
- `[12]` A mist rises from a 3-tiered / fountain. Drink from it (y/n)?
- `[13]` Magic Monster Pit. Jump in (y/n)?
- `[14]` <- Castle / Pinehurst
- `[15]` Vulcania / -[
- `[16]` Tundara  [ / Woodhaven[
- `[17]` Sandsobar[ / Hillstone[
- `[18]` Luxus $ / Palace
- `[19]` Ruins of a mighty castle are strewn / across the area.
- `[20]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
