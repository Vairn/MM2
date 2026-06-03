# Location 31 — Gemmaker's Cave

- **event.dat** offset `0x00A43D`, length **1374** bytes
- **Map:** map screen **31**; Gemmaker's Cave
- **Record kind:** `standard`
- **Triggers:** 79; **script segments:** 16; **strings:** 18

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,14) | `0x0E` | **6** | ANY_DIR |
| (0,15) | `0x0F` | **6** | ANY_DIR |
| (1,14) | `0x1E` | **6** | ANY_DIR |
| (1,15) | `0x1F` | **6** | ANY_DIR |
| (2,5) | `0x25` | **11** | ENTER |
| (2,11) | `0x2B` | **13** | ENTER |
| (3,0) | `0x30` | **6** | ANY_DIR |
| (3,3) | `0x33` | **1** | ANY_DIR |
| (3,9) | `0x39` | **7** | ENTER |
| (3,10) | `0x3A` | **6** | ANY_DIR |
| (4,5) | `0x45` | **6** | ANY_DIR |
| (4,8) | `0x48` | **4** | ANY_DIR |
| (4,10) | `0x4A` | **4** | ANY_DIR |
| (5,7) | `0x57` | **4** | ANY_DIR |
| (5,8) | `0x58` | **4** | ANY_DIR |
| (5,10) | `0x5A` | **4** | ANY_DIR |
| (5,11) | `0x5B` | **4** | ANY_DIR |
| (6,6) | `0x66` | **4** | ANY_DIR |
| (6,7) | `0x67` | **4** | ANY_DIR |
| (6,8) | `0x68` | **4** | ANY_DIR |
| (6,10) | `0x6A` | **4** | ANY_DIR |
| (6,11) | `0x6B` | **9** | ANY_DIR |
| (6,12) | `0x6C` | **4** | ANY_DIR |
| (6,13) | `0x6D` | **6** | ANY_DIR |
| (7,5) | `0x75` | **4** | ANY_DIR |
| (7,6) | `0x76` | **4** | ANY_DIR |
| (7,7) | `0x77` | **8** | ANY_DIR |
| (7,8) | `0x78` | **4** | ANY_DIR |
| (7,10) | `0x7A` | **4** | ANY_DIR |
| (7,11) | `0x7B` | **4** | ANY_DIR |
| (7,12) | `0x7C` | **4** | ANY_DIR |
| (7,13) | `0x7D` | **4** | ANY_DIR |
| (7,14) | `0x7E` | **14** | ENTER |
| (8,4) | `0x84` | **7** | DIR_SPECIAL |
| (8,5) | `0x85` | **4** | ANY_DIR |
| (8,6) | `0x86` | **4** | ANY_DIR |
| (8,7) | `0x87` | **4** | ANY_DIR |
| (8,8) | `0x88` | **4** | ANY_DIR |
| (8,9) | `0x89` | **3** | ANY_DIR |
| (8,10) | `0x8A` | **4** | ANY_DIR |
| (8,11) | `0x8B` | **4** | ANY_DIR |
| (8,12) | `0x8C` | **4** | ANY_DIR |
| (8,13) | `0x8D` | **4** | ANY_DIR |
| (8,14) | `0x8E` | **7** | ALWAYS |
| (9,1) | `0x91` | **6** | ANY_DIR |
| (9,5) | `0x95` | **4** | ANY_DIR |
| (9,6) | `0x96` | **4** | ANY_DIR |
| (9,7) | `0x97` | **4** | ANY_DIR |
| (9,8) | `0x98` | **4** | ANY_DIR |
| (9,9) | `0x99` | **4** | ANY_DIR |
| (9,10) | `0x9A` | **4** | ANY_DIR |
| (9,11) | `0x9B` | **4** | ANY_DIR |
| (9,12) | `0x9C` | **4** | ANY_DIR |
| (9,13) | `0x9D` | **4** | ANY_DIR |
| (9,14) | `0x9E` | **6** | ANY_DIR |
| (10,6) | `0xA6` | **5** | ANY_DIR |
| (10,7) | `0xA7` | **4** | ANY_DIR |
| (10,8) | `0xA8` | **4** | ANY_DIR |
| (10,9) | `0xA9` | **10** | ANY_DIR |
| (10,10) | `0xAA` | **4** | ANY_DIR |
| (10,11) | `0xAB` | **4** | ANY_DIR |
| (10,12) | `0xAC` | **4** | ANY_DIR |
| (11,4) | `0xB4` | **6** | ANY_DIR |
| (11,7) | `0xB7` | **4** | ANY_DIR |
| (11,8) | `0xB8` | **4** | ANY_DIR |
| (11,9) | `0xB9` | **4** | ANY_DIR |
| (11,10) | `0xBA` | **4** | ANY_DIR |
| (11,11) | `0xBB` | **4** | ANY_DIR |
| (12,7) | `0xC7` | **12** | DIR_N? |
| (12,8) | `0xC8` | **4** | ANY_DIR |
| (12,9) | `0xC9` | **4** | ANY_DIR |
| (12,10) | `0xCA` | **4** | ANY_DIR |
| (12,14) | `0xCE` | **6** | ANY_DIR |
| (13,2) | `0xD2` | **6** | ANY_DIR |
| (13,4) | `0xD4` | **6** | ANY_DIR |
| (13,9) | `0xD9` | **7** | DIR_N? |
| (14,6) | `0xE6` | **6** | ANY_DIR |
| (15,0) | `0xF0` | **2** | 0x90 |
| (15,11) | `0xFB` | **6** | ANY_DIR |

