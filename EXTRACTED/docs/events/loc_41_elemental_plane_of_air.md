# Location 41 — Elemental Plane of Air

- **event.dat** offset `0x00D366`, length **756** bytes
- **Map:** map screen **41**; Elemental Plane of Air
- **Record kind:** `standard`
- **Triggers:** 34; **script segments:** 8; **strings:** 11

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,6) | `0x16` | **3** | ANY_DIR |
| (2,2) | `0x22` | **3** | ANY_DIR |
| (2,9) | `0x29` | **3** | ANY_DIR |
| (4,5) | `0x45` | **3** | ANY_DIR |
| (4,12) | `0x4C` | **3** | ANY_DIR |
| (5,4) | `0x54` | **3** | ANY_DIR |
| (5,5) | `0x55` | **1** | ANY_DIR |
| (5,6) | `0x56` | **3** | ANY_DIR |
| (6,3) | `0x63` | **3** | ANY_DIR |
| (6,7) | `0x67` | **3** | ANY_DIR |
| (6,11) | `0x6B` | **3** | ANY_DIR |
| (7,0) | `0x70` | **4** | 0xA0 |
| (7,3) | `0x73` | **3** | ANY_DIR |
| (7,8) | `0x78` | **3** | ANY_DIR |
| (7,10) | `0x7A` | **3** | ANY_DIR |
| (7,11) | `0x7B` | **2** | ANY_DIR |
| (7,12) | `0x7C` | **3** | ANY_DIR |
| (7,14) | `0x7E` | **3** | ANY_DIR |
| (8,2) | `0x82` | **3** | ANY_DIR |
| (8,8) | `0x88` | **3** | ANY_DIR |
| (8,11) | `0x8B` | **3** | ANY_DIR |
| (9,1) | `0x91` | **3** | ANY_DIR |
| (9,8) | `0x98` | **3** | ANY_DIR |
| (10,6) | `0xA6` | **3** | ANY_DIR |
| (10,10) | `0xAA` | **3** | ANY_DIR |
| (11,11) | `0xBB` | **3** | ANY_DIR |
| (12,2) | `0xC2` | **3** | ANY_DIR |
| (12,6) | `0xC6` | **3** | ANY_DIR |
| (13,12) | `0xDC` | **3** | ANY_DIR |
| (15,0) | `0xF0` | **6** | ANY_DIR |
| (15,4) | `0xF4` | **3** | ANY_DIR |
| (15,7) | `0xF7` | **5** | 0x50 |
| (15,14) | `0xFE` | **3** | ANY_DIR |
| (15,15) | `0xFF` | **3** | ANY_DIR |

## Events

**Event 01** — triggers: (5,5)/ANY_DIR

```hex
2b 02 02 01 12 c2 c2 c2 c2 c2 fb c2 c2 c2 c2 00 00 14
```

```
00: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[1] "Lord Shalwend bellows, "How dare you / invade my plane! Die intruders!"")
02: encounter_setup(monsters=C2 C2 C2 C2 C2 FB C2 C2 C2 C2, flags=00 00)
03: clear_current_tile_event_flag()
```

**Event 02** — triggers: (7,11)/ANY_DIR

```hex
28 00 e8 11 04 03 02 07 19 01 dc 01 00 14 02 03 29
```

```
00: cond = consume_item(item_id=232, name="Air Disc", probe=0)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[3] "A transparent shrine contains a domed / pedestal. A small disc slot appe")
02: show_text(str[2] "A transparent shrine contains a domed / pedestal. You insert your Air Di")
03: wait_for_space()
04: add_party_entity(0x01, f3a=0xDC, f40=0x01, f46=0x00)
05: clear_current_tile_event_flag()
06: show_text_block(str[3] "A transparent shrine contains a domed / pedestal. A small disc slot appe")
07: set_abort_and_exit()
```

**Event 03** — triggers: (1,6)/ANY_DIR, (2,2)/ANY_DIR, (2,9)/ANY_DIR, (4,5)/ANY_DIR, (4,12)/ANY_DIR, (5,4)/ANY_DIR, (5,6)/ANY_DIR, (6,3)/ANY_DIR, (6,7)/ANY_DIR, (6,11)/ANY_DIR, (7,3)/ANY_DIR, (7,8)/ANY_DIR, (7,10)/ANY_DIR, (7,12)/ANY_DIR, (7,14)/ANY_DIR, (8,2)/ANY_DIR, (8,8)/ANY_DIR, (8,11)/ANY_DIR, (9,1)/ANY_DIR, (9,8)/ANY_DIR, (10,6)/ANY_DIR, (10,10)/ANY_DIR, (11,11)/ANY_DIR, (12,2)/ANY_DIR, (12,6)/ANY_DIR, (13,12)/ANY_DIR, (15,4)/ANY_DIR, (15,14)/ANY_DIR, (15,15)/ANY_DIR

```hex
2b 01 12 ce c2 c2 c2 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=CE C2 C2 C2 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 04** — triggers: (7,0)/0xA0

```hex
06 05 02 06 29
```

```
00: show_text_popup_style_b(str[5] "Red / Message 2")
01: show_text_block(str[6] "entioTn , woosxt,tll fu eq / ithon hro avom td.ke ahaurr")
02: set_abort_and_exit()
```

**Event 05** — triggers: (15,7)/0x50

```hex
06 07 02 08 29
```

```
00: show_text_popup_style_b(str[7] "Red / Message 7")
01: show_text_block(str[8] "t, ng.estonsnbeustit periau / o tO  h TOadandasio feyoVC")
02: set_abort_and_exit()
```

**Event 06** — triggers: (15,0)/ANY_DIR

```hex
17 28 00 11 01 14 02 09 07 0d 09 1a 84 09 0c 05 f0
```

```
00: cond = load_var8(group=0x28, index=0x00)
01: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[9] "You are not of this element. Bo gone!")
02: clear_current_tile_event_flag()
03: show_text_block(str[9] "You are not of this element. Bo gone!")
04: wait_for_space()
05: engine_call(0x09)
06: store_var8(group=0x84, value=0x09)
07: map_transition(0x05, 0xF0)
```

## String table

- `[00]` <EMPTY>
- `[01]` Lord Shalwend bellows, "How dare you / invade my plane! Die intruders!"
- `[02]` A transparent shrine contains a domed / pedestal. You insert your Air Disc / into the slot. The dome opens and you / take the Air Talon that has rested / beneath it for decades.
- `[03]` A transparent shrine contains a domed / pedestal. A small disc slot appears to / be the only way to open it. None of / your items fit into the slot.
- `[04]` A
- `[05]` Red / Message 2
- `[06]`  entioTn , woosxt,tll fu eq / ithon hro avom td.ke ahaurr
- `[07]` Red / Message 7
- `[08]` t, ng.estonsnbeustit periau / o tO  h TOadandasio feyoVC
- `[09]` You are not of this element. Bo gone!
- `[10]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
