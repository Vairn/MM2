# Location 22 — Corak's Cave

- **event.dat** offset `0x0079C7`, length **1403** bytes
- **Map:** map screen **22**; Corak's Cave
- **Record kind:** `standard`
- **Triggers:** 54; **script segments:** 30; **strings:** 19

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,4) | `0x04` | **27** | ANY_DIR |
| (0,8) | `0x08` | **5** | 0x60 |
| (0,11) | `0x0B` | **27** | ANY_DIR |
| (0,13) | `0x0D` | **6** | DIR_W? |
| (2,3) | `0x23` | **27** | ANY_DIR |
| (2,7) | `0x27` | **8** | DIR_S? |
| (2,8) | `0x28` | **8** | DIR_S? |
| (2,12) | `0x2C` | **27** | ANY_DIR |
| (3,4) | `0x34` | **26** | ANY_DIR |
| (3,8) | `0x38` | **27** | ANY_DIR |
| (3,11) | `0x3B` | **26** | ANY_DIR |
| (3,13) | `0x3D` | **4** | DIR_N?+DIR_E? |
| (5,2) | `0x52` | **21** | ANY_DIR |
| (5,7) | `0x57` | **26** | ANY_DIR |
| (5,8) | `0x58` | **26** | ANY_DIR |
| (5,13) | `0x5D` | **22** | ANY_DIR |
| (6,7) | `0x67` | **3** | DIR_S? |
| (6,8) | `0x68` | **3** | DIR_S? |
| (7,3) | `0x73` | **20** | ANY_DIR |
| (7,7) | `0x77` | **7** | DIR_S? |
| (7,8) | `0x78` | **7** | DIR_S? |
| (7,13) | `0x7D` | **19** | ANY_DIR |
| (8,0) | `0x80` | **25** | ANY_DIR |
| (8,2) | `0x82` | **10** | DIR_S? |
| (8,3) | `0x83` | **10** | DIR_S? |
| (8,6) | `0x86` | **24** | ANY_DIR |
| (8,9) | `0x89` | **25** | ANY_DIR |
| (8,12) | `0x8C` | **10** | DIR_S? |
| (8,13) | `0x8D` | **10** | DIR_S? |
| (8,15) | `0x8F` | **25** | ANY_DIR |
| (9,0) | `0x90` | **24** | ANY_DIR |
| (9,4) | `0x94` | **24** | ANY_DIR |
| (9,7) | `0x97` | **9** | DIR_N? |
| (9,8) | `0x98` | **9** | DIR_N? |
| (9,11) | `0x9B` | **25** | ANY_DIR |
| (9,15) | `0x9F` | **24** | ANY_DIR |
| (10,1) | `0xA1` | **14** | ANY_DIR |
| (10,6) | `0xA6` | **18** | ANY_DIR |
| (10,9) | `0xA9` | **18** | ANY_DIR |
| (10,14) | `0xAE` | **11** | ANY_DIR |
| (11,5) | `0xB5` | **23** | ANY_DIR |
| (11,7) | `0xB7` | **1** | 0x30 |
| (11,8) | `0xB8` | **28** | DIR_E? |
| (11,10) | `0xBA` | **23** | ANY_DIR |
| (12,1) | `0xC1` | **15** | ANY_DIR |
| (12,14) | `0xCE` | **12** | ANY_DIR |
| (13,5) | `0xD5` | **17** | ANY_DIR |
| (13,7) | `0xD7` | **2** | DIR_S? |
| (13,8) | `0xD8` | **2** | DIR_S? |
| (13,10) | `0xDA` | **17** | ANY_DIR |
| (14,1) | `0xE1` | **16** | ANY_DIR |
| (14,5) | `0xE5` | **9** | DIR_S? |
| (14,10) | `0xEA` | **9** | DIR_S? |
| (14,14) | `0xEE` | **13** | ANY_DIR |

## Events

**Event 01** — triggers: (11,7)/0x30

```hex
02 01 2e 6f 10 07 14
```

```
00: show_text_block(str[1] "An ingenious sorcerer named Lloyd / tells the tale of his great inventio")
01: set_party_attr(class=0x6F, bits=0x10)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 02** — triggers: (13,7)/DIR_S?, (13,8)/DIR_S?

```hex
02 02 04 03 09 10 01 0f 0c 0b b5
```

```
00: show_text_block(str[2] "A sign above the exit reads, / "Only way out." Take it (y/n)?")
01: show_text_above_door(str[3] "Only Way Out")
02: cond = prompt_yes_no()
03: if cond: skip_tokens(1)
    # skip -> map_transition(0x0B, 0xB5)
04: end_script()
05: map_transition(0x0B, 0xB5)
```

**Event 03** — triggers: (6,7)/DIR_S?, (6,8)/DIR_S?

```hex
28 01 c1 11 02 01 04 29 02 05 07 0d 09 0c 16 77
```

```
00: cond = consume_item(item_id=193, name="Admit 8 Pass", probe=1)
01: if not cond: skip_tokens(2)
    # skip -> show_text_block(str[5] "Ticket takers at the door say, "I / can't let you in if you don't have a")
