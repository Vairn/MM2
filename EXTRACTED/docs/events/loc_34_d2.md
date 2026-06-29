# Location 34 — D2

- **event.dat** offset `0x00B481`, length **1395** bytes
- **Map:** map screen **34**; overland sector **D2**
- **Record kind:** `standard`
- **Triggers:** 22; **script segments:** 17; **strings:** 15

## Tile triggers

| Tile (y,x) | pos | Event | Condition |
|------------|-----|-------|-----------|
| (0,7) | `0x07` | **12** | 0xA0 |
| (4,1) | `0x41` | **13** | ANY_DIR |
| (4,5) | `0x45` | **3** | DIR_E? |
| (4,8) | `0x48` | **4** | DIR_E? |
| (5,5) | `0x55` | **1** | ANY_DIR |
| (6,2) | `0x62` | **5** | ANY_DIR |
| (7,1) | `0x71` | **10** | ANY_DIR |
| (7,6) | `0x76` | **13** | ANY_DIR |
| (8,4) | `0x84` | **5** | ANY_DIR |
| (8,6) | `0x86` | **7** | 0x90 |
| (8,7) | `0x87` | **15** | ANY_DIR |
| (9,3) | `0x93` | **9** | ANY_DIR |
| (10,2) | `0xA2` | **5** | ANY_DIR |
| (10,10) | `0xAA` | **14** | ANY_DIR |
| (11,7) | `0xB7` | **2** | DIR_N? |
| (12,1) | `0xC1` | **8** | ANY_DIR |
| (12,7) | `0xC7` | **5** | ANY_DIR |
| (13,14) | `0xDE` | **14** | ANY_DIR |
| (14,2) | `0xE2` | **5** | ANY_DIR |
| (14,4) | `0xE4` | **5** | ANY_DIR |
| (14,7) | `0xE7` | **5** | ANY_DIR |
| (14,14) | `0xEE` | **6** | DIR_N? |

## Events

**Event 01** — triggers: (5,5)/ANY_DIR

```hex
06 01
```

```
00: show_text_popup_style_b(str[1] "Royal / Territory")
```

**Event 02** — triggers: (11,7)/DIR_N?

```hex
06 02
```

```
00: show_text_popup_style_b(str[2] "Queen's / Orchard")
```

**Event 03** — triggers: (4,5)/DIR_E?

```hex
06 03
```

```
00: show_text_popup_style_b(str[3] "<- Luxus / Palace")
```

**Event 04** — triggers: (4,8)/DIR_E?

```hex
06 04
```

```
00: show_text_popup_style_b(str[4] "Desert of / Desolation")
```

**Event 05** — triggers: (6,2)/ANY_DIR, (8,4)/ANY_DIR, (10,2)/ANY_DIR, (12,7)/ANY_DIR, (14,2)/ANY_DIR, (14,4)/ANY_DIR, (14,7)/ANY_DIR

```hex
2b 01 12 9c 9c 9c 9c 00 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
01: encounter_setup(monsters=9C 9C 9C 9C 00 00 00 00 00 00, flags=00 00)
02: clear_current_tile_event_flag()
```

**Event 06** — triggers: (14,14)/DIR_N?

```hex
2b 0e 0b 06 00 02 05 08 02 0d 0a 10 01 0f 16 00 d5 10 05 02 06 0a 10 01 0f 12 69 69 69 69 69 69 00 00 00 00 00 00 0c 3a 07
```

```
00: skip_tokens(14)
    # skip -> map_transition(0x3A, 0x07)
01: service_sign(idx=0x06 -> sign 47 [47.anm], pos=0x00)
02: show_text_block(str[5] "The Great Luxus Palace Royale stands / before you majestically. Banners ")
03: wait_key()
04: show_text_block(str[13] "flutter from every turret while / colorfully clad guardsman pace / the b")
05: cond = prompt_yes_no(mode=1)
06: if cond: skip_tokens(1)
    # skip -> cond = check_monster_present(0x00, 0xD5)
07: end_script()
08: cond = check_monster_present(0x00, 0xD5)
09: if cond: skip_tokens(5)
    # skip -> map_transition(0x3A, 0x07)
10: show_text_block(str[6] "Guards rudely deny access, exclaiming, / "No key, no admittance!" Attack")
11: cond = prompt_yes_no(mode=1)
12: if cond: skip_tokens(1)
    # skip -> encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
13: end_script()
14: encounter_setup(monsters=69 69 69 69 69 69 00 00 00 00, flags=00 00)
15: map_transition(0x3A, 0x07)
```

**Event 07** — triggers: (8,6)/0x90

```hex
2b 06 0b 06 00 02 07 0a 10 01 0c 22 76 13 f5 97 99 00 00 00 00 00 00 00 14
```

