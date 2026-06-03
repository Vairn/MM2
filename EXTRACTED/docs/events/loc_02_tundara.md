# Location 02 — Tundara

- **event.dat** offset `0x000D15`, length **1465** bytes
- **Map:** map screen **2**; Tundara
- **Record kind:** `standard`
- **Triggers:** 69; **script segments:** 55; **strings:** 27

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,5) | `0x05` | **33** | DIR_SPECIAL |
| (1,3) | `0x13` | **31** | ENTER |
| (1,5) | `0x15` | **31** | ENTER |
| (1,7) | `0x17` | **31** | ENTER |
| (1,9) | `0x19` | **31** | ENTER |
| (1,11) | `0x1B` | **31** | ENTER |
| (1,13) | `0x1D` | **31** | ENTER |
| (2,2) | `0x22` | **34** | ANY_DIR |
| (2,3) | `0x23` | **35** | ANY_DIR |
| (2,4) | `0x24` | **36** | ANY_DIR |
| (2,5) | `0x25` | **37** | ANY_DIR |
| (2,6) | `0x26` | **38** | ANY_DIR |
| (2,7) | `0x27` | **39** | ANY_DIR |
| (2,8) | `0x28` | **40** | ANY_DIR |
| (2,9) | `0x29` | **41** | ANY_DIR |
| (2,10) | `0x2A` | **42** | ANY_DIR |
| (2,11) | `0x2B` | **43** | ANY_DIR |
| (2,12) | `0x2C` | **44** | ANY_DIR |
| (2,13) | `0x2D` | **45** | ANY_DIR |
| (3,0) | `0x30` | **33** | DIR_N? |
| (3,2) | `0x32` | **31** | DIR_N? |
| (3,4) | `0x34` | **31** | DIR_N? |
| (3,6) | `0x36` | **31** | DIR_N? |
| (3,8) | `0x38` | **31** | DIR_N? |
| (3,10) | `0x3A` | **31** | DIR_N? |
| (3,12) | `0x3C` | **31** | DIR_N? |
| (3,15) | `0x3F` | **33** | ENTER |
| (4,13) | `0x4D` | **51** | DIR_N? |
| (5,3) | `0x53` | **29** | ANY_DIR |
| (6,7) | `0x67` | **21** | 0x60 |
| (6,11) | `0x6B` | **16** | DIR_N? |
| (7,2) | `0x72` | **32** | ALWAYS |
| (7,9) | `0x79` | **50** | DIR_N? |
| (7,11) | `0x7B` | **5** | DIR_N? |
| (7,14) | `0x7E` | **27** | ANY_DIR |
| (8,6) | `0x86` | **6** | ENTER |
| (8,8) | `0x88` | **52** | 0x30 |
| (8,15) | `0x8F` | **28** | ANY_DIR |
| (9,3) | `0x93` | **47** | ANY_DIR |
| (9,4) | `0x94` | **48** | ANY_DIR |
| (9,6) | `0x96` | **18** | ENTER |
| (9,7) | `0x97` | **14** | ALWAYS |
| (9,8) | `0x98` | **2** | ALWAYS |
| (9,13) | `0x9D` | **53** | 0xA0 |
| (9,15) | `0x9F` | **26** | 0xA0 |
| (10,0) | `0xA0` | **33** | DIR_N? |
| (10,3) | `0xA3` | **46** | ANY_DIR |
| (10,4) | `0xA4` | **49** | ANY_DIR |
| (10,6) | `0xA6` | **7** | ENTER |
| (10,10) | `0xAA` | **3** | DIR_SPECIAL |
| (10,11) | `0xAB` | **12** | DIR_SPECIAL |
| (10,15) | `0xAF` | **25** | ENTER |
| (11,6) | `0xB6` | **19** | ENTER |
| (11,7) | `0xB7` | **15** | ALWAYS |
| (11,8) | `0xB8` | **1** | ALWAYS |
| (11,15) | `0xBF` | **20** | DIR_SPECIAL |
| (12,5) | `0xC5` | **10** | ENTER |
| (12,11) | `0xCB` | **4** | ENTER |
| (12,15) | `0xCF` | **30** | DIR_N? |
| (13,5) | `0xD5` | **23** | ENTER |
| (13,8) | `0xD8` | **24** | DIR_N? |
| (13,11) | `0xDB` | **13** | ENTER |
| (13,14) | `0xDE` | **17** | DIR_N? |
| (14,2) | `0xE2` | **22** | ALWAYS |
| (14,3) | `0xE3` | **9** | ALWAYS |
| (14,8) | `0xE8` | **11** | DIR_N? |
| (14,14) | `0xEE` | **8** | DIR_N? |
| (15,4) | `0xF4` | **33** | ALWAYS |
| (15,12) | `0xFC` | **33** | ALWAYS |

