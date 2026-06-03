# Location 26 — Murray's Cave

- **event.dat** offset `0x008A28`, length **1379** bytes
- **Map:** map screen **26**; Murray's Cave
- **Record kind:** `standard`
- **Triggers:** 27; **script segments:** 25; **strings:** 21

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,0) | `0x00` | **20** | DIR_N? |
| (1,2) | `0x12` | **14** | ANY_DIR |
| (1,3) | `0x13` | **11** | ALWAYS |
| (1,14) | `0x1E` | **21** | DIR_SPECIAL |
| (2,1) | `0x21` | **19** | DIR_SPECIAL |
| (2,3) | `0x23` | **18** | ALWAYS |
| (2,9) | `0x29` | **16** | DIR_SPECIAL |
| (2,14) | `0x2E` | **16** | ALWAYS |
| (3,11) | `0x3B` | **15** | ANY_DIR |
| (3,12) | `0x3C` | **15** | ANY_DIR |
| (5,2) | `0x52` | **9** | ENTER |
| (5,6) | `0x56` | **12** | 0x60 |
| (6,2) | `0x62` | **10** | ENTER |
| (7,12) | `0x7C` | **23** | ENTER |
| (8,0) | `0x80` | **2** | ALWAYS |
| (8,1) | `0x81` | **17** | ALWAYS |
| (8,3) | `0x83` | **6** | ALWAYS |
| (8,4) | `0x84` | **5** | 0x50 |
| (8,5) | `0x85` | **4** | 0x50 |
| (8,6) | `0x86` | **3** | 0x70 |
| (9,0) | `0x90` | **8** | DIR_N? |
| (11,12) | `0xBC` | **16** | ENTER |
| (12,0) | `0xC0` | **22** | DIR_N? |
| (12,12) | `0xCC` | **15** | ANY_DIR |
| (14,14) | `0xEE` | **1** | ENTER+SPECIAL |
| (15,5) | `0xF5` | **7** | DIR_SPECIAL |
| (15,7) | `0xF7` | **13** | ANY_DIR |

## Events

**Event 01** — triggers: (14,14)/ENTER+SPECIAL

```hex
01 01 09 11 01 0c 10 22 0f
```

```
00: show_text_basic(str[1] "Ascend to Murray's Resort Isle (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x10, 0x22)
04: end_script()
```

**Event 02** — triggers: (8,0)/ALWAYS

```hex
02 02 09 11 01 0c 1e 3b 0f
```

```
00: show_text_block(str[2] "An evil veil hides mystic teleport / to the horrid Dawn's Mist Bog. / En")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x1E, 0x3B)
04: end_script()
```

**Event 03** — triggers: (8,6)/0x70

```hex
15 00 01 80 11 04 01 03 07 0d 09 0c 1a 87 0f
```

```
00: apply_party(count=0x00, op=0x01, val=0x80)
01: if not cond: skip_tokens(4)
    # skip -> end_script()
02: show_text_basic(str[3] "No hirelings allowed!")
03: wait_for_space()
04: engine_call(0x09)
05: map_transition(0x1A, 0x87)
06: end_script()
```

**Event 04** — triggers: (8,5)/0x50

```hex
15 01 22 00 1b 28 10 04 01 04 07 0d 09 0c 1a 87 0f
```

```
00: apply_party(count=0x01, op=0x22, val=0x00)
01: cond = (cond >= 0x28)
02: if cond: skip_tokens(4)
    # skip -> end_script()
03: show_text_basic(str[4] "Not strong enough!")
04: wait_for_space()
05: engine_call(0x09)
06: map_transition(0x1A, 0x87)
07: end_script()
```

**Event 05** — triggers: (8,4)/0x50

```hex
32 08 10 04 01 05 07 0d 01 0c 1a 87 0f
```

