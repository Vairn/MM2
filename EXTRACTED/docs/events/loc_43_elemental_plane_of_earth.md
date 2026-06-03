# Location 43 — Elemental Plane of Earth

- **event.dat** offset `0x00D9A9`, length **907** bytes
- **Map:** map screen **43**; Elemental Plane of Earth
- **Record kind:** `standard`
- **Triggers:** 48; **script segments:** 9; **strings:** 13

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,4) | `0x04` | **3** | ANY_DIR |
| (0,7) | `0x07` | **5** | 0x50 |
| (0,9) | `0x09` | **3** | ANY_DIR |
| (0,15) | `0x0F` | **7** | ANY_DIR |
| (1,13) | `0x1D` | **3** | ANY_DIR |
| (2,0) | `0x20` | **3** | ANY_DIR |
| (2,4) | `0x24` | **3** | ANY_DIR |
| (2,6) | `0x26` | **3** | ANY_DIR |
| (3,2) | `0x32` | **3** | ANY_DIR |
| (3,7) | `0x37` | **3** | ANY_DIR |
| (3,11) | `0x3B` | **3** | ANY_DIR |
| (4,4) | `0x44` | **3** | ANY_DIR |
| (4,12) | `0x4C` | **3** | ANY_DIR |
| (5,2) | `0x52` | **3** | ANY_DIR |
| (5,3) | `0x53` | **3** | ANY_DIR |
| (5,4) | `0x54` | **1** | ANY_DIR |
| (5,5) | `0x55` | **3** | ANY_DIR |
| (5,8) | `0x58` | **3** | ANY_DIR |
| (5,9) | `0x59` | **3** | ANY_DIR |
| (5,13) | `0x5D` | **3** | ANY_DIR |
| (6,4) | `0x64` | **3** | ANY_DIR |
| (6,9) | `0x69` | **6** | 0xA0 |
| (6,10) | `0x6A` | **3** | ANY_DIR |
| (7,3) | `0x73` | **3** | ANY_DIR |
| (7,8) | `0x78` | **3** | ANY_DIR |
| (7,11) | `0x7B` | **3** | ANY_DIR |
| (8,1) | `0x81` | **3** | ANY_DIR |
| (8,5) | `0x85` | **3** | ANY_DIR |
| (8,7) | `0x87` | **3** | ANY_DIR |
| (8,8) | `0x88` | **2** | ANY_DIR |
| (8,9) | `0x89` | **3** | ANY_DIR |
| (8,12) | `0x8C` | **3** | ANY_DIR |
| (8,15) | `0x8F` | **4** | 0xA0 |
| (9,8) | `0x98` | **3** | ANY_DIR |
| (9,13) | `0x9D` | **3** | ANY_DIR |
| (10,3) | `0xA3` | **3** | ANY_DIR |
| (10,6) | `0xA6` | **3** | ANY_DIR |
| (10,7) | `0xA7` | **3** | ANY_DIR |
| (10,10) | `0xAA` | **3** | ANY_DIR |
| (11,9) | `0xB9` | **3** | ANY_DIR |
| (11,13) | `0xBD` | **3** | ANY_DIR |
| (12,1) | `0xC1` | **3** | ANY_DIR |
| (12,4) | `0xC4` | **3** | ANY_DIR |
| (12,12) | `0xCC` | **3** | ANY_DIR |
| (13,8) | `0xD8` | **3** | ANY_DIR |
| (13,11) | `0xDB` | **3** | ANY_DIR |
| (14,2) | `0xE2` | **3** | ANY_DIR |
| (14,14) | `0xEE` | **3** | ANY_DIR |

## Events

**Event 01** — triggers: (5,4)/ANY_DIR

```hex
2b 02 02 01 12 e0 e0 e0 e0 e0 e0 fe e0 e0 e0 00 00 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[1] "Lord Gralkor exclaims, "How dare you / invade my plane! Die intruders!"")
02: encounter_setup(monsters=E0 E0 E0 E0 E0 E0 FE E0 E0 E0, flags=00 00)
03: clear_current_tile_event_flag()
```

**Event 02** — triggers: (8,8)/ANY_DIR

```hex
28 00 ea 11 04 03 02 07 19 01 de 01 00 14 03 03 29
```

