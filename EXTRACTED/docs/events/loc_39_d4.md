# Location 39 — D4

- **event.dat** offset `0x00CA8D`, length **1050** bytes
- **Map:** map screen **39**; overland sector **D4**
- **Record kind:** `standard`
- **Triggers:** 45; **script segments:** 19; **strings:** 14

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **8** | ANY_DIR |
| (0,5) | `0x05` | **7** | ANY_DIR |
| (1,3) | `0x13` | **8** | ANY_DIR |
| (1,8) | `0x18` | **8** | ANY_DIR |
| (1,13) | `0x1D` | **2** | 0x60 |
| (2,1) | `0x21` | **7** | ANY_DIR |
| (2,9) | `0x29` | **8** | ANY_DIR |
| (2,12) | `0x2C` | **5** | ANY_DIR |
| (2,13) | `0x2D` | **13** | ANY_DIR |
| (3,5) | `0x35` | **6** | ANY_DIR |
| (3,7) | `0x37` | **6** | ANY_DIR |
| (3,13) | `0x3D` | **14** | ANY_DIR |
| (4,3) | `0x43` | **8** | ANY_DIR |
| (4,4) | `0x44` | **8** | ANY_DIR |
| (4,13) | `0x4D` | **15** | ANY_DIR |
| (5,1) | `0x51` | **7** | ANY_DIR |
| (5,8) | `0x58` | **5** | ANY_DIR |
| (5,11) | `0x5B` | **4** | ANY_DIR |
| (6,0) | `0x60` | **9** | ANY_DIR |
| (7,2) | `0x72` | **8** | ANY_DIR |
| (7,3) | `0x73` | **1** | DIR_N? |
| (7,4) | `0x74` | **8** | ANY_DIR |
| (8,1) | `0x81` | **7** | ANY_DIR |
| (8,3) | `0x83` | **8** | ANY_DIR |
| (8,7) | `0x87` | **4** | ANY_DIR |
| (8,8) | `0x88` | **14** | ANY_DIR |
| (8,9) | `0x89` | **15** | ANY_DIR |
| (9,5) | `0x95` | **5** | ANY_DIR |
| (9,8) | `0x98` | **13** | ANY_DIR |
| (10,2) | `0xA2` | **6** | ANY_DIR |
| (10,12) | `0xAC` | **16** | ANY_DIR |
| (10,13) | `0xAD` | **3** | DIR_S? |
| (11,0) | `0xB0` | **5** | ANY_DIR |
| (11,5) | `0xB5` | **4** | ANY_DIR |
| (11,9) | `0xB9` | **11** | ANY_DIR |
| (11,14) | `0xBE` | **10** | ANY_DIR |
| (12,2) | `0xC2` | **4** | ANY_DIR |
| (13,1) | `0xD1` | **13** | ANY_DIR |
| (13,2) | `0xD2` | **14** | ANY_DIR |
| (13,3) | `0xD3` | **15** | ANY_DIR |
| (14,7) | `0xE7` | **12** | 0x30 |
| (14,9) | `0xE9` | **14** | ANY_DIR |
| (14,10) | `0xEA` | **13** | ANY_DIR |
| (14,11) | `0xEB` | **15** | ANY_DIR |
| (15,14) | `0xFE` | **17** | ANY_DIR |

## Events

**Event 01** — triggers: (7,3)/DIR_N?

```hex
06 02 02 01 09 10 01 0f 0c 1e 08
```

```
00: show_text_popup_style_b(str[2] "Keep / Out!")
01: show_text_block(str[1] "A sign above a dark hole reads, / "Dawn's Cavern, Keep Out!" Do you / da")
02: cond = prompt_yes_no()
03: if cond: skip_tokens(1)
    # skip -> map_transition(0x1E, 0x08)
04: end_script()
05: map_transition(0x1E, 0x08)
```

**Event 02** — triggers: (1,13)/0x60

```hex
2b 0c 0b 06 00 02 03 0a 10 01 0f 16 00 d5 10 05 02 08 0a 10 01 0f 12 69 69 69 69 69 69 00 00 00 00 00 00 0c 37 f7
```

```
00: skip_tokens(12)
    # skip -> map_transition(0x37, 0xF7)
01: service_sign(idx=0x06 -> sign 47 [47.anm], pos=0x00)
02: show_text_block(str[3] "The marble walls of Castle Hillstone / gleam before you. Enter (y/n)?")
03: cond = prompt_yes_no(mode=1)
04: if cond: skip_tokens(1)
    # skip -> cond = check_monster_present(0x00, 0xD5)
05: end_script()
06: cond = check_monster_present(0x00, 0xD5)
07: if cond: skip_tokens(5)
    # skip -> map_transition(0x37, 0xF7)
08: show_text_block(str[8] "Castle guards exclaim, "No key, no / admittance." Force your way in (y/n")
09: cond = prompt_yes_no(mode=1)
10: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
11: end_script()
12: encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
13: map_transition(0x37, 0xF7)
```

**Event 03** — triggers: (10,13)/DIR_S?

```hex
06 04
```

```
00: show_text_popup_style_b(str[4] "<Sandsobar / Hillstone$")
```

**Event 04** — triggers: (5,11)/ANY_DIR, (8,7)/ANY_DIR, (11,5)/ANY_DIR, (12,2)/ANY_DIR

