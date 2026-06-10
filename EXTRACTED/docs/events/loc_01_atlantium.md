# Location 01 — Atlantium

- **event.dat** offset `0x000758`, length **1469** bytes
- **Map:** map screen **1**; Atlantium
- **Record kind:** `standard`
- **Triggers:** 57; **script segments:** 54; **strings:** 29

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **36** | ANY_DIR |
| (0,3) | `0x03` | **25** | DIR_W? |
| (0,4) | `0x04` | **5** | DIR_W? |
| (0,11) | `0x0B` | **6** | DIR_E? |
| (0,12) | `0x0C` | **26** | DIR_E? |
| (0,15) | `0x0F` | **40** | ANY_DIR |
| (1,13) | `0x1D` | **39** | ANY_DIR |
| (2,0) | `0x20` | **34** | ANY_DIR |
| (2,6) | `0x26` | **27** | DIR_S? |
| (2,9) | `0x29` | **29** | DIR_S? |
| (3,2) | `0x32` | **35** | ANY_DIR |
| (3,6) | `0x36` | **10** | DIR_S? |
| (3,8) | `0x38` | **11** | DIR_N? |
| (3,9) | `0x39` | **12** | DIR_S? |
| (3,15) | `0x3F` | **38** | ANY_DIR |
| (4,5) | `0x45` | **19** | DIR_W? |
| (4,6) | `0x46` | **7** | DIR_W? |
| (4,8) | `0x48` | **28** | DIR_N? |
| (4,9) | `0x49` | **4** | DIR_E? |
| (4,10) | `0x4A` | **20** | DIR_E? |
| (4,13) | `0x4D` | **37** | ANY_DIR |
| (5,0) | `0x50` | **33** | ANY_DIR |
| (6,2) | `0x62` | **41** | DIR_S? |
| (6,13) | `0x6D` | **42** | DIR_S? |
| (7,4) | `0x74` | **18** | DIR_W? |
| (7,5) | `0x75` | **13** | DIR_W? |
| (7,7) | `0x77` | **9** | DIR_N? |
| (7,8) | `0x78` | **9** | DIR_N? |
| (7,11) | `0x7B` | **30** | DIR_E? |
| (8,0) | `0x80` | **46** | DIR_W? |
| (8,15) | `0x8F` | **50** | DIR_E? |
| (9,0) | `0x90` | **45** | DIR_W? |
| (9,5) | `0x95` | **9** | DIR_E? |
| (9,7) | `0x97` | **52** | ANY_DIR |
| (9,10) | `0x9A` | **9** | DIR_W? |
| (9,15) | `0x9F` | **49** | DIR_E? |
| (10,0) | `0xA0` | **44** | DIR_W? |
| (10,3) | `0xA3` | **24** | DIR_W? |
| (10,4) | `0xA4` | **8** | DIR_W? |
| (10,11) | `0xAB` | **3** | DIR_E? |
| (10,12) | `0xAC` | **21** | DIR_E? |
| (10,15) | `0xAF` | **48** | DIR_E? |
| (11,0) | `0xB0` | **43** | DIR_W? |
| (11,7) | `0xB7` | **9** | DIR_S? |
| (11,8) | `0xB8` | **9** | DIR_S? |
| (11,15) | `0xBF` | **47** | DIR_E? |
| (13,0) | `0xD0` | **32** | ANY_DIR |
| (13,1) | `0xD1` | **15** | DIR_W? |
| (13,3) | `0xD3` | **14** | DIR_W? |
| (13,6) | `0xD6` | **17** | DIR_S? |
| (13,9) | `0xD9` | **16** | DIR_S? |
| (13,15) | `0xDF` | **31** | ANY_DIR |
| (14,0) | `0xE0` | **51** | DIR_N? |
| (14,6) | `0xE6` | **2** | DIR_S? |
| (14,9) | `0xE9` | **1** | DIR_S? |
| (15,0) | `0xF0` | **23** | DIR_W? |
| (15,15) | `0xFF` | **22** | DIR_E? |

## Events

**Event 01** — triggers: (14,9)/DIR_S?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Carriage Inn")
```

**Event 02** — triggers: (14,6)/DIR_S?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Drewnhald Ironworks")
```

**Event 03** — triggers: (10,11)/DIR_E?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Boar's Tongue Tavern")
```

**Event 04** — triggers: (4,9)/DIR_E?

```hex
04 04
```

```
00: show_text_above_door(str[4] "Island Training")
```

**Event 05** — triggers: (0,4)/DIR_W?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Beautify Atlantium")
```

**Event 06** — triggers: (0,11)/DIR_E?

```hex
04 06
```

```
00: show_text_above_door(str[6] "The Mystic Portal")
```

