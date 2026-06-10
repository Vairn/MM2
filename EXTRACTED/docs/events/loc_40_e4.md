# Location 40 — E4

- **event.dat** offset `0x00CEA7`, length **1215** bytes
- **Map:** map screen **40**; overland sector **E4**
- **Record kind:** `standard`
- **Triggers:** 35; **script segments:** 15; **strings:** 13

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,15) | `0x0F` | **2** | 0x60 |
| (1,6) | `0x16` | **5** | ANY_DIR |
| (1,14) | `0x1E` | **3** | ANY_DIR |
| (2,9) | `0x29` | **5** | ANY_DIR |
| (3,13) | `0x3D` | **5** | ANY_DIR |
| (5,1) | `0x51` | **9** | ANY_DIR |
| (6,1) | `0x61` | **8** | ANY_DIR |
| (6,10) | `0x6A` | **5** | ANY_DIR |
| (6,13) | `0x6D` | **5** | ANY_DIR |
| (7,1) | `0x71` | **10** | ANY_DIR |
| (7,5) | `0x75` | **9** | ANY_DIR |
| (8,6) | `0x86` | **11** | ANY_DIR |
| (8,8) | `0x88` | **4** | ANY_DIR |
| (9,13) | `0x9D` | **5** | ANY_DIR |
| (10,0) | `0xA0` | **11** | ANY_DIR |
| (10,3) | `0xA3` | **6** | ANY_DIR |
| (10,4) | `0xA4` | **1** | DIR_E? |
| (11,2) | `0xB2` | **12** | ANY_DIR |
| (11,3) | `0xB3` | **10** | ANY_DIR |
| (11,8) | `0xB8` | **7** | ANY_DIR |
| (12,0) | `0xC0` | **7** | ANY_DIR |
| (12,6) | `0xC6` | **7** | ANY_DIR |
| (13,2) | `0xD2` | **7** | ANY_DIR |
| (13,4) | `0xD4` | **7** | ANY_DIR |
| (13,9) | `0xD9` | **7** | ANY_DIR |
| (14,4) | `0xE4` | **7** | ANY_DIR |
| (14,7) | `0xE7` | **7** | ANY_DIR |
| (14,11) | `0xEB` | **7** | ANY_DIR |
| (14,12) | `0xEC` | **9** | ANY_DIR |
| (15,0) | `0xF0` | **7** | ANY_DIR |
| (15,7) | `0xF7` | **13** | ANY_DIR |
| (15,9) | `0xF9` | **7** | ANY_DIR |
| (15,11) | `0xFB` | **11** | ANY_DIR |
| (15,12) | `0xFC` | **7** | ANY_DIR |
| (15,13) | `0xFD` | **10** | ANY_DIR |

## Events

**Event 01** — triggers: (10,4)/DIR_E?

```hex
0b 17 00 02 01 0a 11 01 0c 04 e0 0f
```

```
00: set_service_context(str[23], mode=0x00)
01: show_text_block(str[1] "In a secluded grove of trees sits / Sandsobar. Enter (y/n)?")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x04, 0xE0)
05: end_script()
```

**Event 02** — triggers: (0,15)/0x60

```hex
02 02 09 10 01 0f 1a 84 04 0c 2b 0f
```

```
00: show_text_block(str[2] "A hole full of quaking rocks, soil and / sand defines a passage to the t")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> store_var8(group=0x84, value=0x04)
03: end_script()
04: store_var8(group=0x84, value=0x04)
05: map_transition(0x2B, 0x0F)
```

**Event 03** — triggers: (1,14)/ANY_DIR

```hex
02 03 2e f2 10 07 14
```

```
00: show_text_block(str[3] "After traveling through the elemental / plane of earth, an understanding")
01: set_party_attr(class=0xF2, bits=0x10)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 04** — triggers: (8,8)/ANY_DIR

```hex
03 04 2e f1 80 07 14
```

```
00: show_text(str[4] "Among the ruins of a deserted outpost / a tablet is found. Those amongst")
01: set_party_attr(class=0xF1, bits=0x80)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 05** — triggers: (1,6)/ANY_DIR, (2,9)/ANY_DIR, (3,13)/ANY_DIR, (6,10)/ANY_DIR, (6,13)/ANY_DIR, (9,13)/ANY_DIR

```hex
2b 01 12 e0 e0 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=E0 E0 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 06** — triggers: (10,3)/ANY_DIR

```hex
2b 04 15 00 77 10 11 02 02 05 12 5e 5e 5e 5e 5e 5e 5e 5e 59 00 00 00 14
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: apply_party(count=0x00, op=0x77, val=0x10)
02: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
03: show_text_block(str[5] "Outraged at the tell-tale odor from a / belch let loose by one of you, m")
04: encounter_setup(monsters=5E 5E 5E 5E 5E 5E 5E 5E 59 00, flags=00 00)
05: clear_current_tile_event_flag()
```

**Event 07** — triggers: (11,8)/ANY_DIR, (12,0)/ANY_DIR, (12,6)/ANY_DIR, (13,2)/ANY_DIR, (13,4)/ANY_DIR, (13,9)/ANY_DIR, (14,4)/ANY_DIR, (14,7)/ANY_DIR, (14,11)/ANY_DIR, (15,0)/ANY_DIR, (15,9)/ANY_DIR, (15,12)/ANY_DIR

```hex
1c 64 1b 1f 10 02 01 06 31 00 14 00 29
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1F)
02: if cond: skip_tokens(2)
    # skip -> set_abort_and_exit()
