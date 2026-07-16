# roster.dat — MM2 Amiga Character Record Format

**8320 bytes** = 48 playable character records + **2080-byte global save stream**
(slots 48–63 in the editor round-trip container).

File byte order: **little-endian** for multibyte fields (matches the DOS/PC
original; the Amiga 68000 port byte-swaps word/long fields on load for roster
stats at `0x3290`).

## File layout

| Region | File offset | Size | Notes |
|--------|-------------|------|-------|
| Characters 0–47 | `$0000` | `48 × $82` = **6240** (`$1860`) | Loaded into `A4-$2A3E` (`Read` @ `0x3290`) |
| Global stream | `$1860` | **2080** (`$820`) | Serialized by `save_game_state` @ **`0x823C`**; editor stores as roster slots 48–63 |

The global section is **not** sixteen independent 130-byte character records in
runtime — it is a packed quest/calendar/combat-state blob. The editor keeps slots
48–63 as an opaque round-trip container for those 2080 bytes.

## Record Layout (per character, `$82` bytes)

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| `$00`–`$0A` | 11 | **Name** | Null-terminated, space-padded |
| `$0B` | 1 | **Town/Inn** | Low 7 bits: 1–5 (see enum). Bit 7: "in party" flag |
| `$0C` | 1 | **Sex** | 0 = Male, non-zero = Female |
| `$0D` | 1 | **Alignment** (current) | 0 = Good, 1 = Neutral, 2 = Evil |
| `$0E` | 1 | **Race** | Index into race string table (A4-$86F8) |
| `$0F` | 1 | **Class** | Index into class string table (A4-$86D8) |
| `$10` | 1 | Might (current) | |
| `$11` | 1 | Intelligence (current) | |
| `$12` | 1 | Personality (current) | |
| `$13` | 1 | Speed (current) | |
| `$14` | 1 | Accuracy (current) | |
| `$15` | 1 | Luck (current) | Copied to `$70` on party load |
| `$16` | 1 | **Thievery %** | Robber 30% / Ninja 10% at creation; +1%/level (FAQ) — see [`32-character-mechanics.md`](32-character-mechanics.md) |
| `$17`–`$19` | 3 | **Secondary skills** | Skill bytes (Cartographer, Pathfinder, …); sellers in [`33-skills-and-hirelings.md`](33-skills-and-hirelings.md). Often 5/5/5 or 10/10/10 in saves |
| `$1A`–`$20` | 7 | *Quest / progress flags* | Likely class-quest and world-quest bits; FAQ condition strings (`[PALADINS ONLY]`, `CORAK'S SOUL`, `ADMIT 8 PASS`) — **not ASM-mapped** |
| `$21` | 1 | **Age** | Starts at 18 |
| `$22`–`$23` | 2 | *Unknown* (word, LE) | |
| `$24` | 1 | **Armor Class** | Effective AC (temp, modified by equipment) |
| `$25` | 1 | **Food** | Ration count |
| `$26` | 1 | **Condition** | Living = OR bitfield (≤`$7F`): `$01` Cursed, `$02` Silenced, `$04` Diseased, `$08` Poisoned, `$10` Asleep, `$20` Paralyzed, `$40` Unconscious. Fatal whole-byte: `$81` Dead (`0x1EEC8`), `$82` Stoned (`0x1EEDA`), `$FF` Eradicated (`0x1EEEC`); any other ≥`$80` treated as eradicated. |
| `$27` | 1 | **Endurance** (current) | Copied to `$73` on party load |
| `$28`–`$2D` | 6 | **Equipped item ids** | 6 contiguous item ids (0 = empty). **Structure-of-Arrays** — see below |
| `$2E`–`$33` | 6 | **Equipped charges** | per-item charge counter (Blitz3D "plus"); decremented on use @ `$138A6` |
| `$34`–`$39` | 6 | **Equipped flags** | per-item flags (Blitz3D "efft"); cleared on depletion @ `$138EE` |
| `$3A`–`$3F` | 6 | **Backpack item ids** | 6 contiguous item ids (0 = empty) |
| `$40`–`$45` | 6 | **Backpack charges** | per-item charge counter; decremented on use @ `$137F0` |
| `$46`–`$4B` | 6 | **Backpack flags** | per-item flags; cleared on depletion @ `$13838` |
| `$4C`–`$57` | 12 | **Spell book** | Bitmask of known spells (Sorc + Cleric levels) |
| `$58`–`$59` | 2 | **SP max** | Word (LE) |
| `$5A`–`$5B` | 2 | **SP current** | Word (LE) |
| `$5C`–`$5D` | 2 | **Gems** | Word (LE) |
| `$5E`–`$5F` | 2 | **HP max** | Word (LE). Display code: `MOVE.W $5E(A0)` |
| `$60`–`$61` | 2 | **HP** (temp/prior max?) | Word (LE) |
| `$62`–`$65` | 4 | **Experience** | Long (LE) |
| `$66`–`$69` | 4 | **Gold** | Long (LE) |
| `$6A` | 1 | **Alignment** (base/perm) | Used for display: index into table at A4-$870C |
| `$6B` | 1 | Might (base) | |
| `$6C` | 1 | Intelligence (base) | |
| `$6D` | 1 | Personality (base) | |
| `$6E` | 1 | Speed (base) | |
| `$6F` | 1 | Accuracy (base) | |
| `$70` | 1 | Luck (base) | |
| `$71` | 1 | **Level** | Display code: `MOVE.B $71(A0)` |
| `$72` | 1 | **Secondary Level** (spell level) | |
| `$73` | 1 | Endurance (base) | |
| `$74`–`$75` | 2 | **HP current** | Word (LE). Display code: `MOVE.W $74(A0)` |
| `$76`–`$77` | 2 | **Temp/score word** | u16 LE; purpose unknown |
| `$78` | 1 | **Script/work flag** | Transient script/work flag |
| `$79` | 1 | **Class-quest / guild mask** | Bit 7: in-game class **'+'** display; low bits AND `A4-$66A9[town]` for mage-guild purchase gate @ `0x1E41A`; OR'd on quest completion @ `0x9FE0`. **Bug:** @ `0x9FBE` the reward handler uses table **`A4-$7154`** as a roster **offset** before `$79`, so the OR often hits **`$7E`** (padding) or the next record — see [`36-class-quest-hp-bug.md`](36-class-quest-hp-bug.md). |
| `$7A`–`$81` | 10 | *Padding / unknown* | Mostly zero |

