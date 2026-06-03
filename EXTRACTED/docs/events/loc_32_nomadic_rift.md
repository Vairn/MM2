# Location 32 — Nomadic Rift

- **event.dat** offset `0x00A99B`, length **1466** bytes
- **Map:** map screen **32**; Nomadic Rift
- **Record kind:** `standard`
- **Triggers:** 45; **script segments:** 28; **strings:** 14

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,3) | `0x03` | **18** | DIR_N? |
| (0,15) | `0x0F` | **3** | DIR_N? |
| (1,0) | `0x10` | **16** | ALWAYS |
| (1,14) | `0x1E` | **3** | ALWAYS |
| (1,15) | `0x1F` | **26** | DIR_N? |
| (2,14) | `0x2E` | **20** | DIR_SPECIAL |
| (3,0) | `0x30` | **16** | ALWAYS |
| (3,6) | `0x36` | **25** | ALWAYS |
| (3,14) | `0x3E` | **3** | ALWAYS |
| (4,4) | `0x44` | **24** | ALWAYS |
| (5,0) | `0x50` | **6** | DIR_N? |
| (6,2) | `0x62` | **6** | DIR_N? |
| (6,4) | `0x64` | **16** | DIR_N? |
| (6,6) | `0x66` | **5** | DIR_N? |
| (6,8) | `0x68` | **5** | DIR_N? |
| (6,10) | `0x6A` | **4** | DIR_N? |
| (6,12) | `0x6C` | **4** | DIR_N? |
| (7,1) | `0x71` | **15** | ANY_DIR |
| (7,2) | `0x72` | **14** | ANY_DIR |
| (7,4) | `0x74` | **13** | ANY_DIR |
| (7,6) | `0x76` | **12** | ANY_DIR |
| (7,8) | `0x78` | **11** | ANY_DIR |
| (7,10) | `0x7A` | **10** | ANY_DIR |
| (7,12) | `0x7C` | **9** | ANY_DIR |
| (7,14) | `0x7E` | **8** | ANY_DIR |
| (7,15) | `0x7F` | **1** | DIR_SPECIAL |
| (8,2) | `0x82` | **6** | ENTER |
| (8,4) | `0x84` | **16** | ENTER |
| (8,6) | `0x86` | **5** | ENTER |
| (8,8) | `0x88` | **5** | ENTER |
| (8,10) | `0x8A` | **4** | ENTER |
| (8,12) | `0x8C` | **4** | ENTER |
| (9,0) | `0x90` | **6** | ENTER |
| (10,10) | `0xAA` | **19** | DIR_SPECIAL |
| (11,0) | `0xB0` | **16** | ALWAYS |
| (11,14) | `0xBE` | **3** | ALWAYS |
| (12,2) | `0xC2` | **17** | DIR_SPECIAL |
| (12,8) | `0xC8` | **21** | ALWAYS |
| (13,0) | `0xD0` | **16** | ALWAYS |
| (13,14) | `0xDE` | **3** | ALWAYS |
| (15,0) | `0xF0` | **2** | ALWAYS |
| (15,1) | `0xF1` | **7** | ANY_DIR |
| (15,8) | `0xF8` | **23** | DIR_SPECIAL |
| (15,12) | `0xFC` | **22** | DIR_SPECIAL |
| (15,14) | `0xFE` | **3** | ALWAYS |

## Events

**Event 01** — triggers: (7,15)/DIR_SPECIAL

```hex
01 01 09 11 01 0c 25 55 0f
```

```
00: show_text_basic(str[1] "Return to the Nomadic Rift (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: map_transition(0x25, 0x55)
04: end_script()
```

**Event 02** — triggers: (15,0)/ALWAYS

```hex
02 02 09 11 22 15 01 13 00 1b 32 10 01 1f 01 13 01 0a 00 00 15 02 13 00 1b 32 10 01 1f 02 13 01 0a 00 00 15 03 13 00 1b 32 10 01 1f 03 13 01 0a 00 00 15 04 13 00 1b 32 10 01 1f 04 13 01 0a 00 00 15 05 13 00 1b 32 10 01 1f 05 13 01 0a 00 00 15 06 13 00 1b 32 10 01 1f 06 13 01 0a 00 00 15 07 13 00 1b 32 10 01 1f 07 13 01 0a 00 00 15 08 13 00 1b 32 10 01 1f 08 13 01 0a 00 00 02 03 07 14
```

