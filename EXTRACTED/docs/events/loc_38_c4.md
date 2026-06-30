# Location 38 — C4

- **event.dat** offset `0x00C4AF`, length **1502** bytes
- **Map:** map screen **38**; overland sector **C4**
- **Record kind:** `standard`
- **Triggers:** 44; **script segments:** 18; **strings:** 15

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,9) | `0x09` | **5** | ANY_DIR |
| (0,15) | `0x0F` | **11** | ANY_DIR |
| (1,5) | `0x15` | **4** | ANY_DIR |
| (1,7) | `0x17` | **10** | ANY_DIR |
| (1,12) | `0x1C` | **5** | ANY_DIR |
| (2,5) | `0x25` | **6** | ANY_DIR |
| (2,8) | `0x28` | **10** | ANY_DIR |
| (2,11) | `0x2B` | **10** | ANY_DIR |
| (3,4) | `0x34` | **15** | ANY_DIR |
| (3,9) | `0x39` | **8** | ANY_DIR |
| (3,12) | `0x3C` | **10** | ANY_DIR |
| (3,13) | `0x3D` | **10** | ANY_DIR |
| (3,14) | `0x3E` | **14** | ANY_DIR |
| (4,7) | `0x47` | **4** | ANY_DIR |
| (4,9) | `0x49` | **5** | ANY_DIR |
| (5,4) | `0x54` | **6** | ANY_DIR |
| (5,9) | `0x59` | **10** | ANY_DIR |
| (5,13) | `0x5D` | **5** | ANY_DIR |
| (5,14) | `0x5E` | **13** | ANY_DIR |
| (5,15) | `0x5F` | **9** | ANY_DIR |
| (6,7) | `0x67` | **7** | ANY_DIR |
| (6,10) | `0x6A` | **10** | ANY_DIR |
| (7,5) | `0x75` | **6** | ANY_DIR |
| (7,7) | `0x77` | **4** | ANY_DIR |
| (7,11) | `0x7B` | **8** | ANY_DIR |
| (7,14) | `0x7E` | **10** | ANY_DIR |
| (8,5) | `0x85` | **14** | ANY_DIR |
| (8,9) | `0x89` | **5** | ANY_DIR |
| (8,12) | `0x8C` | **10** | ANY_DIR |
| (8,13) | `0x8D` | **10** | ANY_DIR |
| (8,14) | `0x8E` | **1** | ANY_DIR |
| (9,1) | `0x91` | **3** | ANY_DIR |
| (9,2) | `0x92` | **15** | ANY_DIR |
| (9,6) | `0x96` | **6** | ANY_DIR |
| (9,8) | `0x98` | **7** | ANY_DIR |
| (10,10) | `0xAA` | **4** | ANY_DIR |
| (10,15) | `0xAF` | **9** | ANY_DIR |
| (11,1) | `0xB1` | **14** | ANY_DIR |
| (11,11) | `0xBB` | **7** | ANY_DIR |
| (11,14) | `0xBE` | **4** | ANY_DIR |
| (12,10) | `0xCA` | **15** | ANY_DIR |
| (12,13) | `0xCD` | **8** | ANY_DIR |
| (14,9) | `0xE9` | **2** | DIR_E? |
| (15,0) | `0xF0` | **12** | ANY_DIR |

## Events

**Event 01** — triggers: (8,14)/ANY_DIR

```hex
15 00 78 10 10 02 02 01 29 02 02 29
```

```
00: apply_party(count=0x00, op=0x78, val=0x10)
01: if cond: skip_tokens(2)
    # skip -> show_text_block(str[2] "A tongueless toad hops helplessly / before you as an eerie sound emanate")
02: show_text_block(str[1] "The tongue of a fat toad lashes / rapidly as it croaks.")
03: set_abort_and_exit()
04: show_text_block(str[2] "A tongueless toad hops helplessly / before you as an eerie sound emanate")
05: set_abort_and_exit()
```

**Event 02** — triggers: (14,9)/DIR_E?

```hex
02 03 09 11 04 18 00 3a 00 96 18 00 3b 00 00 18 00 20 00 96 18 00 21 00 00 0f
```

```
00: show_text_block(str[3] "A small pond looks inviting. / Swim (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(4)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x3A, and=0x00, or=0x96)
04: apply_party_masked(count=0x00, set=0x3B, and=0x00, or=0x00)
05: apply_party_masked(count=0x00, set=0x20, and=0x00, or=0x96)
06: apply_party_masked(count=0x00, set=0x21, and=0x00, or=0x00)
07: end_script()
```

