# Location 06 — B1

- **event.dat** offset `0x002276`, length **1459** bytes
- **Map:** map screen **6**; overland sector **B1**
- **Record kind:** `standard`
- **Triggers:** 29; **script segments:** 13; **strings:** 19

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,6) | `0x06` | **5** | ANY_DIR |
| (0,11) | `0x0B` | **6** | ANY_DIR |
| (1,3) | `0x13` | **5** | ANY_DIR |
| (1,8) | `0x18` | **6** | ANY_DIR |
| (1,14) | `0x1E` | **9** | ANY_DIR |
| (2,1) | `0x21` | **5** | ANY_DIR |
| (2,12) | `0x2C` | **5** | ANY_DIR |
| (3,1) | `0x31` | **8** | ANY_DIR |
| (3,5) | `0x35` | **9** | ANY_DIR |
| (3,12) | `0x3C` | **10** | ANY_DIR |
| (3,14) | `0x3E` | **7** | ENTER |
| (3,15) | `0x3F` | **10** | ANY_DIR |
| (5,5) | `0x55` | **11** | ENTER |
| (7,2) | `0x72` | **5** | ANY_DIR |
| (7,7) | `0x77` | **5** | ANY_DIR |
| (8,5) | `0x85` | **6** | ANY_DIR |
| (8,9) | `0x89` | **5** | ANY_DIR |
| (9,5) | `0x95` | **6** | ANY_DIR |
| (9,6) | `0x96` | **6** | ANY_DIR |
| (9,8) | `0x98` | **5** | ANY_DIR |
| (9,9) | `0x99` | **2** | ANY_DIR |
| (9,10) | `0x9A` | **5** | ANY_DIR |
| (10,3) | `0xA3` | **6** | ANY_DIR |
| (11,7) | `0xB7` | **5** | ANY_DIR |
| (12,4) | `0xC4` | **1** | 0xD0 |
| (12,12) | `0xCC` | **8** | ANY_DIR |
| (13,15) | `0xDF` | **4** | ANY_DIR |
| (14,1) | `0xE1` | **6** | ANY_DIR |
| (14,11) | `0xEB` | **3** | ENTER |

## Events

**Event 01** — triggers: (12,4)/0xD0

```hex
02 01 09 11 01 0c 18 70 0f
```

```
00: show_text_block(str[1] "Sharp icicles dangle dangerously from / the cave entrance. Go in (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x18, 0x70)
04: end_script()
```

**Event 02** — triggers: (9,9)/ANY_DIR

```hex
0b 0e 00 17 32 00 11 03 02 06 08 14 02 03 2f 30 cd d5 d5 cc c5 fa fa fa fa fa 10 03 02 04 08 14 02 05 2a a0 86 01 00 00 00 00 00 00 00 00 00 00 00 1a 32 01 08 14
```

```
00: set_service_context(str[14] "< Shortcut / Pinehurst", mode=0x00)
01: cond = load_var8(group=0x32, index=0x00)
02: if not cond: skip_tokens(3)
    # skip -> show_text_block(str[3] ""I am your Guardian Pegasus. / What is my name?"")
03: show_text_block(str[6] ""I am your Guardian Pegasus. / Begone, for I will only see / you once a ")
04: wait_key()
05: clear_current_tile_event_flag()
06: show_text_block(str[3] ""I am your Guardian Pegasus. / What is my name?"")
07: op_2f_clear_input_buf()
08: cond = check_answer("MEENU")
09: if cond: skip_tokens(3)
    # skip -> show_text_block(str[5] ""Correct! Your reward is 100,000 gold. / Search and you will find it."")
10: show_text_block(str[4] ""Incorrect. I cannot reward you."")
11: wait_key()
12: clear_current_tile_event_flag()
13: show_text_block(str[5] ""Correct! Your reward is 100,000 gold. / Search and you will find it."")
14: set_treasure(gold/exp=100000, gems=0, items=[0, 0, 0])
15: store_var8(group=0x32, value=0x01)
16: wait_key()
17: clear_current_tile_event_flag()
```

