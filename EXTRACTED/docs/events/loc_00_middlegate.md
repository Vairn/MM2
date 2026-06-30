# Location 00 — Middlegate

- **event.dat** offset `0x0001AA`, length **1454** bytes
- **Map:** map screen **0**; Middlegate
- **Record kind:** `standard`
- **Triggers:** 42; **script segments:** 44; **strings:** 36

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,8) | `0x08` | **29** | DIR_W? |
| (1,8) | `0x18` | **41** | ANY_DIR |
| (2,1) | `0x21` | **31** | DIR_N? |
| (2,10) | `0x2A` | **30** | DIR_W? |
| (2,13) | `0x2D` | **9** | ANY_DIR |
| (3,7) | `0x37` | **25** | DIR_S? |
| (3,10) | `0x3A` | **37** | DIR_N? |
| (4,4) | `0x44` | **22** | DIR_W? |
| (4,5) | `0x45` | **2** | DIR_W? |
| (4,7) | `0x47` | **18** | DIR_N? |
| (4,8) | `0x48` | **42** | DIR_S? |
| (4,10) | `0x4A` | **38** | ANY_DIR |
| (5,0) | `0x50` | **21** | DIR_W? |
| (5,1) | `0x51` | **7** | DIR_W? |
| (5,7) | `0x57` | **1** | DIR_S? |
| (6,4) | `0x64` | **24** | DIR_W? |
| (6,5) | `0x65` | **3** | DIR_W? |
| (6,7) | `0x67` | **4** | DIR_N? |
| (6,13) | `0x6D` | **6** | DIR_S? |
| (6,14) | `0x6E` | **10** | DIR_S? |
| (7,7) | `0x77` | **23** | DIR_N? |
| (7,9) | `0x79` | **5** | DIR_E? |
| (7,10) | `0x7A` | **26** | DIR_E? |
| (8,1) | `0x81` | **16** | DIR_W? |
| (8,2) | `0x82` | **11** | DIR_W? |
| (8,13) | `0x8D` | **28** | DIR_E? |
| (8,14) | `0x8E` | **36** | DIR_E? |
| (9,1) | `0x91` | **35** | DIR_W? |
| (9,2) | `0x92` | **14** | DIR_W? |
| (10,12) | `0xAC` | **32** | DIR_S? |
| (11,12) | `0xBC` | **15** | DIR_S? |
| (12,1) | `0xC1` | **19** | DIR_W? |
| (12,2) | `0xC2` | **13** | DIR_E? |
| (12,3) | `0xC3` | **34** | DIR_E? |
| (12,13) | `0xCD` | **39** | ANY_DIR |
| (13,7) | `0xD7` | **8** | DIR_N? |
| (14,7) | `0xE7` | **27** | DIR_N? |
| (14,12) | `0xEC` | **40** | ANY_DIR |
| (15,0) | `0xF0` | **33** | DIR_W? |
| (15,1) | `0xF1` | **12** | DIR_W? |
| (15,5) | `0xF5` | **20** | DIR_N? |
| (15,15) | `0xFF` | **17** | DIR_N?+DIR_E? |

## Events

**Event 01** — triggers: (5,7)/DIR_S?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Middlegate Inn")
```

**Event 02** — triggers: (4,5)/DIR_W?

```hex
04 02
```

```
00: show_text_above_door(str[2] "S.J. Blacksmith")
```

**Event 03** — triggers: (6,5)/DIR_W?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Slaughtered Lamb")
```

**Event 04** — triggers: (6,7)/DIR_N?

```hex
04 04
```

```
00: show_text_above_door(str[4] "Gateway Temple")
```

**Event 05** — triggers: (7,9)/DIR_E?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Turkov's Training")
```

**Event 06** — triggers: (6,13)/DIR_S?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Arena Entrance Only!")
```

**Event 07** — triggers: (5,1)/DIR_W?

```hex
04 07
```

```
00: show_text_above_door(str[7] "The Poorman's Portal")
```

**Event 08** — triggers: (13,7)/DIR_N?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Sleepy's Mage Guild")
```

**Event 09** — triggers: (2,13)/ANY_DIR

```hex
0e 08
```

```
00: exec_selector(0x08)  # open_arena_shop
```

**Event 10** — triggers: (6,14)/DIR_S?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Exit Only")
```

**Event 11** — triggers: (8,2)/DIR_W?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Lock and Key LTD")
```

**Event 12** — triggers: (15,1)/DIR_W?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Otto Mapper, Esquire")
```