## Equipped / backpack items — Structure-of-Arrays (`$28`–`$4B`)

The 36 bytes `$28`–`$4B` are **NOT** six interleaved `(id, charges, flags)`
triplets. They are stored as **three parallel 6-byte runs** per group: six item
ids, then six charges, then six flags — for equipped (`$28`/`$2E`/`$34`) then
backpack (`$3A`/`$40`/`$46`). An earlier audit flagged the array-of-structs
model as suspect; the 68k ASM (and the Blitz3D editor) confirm SoA.

### 68k ASM evidence (`EXTRACTED/mm2.capstone.annotated.asm`)

- **OP_16 item scan** `event_op16` @ `0x16520`: inner loop index `i` 0..5 reads
  `move.b $3a(a0),d0` and `move.b $28(a0),d0` with `a0 = base + i` (`adda.l`),
  i.e. **stride 1** over six contiguous ids at `+$3A+i` / `+$28+i`
  (`0x16572`, `0x1655C`).
- **OP_19 add-item** `event_op19` @ `0x165D8`: for the chosen empty slot `i` it
  writes the three correlated bytes to `+$3A+i`, `+$40+i`, `+$46+i`
  (`0x16648`, `0x1665A`, `0x1666C`) — spaced **6 apart** (id / charges / flags).