## Events

**Event 01** — triggers: (11,8)/ALWAYS

```hex
04 01
```

```
00: show_text_above_door(str[1] "Tundaran Arms Inn")
```

**Event 02** — triggers: (9,8)/ALWAYS

```hex
04 02
```

```
00: show_text_above_door(str[2] "Lucky Dog Saloon")
```

**Event 03** — triggers: (10,10)/DIR_SPECIAL

```hex
04 03
```

```
00: show_text_above_door(str[3] "Thundrax Weaponry")
```

**Event 04** — triggers: (12,11)/ENTER

```hex
04 04
```

```
00: show_text_above_door(str[4] "White Dove Temple")
```

**Event 05** — triggers: (7,11)/DIR_N?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Enhancement Center")
```

**Event 06** — triggers: (8,6)/ENTER

```hex
04 06
```

```
00: show_text_above_door(str[6] "Polar Passage Portal")
```

**Event 07** — triggers: (10,6)/ENTER

```hex
04 07
```

```
00: show_text_above_door(str[7] "La Porte")
```

**Event 08** — triggers: (14,14)/DIR_N?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Mystical Mage Guild")
```

**Event 09** — triggers: (14,3)/ALWAYS

```hex
04 09
```

```
00: show_text_above_door(str[9] "Saracen's Denial")
```

**Event 10** — triggers: (12,5)/ENTER

```hex
04 0a
```

```
00: show_text_above_door(str[10] "International Market")
```