**Event 13** — triggers: (12,2)/DIR_E?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Edmund's Expeditions")
```

**Event 14** — triggers: (9,2)/DIR_W?

```hex
04 0e
```

```
00: show_text_above_door(str[14] "Track and Trail")
```

**Event 15** — triggers: (11,12)/DIR_S?

```hex
04 0f
```

```
00: show_text_above_door(str[15] "Brain Detoxification")
```

**Event 16** — triggers: (8,1)/DIR_W?

```hex
0e 10
```

```
00: exec_selector(0x10)
```

**Event 17** — triggers: (15,15)/DIR_N?+DIR_E?

```hex
0e 0e
```

```
00: exec_selector(0x0E)
```

**Event 18** — triggers: (4,7)/DIR_N?

```hex
15 00 74 01 10 02 0b 0b 00 0e 09 14
```

```
00: apply_party(count=0x00, op=0x74, val=0x01)
01: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: service_sign(idx=0x0B -> sign 51 [51.anm], pos=0x00)
03: exec_selector(0x09)
04: clear_current_tile_event_flag()
```

**Event 19** — triggers: (12,1)/DIR_W?

```hex
0b 14 00 0e 0d
```

```
00: service_sign(idx=0x14 -> sign 37 [37.anm], pos=0x00)
01: exec_selector(0x0D)  # enroll_mages_guild
```

**Event 20** — triggers: (15,5)/DIR_N?

```hex
01 18 09 10 01 0f 0c 0b 37
```

```
00: show_text_basic(str[24] "You are at the city gates. Exit (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x0B, 0x37)  # overland_area_c3
03: end_script()
04: map_transition(0x0B, 0x37)  # overland_area_c3
```

**Event 21** — triggers: (5,0)/DIR_W?

```hex
0e 11
```

```
00: exec_selector(0x11)
```

**Event 22** — triggers: (4,4)/DIR_W?

```hex
0b 02 00 0e 06
```

```
00: service_sign(idx=0x02 -> sign 62 [62.anm], pos=0x00)
01: exec_selector(0x06)  # open_blacksmith_shop
```

**Event 23** — triggers: (7,7)/DIR_N?

```hex
0b 05 00 0e 04
```

```
00: service_sign(idx=0x05 -> sign 67 [67.anm], pos=0x00)
01: exec_selector(0x04)  # open_temple
```

**Event 24** — triggers: (6,4)/DIR_W?

```hex
0b 04 00 0e 03
```

```
00: service_sign(idx=0x04 -> sign 66 [66.anm], pos=0x00)
01: exec_selector(0x03)  # open_tavern_food
```

**Event 25** — triggers: (3,7)/DIR_S?

```hex
0b 03 00 0e 01
```

```
00: service_sign(idx=0x03 -> sign 63 [63.anm], pos=0x00)
01: exec_selector(0x01)  # open_inn_lodging
```

**Event 26** — triggers: (7,10)/DIR_E?

```hex
0b 06 00 0e 02
```

```
00: service_sign(idx=0x06 -> sign 68 [68.anm], pos=0x00)
01: exec_selector(0x02)  # open_training
```

**Event 27** — triggers: (14,7)/DIR_N?

```hex
0b 14 00 0e 05
```

```
00: service_sign(idx=0x14 -> sign 37 [37.anm], pos=0x00)
01: exec_selector(0x05)  # open_mages_guild
```

**Event 28** — triggers: (8,13)/DIR_E?

```hex
04 1a
```

```
00: show_text_above_door(str[26] "Travel Moore")
```

**Event 29** — triggers: (0,8)/DIR_W?

```hex
01 15 09 11 01 0c 11 8f 0f
```

```
00: show_text_basic(str[21] "Stairs to Cavern. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x11, 0x8F)  # cavern_of_middlegate
04: end_script()
```

**Event 30** — triggers: (2,10)/DIR_W?

```hex
0b 14 00 0e 0a
```

```
00: service_sign(idx=0x14 -> sign 37 [37.anm], pos=0x00)
01: exec_selector(0x0A)  # goblet_quest
```

**Event 31** — triggers: (2,1)/DIR_N?

```hex
0e 0b
```

```
00: exec_selector(0x0B)
```

**Event 32** — triggers: (10,12)/DIR_S?

```hex
0e 07
```

```
00: exec_selector(0x07)  # open_general_store
```

**Event 33** — triggers: (15,0)/DIR_W?

```hex
02 16 09 10 01 0f 02 1b 26 15 09 6d 0f 10 04 24 0a 00 11 0a 18 09 6d f0 03 0f 15 09 6d f0 10 04 24 0a 00 11 04 18 09 6d 0f 30 0f 01 1c 29 01 11 29
```

```
00: show_text_block(str[22] "Otto adds the finishing touches to an / intricate map. Pay 10 gold to le")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_block(str[27] "Who will learn this skill (1-8)")
03: end_script()
04: show_text_block(str[27] "Who will learn this skill (1-8)")
05: selected = select_party_member()
06: apply_party(count=0x09, op=0x6D, val=0x0F)
07: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x09, op=0x6D, val=0xF0)
08: cond = check_gold_at_least(10)
09: if not cond: skip_tokens(10)
    # skip -> show_text_basic(str[17] "Not enough gold!")
10: apply_party_masked(count=0x09, set=0x6D, and=0xF0, or=0x03)
11: end_script()
12: apply_party(count=0x09, op=0x6D, val=0xF0)
13: if cond: skip_tokens(4)
    # skip -> show_text_basic(str[28] "You already have two skills!")
14: cond = check_gold_at_least(10)
15: if not cond: skip_tokens(4)
    # skip -> show_text_basic(str[17] "Not enough gold!")
16: apply_party_masked(count=0x09, set=0x6D, and=0x0F, or=0x30)
17: end_script()
18: show_text_basic(str[28] "You already have two skills!")
19: set_abort_and_exit()
20: show_text_basic(str[17] "Not enough gold!")
21: set_abort_and_exit()
```

**Event 34** — triggers: (12,3)/DIR_E?

```hex
02 1d 09 10 01 0f 02 1b 26 15 09 6d 0f 10 04 24 d0 07 11 0a 18 09 6d f0 0b 0f 15 09 6d f0 10 04 24 d0 07 11 04 18 09 6d 0f b0 0f 01 1c 29 01 11 29
```

```
00: show_text_block(str[29] "Sir Edmund Hilary will teach you / Mountaineering for only 2000 gold. / ")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_block(str[27] "Who will learn this skill (1-8)")
03: end_script()
04: show_text_block(str[27] "Who will learn this skill (1-8)")
05: selected = select_party_member()
06: apply_party(count=0x09, op=0x6D, val=0x0F)
07: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x09, op=0x6D, val=0xF0)
08: cond = check_gold_at_least(2000)
09: if not cond: skip_tokens(10)
    # skip -> show_text_basic(str[17] "Not enough gold!")