- **Equip** (backpack→equipped not shown) and **unequip** move all three runs
  together: `$28→$3A`, `$2E→$40`, `$34→$46` @ `0x0EB9A`/`0x0EBB8`/`0x0EBD6`;
  the reverse `$3A→$28`, `$40→$2E`, `$46→$34` @ `0x0EF8A`/`0x0EFA8`/`0x0EFC6`.
- **Whole-character copy** copies each run independently:
  equipped `$28→$28`, `$2E→$2E`, `$34→$34` @ `0x0E09A`/`0x0E0BA`/`0x0E0DA`;
  backpack `$3A`/`$40`/`$46` @ `0x04716`/`0x04736`/`0x04756`.
- **Item-use charge decrement** (`combat_use_item_handler` @ `0x133EC` region):
  backpack `tst/subq $40(a0)`; on depletion `move.b #$ff,$3a(a0)` + `clr.b
  $46(a0)` @ `0x137F0`–`0x13838`; equipped `tst/subq $2e(a0)`; `move.b
  #$ff,$28(a0)` + `clr.b $34(a0)` @ `0x138A6`–`0x138EE`. This identifies the
  `$2E`/`$40` run as **charges** and the `$34`/`$46` run as **flags**.

### Blitz3D editor cross-check (`b3dmm2/Character.bb`)

The reference editor reads the record sequentially into six `equiped[6]`, six
`equiplus[6]`, six `equiefft[6]`, then six `backpack[6]`, `backplus[6]`,
`backefft[6]` — exactly the SoA order above. (`equiplus`/`backplus` = charges;
`equiefft`/`backefft` = flags.)

## Global save stream (file `$1860`, runtime via `0x823C`)

`save_game_state` @ **`0x823C`** reads/writes a **6240-byte** (`$1860`) serialized
blob (same size as the character section — stored in roster slots 48–63 in the
editor). Reload path @ **`0x86F6`** re-parses when the session timer fires.
Tail byte indices used by the editor mirror the memcpy order in that routine; see
`editor/src/core/RosterGlobalTail.h`. C codec accessors:
`EXTRACTED/decomp/mm2_roster_codec.h` (`MM2_ROSTER_TAIL_*`,
`mm2_roster_tail_u8/u16`). **Party selection persists here**: 8×u16 roster
indices @ tail `+$028` (→ `-$796A`, `0xFFFF` = empty; stream loop @ `0x8302`)
then u16 party size @ `+$038` (→ `-$795A` @ `0x836A`); the event/quest bank
(hireling A..X availability) @ `+$7CE` (→ `-$798B`, write @ `0x84A2`).

### Direct global bytes (also event-script targets)

