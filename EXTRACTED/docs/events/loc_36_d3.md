# Location 36 — D3

- **event.dat** offset `0x00BD16`, length **1076** bytes
- **Map:** map screen **36**; overland sector **D3**
- **Record kind:** `standard`
- **Triggers:** 29; **script segments:** 15; **strings:** 16

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (1,13) | `0x1D` | **8** | ANY_DIR |
| (2,8) | `0x28` | **11** | ANY_DIR |
| (3,3) | `0x33` | **8** | ANY_DIR |
| (3,4) | `0x34` | **10** | ANY_DIR |
| (3,5) | `0x35` | **9** | ANY_DIR |
| (3,13) | `0x3D` | **4** | DIR_E? |
| (4,10) | `0x4A` | **9** | ANY_DIR |
| (4,12) | `0x4C` | **10** | ANY_DIR |
| (4,13) | `0x4D` | **13** | ANY_DIR |
| (5,1) | `0x51` | **10** | ANY_DIR |
| (6,1) | `0x61` | **9** | ANY_DIR |
| (6,5) | `0x65` | **6** | ANY_DIR |
| (7,1) | `0x71` | **8** | ANY_DIR |
| (7,7) | `0x77` | **6** | ANY_DIR |
| (7,13) | `0x7D` | **5** | ANY_DIR |
| (8,3) | `0x83` | **8** | ANY_DIR |
| (8,11) | `0x8B` | **5** | ANY_DIR |
| (9,8) | `0x98` | **6** | ANY_DIR |
| (10,2) | `0xA2` | **7** | ANY_DIR |
| (10,5) | `0xA5` | **12** | ANY_DIR |
| (11,1) | `0xB1` | **10** | ANY_DIR |
| (11,8) | `0xB8` | **6** | ANY_DIR |
| (12,4) | `0xC4` | **7** | ANY_DIR |
| (12,6) | `0xC6` | **9** | ANY_DIR |
| (13,7) | `0xD7` | **1** | ANY_DIR |
| (14,1) | `0xE1` | **3** | ANY_DIR |
| (14,3) | `0xE3` | **2** | ANY_DIR |
| (14,4) | `0xE4` | **7** | ANY_DIR |
| (14,6) | `0xE6` | **8** | ANY_DIR |

## Events

**Event 01** — triggers: (13,7)/ANY_DIR

```hex
0b 08 00 28 00 da 11 04 02 02 18 00 7b fe 01 08 14 02 01 08 14
```

```
00: service_sign(idx=0x08 -> sign 42 [42.anm], pos=0x00)
01: cond = consume_item(item_id=218, name="Cupie Doll", probe=0)
02: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[1] "A mad old man ignores you while / ranting about a Cupie Doll.")
03: show_text_block(str[2] "A mad old man raves, "Hooray! A Cupie / Doll! Use the pool in the Inner ")
04: apply_party_masked(count=0x00, set=0x7B, and=0xFE, or=0x01)
05: wait_key()
06: clear_current_tile_event_flag()
07: show_text_block(str[1] "A mad old man ignores you while / ranting about a Cupie Doll.")
08: wait_key()
09: clear_current_tile_event_flag()
```

**Event 02** — triggers: (14,3)/ANY_DIR

```hex
02 03 09 11 03 18 00 29 00 00 18 00 28 00 c8 18 00 27 00 09 0f
```

```
00: show_text_block(str[3] "Large chunks of bark of an ancient / tree appear to be missing. Eat some")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(3)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x29, and=0x00, or=0x00)
04: apply_party_masked(count=0x00, set=0x28, and=0x00, or=0xC8)
05: apply_party_masked(count=0x00, set=0x27, and=0x00, or=0x09)
06: end_script()
```

**Event 03** — triggers: (14,1)/ANY_DIR

```hex
2b 07 17 17 00 11 01 14 02 04 09 11 05 12 cf da 00 00 00 00 00 00 00 00 00 00 02 05 07 1a 17 01 14 0f
```

