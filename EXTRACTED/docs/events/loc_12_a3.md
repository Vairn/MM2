# Location 12 — A3

- **event.dat** offset `0x004477`, length **1325** bytes
- **Map:** map screen **12**; overland sector **A3**
- **Record kind:** `standard`
- **Triggers:** 30; **script segments:** 17; **strings:** 19

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,6) | `0x06` | **3** | ANY_DIR |
| (0,14) | `0x0E` | **3** | ANY_DIR |
| (1,4) | `0x14` | **14** | ANY_DIR |
| (1,8) | `0x18` | **2** | ANY_DIR |
| (1,12) | `0x1C` | **3** | ANY_DIR |
| (2,2) | `0x22` | **4** | ANY_DIR |
| (2,10) | `0x2A` | **3** | ANY_DIR |
| (2,14) | `0x2E` | **14** | ANY_DIR |
| (4,13) | `0x4D` | **13** | ANY_DIR |
| (5,1) | `0x51` | **13** | ANY_DIR |
| (5,8) | `0x58` | **8** | ANY_DIR |
| (6,5) | `0x65` | **8** | ANY_DIR |
| (6,15) | `0x6F` | **13** | ANY_DIR |
| (7,7) | `0x77` | **1** | ANY_DIR |
| (8,0) | `0x80` | **14** | ANY_DIR |
| (8,2) | `0x82` | **13** | ANY_DIR |
| (8,10) | `0x8A` | **5** | ENTER+SPECIAL |
| (9,9) | `0x99` | **5** | DIR_SPECIAL |
| (9,12) | `0x9C` | **9** | ANY_DIR |
| (10,9) | `0xA9` | **5** | DIR_SPECIAL |
| (11,2) | `0xB2` | **11** | ANY_DIR |
| (11,10) | `0xBA` | **5** | DIR_N? |
| (11,11) | `0xBB` | **5** | DIR_N? |
| (11,12) | `0xBC` | **5** | DIR_N? |
| (11,13) | `0xBD` | **5** | DIR_N? |
| (13,1) | `0xD1` | **7** | ALWAYS |
| (14,0) | `0xE0` | **15** | ANY_DIR |
| (14,1) | `0xE1` | **6** | ALWAYS |
| (14,8) | `0xE8` | **12** | ANY_DIR |
| (14,14) | `0xEE` | **10** | ANY_DIR |

## Events

**Event 01** — triggers: (7,7)/ANY_DIR

```hex
0e 5d
```

```
00: exec_selector(0x5D)
```

**Event 02** — triggers: (1,8)/ANY_DIR

```hex
17 11 00 11 01 14 02 04 09 10 01 0f 0e 5e
```

```
00: cond = load_var8(group=0x11, index=0x00)
01: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[4] "Squirming prisoners are staked to the / ground. Free them (y/n)?")
02: clear_current_tile_event_flag()
03: show_text_block(str[4] "Squirming prisoners are staked to the / ground. Free them (y/n)?")
04: cond = prompt_yes_no()
05: if cond: skip_tokens(1)
    # skip -> exec_selector(0x5E)
06: end_script()
07: exec_selector(0x5E)
```

**Event 03** — triggers: (0,6)/ANY_DIR, (0,14)/ANY_DIR, (1,12)/ANY_DIR, (2,10)/ANY_DIR

```hex
2b 05 02 04 09 10 01 0f 12 76 76 76 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(5)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[4] "Squirming prisoners are staked to the / ground. Free them (y/n)?")
02: cond = prompt_yes_no()
03: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=76 76 76 00 00 00 00 00 00 00, flags=00 00)
04: end_script()
05: encounter_setup(monsters=76 76 76 00 00 00 00 00 00 00, flags=00 00)
06: clear_current_tile_event_flag()
```

**Event 04** — triggers: (2,2)/ANY_DIR

