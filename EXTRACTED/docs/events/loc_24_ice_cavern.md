# Location 24 — Ice Cavern

- **event.dat** offset `0x0081AD`, length **1268** bytes
- **Map:** map screen **24**; Ice Cavern
- **Record kind:** `standard`
- **Triggers:** 48; **script segments:** 41; **strings:** 30

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **29** | 0x30 |
| (0,8) | `0x08` | **31** | 0x60 |
| (0,12) | `0x0C` | **33** | 0x30 |
| (0,15) | `0x0F` | **18** | ANY_DIR |
| (1,1) | `0x11` | **7** | ANY_DIR |
| (1,7) | `0x17` | **9** | ANY_DIR |
| (1,13) | `0x1D` | **11** | ANY_DIR |
| (2,1) | `0x21` | **39** | 0xA0 |
| (2,7) | `0x27` | **39** | 0xA0 |
| (2,13) | `0x2D` | **39** | 0xA0 |
| (3,3) | `0x33` | **30** | 0x30 |
| (3,10) | `0x3A` | **32** | 0x70 |
| (4,4) | `0x44` | **8** | ANY_DIR |
| (4,10) | `0x4A` | **10** | ANY_DIR |
| (5,4) | `0x54` | **39** | 0xA0 |
| (5,10) | `0x5A` | **39** | 0xA0 |
| (6,1) | `0x61` | **20** | DIR_S? |
| (6,4) | `0x64` | **21** | DIR_S? |
| (6,7) | `0x67` | **28** | DIR_S? |
| (6,10) | `0x6A` | **22** | DIR_S? |
| (6,13) | `0x6D` | **23** | 0x30 |
| (7,0) | `0x70` | **1** | DIR_W? |
| (7,14) | `0x7E` | **3** | DIR_E? |
| (7,15) | `0x7F` | **5** | DIR_E? |
| (8,0) | `0x80` | **6** | ANY_DIR |
| (8,14) | `0x8E` | **2** | DIR_E? |
| (8,15) | `0x8F` | **4** | DIR_E? |
| (9,1) | `0x91` | **27** | DIR_N? |
| (9,4) | `0x94` | **26** | DIR_N? |
| (9,7) | `0x97` | **25** | DIR_N? |
| (9,10) | `0x9A` | **24** | DIR_N? |
| (9,13) | `0x9D` | **19** | 0x90 |
| (10,4) | `0xA4` | **39** | 0xA0 |
| (10,10) | `0xAA` | **39** | 0xA0 |
| (11,4) | `0xB4` | **13** | ANY_DIR |
| (11,10) | `0xBA` | **16** | ANY_DIR |
| (12,5) | `0xC5` | **35** | DIR_N?+DIR_E? |
| (12,9) | `0xC9` | **38** | 0x90 |
| (13,1) | `0xD1` | **39** | 0xA0 |
| (13,7) | `0xD7` | **39** | 0xA0 |
| (13,13) | `0xDD` | **39** | 0xA0 |
| (14,1) | `0xE1` | **12** | ANY_DIR |
| (14,7) | `0xE7` | **14** | ANY_DIR |
| (14,13) | `0xED` | **15** | ANY_DIR |
| (14,15) | `0xEF` | **17** | ANY_DIR |
| (15,0) | `0xF0` | **34** | 0x90 |
| (15,6) | `0xF6` | **36** | 0x90 |
| (15,14) | `0xFE` | **37** | DIR_N?+DIR_E? |

## Events

**Event 01** — triggers: (7,0)/DIR_W?

```hex
01 01 09 11 01 0c 06 d4 0f
```

```
00: show_text_basic(str[1] "Reenter the Tundra (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x06, 0xD4)
04: end_script()
```

**Event 02** — triggers: (8,14)/DIR_E?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Males only")
```

**Event 03** — triggers: (7,14)/DIR_E?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Females only")
```

**Event 04** — triggers: (8,15)/DIR_E?

```hex
2d 40 00 10 04 01 1b 07 0d 09 0c 18 8e 02 04 09 11 01 0e 6a 0f
```

```
00: cond = check_member_attr(fields=0x40, value=0x00)
01: if cond: skip_tokens(4)
    # skip -> show_text_block(str[4] "A pillar of light extends from floor / to ceiling. Step into it (y/n)?")
02: show_text_basic(str[27] "You must obey the sign!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x18, 0x8E)
06: show_text_block(str[4] "A pillar of light extends from floor / to ceiling. Step into it (y/n)?")
07: cond = prompt_yes_no()
08: if not cond: skip_tokens(1)
    # skip -> end_script()
09: exec_selector(0x6A)
10: end_script()
```

**Event 05** — triggers: (7,15)/DIR_E?

```hex
2d 41 00 10 04 01 1b 07 0d 09 0c 18 7e 02 04 09 11 01 0e 6a 0f
```