```
00: skip_tokens(7)
    # skip -> show_text_block(str[5] "Having destroyed the Lich Lord, the / celebrated Mr. Wizard thanks you /")
01: cond = load_var8(group=0x17, index=0x00)
02: if not cond: skip_tokens(1)
    # skip -> show_text_block(str[4] "A Lich Lord holds a top scientist / captive. Attempt to free him (y/n)?")
03: clear_current_tile_event_flag()
04: show_text_block(str[4] "A Lich Lord holds a top scientist / captive. Attempt to free him (y/n)?")
05: cond = prompt_yes_no()
06: if not cond: skip_tokens(5)
    # skip -> end_script()
07: encounter_setup(monsters=CF DA 00 00 00 00 00 00 00 00, flags=00 00)
08: show_text_block(str[5] "Having destroyed the Lich Lord, the / celebrated Mr. Wizard thanks you /")
09: wait_for_space()
10: store_var8(group=0x17, value=0x01)
11: clear_current_tile_event_flag()
12: end_script()
```

**Event 04** — triggers: (3,13)/DIR_E?

```hex
06 06
```

```
00: show_text_popup_style_b(str[6] "Sandsobar[ / Hillstone[")
```

**Event 05** — triggers: (7,13)/ANY_DIR, (8,11)/ANY_DIR

```hex
01 07 17 23 00 10 03 07 0d 09 0c 20 0d 0d 01 02 0e 29
```

```
00: show_text_basic(str[7] "Rift hole!")
01: cond = load_var8(group=0x23, index=0x00)
02: if cond: skip_tokens(3)
    # skip -> engine_call(0x01)
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x20, 0x0D)
06: engine_call(0x01)
07: show_text_block(str[14] "/    Your levitation spell saved you!")
08: set_abort_and_exit()
```

**Event 06** — triggers: (6,5)/ANY_DIR, (7,7)/ANY_DIR, (9,8)/ANY_DIR, (11,8)/ANY_DIR

```hex
2b 01 12 59 59 59 59 59 59 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=59 59 59 59 59 59 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 07** — triggers: (10,2)/ANY_DIR, (12,4)/ANY_DIR, (14,4)/ANY_DIR

```hex
2b 01 12 62 62 62 62 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=62 62 62 62 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 08** — triggers: (1,13)/ANY_DIR, (3,3)/ANY_DIR, (7,1)/ANY_DIR, (8,3)/ANY_DIR, (14,6)/ANY_DIR

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

**Event 09** — triggers: (3,5)/ANY_DIR, (4,10)/ANY_DIR, (6,1)/ANY_DIR, (12,6)/ANY_DIR

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

**Event 10** — triggers: (3,4)/ANY_DIR, (4,12)/ANY_DIR, (5,1)/ANY_DIR, (11,1)/ANY_DIR

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

**Event 11** — triggers: (2,8)/ANY_DIR

```hex
06 08 02 09 29
```

```
00: show_text_popup_style_b(str[8] "Yellow / Message 9")
01: show_text_block(str[9] "Ifti ecuul wbu. es md.wife /  t ye.In Ts conoed Came  y")
02: set_abort_and_exit()
```

**Event 12** — triggers: (10,5)/ANY_DIR

```hex
06 0a 02 0b 29
```

```
00: show_text_popup_style_b(str[10] "Yellow / Message 7")
01: show_text_block(str[11] "na'd, es ab t n  t cutss t / , ip fmionnothouacanratily")
02: set_abort_and_exit()
```

**Event 13** — triggers: (4,13)/ANY_DIR

```hex
06 0c 02 0d 29
```

```
00: show_text_popup_style_b(str[12] "Yellow / Message 3")
01: show_text_block(str[13] "istoirhoe., adthldhae aror / esree l.was t athe gosch.")
02: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` A mad old man ignores you while / ranting about a Cupie Doll.
- `[02]` A mad old man raves, "Hooray! A Cupie / Doll! Use the pool in the Inner Limits / and then the circus will be of great / benefit."
- `[03]` Large chunks of bark of an ancient / tree appear to be missing. Eat some / of the remaining bark (y/n)?
- `[04]` A Lich Lord holds a top scientist / captive. Attempt to free him (y/n)?
- `[05]` Having destroyed the Lich Lord, the / celebrated Mr. Wizard thanks you / profusely and offers himself for hire / at the Hourglass Inn.
- `[06]` Sandsobar[ / Hillstone[
- `[07]` Rift hole!
- `[08]` Yellow / Message 9
- `[09]` Ifti ecuul wbu. es md.wife /  t ye.In Ts conoed Came  y
- `[10]` Yellow / Message 7
- `[11]` na'd, es ab t n  t cutss t / , ip fmionnothouacanratily
- `[12]` Yellow / Message 3
- `[13]` istoirhoe., adthldhae aror / esree l.was t athe gosch. 
- `[14]`  /    Your levitation spell saved you!
- `[15]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
