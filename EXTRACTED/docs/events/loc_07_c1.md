# Location 07 — C1

- **event.dat** offset `0x002829`, length **1497** bytes
- **Map:** map screen **7**; overland sector **C1**
- **Record kind:** `standard`
- **Triggers:** 33; **script segments:** 20; **strings:** 18

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,1) | `0x11` | **3** | ANY_DIR |
| (1,8) | `0x18` | **14** | ANY_DIR |
| (3,3) | `0x33` | **11** | ENTER |
| (3,6) | `0x36` | **15** | ANY_DIR |
| (3,14) | `0x3E` | **10** | ENTER |
| (4,5) | `0x45` | **14** | ANY_DIR |
| (5,1) | `0x51` | **8** | DIR_N? |
| (5,5) | `0x55` | **4** | DIR_N? |
| (5,6) | `0x56` | **15** | ANY_DIR |
| (6,2) | `0x62` | **6** | ALWAYS |
| (6,10) | `0x6A` | **13** | ANY_DIR |
| (7,4) | `0x74` | **15** | ANY_DIR |
| (7,6) | `0x76` | **16** | 0x00 |
| (7,7) | `0x77` | **16** | ANY_DIR |
| (8,1) | `0x81` | **2** | ALWAYS |
| (8,9) | `0x89` | **14** | ANY_DIR |
| (9,11) | `0x9B` | **13** | ANY_DIR |
| (9,14) | `0x9E` | **12** | ANY_DIR |
| (10,2) | `0xA2` | **5** | ANY_DIR |
| (10,6) | `0xA6` | **17** | ANY_DIR |
| (10,10) | `0xAA` | **15** | ANY_DIR |
| (10,13) | `0xAD` | **12** | ANY_DIR |
| (11,1) | `0xB1` | **7** | ENTER |
| (11,7) | `0xB7` | **17** | ANY_DIR |
| (11,9) | `0xB9` | **14** | ANY_DIR |
| (13,5) | `0xD5` | **14** | ANY_DIR |
| (13,7) | `0xD7` | **18** | ANY_DIR |
| (13,11) | `0xDB` | **17** | ANY_DIR |
| (13,13) | `0xDD` | **15** | ANY_DIR |
| (14,3) | `0xE3` | **1** | ENTER |
| (14,14) | `0xEE` | **14** | ANY_DIR |
| (15,10) | `0xFA` | **9** | ANY_DIR |
| (15,12) | `0xFC` | **17** | ANY_DIR |

## Events

**Event 01** — triggers: (14,3)/ENTER

```hex
2b 0c 0b 07 00 02 01 0a 10 01 0f 16 01 d5 10 05 02 02 0a 10 01 0f 12 69 69 69 69 69 69 00 00 00 00 00 00 0c 38 08
```

```
00: skip_tokens(12)
    # skip -> map_transition(0x38, 0x08)
01: set_service_context(str[7] "Carved onto a tree, the spell Holy / Word is now yours to utter when / n", mode=0x00)
02: show_text_block(str[1] "The drawbridge of Castle Woodhaven is / raised. Request lowering (y/n)?")
03: cond = prompt_yes_no(mode=1)
04: if cond: skip_tokens(1)
    # skip -> cond = check_monster_present(0x01, 0xD5)
05: end_script()
06: cond = check_monster_present(0x01, 0xD5)
07: if cond: skip_tokens(5)
    # skip -> map_transition(0x38, 0x08)
08: show_text_block(str[2] "Guards rudely deny access, exclaiming, / "No key, no admittance!" Attack")
09: cond = prompt_yes_no(mode=1)
10: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
11: end_script()
12: encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
13: map_transition(0x38, 0x08)
```

**Event 02** — triggers: (8,1)/ALWAYS

```hex
2b 03 15 00 78 40 11 05 12 8c 8c 00 00 00 00 00 00 00 00 00 00 02 03 2e 71 08 07 14 02 10 29
```

```
00: skip_tokens(3)
    # skip -> show_text_block(str[3] "You find an aged scroll inscribed with / the runes for Fingers of Death.")
01: apply_party(count=0x00, op=0x78, val=0x40)
02: if not cond: skip_tokens(5)
    # skip -> show_text_block(str[16] "Remnants of Devil's Food are strewn / about the clearing.")
03: encounter_setup(monsters=8C 8C 00 00 00 00 00 00 00 00, flags=00 00)
04: show_text_block(str[3] "You find an aged scroll inscribed with / the runes for Fingers of Death.")
05: set_party_attr(class=0x71, bits=0x08)
06: wait_for_space()
07: clear_current_tile_event_flag()
08: show_text_block(str[16] "Remnants of Devil's Food are strewn / about the clearing.")
09: set_abort_and_exit()
```

