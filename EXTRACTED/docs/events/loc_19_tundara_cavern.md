# Location 19 — Tundara Cavern

- **event.dat** offset `0x00698A`, length **1329** bytes
- **Map:** map screen **19**; Tundara Cavern
- **Record kind:** `standard`
- **Triggers:** 30; **script segments:** 24; **strings:** 22

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,12) | `0x0C` | **5** | ANY_DIR |
| (0,13) | `0x0D` | **5** | ANY_DIR |
| (0,14) | `0x0E` | **5** | ANY_DIR |
| (0,15) | `0x0F` | **5** | ANY_DIR |
| (1,12) | `0x1C` | **13** | ENTER |
| (1,14) | `0x1E` | **1** | 0x90 |
| (1,15) | `0x1F` | **5** | ANY_DIR |
| (2,8) | `0x28` | **8** | 0x30 |
| (2,10) | `0x2A` | **15** | ALWAYS |
| (2,15) | `0x2F` | **5** | ANY_DIR |
| (3,11) | `0x3B` | **11** | DIR_N? |
| (3,13) | `0x3D` | **7** | DIR_SPECIAL |
| (3,15) | `0x3F` | **5** | ANY_DIR |
| (4,5) | `0x45` | **16** | ENTER |
| (4,10) | `0x4A` | **10** | 0x90 |
| (5,2) | `0x52` | **20** | ENTER |
| (6,12) | `0x6C` | **13** | DIR_SPECIAL |
| (6,13) | `0x6D` | **6** | DIR_SPECIAL |
| (8,7) | `0x87` | **19** | DIR_SPECIAL |
| (8,11) | `0x8B` | **18** | DIR_N? |
| (10,13) | `0xAD` | **17** | DIR_N? |
| (11,3) | `0xB3` | **21** | DIR_N? |
| (12,5) | `0xC5` | **13** | DIR_SPECIAL |
| (12,6) | `0xC6` | **2** | DIR_SPECIAL |
| (12,7) | `0xC7` | **3** | ANY_DIR |
| (12,13) | `0xCD` | **9** | ENTER |
| (13,3) | `0xD3` | **22** | 0x30 |
| (13,10) | `0xDA` | **4** | DIR_N? |
| (14,1) | `0xE1` | **14** | ANY_DIR |
| (14,10) | `0xEA` | **12** | DIR_N? |

## Events

**Event 01** — triggers: (1,14)/0x90

```hex
01 01 09 11 02 1a 33 00 0c 02 67 0f
```

```
00: show_text_basic(str[1] "Go up to Tundara (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: store_var8(group=0x33, value=0x00)
04: map_transition(0x02, 0x67)
05: end_script()
```

**Event 02** — triggers: (12,6)/DIR_SPECIAL

```hex
02 02 29
```

```
00: show_text_block(str[2] "Hundreds of frozen cadavers / fill the room.")
01: set_abort_and_exit()
```

**Event 03** — triggers: (12,7)/ANY_DIR

```hex
2b 03 17 33 00 11 02 12 2a 2a 2a 2a 2a 2a 2a 2a 2a 2a 2a be 14 0f
```

```
00: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
01: cond = load_var8(group=0x33, index=0x00)
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: encounter_setup(monsters=2A 2A 2A 2A 2A 2A 2A 2A 2A 2A, flags=2A BE)
04: clear_current_tile_event_flag()
05: end_script()
```

**Event 04** — triggers: (13,10)/DIR_N?

```hex
01 03 09 11 01 1a 33 01 0f
```

```
00: show_text_basic(str[3] "You see a red lever. Pull it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: store_var8(group=0x33, value=0x01)
04: end_script()
```

**Event 05** — triggers: (0,12)/ANY_DIR, (0,13)/ANY_DIR, (0,14)/ANY_DIR, (0,15)/ANY_DIR, (1,15)/ANY_DIR, (2,15)/ANY_DIR, (3,15)/ANY_DIR

```hex
2b 04 17 33 00 11 01 12 2a 2a 2a 2a 2a 2a 2a 2a 2a 2a 00 00 0f 14
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: cond = load_var8(group=0x33, index=0x00)
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: encounter_setup(monsters=2A 2A 2A 2A 2A 2A 2A 2A 2A 2A, flags=00 00)
04: end_script()
05: clear_current_tile_event_flag()
```

**Event 06** — triggers: (6,13)/DIR_SPECIAL

```hex
02 04 09 11 04 19 00 0e 00 00 10 02 01 0d 07 0f
```

```
00: show_text_block(str[4] "You enter a room full of maces. / Take one (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(4)
    # skip -> end_script()
03: add_party_entity(0x00, f3a=0x0E, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> end_script()
05: show_text_basic(str[13] "*** Backpacks Full ***")
06: wait_for_space()
07: end_script()
```

**Event 07** — triggers: (3,13)/DIR_SPECIAL

```hex
02 05 09 11 04 19 00 a1 05 00 10 02 01 0d 07 0f
```

```
00: show_text_block(str[5] "You enter a room full of torches. / Take one (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(4)
    # skip -> end_script()
03: add_party_entity(0x00, f3a=0xA1, f40=0x05, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> end_script()
05: show_text_basic(str[13] "*** Backpacks Full ***")
06: wait_for_space()
07: end_script()
```

**Event 08** — triggers: (2,8)/0x30

```hex
02 06 09 11 04 19 00 74 00 00 10 02 01 0d 07 0f
```

