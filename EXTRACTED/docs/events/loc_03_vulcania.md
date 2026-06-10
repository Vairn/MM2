# Location 03 — Vulcania

- **event.dat** offset `0x0012CE`, length **1386** bytes
- **Map:** map screen **3**; Vulcania
- **Record kind:** `standard`
- **Triggers:** 41; **script segments:** 44; **strings:** 24

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,5) | `0x05` | **19** | DIR_S? |
| (0,7) | `0x07` | **16** | DIR_S? |
| (0,10) | `0x0A` | **22** | DIR_W? |
| (2,0) | `0x20` | **12** | DIR_N? |
| (2,3) | `0x23` | **15** | DIR_W? |
| (2,4) | `0x24` | **41** | ANY_DIR |
| (2,5) | `0x25` | **3** | DIR_W? |
| (2,6) | `0x26` | **5** | DIR_N? |
| (2,7) | `0x27` | **1** | DIR_S? |
| (2,8) | `0x28` | **6** | DIR_N? |
| (2,15) | `0x2F` | **10** | DIR_N? |
| (3,0) | `0x30` | **27** | DIR_N? |
| (3,4) | `0x34` | **17** | DIR_W? |
| (3,5) | `0x35` | **4** | DIR_W? |
| (3,6) | `0x36` | **20** | DIR_N? |
| (3,8) | `0x38` | **21** | DIR_N? |
| (3,15) | `0x3F` | **29** | DIR_N? |
| (5,10) | `0x5A` | **42** | DIR_S? |
| (5,11) | `0x5B` | **18** | DIR_S? |
| (6,11) | `0x6B` | **7** | DIR_S? |
| (7,3) | `0x73` | **24** | DIR_N? |
| (8,0) | `0x80` | **23** | DIR_W? |
| (8,1) | `0x81` | **8** | DIR_W? |
| (8,11) | `0x8B` | **14** | DIR_W? |
| (8,13) | `0x8D` | **9** | DIR_W? |
| (8,14) | `0x8E` | **2** | DIR_E? |
| (8,15) | `0x8F` | **13** | DIR_E? |
| (9,3) | `0x93` | **28** | DIR_S? |
| (9,6) | `0x96` | **30** | 0x30 |
| (9,8) | `0x98` | **33** | 0x60 |
| (10,3) | `0xA3` | **11** | DIR_S? |
| (11,6) | `0xB6` | **31** | 0x90 |
| (11,8) | `0xB8` | **32** | DIR_N?+DIR_E? |
| (12,3) | `0xC3` | **35** | ANY_DIR |
| (12,7) | `0xC7` | **25** | DIR_N? |
| (12,10) | `0xCA` | **26** | DIR_W? |
| (13,3) | `0xD3` | **36** | ANY_DIR |
| (14,3) | `0xE3` | **37** | ANY_DIR |
| (14,9) | `0xE9` | **39** | ANY_DIR |
| (14,11) | `0xEB` | **40** | ANY_DIR |
| (15,3) | `0xF3` | **38** | ANY_DIR |

## Events

**Event 01** — triggers: (2,7)/DIR_S?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Hotel Four")
```

**Event 02** — triggers: (8,14)/DIR_E?

```hex
04 02
```

```
00: show_text_above_door(str[2] "Bestway Blacksmith")
```

**Event 03** — triggers: (2,5)/DIR_W?

```hex
04 03
```

```
00: show_text_above_door(str[3] "Belinthra's Bar")
```

**Event 04** — triggers: (3,5)/DIR_W?

```hex
04 04
```

```
00: show_text_above_door(str[4] "Training Academy")
```

**Event 05** — triggers: (2,6)/DIR_N?

```hex
04 05
```

```
00: show_text_above_door(str[5] "Vulcanian Transport")
```

**Event 06** — triggers: (2,8)/DIR_N?

```hex
04 06
```

```
00: show_text_above_door(str[6] "Vulcanian Export Co.")
```

**Event 07** — triggers: (6,11)/DIR_S?

```hex
04 07
```

```
00: show_text_above_door(str[7] "Blackrock Mage Guild")
```

**Event 08** — triggers: (8,1)/DIR_W?

```hex
04 08
```

```
00: show_text_above_door(str[8] "Lava Locksmith")
```

**Event 09** — triggers: (8,13)/DIR_W?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Vulcan Temple")
```

**Event 10** — triggers: (2,15)/DIR_N?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Profiency Expert")
```

**Event 11** — triggers: (10,3)/DIR_S?

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Disembowelments R Us")
```

**Event 12** — triggers: (2,0)/DIR_N?

```hex
04 0c
```

```
00: show_text_above_door(str[12] "Sergeant Pain School")
```

**Event 13** — triggers: (8,15)/DIR_E?

```hex
0b 02 00 0e 06
```

```
00: set_service_context(str[2] "Bestway Blacksmith", mode=0x00)
01: exec_selector(0x06)  # open_blacksmith_shop
```

**Event 14** — triggers: (8,11)/DIR_W?

```hex
0b 05 00 0e 04
```