**Event 07** — triggers: (4,6)/DIR_W?

```hex
04 07
```

```
00: show_text_above_door(str[7] "Cabalist Mage Guild")
```

**Event 08** — triggers: (10,4)/DIR_W?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Classic Key Shoppe")
```

**Event 09** — triggers: (7,7)/DIR_N?, (7,8)/DIR_N?, (9,5)/DIR_E?, (9,10)/DIR_W?, (11,7)/DIR_S?, (11,8)/DIR_S?

```hex
04 09
```

```
00: show_text_above_door(str[9] "The Colosseum")
```

**Event 10** — triggers: (3,6)/DIR_S?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "The Olympic Trial")
```

**Event 11** — triggers: (3,8)/DIR_N?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Odysseus' Tongue")
```

**Event 12** — triggers: (3,9)/DIR_S?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Hippomenes + Atlanta")
```

**Event 13** — triggers: (7,5)/DIR_W?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Eleusinian Temple")
```

**Event 14** — triggers: (13,3)/DIR_W?

```hex
04 0e
```

```
00: show_text_above_door(str[14] "Danger! City Jail!")
```

**Event 15** — triggers: (13,1)/DIR_W?

```hex
04 0f
```

```
00: show_text_above_door(str[15] "Death Row!")
```

**Event 16** — triggers: (13,9)/DIR_S?

```hex
0b 03 00 0e 01
```

```
00: set_service_context(str[3] "Boar's Tongue Tavern", mode=0x00)
01: exec_selector(0x01)  # open_tavern_food
```

**Event 17** — triggers: (13,6)/DIR_S?

```hex
0b 02 00 0e 06
```

```
00: set_service_context(str[2] "Drewnhald Ironworks", mode=0x00)
01: exec_selector(0x06)  # open_blacksmith_shop
```

**Event 18** — triggers: (7,4)/DIR_W?

```hex
0b 05 00 0e 04
```

```
00: set_service_context(str[5] "Beautify Atlantium", mode=0x00)
01: exec_selector(0x04)  # open_training
```

**Event 19** — triggers: (4,5)/DIR_W?

```hex
0b 14 00 0e 05
```

```
00: set_service_context(str[20] "Not enough gold!", mode=0x00)
01: exec_selector(0x05)  # open_mages_guild
```

**Event 20** — triggers: (4,10)/DIR_E?

```hex
0b 06 00 0e 02
```

```
00: set_service_context(str[6] "The Mystic Portal", mode=0x00)
01: exec_selector(0x02)  # open_inn_lodging
```

**Event 21** — triggers: (10,12)/DIR_E?

```hex
0b 04 00 0e 03
```

```
00: set_service_context(str[4] "Island Training", mode=0x00)
01: exec_selector(0x03)  # open_temple
```

**Event 22** — triggers: (15,15)/DIR_E?

```hex
02 10 09 10 01 0f 0c 0f ad
```

```
00: show_text_block(str[16] "An empty pier stretches out of town. / Leave (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x0F, 0xAD)
03: end_script()
04: map_transition(0x0F, 0xAD)
```

**Event 23** — triggers: (15,0)/DIR_W?

```hex
01 11 09 10 01 0f 0c 12 88
```

```
00: show_text_basic(str[17] "Stairs to cavern. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x12, 0x88)
03: end_script()
04: map_transition(0x12, 0x88)
```

**Event 24** — triggers: (10,3)/DIR_W?

```hex
0e 1c
```

```
00: exec_selector(0x1C)
```

**Event 25** — triggers: (0,3)/DIR_W?

```hex
02 13 09 10 01 0f 24 32 00 11 01 0c 03 38 01 14 29
```

```
00: show_text_block(str[19] "A girl in a pure white toga offers / travel to Vulcania for 50 gold. / A")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(50)
03: end_script()
04: cond = check_gold_at_least(50)
05: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[20] "Not enough gold!")
06: map_transition(0x03, 0x38)
07: show_text_basic(str[20] "Not enough gold!")
08: set_abort_and_exit()
```

**Event 26** — triggers: (0,12)/DIR_E?

```hex
02 15 09 10 01 0f 24 19 00 11 01 0c 00 50 01 14 29
```

```
00: show_text_block(str[21] "The Keep Atlantium Beautiful Committee / offers to send you to Middlegat")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(25)
03: end_script()
04: cond = check_gold_at_least(25)
05: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[20] "Not enough gold!")
06: map_transition(0x00, 0x50)
07: show_text_basic(str[20] "Not enough gold!")
08: set_abort_and_exit()
```

**Event 27** — triggers: (2,6)/DIR_S?

```hex
02 16 09 11 08 02 1b 26 15 09 6d 0f 10 05 24 f4 01 11 0c 18 09 6d f0 02 1f 09 13 01 05 00 00 0f 15 09 6d f0 10 05 24 f4 01 11 05 18 09 6d 0f 20 1f 09 13 01 05 00 00 0f 01 17 29 01 14 29
```

```
00: show_text_block(str[22] "Hurl the spear, let fly the discus, / all in the Olympic manner. Spend 5")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(8)
    # skip -> end_script()