```
00: cond = load_cond_from_var(0x08)
01: if cond: skip_tokens(4)
    # skip -> end_script()
02: show_text_basic(str[5] "You are not heroic!")
03: wait_for_space()
04: engine_call(0x01)
05: map_transition(0x1A, 0x87)
06: end_script()
```

**Event 06** — triggers: (8,3)/ALWAYS

```hex
04 06
```

```
00: show_text_above_door(str[6] "Murray is IN!")
```

**Event 07** — triggers: (15,5)/DIR_SPECIAL

```hex
02 07 09 11 05 02 08 1f 00 20 02 f4 01 00 1f 00 3a 02 f4 01 00 07 14 0f
```

```
00: show_text_block(str[7] "A storeroom is filled with / Murray's Power Oil. Rub on (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(5)
    # skip -> end_script()
03: show_text_block(str[8] "You feel as if you could / take much damage today!")
04: party_effect(sel=0x00, 20 02 F4 01 00)
05: party_effect(sel=0x00, 3A 02 F4 01 00)
06: wait_for_space()
07: clear_current_tile_event_flag()
08: end_script()
```

**Event 08** — triggers: (9,0)/DIR_N?

```hex
02 09 09 11 07 01 0a 07 18 00 26 00 c8 18 00 20 00 01 18 00 21 00 00 18 00 3a 00 01 18 00 3b 00 00 0f
```

```
00: show_text_block(str[9] "Rows upon rows of casks filled with / the famous Murray's Goofy Juice li")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(7)
    # skip -> end_script()
03: show_text_basic(str[10] "That's some good goofy juice!")
04: wait_for_space()
05: apply_party_masked(count=0x00, set=0x26, and=0x00, or=0xC8)
06: apply_party_masked(count=0x00, set=0x20, and=0x00, or=0x01)
07: apply_party_masked(count=0x00, set=0x21, and=0x00, or=0x00)
08: apply_party_masked(count=0x00, set=0x3A, and=0x00, or=0x01)
09: apply_party_masked(count=0x00, set=0x3B, and=0x00, or=0x00)
10: end_script()
```

**Event 09** — triggers: (5,2)/ENTER

```hex
06 0b
```

```
00: show_text_popup_style_b(str[11] "Murray's / Vault")
```

**Event 10** — triggers: (6,2)/ENTER

```hex
05 0c
```

```
00: show_text_popup_style_a(str[12] "Dare steal from me / impetuous fool, / and suffer the / consequences! / ")
```

**Event 11** — triggers: (1,3)/ALWAYS

```hex
04 0d
```

```
00: show_text_above_door(str[13] "Murray Only!")
```

**Event 12** — triggers: (5,6)/0x60

```hex
17 3c 00 10 0e 02 0e 09 11 0c 18 00 10 00 05 18 00 11 00 05 18 00 12 00 05 18 00 13 00 05 18 00 14 00 05 18 00 15 00 05 18 00 44 00 05 2a 40 4b 4c 00 00 00 00 00 00 00 00 00 00 00 1a 3c 01 02 0f 07 14 0f
```

```
00: cond = load_var8(group=0x3C, index=0x00)
01: if cond: skip_tokens(14)
    # skip -> clear_current_tile_event_flag()
02: show_text_block(str[14] "Before you lies an infinitesimally / small portion of Murray's wealth. /")
03: cond = prompt_yes_no()
04: if not cond: skip_tokens(12)
    # skip -> end_script()
05: apply_party_masked(count=0x00, set=0x10, and=0x00, or=0x05)
06: apply_party_masked(count=0x00, set=0x11, and=0x00, or=0x05)
07: apply_party_masked(count=0x00, set=0x12, and=0x00, or=0x05)
08: apply_party_masked(count=0x00, set=0x13, and=0x00, or=0x05)
09: apply_party_masked(count=0x00, set=0x14, and=0x00, or=0x05)
10: apply_party_masked(count=0x00, set=0x15, and=0x00, or=0x05)
11: apply_party_masked(count=0x00, set=0x44, and=0x00, or=0x05)
12: set_treasure(gold/exp=5000000, gems=0, items=[0, 0, 0])
13: store_var8(group=0x3C, value=0x01)
14: show_text_block(str[15] "Seek and you shall find the treasure, / but you were warned!")
15: wait_for_space()
16: clear_current_tile_event_flag()
17: end_script()
```

