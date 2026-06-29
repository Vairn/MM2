# Location 49 — Pinehurst Dungeon Level 1

- **event.dat** offset `0x00F223`, length **1443** bytes
- **Map:** map screen **49**; Pinehurst Dungeon Level 1
- **Record kind:** `standard`
- **Triggers:** 18; **script segments:** 14; **strings:** 21

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,15) | `0x0F` | **9** | DIR_S? |
| (1,5) | `0x15` | **9** | DIR_N? |
| (2,1) | `0x21` | **2** | DIR_W? |
| (2,14) | `0x2E` | **9** | 0x60 |
| (4,14) | `0x4E` | **12** | DIR_N? |
| (5,6) | `0x56` | **11** | 0x90 |
| (5,14) | `0x5E` | **7** | DIR_N? |
| (6,4) | `0x64` | **9** | DIR_W? |
| (6,6) | `0x66` | **5** | DIR_N? |
| (7,2) | `0x72` | **6** | DIR_S? |
| (8,12) | `0x8C` | **3** | DIR_S? |
| (11,8) | `0xB8` | **9** | DIR_E? |
| (13,11) | `0xDB` | **4** | DIR_S? |
| (13,14) | `0xDE` | **8** | DIR_W? |
| (14,0) | `0xE0` | **12** | DIR_S? |
| (14,11) | `0xEB` | **10** | DIR_S? |
| (14,15) | `0xEF` | **9** | DIR_S? |
| (15,0) | `0xF0` | **1** | DIR_N? |

## Events

**Event 01** — triggers: (15,0)/DIR_N?

```hex
01 01 09 11 01 0c 39 2b 0f
```

```
00: show_text_basic(str[1] "Stairs lead up. Climb them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x39, 0x2B)
04: end_script()
```

**Event 02** — triggers: (2,1)/DIR_W?

```hex
01 02 09 11 01 0c 32 77 0f
```

```
00: show_text_basic(str[2] "Stairs leading down. Take them (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x32, 0x77)
04: end_script()
```

**Event 03** — triggers: (8,12)/DIR_S?

```hex
0b 0f 00 02 03 0a 11 01 0c 33 9b 0f
```

```
00: service_sign(idx=0x0F -> sign 24 [24.anm], pos=0x00)
01: show_text_block(str[3] ""If you like fighting bad guys as much / as I do, then take this portal ")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(1)
    # skip -> end_script()
04: map_transition(0x33, 0x9B)
05: end_script()
```

**Event 04** — triggers: (13,11)/DIR_S?

```hex
2d 20 00 11 04 01 04 07 0d 09 0c 31 21 02 05 19 01 f5 00 00 10 01 01 12 07 14
```