03: show_text_block(str[27] "Who will learn this skill (1-8)?")
04: selected = select_party_member()
05: apply_party(count=0x09, op=0x6D, val=0x0F)
06: if cond: skip_tokens(5)
    # skip -> apply_party(count=0x09, op=0x6D, val=0xF0)
07: cond = check_gold_at_least(500)
08: if not cond: skip_tokens(12)
    # skip -> show_text_basic(str[20] "Not enough gold!")
09: apply_party_masked(count=0x09, set=0x6D, and=0xF0, or=0x02)
10: party_effect(sel=0x09, 13 01 05 00 00)
11: end_script()
12: apply_party(count=0x09, op=0x6D, val=0xF0)
13: if cond: skip_tokens(5)
    # skip -> show_text_basic(str[23] "You already have two Skills!")
14: cond = check_gold_at_least(500)
15: if not cond: skip_tokens(5)
    # skip -> show_text_basic(str[20] "Not enough gold!")
16: apply_party_masked(count=0x09, set=0x6D, and=0x0F, or=0x20)
17: party_effect(sel=0x09, 13 01 05 00 00)
18: end_script()
19: show_text_basic(str[23] "You already have two Skills!")
20: set_abort_and_exit()
21: show_text_basic(str[20] "Not enough gold!")
22: set_abort_and_exit()
```

**Event 28** — triggers: (4,8)/DIR_N?

```hex
02 18 09 11 08 02 1b 26 15 09 6d 0f 10 05 24 f4 01 11 0c 18 09 6d f0 09 1f 09 11 01 05 00 00 0f 15 09 6d f0 10 05 24 f4 01 11 05 18 09 6d 0f 90 1f 09 11 01 05 00 00 0f 01 17 29 01 14 29
```

```
00: show_text_block(str[24] "Let crafty Odysseus teach you his many / linguistic skills for 500 gold ")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(8)
    # skip -> end_script()
03: show_text_block(str[27] "Who will learn this skill (1-8)?")
04: selected = select_party_member()
05: apply_party(count=0x09, op=0x6D, val=0x0F)
06: if cond: skip_tokens(5)
    # skip -> apply_party(count=0x09, op=0x6D, val=0xF0)
07: cond = check_gold_at_least(500)
08: if not cond: skip_tokens(12)
    # skip -> show_text_basic(str[20] "Not enough gold!")
09: apply_party_masked(count=0x09, set=0x6D, and=0xF0, or=0x09)
10: party_effect(sel=0x09, 11 01 05 00 00)
11: end_script()
12: apply_party(count=0x09, op=0x6D, val=0xF0)
13: if cond: skip_tokens(5)
    # skip -> show_text_basic(str[23] "You already have two Skills!")
14: cond = check_gold_at_least(500)
15: if not cond: skip_tokens(5)
    # skip -> show_text_basic(str[20] "Not enough gold!")