**Event 13** — triggers: (15,7)/ANY_DIR

```hex
2b 01 12 73 73 73 73 73 73 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=73 73 73 73 73 73 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (1,2)/ANY_DIR

```hex
2b 01 12 a2 a2 a2 a2 a2 a2 bd 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=A2 A2 A2 A2 A2 A2 BD 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (3,11)/ANY_DIR, (3,12)/ANY_DIR, (12,12)/ANY_DIR

```hex
2b 01 12 18 18 18 18 18 18 48 48 48 48 48 10 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=18 18 18 18 18 18 48 48 48 48, flags=48 10)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (2,9)/DIR_SPECIAL, (2,14)/ALWAYS, (11,12)/ENTER

```hex
04 10
```

```
00: show_text_above_door(str[16] "Laboratory-keep out!")
```

**Event 17** — triggers: (8,1)/ALWAYS

```hex
0e 66
```

```
00: exec_selector(0x66)
```

**Event 18** — triggers: (2,3)/ALWAYS

```hex
0e 6d
```

```
00: exec_selector(0x6D)
```

**Event 19** — triggers: (2,1)/DIR_SPECIAL

```hex
0e 71
```

```
00: exec_selector(0x71)
```

**Event 20** — triggers: (0,0)/DIR_N?

```hex
0e 7b
```

```
00: exec_selector(0x7B)
```

**Event 21** — triggers: (1,14)/DIR_SPECIAL

```hex
02 11 29
```

```
00: show_text_block(str[17] "King Kalohn and the invincible Mega / Dragon battled in the 9th century ")
01: set_abort_and_exit()
```

**Event 22** — triggers: (12,0)/DIR_N?

```hex
02 12 29
```

```
00: show_text_block(str[18] "The Sword of Nobility, usable by those / of noble descent, is guarded in")
01: set_abort_and_exit()
```

**Event 23** — triggers: (7,12)/ENTER

```hex
02 13 29
```

```
00: show_text_block(str[19] "Only the valiant may wield the Sword / of Valor, hidden for years in A2 ")
01: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` Ascend to Murray's Resort Isle (y/n)?
- `[02]` An evil veil hides mystic teleport / to the horrid Dawn's Mist Bog. / Enter (y/n)?
- `[03]` No hirelings allowed!
- `[04]` Not strong enough!
- `[05]` You are not heroic!
- `[06]` Murray is IN!
- `[07]` A storeroom is filled with / Murray's Power Oil. Rub on (y/n)?
- `[08]` You feel as if you could / take much damage today!
- `[09]` Rows upon rows of casks filled with / the famous Murray's Goofy Juice line / the walls of the storeroom. / Drink some (y/n)?
- `[10]` That's some good goofy juice!
- `[11]` Murray's / Vault
- `[12]` Dare steal from me / impetuous fool, / and suffer the / consequences! /           Love, /               Murray
- `[13]` Murray Only!
- `[14]` Before you lies an infinitesimally / small portion of Murray's wealth. / Do you dare steal it (y/n)?
- `[15]` Seek and you shall find the treasure, / but you were warned!
- `[16]` Laboratory-keep out!
- `[17]` King Kalohn and the invincible Mega / Dragon battled in the 9th century / in C4 at 14,5.
- `[18]` The Sword of Nobility, usable by those / of noble descent, is guarded in D1 / at 0,8.
- `[19]` Only the valiant may wield the Sword / of Valor, hidden for years in A2 / at 11,2.
- `[20]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
