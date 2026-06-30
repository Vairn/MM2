# Location 33 — E1

- **event.dat** offset `0x00AF55`, length **1324** bytes
- **Map:** map screen **33**; overland sector **E1**
- **Record kind:** `standard`
- **Triggers:** 60; **script segments:** 19; **strings:** 16

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,1) | `0x11` | **17** | ANY_DIR |
| (1,3) | `0x13` | **14** | ANY_DIR |
| (1,4) | `0x14` | **16** | ANY_DIR |
| (1,5) | `0x15` | **14** | ANY_DIR |
| (1,7) | `0x17` | **8** | ANY_DIR |
| (1,8) | `0x18` | **8** | ANY_DIR |
| (1,9) | `0x19` | **8** | ANY_DIR |
| (1,11) | `0x1B` | **8** | ANY_DIR |
| (1,12) | `0x1C` | **8** | ANY_DIR |
| (2,5) | `0x25` | **15** | ANY_DIR |
| (2,7) | `0x27` | **8** | ANY_DIR |
| (2,8) | `0x28` | **14** | ANY_DIR |
| (2,9) | `0x29` | **8** | ANY_DIR |
| (2,11) | `0x2B` | **11** | DIR_N? |
| (3,0) | `0x30` | **17** | ANY_DIR |
| (3,1) | `0x31` | **15** | ANY_DIR |
| (3,2) | `0x32` | **7** | ANY_DIR |
| (3,3) | `0x33` | **10** | DIR_E? |
| (3,7) | `0x37` | **8** | ANY_DIR |
| (3,8) | `0x38` | **8** | ANY_DIR |
| (3,9) | `0x39` | **8** | ANY_DIR |
| (4,3) | `0x43` | **1** | DIR_N? |
| (4,5) | `0x45` | **9** | DIR_N? |
| (5,1) | `0x51` | **13** | DIR_S? |
| (5,5) | `0x55` | **8** | ANY_DIR |
| (5,6) | `0x56` | **8** | ANY_DIR |
| (5,8) | `0x58` | **15** | ANY_DIR |
| (5,13) | `0x5D` | **6** | ANY_DIR |
| (6,1) | `0x61` | **8** | ANY_DIR |
| (6,3) | `0x63` | **8** | ANY_DIR |
| (6,4) | `0x64` | **8** | ANY_DIR |
| (6,5) | `0x65` | **8** | ANY_DIR |
| (6,6) | `0x66` | **14** | ANY_DIR |
| (6,7) | `0x67` | **17** | ANY_DIR |
| (7,1) | `0x71` | **8** | ANY_DIR |
| (7,2) | `0x72` | **8** | ANY_DIR |
| (7,3) | `0x73` | **8** | ANY_DIR |
| (7,4) | `0x74` | **2** | ANY_DIR |
| (7,5) | `0x75` | **8** | ANY_DIR |
| (7,6) | `0x76` | **8** | ANY_DIR |
| (8,1) | `0x81` | **8** | ANY_DIR |
| (8,3) | `0x83` | **8** | ANY_DIR |
| (8,4) | `0x84` | **8** | ANY_DIR |
| (8,5) | `0x85` | **8** | ANY_DIR |
| (8,8) | `0x88` | **5** | DIR_N?+DIR_E? |
| (8,14) | `0x8E` | **6** | ANY_DIR |
| (9,4) | `0x94` | **16** | ANY_DIR |
| (10,1) | `0xA1` | **8** | ANY_DIR |
| (10,2) | `0xA2` | **8** | ANY_DIR |
| (10,3) | `0xA3` | **12** | ANY_DIR |
| (11,1) | `0xB1` | **8** | ANY_DIR |
| (11,2) | `0xB2` | **8** | ANY_DIR |
| (11,9) | `0xB9` | **6** | ANY_DIR |
| (11,13) | `0xBD` | **6** | ANY_DIR |
| (12,0) | `0xC0` | **16** | ANY_DIR |
| (12,1) | `0xC1` | **8** | ANY_DIR |
| (14,4) | `0xE4` | **6** | ANY_DIR |
| (14,8) | `0xE8` | **6** | ANY_DIR |
| (14,14) | `0xEE` | **4** | ANY_DIR |
| (15,15) | `0xFF` | **3** | DIR_N?+DIR_E? |