```
00: show_text_block(str[6] "You enter a room full of Large / Shields. Take one (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(4)
    # skip -> end_script()
03: add_party_entity(0x00, f3a=0x74, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> end_script()
05: show_text_basic(str[13] "*** Backpacks Full ***")
06: wait_for_space()
07: end_script()
```

**Event 09** — triggers: (12,13)/ENTER

```hex
02 07 09 11 04 19 00 a0 0a 00 10 02 01 0d 07 0f
```

```
00: show_text_block(str[7] "You enter a room full of Magic Herbs. / Take one (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(4)
    # skip -> end_script()
03: add_party_entity(0x00, f3a=0xA0, f40=0x0A, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> end_script()
05: show_text_basic(str[13] "*** Backpacks Full ***")
06: wait_for_space()
07: end_script()
```

**Event 10** — triggers: (4,10)/0x90

```hex
02 08 09 11 04 19 00 9b 00 00 10 02 01 0d 07 0f
```

```
00: show_text_block(str[8] "You enter a room full of Helms. / Take one (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(4)
    # skip -> end_script()
03: add_party_entity(0x00, f3a=0x9B, f40=0x00, f46=0x00)
04: if cond: skip_tokens(2)
    # skip -> end_script()
05: show_text_basic(str[13] "*** Backpacks Full ***")
06: wait_for_space()
07: end_script()
```

**Event 11** — triggers: (3,11)/DIR_N?

```hex
04 09
```

```
00: show_text_above_door(str[9] "Keep Out!")
```

**Event 12** — triggers: (14,10)/DIR_N?

```hex
04 0a
```

```
00: show_text_above_door(str[10] "Controls")
```

**Event 13** — triggers: (1,12)/ENTER, (6,12)/DIR_SPECIAL, (12,5)/DIR_SPECIAL

```hex
04 0b
```

```
00: show_text_above_door(str[11] "Storage")
```

**Event 14** — triggers: (14,1)/ANY_DIR

```hex
2b 01 12 62 62 62 62 62 62 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=62 62 62 62 62 62 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (2,10)/ALWAYS

```hex
02 0c 09 11 01 0e 7e 0f
```

```
00: show_text_block(str[12] "Set the magic location (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: exec_selector(0x7E)  # special_126
04: end_script()
```

**Event 16** — triggers: (4,5)/ENTER

```hex
02 0e 29
```

```
00: show_text_block(str[14] "A group of natives who reside in / Native's Cove at 8,1 oftentimes go / ")
01: set_abort_and_exit()
```

**Event 17** — triggers: (10,13)/DIR_N?

```hex
02 0f 29
```

```
00: show_text_block(str[15] "Fire Encasement is hidden in the / Elemental Plane of Fire at 14,14.")
01: set_abort_and_exit()
```

**Event 18** — triggers: (8,11)/DIR_N?

```hex
02 10 29
```

```
00: show_text_block(str[16] "The Master Gemmaker, a recluse by / nature, teaches Enchant Item for a /")
01: set_abort_and_exit()
```

**Event 19** — triggers: (8,7)/DIR_SPECIAL

```hex
02 11 29
```

```
00: show_text_block(str[17] "A message blasted into the rock reads: / To find the weird warrior Spaz ")
01: set_abort_and_exit()
```

**Event 20** — triggers: (5,2)/ENTER

```hex
05 12
```

```
00: show_text_popup_style_a(str[18] "The four discs are / needed to get the / corresponding talons")
```

**Event 21** — triggers: (11,3)/DIR_N?

```hex
02 13 29
```

```
00: show_text_block(str[19] "Burnt into the ceiling are the words: / "Castle Xabran-6,14-Fire Disc"")
01: set_abort_and_exit()
```

**Event 22** — triggers: (13,3)/0x30

```hex
02 14 29
```

```
00: show_text_block(str[20] "The mystifying M-27 Radicon rests at / 2,11 in Castle Woodhaven.")
01: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` Go up to Tundara (y/n)?
- `[02]` Hundreds of frozen cadavers / fill the room.
- `[03]` You see a red lever. Pull it (y/n)?
- `[04]` You enter a room full of maces. / Take one (y/n)?
- `[05]` You enter a room full of torches. / Take one (y/n)?
- `[06]` You enter a room full of Large / Shields. Take one (y/n)?
- `[07]` You enter a room full of Magic Herbs. / Take one (y/n)?
- `[08]` You enter a room full of Helms. / Take one (y/n)?
- `[09]` Keep Out!
- `[10]` Controls
- `[11]` Storage
- `[12]` Set the magic location (y/n)?
- `[13]` *** Backpacks Full ***
- `[14]` A group of natives who reside in / Native's Cove at 8,1 oftentimes go / into a Frenzy.
- `[15]` Fire Encasement is hidden in the / Elemental Plane of Fire at 14,14.
- `[16]` The Master Gemmaker, a recluse by / nature, teaches Enchant Item for a / time worn fee.
- `[17]` A message blasted into the rock reads: / To find the weird warrior Spaz Twit, / phaser armed ancestor of Lord Haart, / travel to the 7th century A1 11,3.
- `[18]` The four discs are / needed to get the / corresponding talons
- `[19]` Burnt into the ceiling are the words: / "Castle Xabran-6,14-Fire Disc"
- `[20]` The mystifying M-27 Radicon rests at / 2,11 in Castle Woodhaven.
- `[21]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