```
00: set_service_context(str[5] "Vulcanian Transport", mode=0x00)
01: exec_selector(0x04)  # open_training
```

**Event 15** — triggers: (2,3)/DIR_W?

```hex
0b 04 00 0e 03
```

```
00: set_service_context(str[4] "Training Academy", mode=0x00)
01: exec_selector(0x03)  # open_temple
```

**Event 16** — triggers: (0,7)/DIR_S?

```hex
0b 03 00 0e 01
```

```
00: set_service_context(str[3] "Belinthra's Bar", mode=0x00)
01: exec_selector(0x01)  # open_tavern_food
```

**Event 17** — triggers: (3,4)/DIR_W?

```hex
0b 06 00 0e 02
```

```
00: set_service_context(str[6] "Vulcanian Export Co.", mode=0x00)
01: exec_selector(0x02)  # open_inn_lodging
```

**Event 18** — triggers: (5,11)/DIR_S?

```hex
0b 14 00 0e 05
```

```
00: set_service_context(str[20] "Not enough gold!", mode=0x00)
01: exec_selector(0x05)  # open_mages_guild
```

**Event 19** — triggers: (0,5)/DIR_S?

```hex
02 0d 09 10 01 0f 0c 21 43
```

```
00: show_text_block(str[13] "A passage leads out of Vulcania. / Exit (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x21, 0x43)
03: end_script()
04: map_transition(0x21, 0x43)
```

**Event 20** — triggers: (3,6)/DIR_N?

```hex
02 0e 09 10 01 0f 24 1e 00 11 01 0c 02 96 01 14 29
```

```
00: show_text_block(str[14] "The Vulcanian Transport Company offers / immediate transference to Tunda")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(30)
03: end_script()
04: cond = check_gold_at_least(30)
05: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[20] "Not enough gold!")
06: map_transition(0x02, 0x96)
07: show_text_basic(str[20] "Not enough gold!")
08: set_abort_and_exit()
```

**Event 21** — triggers: (3,8)/DIR_N?

```hex
02 0f 09 10 01 0f 24 64 00 11 01 0c 01 03 01 14 29
```

```
00: show_text_block(str[15] "Wearing a uniform labeled "Vulcanian / Export" a pointy-eared dust devil")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> cond = check_gold_at_least(100)
03: end_script()
04: cond = check_gold_at_least(100)
05: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[20] "Not enough gold!")
06: map_transition(0x01, 0x03)
07: show_text_basic(str[20] "Not enough gold!")
08: set_abort_and_exit()
```

**Event 22** — triggers: (0,10)/DIR_W?

```hex
01 10 09 10 01 0f 0c 14 0a
```

```
00: show_text_basic(str[16] "Stairs to Cavern. Descend (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x14, 0x0A)
03: end_script()
04: map_transition(0x14, 0x0A)
```

**Event 23** — triggers: (8,0)/DIR_W?

```hex
02 11 09 11 07 24 c4 09 11 06 19 00 71 00 00 11 01 0f 01 12 29 0c 03 81 01 14 29
```

```
00: show_text_block(str[17] "Nestled amongst several bland keys, a / crimson wonder hypnotically reel")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(7)
    # skip -> map_transition(0x03, 0x81)
03: cond = check_gold_at_least(2500)
04: if not cond: skip_tokens(6)
    # skip -> show_text_basic(str[20] "Not enough gold!")
05: add_party_entity(0x00, f3a=0x71, f40=0x00, f46=0x00)
06: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[18] "*** Backpacks Full ***")
07: end_script()
08: show_text_basic(str[18] "*** Backpacks Full ***")
09: set_abort_and_exit()
10: map_transition(0x03, 0x81)
11: show_text_basic(str[20] "Not enough gold!")
12: set_abort_and_exit()
```

**Event 24** — triggers: (7,3)/DIR_N?

```hex
0b 14 00 02 13 0a 11 03 24 10 27 11 02 18 00 74 ef 10 0f 01 14 08 0f
```

```
00: set_service_context(str[20] "Not enough gold!", mode=0x00)
01: show_text_block(str[19] "A silver-tongued illusionist will / allow you to join the prestigious / ")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(3)
    # skip -> end_script()
04: cond = check_gold_at_least(10000)
05: if not cond: skip_tokens(2)
    # skip -> show_text_basic(str[20] "Not enough gold!")
06: apply_party_masked(count=0x00, set=0x74, and=0xEF, or=0x10)
07: end_script()
08: show_text_basic(str[20] "Not enough gold!")
09: wait_key()
10: end_script()
```

**Event 25** — triggers: (12,7)/DIR_N?

```hex
04 15
```

```
00: show_text_above_door(str[21] "Danger Wild Section!")
```

**Event 26** — triggers: (12,10)/DIR_W?

```hex
02 16 19 00 b8 01 00 10 01 01 12 07 14
```