| A4 offset | Field | Notes |
|-----------|-------|-------|
| `-$79DE` | `day[era]` | word[10] calendar day 1..180 per era |
| `-$79CA` | `year[era]` | word[10] year per era |
| `-$79B6` | `era` | Timeline index 0..9 |
| `-$79B5` | `era_low` | Low byte of era word; **g=0x84** for event gating |
| `-$79B4` | `time_subday` | 256 ticks = 1 day |
| `-$79AB` | `light_factor` | Light / Lasting Light; darkness drain |
| `-$79AA` | `endgame_score_a` | Level+10 on class-quest finish |
| `-$79A9` | `endgame_score_b` | Level+20 on class-quest finish |
| `-$79A8` | `special_quest` | **g=0x23** |
| `-$79A7` / `-$79A6` | achievement flags | Endgame screen counters |
| `-$79A5` | `encounter_mod` | Encounter difficulty modifier |
| `-$79A4`–`-$79A1` | elemental talismans | **g=0x27..0x2A** (Acwalandar / Shalwend / Pyrannaste / Gralkor) |
| `-$79A0` | **Eagle Eye** step timer (was mislabeled "class-quest counter") | **g=0x2B** |
| `-$799F` | **Wizard Eye** step timer (was mislabeled "secondary quest counter") | **g=0x2C** |
| `-$799E` | temple donation bits | **g=0x13** (OR town bits 1/2/4/8/16) |
| `-$799D`–`-$7999` | `input_state` | Input-state bytes |
| `-$7996` | guardian cave gate | **g=0x32** |
| `-$7990` | Tundara cavern lever | **g=0x33** |
| `-$798F` | Castle Xabran gate | **g=0x3B** |
| `-$798E` | Dawn's Mist / resort gate | **g=0x3C** |
| `-$798D` | `period_flag_b` | **g=0x3D** — calendar period flag B (day 60/120/180 reset), **not** a dungeon gate |
| `-$798C` | `period_flag_a` | **g=0x3E** — calendar period flag A (day 60/120/180 reset) |
| `-$798B` + n | event bank | **g=0x00..0x17** — 24-byte general quest/event bank. Doubles as **hireling A..X availability**: the roster/party UI gates hireling-page entries (page offset `$18`) on `bank[letter] != 0` (`tst.b (-$798B,d0)` @ `0x586`, `0x7B6`, `0xB68`, `0xD1C`, `0x292B2`). Persisted in the tail @ `+$7CE` |
| `-$7995` + n | second gate bank | **g=0x80..0x83** (4 bytes) |
| `-$796C` | move counter | |
| `-$796B` | encounter mode | |
| `-$79B2` | `new_game_flag` | |
| `-$79B0` / `-$79AF` | sounds / walk beep | bit 0 each |
| `-$79AE` / `-$79AD` | disposition / delay | |

**New-game reset** @ `0x19B28` clears `-$79A6`..`-$79AB`, `-$799F`, `-$79A0`,
`-$79A5`, and `-$79A1`..`-$79A4`.

### Event variable resolver @ `0x15620` (OP_17 / OP_1A / OP_32)

| Group `g` | A4 target | Meaning |
|-----------|-----------|---------|
| `0x00`..`0x17` | `-$798B` + id | 24-byte general quest/event bank |
| `0x13` | `-$799E` | Temple donation quest bitfield |
| `0x23` | `-$79A8` | Special quest byte |
| `0x27`..`0x2A` | `-$79A4` + (id−`0x27`) | Elemental talisman flags |
| `0x2B` | `-$79A0` | **Eagle Eye** vision step timer (outdoor 5×5 overhead) |
| `0x2C` | `-$799F` | **Wizard Eye** vision step timer (indoor/maze 5×5 overhead) |
| `0x32` | `-$7996` | Guardian cave gate |
| `0x33` | `-$7990` | Tundara cavern lever |
| `0x3B` | `-$798F` | Castle Xabran gate |
| `0x3C` | `-$798E` | Dawn's Mist / resort gate |
| `0x3D` | `-$798D` | Calendar period flag B |
| `0x3E` | `-$798C` | Calendar period flag A |
| `0x80`..`0x83` | `-$7995` + (id−`0x80`) | Second 4-byte gate bank |
| `0x84` | `-$79B5` (era low) | Era index for event gating |

### Eagle Eye / Wizard Eye vision timers (`-$79A0` / `-$799F`)

Groups `0x2B`/`0x2C` are **not** quest counters — they are the two overhead-vision
spell-effect step timers (decrement one-per-step; a quest counter would not).

- **Cast handlers:** Eagle Eye sets `-$79A0` @ `0xA91C`; Wizard Eye sets `-$799F`
  @ `0xAD20`. Both add `5` per caster level (manual "5 steps/L") and clamp at
  `0xFA`/`0xFF`.
- **Per-step ticker @ `0x4672`:** selects the timer for the current view mode
  (`-$79E2`: outdoor → `-$79A0` Eagle Eye, dungeon → `-$799F` Wizard Eye), does
  `subq #1` while nonzero, and while active calls the overhead-render path
  (`-$7C3E`/`-$7EAE`).
