# Location 37 — E3

- **event.dat** offset `0x00C14A`, length **869** bytes
- **Map:** map screen **37**; overland sector **E3**
- **Record kind:** `standard`
- **Triggers:** 47; **script segments:** 14; **strings:** 14

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,1) | `0x01` | **6** | ANY_DIR |
| (0,2) | `0x02` | **9** | ANY_DIR |
| (0,4) | `0x04` | **6** | ANY_DIR |
| (0,6) | `0x06` | **6** | ANY_DIR |
| (0,9) | `0x09` | **6** | ANY_DIR |
| (0,11) | `0x0B` | **6** | ANY_DIR |
| (0,13) | `0x0D` | **6** | ANY_DIR |
| (1,0) | `0x10` | **6** | ANY_DIR |
| (1,3) | `0x13` | **6** | ANY_DIR |
| (1,7) | `0x17` | **6** | ANY_DIR |
| (1,10) | `0x1A` | **6** | ANY_DIR |
| (1,12) | `0x1C` | **6** | ANY_DIR |
| (1,13) | `0x1D` | **9** | ANY_DIR |
| (1,14) | `0x1E` | **8** | ANY_DIR |
| (2,1) | `0x21` | **6** | ANY_DIR |
| (2,4) | `0x24` | **10** | ANY_DIR |
| (2,5) | `0x25` | **6** | ANY_DIR |
| (2,7) | `0x27` | **11** | ANY_DIR |
| (2,8) | `0x28` | **6** | ANY_DIR |
| (2,14) | `0x2E` | **6** | ANY_DIR |
| (3,0) | `0x30` | **12** | ANY_DIR |
| (3,2) | `0x32` | **8** | ANY_DIR |
| (3,3) | `0x33` | **6** | ANY_DIR |
| (3,9) | `0x39` | **10** | ANY_DIR |
| (3,10) | `0x3A` | **6** | ANY_DIR |
| (4,1) | `0x41` | **6** | ANY_DIR |
| (4,5) | `0x45` | **8** | ANY_DIR |
| (4,6) | `0x46` | **6** | ANY_DIR |
| (4,7) | `0x47` | **9** | ANY_DIR |
| (4,10) | `0x4A` | **7** | ANY_DIR |
| (4,12) | `0x4C` | **7** | ANY_DIR |
| (5,0) | `0x50` | **6** | ANY_DIR |
| (5,2) | `0x52` | **10** | ANY_DIR |
| (5,4) | `0x54` | **6** | ANY_DIR |
| (5,5) | `0x55` | **1** | ENTER+SPECIAL |
| (6,2) | `0x62` | **6** | 0xF1 |
| (6,5) | `0x65` | **2** | ANY_DIR |
| (6,11) | `0x6B` | **5** | ANY_DIR |
| (6,14) | `0x6E` | **7** | ANY_DIR |
| (8,4) | `0x84` | **7** | ANY_DIR |
| (8,6) | `0x86` | **5** | ANY_DIR |
| (9,11) | `0x9B` | **7** | ANY_DIR |
| (10,2) | `0xA2` | **5** | ANY_DIR |
| (12,2) | `0xC2` | **7** | ANY_DIR |
| (12,10) | `0xCA` | **3** | ANY_DIR |
| (13,10) | `0xDA` | **4** | ANY_DIR |
| (15,3) | `0xF3` | **7** | ANY_DIR |

## Events

**Event 01** — triggers: (5,5)/ENTER+SPECIAL

```hex
02 01 09 11 01 0c 20 7f 0f
```

```
00: show_text_block(str[1] "You detect a rough-hewn cave opening. / Enter (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x20, 0x7F)
04: end_script()
```

**Event 02** — triggers: (6,5)/ANY_DIR

```hex
2b 05 15 00 7a 20 11 01 14 02 06 12 f3 00 00 00 00 00 00 00 00 00 00 00 18 00 7a df 20 14
```