```
00: show_text_block(str[22] "Look, someone stashed a Lava Grenade / in this corner and now it's yours")
01: add_party_entity(0x00, f3a=0xB8, f40=0x01, f46=0x00)
02: if cond: skip_tokens(1)
    # skip -> wait_for_space()
03: show_text_basic(str[18] "*** Backpacks Full ***")
04: wait_for_space()
05: clear_current_tile_event_flag()
```

**Event 27** — triggers: (3,0)/DIR_N?

```hex
0e 3e
```

```
00: exec_selector(0x3E)
```

**Event 28** — triggers: (9,3)/DIR_S?

```hex
0e 3f
```

```
00: exec_selector(0x3F)
```

**Event 29** — triggers: (3,15)/DIR_N?

```hex
0e 40
```

```
00: exec_selector(0x40)
```

**Event 30** — triggers: (9,6)/0x30

```hex
0b 07 00 0e 41
```

```
00: set_service_context(str[7] "Blackrock Mage Guild", mode=0x00)
01: exec_selector(0x41)
```

**Event 31** — triggers: (11,6)/0x90

```hex
0b 07 00 0e 42
```

```
00: set_service_context(str[7] "Blackrock Mage Guild", mode=0x00)
01: exec_selector(0x42)
```

**Event 32** — triggers: (11,8)/DIR_N?+DIR_E?

```hex
0b 07 00 0e 43
```

```
00: set_service_context(str[7] "Blackrock Mage Guild", mode=0x00)
01: exec_selector(0x43)
```

**Event 33** — triggers: (9,8)/0x60

```hex
0b 07 00 0e 44
```

```
00: set_service_context(str[7] "Blackrock Mage Guild", mode=0x00)
01: exec_selector(0x44)
```

**Event 35** — triggers: (12,3)/ANY_DIR

```hex
2b 01 0e 2e 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x2E)
02: clear_current_tile_event_flag()
```

**Event 36** — triggers: (13,3)/ANY_DIR

```hex
2b 01 0e 46 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x46)
02: clear_current_tile_event_flag()
```

**Event 37** — triggers: (14,3)/ANY_DIR

```hex
2b 01 0e 34 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x34)
02: clear_current_tile_event_flag()
```

**Event 38** — triggers: (15,3)/ANY_DIR

```hex
2b 01 0e 47 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x47)
02: clear_current_tile_event_flag()
```

**Event 39** — triggers: (14,9)/ANY_DIR

```hex
2b 01 0e 45 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x45)
02: clear_current_tile_event_flag()
```

**Event 40** — triggers: (14,11)/ANY_DIR

```hex
2b 01 0e 30 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x30)
02: clear_current_tile_event_flag()
```

**Event 41** — triggers: (2,4)/ANY_DIR

```hex
2b 04 15 00 78 04 10 01 0f 12 04 04 04 04 04 04 06 06 06 06 06 02 17 03 00 11 01 14 0e 48
```

```
00: skip_tokens(4)
    # skip -> cond = load_var8(group=0x03, index=0x00)
01: apply_party(count=0x00, op=0x78, val=0x04)
02: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=04 04 04 04 04 04 06 06 06 06, flags=06 02)
03: end_script()
04: encounter_setup(monsters=04 04 04 04 04 04 06 06 06 06, flags=06 02)
05: cond = load_var8(group=0x03, index=0x00)
06: if not cond: skip_tokens(1)
    # skip -> exec_selector(0x48)
07: clear_current_tile_event_flag()
08: exec_selector(0x48)
```

**Event 42** — triggers: (5,10)/DIR_S?

```hex
0e 07
```

```
00: exec_selector(0x07)  # open_general_store
```

## String table

- `[00]` <EMPTY>
- `[01]` Hotel Four
- `[02]` Bestway Blacksmith
- `[03]` Belinthra's Bar
- `[04]` Training Academy
- `[05]` Vulcanian Transport
- `[06]` Vulcanian Export Co.
- `[07]` Blackrock Mage Guild
- `[08]` Lava Locksmith
- `[09]` Vulcan Temple
- `[10]` Profiency Expert
- `[11]` Disembowelments R Us
- `[12]` Sergeant Pain School
- `[13]` A passage leads out of Vulcania. / Exit (y/n)?
- `[14]` The Vulcanian Transport Company offers / immediate transference to Tundara for / 30 gold. "Well, how about it (y/n)?"
- `[15]` Wearing a uniform labeled "Vulcanian / Export" a pointy-eared dust devil / barks, "Fare to Atlantium is 100 gold. / Pay up (y/n)?"
- `[16]` Stairs to Cavern. Descend (y/n)?
- `[17]` Nestled amongst several bland keys, a / crimson wonder hypnotically reels you / in. The locksmith pounces, "The Red / Key goes for 2500 gold. Pay (y/n)?"
- `[18]` *** Backpacks Full ***
- `[19]` A silver-tongued illusionist will / allow you to join the prestigious / local mage guild for the special / price of 10,000 gold. Act now (y/n)?
- `[20]` Not enough gold!
- `[21]` Danger Wild Section!
- `[22]` Look, someone stashed a Lava Grenade / in this corner and now it's yours.
- `[23]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