**Event 03** — triggers: (9,1)/ANY_DIR

```hex
2b 04 02 04 09 11 02 12 df 3f 3f 3f 3f 3f 3f 3f 3f 3f 3f e7 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[4] "You stumble across The Door to Hell! / Open (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=DF 3F 3F 3F 3F 3F 3F 3F 3F 3F, flags=3F E7)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 04** — triggers: (1,5)/ANY_DIR, (4,7)/ANY_DIR, (7,7)/ANY_DIR, (10,10)/ANY_DIR, (11,14)/ANY_DIR

```hex
01 05 07 0d 09 0c 28 46
```

```
00: show_text_basic(str[5] "A sinkhole pulls you under!")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x28, 0x46)
```

**Event 05** — triggers: (0,9)/ANY_DIR, (1,12)/ANY_DIR, (4,9)/ANY_DIR, (5,13)/ANY_DIR, (8,9)/ANY_DIR

```hex
02 06 0d 09 1c 08 18 09 43 00 81 29
```

```
00: show_text_block(str[6] "Quicksand!")
01: engine_call(0x09)
02: op_1c_engine_query_to_cond(0x08)
03: apply_party_masked(count=0x09, set=0x43, and=0x00, or=0x81)
04: set_abort_and_exit()
```

**Event 06** — triggers: (2,5)/ANY_DIR, (5,4)/ANY_DIR, (7,5)/ANY_DIR, (9,6)/ANY_DIR

```hex
2b 01 12 13 13 13 13 13 13 13 13 13 13 13 0a 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=13 13 13 13 13 13 13 13 13 13, flags=13 0A)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (6,7)/ANY_DIR, (9,8)/ANY_DIR, (11,11)/ANY_DIR

```hex
2b 01 12 13 13 13 13 13 35 35 35 35 35 35 05 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=13 13 13 13 13 35 35 35 35 35, flags=35 05)
02: clear_current_tile_event_flag()
```

**Event 08** — triggers: (3,9)/ANY_DIR, (7,11)/ANY_DIR, (12,13)/ANY_DIR

```hex
2b 01 12 6c 6c 6c 6c 6c 6c 6c 6c 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6C 6C 6C 6C 6C 6C 6C 6C 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (5,15)/ANY_DIR, (10,15)/ANY_DIR

```hex
2b 01 12 66 66 66 66 66 66 66 66 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=66 66 66 66 66 66 66 66 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (1,7)/ANY_DIR, (2,8)/ANY_DIR, (2,11)/ANY_DIR, (3,12)/ANY_DIR, (3,13)/ANY_DIR, (5,9)/ANY_DIR, (6,10)/ANY_DIR, (7,14)/ANY_DIR, (8,12)/ANY_DIR, (8,13)/ANY_DIR

```hex
2b 01 12 80 80 80 80 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=80 80 80 80 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 11** — triggers: (0,15)/ANY_DIR

```hex
2b 01 12 b1 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=B1 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (15,0)/ANY_DIR

```hex
2b 07 2d 07 05 10 03 02 07 07 14 02 08 12 ed 9b 9b 00 00 00 00 00 00 00 00 00 18 00 75 fe 01 02 0c 07 14
```

```
00: skip_tokens(7)
    # skip -> apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
01: cond = check_member_attr(fields=0x07, value=0x05)
02: if cond: skip_tokens(3)
    # skip -> show_text_block(str[8] "Bruno, Chief of the Barbaric Hills, / challenges your Barbarian to a tes")
03: show_text_block(str[7] "Only the barbaric may speak / to Brutal Bruno.")
04: wait_for_space()
05: clear_current_tile_event_flag()
06: show_text_block(str[8] "Bruno, Chief of the Barbaric Hills, / challenges your Barbarian to a tes")
07: encounter_setup(monsters=ED 9B 9B 00 00 00 00 00 00 00, flags=00 00)
08: apply_party_masked(count=0x00, set=0x75, and=0xFE, or=0x01)
09: show_text_block(str[12] "Victory! Now return to the Jurors.")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 13** — triggers: (5,14)/ANY_DIR

```hex
22 08 08 11 1c 2b 1b 16 01 db 11 15 16 01 dc 11 13 16 01 dd 11 11 16 01 de 11 0f 16 01 df 11 0d 02 09 09 11 0a 28 01 db 28 01 dc 28 01 dd 28 01 de 28 01 df 0b 12 00 03 0a 18 00 7f fd 02 08 14 02 0b 07 02 0d 12 fa 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: cond = (era in [8..8])
01: if not cond: skip_tokens(28)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(27)
    # skip -> clear_current_tile_event_flag()
