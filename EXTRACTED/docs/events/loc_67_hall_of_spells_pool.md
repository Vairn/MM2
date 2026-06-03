# Location 67 — Hall of Spells pool

- **event.dat** offset `0x015BF6`, length **2037** bytes
- **Map:** Hall of Spells pool
- **Record kind:** `mixed_pool`
- **Triggers:** 56; **script segments:** 41; **strings:** 0

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (2,11) | `0x2B` | **1** | 0xFF |
| (1,5) | `0x15` | **1** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,1) | `0x01` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (1,5) | `0x15` | **2** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,2) | `0x02` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (1,5) | `0x15` | **3** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,3) | `0x03` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (1,5) | `0x15` | **4** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,4) | `0x04` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (1,5) | `0x15` | **5** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,5) | `0x05` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (1,5) | `0x15` | **6** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,6) | `0x06` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (1,5) | `0x15` | **7** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,7) | `0x07` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (1,5) | `0x15` | **8** | 0x12 |
| (0,0) | `0x00` | **27** | 0x32 |
| (1,0) | `0x10` | **1** | 0x1F |
| (0,8) | `0x08` | **18** | 0x01 |
| (0,10) | `0x0A` | **0** | 0x00 |
| (0,1) | `0x01` | **1** | 0x02 |
| (0,2) | `0x02` | **7** | 0x0D |
| (0,9) | `0x09` | **21** | 0x01 |
| (0,12) | `0x0C` | **0** | ALWAYS |
| (0,1) | `0x01` | **12** | 0x18 |
| (14,15) | `0xEF` | **12** | 0x18 |
| (0,15) | `0x0F` | **255** | 0x02 |
| (0,3) | `0x03` | **26** | 0x15 |
| (0,1) | `0x01` | **26** | 0x16 |
| (0,1) | `0x01` | **7** | 0x14 |
| (15,15) | `0xFF` | **2** | 0x04 |
| (0,9) | `0x09` | **17** | 0x01 |
| (1,2) | `0x12` | **113** | 0x71 |
| (7,1) | `0x71` | **113** | 0x91 |
| (9,1) | `0x91` | **0** | 0x00 |

## Events

**Event 00** — triggers: (0,10)/0x00, (0,10)/0x00, (0,10)/0x00, (0,10)/0x00, (0,10)/0x00, (0,10)/0x00, (0,10)/0x00, (0,10)/0x00, (0,12)/ALWAYS, (9,1)/0x00

```hex
09 0c 1e b1
```

```
00: cond = prompt_yes_no()
01: map_transition(0x1E, 0xB1)
```

**Event 01** — triggers: (2,11)/0xFF, (1,5)/0x12, (1,0)/0x1F, (1,0)/0x1F, (1,0)/0x1F, (1,0)/0x1F, (1,0)/0x1F, (1,0)/0x1F, (1,0)/0x1F, (1,0)/0x1F, (0,1)/0x02

```hex
02 05 29
```

```
00: show_text_block(str[5])
01: set_abort_and_exit()
```

**Event 02** — triggers: (1,5)/0x12, (15,15)/0x04

```hex
02 06 29
```

```
00: show_text_block(str[6])
01: set_abort_and_exit()
```

**Event 03** — triggers: (1,5)/0x12

```hex
02 07 29
```

```
00: show_text_block(str[7])
01: set_abort_and_exit()
```

**Event 04** — triggers: (1,5)/0x12

```hex
02 08 29
```

```
00: show_text_block(str[8])
01: set_abort_and_exit()
```

**Event 05** — triggers: (1,5)/0x12

```hex
02 09 29
```

```
00: show_text_block(str[9])
01: set_abort_and_exit()
```

**Event 06** — triggers: (1,5)/0x12

```hex
02 0a 29
```

```
00: show_text_block(str[10])
01: set_abort_and_exit()
```

**Event 07** — triggers: (1,5)/0x12, (0,2)/0x0D, (0,1)/0x14

```hex
16 01 fb 11 0c 16 01 fc 11 0a 16 01 fd 11 08 28 01 fe 11 06 02 0b 28 01 fb 28 01 fc 28 01 fd 19 01 df 00 00 07 0f
```

```
00: cond = check_monster_present(0x01, 0xFB)
01: if not cond: skip_tokens(12)
    # skip -> end_script()
02: cond = check_monster_present(0x01, 0xFC)
03: if not cond: skip_tokens(10)
    # skip -> end_script()
04: cond = check_monster_present(0x01, 0xFD)
05: if not cond: skip_tokens(8)
    # skip -> end_script()
06: cond = consume_item(item_id=254, name="N-19 Capitor", probe=1)
07: if not cond: skip_tokens(6)
    # skip -> end_script()
08: show_text_block(str[11])
09: cond = consume_item(item_id=251, name="J-26 Fluxer", probe=1)
10: cond = consume_item(item_id=252, name="M-27 Radicon", probe=1)
11: cond = consume_item(item_id=253, name="A-1 Todilor", probe=1)
12: add_party_entity(0x01, f3a=0xDF, f40=0x00, f46=0x00)
13: wait_for_space()
14: end_script()
```

**Event 08** — triggers: (1,5)/0x12

```hex
02 0c 29
```

```
00: show_text_block(str[12])
01: set_abort_and_exit()
```

**Event 12** — triggers: (0,1)/0x18, (14,15)/0x18

```hex
02 10 19 01 7e c8 07 19 01 9a c8 07 19 01 9f c8 07 11 02 07 14 01 11 07 14
```

```
00: show_text_block(str[16])
01: add_party_entity(0x01, f3a=0x7E, f40=0xC8, f46=0x07)
02: add_party_entity(0x01, f3a=0x9A, f40=0xC8, f46=0x07)
03: add_party_entity(0x01, f3a=0x9F, f40=0xC8, f46=0x07)
04: if not cond: skip_tokens(2)
    # skip -> show_text_basic(str[17])
05: wait_for_space()
06: clear_current_tile_event_flag()
07: show_text_basic(str[17])
08: wait_for_space()
09: clear_current_tile_event_flag()
```

**Event 17** — triggers: (0,9)/0x01

*(empty segment)*

**Event 18** — triggers: (0,1)/0x01, (0,2)/0x01, (0,3)/0x01, (0,4)/0x01, (0,5)/0x01, (0,6)/0x01, (0,7)/0x01, (0,8)/0x01

```
You've got personality!
```

*Plain-text record in the 0xFF pool (not an opcode script).*

**Event 21** — triggers: (0,9)/0x01

```
A bedraggled cleric and a battered / robber are being used as targets by a / group of monsters. Intercede (y/n)?
```

*Plain-text record in the 0xFF pool (not an opcode script).*

**Event 26** — triggers: (0,3)/0x15, (0,1)/0x16

```
The Elder Druid of Druid's Cave has in / his hands the secrets needed to cast / Divine Intervention.
```

*Plain-text record in the 0xFF pool (not an opcode script).*

**Event 27** — triggers: (0,0)/0x32, (0,0)/0x32, (0,0)/0x32, (0,0)/0x32, (0,0)/0x32, (0,0)/0x32, (0,0)/0x32, (0,0)/0x32

```
Holy Word is best found facing south / while walking through the woods.
```

*Plain-text record in the 0xFF pool (not an opcode script).*

**Event 113** — *(missing script segment)*

**Event 255** — *(missing script segment)*

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