02: show_text_basic(str[4] "Your Admit 8 Pass is taken at the door")
03: set_abort_and_exit()
04: show_text_block(str[5] "Ticket takers at the door say, "I / can't let you in if you don't have a")
05: wait_for_space()
06: engine_call(0x09)
07: map_transition(0x16, 0x77)
```

**Event 04** — triggers: (3,13)/DIR_N?+DIR_E?

```hex
2d 03 05 10 02 02 06 29 16 01 e5 10 02 02 07 29 02 08 21 37 00 02 21 38 00 02 21 27 08 22 21 28 08 22 07 14
```

```
00: cond = check_member_attr(fields=0x03, value=0x05)
01: if cond: skip_tokens(2)
    # skip -> cond = check_monster_present(0x01, 0xE5)
02: show_text_block(str[6] ""I cannot help anyone but Clerics / and their Robber assistants."")
03: set_abort_and_exit()
04: cond = check_monster_present(0x01, 0xE5)
05: if cond: skip_tokens(2)
    # skip -> show_text_block(str[8] ""Ah, I have been waiting for you. / Proceed to the Hero's Tomb. I have /")
06: show_text_block(str[7] ""I cannot help you until you / bring me Corak's Soul!"")
07: set_abort_and_exit()
08: show_text_block(str[8] ""Ah, I have been waiting for you. / Proceed to the Hero's Tomb. I have /")
09: set_tile((y=3,x=7), 0x00, 0x02)
10: set_tile((y=3,x=8), 0x00, 0x02)
11: set_tile((y=2,x=7), 0x08, 0x22)
12: set_tile((y=2,x=8), 0x08, 0x22)
13: wait_for_space()
14: clear_current_tile_event_flag()
```

**Event 05** — triggers: (0,8)/0x60

```hex
2d 03 05 10 04 02 09 07 0d 09 0c 16 b5 2b 02 02 0a 12 ba ba ba ba ba ba ba ba 00 00 00 00 28 01 e5 10 01 14 0e 65
```

```
00: cond = check_member_attr(fields=0x03, value=0x05)
01: if cond: skip_tokens(4)
02: show_text_block(str[9] "You catch a glimpse of Corak's coffin / and then -")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x16, 0xB5)
06: skip_tokens(2)
    # skip -> cond = consume_item(item_id=229, name="Corak's Soul", probe=1)
07: show_text_block(str[10] "You approach Corak's coffin. / Suddenly you are attacked.")
08: encounter_setup(monsters=BA BA BA BA BA BA BA BA 00 00, flags=00 00)
09: cond = consume_item(item_id=229, name="Corak's Soul", probe=1)
10: if cond: skip_tokens(1)
    # skip -> exec_selector(0x65)
11: clear_current_tile_event_flag()
12: exec_selector(0x65)
```

**Event 06** — triggers: (0,13)/DIR_W?

```hex
05 0d
```

```
00: show_text_popup_style_a(str[13] "Holy Word -H. Gibson / Look south on a tree / in Lost Soul's Woods / it ")
```

**Event 07** — triggers: (7,7)/DIR_S?, (7,8)/DIR_S?

```hex
04 0e
```

```
00: show_text_above_door(str[14] "Corak's Crypt")
```

**Event 08** — triggers: (2,7)/DIR_S?, (2,8)/DIR_S?

```hex
04 0f
```

```
00: show_text_above_door(str[15] "Hero's Tomb")
```

**Event 09** — triggers: (9,7)/DIR_N?, (9,8)/DIR_N?, (14,5)/DIR_S?, (14,10)/DIR_S?

```hex
04 10
```

```
00: show_text_above_door(str[16] "Zombie Sanctuary")
```

**Event 10** — triggers: (8,2)/DIR_S?, (8,3)/DIR_S?, (8,12)/DIR_S?, (8,13)/DIR_S?

```hex
04 11
```

```
00: show_text_above_door(str[17] "Sarcophagus Storage")
```

**Event 11** — triggers: (10,14)/ANY_DIR

```hex
2b 01 12 0c 0c 0c 0c 0c 0c 0c 0c 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=0C 0C 0C 0C 0C 0C 0C 0C 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (12,14)/ANY_DIR

```hex
2b 01 12 1d 1d 1d 1d 1d 1d 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1D 1D 1D 1D 1D 1D 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (14,14)/ANY_DIR

```hex
2b 01 12 0c 0c 1d 1d 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=0C 0C 1D 1D 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (10,1)/ANY_DIR

```hex
2b 01 12 2b 2b 2b 2b 0c 0c 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2B 2B 2B 2B 0C 0C 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (12,1)/ANY_DIR

```hex
2b 01 12 3b 3b 3b 3b 3b 3b 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=3B 3B 3B 3B 3B 3B 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (14,1)/ANY_DIR

```hex
2b 01 12 3c 3c 3c 3c 1d 1d 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=3C 3C 3C 3C 1D 1D 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (13,5)/ANY_DIR, (13,10)/ANY_DIR

```hex
2b 01 12 0b 0b 0b 0b 0a 0a 0a 0a 0a 0a 0a 02 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=0B 0B 0B 0B 0A 0A 0A 0A 0A 0A, flags=0A 02)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (10,6)/ANY_DIR, (10,9)/ANY_DIR

```hex
2b 01 12 1a 1a 1a 1a 1a 1a 1a 1b 1b 1b 1b 01 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1A 1A 1A 1A 1A 1A 1A 1B 1B 1B, flags=1B 01)
02: clear_current_tile_event_flag()
```

**Event 19** — triggers: (7,13)/ANY_DIR

```hex
2b 01 12 2a 2a 2a 2a 2a 2a 0a 0a 0a 0a 0a 10 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2A 2A 2A 2A 2A 2A 0A 0A 0A 0A, flags=0A 10)
02: clear_current_tile_event_flag()
```

**Event 20** — triggers: (7,3)/ANY_DIR

```hex
2b 01 12 2d 2d 2d 2d 2d 2d 0a 0a 0a 0a 0a 10 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2D 2D 2D 2D 2D 2D 0A 0A 0A 0A, flags=0A 10)
02: clear_current_tile_event_flag()
```

**Event 21** — triggers: (5,2)/ANY_DIR

```hex
2b 01 12 3a 3a 3a 3a 3a 3a 3a 3a 3a 3a 3a 05 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=3A 3A 3A 3A 3A 3A 3A 3A 3A 3A, flags=3A 05)
02: clear_current_tile_event_flag()
```

**Event 22** — triggers: (5,13)/ANY_DIR

```hex
2b 01 12 3b 3b 3b 3b 3b 3b 3b 3b 3b 3b 3b 05 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=3B 3B 3B 3B 3B 3B 3B 3B 3B 3B, flags=3B 05)
02: clear_current_tile_event_flag()
```

**Event 23** — triggers: (11,5)/ANY_DIR, (11,10)/ANY_DIR

```hex
2b 01 12 4c 4c 4c 4c 4c 4c 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=4C 4C 4C 4C 4C 4C 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 24** — triggers: (8,6)/ANY_DIR, (9,0)/ANY_DIR, (9,4)/ANY_DIR, (9,15)/ANY_DIR

```hex
2b 01 12 1a 1a 1a 1a 1a 1a 1a 1a 0a 0a 0a 06 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1A 1A 1A 1A 1A 1A 1A 1A 0A 0A, flags=0A 06)
02: clear_current_tile_event_flag()
```

**Event 25** — triggers: (8,0)/ANY_DIR, (8,9)/ANY_DIR, (8,15)/ANY_DIR, (9,11)/ANY_DIR

```hex
2b 01 12 0a 0a 0a 0a 0a 0a 0a 0a 1a 1a 1a 06 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=0A 0A 0A 0A 0A 0A 0A 0A 1A 1A, flags=1A 06)
02: clear_current_tile_event_flag()
```

**Event 26** — triggers: (3,4)/ANY_DIR, (3,11)/ANY_DIR, (5,7)/ANY_DIR, (5,8)/ANY_DIR

```hex
2b 01 12 5d 5d 5d 5d 5c 5c 5c 5c 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=5D 5D 5D 5D 5C 5C 5C 5C 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 27** — triggers: (0,4)/ANY_DIR, (0,11)/ANY_DIR, (2,3)/ANY_DIR, (2,12)/ANY_DIR, (3,8)/ANY_DIR