03: cond = check_monster_present(0x01, 0xDB)
04: if not cond: skip_tokens(21)
    # skip -> show_text_block(str[11] "You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave")
05: cond = check_monster_present(0x01, 0xDC)
06: if not cond: skip_tokens(19)
    # skip -> show_text_block(str[11] "You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave")
07: cond = check_monster_present(0x01, 0xDD)
08: if not cond: skip_tokens(17)
    # skip -> show_text_block(str[11] "You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave")
09: cond = check_monster_present(0x01, 0xDE)
10: if not cond: skip_tokens(15)
    # skip -> show_text_block(str[11] "You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave")
11: cond = check_monster_present(0x01, 0xDF)
12: if not cond: skip_tokens(13)
    # skip -> show_text_block(str[11] "You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave")
13: show_text_block(str[9] "King Kalohn eyes your party / appreciatively. He wonders if you / would ")
14: cond = prompt_yes_no()
15: if not cond: skip_tokens(10)
    # skip -> show_text_block(str[11] "You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave")
16: cond = consume_item(item_id=219, name="Water Talon", probe=1)
17: cond = consume_item(item_id=220, name="Air Talon", probe=1)
18: cond = consume_item(item_id=221, name="Fire Talon", probe=1)
19: cond = consume_item(item_id=222, name="Earth Talon", probe=1)
20: cond = consume_item(item_id=223, name="Element Orb", probe=1)
21: service_sign(idx=0x12 -> sign 52 [52.anm], pos=0x00)
22: show_text(str[10] "King Kalohn the Conjurer thanks you / greatly. He then engages in an epi")
23: apply_party_masked(count=0x00, set=0x7F, and=0xFD, or=0x02)
24: wait_key()
25: clear_current_tile_event_flag()
26: show_text_block(str[11] "You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave")
27: wait_for_space()
28: show_text_block(str[13] "If only you'd had the Elemental Orb / and Talons...")
29: encounter_setup(monsters=FA 00 00 00 00 00 00 00 00 00, flags=00 00)
30: clear_current_tile_event_flag()
```

**Event 14** — triggers: (3,14)/ANY_DIR, (8,5)/ANY_DIR, (11,1)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 e0 e1 e1 c1 c1 c1 c1 c2 c2 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=E0 E1 E1 C1 C1 C1 C1 C2 C2 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 15** — triggers: (3,4)/ANY_DIR, (9,2)/ANY_DIR, (12,10)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 e1 c1 c1 c2 c2 c2 c2 ce ce 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=E1 C1 C1 C2 C2 C2 C2 CE CE 00)
04: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` The tongue of a fat toad lashes / rapidly as it croaks.
- `[02]` A tongueless toad hops helplessly / before you as an eerie sound emanates / from your stomach. "The Jurors are on / Mt. Farview."
- `[03]` A small pond looks inviting. / Swim (y/n)?
- `[04]` You stumble across The Door to Hell! / Open (y/n)?
- `[05]` A sinkhole pulls you under!
- `[06]` Quicksand!
- `[07]` Only the barbaric may speak / to Brutal Bruno.
- `[08]` Bruno, Chief of the Barbaric Hills, / challenges your Barbarian to a test of / brute force. To the death.
- `[09]` King Kalohn eyes your party / appreciatively. He wonders if you / would give him the Element Orb and / the four Elemental Talons (y/n)?
- `[10]` King Kalohn the Conjurer thanks you / greatly. He then engages in an epic / battle with the Mega Dragon. History / has changed and he wins. Return / to the tenth century for further / instructions at Luxus Palace Royale.
- `[11]` You watch King Kalohn die as he / engages the mighty Mega Dragon. A wave / of water drowns the land as the Mega / Dragon approaches you.
- `[12]` Victory! Now return to the Jurors.
- `[13]` If only you'd had the Elemental Orb / and Talons...
- `[14]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