## Events

**Event 01** — triggers: (4,3)/DIR_N?

```hex
0b 17 00 02 01 0a 11 01 0c 03 05 0f
```

```
00: service_sign(idx=0x17 -> sign 3 [3.anm], pos=0x00)
01: show_text_block(str[1] "Surrounded by porous crags, Vulcania / swelters before you. Enter (y/n)?")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x03, 0x05)
05: end_script()
```

**Event 02** — triggers: (7,4)/ANY_DIR

```hex
02 02 09 11 01 0c 1f f0 0f
```

```
00: show_text_block(str[2] "A churning sound resonates from the / man-made cave opening of Gemmaker ")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x1F, 0xF0)
04: end_script()
```

**Event 03** — triggers: (15,15)/DIR_N?+DIR_E?

```hex
02 03 09 10 01 0f 1a 84 03 0c 2a ee
```

```
00: show_text_block(str[3] "Incredible heat radiates from the / passage to the time of Fire. / Enter")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> store_var8(group=0x84, value=0x03)
03: end_script()
04: store_var8(group=0x84, value=0x03)
05: map_transition(0x2A, 0xEE)
```

**Event 04** — triggers: (14,14)/ANY_DIR

```hex
02 04 2e f3 01 07 14
```

```
00: show_text_block(str[4] "Perpetually surrounded by fire, your / cleric devises a new form of atta")
01: set_party_attr(class=0xF3, bits=0x01)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 05** — triggers: (8,8)/DIR_N?+DIR_E?

```hex
02 05 2e f3 02 07 14
```

```
00: show_text_block(str[5] "Inscribed upon a plaque in an old fort / overlooking the deadly elementa")
01: set_party_attr(class=0xF3, bits=0x02)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 06** — triggers: (5,13)/ANY_DIR, (8,14)/ANY_DIR, (11,9)/ANY_DIR, (11,13)/ANY_DIR, (14,4)/ANY_DIR, (14,8)/ANY_DIR

```hex
2b 01 13 c1 c1 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup_b(data=C1 C1 00 00 00 00 00 00 00 00)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (3,2)/ANY_DIR

```hex
2b 05 15 00 78 08 10 01 14 02 06 12 03 03 03 03 03 03 03 03 03 03 03 5a 14
```

```
00: skip_tokens(5)
    # skip -> clear_current_tile_event_flag()
01: apply_party(count=0x00, op=0x78, val=0x08)
02: if cond: skip_tokens(1)
    # skip -> show_text_block(str[6] "Furious at the Cream of Kobold stains / on your clothes, vengeful kobold")
03: clear_current_tile_event_flag()
04: show_text_block(str[6] "Furious at the Cream of Kobold stains / on your clothes, vengeful kobold")
05: encounter_setup(monsters=03 03 03 03 03 03 03 03 03 03, flags=03 5A)
06: clear_current_tile_event_flag()
```

**Event 08** — triggers: (1,7)/ANY_DIR, (1,8)/ANY_DIR, (1,9)/ANY_DIR, (1,11)/ANY_DIR, (1,12)/ANY_DIR, (2,7)/ANY_DIR, (2,9)/ANY_DIR, (3,7)/ANY_DIR, (3,8)/ANY_DIR, (3,9)/ANY_DIR, (5,5)/ANY_DIR, (5,6)/ANY_DIR, (6,1)/ANY_DIR, (6,3)/ANY_DIR, (6,4)/ANY_DIR, (6,5)/ANY_DIR, (7,1)/ANY_DIR, (7,2)/ANY_DIR, (7,3)/ANY_DIR, (7,5)/ANY_DIR, (7,6)/ANY_DIR, (8,1)/ANY_DIR, (8,3)/ANY_DIR, (8,4)/ANY_DIR, (8,5)/ANY_DIR, (10,1)/ANY_DIR, (10,2)/ANY_DIR, (11,1)/ANY_DIR, (11,2)/ANY_DIR, (12,1)/ANY_DIR

```hex
01 07 0d 09 17 23 00 10 02 31 00 32 00 29 02 0e 29
```

```
00: show_text_basic(str[7] "A pool of bubbling lava!")
01: engine_call(0x09)
02: cond = load_var8(group=0x23, index=0x00)
03: if cond: skip_tokens(2)
    # skip -> show_text_block(str[14] "/    Your levitation spell saved you!")