**Event 03** — triggers: (14,11)/ENTER

```hex
02 07 09 11 01 18 00 22 00 28 0f
```

```
00: show_text_block(str[7] "The water in this fountain is frozen / solid. Break it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x22, and=0x00, or=0x28)
04: end_script()
```

**Event 04** — triggers: (13,15)/ANY_DIR

```hex
0b 04 00 02 08 0a 11 07 02 02 27 15 09 22 00 1b 28 10 03 02 0d 08 0f 02 09 19 01 ba 0f 00 10 01 01 0a 08 14
```

```
00: set_service_context(str[4] ""Incorrect. I cannot reward you."", mode=0x00)
01: show_text_block(str[8] "Chopping the tall trees of the Timber- / lands, muscular men sing, "Oh, ")
02: cond = prompt_yes_no(mode=1)
03: if not cond: skip_tokens(7)
    # skip -> end_script()
04: show_text_block(str[2] "Who will be strong enough to / try (1-8)?")
05: selected = select_party_member(mode=1)
06: apply_party(count=0x09, op=0x22, val=0x00)
07: cond = (cond >= 0x28)
08: if cond: skip_tokens(3)
    # skip -> show_text_block(str[9] ""Well done, brute! / Your prize is an Instant Keep."")
09: show_text_block(str[13] "You have failed the challenge. One as / weak as you could never lead the")
10: wait_key()
11: end_script()
12: show_text_block(str[9] ""Well done, brute! / Your prize is an Instant Keep."")
13: add_party_entity(0x01, f3a=0xBA, f40=0x0F, f46=0x00)
14: if cond: skip_tokens(1)
    # skip -> wait_key()
15: show_text_basic(str[10] "*** Backpacks Full ***")
16: wait_key()
17: clear_current_tile_event_flag()
```

**Event 05** — triggers: (0,6)/ANY_DIR, (1,3)/ANY_DIR, (2,1)/ANY_DIR, (2,12)/ANY_DIR, (7,2)/ANY_DIR, (7,7)/ANY_DIR, (8,9)/ANY_DIR, (9,8)/ANY_DIR, (9,10)/ANY_DIR, (11,7)/ANY_DIR

```hex
01 0b 31 00 14 00 07 0d 09 0c 06 33
```

```
00: show_text_basic(str[11] "A blizzard blinds the party!")
01: for_party(mask=0x00): call(0x14,0x00)
02: wait_for_space()
03: engine_call(0x09)
04: map_transition(0x06, 0x33)
```

**Event 06** — triggers: (0,11)/ANY_DIR, (1,8)/ANY_DIR, (8,5)/ANY_DIR, (9,5)/ANY_DIR, (9,6)/ANY_DIR, (10,3)/ANY_DIR, (14,1)/ANY_DIR

```hex
01 0c 31 00 1e 00 07 0d 09 0c 06 33
```

```
00: show_text_basic(str[12] "A snowdrift sweeps the party away!")
01: for_party(mask=0x00): call(0x1E,0x00)
02: wait_for_space()
03: engine_call(0x09)
04: map_transition(0x06, 0x33)
```

**Event 07** — triggers: (3,14)/ENTER

```hex
06 0e
```

```
00: show_text_popup_style_b(str[14] "< Shortcut / Pinehurst")
```

