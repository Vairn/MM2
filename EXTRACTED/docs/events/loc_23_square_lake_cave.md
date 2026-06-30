# Location 23 — Square Lake Cave

- **event.dat** offset `0x007F42`, length **619** bytes
- **Map:** map screen **23**; Square Lake Cave
- **Record kind:** `standard`
- **Triggers:** 29; **script segments:** 20; **strings:** 20

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **15** | DIR_W? |
| (0,1) | `0x01` | **14** | DIR_E? |
| (1,15) | `0x1F` | **17** | ANY_DIR |
| (2,14) | `0x2E` | **17** | ANY_DIR |
| (3,13) | `0x3D` | **17** | ANY_DIR |
| (4,12) | `0x4C` | **17** | ANY_DIR |
| (5,11) | `0x5B` | **17** | ANY_DIR |
| (6,10) | `0x6A` | **17** | ANY_DIR |
| (7,9) | `0x79` | **17** | ANY_DIR |
| (8,8) | `0x88` | **17** | ANY_DIR |
| (9,7) | `0x97` | **17** | ANY_DIR |
| (10,6) | `0xA6` | **17** | ANY_DIR |
| (11,5) | `0xB5` | **17** | ANY_DIR |
| (12,4) | `0xC4` | **17** | ANY_DIR |
| (14,11) | `0xEB` | **16** | ANY_DIR |
| (15,0) | `0xF0` | **18** | DIR_E? |
| (15,3) | `0xF3` | **1** | DIR_E? |
| (15,4) | `0xF4` | **2** | DIR_E? |
| (15,5) | `0xF5` | **3** | DIR_E? |
| (15,6) | `0xF6` | **4** | DIR_E? |
| (15,7) | `0xF7` | **5** | DIR_E? |
| (15,8) | `0xF8` | **6** | DIR_E? |
| (15,9) | `0xF9` | **7** | DIR_E? |
| (15,10) | `0xFA` | **8** | DIR_E? |
| (15,11) | `0xFB` | **9** | DIR_E? |
| (15,12) | `0xFC` | **10** | DIR_E? |
| (15,13) | `0xFD` | **11** | DIR_E? |
| (15,14) | `0xFE` | **12** | DIR_E? |
| (15,15) | `0xFF` | **13** | DIR_E? |

## Events

**Event 01** — triggers: (15,3)/DIR_E?

```hex
06 01
```

```
00: show_text_popup_style_b(str[1] "Hello!")
```

**Event 02** — triggers: (15,4)/DIR_E?

```hex
06 02
```

```
00: show_text_popup_style_b(str[2] "Are you / ready?")
```

**Event 03** — triggers: (15,5)/DIR_E?

```hex
06 03
```

```
00: show_text_popup_style_b(str[3] "Hope so!")
```

**Event 04** — triggers: (15,6)/DIR_E?

```hex
06 04
```

```
00: show_text_popup_style_b(str[4] "Almost / there!")
```

**Event 05** — triggers: (15,7)/DIR_E?

```hex
06 05
```

```
00: show_text_popup_style_b(str[5] "The end / is near.")
```

**Event 06** — triggers: (15,8)/DIR_E?

```hex
06 06
```

```
00: show_text_popup_style_b(str[6] "Having / fun?")
```

**Event 07** — triggers: (15,9)/DIR_E?

```hex
06 07
```

```
00: show_text_popup_style_b(str[7] "Getting / closer ...")
```

**Event 08** — triggers: (15,10)/DIR_E?

```hex
06 08
```

```
00: show_text_popup_style_b(str[8] "Keep / going")
```

**Event 09** — triggers: (15,11)/DIR_E?

```hex
06 09
```

```
00: show_text_popup_style_b(str[9] "A little / further")
```

**Event 10** — triggers: (15,12)/DIR_E?

```hex
06 0a
```

```
00: show_text_popup_style_b(str[10] "Here it / is!!!!!")
```

**Event 11** — triggers: (15,13)/DIR_E?

```hex
06 0b
```

```
00: show_text_popup_style_b(str[11] "Just / kidding!")
```

**Event 12** — triggers: (15,14)/DIR_E?

