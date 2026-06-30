# Location 08 — D1

- **event.dat** offset `0x002E02`, length **1480** bytes
- **Map:** map screen **8**; overland sector **D1**
- **Record kind:** `standard`
- **Triggers:** 42; **script segments:** 20; **strings:** 17

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,2) | `0x12` | **17** | DIR_E? |
| (1,4) | `0x14` | **17** | DIR_S? |
| (1,6) | `0x16` | **17** | DIR_S? |
| (1,8) | `0x18` | **17** | DIR_W? |
| (1,11) | `0x1B` | **3** | DIR_E? |
| (1,14) | `0x1E` | **9** | DIR_E? |
| (2,11) | `0x2B` | **2** | DIR_S? |
| (3,1) | `0x31` | **11** | 0x50 |
| (3,4) | `0x34` | **12** | 0x50 |
| (3,6) | `0x36` | **10** | 0x50 |
| (3,9) | `0x39` | **10** | 0x50 |
| (4,5) | `0x45` | **1** | DIR_N? |
| (5,3) | `0x53` | **1** | DIR_E? |
| (5,4) | `0x54` | **4** | ANY_DIR |
| (5,5) | `0x55` | **4** | ANY_DIR |
| (5,6) | `0x56` | **4** | ANY_DIR |
| (5,7) | `0x57` | **1** | DIR_W? |
| (6,3) | `0x63` | **1** | DIR_E? |
| (6,4) | `0x64` | **4** | ANY_DIR |
| (6,5) | `0x65` | **5** | ANY_DIR |
| (6,6) | `0x66` | **4** | ANY_DIR |
| (6,7) | `0x67` | **1** | DIR_W? |
| (7,3) | `0x73` | **1** | DIR_E? |
| (7,4) | `0x74` | **4** | ANY_DIR |
| (7,5) | `0x75` | **4** | ANY_DIR |
| (7,6) | `0x76` | **4** | ANY_DIR |
| (7,7) | `0x77` | **1** | DIR_W? |
| (7,9) | `0x79` | **15** | ANY_DIR |
| (8,0) | `0x80` | **8** | ANY_DIR |
| (8,4) | `0x84` | **1** | DIR_S? |
| (8,5) | `0x85` | **1** | DIR_S? |
| (8,6) | `0x86` | **1** | DIR_S? |
| (9,7) | `0x97` | **14** | ANY_DIR |
| (9,13) | `0x9D` | **13** | DIR_W? |
| (10,9) | `0xA9` | **15** | ANY_DIR |
| (11,2) | `0xB2` | **14** | ANY_DIR |
| (11,6) | `0xB6` | **14** | ANY_DIR |
| (12,10) | `0xCA` | **7** | ANY_DIR |
| (12,12) | `0xCC` | **15** | ANY_DIR |
| (14,8) | `0xE8` | **16** | ANY_DIR |
| (14,11) | `0xEB` | **15** | ANY_DIR |
| (14,12) | `0xEC` | **6** | DIR_N?+DIR_E? |

## Events

**Event 01** — triggers: (4,5)/DIR_N?, (5,3)/DIR_E?, (5,7)/DIR_W?, (6,3)/DIR_E?, (6,7)/DIR_W?, (7,3)/DIR_E?, (7,7)/DIR_W?, (8,4)/DIR_S?, (8,5)/DIR_S?, (8,6)/DIR_S?

```hex
06 01
```

```
00: show_text_popup_style_b(str[1] "Dead Zone / Radiation!")
```

**Event 02** — triggers: (2,11)/DIR_S?

```hex
06 02
```

```
00: show_text_popup_style_b(str[2] "Bozorc's / Dominion")
```

**Event 03** — triggers: (1,11)/DIR_E?

```hex
06 03
```

```
00: show_text_popup_style_b(str[3] "No Orc / Jokes!")
```

**Event 04** — triggers: (5,4)/ANY_DIR, (5,5)/ANY_DIR, (5,6)/ANY_DIR, (6,4)/ANY_DIR, (6,6)/ANY_DIR, (7,4)/ANY_DIR, (7,5)/ANY_DIR, (7,6)/ANY_DIR

```hex
02 04 1c 08 18 09 43 00 fe 1f 09 43 01 01 00 00 07 0d 09 0c 27 00
```