```
00: skip_tokens(5)
    # skip -> apply_party_masked(count=0x00, set=0x7A, and=0xDF, or=0x20)
01: apply_party(count=0x00, op=0x7A, val=0x20)
02: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[6] "The Slithering Envoy of Evil attacks!")
03: clear_current_tile_event_flag()
04: show_text_block(str[6] "The Slithering Envoy of Evil attacks!")
05: encounter_setup(monsters=F3 00 00 00 00 00 00 00 00 00, flags=00 00)
06: apply_party_masked(count=0x00, set=0x7A, and=0xDF, or=0x20)
07: clear_current_tile_event_flag()
```

**Event 03** — triggers: (12,10)/ANY_DIR

```hex
02 02 09 10 01 0f 15 00 7b 01 10 14 02 07 18 00 4b 00 00 18 00 4c 00 00 18 00 4d 00 00 18 00 4e 00 00 18 00 4f 00 00 18 00 50 00 00 18 00 57 00 00 18 00 58 00 00 18 00 59 00 00 18 00 5a 00 00 18 00 5b 00 00 18 00 5c 00 00 18 00 63 00 00 18 00 64 00 00 18 00 65 00 00 18 00 66 00 00 18 00 67 00 00 18 00 68 00 00 29 02 08 18 00 7b fc 02 07 14
```

```
00: show_text_block(str[2] "A cool sparkling pool. Bathe (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x00, op=0x7B, val=0x01)
03: end_script()
04: apply_party(count=0x00, op=0x7B, val=0x01)
05: if cond: skip_tokens(20)
    # skip -> show_text_block(str[8] "You feel like a winner now!")
06: show_text_block(str[7] "Acid melts the items in your backpack!")
07: apply_party_masked(count=0x00, set=0x4B, and=0x00, or=0x00)
08: apply_party_masked(count=0x00, set=0x4C, and=0x00, or=0x00)
09: apply_party_masked(count=0x00, set=0x4D, and=0x00, or=0x00)
10: apply_party_masked(count=0x00, set=0x4E, and=0x00, or=0x00)
11: apply_party_masked(count=0x00, set=0x4F, and=0x00, or=0x00)
12: apply_party_masked(count=0x00, set=0x50, and=0x00, or=0x00)
13: apply_party_masked(count=0x00, set=0x57, and=0x00, or=0x00)
14: apply_party_masked(count=0x00, set=0x58, and=0x00, or=0x00)
15: apply_party_masked(count=0x00, set=0x59, and=0x00, or=0x00)
16: apply_party_masked(count=0x00, set=0x5A, and=0x00, or=0x00)
17: apply_party_masked(count=0x00, set=0x5B, and=0x00, or=0x00)
18: apply_party_masked(count=0x00, set=0x5C, and=0x00, or=0x00)
19: apply_party_masked(count=0x00, set=0x63, and=0x00, or=0x00)
20: apply_party_masked(count=0x00, set=0x64, and=0x00, or=0x00)
21: apply_party_masked(count=0x00, set=0x65, and=0x00, or=0x00)
22: apply_party_masked(count=0x00, set=0x66, and=0x00, or=0x00)
23: apply_party_masked(count=0x00, set=0x67, and=0x00, or=0x00)
24: apply_party_masked(count=0x00, set=0x68, and=0x00, or=0x00)
25: set_abort_and_exit()
26: show_text_block(str[8] "You feel like a winner now!")
27: apply_party_masked(count=0x00, set=0x7B, and=0xFC, or=0x02)
28: wait_for_space()
29: clear_current_tile_event_flag()
```

**Event 04** — triggers: (13,10)/ANY_DIR