```
00: cond = check_member_attr(fields=0x20, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[5] "You have found an Ivory Cameo!")
02: show_text_basic(str[4] "The sign said NO KNIGHTS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x31, 0x21)
06: show_text_block(str[5] "You have found an Ivory Cameo!")
07: add_party_entity(0x01, f3a=0xF5, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[18] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 05** — triggers: (6,6)/DIR_N?

```hex
2d 21 00 11 04 01 06 07 0d 09 0c 31 21 02 07 19 01 f1 00 00 10 01 01 12 07 14
```

```
00: cond = check_member_attr(fields=0x21, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[7] "You have found an Agate Grail!")
02: show_text_basic(str[6] "The sign said NO PALADINS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x31, 0x21)
06: show_text_block(str[7] "You have found an Agate Grail!")
07: add_party_entity(0x01, f3a=0xF1, f40=0x00, f46=0x00)
08: if cond: skip_tokens(1)
    # skip -> wait_for_space()
09: show_text_basic(str[18] "*** Backpacks Full ***")
10: wait_for_space()
11: clear_current_tile_event_flag()
```

**Event 06** — triggers: (7,2)/DIR_S?

```hex
02 08 09 11 05 01 09 26 20 09 13 01 05 00 00 11 01 1f 09 14 01 03 00 00 0f
```

```
00: show_text_block(str[8] ""Actions are sluggish, but precise." / reads a placard next to a giant l")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_basic(str[9] "Who will try (1-8)?")
04: selected = select_party_member()
05: party_effect_b(sel=0x09, 13 01 05 00 00)
06: if not cond: skip_tokens(1)
    # skip -> end_script()
07: party_effect(sel=0x09, 14 01 03 00 00)
08: end_script()
```

**Event 07** — triggers: (5,14)/DIR_N?

```hex
2d a0 00 11 04 01 0a 07 0d 09 0c 31 21 17 3d 00 10 06 02 0b 07 1c 05 1f 80 3c 02 00 00 00 1a 3d 01 14 02 13 29
```

```
00: cond = check_member_attr(fields=0xA0, value=0x00)
01: if not cond: skip_tokens(4)
    # skip -> cond = load_var8(group=0x3D, index=0x00)
02: show_text_basic(str[10] "The sign said NO HUMANS!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x31, 0x21)
06: cond = load_var8(group=0x3D, index=0x00)
07: if cond: skip_tokens(6)
    # skip -> show_text_block(str[19] "Sorry, benefits are given only once / per moon phase.")
08: show_text_block(str[11] "Powerful currents move rapidly from / your feet to your head!")
09: wait_for_space()
10: op_1c_engine_query_to_cond(0x05)
11: party_effect(sel=0x80, 3C 02 00 00 00)
12: store_var8(group=0x3D, value=0x01)
13: clear_current_tile_event_flag()
14: show_text_block(str[19] "Sorry, benefits are given only once / per moon phase.")
15: set_abort_and_exit()
```

**Event 08** — triggers: (13,14)/DIR_W?

```hex
02 0c 09 11 23 15 01 14 00 1b 32 10 01 1f 01 14 01 0a 00 00 15 02 14 00 1b 32 10 01 1f 02 14 01 0a 00 00 15 03 14 00 1b 32 10 01 1f 03 14 01 0a 00 00 15 04 14 00 1b 32 10 01 1f 04 14 01 0a 00 00 15 05 14 00 1b 32 10 01 1f 05 14 01 0a 00 00 15 06 14 00 1b 32 10 01 1f 06 14 01 0a 00 00 15 07 14 00 1b 32 10 01 1f 07 14 01 0a 00 00 15 08 14 00 1b 32 10 01 1f 08 14 01 0a 00 00 02 0d 07 14 0f
```

```
00: show_text_block(str[12] "A mysterious pool appears before you. / Every few seconds, a jet of wate")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(35)
    # skip -> end_script()
03: apply_party(count=0x01, op=0x14, val=0x00)
04: cond = (cond >= 0x32)
05: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x14, val=0x00)
06: party_effect(sel=0x01, 14 01 0A 00 00)
07: apply_party(count=0x02, op=0x14, val=0x00)
08: cond = (cond >= 0x32)
09: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x14, val=0x00)
10: party_effect(sel=0x02, 14 01 0A 00 00)
11: apply_party(count=0x03, op=0x14, val=0x00)
12: cond = (cond >= 0x32)
13: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x14, val=0x00)
14: party_effect(sel=0x03, 14 01 0A 00 00)
15: apply_party(count=0x04, op=0x14, val=0x00)
16: cond = (cond >= 0x32)
17: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x14, val=0x00)
18: party_effect(sel=0x04, 14 01 0A 00 00)
19: apply_party(count=0x05, op=0x14, val=0x00)
20: cond = (cond >= 0x32)
21: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x14, val=0x00)
22: party_effect(sel=0x05, 14 01 0A 00 00)
23: apply_party(count=0x06, op=0x14, val=0x00)
24: cond = (cond >= 0x32)
25: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x14, val=0x00)
26: party_effect(sel=0x06, 14 01 0A 00 00)
27: apply_party(count=0x07, op=0x14, val=0x00)
28: cond = (cond >= 0x32)
29: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x14, val=0x00)
30: party_effect(sel=0x07, 14 01 0A 00 00)
31: apply_party(count=0x08, op=0x14, val=0x00)
32: cond = (cond >= 0x32)
33: if cond: skip_tokens(1)
    # skip -> show_text_block(str[13] "The pool shoots you skyward. As you / fall back to earth, you notice tha")
