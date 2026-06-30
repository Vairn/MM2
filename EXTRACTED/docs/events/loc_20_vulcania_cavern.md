# Location 20 — Vulcania Cavern

- **event.dat** offset `0x006EBB`, length **1410** bytes
- **Map:** map screen **20**; Vulcania Cavern
- **Record kind:** `standard`
- **Triggers:** 51; **script segments:** 17; **strings:** 17

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,10) | `0x0A` | **1** | DIR_E? |
| (0,13) | `0x0D` | **6** | ANY_DIR |
| (1,1) | `0x11` | **10** | 0x90 |
| (1,4) | `0x14` | **6** | ANY_DIR |
| (1,9) | `0x19` | **9** | DIR_S? |
| (1,11) | `0x1B` | **6** | ANY_DIR |
| (1,15) | `0x1F` | **7** | DIR_E? |
| (2,4) | `0x24` | **6** | ANY_DIR |
| (2,10) | `0x2A` | **6** | ANY_DIR |
| (3,5) | `0x35` | **6** | ANY_DIR |
| (3,7) | `0x37` | **6** | ANY_DIR |
| (3,11) | `0x3B` | **6** | ANY_DIR |
| (4,1) | `0x41` | **4** | ANY_DIR |
| (4,4) | `0x44` | **6** | ANY_DIR |
| (4,6) | `0x46` | **6** | ANY_DIR |
| (4,9) | `0x49` | **6** | ANY_DIR |
| (4,11) | `0x4B` | **6** | ANY_DIR |
| (4,14) | `0x4E` | **6** | ANY_DIR |
| (5,3) | `0x53` | **6** | ANY_DIR |
| (5,5) | `0x55` | **6** | ANY_DIR |
| (6,1) | `0x61` | **6** | ANY_DIR |
| (6,3) | `0x63` | **15** | DIR_E? |
| (6,7) | `0x67` | **6** | ANY_DIR |
| (6,9) | `0x69` | **6** | ANY_DIR |
| (6,13) | `0x6D` | **12** | DIR_W? |
| (7,4) | `0x74` | **6** | ANY_DIR |
| (8,1) | `0x81` | **6** | ANY_DIR |
| (8,2) | `0x82` | **13** | DIR_N? |
| (9,2) | `0x92` | **3** | ANY_DIR |
| (9,4) | `0x94` | **6** | ANY_DIR |
| (9,6) | `0x96` | **6** | ANY_DIR |
| (9,7) | `0x97` | **6** | ANY_DIR |
| (9,11) | `0x9B` | **6** | ANY_DIR |
| (10,5) | `0xA5` | **6** | ANY_DIR |
| (11,3) | `0xB3` | **6** | ANY_DIR |
| (11,4) | `0xB4` | **6** | ANY_DIR |
| (11,9) | `0xB9` | **6** | ANY_DIR |
| (11,12) | `0xBC` | **6** | ANY_DIR |
| (12,1) | `0xC1` | **6** | ANY_DIR |
| (12,3) | `0xC3` | **6** | ANY_DIR |
| (12,4) | `0xC4` | **6** | ANY_DIR |
| (12,6) | `0xC6` | **6** | ANY_DIR |
| (13,6) | `0xD6` | **6** | ANY_DIR |
| (14,1) | `0xE1` | **2** | 0xB0 |
| (14,2) | `0xE2` | **8** | ANY_DIR |
| (14,5) | `0xE5` | **11** | DIR_N? |
| (14,10) | `0xEA` | **6** | ANY_DIR |
| (14,12) | `0xEC` | **6** | ANY_DIR |
| (14,15) | `0xEF` | **5** | ANY_DIR |
| (15,7) | `0xF7` | **6** | ANY_DIR |
| (15,15) | `0xFF` | **14** | DIR_E? |

## Events

**Event 01** — triggers: (0,10)/DIR_E?

```hex
01 01 09 11 01 0c 03 0a 0f
```

```
00: show_text_basic(str[1] "Cave exit. Leave (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x03, 0x0A)
04: end_script()
```

**Event 02** — triggers: (14,1)/0xB0

```hex
17 07 00 10 04 03 02 1a 07 01 1a 08 01 07 14
```

```
00: cond = load_var8(group=0x07, index=0x00)
01: if cond: skip_tokens(4)
    # skip -> clear_current_tile_event_flag()
02: show_text(str[2] "An amnesiac and a wild-eyed ninja rub / their wrists after you free them")
03: store_var8(group=0x07, value=0x01)
04: store_var8(group=0x08, value=0x01)
05: wait_for_space()
06: clear_current_tile_event_flag()
```