```hex
2b 01 12 7d 7d 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=7D 7D 00 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 05** — triggers: (6,11)/ANY_DIR, (8,6)/ANY_DIR, (10,2)/ANY_DIR

```hex
02 03 18 00 42 00 28 07 14
```

```
00: show_text_block(str[3] "A small oasis. You find 40 food.")
01: apply_party_masked(count=0x00, set=0x42, and=0x00, or=0x28)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 06** — triggers: (0,1)/ANY_DIR, (0,4)/ANY_DIR, (0,6)/ANY_DIR, (0,9)/ANY_DIR, (0,11)/ANY_DIR, (0,13)/ANY_DIR, (1,0)/ANY_DIR, (1,3)/ANY_DIR, (1,7)/ANY_DIR, (1,10)/ANY_DIR, (1,12)/ANY_DIR, (2,1)/ANY_DIR, (2,5)/ANY_DIR, (2,8)/ANY_DIR, (2,14)/ANY_DIR, (3,3)/ANY_DIR, (3,10)/ANY_DIR, (4,1)/ANY_DIR, (4,6)/ANY_DIR, (5,0)/ANY_DIR, (5,4)/ANY_DIR, (6,2)/0xF1

```hex
1c 64 1b 1f 11 01 0f 01 04 0d 09 31 00 14 00 29
```

```
00: op_1c_engine_query_to_cond(0x64)
01: cond = (cond >= 0x1F)
02: if not cond: skip_tokens(1)
    # skip -> show_text_basic(str[4] "An earthquake shakes the party!")
03: end_script()
04: show_text_basic(str[4] "An earthquake shakes the party!")
05: engine_call(0x09)
06: for_party(mask=0x00): call(0x14,0x00)
07: set_abort_and_exit()
```

**Event 07** — triggers: (4,10)/ANY_DIR, (4,12)/ANY_DIR, (6,14)/ANY_DIR, (8,4)/ANY_DIR, (9,11)/ANY_DIR, (12,2)/ANY_DIR, (15,3)/ANY_DIR

```hex
01 05 07 0d 09 31 00 14 00 0c a5 00
```

```
00: show_text_basic(str[5] "A sandstorm whips the party away!")
01: wait_for_space()
02: engine_call(0x09)
03: for_party(mask=0x00): call(0x14,0x00)
04: map_transition(0xA5, 0x00)
```

**Event 08** — triggers: (1,14)/ANY_DIR, (3,2)/ANY_DIR, (4,5)/ANY_DIR

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

**Event 09** — triggers: (0,2)/ANY_DIR, (1,13)/ANY_DIR, (4,7)/ANY_DIR

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

**Event 10** — triggers: (2,4)/ANY_DIR, (3,9)/ANY_DIR, (5,2)/ANY_DIR

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

**Event 11** — triggers: (2,7)/ANY_DIR

```hex
06 09 02 0a 29
```

```
00: show_text_popup_style_b(str[9] "Yellow / Message 1")
01: show_text_block(str[10] "sooucemekeumn'caysho hne a / oy tnkLih , Wior bs o isru")
02: set_abort_and_exit()
```

**Event 12** — triggers: (3,0)/ANY_DIR

```hex
06 0b 02 0c 29
```

```
00: show_text_popup_style_b(str[11] "Yellow / Message 8")
01: show_text_block(str[12] "ate  c snces br  oe thdw F / iv fthoot  irs s taythhied")
02: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` You detect a rough-hewn cave opening. / Enter (y/n)?
- `[02]` A cool sparkling pool. Bathe (y/n)?
- `[03]` A small oasis. You find 40 food.
- `[04]` An earthquake shakes the party!
- `[05]` A sandstorm whips the party away!
- `[06]` The Slithering Envoy of Evil attacks!
- `[07]` Acid melts the items in your backpack!
- `[08]` You feel like a winner now!
- `[09]` Yellow / Message 1
- `[10]` sooucemekeumn'caysho hne a / oy tnkLih , Wior bs o isru
- `[11]` Yellow / Message 8
- `[12]` ate  c snces br  oe thdw F / iv fthoot  irs s taythhied
- `[13]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