```hex
2b 01 12 7a 7a 7a 79 79 79 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=7A 7A 7A 79 79 79 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 28** — triggers: (11,8)/DIR_E?

```hex
0e 7a
```

```
00: exec_selector(0x7A)
```

## String table

- `[00]` <EMPTY>
- `[01]` An ingenious sorcerer named Lloyd / tells the tale of his great invention, / a Beacon Spell, "It's been real useful / for me, now it will aid you as well."
- `[02]` A sign above the exit reads, / "Only way out." Take it (y/n)?
- `[03]` Only Way Out
- `[04]` Your Admit 8 Pass is taken at the door
- `[05]` Ticket takers at the door say, "I / can't let you in if you don't have an / Admit 8 Pass."
- `[06]` "I cannot help anyone but Clerics / and their Robber assistants."
- `[07]` "I cannot help you until you / bring me Corak's Soul!"
- `[08]` "Ah, I have been waiting for you. / Proceed to the Hero's Tomb. I have / lowered the barriers for you."
- `[09]` You catch a glimpse of Corak's coffin / and then -
- `[10]` You approach Corak's coffin. / Suddenly you are attacked.
- `[11]` A
- `[12]` A
- `[13]` Holy Word -H. Gibson / Look south on a tree / in Lost Soul's Woods / it will be.
- `[14]` Corak's Crypt
- `[15]` Hero's Tomb
- `[16]` Zombie Sanctuary
- `[17]` Sarcophagus Storage
- `[18]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
