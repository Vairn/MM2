# Location 15 — A4

- **event.dat** offset `0x005512`, length **1119** bytes
- **Map:** map screen **15**; overland sector **A4**
- **Record kind:** `standard`
- **Triggers:** 30; **script segments:** 16; **strings:** 13

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **2** | ANY_DIR |
| (1,1) | `0x11` | **4** | ANY_DIR |
| (1,5) | `0x15` | **6** | ANY_DIR |
| (1,10) | `0x1A` | **12** | ANY_DIR |
| (1,12) | `0x1C` | **14** | ANY_DIR |
| (2,8) | `0x28` | **6** | ANY_DIR |
| (2,10) | `0x2A` | **13** | ANY_DIR |
| (3,2) | `0x32` | **12** | ANY_DIR |
| (3,3) | `0x33` | **13** | ANY_DIR |
| (4,1) | `0x41` | **6** | ANY_DIR |
| (4,4) | `0x44` | **6** | ANY_DIR |
| (4,7) | `0x47` | **13** | ANY_DIR |
| (5,14) | `0x5E` | **10** | ANY_DIR |
| (6,4) | `0x64` | **14** | ANY_DIR |
| (6,6) | `0x66` | **12** | ANY_DIR |
| (7,12) | `0x7C` | **7** | DIR_S? |
| (8,1) | `0x81` | **6** | ANY_DIR |
| (8,8) | `0x88` | **5** | ANY_DIR |
| (10,0) | `0xA0` | **14** | ANY_DIR |
| (10,8) | `0xA8` | **10** | ANY_DIR |
| (10,10) | `0xAA` | **8** | DIR_W? |
| (10,13) | `0xAD` | **1** | DIR_W? |
| (12,7) | `0xC7` | **11** | ANY_DIR |
| (12,9) | `0xC9` | **10** | ANY_DIR |
| (13,13) | `0xDD` | **9** | DIR_N? |
| (14,5) | `0xE5` | **10** | ANY_DIR |
| (14,10) | `0xEA` | **3** | ANY_DIR |
| (14,15) | `0xEF` | **10** | ANY_DIR |
| (15,8) | `0xF8` | **3** | ANY_DIR |
| (15,12) | `0xFC` | **3** | ANY_DIR |

## Events

**Event 01** — triggers: (10,13)/DIR_W?

```hex
0b 18 00 02 01 0a 11 01 0c 01 fe 0f
```

```
00: service_sign(idx=0x18 -> sign 29 [29.anm], pos=0x00)
01: show_text_block(str[1] "You spy upon a lone seagull drifting / down to Atlantium. Enter (y/n)?")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x01, 0xFE)
05: end_script()
```

**Event 02** — triggers: (0,0)/ANY_DIR

```hex
02 02 09 11 02 1a 84 01 0c 2c 00 0f
```

```
00: show_text_block(str[2] "A wet mist drenches the party as you / stand before the passage to the /")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: store_var8(group=0x84, value=0x01)
04: map_transition(0x2C, 0x00)
05: end_script()
```

**Event 03** — triggers: (14,10)/ANY_DIR, (15,8)/ANY_DIR, (15,12)/ANY_DIR

```hex
2b 04 02 03 09 11 02 12 76 76 76 00 00 00 00 00 00 00 00 00 14 0f
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[3] "Prisoners are staked to the ground. / Free them (y/n)?")
02: cond = prompt_yes_no()
03: if not cond: skip_tokens(2)
    # skip -> end_script()
04: encounter_setup(monsters=76 76 76 00 00 00 00 00 00 00, flags=00 00)
05: clear_current_tile_event_flag()
06: end_script()
```

**Event 04** — triggers: (1,1)/ANY_DIR

```hex
03 04 2e f2 04 07 14
```

```
00: show_text(str[4] "By adventuring in the watery abyss, an / understanding with liquid has b")
01: set_party_attr(class=0xF2, bits=0x04)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 05** — triggers: (8,8)/ANY_DIR

```hex
02 05 2e f2 08 07 14
```

```
00: show_text_block(str[5] "Affixed to a table in the center of an / empty garrison, an old, rotted ")
01: set_party_attr(class=0xF2, bits=0x08)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 06** — triggers: (1,5)/ANY_DIR, (2,8)/ANY_DIR, (4,1)/ANY_DIR, (4,4)/ANY_DIR, (8,1)/ANY_DIR

