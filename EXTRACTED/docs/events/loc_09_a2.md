# Location 09 — A2

- **event.dat** offset `0x0033CA`, length **1392** bytes
- **Map:** map screen **9**; overland sector **A2**
- **Record kind:** `standard`
- **Triggers:** 29; **script segments:** 19; **strings:** 17

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,1) | `0x11` | **1** | DIR_W? |
| (2,7) | `0x27` | **15** | ANY_DIR |
| (2,11) | `0x2B` | **3** | ANY_DIR |
| (3,1) | `0x31` | **4** | DIR_W? |
| (3,6) | `0x36` | **16** | ANY_DIR |
| (3,12) | `0x3C` | **2** | DIR_N? |
| (3,14) | `0x3E` | **15** | ANY_DIR |
| (4,2) | `0x42` | **15** | ANY_DIR |
| (4,6) | `0x46` | **14** | ANY_DIR |
| (5,0) | `0x50` | **10** | ANY_DIR |
| (5,6) | `0x56` | **9** | ANY_DIR |
| (5,12) | `0x5C` | **16** | ANY_DIR |
| (7,8) | `0x78` | **5** | DIR_W? |
| (7,12) | `0x7C` | **14** | ANY_DIR |
| (7,14) | `0x7E` | **6** | 0x50 |
| (9,2) | `0x92` | **13** | ANY_DIR |
| (9,7) | `0x97` | **8** | ANY_DIR |
| (9,14) | `0x9E` | **11** | ANY_DIR |
| (10,3) | `0xA3` | **16** | ANY_DIR |
| (10,14) | `0xAE` | **17** | ANY_DIR |
| (11,4) | `0xB4` | **8** | ANY_DIR |
| (11,9) | `0xB9` | **8** | ANY_DIR |
| (11,15) | `0xBF` | **12** | ANY_DIR |
| (12,0) | `0xC0` | **7** | ANY_DIR |
| (12,13) | `0xCD` | **7** | ANY_DIR |
| (13,2) | `0xD2` | **8** | ANY_DIR |
| (13,7) | `0xD7` | **7** | ANY_DIR |
| (14,1) | `0xE1` | **14** | ANY_DIR |
| (14,10) | `0xEA` | **8** | ANY_DIR |

## Events

**Event 01** — triggers: (1,1)/DIR_W?

```hex
2b 0c 0b 07 00 02 01 0a 10 01 0f 16 01 d5 10 05 02 0b 0a 10 01 0f 12 69 69 69 69 69 69 00 00 00 00 00 00 0c 39 8f
```

```
00: skip_tokens(12)
    # skip -> map_transition(0x39, 0x8F)
01: service_sign(idx=0x07 -> sign 74 [74.anm], pos=0x00)
02: show_text_block(str[1] "Before you stands the hold of Lord / Peabody, Castle Pinehurst. / Enter ")
03: cond = prompt_yes_no(mode=1)
04: if cond: skip_tokens(1)
    # skip -> cond = check_monster_present(0x01, 0xD5)
05: end_script()
06: cond = check_monster_present(0x01, 0xD5)
07: if cond: skip_tokens(5)
    # skip -> map_transition(0x39, 0x8F)
08: show_text_block(str[11] "Castle guards exclaim, "No key, no / admittance!" Force your way in (y/n")
09: cond = prompt_yes_no(mode=1)
10: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
11: end_script()
12: encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
13: map_transition(0x39, 0x8F)
```

**Event 02** — triggers: (3,12)/DIR_N?

```hex
02 02 09 11 01 0c 19 08 0f
```

```
00: show_text_block(str[2] "Between the gaping jaws of a skull / carved in the mountainside sits the")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x19, 0x08)
04: end_script()
```

**Event 03** — triggers: (2,11)/ANY_DIR

```hex
2b 06 0b 04 00 02 03 0a 10 01 0f 12 78 78 78 78 78 78 00 00 00 00 00 00 2a 00 00 00 00 00 e2 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(6)
    # skip -> set_treasure(gold/exp=0, gems=0, items=[226, 0, 0])
01: service_sign(idx=0x04 -> sign 58 [58.anm], pos=0x00)
02: show_text_block(str[3] "Burly mountain men guard the Sword of / Valor. Challenge them for it (y/")
03: cond = prompt_yes_no(mode=1)
04: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=78 78 78 78 78 78 00 00 00 00, flags=00 00)
05: end_script()
06: encounter_setup(monsters=78 78 78 78 78 78 00 00 00 00, flags=00 00)
07: set_treasure(gold/exp=0, gems=0, items=[226, 0, 0])
08: clear_current_tile_event_flag()
```

**Event 04** — triggers: (3,1)/DIR_W?

```hex
02 04 09 11 02 18 00 43 fb 04 18 00 22 00 1e 0f
```

```
00: show_text_block(str[4] "Pool of pestilence. Jump in (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x43, and=0xFB, or=0x04)
04: apply_party_masked(count=0x00, set=0x22, and=0x00, or=0x1E)
05: end_script()
```

**Event 05** — triggers: (7,8)/DIR_W?

```hex
06 05
```

```
00: show_text_popup_style_b(str[5] "Pinehurst / < This Way")
```

**Event 06** — triggers: (7,14)/0x50

```hex
02 06 29
```

```
00: show_text_block(str[6] "Wagon tracks lead off the side of the / road.")
01: set_abort_and_exit()
```

**Event 07** — triggers: (12,0)/ANY_DIR, (12,13)/ANY_DIR, (13,7)/ANY_DIR

```hex
1c 64 1b 1e 10 05 01 07 0d 09 07 31 00 1e 00 0c 89 00 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_basic(str[7] "A snow drift sweeps the party away!")
04: engine_call(0x09)
05: wait_for_space()
06: for_party(mask=0x00): call(0x1E,0x00)
07: map_transition(0x89, 0x00)
08: end_script()
```