## Events

**Event 01** — triggers: (3,3)/ANY_DIR

```hex
02 01 09 10 01 0f 2e 73 80 1f 00 2f 01 0a 00 00 14
```

```
00: show_text_block(str[1] "A sweaty master gemmaker is willing to / sell you the spell, Enchant Ite")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> set_party_attr(class=0x73, bits=0x80)
03: end_script()
04: set_party_attr(class=0x73, bits=0x80)
05: party_effect(sel=0x00, 2F 01 0A 00 00)
06: clear_current_tile_event_flag()
```

**Event 02** — triggers: (15,0)/0x90

```hex
01 02 09 10 01 0f 0c 21 74
```

```
00: show_text_basic(str[2] "Exit volcano (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> map_transition(0x21, 0x74)
03: end_script()
04: map_transition(0x21, 0x74)
```

**Event 03** — triggers: (8,9)/ANY_DIR

```hex
16 00 32 10 08 02 03 09 10 01 0f 19 01 32 c8 0a 10 02 01 0b 07 14
```

```
00: cond = check_monster_present(0x00, 0x32)
01: if cond: skip_tokens(8)
    # skip -> clear_current_tile_event_flag()
02: show_text_block(str[3] "A Flaming Sword is stuck in the glassy / volcanic rock. Pry it loose (y/")
03: cond = prompt_yes_no()
04: if cond: skip_tokens(1)
    # skip -> add_party_entity(0x01, f3a=0x32, f40=0xC8, f46=0x0A)
05: end_script()
06: add_party_entity(0x01, f3a=0x32, f40=0xC8, f46=0x0A)
07: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
08: show_text_basic(str[11] "*** Backpacks Full ***")
09: wait_for_space()
10: clear_current_tile_event_flag()
```

**Event 04** — triggers: (4,8)/ANY_DIR, (4,10)/ANY_DIR, (5,7)/ANY_DIR, (5,8)/ANY_DIR, (5,10)/ANY_DIR, (5,11)/ANY_DIR, (6,6)/ANY_DIR, (6,7)/ANY_DIR, (6,8)/ANY_DIR, (6,10)/ANY_DIR, (6,12)/ANY_DIR, (7,5)/ANY_DIR, (7,6)/ANY_DIR, (7,8)/ANY_DIR, (7,10)/ANY_DIR, (7,11)/ANY_DIR, (7,12)/ANY_DIR, (7,13)/ANY_DIR, (8,5)/ANY_DIR, (8,6)/ANY_DIR, (8,7)/ANY_DIR, (8,8)/ANY_DIR, (8,10)/ANY_DIR, (8,11)/ANY_DIR, (8,12)/ANY_DIR, (8,13)/ANY_DIR, (9,5)/ANY_DIR, (9,6)/ANY_DIR, (9,7)/ANY_DIR, (9,8)/ANY_DIR, (9,9)/ANY_DIR, (9,10)/ANY_DIR, (9,11)/ANY_DIR, (9,12)/ANY_DIR, (9,13)/ANY_DIR, (10,7)/ANY_DIR, (10,8)/ANY_DIR, (10,10)/ANY_DIR, (10,11)/ANY_DIR, (10,12)/ANY_DIR, (11,7)/ANY_DIR, (11,8)/ANY_DIR, (11,9)/ANY_DIR, (11,10)/ANY_DIR, (11,11)/ANY_DIR, (12,8)/ANY_DIR, (12,9)/ANY_DIR, (12,10)/ANY_DIR

```hex
01 04 17 23 00 10 04 07 0d 09 31 00 32 00 0c 1f e2 02 10 29
```

```
00: show_text_basic(str[4] "Magical Lava! (poof)")
01: cond = load_var8(group=0x23, index=0x00)
02: if cond: skip_tokens(4)
    # skip -> show_text_block(str[16] "/    Your levitation spell saved you!")
03: wait_for_space()
04: engine_call(0x09)
05: for_party(mask=0x00): call(0x32,0x00)
06: map_transition(0x1F, 0xE2)
07: show_text_block(str[16] "/    Your levitation spell saved you!")
08: set_abort_and_exit()
```

**Event 05** — triggers: (10,6)/ANY_DIR

```hex
01 05 07 1f 01 38 02 f4 01 00 0d 09 14
```

```
00: show_text_basic(str[5] "You find a rich deposit of gems!")
01: wait_for_space()
02: party_effect(sel=0x01, 38 02 F4 01 00)
03: engine_call(0x09)
04: clear_current_tile_event_flag()
```