```hex
2b 01 12 13 13 13 13 13 13 13 13 13 13 13 0f 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=13 13 13 13 13 13 13 13 13 13, flags=13 0F)
02: clear_current_tile_event_flag()
```

**Event 05** — triggers: (2,12)/ANY_DIR, (5,8)/ANY_DIR, (9,5)/ANY_DIR, (11,0)/ANY_DIR

```hex
2b 01 12 13 13 13 13 35 35 35 35 35 35 35 02 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=13 13 13 13 35 35 35 35 35 35, flags=35 02)
02: clear_current_tile_event_flag()
```

**Event 06** — triggers: (3,5)/ANY_DIR, (3,7)/ANY_DIR, (10,2)/ANY_DIR

```hex
2b 01 12 6c 6c 6c 6c 6c 6c 6c 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6C 6C 6C 6C 6C 6C 6C 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (0,5)/ANY_DIR, (2,1)/ANY_DIR, (5,1)/ANY_DIR, (8,1)/ANY_DIR

```hex
2b 01 12 66 66 66 66 66 66 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=66 66 66 66 66 66 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 08** — triggers: (0,2)/ANY_DIR, (1,3)/ANY_DIR, (1,8)/ANY_DIR, (2,9)/ANY_DIR, (4,3)/ANY_DIR, (4,4)/ANY_DIR, (7,2)/ANY_DIR, (7,4)/ANY_DIR, (8,3)/ANY_DIR

```hex
2b 01 12 80 80 80 80 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=80 80 80 80 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (6,0)/ANY_DIR

```hex
2b 01 12 b1 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=B1 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (11,14)/ANY_DIR

```hex
2b 05 02 05 09 10 01 0f 12 7d 7d 7d 7d 7d 7d 00 00 00 00 00 00 2a 00 00 00 00 00 e3 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(5)
    # skip -> set_treasure(gold/exp=0, gems=0, items=[227, 0, 0])
01: show_text_block(str[5] "The Guardian defends the Sword of / Honor. Challenge for it (y/n)?")
02: cond = prompt_yes_no()
03: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=7D 7D 7D 7D 7D 7D 00 00 00 00, flags=00 00)
04: end_script()
05: encounter_setup(monsters=7D 7D 7D 7D 7D 7D 00 00 00 00, flags=00 00)
06: set_treasure(gold/exp=0, gems=0, items=[227, 0, 0])
07: clear_current_tile_event_flag()
```

**Event 11** — triggers: (11,9)/ANY_DIR

```hex
2b 04 01 06 09 11 02 12 63 63 63 63 63 63 63 63 63 63 63 22 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_basic(str[6] "Farm of Fear. Enter (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=63 63 63 63 63 63 63 63 63 63, flags=63 22)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 12** — triggers: (14,7)/0x30

```hex
2b 04 02 07 09 11 02 12 64 64 64 64 64 64 64 62 64 64 64 5b 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[7] "Outcast lepers sit around large / tables playing various games. A sign /")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=64 64 64 64 64 64 64 62 64 64, flags=64 5B)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 13** — triggers: (2,13)/ANY_DIR, (9,8)/ANY_DIR, (13,1)/ANY_DIR, (14,10)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 e1 e0 c1 e1 c2 c1 c1 c1 c2 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=E1 E0 C1 E1 C2 C1 C1 C1 C2 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 14** — triggers: (3,13)/ANY_DIR, (8,8)/ANY_DIR, (13,2)/ANY_DIR, (14,9)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 c2 c1 e1 c1 c2 ce ce c2 c2 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=C2 C1 E1 C1 C2 CE CE C2 C2 00)
04: clear_current_tile_event_flag()
```

**Event 15** — triggers: (4,13)/ANY_DIR, (8,9)/ANY_DIR, (13,3)/ANY_DIR, (14,11)/ANY_DIR

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

**Event 16** — triggers: (10,12)/ANY_DIR

```hex
06 09 02 0a 29
```

```
00: show_text_popup_style_b(str[9] "Yellow / Message 4")
01: show_text_block(str[10] "l  lwi y cprfeofo an i ire / he frotscew  tghk d iscs h")
02: set_abort_and_exit()
```

**Event 17** — triggers: (15,14)/ANY_DIR

```hex
06 0b 02 0c 29
```

```
00: show_text_popup_style_b(str[11] "Yellow / Message 6")
01: show_text_block(str[12] "er yangata ddou to wiserby /  t arir ugid. thgocu tat t")
02: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` A sign above a dark hole reads, / "Dawn's Cavern, Keep Out!" Do you / dare enter (y/n)?
- `[02]` Keep / Out!
- `[03]` The marble walls of Castle Hillstone / gleam before you. Enter (y/n)?
- `[04]` <Sandsobar / Hillstone$
- `[05]` The Guardian defends the Sword of / Honor. Challenge for it (y/n)?
- `[06]` Farm of Fear. Enter (y/n)?
- `[07]` Outcast lepers sit around large / tables playing various games. A sign / reads "LeperCON". Disrupt (y/n)?
- `[08]` Castle guards exclaim, "No key, no / admittance." Force your way in (y/n)?
- `[09]` Yellow / Message 4
- `[10]` l  lwi y cprfeofo an i ire / he frotscew  tghk d iscs h
- `[11]` Yellow / Message 6
- `[12]` er yangata ddou to wiserby /  t arir ugid. thgocu tat t
- `[13]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