**Event 11** — triggers: (14,8)/DIR_N?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Columbus's Sextant")
```

**Event 12** — triggers: (10,11)/DIR_SPECIAL

```hex
0b 02 00 0e 06
```

```
00: set_service_context(str[2] "Lucky Dog Saloon", mode=0x00)
01: exec_selector(0x06)  # open_blacksmith_shop
```

**Event 13** — triggers: (13,11)/ENTER

```hex
0b 05 00 0e 04
```

```
00: set_service_context(str[5] "Enhancement Center", mode=0x00)
01: exec_selector(0x04)  # open_training
```

**Event 14** — triggers: (9,7)/ALWAYS

```hex
0b 04 00 0e 03
```

```
00: set_service_context(str[4] "White Dove Temple", mode=0x00)
01: exec_selector(0x03)  # open_temple
```

**Event 15** — triggers: (11,7)/ALWAYS

```hex
0b 03 00 0e 01
```

```
00: set_service_context(str[3] "Thundrax Weaponry", mode=0x00)
01: exec_selector(0x01)  # open_tavern_food
```

**Event 16** — triggers: (6,11)/DIR_N?

```hex
0b 06 00 0e 02
```

```
00: set_service_context(str[6] "Polar Passage Portal", mode=0x00)
01: exec_selector(0x02)  # open_inn_lodging
```

**Event 17** — triggers: (13,14)/DIR_N?

```hex
0b 14 00 0e 05
```

```
00: set_service_context(str[20] "Don't ya want to haggle? Pay 1000 gold / to become a merchant (y/n)?", mode=0x00)
01: exec_selector(0x05)  # open_mages_guild
```

**Event 18** — triggers: (9,6)/ENTER

```hex
02 0c 09 10 01 0f 24 32 00 11 01 0c 03 36 01 12 29
```

```
00: show_text_block(str[12] "A talkative polar bear sells passage / to Vulcania for 50 gold. Travel (")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(50)
03: end_script()
04: cond = check_gold_at_least(50)
05: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[18] "Not enough gold!")
06: map_transition(0x03, 0x36)
07: show_text_basic(str[18] "Not enough gold!")
08: set_abort_and_exit()
```

**Event 19** — triggers: (11,6)/ENTER

```hex
02 0d 09 10 01 0f 24 0a 00 11 01 0c 04 f4 01 12 29
```

```
00: show_text_block(str[13] ""I am Jean-Luc. Energize you to / Sandsobar for 10 gold (y/n)?"")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(10)
03: end_script()
04: cond = check_gold_at_least(10)
05: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[18] "Not enough gold!")
06: map_transition(0x04, 0xF4)
07: show_text_basic(str[18] "Not enough gold!")
08: set_abort_and_exit()
```

**Event 20** — triggers: (11,15)/DIR_SPECIAL

```hex
02 0e 09 10 01 0f 0c 05 3c
```

```
00: show_text_block(str[14] "A pathway leads to the snowdrift. / Exit (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x05, 0x3C)
03: end_script()
04: map_transition(0x05, 0x3C)
```

**Event 21** — triggers: (6,7)/0x60

```hex
01 0f 09 10 01 0f 0c 13 1e
```

```
00: show_text_basic(str[15] "Stairs to Cavern. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x13, 0x1E)
03: end_script()
04: map_transition(0x13, 0x1E)
```

**Event 22** — triggers: (14,2)/ALWAYS

```hex
02 10 09 10 01 0f 02 11 26 15 09 6d 0f 10 04 24 fa 00 11 0a 18 09 6d f0 04 0f 15 09 6d f0 10 04 24 fa 00 11 04 18 09 6d 0f 40 0f 01 13 29 01 12 29
```

```
00: show_text_block(str[16] "An armed priest shouts, "Fight for the / cause!" Pay 250 gold to crusade")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_block(str[17] "Who will learn this skill (1-8)?")
03: end_script()
04: show_text_block(str[17] "Who will learn this skill (1-8)?")
05: selected = select_party_member()
06: apply_party(count=0x09, op=0x6D, val=0x0F)
07: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x09, op=0x6D, val=0xF0)
08: cond = check_gold_at_least(250)
09: if not cond: skip_tokens(10)
    # skip -> show_text_basic(str[18] "Not enough gold!")
10: apply_party_masked(count=0x09, set=0x6D, and=0xF0, or=0x04)
11: end_script()
12: apply_party(count=0x09, op=0x6D, val=0xF0)
13: if cond: skip_tokens(4)
    # skip -> show_text_basic(str[19] "You already have two skills!")
14: cond = check_gold_at_least(250)
15: if not cond: skip_tokens(4)
    # skip -> show_text_basic(str[18] "Not enough gold!")
16: apply_party_masked(count=0x09, set=0x6D, and=0x0F, or=0x40)
17: end_script()
18: show_text_basic(str[19] "You already have two skills!")
19: set_abort_and_exit()
20: show_text_basic(str[18] "Not enough gold!")
21: set_abort_and_exit()
```

**Event 23** — triggers: (13,5)/ENTER

```hex
02 14 09 10 01 0f 02 11 26 15 09 6d 0f 10 04 24 e8 03 11 0a 18 09 6d f0 0a 0f 15 09 6d f0 10 04 24 e8 03 11 04 18 09 6d 0f a0 0f 01 13 29 01 12 29
```

```
00: show_text_block(str[20] "Don't ya want to haggle? Pay 1000 gold / to become a merchant (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_block(str[17] "Who will learn this skill (1-8)?")
03: end_script()
04: show_text_block(str[17] "Who will learn this skill (1-8)?")
05: selected = select_party_member()
06: apply_party(count=0x09, op=0x6D, val=0x0F)
07: if cond: skip_tokens(4)
    # skip -> apply_party(count=0x09, op=0x6D, val=0xF0)
08: cond = check_gold_at_least(1000)
09: if not cond: skip_tokens(10)
    # skip -> show_text_basic(str[18] "Not enough gold!")
10: apply_party_masked(count=0x09, set=0x6D, and=0xF0, or=0x0A)
11: end_script()
12: apply_party(count=0x09, op=0x6D, val=0xF0)
13: if cond: skip_tokens(4)
    # skip -> show_text_basic(str[19] "You already have two skills!")
14: cond = check_gold_at_least(1000)
15: if not cond: skip_tokens(4)
    # skip -> show_text_basic(str[18] "Not enough gold!")
16: apply_party_masked(count=0x09, set=0x6D, and=0x0F, or=0xA0)
17: end_script()
18: show_text_basic(str[19] "You already have two skills!")
19: set_abort_and_exit()
20: show_text_basic(str[18] "Not enough gold!")
21: set_abort_and_exit()
```

**Event 24** — triggers: (13,8)/DIR_N?

```hex
0e 38
```

```
00: exec_selector(0x38)
```

**Event 25** — triggers: (10,15)/ENTER

```hex
17 09 00 11 01 14 0e 39
```

```
00: cond = load_var8(group=0x09, index=0x00)
01: if not cond: skip_tokens(1)
    # skip -> exec_selector(0x39)
02: clear_current_tile_event_flag()
03: exec_selector(0x39)
```

**Event 26** — triggers: (9,15)/0xA0

```hex
16 01 e6 10 01 0e 3a 14
```

```
00: cond = check_monster_present(0x01, 0xE6)
01: if cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
02: exec_selector(0x3A)
03: clear_current_tile_event_flag()
```

**Event 27** — triggers: (7,14)/ANY_DIR

```hex
28 01 e6 11 01 0e 3b 0f
```

```
00: cond = consume_item(item_id=230, name="Emerald Ring", probe=1)
01: if not cond: skip_tokens(1)
    # skip -> end_script()
02: exec_selector(0x3B)
03: end_script()
```

**Event 28** — triggers: (8,15)/ANY_DIR

```hex
2b 01 12 eb eb eb eb eb eb eb eb 00 00 00 00 2a 10 27 00 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> set_treasure(gold/exp=10000, gems=0, items=[0, 0, 0])
01: encounter_setup(monsters=EB EB EB EB EB EB EB EB 00 00, flags=00 00)
02: set_treasure(gold/exp=10000, gems=0, items=[0, 0, 0])
03: clear_current_tile_event_flag()
```

**Event 29** — triggers: (5,3)/ANY_DIR

```hex
15 00 7b 80 10 01 0f 0e 37
```

```
00: apply_party(count=0x00, op=0x7B, val=0x80)
01: if cond: skip_tokens(1)
    # skip -> exec_selector(0x37)
02: end_script()
03: exec_selector(0x37)
```

**Event 30** — triggers: (12,15)/DIR_N?

```hex
02 16 09 10 01 0f 01 18 0d 00 21 7f 11 32 29
```

```
00: show_text_block(str[22] "A large button protrudes from the / wall. Push it (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> show_text_basic(str[24] "Click!")
03: end_script()
04: show_text_basic(str[24] "Click!")
05: engine_call(0x00)
06: set_tile((y=7,x=15), 0x11, 0x32)
07: set_abort_and_exit()
```

**Event 31** — triggers: (1,3)/ENTER, (1,5)/ENTER, (1,7)/ENTER, (1,9)/ENTER, (1,11)/ENTER, (1,13)/ENTER, (3,2)/DIR_N?, (3,4)/DIR_N?, (3,6)/DIR_N?, (3,8)/DIR_N?, (3,10)/DIR_N?, (3,12)/DIR_N?

```hex
04 15
```

```
00: show_text_above_door(str[21] "Frozen Monster")
```

**Event 32** — triggers: (7,2)/ALWAYS

```hex
05 17
```

```
00: show_text_popup_style_a(str[23] "Monster Freezing / Authorized Personnel / Only!")
```

**Event 33** — triggers: (0,5)/DIR_SPECIAL, (3,0)/DIR_N?, (3,15)/ENTER, (10,0)/DIR_N?, (15,4)/ALWAYS, (15,12)/ALWAYS

```hex
2b 01 0e 0c 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x0C)
02: clear_current_tile_event_flag()
```

**Event 34** — triggers: (2,2)/ANY_DIR

```hex
2b 01 0e 0f 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x0F)
02: clear_current_tile_event_flag()
```

**Event 35** — triggers: (2,3)/ANY_DIR

```hex
2b 01 0e 34 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x34)
02: clear_current_tile_event_flag()
```

**Event 36** — triggers: (2,4)/ANY_DIR

```hex
2b 01 0e 26 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x26)
02: clear_current_tile_event_flag()
```

**Event 37** — triggers: (2,5)/ANY_DIR

```hex
2b 01 0e 2a 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2A)
02: clear_current_tile_event_flag()
```

**Event 38** — triggers: (2,6)/ANY_DIR

```hex
2b 01 0e 2b 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2B)
02: clear_current_tile_event_flag()
```

**Event 39** — triggers: (2,7)/ANY_DIR

```hex
2b 01 0e 2c 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2C)
02: clear_current_tile_event_flag()
```

**Event 40** — triggers: (2,8)/ANY_DIR

```hex
2b 01 0e 2d 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2D)
02: clear_current_tile_event_flag()
```

**Event 41** — triggers: (2,9)/ANY_DIR

```hex
2b 01 0e 14 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x14)
02: clear_current_tile_event_flag()
```

**Event 42** — triggers: (2,10)/ANY_DIR

```hex
2b 01 0e 2e 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2E)
02: clear_current_tile_event_flag()
```

**Event 43** — triggers: (2,11)/ANY_DIR

```hex
2b 01 0e 2f 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2F)
02: clear_current_tile_event_flag()
```

**Event 44** — triggers: (2,12)/ANY_DIR

```hex
2b 01 0e 30 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x30)
02: clear_current_tile_event_flag()
```

**Event 45** — triggers: (2,13)/ANY_DIR

```hex
2b 01 0e 31 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x31)
02: clear_current_tile_event_flag()
```

**Event 46** — triggers: (10,3)/ANY_DIR

```hex
2b 01 0e 32 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x32)
02: clear_current_tile_event_flag()
```

**Event 47** — triggers: (9,3)/ANY_DIR

```hex
2b 01 0e 33 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x33)
02: clear_current_tile_event_flag()
```

**Event 48** — triggers: (9,4)/ANY_DIR

```hex
2b 01 0e 35 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x35)
02: clear_current_tile_event_flag()
```

**Event 49** — triggers: (10,4)/ANY_DIR

```hex
2b 01 0e 36 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x36)
02: clear_current_tile_event_flag()
```

**Event 50** — triggers: (7,9)/DIR_N?

```hex
0b 14 00 0e 50
```

```
00: set_service_context(str[20] "Don't ya want to haggle? Pay 1000 gold / to become a merchant (y/n)?", mode=0x00)
01: exec_selector(0x50)
```

**Event 51** — triggers: (4,13)/DIR_N?

```hex
05 19
```

```
00: show_text_popup_style_a(str[25] "MISSING / Old Hanna / Last seen / wearing an / Emerald Ring")
```

**Event 52** — triggers: (8,8)/0x30

```hex
0e 3c
```

```
00: exec_selector(0x3C)
```

**Event 53** — triggers: (9,13)/0xA0

```hex
0b 0e 00 0e 3d
```

```
00: set_service_context(str[14] "A pathway leads to the snowdrift. / Exit (y/n)?", mode=0x00)
01: exec_selector(0x3D)
```

## String table

- `[00]` <EMPTY>
- `[01]` Tundaran Arms Inn
- `[02]` Lucky Dog Saloon
- `[03]` Thundrax Weaponry
- `[04]` White Dove Temple
- `[05]` Enhancement Center
- `[06]` Polar Passage Portal
- `[07]` La Porte
- `[08]` Mystical Mage Guild
- `[09]` Saracen's Denial
- `[10]` International Market
- `[11]` Columbus's Sextant
- `[12]` A talkative polar bear sells passage / to Vulcania for 50 gold. Travel (y/n)?
- `[13]` "I am Jean-Luc. Energize you to / Sandsobar for 10 gold (y/n)?"
- `[14]` A pathway leads to the snowdrift. / Exit (y/n)?
- `[15]` Stairs to Cavern. Descend (y/n)?
- `[16]` An armed priest shouts, "Fight for the / cause!" Pay 250 gold to crusade (y/n)?
- `[17]` Who will learn this skill (1-8)?
- `[18]` Not enough gold!
- `[19]` You already have two skills!
- `[20]` Don't ya want to haggle? Pay 1000 gold / to become a merchant (y/n)?
- `[21]` Frozen Monster
- `[22]` A large button protrudes from the / wall. Push it (y/n)?
- `[23]` Monster Freezing / Authorized Personnel / Only!
- `[24]` Click!
- `[25]` MISSING / Old Hanna / Last seen / wearing an / Emerald Ring
- `[26]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