**Event 06** — triggers: (0,14)/ANY_DIR, (0,15)/ANY_DIR, (1,14)/ANY_DIR, (1,15)/ANY_DIR, (3,0)/ANY_DIR, (3,10)/ANY_DIR, (4,5)/ANY_DIR, (6,13)/ANY_DIR, (9,1)/ANY_DIR, (9,14)/ANY_DIR, (11,4)/ANY_DIR, (12,14)/ANY_DIR, (13,2)/ANY_DIR, (13,4)/ANY_DIR, (14,6)/ANY_DIR, (15,11)/ANY_DIR

```hex
01 06 07 1f 01 38 02 05 00 00 0d 09 14
```

```
00: show_text_basic(str[6] "You find gems strewn about the floor")
01: wait_for_space()
02: party_effect(sel=0x01, 38 02 05 00 00)
03: engine_call(0x09)
04: clear_current_tile_event_flag()
```

**Event 07** — triggers: (3,9)/ENTER, (8,4)/DIR_SPECIAL, (8,14)/ALWAYS, (13,9)/DIR_N?

```hex
02 07 29
```

```
00: show_text_block(str[7] "Molten magma fills the large cavern / before you, yet something shines a")
01: set_abort_and_exit()
```

**Event 08** — triggers: (7,7)/ANY_DIR

```hex
02 08 19 01 24 05 04 07 10 02 01 0b 07 14
```

```
00: show_text_block(str[8] "Amongst the lava you find a / Fiery Spear +4.")
01: add_party_entity(0x01, f3a=0x24, f40=0x05, f46=0x04)
02: wait_for_space()
03: if cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
04: show_text_basic(str[11] "*** Backpacks Full ***")
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 09** — triggers: (6,11)/ANY_DIR

```hex
02 09 19 01 52 05 04 10 01 01 0b 07 14
```

```
00: show_text_block(str[9] "You find a Fire Glave +4 amongst the / molten ooze.")
01: add_party_entity(0x01, f3a=0x52, f40=0x05, f46=0x04)
02: if cond: skip_tokens(1)
    # skip -> wait_for_space()
03: show_text_basic(str[11] "*** Backpacks Full ***")
04: wait_for_space()
05: clear_current_tile_event_flag()
```

**Event 10** — triggers: (10,9)/ANY_DIR

```hex
02 0a 19 01 76 05 04 10 01 01 0b 07 14
```

```
00: show_text_block(str[10] "An untarnished Fire Shield is found.")
01: add_party_entity(0x01, f3a=0x76, f40=0x05, f46=0x04)
02: if cond: skip_tokens(1)
    # skip -> wait_for_space()
03: show_text_basic(str[11] "*** Backpacks Full ***")
04: wait_for_space()
05: clear_current_tile_event_flag()
```

**Event 11** — triggers: (2,5)/ENTER

```hex
02 0c 29
```

```
00: show_text_block(str[12] "A small puddle forms a message: "At / 10,10 within a shrine in the Plane")
01: set_abort_and_exit()
```

**Event 12** — triggers: (12,7)/DIR_N?

```hex
02 0d 29
```

```
00: show_text_block(str[13] "Written in mud upon the mossy wall / "The Earth Talon is enshrined at 8,")
01: set_abort_and_exit()
```

**Event 13** — triggers: (2,11)/ENTER

```hex
02 0e 29
```

```
00: show_text_block(str[14] "The wind wails, "Upon a pedestal at / 11,7 in the Elemental Plane of Air")
01: set_abort_and_exit()
```

**Event 14** — triggers: (7,14)/ENTER

```hex
02 0f 29
```

```
00: show_text_block(str[15] "Smoke signals read: "The Fire Talon / rests at 4,4 in the Elemental Plan")
01: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` A sweaty master gemmaker is willing to / sell you the spell, Enchant Item, for / an undisclosed price. Buy it (y/n)?
- `[02]` Exit volcano (y/n)?
- `[03]` A Flaming Sword is stuck in the glassy / volcanic rock. Pry it loose (y/n)?
- `[04]` Magical Lava! (poof)
- `[05]` You find a rich deposit of gems!
- `[06]` You find gems strewn about the floor
- `[07]` Molten magma fills the large cavern / before you, yet something shines at / the center.
- `[08]` Amongst the lava you find a / Fiery Spear +4.
- `[09]` You find a Fire Glave +4 amongst the / molten ooze.
- `[10]` An untarnished Fire Shield is found.
- `[11]` *** Backpacks Full ***
- `[12]` A small puddle forms a message: "At / 10,10 within a shrine in the Plane of / Water rests the Water Talon."
- `[13]` Written in mud upon the mossy wall / "The Earth Talon is enshrined at 8,8 / in the Elemental Plane of Earth."
- `[14]` The wind wails, "Upon a pedestal at / 11,7 in the Elemental Plane of Air / you will find the Air Talon."
- `[15]` Smoke signals read: "The Fire Talon / rests at 4,4 in the Elemental Plane / of Fire."
- `[16]`  /    Your levitation spell saved you!
- `[17]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