10: apply_party_masked(count=0x09, set=0x6D, and=0xF0, or=0x0B)
11: end_script()
12: apply_party(count=0x09, op=0x6D, val=0xF0)
13: if cond: skip_tokens(4)
    # skip -> show_text_basic(str[28] "You already have two skills!")
14: cond = check_gold_at_least(2000)
15: if not cond: skip_tokens(4)
    # skip -> show_text_basic(str[17] "Not enough gold!")
16: apply_party_masked(count=0x09, set=0x6D, and=0x0F, or=0xB0)
17: end_script()
18: show_text_basic(str[28] "You already have two skills!")
19: set_abort_and_exit()
20: show_text_basic(str[17] "Not enough gold!")
21: set_abort_and_exit()
```

**Event 35** — triggers: (9,1)/DIR_W?

```hex
02 1e 09 10 01 0f 02 1b 26 15 09 6d 0f 10 04 24 d0 07 11 0a 18 09 6d f0 0d 0f 15 09 6d f0 10 04 24 d0 07 11 04 18 09 6d 0f d0 0f 01 1c 29 01 11 29
```

```
00: show_text_block(str[30] "A ranger will train you in the / difficulties of forest travel for / 200")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_block(str[27] "Who will learn this skill (1-8)")
03: end_script()
04: show_text_block(str[27] "Who will learn this skill (1-8)")
05: selected = select_party_member()
06: apply_party(count=0x09, op=0x6D, val=0x0F)
07: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x09, op=0x6D, val=0xF0)
08: cond = check_gold_at_least(2000)
09: if not cond: skip_tokens(10)
    # skip -> show_text_basic(str[17] "Not enough gold!")
10: apply_party_masked(count=0x09, set=0x6D, and=0xF0, or=0x0D)
11: end_script()
12: apply_party(count=0x09, op=0x6D, val=0xF0)
13: if cond: skip_tokens(4)
    # skip -> show_text_basic(str[28] "You already have two skills!")
