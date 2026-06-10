# Location 25 — Sarakin's Mine

- **event.dat** offset `0x0086A1`, length **903** bytes
- **Map:** map screen **25**; Sarakin's Mine
- **Record kind:** `standard`
- **Triggers:** 48; **script segments:** 12; **strings:** 12

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,8) | `0x08` | **4** | DIR_S? |
| (1,8) | `0x18` | **3** | DIR_N? |
| (2,7) | `0x27` | **6** | ANY_DIR |
| (2,9) | `0x29` | **8** | ANY_DIR |
| (3,2) | `0x32` | **2** | ANY_DIR |
| (3,7) | `0x37` | **10** | DIR_E? |
| (4,2) | `0x42` | **1** | DIR_S? |
| (4,7) | `0x47` | **9** | ANY_DIR |
| (4,9) | `0x49` | **9** | ANY_DIR |
| (4,13) | `0x4D` | **9** | ANY_DIR |
| (5,0) | `0x50` | **8** | ANY_DIR |
| (5,6) | `0x56` | **9** | ANY_DIR |
| (5,7) | `0x57` | **9** | ANY_DIR |
| (5,9) | `0x59` | **9** | ANY_DIR |
| (5,10) | `0x5A` | **9** | ANY_DIR |
| (6,0) | `0x60` | **8** | ANY_DIR |
| (6,1) | `0x61` | **8** | ANY_DIR |
| (6,2) | `0x62` | **9** | ANY_DIR |
| (6,3) | `0x63` | **8** | ANY_DIR |
| (6,8) | `0x68` | **7** | DIR_N? |
| (6,11) | `0x6B` | **9** | ANY_DIR |
| (6,13) | `0x6D` | **8** | ANY_DIR |
| (7,0) | `0x70` | **8** | ANY_DIR |
| (7,1) | `0x71` | **8** | ANY_DIR |
| (7,3) | `0x73` | **8** | ANY_DIR |
| (7,6) | `0x76` | **9** | ANY_DIR |
| (7,7) | `0x77` | **9** | ANY_DIR |
| (7,9) | `0x79` | **9** | ANY_DIR |
| (7,10) | `0x7A` | **9** | ANY_DIR |
| (8,0) | `0x80` | **8** | ANY_DIR |
| (8,7) | `0x87` | **9** | ANY_DIR |
| (8,9) | `0x89` | **9** | ANY_DIR |
| (9,0) | `0x90` | **8** | ANY_DIR |
| (9,2) | `0x92` | **8** | ANY_DIR |
| (9,3) | `0x93` | **8** | ANY_DIR |
| (9,8) | `0x98` | **9** | ANY_DIR |
| (9,13) | `0x9D` | **8** | ANY_DIR |
| (10,2) | `0xA2` | **8** | ANY_DIR |
| (10,3) | `0xA3` | **8** | ANY_DIR |
| (10,4) | `0xA4` | **8** | ANY_DIR |
| (10,11) | `0xAB` | **9** | ANY_DIR |
| (11,9) | `0xB9` | **8** | ANY_DIR |
| (12,6) | `0xC6` | **9** | ANY_DIR |
| (14,10) | `0xEA` | **8** | ANY_DIR |
| (14,13) | `0xED` | **8** | ANY_DIR |
| (14,15) | `0xEF` | **8** | ANY_DIR |
| (15,0) | `0xF0` | **8** | ANY_DIR |
| (15,1) | `0xF1` | **5** | DIR_N? |

## Events

**Event 01** — triggers: (4,2)/DIR_S?

```hex
04 01
```

```
00: show_text_above_door(str[1] "Friends of Sarakin")
```

**Event 02** — triggers: (3,2)/ANY_DIR