**Event 08** — triggers: (3,1)/ANY_DIR, (12,12)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 c2 e0 c1 c1 e1 c1 e1 c1 c2 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=C2 E0 C1 C1 E1 C1 E1 C1 C2 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 09** — triggers: (1,14)/ANY_DIR, (3,5)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 ce c1 e1 c2 c2 c1 c2 c2 ce 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE C1 E1 C2 C2 C1 C2 C2 CE 00)
04: clear_current_tile_event_flag()
```

**Event 10** — triggers: (3,12)/ANY_DIR, (3,15)/ANY_DIR

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

**Event 11** — triggers: (5,5)/ENTER

```hex
0b 07 00 02 0f 0a 10 01 0f 15 00 7e 10 10 01 0e 56 15 00 76 80 11 04 16 01 e1 11 02 28 01 c8 10 03 02 10 08 14 02 11 2a 50 c3 00 f4 01 00 00 00 00 00 00 00 00 00 1f 00 31 04 90 d0 03 18 00 76 7f 00 18 00 7e ef 00 28 01 e1 08 14
```

```
00: set_service_context(str[7] "The water in this fountain is frozen / solid. Break it (y/n)?", mode=0x00)
01: show_text_block(str[15] "Before you rests antiquated Haart / Hold. Request an audience with Lord ")
02: cond = prompt_yes_no(mode=1)
03: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x00, op=0x7E, val=0x10)
04: end_script()
05: apply_party(count=0x00, op=0x7E, val=0x10)
06: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x00, op=0x76, val=0x80)
07: exec_selector(0x56)
08: apply_party(count=0x00, op=0x76, val=0x80)
09: if not cond: skip_tokens(4)
    # skip -> show_text_block(str[16] "I implore you, retrieve the phaser and / loincloth from the 7th and 8th ")
10: cond = check_monster_present(0x01, 0xE1)
11: if not cond: skip_tokens(2)
    # skip -> show_text_block(str[16] "I implore you, retrieve the phaser and / loincloth from the 7th and 8th ")
12: cond = consume_item(item_id=200, name="Phaser", probe=1)
13: if cond: skip_tokens(3)
    # skip -> show_text_block(str[17] "Thank you so much. The House of Haart / is indebted to you. You shall re")
14: show_text_block(str[16] "I implore you, retrieve the phaser and / loincloth from the 7th and 8th ")
15: wait_key()
16: clear_current_tile_event_flag()
17: show_text_block(str[17] "Thank you so much. The House of Haart / is indebted to you. You shall re")
18: set_treasure(gold/exp=50000, gems=500, items=[0, 0, 0])
19: party_effect(sel=0x00, 31 04 90 D0 03)
20: apply_party_masked(count=0x00, set=0x76, and=0x7F, or=0x00)
21: apply_party_masked(count=0x00, set=0x7E, and=0xEF, or=0x00)
22: cond = consume_item(item_id=225, name="+7 Loincloth", probe=1)
23: wait_key()
24: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` Sharp icicles dangle dangerously from / the cave entrance. Go in (y/n)?
- `[02]` Who will be strong enough to / try (1-8)?
- `[03]` "I am your Guardian Pegasus. / What is my name?"
- `[04]` "Incorrect. I cannot reward you."
- `[05]` "Correct! Your reward is 100,000 gold. / Search and you will find it."
- `[06]` "I am your Guardian Pegasus. / Begone, for I will only see / you once a year."
- `[07]` The water in this fountain is frozen / solid. Break it (y/n)?
- `[08]` Chopping the tall trees of the Timber- / lands, muscular men sing, "Oh, I'm a / lumberjack..." They challenge you to / fell a tree. Accept (y/n)?
- `[09]` "Well done, brute! / Your prize is an Instant Keep."
- `[10]` *** Backpacks Full ***
- `[11]` A blizzard blinds the party!
- `[12]` A snowdrift sweeps the party away!
- `[13]` You have failed the challenge. One as / weak as you could never lead the life / of a lumberjack.
- `[14]` < Shortcut / Pinehurst
- `[15]` Before you rests antiquated Haart / Hold. Request an audience with Lord / Haart (y/n)?
- `[16]` I implore you, retrieve the phaser and / loincloth from the 7th and 8th / centuries!
- `[17]` Thank you so much. The House of Haart / is indebted to you. You shall receive / 250,000 experience. Search now for / your gold and gems.
- `[18]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