```
00: cond = consume_item(item_id=234, name="Earth Disc", probe=0)
01: if not cond: skip_tokens(4)
    # skip -> show_text(str[3] "A small shrine made from dried mud / contains a domed pedestal. A small ")
02: show_text(str[2] "A small shrine made from dried mud / contains a domed pedestal. You inse")
03: wait_for_space()
04: add_party_entity(0x01, f3a=0xDE, f40=0x01, f46=0x00)
05: clear_current_tile_event_flag()
06: show_text(str[3] "A small shrine made from dried mud / contains a domed pedestal. A small ")
07: set_abort_and_exit()
```

**Event 03** — triggers: (0,4)/ANY_DIR, (0,9)/ANY_DIR, (1,13)/ANY_DIR, (2,0)/ANY_DIR, (2,4)/ANY_DIR, (2,6)/ANY_DIR, (3,2)/ANY_DIR, (3,7)/ANY_DIR, (3,11)/ANY_DIR, (4,4)/ANY_DIR, (4,12)/ANY_DIR, (5,2)/ANY_DIR, (5,3)/ANY_DIR, (5,5)/ANY_DIR, (5,8)/ANY_DIR, (5,9)/ANY_DIR, (5,13)/ANY_DIR, (6,4)/ANY_DIR, (6,10)/ANY_DIR, (7,3)/ANY_DIR, (7,8)/ANY_DIR, (7,11)/ANY_DIR, (8,1)/ANY_DIR, (8,5)/ANY_DIR, (8,7)/ANY_DIR, (8,9)/ANY_DIR, (8,12)/ANY_DIR, (9,8)/ANY_DIR, (9,13)/ANY_DIR, (10,3)/ANY_DIR, (10,6)/ANY_DIR, (10,7)/ANY_DIR, (10,10)/ANY_DIR, (11,9)/ANY_DIR, (11,13)/ANY_DIR, (12,1)/ANY_DIR, (12,4)/ANY_DIR, (12,12)/ANY_DIR, (13,8)/ANY_DIR, (13,11)/ANY_DIR, (14,2)/ANY_DIR, (14,14)/ANY_DIR

```hex
2b 01 12 ce e0 e0 e0 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=CE E0 E0 E0 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 04** — triggers: (8,15)/0xA0

```hex
06 05 02 06 29
```

```
00: show_text_popup_style_b(str[5] "Red / Message 1")
01: show_text_block(str[6] "iwrs vist Arlume pisoll est / e Qe s tinthtruehemetswhly")
02: set_abort_and_exit()
```

**Event 05** — triggers: (0,7)/0x50

```hex
06 07 02 08 29
```

```
00: show_text_popup_style_b(str[7] "Red / Message 4")
01: show_text_block(str[8] "th Th  bterd w spveasonndi / he nTralor K gest ind u")
02: set_abort_and_exit()
```

**Event 06** — triggers: (6,9)/0xA0

```hex
06 09 02 0a 29
```

```
00: show_text_popup_style_b(str[9] "Red / Message 6")
01: show_text_block(str[10] "rt n irmuithwse th y lfidue /  thsbeughae desh Tgeost ied")
02: set_abort_and_exit()
```

**Event 07** — triggers: (0,15)/ANY_DIR

```hex
17 2a 00 11 01 14 02 0b 07 0d 09 1a 84 09 0c 28 0f
```

```
00: cond = load_var8(group=0x2A, index=0x00)
01: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[11] "You are not of this element. Be gone!")
02: clear_current_tile_event_flag()
03: show_text_block(str[11] "You are not of this element. Be gone!")
04: wait_for_space()
05: engine_call(0x09)
06: store_var8(group=0x84, value=0x09)
07: map_transition(0x28, 0x0F)
```

## String table

- `[00]` <EMPTY>
- `[01]` Lord Gralkor exclaims, "How dare you / invade my plane! Die intruders!"
- `[02]` A small shrine made from dried mud / contains a domed pedestal. You insert / your Earth Disc into the slot. The / dome opens and you take the Earth / Talon that has rested beneath it for / decades.
- `[03]` A small shrine made from dried mud / contains a domed pedestal. A small / disc slot appears to be the only way / to open it. None of your items fit / into the slot.
- `[04]` *
- `[05]` Red / Message 1
- `[06]` iwrs vist Arlume pisoll est / e Qe s tinthtruehemetswhly 
- `[07]` Red / Message 4
- `[08]`  th Th  bterd w spveasonndi / he nTralor K gest ind u    
- `[09]` Red / Message 6
- `[10]` rt n irmuithwse th y lfidue /  thsbeughae desh Tgeost ied
- `[11]` You are not of this element. Be gone!
- `[12]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
