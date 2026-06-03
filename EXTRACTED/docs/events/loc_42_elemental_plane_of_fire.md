# Location 42 — Elemental Plane of Fire

- **event.dat** offset `0x00D65A`, length **847** bytes
- **Map:** map screen **42**; Elemental Plane of Fire
- **Record kind:** `standard`
- **Triggers:** 37; **script segments:** 8; **strings:** 11

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,6) | `0x06` | **3** | ANY_DIR |
| (1,2) | `0x12` | **3** | ANY_DIR |
| (1,7) | `0x17` | **3** | ANY_DIR |
| (2,1) | `0x21` | **3** | ANY_DIR |
| (2,3) | `0x23` | **3** | ANY_DIR |
| (2,12) | `0x2C` | **3** | ANY_DIR |
| (3,4) | `0x34` | **3** | ANY_DIR |
| (3,11) | `0x3B` | **3** | ANY_DIR |
| (3,13) | `0x3D` | **3** | ANY_DIR |
| (4,3) | `0x43` | **3** | ANY_DIR |
| (4,4) | `0x44` | **2** | ANY_DIR |
| (4,5) | `0x45` | **3** | ANY_DIR |
| (4,12) | `0x4C` | **3** | ANY_DIR |
| (5,4) | `0x54` | **3** | ANY_DIR |
| (5,8) | `0x58` | **3** | ANY_DIR |
| (6,2) | `0x62` | **3** | ANY_DIR |
| (6,11) | `0x6B` | **3** | ANY_DIR |
| (7,15) | `0x7F` | **4** | 0xA0 |
| (8,5) | `0x85` | **3** | ANY_DIR |
| (8,9) | `0x89` | **3** | ANY_DIR |
| (9,2) | `0x92` | **3** | ANY_DIR |
| (9,8) | `0x98` | **3** | ANY_DIR |
| (9,9) | `0x99` | **1** | ANY_DIR |
| (9,10) | `0x9A` | **3** | ANY_DIR |
| (10,7) | `0xA7` | **3** | ANY_DIR |
| (10,11) | `0xAB` | **3** | ANY_DIR |
| (10,12) | `0xAC` | **3** | ANY_DIR |
| (11,3) | `0xB3` | **3** | ANY_DIR |
| (11,6) | `0xB6` | **3** | ANY_DIR |
| (11,13) | `0xBD` | **3** | ANY_DIR |
| (11,14) | `0xBE` | **3** | ANY_DIR |
| (11,15) | `0xBF` | **3** | ANY_DIR |
| (12,5) | `0xC5` | **3** | ANY_DIR |
| (13,8) | `0xD8` | **3** | ANY_DIR |
| (14,14) | `0xEE` | **6** | ANY_DIR |
| (15,3) | `0xF3` | **3** | ANY_DIR |
| (15,6) | `0xF6` | **5** | 0x50 |

## Events

**Event 01** — triggers: (9,9)/ANY_DIR

```hex
2b 02 02 01 13 c1 c1 c1 c1 c1 c1 fc c1 c1 c1 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[1] "Lord Pyrannaste's voice crackles, / "You dare to invade my plane? / Die ")
02: encounter_setup_b(data=C1 C1 C1 C1 C1 C1 FC C1 C1 C1)
03: clear_current_tile_event_flag()
```

**Event 02** — triggers: (4,4)/ANY_DIR

```hex
28 00 e9 11 04 03 02 19 01 dd 01 00 07 14 03 03 29
```

```
00: cond = consume_item(item_id=233, name="Fire Disc", probe=0)
01: if not cond: skip_tokens(4)
    # skip -> show_text(str[3] "A small shrine of copper with / intricate cloisonne inlay, contains a / ")
02: show_text(str[2] "A shrine of red copper with intricate / cloisonne inlay, contains a dome")
03: add_party_entity(0x01, f3a=0xDD, f40=0x01, f46=0x00)
04: wait_for_space()
05: clear_current_tile_event_flag()
06: show_text(str[3] "A small shrine of copper with / intricate cloisonne inlay, contains a / ")
07: set_abort_and_exit()
```

**Event 03** — triggers: (0,6)/ANY_DIR, (1,2)/ANY_DIR, (1,7)/ANY_DIR, (2,1)/ANY_DIR, (2,3)/ANY_DIR, (2,12)/ANY_DIR, (3,4)/ANY_DIR, (3,11)/ANY_DIR, (3,13)/ANY_DIR, (4,3)/ANY_DIR, (4,5)/ANY_DIR, (4,12)/ANY_DIR, (5,4)/ANY_DIR, (5,8)/ANY_DIR, (6,2)/ANY_DIR, (6,11)/ANY_DIR, (8,5)/ANY_DIR, (8,9)/ANY_DIR, (9,2)/ANY_DIR, (9,8)/ANY_DIR, (9,10)/ANY_DIR, (10,7)/ANY_DIR, (10,11)/ANY_DIR, (10,12)/ANY_DIR, (11,3)/ANY_DIR, (11,6)/ANY_DIR, (11,13)/ANY_DIR, (11,14)/ANY_DIR, (11,15)/ANY_DIR, (12,5)/ANY_DIR, (13,8)/ANY_DIR, (15,3)/ANY_DIR

```hex
2b 01 12 c1 c1 c1 ce 68 68 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=C1 C1 C1 CE 68 68 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 04** — triggers: (7,15)/0xA0

```hex
06 05 02 06 29
```

```
00: show_text_popup_style_b(str[5] "Red / Message 9")
01: show_text_block(str[6] "hgs lolir, Mao y mdhair tn  / n s n.wit deon pvis tgh . J")
02: set_abort_and_exit()
```

**Event 05** — triggers: (15,6)/0x50

```hex
06 07 02 08 29
```

```
00: show_text_popup_style_b(str[7] "Red / Message 5")
01: show_text_block(str[8] "ociolu owoneBol Neiuamayl R / e whcal ts S fr or lap tt h")
02: set_abort_and_exit()
```

**Event 06** — triggers: (14,14)/ANY_DIR

```hex
17 29 00 11 01 14 02 09 07 0d 09 1a 84 09 0c 21 ef
```

```
00: cond = load_var8(group=0x29, index=0x00)
01: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[9] "You are not of this element. Be gone!")
02: clear_current_tile_event_flag()
03: show_text_block(str[9] "You are not of this element. Be gone!")
04: wait_for_space()
05: engine_call(0x09)
06: store_var8(group=0x84, value=0x09)
07: map_transition(0x21, 0xEF)
```

## String table

- `[00]` <EMPTY>
- `[01]` Lord Pyrannaste's voice crackles, / "You dare to invade my plane? / Die intruders!"
- `[02]` A shrine of red copper with intricate / cloisonne inlay, contains a domed / pedestal. You insert your Fire Disc / into the slot. The dome opens and you / take the Fire Talon that has rested / beneath it for decades.
- `[03]` A small shrine of copper with / intricate cloisonne inlay, contains a / domed pedestal. A small disc slot / appears to be your only way to open / it. None of your items fit into the / slot.
- `[04]` *
- `[05]` Red / Message 9
- `[06]` hgs lolir, Mao y mdhair tn  / n s n.wit deon pvis tgh . J
- `[07]` Red / Message 5
- `[08]` ociolu owoneBol Neiuamayl R / e whcal ts S fr or lap tt h
- `[09]` You are not of this element. Be gone!
- `[10]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