- **Fountain of Clairvoyance** (Middlegate loc 00, event 42) shortcuts the cast:
  it writes both timers to `0x32` (50 steps) directly via `store_var8` (OP_1A),
  granting 50 steps of each.
- **Port note:** `mm2_gamestate.h` still names these `MM2_GS_CLASS_QUEST_CNT`
  (`-0x79A0`) and `MM2_GS_QUEST_COUNTER_B` (`-0x799F`); the *offsets are correct*
  but the names are stale. Rename to Eagle/Wizard Eye timers when the spell-effect
  system is implemented (see `19-spells-and-item-use.md`).

## Enumerations

### Race (`$0E`)
| Value | Race |
|-------|------|
| 0 | Human |
| 1 | Elf |
| 2 | Dwarf |
| 3 | Gnome |
| 4 | Half-Orc |

### Class (`$0F`)
| Value | Class |
|-------|------|
| 0 | Knight |
| 1 | Paladin |
| 2 | Archer |
| 3 | Cleric |
| 4 | Sorcerer |
| 5 | Robber |
| 6 | Ninja |
| 7 | Barbarian |

### Town (`$0B` & 0x7F)
| Value | Town |
|-------|------|
| 1 | Middlegate |
| 2 | Atlantium |
| 3 | Tundara |
| 4 | Vulcania |
| 5 | Sandsobar |

## ASM Cross-References

- **Record accessor** (`LAB_47A6` at `$47A6`): Takes party slot index, looks up
  roster index from table at A4-$8696, multiplies by $82, adds base A4-$2A3E.
- **Character sheet display** (`$3A2C`–`$3B10`): Reads $C (sex), $6A (alignment),
  $E (race), $F (class), $71 (level), $5E (HP max), $74 (HP current).
- **Class branch** (`$7B36`): Checks `$F == 2` (Archer) or `$F == 4` (Sorcerer) for
  INT-based spell point calculation; else PER-based (Paladin/Cleric).
- **Party setup** (`$520E`–`$528C`): Copies 8 roster indices from A4-$8696 → A4-$A1A2,
  then iterates characters copying `$15`→`$70` and `$27`→`$73`.
- **Roster load** (`$3290`): `Read` **$1860** bytes into A4-$2A3E; per-record word
  swap for SP/gems/HP/XP/gold and +$76 word; then `save_game_state` parse @ `$823C`.
- **Roster save** (`$34A4` path): Writes **$1860** character bytes back, then global
  serializer (mirror of `$823C`).
- **Event g-var resolver** (`$15620`): Maps script `g` ids to `A4` bytes (OP_17/1A/32).
- **Filename** embedded at code offset `$366`: ASCII `"roster.dat\0"`.

## FAQ cross-reference (quest flags)

Class and world quests (FAQ §§6–7) gate events on roster or global flags.
Examples to trace in `event.dat` / `exec_selector` conditions:

- Paladin-only squares, Corak soul token, eight-passenger ferry, Nordon goblet state
- Byte **`$79`** (class-quest / guild mask) is **ASM-confirmed** for mage-guild gate;
  bytes **`$1A`–`$20`** remain candidates for per-quest progress

See [`06-event-dat-format.md`](06-event-dat-format.md) § FAQ event validation.

## Notes

- Current vs Base stats: Fields `$10`–`$15` / `$27` hold *effective* (possibly
  buffed) values. Fields `$6B`–`$70` / `$73` hold creation-time base values.
  They can diverge through training, items, or temporary effects.
- The `$6A` alignment (used for display) and `$0D` alignment (current) normally
  match but can diverge if alignment shifts during gameplay.
- The `$0B` town byte's bit 7 appears to mark characters currently in the active
  party (cleared when dismissed to an inn).
- C codec: `EXTRACTED/decomp/mm2_roster_codec.{h,c}` — editor: `editor/src/core/RosterFile.{h,cpp}`.