03: show_text_basic(str[6] "An earthquake shakes the party!")
04: for_party(mask=0x00): call(0x14,0x00)
05: set_abort_and_exit()
```

**Event 08** — triggers: (6,1)/ANY_DIR

```hex
02 07 09 11 18 15 01 43 80 10 01 18 01 43 00 00 15 02 43 80 10 01 18 02 43 00 00 15 03 43 80 10 01 18 03 43 00 00 15 04 43 80 10 01 18 04 43 00 00 15 05 43 80 10 01 18 05 43 00 00 15 06 43 80 10 01 18 06 43 00 00 15 07 43 80 10 01 18 07 43 00 00 15 08 43 80 10 01 18 08 43 00 00 0f
```

```
00: show_text_block(str[7] "This fountain is famed for its healing / water. Drink (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(24)
    # skip -> end_script()
03: apply_party(count=0x01, op=0x43, val=0x80)
04: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x43, val=0x80)
05: apply_party_masked(count=0x01, set=0x43, and=0x00, or=0x00)
06: apply_party(count=0x02, op=0x43, val=0x80)
07: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x43, val=0x80)
08: apply_party_masked(count=0x02, set=0x43, and=0x00, or=0x00)
09: apply_party(count=0x03, op=0x43, val=0x80)
10: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x43, val=0x80)
11: apply_party_masked(count=0x03, set=0x43, and=0x00, or=0x00)
12: apply_party(count=0x04, op=0x43, val=0x80)
13: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x43, val=0x80)
14: apply_party_masked(count=0x04, set=0x43, and=0x00, or=0x00)
15: apply_party(count=0x05, op=0x43, val=0x80)
16: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x43, val=0x80)
17: apply_party_masked(count=0x05, set=0x43, and=0x00, or=0x00)
18: apply_party(count=0x06, op=0x43, val=0x80)
19: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x43, val=0x80)
20: apply_party_masked(count=0x06, set=0x43, and=0x00, or=0x00)
21: apply_party(count=0x07, op=0x43, val=0x80)
22: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x43, val=0x80)
23: apply_party_masked(count=0x07, set=0x43, and=0x00, or=0x00)
24: apply_party(count=0x08, op=0x43, val=0x80)
25: if cond: skip_tokens(1)
    # skip -> end_script()
26: apply_party_masked(count=0x08, set=0x43, and=0x00, or=0x00)
27: end_script()
```

**Event 09** — triggers: (5,1)/ANY_DIR, (7,5)/ANY_DIR, (14,12)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 c2 c1 c2 e0 e1 e1 c1 c1 c1 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=C2 C1 C2 E0 E1 E1 C1 C1 C1 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 10** — triggers: (7,1)/ANY_DIR, (11,3)/ANY_DIR, (15,13)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 ce ce c2 c1 c2 c2 c1 e1 c2 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE CE C2 C1 C2 C2 C1 E1 C2 00)
04: clear_current_tile_event_flag()
```

**Event 11** — triggers: (8,6)/ANY_DIR, (10,0)/ANY_DIR, (15,11)/ANY_DIR

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

**Event 12** — triggers: (11,2)/ANY_DIR

```hex
06 08 02 09 29
```

```
00: show_text_popup_style_b(str[8] "Yellow / Message 2")
01: show_text_block(str[9] "stikthouhaizelfean bn s e. /  gorm  p iithilytople  wat")
02: set_abort_and_exit()
```

**Event 13** — triggers: (15,7)/ANY_DIR

```hex
06 0a 02 0b 29
```

```
00: show_text_popup_style_b(str[10] "Yellow / Message 5")
01: show_text_block(str[11] "pcsnhs d int Yoe an Hldd  / heou Dnehoacolw , iresstou")
02: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` In a secluded grove of trees sits / Sandsobar. Enter (y/n)?
- `[02]` A hole full of quaking rocks, soil and / sand defines a passage to the time of / Earth. Enter (y/n)?
- `[03]` After traveling through the elemental / plane of earth, an understanding of / the spell Earth Encasement has been / achieved by those with proper ability.
- `[04]` Among the ruins of a deserted outpost / a tablet is found. Those amongst your / party, with clerical ability, / comprehend the writings to be the / spell Earth Transmutation.
- `[05]` Outraged at the tell-tale odor from a / belch let loose by one of you, mad / peasants attack seeking revenge / for their roasted friend.
- `[06]` An earthquake shakes the party!
- `[07]` This fountain is famed for its healing / water. Drink (y/n)?
- `[08]` Yellow / Message 2
- `[09]` stikthouhaizelfean bn s e. /  gorm  p iithilytople  wat
- `[10]` Yellow / Message 5
- `[11]`  pcsnhs d int Yoe an Hldd  / heou Dnehoacolw , iresstou
- `[12]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