34: party_effect(sel=0x08, 14 01 0A 00 00)
35: show_text_block(str[13] "The pool shoots you skyward. As you / fall back to earth, you notice tha")
36: wait_for_space()
37: clear_current_tile_event_flag()
38: end_script()
```

**Event 09** — triggers: (0,15)/DIR_S?, (1,5)/DIR_N?, (2,14)/0x60, (6,4)/DIR_W?, (11,8)/DIR_E?, (14,15)/DIR_S?

```hex
2b 1b 02 0e 07 20 01 28 02 14 00 00 10 01 18 01 28 00 00 20 02 28 02 14 00 00 10 01 18 02 28 00 00 20 03 28 02 14 00 00 10 01 18 03 28 00 00 20 04 28 02 14 00 00 10 01 18 04 28 00 00 20 05 28 02 14 00 00 10 01 18 05 28 00 00 20 06 28 02 14 00 00 10 01 18 06 28 00 00 20 07 28 02 14 00 00 10 01 18 07 28 00 00 20 08 28 02 14 00 00 10 01 18 08 28 00 00 13 00 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(27)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[14] "An ancient curse from a misguided / wizard drains your magic.")
02: wait_for_space()
03: party_effect_b(sel=0x01, 28 02 14 00 00)
04: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x02, 28 02 14 00 00)
05: apply_party_masked(count=0x01, set=0x28, and=0x00, or=0x00)
06: party_effect_b(sel=0x02, 28 02 14 00 00)
07: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x03, 28 02 14 00 00)
08: apply_party_masked(count=0x02, set=0x28, and=0x00, or=0x00)
09: party_effect_b(sel=0x03, 28 02 14 00 00)
10: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x04, 28 02 14 00 00)
11: apply_party_masked(count=0x03, set=0x28, and=0x00, or=0x00)
12: party_effect_b(sel=0x04, 28 02 14 00 00)
13: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x05, 28 02 14 00 00)
14: apply_party_masked(count=0x04, set=0x28, and=0x00, or=0x00)
15: party_effect_b(sel=0x05, 28 02 14 00 00)
16: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x06, 28 02 14 00 00)
17: apply_party_masked(count=0x05, set=0x28, and=0x00, or=0x00)
18: party_effect_b(sel=0x06, 28 02 14 00 00)
19: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x07, 28 02 14 00 00)
20: apply_party_masked(count=0x06, set=0x28, and=0x00, or=0x00)
21: party_effect_b(sel=0x07, 28 02 14 00 00)
22: if cond: skip_tokens(1)
    # skip -> party_effect_b(sel=0x08, 28 02 14 00 00)
23: apply_party_masked(count=0x07, set=0x28, and=0x00, or=0x00)
24: party_effect_b(sel=0x08, 28 02 14 00 00)
25: if cond: skip_tokens(1)
    # skip -> encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
26: apply_party_masked(count=0x08, set=0x28, and=0x00, or=0x00)
27: encounter_setup_b(data=00 00 00 00 00 00 00 00 00 00)
28: clear_current_tile_event_flag()
```

**Event 10** — triggers: (14,11)/DIR_S?

```hex
06 0f
```

```
00: show_text_popup_style_b(str[15] "No / Knights!")
```

**Event 11** — triggers: (5,6)/0x90

```hex
06 10
```

```
00: show_text_popup_style_b(str[16] "No / Paladins!")
```

**Event 12** — triggers: (4,14)/DIR_N?, (14,0)/DIR_S?

```hex
06 11
```

```
00: show_text_popup_style_b(str[17] "No / Humans!")
```

## String table

- `[00]` <EMPTY>
- `[01]` Stairs lead up. Climb them (y/n)?
- `[02]` Stairs leading down. Take them (y/n)?
- `[03]` "If you like fighting bad guys as much / as I do, then take this portal to / Luxus Palace Dungeon." Take it (y/n)?
- `[04]` The sign said NO KNIGHTS!
- `[05]` You have found an Ivory Cameo!
- `[06]` The sign said NO PALADINS!
- `[07]` You have found an Agate Grail!
- `[08]` "Actions are sluggish, but precise." / reads a placard next to a giant lever. / Will you pull the lever (y/n)?
- `[09]` Who will try (1-8)?
- `[10]` The sign said NO HUMANS!
- `[11]` Powerful currents move rapidly from / your feet to your head!
- `[12]` A mysterious pool appears before you. / Every few seconds, a jet of water / shoots upward and stops just short of / the ceiling. Jump in (y/n)?
- `[13]` The pool shoots you skyward. As you / fall back to earth, you notice that / the pool is gone. All who are worthy / attain a new level of accuracy!
- `[14]` An ancient curse from a misguided / wizard drains your magic.
- `[15]` No / Knights!
- `[16]` No / Paladins!
- `[17]` No / Humans!
- `[18]` *** Backpacks Full ***
- `[19]` Sorry, benefits are given only once / per moon phase.
- `[20]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