```
00: show_text_block(str[2] "A giant treadmill looms before your / party. Step onto it (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(34)
    # skip -> clear_current_tile_event_flag()
03: apply_party(count=0x01, op=0x13, val=0x00)
04: cond = (cond >= 0x32)
05: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x02, op=0x13, val=0x00)
06: party_effect(sel=0x01, 13 01 0A 00 00)
07: apply_party(count=0x02, op=0x13, val=0x00)
08: cond = (cond >= 0x32)
09: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x03, op=0x13, val=0x00)
10: party_effect(sel=0x02, 13 01 0A 00 00)
11: apply_party(count=0x03, op=0x13, val=0x00)
12: cond = (cond >= 0x32)
13: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x04, op=0x13, val=0x00)
14: party_effect(sel=0x03, 13 01 0A 00 00)
15: apply_party(count=0x04, op=0x13, val=0x00)
16: cond = (cond >= 0x32)
17: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x05, op=0x13, val=0x00)
18: party_effect(sel=0x04, 13 01 0A 00 00)
19: apply_party(count=0x05, op=0x13, val=0x00)
20: cond = (cond >= 0x32)
21: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x06, op=0x13, val=0x00)
22: party_effect(sel=0x05, 13 01 0A 00 00)
23: apply_party(count=0x06, op=0x13, val=0x00)
24: cond = (cond >= 0x32)
25: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x07, op=0x13, val=0x00)
26: party_effect(sel=0x06, 13 01 0A 00 00)
27: apply_party(count=0x07, op=0x13, val=0x00)
28: cond = (cond >= 0x32)
29: if cond: skip_tokens(1)
    # skip -> apply_party(count=0x08, op=0x13, val=0x00)
30: party_effect(sel=0x07, 13 01 0A 00 00)
31: apply_party(count=0x08, op=0x13, val=0x00)
32: cond = (cond >= 0x32)
33: if cond: skip_tokens(1)
    # skip -> show_text_block(str[3] "If able, your speed has increased.")
34: party_effect(sel=0x08, 13 01 0A 00 00)
35: show_text_block(str[3] "If able, your speed has increased.")
36: wait_for_space()
37: clear_current_tile_event_flag()
```

**Event 03** — triggers: (0,15)/DIR_N?, (1,14)/ALWAYS, (3,14)/ALWAYS, (11,14)/ALWAYS, (13,14)/ALWAYS, (15,14)/ALWAYS

```hex
01 04 07 1c 18 19 81 00 00 00 14
```

```
00: show_text_basic(str[4] "You have found a weapon!")
01: wait_for_space()
02: op_1c_engine_query_to_cond(0x18)
03: add_party_entity(0x81, f3a=0x00, f40=0x00, f46=0x00)
04: clear_current_tile_event_flag()
```

**Event 04** — triggers: (6,10)/DIR_N?, (6,12)/DIR_N?, (8,10)/ENTER, (8,12)/ENTER

```hex
0e 81
```

```
00: exec_selector(0x81)  # special_129
```

**Event 05** — triggers: (6,6)/DIR_N?, (6,8)/DIR_N?, (8,6)/ENTER, (8,8)/ENTER

```hex
0e 82
```

```
00: exec_selector(0x82)  # special_130
```

**Event 06** — triggers: (5,0)/DIR_N?, (6,2)/DIR_N?, (8,2)/ENTER, (9,0)/ENTER

```hex
0e 83
```

```
00: exec_selector(0x83)  # special_131
```

**Event 07** — triggers: (15,1)/ANY_DIR

```hex
2b 01 12 62 62 62 62 62 62 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=62 62 62 62 62 62 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 08** — triggers: (7,14)/ANY_DIR

```hex
2b 01 12 09 09 09 09 09 09 09 09 09 09 09 05 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=09 09 09 09 09 09 09 09 09 09, flags=09 05)
02: clear_current_tile_event_flag()
```

**Event 09** — triggers: (7,12)/ANY_DIR

```hex
2b 01 12 10 10 10 10 10 10 10 10 10 10 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=10 10 10 10 10 10 10 10 10 10, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 10** — triggers: (7,10)/ANY_DIR

```hex
2b 01 12 1f 1f 1f 1f 1f 1f 1f 1f 1f 1f 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=1F 1F 1F 1F 1F 1F 1F 1F 1F 1F, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 11** — triggers: (7,8)/ANY_DIR

```hex
2b 01 12 38 38 38 38 38 38 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=38 38 38 38 38 38 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 12** — triggers: (7,6)/ANY_DIR

```hex
2b 01 12 50 50 50 50 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=50 50 50 50 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 13** — triggers: (7,4)/ANY_DIR

```hex
2b 01 12 33 33 33 33 38 38 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=33 33 33 33 38 38 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 14** — triggers: (7,2)/ANY_DIR