04: for_party(mask=0x00): call(0x32,0x00)
05: set_abort_and_exit()
06: show_text_block(str[14] "/    Your levitation spell saved you!")
07: set_abort_and_exit()
```

**Event 09** — triggers: (4,5)/DIR_N?

```hex
06 08
```

```
00: show_text_popup_style_b(str[8] "Danger / Lava!")
```

**Event 10** — triggers: (3,3)/DIR_E?

```hex
06 09
```

```
00: show_text_popup_style_b(str[9] "Vulcania / <-")
```

**Event 11** — triggers: (2,11)/DIR_N?

```hex
02 0a 09 11 02 1c 08 18 09 43 00 81 0f
```

```
00: show_text_block(str[10] "An unusual odor rises from an exotic / drinking fountain. Take a sip (y/")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: op_1c_engine_query_to_cond(0x08)
04: apply_party_masked(count=0x09, set=0x43, and=0x00, or=0x81)
05: end_script()
```

**Event 12** — triggers: (10,3)/ANY_DIR

```hex
02 0a 09 11 20 15 01 43 00 1b fe 10 01 18 01 43 00 00 15 02 43 00 1b fe 10 01 18 02 43 00 00 15 04 43 00 1b fe 10 01 18 04 43 00 00 15 05 43 00 1b fe 10 01 18 05 43 00 00 15 06 43 00 1b fe 10 01 18 06 43 00 00 15 07 43 00 1b fe 10 01 18 07 43 00 00 15 08 43 00 1b fe 10 01 18 08 43 00 00 15 03 43 00 1b fe 10 01 18 03 43 00 00 0f
```

```
00: show_text_block(str[10] "An unusual odor rises from an exotic / drinking fountain. Take a sip (y/")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(32)
    # skip -> end_script()
03: apply_party(count=0x01, op=0x43, val=0x00)
04: cond = (cond >= 0xFE)
05: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x43, val=0x00)
06: apply_party_masked(count=0x01, set=0x43, and=0x00, or=0x00)
07: apply_party(count=0x02, op=0x43, val=0x00)
08: cond = (cond >= 0xFE)
09: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x43, val=0x00)
10: apply_party_masked(count=0x02, set=0x43, and=0x00, or=0x00)
11: apply_party(count=0x04, op=0x43, val=0x00)
12: cond = (cond >= 0xFE)
13: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x43, val=0x00)
14: apply_party_masked(count=0x04, set=0x43, and=0x00, or=0x00)
15: apply_party(count=0x05, op=0x43, val=0x00)
16: cond = (cond >= 0xFE)
17: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x43, val=0x00)
18: apply_party_masked(count=0x05, set=0x43, and=0x00, or=0x00)
19: apply_party(count=0x06, op=0x43, val=0x00)
20: cond = (cond >= 0xFE)
21: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x43, val=0x00)
22: apply_party_masked(count=0x06, set=0x43, and=0x00, or=0x00)
23: apply_party(count=0x07, op=0x43, val=0x00)
24: cond = (cond >= 0xFE)
25: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x43, val=0x00)
26: apply_party_masked(count=0x07, set=0x43, and=0x00, or=0x00)
27: apply_party(count=0x08, op=0x43, val=0x00)
28: cond = (cond >= 0xFE)
29: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x43, val=0x00)
30: apply_party_masked(count=0x08, set=0x43, and=0x00, or=0x00)
31: apply_party(count=0x03, op=0x43, val=0x00)
32: cond = (cond >= 0xFE)
33: if cond: skip_tokens(1)
    # skip -> end_script()