14: cond = check_gold_at_least(2000)
15: if not cond: skip_tokens(4)
    # skip -> show_text_basic(str[17] "Not enough gold!")
16: apply_party_masked(count=0x09, set=0x6D, and=0x0F, or=0xD0)
17: end_script()
18: show_text_basic(str[28] "You already have two skills!")
19: set_abort_and_exit()
20: show_text_basic(str[17] "Not enough gold!")
21: set_abort_and_exit()
```

**Event 36** — triggers: (8,14)/DIR_E?

```hex
02 1f 09 10 01 0f 24 88 13 10 03 01 11 0d 09 29 02 21 18 00 7b fb 04 29
```

```
00: show_text_block(str[31] "Joan, the travel agent, will book you / on a trip to beautiful Murray's ")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(5000)
03: end_script()
04: cond = check_gold_at_least(5000)
05: if cond: skip_tokens(3)
    # skip -> show_text_block(str[33] "Catch the ferry in area C3 at x7, y9.")
06: show_text_basic(str[17] "Not enough gold!")
07: engine_call(0x09)
08: set_abort_and_exit()
09: show_text_block(str[33] "Catch the ferry in area C3 at x7, y9.")
10: apply_party_masked(count=0x00, set=0x7B, and=0xFB, or=0x04)
11: set_abort_and_exit()
```

**Event 37** — triggers: (3,10)/DIR_N?

```hex
04 22
```

```
00: show_text_above_door(str[34] "Skeleton Closet")
```

**Event 38** — triggers: (4,10)/ANY_DIR

```hex
2b 01 0e 26 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x26)
02: clear_current_tile_event_flag()
```

**Event 39** — triggers: (12,13)/ANY_DIR

```hex
2b 01 0e 27 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x27)
02: clear_current_tile_event_flag()
```

**Event 40** — triggers: (14,12)/ANY_DIR

```hex
2b 01 0e 28 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x28)
02: clear_current_tile_event_flag()
```

**Event 41** — triggers: (1,8)/ANY_DIR

```hex
2b 01 0e 29 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x29)
02: clear_current_tile_event_flag()
```

**Event 42** — triggers: (4,8)/DIR_S?

```hex
02 09 09 11 02 1a 2b 32 1a 2c 32 0f
```

```
00: show_text_block(str[9] "Drink from the fountain of / clairvoyance (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: store_var8(group=0x2B, value=0x32)
04: store_var8(group=0x2C, value=0x32)
05: end_script()
```

## String table

- `[00]` <EMPTY>
- `[01]` Middlegate Inn
- `[02]` S.J. Blacksmith
- `[03]` Slaughtered Lamb
- `[04]` Gateway Temple
- `[05]` Turkov's Training
- `[06]` Arena Entrance Only!
- `[07]` The Poorman's Portal
- `[08]` Sleepy's Mage Guild
- `[09]` Drink from the fountain of / clairvoyance (y/n)?
- `[10]` Exit Only
- `[11]` Lock and Key LTD
- `[12]` Otto Mapper, Esquire
- `[13]` Edmund's Expeditions
- `[14]` Track and Trail
- `[15]` Brain Detoxification
- `[16]` <SENTINEL_Z>
- `[17]` Not enough gold!
- `[18]` <SENTINEL_Z>
- `[19]` You find a fabulous castle key!
- `[20]` Fool, you have no farthing to flick!
- `[21]` Stairs to Cavern. Descend (y/n)?
- `[22]` Otto adds the finishing touches to an / intricate map. Pay 10 gold to learn / this skill (y/n)?
- `[23]` <SENTINEL_Z>
- `[24]` You are at the city gates. Exit (y/n)?
- `[25]` <SENTINEL_Z>
- `[26]` Travel Moore
- `[27]` Who will learn this skill (1-8)
- `[28]` You already have two skills!
- `[29]` Sir Edmund Hilary will teach you / Mountaineering for only 2000 gold. / Pay (y/n)?
- `[30]` A ranger will train you in the / difficulties of forest travel for / 2000 gold. Train (y/n)?
- `[31]` Joan, the travel agent, will book you / on a trip to beautiful Murray's Resort / Isle for 5,000 gold. Pay (y/n)?
- `[32]` <NEWLINE>
- `[33]` Catch the ferry in area C3 at x7, y9.
- `[34]` Skeleton Closet
- `[35]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