**Event 03** — triggers: (9,2)/ANY_DIR

```hex
02 03 1f 01 38 02 c8 00 00 07 14
```

```
00: show_text_block(str[3] "You stumble upon some wayward gems.")
01: party_effect(sel=0x01, 38 02 C8 00 00)
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 04** — triggers: (4,1)/ANY_DIR

```hex
06 04
```

```
00: show_text_popup_style_b(str[4] "Keep / trying")
```

**Event 05** — triggers: (14,15)/ANY_DIR

```hex
0b 07 00 02 05 0a 11 21 15 02 44 00 1b 32 10 01 1f 02 44 01 0a 00 00 15 03 44 00 1b 32 10 01 1f 03 44 01 0a 00 00 15 04 44 00 1b 32 10 01 1f 04 44 01 0a 00 00 15 05 44 00 1b 32 10 01 1f 05 44 01 0a 00 00 15 06 44 00 1b 32 10 01 1f 06 44 01 0a 00 00 15 07 44 00 1b 32 10 01 1f 07 44 01 0a 00 00 15 08 44 00 1b 32 10 01 1f 08 44 01 0a 00 00 15 01 44 00 1b 32 10 01 1f 01 44 01 0a 00 00 14 0f
```

```
00: service_sign(idx=0x07 -> sign 69 [69.anm], pos=0x00)
01: show_text_block(str[5] "You cringe as a lumbering giant sings / a popular ballad very badly. / S")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(33)
    # skip -> end_script()
04: apply_party(count=0x02, op=0x44, val=0x00)
05: cond = (cond >= 0x32)
06: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x44, val=0x00)
07: party_effect(sel=0x02, 44 01 0A 00 00)
08: apply_party(count=0x03, op=0x44, val=0x00)
09: cond = (cond >= 0x32)
10: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x44, val=0x00)
11: party_effect(sel=0x03, 44 01 0A 00 00)
12: apply_party(count=0x04, op=0x44, val=0x00)
13: cond = (cond >= 0x32)
14: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x44, val=0x00)
15: party_effect(sel=0x04, 44 01 0A 00 00)
16: apply_party(count=0x05, op=0x44, val=0x00)
17: cond = (cond >= 0x32)
18: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x44, val=0x00)
19: party_effect(sel=0x05, 44 01 0A 00 00)
20: apply_party(count=0x06, op=0x44, val=0x00)
21: cond = (cond >= 0x32)
22: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x44, val=0x00)
23: party_effect(sel=0x06, 44 01 0A 00 00)
24: apply_party(count=0x07, op=0x44, val=0x00)
25: cond = (cond >= 0x32)
26: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x44, val=0x00)
27: party_effect(sel=0x07, 44 01 0A 00 00)
28: apply_party(count=0x08, op=0x44, val=0x00)
29: cond = (cond >= 0x32)
30: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x01, op=0x44, val=0x00)
31: party_effect(sel=0x08, 44 01 0A 00 00)
32: apply_party(count=0x01, op=0x44, val=0x00)
33: cond = (cond >= 0x32)
34: if cond: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
35: party_effect(sel=0x01, 44 01 0A 00 00)
36: clear_current_tile_event_flag()
37: end_script()
```

**Event 06** — triggers: (0,13)/ANY_DIR, (1,4)/ANY_DIR, (1,11)/ANY_DIR, (2,4)/ANY_DIR, (2,10)/ANY_DIR, (3,5)/ANY_DIR, (3,7)/ANY_DIR, (3,11)/ANY_DIR, (4,4)/ANY_DIR, (4,6)/ANY_DIR, (4,9)/ANY_DIR, (4,11)/ANY_DIR, (4,14)/ANY_DIR, (5,3)/ANY_DIR, (5,5)/ANY_DIR, (6,1)/ANY_DIR, (6,7)/ANY_DIR, (6,9)/ANY_DIR, (7,4)/ANY_DIR, (8,1)/ANY_DIR, (9,4)/ANY_DIR, (9,6)/ANY_DIR, (9,7)/ANY_DIR, (9,11)/ANY_DIR, (10,5)/ANY_DIR, (11,3)/ANY_DIR, (11,4)/ANY_DIR, (11,9)/ANY_DIR, (11,12)/ANY_DIR, (12,1)/ANY_DIR, (12,3)/ANY_DIR, (12,4)/ANY_DIR, (12,6)/ANY_DIR, (13,6)/ANY_DIR, (14,10)/ANY_DIR, (14,12)/ANY_DIR, (15,7)/ANY_DIR

```hex
2b 09 02 06 17 23 01 10 07 1c 64 31 80 01 00 1c 64 1b 14 10 03 12 63 63 63 63 00 00 00 00 00 00 00 00 14 01 08 29
```

```
00: skip_tokens(9)
    # skip -> clear_current_tile_event_flag()