16: apply_party_masked(count=0x09, set=0x6D, and=0x0F, or=0x90)
17: party_effect(sel=0x09, 11 01 05 00 00)
18: end_script()
19: show_text_basic(str[23] "You already have two Skills!")
20: set_abort_and_exit()
21: show_text_basic(str[20] "Not enough gold!")
22: set_abort_and_exit()
```

**Event 29** — triggers: (2,9)/DIR_S?

```hex
02 19 09 11 03 02 1b 26 0e 52 0f
```

```
00: show_text_block(str[25] "Become the Hero/Heroine of your / dreams for 1000 gold (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(3)
    # skip -> end_script()
03: show_text_block(str[27] "Who will learn this skill (1-8)?")
04: selected = select_party_member()
05: exec_selector(0x52)
06: end_script()
```

**Event 30** — triggers: (7,11)/DIR_E?

```hex
0b 14 00 0e 12
```

```
00: set_service_context(str[20] "Not enough gold!", mode=0x00)
01: exec_selector(0x12)
```

**Event 31** — triggers: (13,15)/ANY_DIR

```hex
2b 01 0e 26 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x26)
02: clear_current_tile_event_flag()
```

**Event 32** — triggers: (13,0)/ANY_DIR

```hex
2b 01 0e 13 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x13)
02: clear_current_tile_event_flag()
```

**Event 33** — triggers: (5,0)/ANY_DIR

```hex
2b 01 0e 14 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x14)
02: clear_current_tile_event_flag()
```

**Event 34** — triggers: (2,0)/ANY_DIR

```hex
2b 01 0e 15 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x15)
02: clear_current_tile_event_flag()
```

**Event 35** — triggers: (3,2)/ANY_DIR

```hex
2b 01 0e 16 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x16)
02: clear_current_tile_event_flag()
```

**Event 36** — triggers: (0,2)/ANY_DIR

```hex
2b 01 0e 17 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x17)
02: clear_current_tile_event_flag()
```

**Event 37** — triggers: (4,13)/ANY_DIR

```hex
2b 01 0e 18 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x18)
02: clear_current_tile_event_flag()
```

**Event 38** — triggers: (3,15)/ANY_DIR

```hex
2b 01 0e 19 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x19)
02: clear_current_tile_event_flag()
```

**Event 39** — triggers: (1,13)/ANY_DIR

```hex
2b 01 0e 1a 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x1A)
02: clear_current_tile_event_flag()
```

**Event 40** — triggers: (0,15)/ANY_DIR

```hex
2b 01 0e 1b 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x1B)
02: clear_current_tile_event_flag()
```

**Event 41** — triggers: (6,2)/DIR_S?

```hex
04 1a
```

```
00: show_text_above_door(str[26] "Knights/Warriors")
```

**Event 42** — triggers: (6,13)/DIR_S?

```hex
04 12
```

```
00: show_text_above_door(str[18] "Sorcerers/Clerics")
```

**Event 43** — triggers: (11,0)/DIR_W?

```hex
0b 07 00 0e 24
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x24)
```

**Event 44** — triggers: (10,0)/DIR_W?

```hex
0b 07 00 0e 1d
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x1D)
```

**Event 45** — triggers: (9,0)/DIR_W?

```hex
0b 07 00 0e 1e
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x1E)
```

**Event 46** — triggers: (8,0)/DIR_W?

```hex
0b 07 00 0e 1f
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x1F)
```

**Event 47** — triggers: (11,15)/DIR_E?

```hex
0b 07 00 0e 20
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x20)
```

**Event 48** — triggers: (10,15)/DIR_E?

```hex
0b 07 00 0e 21
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x21)
```

**Event 49** — triggers: (9,15)/DIR_E?

```hex
0b 07 00 0e 22
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x22)
```

**Event 50** — triggers: (8,15)/DIR_E?

```hex
0b 07 00 0e 23
```

```
00: set_service_context(str[7] "Cabalist Mage Guild", mode=0x00)
01: exec_selector(0x23)
```

**Event 51** — triggers: (14,0)/DIR_N?

```hex
17 05 00 10 01 0e 25 14
```

```
00: cond = load_var8(group=0x05, index=0x00)
01: if cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
02: exec_selector(0x25)
03: clear_current_tile_event_flag()
```

**Event 52** — triggers: (9,7)/ANY_DIR

```hex
0e 08
```

```
00: exec_selector(0x08)  # open_special_shop
```

## String table

- `[00]` <EMPTY>
- `[01]` Carriage Inn
- `[02]` Drewnhald Ironworks
- `[03]` Boar's Tongue Tavern
- `[04]` Island Training
- `[05]` Beautify Atlantium
- `[06]` The Mystic Portal
- `[07]` Cabalist Mage Guild
- `[08]` Classic Key Shoppe
- `[09]` The Colosseum
- `[10]` The Olympic Trial
- `[11]` Odysseus' Tongue
- `[12]` Hippomenes + Atlanta
- `[13]` Eleusinian Temple
- `[14]` Danger! City Jail!
- `[15]` Death Row!
- `[16]` An empty pier stretches out of town. / Leave (y/n)?
- `[17]` Stairs to cavern. Descend (y/n)?
- `[18]` Sorcerers/Clerics
- `[19]` A girl in a pure white toga offers / travel to Vulcania for 50 gold. / Accept (y/n)?
- `[20]` Not enough gold!
- `[21]` The Keep Atlantium Beautiful Committee / offers to send you to Middlegate for / 25 gold. Embark (y/n)?
- `[22]` Hurl the spear, let fly the discus, / all in the Olympic manner. Spend 500 / gold to be an Athlete (y/n)?
- `[23]` You already have two Skills!
- `[24]` Let crafty Odysseus teach you his many / linguistic skills for 500 gold (y/n)?
- `[25]` Become the Hero/Heroine of your / dreams for 1000 gold (y/n)?
- `[26]` Knights/Warriors
- `[27]` Who will learn this skill (1-8)?
- `[28]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