34: apply_party_masked(count=0x03, set=0x43, and=0x00, or=0x00)
35: end_script()
```

**Event 13** — triggers: (5,1)/DIR_S?

```hex
02 0a 09 11 08 18 00 24 00 28 18 00 26 00 14 18 00 2a 00 28 18 00 2b 00 28 18 00 2c 00 28 18 00 2d 00 28 18 00 22 00 28 18 00 23 00 28 0f
```

```
00: show_text_block(str[10] "An unusual odor rises from an exotic / drinking fountain. Take a sip (y/")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(8)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x24, and=0x00, or=0x28)
04: apply_party_masked(count=0x00, set=0x26, and=0x00, or=0x14)
05: apply_party_masked(count=0x00, set=0x2A, and=0x00, or=0x28)
06: apply_party_masked(count=0x00, set=0x2B, and=0x00, or=0x28)
07: apply_party_masked(count=0x00, set=0x2C, and=0x00, or=0x28)
08: apply_party_masked(count=0x00, set=0x2D, and=0x00, or=0x28)
09: apply_party_masked(count=0x00, set=0x22, and=0x00, or=0x28)
10: apply_party_masked(count=0x00, set=0x23, and=0x00, or=0x28)
11: end_script()
```

**Event 14** — triggers: (1,3)/ANY_DIR, (1,5)/ANY_DIR, (2,8)/ANY_DIR, (6,6)/ANY_DIR

```hex
1c 64 1b 1f 10 04 02 0d 31 00 c8 00 0d 09 29 0f
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1F)
02: if cond: skip_tokens(4)
    # skip -> end_script()
03: show_text_block(str[13] "Hot lava spews violently from the / erupting volcano.")
04: for_party(mask=0x00): call(0xC8,0x00)
05: engine_call(0x09)
06: set_abort_and_exit()
07: end_script()
```

**Event 15** — triggers: (2,5)/ANY_DIR, (3,1)/ANY_DIR, (5,8)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 e1 e0 e1 c1 c1 c1 c1 c2 c2 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=E1 E0 E1 C1 C1 C1 C1 C2 C2 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 16** — triggers: (1,4)/ANY_DIR, (9,4)/ANY_DIR, (12,0)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 ce c2 c1 e1 c1 c2 ce c2 c2 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE C2 C1 E1 C1 C2 CE C2 C2 00)
04: clear_current_tile_event_flag()
```

**Event 17** — triggers: (1,1)/ANY_DIR, (3,0)/ANY_DIR, (6,7)/ANY_DIR

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
- `[01]` Surrounded by porous crags, Vulcania / swelters before you. Enter (y/n)?
- `[02]` A churning sound resonates from the / man-made cave opening of Gemmaker / Volcano. Enter (y/n)?
- `[03]` Incredible heat radiates from the / passage to the time of Fire. / Enter (y/n)?
- `[04]` Perpetually surrounded by fire, your / cleric devises a new form of attack. / Fire Encasement will devastate future / enemies!
- `[05]` Inscribed upon a plaque in an old fort / overlooking the deadly elemental plane / is the spell Fire Transmutation.
- `[06]` Furious at the Cream of Kobold stains / on your clothes, vengeful kobolds / attack!
- `[07]` A pool of bubbling lava!
- `[08]` Danger / Lava!
- `[09]` Vulcania / <-
- `[10]` An unusual odor rises from an exotic / drinking fountain. Take a sip (y/n)?
- `[11]` <SENTINEL_Z>
- `[12]` <SENTINEL_Z>
- `[13]` Hot lava spews violently from the / erupting volcano.
- `[14]`  /    Your levitation spell saved you!
- `[15]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