```
00: show_text_block(str[4] "The party is overcome with queasiness / and uncontrollable retching. You")
01: op_1c_engine_query_to_cond(0x08)
02: apply_party_masked(count=0x09, set=0x43, and=0x00, or=0xFE)
03: party_effect(sel=0x09, 43 01 01 00 00)
04: wait_for_space()
05: engine_call(0x09)
06: map_transition(0x27, 0x00)
```

**Event 05** — triggers: (6,5)/ANY_DIR

```hex
06 05 23 5d 5d 11 04 02 06 07 2e 73 40 14 03 07 07 18 00 43 00 81 14
```

```
00: show_text_popup_style_b(str[5] "Ground / Zero!")
01: cond = check_day_of_year(0x5D, 0x5D)
02: if not cond: skip_tokens(4)
    # skip -> show_text(str[7] "A scorching wind burns your soul and / the blinding light pierces your b")
03: show_text_block(str[6] "Embedded in the void which comprises / the Dead Zone, the ancient spell ")
04: wait_for_space()
05: set_party_attr(class=0x73, bits=0x40)
06: clear_current_tile_event_flag()
07: show_text(str[7] "A scorching wind burns your soul and / the blinding light pierces your b")
08: wait_for_space()
09: apply_party_masked(count=0x00, set=0x43, and=0x00, or=0x81)
10: clear_current_tile_event_flag()
```

**Event 06** — triggers: (14,12)/DIR_N?+DIR_E?

```hex
02 08 09 11 01 0c 1d 80 0f
```

```
00: show_text_block(str[8] "Dragon dung and scattered bones litter / this lair entrance. Go in (y/n)")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x1D, 0x80)
04: end_script()
```

**Event 07** — triggers: (12,10)/ANY_DIR

```hex
2b 04 15 00 7a 80 10 03 02 09 12 f4 37 05 05 00 00 00 00 00 00 00 00 18 00 7a 7f 80 14
```

```
00: skip_tokens(4)
    # skip -> apply_party_masked(count=0x00, set=0x7A, and=0x7F, or=0x80)
01: apply_party(count=0x00, op=0x7A, val=0x80)
02: if cond: skip_tokens(3)
    # skip -> clear_current_tile_event_flag()
03: show_text_block(str[9] "The Winged Envoy of Evil attacks you!")
04: encounter_setup(monsters=F4 37 05 05 00 00 00 00 00 00, flags=00 00)
05: apply_party_masked(count=0x00, set=0x7A, and=0x7F, or=0x80)
06: clear_current_tile_event_flag()
```

**Event 08** — triggers: (8,0)/ANY_DIR

```hex
2b 04 02 0a 09 11 03 12 89 89 89 89 00 00 00 00 00 00 00 00 2a 00 00 00 00 00 e4 00 00 00 00 00 00 00 00 14 0f
```

```
00: skip_tokens(4)
    # skip -> set_treasure(gold/exp=0, gems=0, items=[228, 0, 0])
01: show_text_block(str[10] "Do you seek the Sword of / Nobility (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(3)
    # skip -> end_script()
04: encounter_setup(monsters=89 89 89 89 00 00 00 00 00 00, flags=00 00)
05: set_treasure(gold/exp=0, gems=0, items=[228, 0, 0])
06: clear_current_tile_event_flag()
07: end_script()
```

**Event 09** — triggers: (1,14)/DIR_E?

```hex
2b 01 12 ec 2d 11 11 11 11 11 11 11 11 11 5c 17 0d 00 10 04 02 0f 07 1a 0d 01 1a 0e 01 14
```

```
00: skip_tokens(1)
    # skip -> cond = load_var8(group=0x0D, index=0x00)
01: encounter_setup(monsters=EC 2D 11 11 11 11 11 11 11 11, flags=11 5C)
02: cond = load_var8(group=0x0D, index=0x00)
03: if cond: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
04: show_text_block(str[15] "Red Duke and Dead Eye are free! / Hire at Hotel Four.")
05: wait_for_space()
06: store_var8(group=0x0D, value=0x01)
07: store_var8(group=0x0E, value=0x01)
08: clear_current_tile_event_flag()
```

**Event 10** — triggers: (3,6)/0x50, (3,9)/0x50

```hex
2b 05 1c 64 1b 1e 10 03 02 0b 12 10 05 05 05 05 05 05 05 05 05 05 0b 14 0f
```