01: show_text_block(str[6] "A bubbling lava pit!")
02: cond = load_var8(group=0x23, index=0x01)
03: if cond: skip_tokens(7)
    # skip -> show_text_basic(str[8] "Your levitation spell saved you!")
04: op_1c_engine_query_to_cond(0x64)
05: for_party(mask=0x80): call(0x01,0x00)
06: op_1c_engine_query_to_cond(0x64)
07: cond = (cond >= 0x14)
08: if cond: skip_tokens(3)
    # skip -> set_abort_and_exit()
09: encounter_setup(monsters=63 63 63 63 00 00 00 00 00 00, flags=00 00)
10: clear_current_tile_event_flag()
11: show_text_basic(str[8] "Your levitation spell saved you!")
12: set_abort_and_exit()
```

**Event 07** — triggers: (1,15)/DIR_E?

```hex
06 07
```

```
00: show_text_popup_style_b(str[7] "<Endurance / Help[")
```

**Event 08** — triggers: (14,2)/ANY_DIR

```hex
2b 01 12 68 68 68 00 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=68 68 68 00 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (1,9)/DIR_S?

```hex
02 09 29
```

```
00: show_text_block(str[9] "To unlock the frozen secrets of Evil, / try Right 46 and Left 23.")
01: set_abort_and_exit()
```

**Event 10** — triggers: (1,1)/0x90

```hex
02 0a 29
```

```
00: show_text_block(str[10] "Dead Eye and Red Duke are in Bozorc's / control in D1 at 14,1.")
01: set_abort_and_exit()
```

**Event 11** — triggers: (14,5)/DIR_N?

```hex
02 0b 29
```

```
00: show_text_block(str[11] "Star Burst is in the center of the / Dead Zone.")
01: set_abort_and_exit()
```

**Event 12** — triggers: (6,13)/DIR_W?

```hex
02 0c 29
```

```
00: show_text_block(str[12] "Thund R. and Aeriel, a Barbarian and / Sorceress team, hang out around t")
01: set_abort_and_exit()
```

**Event 13** — triggers: (8,2)/DIR_N?

```hex
02 0d 29
```

```
00: show_text_block(str[13] "Unlucky Sir Kill and Jed I should not / have eaten so much fatty food. T")
01: set_abort_and_exit()
```

**Event 14** — triggers: (15,15)/DIR_E?

```hex
02 0e 29
```

```
00: show_text_block(str[14] "A sudden draft sends a chill up your / spine as a voice whispers, "The A")
01: set_abort_and_exit()
```

**Event 15** — triggers: (6,3)/DIR_E?

```hex
02 0f 29
```

```
00: show_text_block(str[15] "Look by 0,6 in the Luxus Palace / Royale for the A-1 Todilor.")
01: set_abort_and_exit()
```

## String table

- `[00]` <EMPTY>
- `[01]` Cave exit. Leave (y/n)?
- `[02]` An amnesiac and a wild-eyed ninja rub / their wrists after you free them from / some manacles. For your effort, Harry / Kari and No Name are available for / hire at the Middlegate Inn.
- `[03]` You stumble upon some wayward gems.
- `[04]` Keep / trying
- `[05]` You cringe as a lumbering giant sings / a popular ballad very badly. / Stay and listen (y/n)?
- `[06]`          A bubbling lava pit!
- `[07]` <Endurance / Help[
- `[08]` Your levitation spell saved you!
- `[09]` To unlock the frozen secrets of Evil, / try Right 46 and Left 23.
- `[10]` Dead Eye and Red Duke are in Bozorc's / control in D1 at 14,1.
- `[11]` Star Burst is in the center of the / Dead Zone.
- `[12]` Thund R. and Aeriel, a Barbarian and / Sorceress team, hang out around the / bar in Vulcania. They enjoy Deep Fried / Troll Liver.
- `[13]` Unlucky Sir Kill and Jed I should not / have eaten so much fatty food. They / are trapped by a cave-in in Sarakin's.
- `[14]` A sudden draft sends a chill up your / spine as a voice whispers, "The Air / Disc is at 15,15 in Xabran."
- `[15]` Look by 0,6 in the Luxus Palace / Royale for the A-1 Todilor.
- `[16]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