**Event 03** — triggers: (1,1)/ANY_DIR

```hex
0b 10 00 02 04 08 16 00 d6 10 05 02 05 1c 08 18 09 43 00 81 08 14 02 06 28 00 d6 1f 00 31 04 10 27 00 08 14
```

```
00: set_service_context(str[16] "Remnants of Devil's Food are strewn / about the clearing.", mode=0x00)
01: show_text_block(str[4] "Mark, a whistling hermit, yells, / "Where are my keys!?!"")
02: wait_key()
03: cond = check_monster_present(0x00, 0xD6)
04: if cond: skip_tokens(5)
    # skip -> show_text_block(str[6] ""Thanks, I've been looking everywhere / for these keys!" (+10,000 exp)")
05: show_text_block(str[5] ""You don't have my keys? Get / the rope and hang 'em high!"")
06: op_1c_engine_query_to_cond(0x08)
07: apply_party_masked(count=0x09, set=0x43, and=0x00, or=0x81)
08: wait_key()
09: clear_current_tile_event_flag()
10: show_text_block(str[6] ""Thanks, I've been looking everywhere / for these keys!" (+10,000 exp)")
11: cond = consume_item(item_id=214, name="Mark's Keys", probe=0)
12: party_effect(sel=0x00, 31 04 10 27 00)
13: wait_key()
14: clear_current_tile_event_flag()
```

**Event 04** — triggers: (5,5)/DIR_N?

```hex
02 07 2e f3 20 07 14
```

```
00: show_text_block(str[7] "Carved onto a tree, the spell Holy / Word is now yours to utter when / n")
01: set_party_attr(class=0xF3, bits=0x20)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 05** — triggers: (10,2)/ANY_DIR

```hex
2b 03 15 00 77 01 11 02 12 4c 4c 4c 4c 4c 4c 00 00 00 00 00 00 14 02 08 07 0d 09 0c 07 33
```

```
00: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
01: apply_party(count=0x00, op=0x77, val=0x01)
02: if not cond: skip_tokens(2)
    # skip -> show_text_block(str[8] "The sight of ghastly 'horrors' is too / much to bear on an empty stomach")
03: encounter_setup(monsters=4C 4C 4C 4C 4C 4C 00 00 00 00, flags=00 00)
04: clear_current_tile_event_flag()
05: show_text_block(str[8] "The sight of ghastly 'horrors' is too / much to bear on an empty stomach")
06: wait_for_space()
07: engine_call(0x09)
08: map_transition(0x07, 0x33)
```

**Event 06** — triggers: (6,2)/ALWAYS

```hex
2b 03 15 00 77 02 10 02 12 2b 2b 2b 2b 2b 2b 2b 2b 00 00 00 00 14 02 09 29
```

```
00: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
01: apply_party(count=0x00, op=0x77, val=0x02)
02: if cond: skip_tokens(2)
    # skip -> show_text_block(str[9] "Aghast at the stench of their fellow / ghouls still fresh on your breath")