```hex
2b 01 12 28 28 28 28 50 50 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=28 28 28 28 50 50 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 15** — triggers: (7,1)/ANY_DIR

```hex
2b 01 12 6a 6a 6a 6a 6a 6a 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=6A 6A 6A 6A 6A 6A 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 16** — triggers: (1,0)/ALWAYS, (3,0)/ALWAYS, (6,4)/DIR_N?, (8,4)/ENTER, (11,0)/ALWAYS, (13,0)/ALWAYS

```hex
02 05 2a e8 03 00 00 00 00 00 00 00 00 00 00 00 00 07 14
```

```
00: show_text_block(str[5] "A rumor you heard about the Nomad's / treasure horde suddenly pops back ")
01: set_treasure(gold/exp=1000, gems=0, items=[0, 0, 0])
02: wait_for_space()
03: clear_current_tile_event_flag()
```

**Event 17** — triggers: (12,2)/DIR_SPECIAL

```hex
02 06 29
```

```
00: show_text_block(str[6] "Mystic Castle Xabran, holder of / secrets, lies unknown in the 9th / cen")
01: set_abort_and_exit()
```

**Event 18** — triggers: (0,3)/DIR_N?

```hex
02 07 29
```

```
00: show_text_block(str[7] "The Queen Beetle holds court in E2 at / 11,6.")
01: set_abort_and_exit()
```

**Event 19** — triggers: (10,10)/DIR_SPECIAL

```hex
02 08 29
```

```
00: show_text_block(str[8] "The mighty Serpent King slithers in E3 / at 5,6.")
01: set_abort_and_exit()
```

**Event 20** — triggers: (2,14)/DIR_SPECIAL

```hex
02 09 29
```

```
00: show_text_block(str[9] "The supreme Dragon Lord oversees a / realm of destruction in D1 at 10,12")
01: set_abort_and_exit()
```

**Event 21** — triggers: (12,8)/ALWAYS

```hex
02 0a 29
```

```
00: show_text_block(str[10] "The Mist Warrior keeps his lair atop / Mist Haven at 15,11. He keeps at ")
01: set_abort_and_exit()
```

**Event 22** — triggers: (15,12)/DIR_SPECIAL

```hex
02 0b 29
```

```
00: show_text_block(str[11] "Mighty Nakazawa and Lord Peabody's / servant Sherman were last seen havi")
01: set_abort_and_exit()
```

**Event 23** — triggers: (15,8)/DIR_SPECIAL

```hex
02 0c 29
```

```
00: show_text_block(str[12] "Mr. Wizard, a top scientist, has been / interrupted in his research of t")
01: set_abort_and_exit()
```

**Event 24** — triggers: (4,4)/ALWAYS

```hex
0e 7c
```

```
00: exec_selector(0x7C)
```

**Event 25** — triggers: (3,6)/ALWAYS

```hex
0e 77
```

```
00: exec_selector(0x77)
```

**Event 26** — triggers: (1,15)/DIR_N?

```hex
02 0d 09 10 01 0f 21 1f 10 00 14
```

```
00: show_text_block(str[13] "This torch looks crooked. Adjust / it (y/n)?")
01: cond = prompt_yes_no()
02: if cond: skip_tokens(1)
    # skip -> set_tile((y=1,x=15), 0x10, 0x00)
03: end_script()
04: set_tile((y=1,x=15), 0x10, 0x00)
05: clear_current_tile_event_flag()
```

## String table

- `[00]` <EMPTY>
- `[01]` Return to the Nomadic Rift (y/n)?
- `[02]` A giant treadmill looms before your / party. Step onto it (y/n)?
- `[03]` If able, your speed has increased.
- `[04]` You have found a weapon!
- `[05]` A rumor you heard about the Nomad's / treasure horde suddenly pops back / into your memory, as you spy an / overly large black "X".
- `[06]` Mystic Castle Xabran, holder of / secrets, lies unknown in the 9th / century in C2 at 14,8.
- `[07]` The Queen Beetle holds court in E2 at / 11,6.
- `[08]` The mighty Serpent King slithers in E3 / at 5,6.
- `[09]` The supreme Dragon Lord oversees a / realm of destruction in D1 at 10,12.
- `[10]` The Mist Warrior keeps his lair atop / Mist Haven at 15,11. He keeps at bay / all who trespass with a Dancing Sword.
- `[11]` Mighty Nakazawa and Lord Peabody's / servant Sherman were last seen having / some problems with Amazons near / Native's Cove at 10,1.
- `[12]` Mr. Wizard, a top scientist, has been / interrupted in his research of the / Arcane Wilderness by a very nasty Lich / Lord at 1,14.
- `[13]` This torch looks crooked. Adjust / it (y/n)?

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
