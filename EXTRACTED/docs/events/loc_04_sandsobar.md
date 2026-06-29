# Location 04 — Sandsobar

- **event.dat** offset `0x001838`, length **1447** bytes
- **Map:** map screen **4**; Sandsobar
- **Record kind:** `standard`
- **Triggers:** 50; **script segments:** 46; **strings:** 29

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,2) | `0x02` | **12** | DIR_E? |
| (0,3) | `0x03` | **36** | DIR_E? |
| (0,8) | `0x08` | **8** | DIR_N? |
| (0,10) | `0x0A` | **15** | DIR_W? |
| (0,11) | `0x0B` | **30** | DIR_S? |
| (0,13) | `0x0D` | **28** | DIR_W? |
| (0,14) | `0x0E` | **28** | DIR_E? |
| (0,15) | `0x0F` | **31** | DIR_E? |
| (1,8) | `0x18` | **24** | DIR_N? |
| (1,10) | `0x1A` | **27** | DIR_E? |
| (1,11) | `0x1B` | **28** | DIR_E? |
| (1,13) | `0x1D` | **28** | DIR_S? |
| (2,6) | `0x26` | **22** | DIR_S? |
| (2,11) | `0x2B` | **28** | DIR_W? |
| (2,12) | `0x2C` | **28** | DIR_E? |
| (2,13) | `0x2D` | **32** | DIR_E? |
| (2,15) | `0x2F` | **39** | ANY_DIR |
| (3,6) | `0x36` | **10** | DIR_S? |
| (3,11) | `0x3B` | **29** | DIR_N? |
| (3,13) | `0x3D` | **28** | DIR_N? |
| (3,14) | `0x3E` | **38** | DIR_E? |
| (3,15) | `0x3F` | **28** | DIR_N? |
| (4,2) | `0x42` | **11** | DIR_E? |
| (4,3) | `0x43` | **34** | DIR_E? |
| (4,7) | `0x47` | **43** | DIR_W? |
| (4,8) | `0x48` | **42** | DIR_W? |
| (5,0) | `0x50` | **35** | DIR_W? |
| (5,1) | `0x51` | **13** | DIR_W? |
| (6,9) | `0x69` | **41** | DIR_W? |
| (7,2) | `0x72` | **5** | DIR_E? |
| (7,3) | `0x73` | **20** | DIR_E? |
| (7,5) | `0x75` | **9** | DIR_E? |
| (7,6) | `0x76` | **21** | DIR_E? |
| (8,10) | `0x8A` | **6** | DIR_E? |
| (8,13) | `0x8D` | **26** | ANY_DIR |
| (10,2) | `0xA2` | **1** | DIR_E? |
| (10,3) | `0xA3` | **19** | DIR_E? |
| (10,4) | `0xA4` | **37** | ANY_DIR |
| (10,5) | `0xA5` | **3** | DIR_W? |
| (11,3) | `0xB3` | **44** | DIR_W? |
| (11,4) | `0xB4` | **18** | DIR_N? |
| (11,5) | `0xB5` | **4** | DIR_E? |
| (11,6) | `0xB6` | **17** | DIR_E? |
| (12,0) | `0xC0` | **25** | DIR_W? |
| (14,0) | `0xE0` | **14** | DIR_W? |
| (14,4) | `0xE4` | **7** | DIR_N? |
| (14,7) | `0xE7` | **2** | DIR_N? |
| (15,4) | `0xF4` | **23** | DIR_N? |
| (15,7) | `0xF7` | **16** | DIR_N? |
| (15,12) | `0xFC` | **40** | ANY_DIR |

## Events

**Event 01** — triggers: (10,2)/DIR_E?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Hourglass Inn")
```

**Event 02** — triggers: (14,7)/DIR_N?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Big Al's Accessories")
```

**Event 03** — triggers: (10,5)/DIR_W?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Red Lantern Tavern")
```

**Event 04** — triggers: (11,5)/DIR_E?

```hex
04 04
```

```
00: show_text_above_door(str[4] "Temple Benedictus")
```

**Event 05** — triggers: (7,2)/DIR_E?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Sheik Training Arena")
```

**Event 06** — triggers: (8,10)/DIR_E?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Monster Bowl")
```

**Event 07** — triggers: (14,4)/DIR_N?

```hex
04 07
```

```
00: show_text_above_door(str[7] "Sirocco Portal")
```

**Event 08** — triggers: (0,8)/DIR_N?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Portal Dune")
```

**Event 09** — triggers: (7,5)/DIR_E?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Whirlwind Mage Guild")
```

**Event 10** — triggers: (3,6)/DIR_S?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Fitpro Locksmith")
```

**Event 11** — triggers: (4,2)/DIR_E?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "The Embassy")
```

**Event 12** — triggers: (0,2)/DIR_E?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "The Sandy Dunes")
```