```
00: skip_tokens(5)
    # skip -> clear_current_tile_event_flag()
01: op_1c_engine_query_to_cond(0x64)
02: cond = (cond >= 0x1E)
03: if cond: skip_tokens(3)
    # skip -> end_script()
04: show_text_block(str[11] "/                AMBUSH!")
05: encounter_setup(monsters=10 05 05 05 05 05 05 05 05 05, flags=05 0B)
06: clear_current_tile_event_flag()
07: end_script()
```

**Event 11** — triggers: (3,1)/0x50

```hex
2b 05 1c 64 1b 1e 10 03 02 0b 12 10 05 05 05 05 05 05 05 05 05 05 29 14 0f
```

```
00: skip_tokens(5)
    # skip -> clear_current_tile_event_flag()
01: op_1c_engine_query_to_cond(0x64)
02: cond = (cond >= 0x1E)
03: if cond: skip_tokens(3)
    # skip -> end_script()
04: show_text_block(str[11] "/                AMBUSH!")
05: encounter_setup(monsters=10 05 05 05 05 05 05 05 05 05, flags=05 29)
06: clear_current_tile_event_flag()
07: end_script()
```

**Event 12** — triggers: (3,4)/0x50

```hex
2b 05 1c 64 1b 1e 10 03 02 0b 12 10 05 05 05 05 05 05 05 05 05 05 47 14 0f
```

```
00: skip_tokens(5)
    # skip -> clear_current_tile_event_flag()
01: op_1c_engine_query_to_cond(0x64)
02: cond = (cond >= 0x1E)
03: if cond: skip_tokens(3)
    # skip -> end_script()
04: show_text_block(str[11] "/                AMBUSH!")
05: encounter_setup(monsters=10 05 05 05 05 05 05 05 05 05, flags=05 47)
06: clear_current_tile_event_flag()
07: end_script()
```

**Event 13** — triggers: (9,13)/DIR_W?

```hex
02 0c 09 11 01 18 00 2c 00 41 0f
```

```
00: show_text_block(str[12] "A wishing well appears before you. / Would you like to make a wish (y/n)")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x2C, and=0x00, or=0x41)
04: end_script()
```

**Event 14** — triggers: (9,7)/ANY_DIR, (11,2)/ANY_DIR, (11,6)/ANY_DIR

```hex
2b 01 12 74 74 74 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=74 74 74 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (7,9)/ANY_DIR, (10,9)/ANY_DIR, (12,12)/ANY_DIR, (14,11)/ANY_DIR

```hex
2b 01 12 75 75 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=75 75 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (14,8)/ANY_DIR

```hex
2b 01 12 85 85 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=85 85 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 17** — triggers: (1,2)/DIR_E?, (1,4)/DIR_S?, (1,6)/DIR_S?, (1,8)/DIR_W?

```hex
2b 04 02 0d 09 11 02 12 05 05 05 05 05 05 05 05 05 05 05 f0 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[13] "Nestled before you is a peaceful / Goblin Village, whose destiny is to /")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=05 05 05 05 05 05 05 05 05 05, flags=05 F0)
05: clear_current_tile_event_flag()
06: end_script()
```

## String table

- `[00]` <EMPTY>
- `[01]` Dead Zone / Radiation!
- `[02]` Bozorc's / Dominion
- `[03]` No Orc / Jokes!
- `[04]` The party is overcome with queasiness / and uncontrollable retching. You begin / to scream silently as the flesh melts / from your bones!
- `[05]` Ground / Zero!
- `[06]` Embedded in the void which comprises / the Dead Zone, the ancient spell Star / Burst has lain hidden for many years- / until now!
- `[07]` A scorching wind burns your soul and / the blinding light pierces your brain. / Your skin bubbles as unbearable pain / moves like lightning through your / body and leaves no traces of life.
- `[08]` Dragon dung and scattered bones litter / this lair entrance. Go in (y/n)?
- `[09]` The Winged Envoy of Evil attacks you!
- `[10]` Do you seek the Sword of / Nobility (y/n)?
- `[11]`  /                AMBUSH!
- `[12]` A wishing well appears before you. / Would you like to make a wish (y/n)?
- `[13]` Nestled before you is a peaceful / Goblin Village, whose destiny is to / be slaughtered by ruthless / adventurers. Attack (y/n)?
- `[14]` Several dragons are a little upset / about your choice of entrees.
- `[15]` Red Duke and Dead Eye are free! / Hire at Hotel Four.
- `[16]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