```hex
2b 01 12 e1 e1 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=E1 E1 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (7,12)/DIR_S?

```hex
02 06 09 11 01 18 00 43 fe 01 0f
```

```
00: show_text_block(str[6] "A magical fountain burbles at your / feet. Do you drink the water (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x43, and=0xFE, or=0x01)
04: end_script()
```

**Event 08** — triggers: (10,10)/DIR_W?

```hex
02 07 09 11 07 18 00 22 00 64 18 00 23 00 64 18 00 24 00 64 18 00 2a 00 64 18 00 2b 00 64 18 00 2c 00 64 18 00 2d 00 64 0f
```

```
00: show_text_block(str[7] "A rancid smell rises from a fetid / pool. Dare to sip the sewage (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(7)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x22, and=0x00, or=0x64)
04: apply_party_masked(count=0x00, set=0x23, and=0x00, or=0x64)
05: apply_party_masked(count=0x00, set=0x24, and=0x00, or=0x64)
06: apply_party_masked(count=0x00, set=0x2A, and=0x00, or=0x64)
07: apply_party_masked(count=0x00, set=0x2B, and=0x00, or=0x64)
08: apply_party_masked(count=0x00, set=0x2C, and=0x00, or=0x64)
09: apply_party_masked(count=0x00, set=0x2D, and=0x00, or=0x64)
10: end_script()
```

**Event 09** — triggers: (13,13)/DIR_N?

```hex
02 08 09 11 04 01 09 26 18 09 43 00 fe 1f 09 43 01 01 00 00 0f
```

```
00: show_text_block(str[8] "A pool of blood-like liquid oozes out / of a crevice. Drink (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(4)
    # skip -> end_script()
03: show_text_basic(str[9] "Who will drink (1-8)?")
04: selected = select_party_member()
05: apply_party_masked(count=0x09, set=0x43, and=0x00, or=0xFE)
06: party_effect(sel=0x09, 43 01 01 00 00)
07: end_script()
```

**Event 10** — triggers: (5,14)/ANY_DIR, (10,8)/ANY_DIR, (12,9)/ANY_DIR, (14,5)/ANY_DIR, (14,15)/ANY_DIR

```hex
1c 64 1b 1e 10 04 01 0a 0d 09 31 00 28 00 07 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(4)
    # skip -> end_script()
03: show_text_basic(str[10] "TIDAL WAVE!!!")
04: engine_call(0x09)
05: for_party(mask=0x00): call(0x28,0x00)
06: wait_for_space()
07: end_script()
```

**Event 11** — triggers: (12,7)/ANY_DIR

```hex
1c 64 1b 1e 10 04 01 0b 07 0d 09 0c 26 8a 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1E)
02: if cond: skip_tokens(4)
    # skip -> end_script()
03: show_text_basic(str[11] "WHIRLPOOL!!!")
04: wait_for_space()
05: engine_call(0x09)
06: map_transition(0x26, 0x8A)
07: end_script()
```

**Event 12** — triggers: (1,10)/ANY_DIR, (3,2)/ANY_DIR, (6,6)/ANY_DIR

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

**Event 13** — triggers: (2,10)/ANY_DIR, (3,3)/ANY_DIR, (4,7)/ANY_DIR

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

**Event 14** — triggers: (1,12)/ANY_DIR, (6,4)/ANY_DIR, (10,0)/ANY_DIR

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

## String table

- `[00]` <EMPTY>
- `[01]` You spy upon a lone seagull drifting / down to Atlantium. Enter (y/n)?
- `[02]` A wet mist drenches the party as you / stand before the passage to the / Elemental Plane of Water. Enter (y/n)?
- `[03]` Prisoners are staked to the ground. / Free them (y/n)?
- `[04]` By adventuring in the watery abyss, an / understanding with liquid has been / achieved. The spell Water Encasement / can now be cast by those with proper / ability.
- `[05]` Affixed to a table in the center of an / empty garrison, an old, rotted scroll / tells how to cast the spell Water / Transmutation.
- `[06]` A magical fountain burbles at your / feet. Do you drink the water (y/n)?
- `[07]` A rancid smell rises from a fetid / pool. Dare to sip the sewage (y/n)?
- `[08]` A pool of blood-like liquid oozes out / of a crevice. Drink (y/n)?
- `[09]` Who will drink (1-8)?
- `[10]` TIDAL WAVE!!!
- `[11]` WHIRLPOOL!!!
- `[12]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