```hex
2b 01 12 8d 8d 8d 8d 8d 8d 8c 8c 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=8D 8D 8D 8D 8D 8D 8C 8C 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 03** — triggers: (1,8)/DIR_N?

```hex
06 02
```

```
00: show_text_popup_style_b(str[2] "Beware / Cave-ins!")
```

**Event 04** — triggers: (0,8)/DIR_S?

```hex
02 03 09 11 01 0c 09 3c 0f
```

```
00: show_text_block(str[3] "Leave Sarakin's Mine (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x09, 0x3C)
04: end_script()
```

**Event 05** — triggers: (15,1)/DIR_N?

```hex
2b 01 12 e8 92 92 aa aa aa 93 93 93 00 00 00 02 04 09 11 22 15 01 2f 00 1b 18 11 01 18 01 2f 00 17 15 02 2f 00 1b 18 11 01 18 02 2f 00 17 15 03 2f 00 1b 18 11 01 18 03 2f 00 17 15 04 2f 00 1b 18 11 01 18 04 2f 00 17 15 05 2f 00 1b 18 11 01 18 05 2f 00 17 15 06 2f 00 1b 18 11 01 18 06 2f 00 17 15 07 2f 00 1b 18 11 01 18 07 2f 00 17 15 08 2f 00 1b 18 11 01 18 08 2f 00 17 02 05 07 14
```

```
00: skip_tokens(1)
    # skip -> show_text_block(str[4] "Sarakin's Secret Fountain appears / before you. Take a drink (y/n)?")
01: encounter_setup(monsters=E8 92 92 AA AA AA 93 93 93 00, flags=00 00)
02: show_text_block(str[4] "Sarakin's Secret Fountain appears / before you. Take a drink (y/n)?")
03: cond = prompt_yes_no()
04: if not cond: skip_tokens(34)
    # skip -> clear_current_tile_event_flag()
05: apply_party(count=0x01, op=0x2F, val=0x00)
06: cond = (cond >= 0x18)
07: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x2F, val=0x00)
08: apply_party_masked(count=0x01, set=0x2F, and=0x00, or=0x17)
09: apply_party(count=0x02, op=0x2F, val=0x00)
10: cond = (cond >= 0x18)
11: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x2F, val=0x00)
12: apply_party_masked(count=0x02, set=0x2F, and=0x00, or=0x17)
13: apply_party(count=0x03, op=0x2F, val=0x00)
14: cond = (cond >= 0x18)
15: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x2F, val=0x00)
16: apply_party_masked(count=0x03, set=0x2F, and=0x00, or=0x17)
17: apply_party(count=0x04, op=0x2F, val=0x00)
18: cond = (cond >= 0x18)
19: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x2F, val=0x00)
20: apply_party_masked(count=0x04, set=0x2F, and=0x00, or=0x17)
21: apply_party(count=0x05, op=0x2F, val=0x00)
22: cond = (cond >= 0x18)
23: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x2F, val=0x00)
24: apply_party_masked(count=0x05, set=0x2F, and=0x00, or=0x17)
25: apply_party(count=0x06, op=0x2F, val=0x00)
26: cond = (cond >= 0x18)
27: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x2F, val=0x00)
28: apply_party_masked(count=0x06, set=0x2F, and=0x00, or=0x17)
29: apply_party(count=0x07, op=0x2F, val=0x00)
30: cond = (cond >= 0x18)
31: if not cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x2F, val=0x00)
32: apply_party_masked(count=0x07, set=0x2F, and=0x00, or=0x17)
33: apply_party(count=0x08, op=0x2F, val=0x00)
34: cond = (cond >= 0x18)
35: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[5] "You feel rejuvenated!")
36: apply_party_masked(count=0x08, set=0x2F, and=0x00, or=0x17)
37: show_text_block(str[5] "You feel rejuvenated!")
38: wait_for_space()
39: clear_current_tile_event_flag()
```

**Event 06** — triggers: (2,7)/ANY_DIR

```hex
17 13 00 10 04 03 06 07 1a 13 01 1a 14 01 14
```

```
00: cond = load_var8(group=0x13, index=0x00)
01: if cond: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
02: show_text(str[6] "Sir Kill and Jed I jump at the sight / of your party. Trapped by the cav")
03: wait_for_space()
04: store_var8(group=0x13, value=0x01)
05: store_var8(group=0x14, value=0x01)
06: clear_current_tile_event_flag()
```

**Event 07** — triggers: (6,8)/DIR_N?

```hex
0b 06 00 01 07 08 0f
```

```
00: set_service_context(str[6] "Sir Kill and Jed I jump at the sight / of your party. Trapped by the cav", mode=0x00)
01: show_text_basic(str[7] ""I am Sarakin. Leave my home ore die!"")
02: wait_key()
03: end_script()
```

**Event 08** — triggers: (2,9)/ANY_DIR, (5,0)/ANY_DIR, (6,0)/ANY_DIR, (6,1)/ANY_DIR, (6,3)/ANY_DIR, (6,13)/ANY_DIR, (7,0)/ANY_DIR, (7,1)/ANY_DIR, (7,3)/ANY_DIR, (8,0)/ANY_DIR, (9,0)/ANY_DIR, (9,2)/ANY_DIR, (9,3)/ANY_DIR, (9,13)/ANY_DIR, (10,2)/ANY_DIR, (10,3)/ANY_DIR, (10,4)/ANY_DIR, (11,9)/ANY_DIR, (14,10)/ANY_DIR, (14,13)/ANY_DIR, (14,15)/ANY_DIR, (15,0)/ANY_DIR

```hex
01 08 07 2a d0 07 00 00 00 00 00 00 00 00 00 00 00 00 14
```

```
00: show_text_basic(str[8] "Gold dust is scattered on the floor.")
01: wait_for_space()
02: set_treasure(gold/exp=2000, gems=0, items=[0, 0, 0])
03: clear_current_tile_event_flag()
```

**Event 09** — triggers: (4,7)/ANY_DIR, (4,9)/ANY_DIR, (4,13)/ANY_DIR, (5,6)/ANY_DIR, (5,7)/ANY_DIR, (5,9)/ANY_DIR, (5,10)/ANY_DIR, (6,2)/ANY_DIR, (6,11)/ANY_DIR, (7,6)/ANY_DIR, (7,7)/ANY_DIR, (7,9)/ANY_DIR, (7,10)/ANY_DIR, (8,7)/ANY_DIR, (8,9)/ANY_DIR, (9,8)/ANY_DIR, (10,11)/ANY_DIR, (12,6)/ANY_DIR

```hex
2b 04 01 09 07 31 00 14 00 12 5d 5d 5d 5d 5d 5d 00 00 00 00 00 00 14
```

```
00: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
01: show_text_basic(str[9] "CAVE-IN!")
02: wait_for_space()
03: for_party(mask=0x00): call(0x14,0x00)
04: encounter_setup(monsters=5D 5D 5D 5D 5D 5D 00 00 00 00, flags=00 00)
05: clear_current_tile_event_flag()
```

**Event 10** — triggers: (3,7)/DIR_E?

```hex
15 00 77 20 10 03 02 0a 07 0c 19 36 0f
```

```
00: apply_party(count=0x00, op=0x77, val=0x20)
01: if cond: skip_tokens(3)
    # skip -> end_script()
02: show_text_block(str[10] "A cave-in has rendered the passage / too small for you to enter. Perhaps")
03: wait_for_space()
04: map_transition(0x19, 0x36)
05: end_script()
```

## String table

- `[00]` <EMPTY>
- `[01]` Friends of Sarakin
- `[02]` Beware / Cave-ins!
- `[03]` Leave Sarakin's Mine (y/n)?
- `[04]` Sarakin's Secret Fountain appears / before you. Take a drink (y/n)?
- `[05]` You feel rejuvenated!
- `[06]` Sir Kill and Jed I jump at the sight / of your party. Trapped by the cave-in, / they had to wait for rescue while / slowly starving. Hire the rejuvenated / pair at the Tundaran Arms Inn.
- `[07]` "I am Sarakin. Leave my home ore die!"
- `[08]` Gold dust is scattered on the floor.
- `[09]` CAVE-IN!
- `[10]` A cave-in has rendered the passage / too small for you to enter. Perhaps a / diet is in order.
- `[11]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