```hex
2b 1a 02 03 15 02 0c 00 10 01 18 02 43 00 81 15 03 0c 00 10 01 18 03 43 00 81 15 04 0c 00 10 01 18 04 43 00 81 15 05 0c 00 10 01 18 05 43 00 81 15 06 0c 00 10 01 18 06 43 00 81 15 07 0c 00 10 01 18 07 43 00 81 15 08 0c 00 10 01 18 08 43 00 81 15 01 0c 00 10 01 18 01 43 00 81 12 e4 e4 e4 e4 e2 e2 e3 e3 e5 e5 00 00 02 06 2a 10 27 00 00 00 58 00 00 00 00 00 05 00 00 07 14
```

```
00: skip_tokens(26)
    # skip -> show_text_block(str[6] "In the hull of this old ship, you will / find sunken treasure if you sea")
01: show_text_block(str[3] "A sultry siren's seductive song sweeps / surreptitiously through the sea")
02: apply_party(count=0x02, op=0x0C, val=0x00)
03: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x0C, val=0x00)
04: apply_party_masked(count=0x02, set=0x43, and=0x00, or=0x81)
05: apply_party(count=0x03, op=0x0C, val=0x00)
06: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x0C, val=0x00)
07: apply_party_masked(count=0x03, set=0x43, and=0x00, or=0x81)
08: apply_party(count=0x04, op=0x0C, val=0x00)
09: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x0C, val=0x00)
10: apply_party_masked(count=0x04, set=0x43, and=0x00, or=0x81)
11: apply_party(count=0x05, op=0x0C, val=0x00)
12: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x0C, val=0x00)
13: apply_party_masked(count=0x05, set=0x43, and=0x00, or=0x81)
14: apply_party(count=0x06, op=0x0C, val=0x00)
15: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x0C, val=0x00)
16: apply_party_masked(count=0x06, set=0x43, and=0x00, or=0x81)
17: apply_party(count=0x07, op=0x0C, val=0x00)
18: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x0C, val=0x00)
19: apply_party_masked(count=0x07, set=0x43, and=0x00, or=0x81)
20: apply_party(count=0x08, op=0x0C, val=0x00)
21: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x01, op=0x0C, val=0x00)
22: apply_party_masked(count=0x08, set=0x43, and=0x00, or=0x81)
23: apply_party(count=0x01, op=0x0C, val=0x00)
24: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=E4 E4 E4 E4 E2 E2 E3 E3 E5 E5, flags=00 00)
25: apply_party_masked(count=0x01, set=0x43, and=0x00, or=0x81)
26: encounter_setup(monsters=E4 E4 E4 E4 E2 E2 E3 E3 E5 E5, flags=00 00)
27: show_text_block(str[6] "In the hull of this old ship, you will / find sunken treasure if you sea")
28: set_treasure(gold/exp=10000, gems=0, items=[88, 0, 0])
29: wait_for_space()
30: clear_current_tile_event_flag()
```

**Event 05** — triggers: (8,10)/ENTER+SPECIAL, (9,9)/DIR_SPECIAL, (10,9)/DIR_SPECIAL, (11,10)/DIR_N?, (11,11)/DIR_N?, (11,12)/DIR_N?, (11,13)/DIR_N?

```hex
01 07 29
```

```
00: show_text_basic(str[7] "Rotting corpses line the bay!")
01: set_abort_and_exit()
```

**Event 06** — triggers: (14,1)/ALWAYS

```hex
02 08 09 10 02 02 09 29 02 0a 18 00 43 f7 08 29
```

```
00: show_text_block(str[8] "Two fountains sit side by side. / Drink from the right one (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(2)
    # skip -> show_text_block(str[10] "A voice yells, "Ha, ha! It's poison!"")
03: show_text_block(str[9] "A voice whispers, "You'll never know / what you're missing!"")
04: set_abort_and_exit()
05: show_text_block(str[10] "A voice yells, "Ha, ha! It's poison!"")
06: apply_party_masked(count=0x00, set=0x43, and=0xF7, or=0x08)
07: set_abort_and_exit()
```

**Event 07** — triggers: (13,1)/ALWAYS

```hex
02 0b 09 10 02 02 0c 29 02 0d 1f 00 23 01 19 00 00 29
```