**Event 13** — triggers: (5,1)/DIR_W?

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Sly's Opportunities")
```

**Event 14** — triggers: (14,0)/DIR_W?

```hex
02 0e 09 10 01 0f 0c 28 a4
```

```
00: show_text_block(str[14] "Enter the Plains of Peril (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x28, 0xA4)
03: end_script()
04: map_transition(0x28, 0xA4)
```

**Event 15** — triggers: (0,10)/DIR_W?

```hex
02 0f 09 10 01 0f 0c 15 7c
```

```
00: show_text_block(str[15] "Stairs to Cavern. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x15, 0x7C)
03: end_script()
04: map_transition(0x15, 0x7C)
```

**Event 16** — triggers: (15,7)/DIR_N?

```hex
0b 02 00 0e 06
```

```
00: service_sign(idx=0x02 -> sign 62 [62.anm], pos=0x00)
01: exec_selector(0x06)  # open_blacksmith_shop
```

**Event 17** — triggers: (11,6)/DIR_E?

```hex
0b 05 00 0e 04
```

```
00: service_sign(idx=0x05 -> sign 67 [67.anm], pos=0x00)
01: exec_selector(0x04)  # open_temple
```

**Event 18** — triggers: (11,4)/DIR_N?

```hex
0b 04 00 0e 03
```

```
00: service_sign(idx=0x04 -> sign 66 [66.anm], pos=0x00)
01: exec_selector(0x03)  # open_tavern_food
```

**Event 19** — triggers: (10,3)/DIR_E?

```hex
0b 03 00 0e 01
```

```
00: service_sign(idx=0x03 -> sign 63 [63.anm], pos=0x00)
01: exec_selector(0x01)  # open_inn_lodging
```

**Event 20** — triggers: (7,3)/DIR_E?

```hex
0b 06 00 0e 02
```

```
00: service_sign(idx=0x06 -> sign 68 [68.anm], pos=0x00)
01: exec_selector(0x02)  # open_training
```

**Event 21** — triggers: (7,6)/DIR_E?

```hex
0b 14 00 0e 05
```

```
00: service_sign(idx=0x14 -> sign 37 [37.anm], pos=0x00)
01: exec_selector(0x05)  # open_mages_guild
```

**Event 22** — triggers: (2,6)/DIR_S?

```hex
0e 49
```

```
00: exec_selector(0x49)
```

**Event 23** — triggers: (15,4)/DIR_N?

```hex
02 13 09 10 01 0f 24 32 00 10 02 01 12 29 0c 02 b6
```

```
00: show_text_block(str[19] "A dusty nomad offers transport to / Tundara for 50 gold. Accept (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(50)
03: end_script()
04: cond = check_gold_at_least(50)
05: if cond: skip_tokens(2)
    # skip -> map_transition(0x02, 0xB6)
06: show_text_basic(str[18] "Not enough gold!")
07: set_abort_and_exit()
08: map_transition(0x02, 0xB6)
```

**Event 24** — triggers: (1,8)/DIR_N?

```hex
02 14 09 10 01 0f 24 14 00 10 02 01 12 29 0c 00 50
```

```
00: show_text_block(str[20] "A desert trader stands next to a / glowing aura of power. "Hi! I sell / ")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(20)
03: end_script()
04: cond = check_gold_at_least(20)
05: if cond: skip_tokens(2)
    # skip -> map_transition(0x00, 0x50)
06: show_text_basic(str[18] "Not enough gold!")
07: set_abort_and_exit()
08: map_transition(0x00, 0x50)
```

**Event 25** — triggers: (12,0)/DIR_W?

```hex
0b 14 00 02 15 0a 10 01 0f 24 c8 00 11 02 18 00 74 df 20 0f 01 12 08 0f
```

```
00: service_sign(idx=0x14 -> sign 37 [37.anm], pos=0x00)
01: show_text_block(str[21] "Drunk beyond belief, a sorcerer / mumbles, "... Join da Magesh Gill... /")
02: cond = prompt_yes_no(mode=1)
03: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(200)
04: end_script()
05: cond = check_gold_at_least(200)
06: if not cond: skip_tokens(2)
    # skip -> show_text_basic(str[18] "Not enough gold!")
07: apply_party_masked(count=0x00, set=0x74, and=0xDF, or=0x20)
08: end_script()
09: show_text_basic(str[18] "Not enough gold!")
10: wait_key()
11: end_script()
```

**Event 26** — triggers: (8,13)/ANY_DIR

```hex
0e 08
```

```
00: exec_selector(0x08)  # open_arena_shop
```

**Event 27** — triggers: (1,10)/DIR_E?

```hex
06 16
```

```
00: show_text_popup_style_b(str[22] "The / Slums")
```

**Event 28** — triggers: (0,13)/DIR_W?, (0,14)/DIR_E?, (1,11)/DIR_E?, (1,13)/DIR_S?, (2,11)/DIR_W?, (2,12)/DIR_E?, (3,13)/DIR_N?, (3,15)/DIR_N?

```hex
02 17 29
```

```
00: show_text_block(str[23] "Sewage and muck ooze under foot!")
01: set_abort_and_exit()
```

**Event 29** — triggers: (3,11)/DIR_N?

```hex
05 18
```

```
00: show_text_popup_style_a(str[24] "S 1,51 Em Pleh H S X / l  N           i p O / uR W  Beware of  o X / mu ")
```

**Event 30** — triggers: (0,11)/DIR_S?

```hex
2b 02 0b 12 00 0e 4b 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: service_sign(idx=0x12 -> sign 4 [4.anm], pos=0x00)
02: exec_selector(0x4B)
03: clear_current_tile_event_flag()
```

**Event 31** — triggers: (0,15)/DIR_E?

```hex
2b 02 0b 0e 00 0e 4a 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: service_sign(idx=0x0E -> sign 12 [12.anm], pos=0x00)
02: exec_selector(0x4A)
03: clear_current_tile_event_flag()
```

**Event 32** — triggers: (2,13)/DIR_E?

```hex
02 10 29
```

```
00: show_text_block(str[16] "An old wino belches, "The snowbeast / has a secret lair in the city wall")
01: set_abort_and_exit()
```

**Event 34** — triggers: (4,3)/DIR_E?

```hex
0e 4d
```

```
00: exec_selector(0x4D)
```

**Event 35** — triggers: (5,0)/DIR_W?

```hex
0e 4e
```

```
00: exec_selector(0x4E)
```

**Event 36** — triggers: (0,3)/DIR_E?

```hex
0e 4f
```

```
00: exec_selector(0x4F)
```

**Event 37** — triggers: (10,4)/ANY_DIR

```hex
17 02 00 10 03 15 00 77 08 11 02 0e 4c 14 0f
```

```
00: cond = load_var8(group=0x02, index=0x00)
01: if cond: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
02: apply_party(count=0x00, op=0x77, val=0x08)
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: exec_selector(0x4C)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 38** — triggers: (3,14)/DIR_E?

```hex
01 19 29
```

```
00: show_text_basic(str[25] "Glowing eyes follow your progress!")
01: set_abort_and_exit()
```

**Event 39** — triggers: (2,15)/ANY_DIR

```hex
2b 01 12 09 09 09 09 09 09 09 09 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=09 09 09 09 09 09 09 09 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 40** — triggers: (15,12)/ANY_DIR

```hex
2b 01 0e 2c 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2C)
02: clear_current_tile_event_flag()
```

**Event 41** — triggers: (6,9)/DIR_W?

```hex
04 1a
```

```
00: show_text_above_door(str[26] "The Wizard's Eye")
```

**Event 42** — triggers: (4,8)/DIR_W?

```hex
04 1b
```

```
00: show_text_above_door(str[27] "The Beggar's Gift")
```

**Event 43** — triggers: (4,7)/DIR_W?

```hex
0b 0e 00 0e 51
```

```
00: service_sign(idx=0x0E -> sign 12 [12.anm], pos=0x00)
01: exec_selector(0x51)
```

**Event 44** — triggers: (11,3)/DIR_W?

```hex
2b 01 0e 53 0e 54
```

```
00: skip_tokens(1)
    # skip -> exec_selector(0x54)
01: exec_selector(0x53)
02: exec_selector(0x54)
```

## String table

- `[00]` <EMPTY>
- `[01]` Hourglass Inn
- `[02]` Big Al's Accessories
- `[03]` Red Lantern Tavern
- `[04]` Temple Benedictus
- `[05]` Sheik Training Arena
- `[06]` Monster Bowl
- `[07]` Sirocco Portal
- `[08]` Portal Dune
- `[09]` Whirlwind Mage Guild
- `[10]` Fitpro Locksmith
- `[11]` The Embassy
- `[12]` The Sandy Dunes
- `[13]` Sly's Opportunities
- `[14]` Enter the Plains of Peril (y/n)?
- `[15]` Stairs to Cavern. Descend (y/n)?
- `[16]` An old wino belches, "The snowbeast / has a secret lair in the city walls!"
- `[17]` *** Backpacks Full ***
- `[18]` Not enough gold!
- `[19]` A dusty nomad offers transport to / Tundara for 50 gold. Accept (y/n)?
- `[20]` A desert trader stands next to a / glowing aura of power. "Hi! I sell / magical transport to Middlegate for 20 / gold. Travel (y/n)?"
- `[21]` Drunk beyond belief, a sorcerer / mumbles, "... Join da Magesh Gill... / burp! ...200 gold ..." Pay (y/n)?
- `[22]` The / Slums
- `[23]` Sewage and muck ooze under foot!
- `[24]` S 1,51 Em Pleh H S X / l  N           i p O / uR W  Beware of  o X / mu C    Eyes!  M rO  / rl             o kr  / ae G The       m  c  / t  a  Meisters   a   / s  m             n   /    e Kilroy was here / The Cripples R Studs
- `[25]` Glowing eyes follow your progress!
- `[26]` The Wizard's Eye
- `[27]` The Beggar's Gift
- `[28]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