```hex
06 0c
```

```
00: show_text_popup_style_b(str[12] "One more / step ...")
```

**Event 13** — triggers: (15,15)/DIR_E?

```hex
06 0d
```

```
00: show_text_popup_style_b(str[13] "This is / IT!!!!!")
```

**Event 14** — triggers: (0,1)/DIR_E?

```hex
06 0e
```

```
00: show_text_popup_style_b(str[14] "Beliaeff's / Beastiary")
```

**Event 15** — triggers: (0,0)/DIR_W?

```hex
02 0f 09 11 01 0c 0b 7a 0f
```

```
00: show_text_block(str[15] "Stairs lead back to Square Lake. / Take them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x0B, 0x7A)
04: end_script()
```

**Event 16** — triggers: (14,11)/ANY_DIR

```hex
2b 08 15 00 7f 01 11 04 02 10 09 10 02 0f 02 11 12 d0 d0 d0 d0 d0 d0 d0 d0 d0 d0 d0 38 14
```

```
00: skip_tokens(8)
    # skip -> clear_current_tile_event_flag()
01: apply_party(count=0x00, op=0x7F, val=0x01)
02: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[17] "66 Devil Kings attack!")
03: show_text_block(str[16] "66 Devil Kings bow down to the / Chosen One. Care to attack (y/n)?")
04: cond = prompt_yes_no()
05: if cond: skip_tokens(2)
    # skip -> encounter_setup(monsters=D0 D0 D0 D0 D0 D0 D0 D0 D0 D0, flags=D0 38)
06: end_script()
07: show_text_block(str[17] "66 Devil Kings attack!")
08: encounter_setup(monsters=D0 D0 D0 D0 D0 D0 D0 D0 D0 D0, flags=D0 38)
09: clear_current_tile_event_flag()
```

**Event 17** — triggers: (1,15)/ANY_DIR, (2,14)/ANY_DIR, (3,13)/ANY_DIR, (4,12)/ANY_DIR, (5,11)/ANY_DIR, (6,10)/ANY_DIR, (7,9)/ANY_DIR, (8,8)/ANY_DIR, (9,7)/ANY_DIR, (10,6)/ANY_DIR, (11,5)/ANY_DIR, (12,4)/ANY_DIR

```hex
2b 01 0e 7f 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: exec_selector(0x7F)  # special_127
02: clear_current_tile_event_flag()
```

**Event 18** — triggers: (15,0)/DIR_E?

```hex
2b 04 15 00 7f 20 11 04 0b 03 00 0e fd 0b 89 00 0e fe 02 12 07 0c 00 50
```

```
00: skip_tokens(4)
    # skip -> service_sign(idx=0x89 -> sign 75 [75.anm], pos=0x00)
01: apply_party(count=0x00, op=0x7F, val=0x20)
02: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[18] "You shouldn't be here until you save / the King in the 9th century.")
03: service_sign(idx=0x03 -> sign 42 [42.anm], pos=0x00)
04: exec_selector(0xFD)  # special_253
05: service_sign(idx=0x89 -> sign 75 [75.anm], pos=0x00)
06: exec_selector(0xFE)
07: show_text_block(str[18] "You shouldn't be here until you save / the King in the 9th century.")
08: wait_for_space()
09: map_transition(0x00, 0x50)
```

## String table

- `[00]` <EMPTY>
- `[01]` Hello!
- `[02]` Are you / ready?
- `[03]` Hope so!
- `[04]` Almost / there!
- `[05]` The end / is near.
- `[06]` Having / fun?
- `[07]` Getting / closer ...
- `[08]` Keep / going
- `[09]` A little / further
- `[10]` Here it / is!!!!!
- `[11]` Just / kidding!
- `[12]` One more / step ...
- `[13]` This is / IT!!!!!
- `[14]` Beliaeff's / Beastiary
- `[15]` Stairs lead back to Square Lake. / Take them (y/n)?
- `[16]` 66 Devil Kings bow down to the / Chosen One. Care to attack (y/n)?
- `[17]` 66 Devil Kings attack!
- `[18]` You shouldn't be here until you save / the King in the 9th century.
- `[19]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