```
00: show_text_block(str[11] "Two fountains sit side by side. / Drink from the left one (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(2)
    # skip -> show_text_block(str[13] "A voice exclaims, "Darn, I thought I / could fool you!" (+25 Speed)")
03: show_text_block(str[12] "A voice whispers, "Good, you should / drink from the right fountain anyw")
04: set_abort_and_exit()
05: show_text_block(str[13] "A voice exclaims, "Darn, I thought I / could fool you!" (+25 Speed)")
06: party_effect(sel=0x00, 23 01 19 00 00)
07: set_abort_and_exit()
```

**Event 08** — triggers: (5,8)/ANY_DIR, (6,5)/ANY_DIR

```hex
2b 02 02 0e 12 85 74 74 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[14] "Dragons strike from above!")
02: encounter_setup(monsters=85 74 74 00 00 00 00 00 00 00, flags=00 00)
03: clear_current_tile_event_flag()
```

**Event 09** — triggers: (9,12)/ANY_DIR

```hex
2b 01 12 92 aa bb 8e 7a 5d 4c 3f 3c 3b 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=92 AA BB 8E 7A 5D 4C 3F 3C 3B, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (14,14)/ANY_DIR

```hex
2b 01 12 2d 2d 2d 2d 2d 2d 2d 2d 2d 2d 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2D 2D 2D 2D 2D 2D 2D 2D 2D 2D, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 11** — triggers: (11,2)/ANY_DIR

```hex
2b 01 12 65 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=65 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (14,8)/ANY_DIR

```hex
2b 01 12 2b 2b 2b 2b 2b 2b 2b 2b 2b 2b 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=2B 2B 2B 2B 2B 2B 2B 2B 2B 2B, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (4,13)/ANY_DIR, (5,1)/ANY_DIR, (6,15)/ANY_DIR, (8,2)/ANY_DIR

```hex
01 0f 07 0d 09 31 00 28 00 0c 0c 48
```

```
00: show_text_basic(str[15] "A tidal wave sweeps the party away!")
01: wait_for_space()
02: engine_call(0x09)
03: for_party(mask=0x00): call(0x28,0x00)
04: map_transition(0x0C, 0x48)
```

**Event 14** — triggers: (1,4)/ANY_DIR, (2,14)/ANY_DIR, (8,0)/ANY_DIR

```hex
01 10 07 0d 09 0c 0f 86
```

```
00: show_text_basic(str[16] "A whirlpool sucks the party under!")
01: wait_for_space()
02: engine_call(0x09)
03: map_transition(0x0F, 0x86)
```

**Event 15** — triggers: (14,0)/ANY_DIR

```hex
2b 02 02 11 12 65 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[17] ""Uh oh! You caught me!"")
02: encounter_setup(monsters=65 00 00 00 00 00 00 00 00 00, flags=00 00)
03: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` N
- `[02]` <SENTINEL_Z>
- `[03]` A sultry siren's seductive song sweeps / surreptitiously through the sea air.
- `[04]` Squirming prisoners are staked to the / ground. Free them (y/n)?
- `[05]` A sultry siren's seductive song sweeps / surreptitiously through the sea air. / The party is hypnotically drawn toward / the sweet sound. All males are dead.
- `[06]` In the hull of this old ship, you will / find sunken treasure if you search.
- `[07]` Rotting corpses line the bay!
- `[08]` Two fountains sit side by side. / Drink from the right one (y/n)?
- `[09]` A voice whispers, "You'll never know / what you're missing!"
- `[10]` A voice yells, "Ha, ha! It's poison!"
- `[11]` Two fountains sit side by side. / Drink from the left one (y/n)?
- `[12]` A voice whispers, "Good, you should / drink from the right fountain anyway."
- `[13]` A voice exclaims, "Darn, I thought I / could fool you!" (+25 Speed)
- `[14]`       Dragons strike from above!
- `[15]` A tidal wave sweeps the party away!
- `[16]` A whirlpool sucks the party under!
- `[17]` "Uh oh! You caught me!"
- `[18]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