**Event 08** — triggers: (9,7)/ANY_DIR, (11,4)/ANY_DIR, (11,9)/ANY_DIR, (13,2)/ANY_DIR, (14,10)/ANY_DIR

```hex
1c 64 1b 1e 10 05 01 08 0d 09 07 31 00 14 00 0c 89 00 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_basic(str[8] "A blizzard blinds the party!")
04: engine_call(0x09)
05: wait_for_space()
06: for_party(mask=0x00): call(0x14,0x00)
07: map_transition(0x89, 0x00)
08: end_script()
```

**Event 09** — triggers: (5,6)/ANY_DIR

```hex
2b 01 12 5b 5b 5b 5b 5b 5b 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=5B 5B 5B 5B 5B 5B 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (5,0)/ANY_DIR

```hex
2b 01 12 88 88 88 88 88 88 17 17 17 17 17 1a 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=88 88 88 88 88 88 17 17 17 17, flags=17 1A)
02: clear_current_tile_event_flag()
```

**Event 11** — triggers: (9,14)/ANY_DIR

```hex
2b 01 12 c3 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=C3 00 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (11,15)/ANY_DIR

```hex
2b 01 12 f1 00 00 00 00 00 00 00 00 00 00 00 02 09 2e 72 10 07 14
```

```
00: skip_tokens(1)
    # skip -> show_text_block(str[9] "Your successful battle with the mighty / Mist Warrior has taught you, th")
01: encounter_setup(monsters=F1 00 00 00 00 00 00 00 00 00, flags=00 00)
02: show_text_block(str[9] "Your successful battle with the mighty / Mist Warrior has taught you, th")
03: set_party_attr(class=0x72, bits=0x10)
04: wait_for_space()
05: clear_current_tile_event_flag()
```

**Event 13** — triggers: (9,2)/ANY_DIR

```hex
2b 0e 0b 17 00 02 0c 08 02 0f 2f 30 cf d5 c1 c7 fa fa fa fa fa fa 11 06 02 0d 19 01 d6 00 00 10 01 01 0e 08 14 12 30 30 30 30 30 30 30 ee 00 00 00 00 14
```

```
00: skip_tokens(14)
    # skip -> clear_current_tile_event_flag()
01: service_sign(idx=0x17 -> sign 3 [3.anm], pos=0x00)
02: show_text_block(str[12] "Suddenly, the party is ensnared in a / massive web! The treacherous Deat")
03: wait_key()
04: show_text_block(str[15] "unless you answer this riddle. / What has Mark lost?"")
05: op_2f_clear_input_buf()
06: cond = check_answer("KEYS")
07: if not cond: skip_tokens(6)
    # skip -> encounter_setup(monsters=30 30 30 30 30 30 30 EE 00 00, flags=00 00)
08: show_text_block(str[13] ""Correct! Here they are."")
09: add_party_entity(0x01, f3a=0xD6, f40=0x00, f46=0x00)
10: if cond: skip_tokens(1)
    # skip -> wait_key()
11: show_text_basic(str[14] "*** Backpacks Full ***")
12: wait_key()
13: clear_current_tile_event_flag()
14: encounter_setup(monsters=30 30 30 30 30 30 30 EE 00 00, flags=00 00)
15: clear_current_tile_event_flag()
```

**Event 14** — triggers: (4,6)/ANY_DIR, (7,12)/ANY_DIR, (14,1)/ANY_DIR

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

**Event 15** — triggers: (2,7)/ANY_DIR, (3,14)/ANY_DIR, (4,2)/ANY_DIR

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

**Event 16** — triggers: (3,6)/ANY_DIR, (5,12)/ANY_DIR, (10,3)/ANY_DIR

```hex
22 07 07 11 02 2b 01 13 ce ce ce ce 00 00 00 00 00 00 14
```

```
00: cond = (era in [7..7])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE CE CE CE 00 00 00 00 00 00)
04: clear_current_tile_event_flag()
```

**Event 17** — triggers: (10,14)/ANY_DIR

```hex
2b 04 15 00 78 20 11 02 02 0a 12 51 51 51 51 51 51 51 51 51 51 00 00 14
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: apply_party(count=0x00, op=0x78, val=0x20)
02: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
03: show_text_block(str[10] "A gnome voice shrieks, "You ate / my brother! Now you must die!"")
04: encounter_setup(monsters=51 51 51 51 51 51 51 51 51 51, flags=00 00)
05: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` Before you stands the hold of Lord / Peabody, Castle Pinehurst. / Enter (y/n)?
- `[02]` Between the gaping jaws of a skull / carved in the mountainside sits the / entrance to Sarakin's Mine. / Enter (y/n)?
- `[03]` Burly mountain men guard the Sword of / Valor. Challenge them for it (y/n)?
- `[04]` Pool of pestilence. Jump in (y/n)?
- `[05]` Pinehurst / < This Way
- `[06]` Wagon tracks lead off the side of the / road.
- `[07]` A snow drift sweeps the party away!
- `[08]` A blizzard blinds the party!
- `[09]` Your successful battle with the mighty / Mist Warrior has taught you, through / careful observation, the Dancing Sword / spell.
- `[10]` A gnome voice shrieks, "You ate / my brother! Now you must die!"
- `[11]` Castle guards exclaim, "No key, no / admittance!" Force your way in (y/n)?
- `[12]` Suddenly, the party is ensnared in a / massive web! The treacherous Death / Spider creaps toward you, hissing, / "Tasty morsels, I will eat you alive,
- `[13]` "Correct! Here they are."
- `[14]` *** Backpacks Full ***
- `[15]` unless you answer this riddle. / What has Mark lost?"
- `[16]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