```
00: cond = check_member_attr(fields=0x41, value=0x00)
01: if cond: skip_tokens(4)
    # skip -> show_text_block(str[4] "A pillar of light extends from floor / to ceiling. Step into it (y/n)?")
02: show_text_basic(str[27] "You must obey the sign!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x18, 0x7E)
06: show_text_block(str[4] "A pillar of light extends from floor / to ceiling. Step into it (y/n)?")
07: cond = prompt_yes_no()
08: if not cond: skip_tokens(1)
    # skip -> end_script()
09: exec_selector(0x6A)
10: end_script()
```

**Event 06** — triggers: (8,0)/ANY_DIR

```hex
2b 01 12 b6 b6 a4 a4 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=B6 B6 A4 A4 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (1,1)/ANY_DIR

```hex
2b 01 12 21 21 21 21 21 21 21 21 21 21 21 5a 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=21 21 21 21 21 21 21 21 21 21, flags=21 5A)
02: clear_current_tile_event_flag()
```

**Event 08** — triggers: (4,4)/ANY_DIR

```hex
2b 01 12 03 03 03 03 03 03 03 03 03 03 03 f0 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=03 03 03 03 03 03 03 03 03 03, flags=03 F0)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (1,7)/ANY_DIR

```hex
2b 01 12 30 30 30 30 30 30 30 30 30 30 30 37 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=30 30 30 30 30 30 30 30 30 30, flags=30 37)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (4,10)/ANY_DIR

```hex
2b 01 12 14 14 14 14 14 14 14 14 14 14 14 8c 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=14 14 14 14 14 14 14 14 14 14, flags=14 8C)
02: clear_current_tile_event_flag()
```

**Event 11** — triggers: (1,13)/ANY_DIR

```hex
2b 01 12 11 11 11 11 11 11 11 11 11 11 11 96 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=11 11 11 11 11 11 11 11 11 11, flags=11 96)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (14,1)/ANY_DIR

```hex
2b 01 12 1e 1e 1e 1e 1e 1e 1e 1e 1e 1e 1e 8c 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1E 1E 1E 1E 1E 1E 1E 1E 1E 1E, flags=1E 8C)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (11,4)/ANY_DIR

```hex
2b 01 12 32 32 32 32 32 32 32 32 32 32 32 28 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=32 32 32 32 32 32 32 32 32 32, flags=32 28)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (14,7)/ANY_DIR

```hex
2b 01 12 05 05 05 05 05 05 05 05 05 05 05 f0 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=05 05 05 05 05 05 05 05 05 05, flags=05 F0)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (14,13)/ANY_DIR

```hex
2b 01 12 44 44 44 44 44 44 44 44 44 44 44 23 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=44 44 44 44 44 44 44 44 44 44, flags=44 23)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (11,10)/ANY_DIR

```hex
2b 01 12 09 09 09 09 09 09 09 09 09 09 09 be 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=09 09 09 09 09 09 09 09 09 09, flags=09 BE)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (14,15)/ANY_DIR

```hex
2b 01 12 7e 7e 38 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=7E 7E 38 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (0,15)/ANY_DIR