```
00: skip_tokens(6)
    # skip -> clear_current_tile_event_flag()
01: service_sign(idx=0x06 -> sign 47 [47.anm], pos=0x00)
02: show_text_block(str[7] "Dreary Mandagual's Keep casts a dark / shadow. A chill crawls up your sp")
03: cond = prompt_yes_no(mode=1)
04: if cond: skip_tokens(1)
    # skip -> encounter_setup_b(data=F5 97 99 00 00 00 00 00 00 00)
05: map_transition(0x22, 0x76)
06: encounter_setup_b(data=F5 97 99 00 00 00 00 00 00 00)
07: clear_current_tile_event_flag()
```

**Event 08** — triggers: (12,1)/ANY_DIR

```hex
02 08 09 11 01 18 00 22 00 c8 0f
```

```
00: show_text_block(str[8] "A tree hangs full with ripe magic / fruit. Pick and eat (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x22, and=0x00, or=0xC8)
04: end_script()
```

**Event 09** — triggers: (9,3)/ANY_DIR

```hex
02 08 09 11 02 1c 08 18 09 43 00 82 0f
```

```
00: show_text_block(str[8] "A tree hangs full with ripe magic / fruit. Pick and eat (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(2)
    # skip -> end_script()
03: op_1c_engine_query_to_cond(0x08)
04: apply_party_masked(count=0x09, set=0x43, and=0x00, or=0x82)
05: end_script()
```

**Event 10** — triggers: (7,1)/ANY_DIR

```hex
02 08 09 11 01 18 00 23 00 c8 0f
```

```
00: show_text_block(str[8] "A tree hangs full with ripe magic / fruit. Pick and eat (y/n)?")
01: cond = prompt_yes_no()
02: if not cond: skip_tokens(1)
    # skip -> end_script()
03: apply_party_masked(count=0x00, set=0x23, and=0x00, or=0xC8)
04: end_script()
```

**Event 12** — triggers: (0,7)/0xA0

```hex
03 0b 07 03 0c 07 0e 97
```

```
00: show_text(str[11] "A plaque left by the Jurors of Mount / Farview reads: "For each characte")
01: wait_for_space()
02: show_text(str[12] ""and recognition in the form of a '+'. / Classes must go alone, without ")
03: wait_for_space()
04: exec_selector(0x97)
```

**Event 13** — triggers: (4,1)/ANY_DIR, (7,6)/ANY_DIR

```hex
22 05 05 11 02 2b 01 12 c2 c1 e1 e0 e0 c1 c1 c1 c2 c2 c2 03 14
```

```
00: cond = (era in [5..5])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup(monsters=C2 C1 E1 E0 E0 C1 C1 C1 C2 C2, flags=C2 03)
04: clear_current_tile_event_flag()
```

**Event 14** — triggers: (10,10)/ANY_DIR, (13,14)/ANY_DIR

```hex
22 06 06 11 02 2b 01 13 ce c1 e1 c1 c2 ce c2 c2 c2 00 14
```

```
00: cond = (era in [6..6])
01: if not cond: skip_tokens(2)
    # skip -> clear_current_tile_event_flag()
02: skip_tokens(1)
    # skip -> clear_current_tile_event_flag()
03: encounter_setup_b(data=CE C1 E1 C1 C2 CE C2 C2 C2 00)
04: clear_current_tile_event_flag()
```

**Event 15** — triggers: (8,7)/ANY_DIR

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

## String table

- `[00]` <EMPTY>
- `[01]` Royal / Territory
- `[02]` Queen's / Orchard
- `[03]` <- Luxus / Palace
- `[04]` Desert of / Desolation
- `[05]` The Great Luxus Palace Royale stands / before you majestically. Banners ...
- `[06]` Guards rudely deny access, exclaiming, / "No key, no admittance!" Attack (y/n)?
- `[07]` Dreary Mandagual's Keep casts a dark / shadow. A chill crawls up your spine / as a blood-curdling scream warns, / "Enter at your own risk!" (y/n)?
- `[08]` A tree hangs full with ripe magic / fruit. Pick and eat (y/n)?
- `[09]` Horns blow, heralding a pack of fierce / hounds chasing a stag, as the royal / hunting party rides swiftly by.
- `[10]` Castle guards confront you, "Poachers / of royal games will be severely / punished!"
- `[11]` A plaque left by the Jurors of Mount / Farview reads: "For each character / class, a test of true standing must be / completed. If you have already / completed a test your reward is 5 / million experience points ..."
- `[12]` "and recognition in the form of a '+'. / Classes must go alone, without the / rest of the party or in the company of / Robbers, who must aid at least one / class to earn their mark."
- `[13]` flutter from every turret while / colorfully clad guardsman pace / the battlements. Enter (y/n)?
- `[14]` <EMPTY>

---

*Generated by `tools/build_event_location_docs.py` from `event.dat`. Decoder: `tools/decode_event.py`.*