03: encounter_setup(monsters=2B 2B 2B 2B 2B 2B 2B 2B 00 00, flags=00 00)
04: clear_current_tile_event_flag()
05: show_text_block(str[9] "Aghast at the stench of their fellow / ghouls still fresh on your breath")
06: set_abort_and_exit()
```

**Event 07** — triggers: (11,1)/ENTER

```hex
02 0a 09 10 01 0f 18 00 27 00 09 0f
```

```
00: show_text_block(str[10] "A sparkling fountain. Drink (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> apply_party_masked(count=0x00, set=0x27, and=0x00, or=0x09)
03: end_script()
04: apply_party_masked(count=0x00, set=0x27, and=0x00, or=0x09)
05: end_script()
```

**Event 08** — triggers: (5,1)/DIR_N?

```hex
02 0b 09 10 01 0f 18 00 28 00 c8 18 00 29 00 00 0f
```

```
00: show_text_block(str[11] "Stars and moons decorate this / fountain. Drink (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> apply_party_masked(count=0x00, set=0x28, and=0x00, or=0xC8)
03: end_script()
04: apply_party_masked(count=0x00, set=0x28, and=0x00, or=0xC8)
05: apply_party_masked(count=0x00, set=0x29, and=0x00, or=0x00)
06: end_script()
```

**Event 09** — triggers: (15,10)/ANY_DIR

```hex
2b 01 12 aa aa aa aa aa aa aa aa 00 00 00 00 16 01 e5 10 05 02 0c 19 01 e5 00 00 10 01 01 0f 07 14
```

```
00: skip_tokens(1)
    # skip -> cond = check_monster_present(0x01, 0xE5)
01: encounter_setup(monsters=AA AA AA AA AA AA AA AA 00 00, flags=00 00)
02: cond = check_monster_present(0x01, 0xE5)
03: if cond: skip_tokens(5)
    # skip -> clear_current_tile_event_flag()
04: show_text_block(str[12] "After defeating those nasty ghosts, / you have discovered, hidden inside")
05: add_party_entity(0x01, f3a=0xE5, f40=0x00, f46=0x00)
06: if cond: skip_tokens(1)
    # skip -> wait_for_space()
07: show_text_basic(str[15] "*** Backpacks Full ***")
08: wait_for_space()
09: clear_current_tile_event_flag()
```

**Event 10** — triggers: (3,14)/ENTER

```hex
06 0d
```

```
00: show_text_popup_style_b(str[13] "Vulcania / This Way[")
```

**Event 11** — triggers: (3,3)/ENTER

```hex
06 0e
```

```
00: show_text_popup_style_b(str[14] "Woodhaven$ / <Tundara")
```

**Event 12** — triggers: (9,14)/ANY_DIR, (10,13)/ANY_DIR

```hex
2b 01 12 2b 2b 2b 2b 2b 2b 2b 2b 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2B 2B 2B 2B 2B 2B 2B 2B 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (6,10)/ANY_DIR, (9,11)/ANY_DIR

```hex
2b 01 12 2d 2d 2d 2d 2d 2d 2d 2d 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2D 2D 2D 2D 2D 2D 2D 2D 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (1,8)/ANY_DIR, (4,5)/ANY_DIR, (8,9)/ANY_DIR, (11,9)/ANY_DIR, (13,5)/ANY_DIR, (14,14)/ANY_DIR

```hex
2b 01 12 3b 3b 3b 3b 3a 3a 3a 3a 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=3B 3B 3B 3B 3A 3A 3A 3A 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (3,6)/ANY_DIR, (5,6)/ANY_DIR, (7,4)/ANY_DIR, (10,10)/ANY_DIR, (13,13)/ANY_DIR

```hex
2b 01 12 3c 3c 3c 3c 3f 3f 3f 3f 4c 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=3C 3C 3C 3C 3F 3F 3F 3F 4C 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (7,6)/0x00, (7,7)/ANY_DIR

```hex
2b 01 12 4c 4c 4c 4c 5c 5c 5c 5c 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=4C 4C 4C 4C 5C 5C 5C 5C 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (10,6)/ANY_DIR, (11,7)/ANY_DIR, (13,11)/ANY_DIR, (15,12)/ANY_DIR

```hex
2b 01 12 7b 7b 5d 5d 5d 5d 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=7B 7B 5D 5D 5D 5D 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (13,7)/ANY_DIR

```hex
2b 01 12 7b 7b 7b 7b 7b 7b 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=7B 7B 7B 7B 7B 7B 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` The drawbridge of Castle Woodhaven is / raised. Request lowering (y/n)?
- `[02]` Guards rudely deny access, exclaiming, / "No key, no admittance!" Attack (y/n)?
- `[03]` You find an aged scroll inscribed with / the runes for Fingers of Death.
- `[04]` Mark, a whistling hermit, yells, / "Where are my keys!?!"
- `[05]` "You don't have my keys? Get / the rope and hang 'em high!"
- `[06]` "Thanks, I've been looking everywhere / for these keys!" (+10,000 exp)
- `[07]` Carved onto a tree, the spell Holy / Word is now yours to utter when / needed.
- `[08]` The sight of ghastly 'horrors' is too / much to bear on an empty stomach so, / the party swifty flees!
- `[09]` Aghast at the stench of their fellow / ghouls still fresh on your breath from / that delicious soup and toast snack, / the monsters flee fearfully!
- `[10]` A sparkling fountain. Drink (y/n)?
- `[11]` Stars and moons decorate this / fountain. Drink (y/n)?
- `[12]` After defeating those nasty ghosts, / you have discovered, hidden inside a / tree, the lost soul of Corak.
- `[13]` Vulcania / This Way[
- `[14]` Woodhaven$ / <Tundara
- `[15]` *** Backpacks Full ***
- `[16]` Remnants of Devil's Food are strewn / about the clearing.
- `[17]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