```hex
2b 01 12 76 76 38 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=76 76 38 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (9,13)/0x90

```hex
06 07
```

```
00: show_text_popup_style_b(str[7] "Ogre's / Pantry")
```

**Event 20** — triggers: (6,1)/DIR_S?

```hex
06 08
```

```
00: show_text_popup_style_b(str[8] "Beware - / Rat Nest")
```

**Event 21** — triggers: (6,4)/DIR_S?

```hex
06 09
```

```
00: show_text_popup_style_b(str[9] "Kobold / Cavern")
```

**Event 22** — triggers: (6,10)/DIR_S?

```hex
06 0a
```

```
00: show_text_popup_style_b(str[10] "The / Bat Cave")
```

**Event 23** — triggers: (6,13)/0x30

```hex
06 0b
```

```
00: show_text_popup_style_b(str[11] "Orc / Retreat")
```

**Event 24** — triggers: (9,10)/DIR_N?

```hex
06 0c
```

```
00: show_text_popup_style_b(str[12] "Thief's / Corner")
```

**Event 25** — triggers: (9,7)/DIR_N?

```hex
06 0d
```

```
00: show_text_popup_style_b(str[13] "Goblin / Chamber")
```

**Event 26** — triggers: (9,4)/DIR_N?

```hex
06 0e
```

```
00: show_text_popup_style_b(str[14] "Short and / Insane")
```

**Event 27** — triggers: (9,1)/DIR_N?

```hex
06 0f
```

```
00: show_text_popup_style_b(str[15] "Welcoming / Committee")
```

**Event 28** — triggers: (6,7)/DIR_S?

```hex
06 10
```

```
00: show_text_popup_style_b(str[16] "Big Bug / Collection")
```

**Event 29** — triggers: (0,0)/0x30

```hex
01 11 07 19 01 af c8 00 19 01 af c8 00 10 02 01 1c 07 14
```

```
00: show_text_basic(str[17] "You have found some Magic Meals!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0xAF, f40=0xC8, f46=0x00)
03: add_party_entity(0x01, f3a=0xAF, f40=0xC8, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_basic(str[28] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
```

**Event 30** — triggers: (3,3)/0x30

```hex
02 12 07 19 01 51 c8 03 19 01 35 c8 03 10 02 01 1c 07 14
```

```
00: show_text_block(str[18] "You have found an Ice Sickle / and a Cold Blade.")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x51, f40=0xC8, f46=0x03)
03: add_party_entity(0x01, f3a=0x35, f40=0xC8, f46=0x03)
04: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
05: show_text_basic(str[28] "*** Backpacks Full ***")
06: wait_for_space()
07: clear_current_tile_event_flag()
```

**Event 31** — triggers: (0,8)/0x60

```hex
01 13 07 19 01 d8 c8 00 10 02 01 1c 07 14
```

```
00: show_text_basic(str[19] "You have found a Web Caster.")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0xD8, f40=0xC8, f46=0x00)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[28] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 32** — triggers: (3,10)/0x70

```hex
01 14 07 19 01 04 00 0f 10 02 01 1c 07 14
```

```
00: show_text_basic(str[20] "You have found a +15 Dagger.")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x04, f40=0x00, f46=0x0F)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[28] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 33** — triggers: (0,12)/0x30

```hex
06 15 2a 88 13 00 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: show_text_popup_style_b(str[21] "Gold Here / & & & & &")
01: set_treasure(gold/exp=5000, gems=0, items=[0, 0, 0])
02: clear_current_tile_event_flag()
```

**Event 34** — triggers: (15,0)/0x90

```hex
01 16 07 1f 01 38 02 f4 01 00 14
```

```
00: show_text_basic(str[22] "You have found 500 Gems!")
01: wait_for_space()
02: party_effect(sel=0x01, 38 02 F4 01 00)
03: clear_current_tile_event_flag()
```

**Event 35** — triggers: (12,5)/DIR_N?+DIR_E?

```hex
01 17 07 19 01 c2 14 03 10 02 01 1c 07 14
```

```
00: show_text_basic(str[23] "You have found +3 Speed Boots!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0xC2, f40=0x14, f46=0x03)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[28] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 36** — triggers: (15,6)/0x90

```hex
01 18 07 19 01 79 00 05 10 02 01 1c 07 14
```

```
00: show_text_basic(str[24] "You have found a +5 Cold Shield!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x79, f40=0x00, f46=0x05)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[28] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 37** — triggers: (15,14)/DIR_N?+DIR_E?

```hex
01 19 07 19 01 69 19 03 10 02 01 1c 07 14
```

```
00: show_text_basic(str[25] "You have found a +3 Giant Sling!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x69, f40=0x19, f46=0x03)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[28] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 38** — triggers: (12,9)/0x90

```hex
01 1a 07 19 01 1c 00 05 10 02 01 1c 07 14
```

```
00: show_text_basic(str[26] "You have found a +5 Looter Knife!")
01: wait_for_space()
02: add_party_entity(0x01, f3a=0x1C, f40=0x00, f46=0x05)
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[28] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 39** — triggers: (2,1)/0xA0, (2,7)/0xA0, (2,13)/0xA0, (5,4)/0xA0, (5,10)/0xA0, (10,4)/0xA0, (10,10)/0xA0, (13,1)/0xA0, (13,7)/0xA0, (13,13)/0xA0

```hex
0e 80
```

```
00: exec_selector(0x80)  # special_128
```

## String table

- `[00]` <EMPTY>
- `[01]` Reenter the Tundra (y/n)?
- `[02]` Males only
- `[03]` Females only
- `[04]` A pillar of light extends from floor / to ceiling. Step into it (y/n)?
- `[05]` Y
- `[06]` Z
- `[07]` Ogre's / Pantry
- `[08]` Beware - / Rat Nest
- `[09]` Kobold / Cavern
- `[10]` The / Bat Cave
- `[11]` Orc / Retreat
- `[12]` Thief's / Corner
- `[13]` Goblin / Chamber
- `[14]` Short and / Insane
- `[15]` Welcoming / Committee
- `[16]` Big Bug / Collection
- `[17]` You have found some Magic Meals!
- `[18]` You have found an Ice Sickle / and a Cold Blade.
- `[19]` You have found a Web Caster.
- `[20]` You have found a +15 Dagger.
- `[21]` Gold Here / & & & & &
- `[22]` You have found 500 Gems!
- `[23]` You have found +3 Speed Boots!
- `[24]` You have found a +5 Cold Shield!
- `[25]` You have found a +3 Giant Sling!
- `[26]` You have found a +5 Looter Knife!
- `[27]` You must obey the sign!
- `[28]` *** Backpacks Full ***
- `[29]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
