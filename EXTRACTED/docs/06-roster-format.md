# roster.dat — MM2 Amiga Character Record Format

64 fixed-size records × **130 bytes (0x82)** = 8320 bytes total.

File byte order: **little-endian** (matches the DOS/PC original; the Amiga
68000 port byte-swaps word/long fields on load).

## Record Layout

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
| `$16` | 1 | Thievery % | Non-zero for Robbers/Ninjas (and some Clerics?) |
| `$17`–`$19` | 3 | Secondary skills / resistances | Often 5/5/5 or 10/10/10 |
| `$1A`–`$20` | 7 | *Unknown* | Includes `$1F`/`$20` which scales with level |
| `$21` | 1 | **Age** | Starts at 18 |
| `$22`–`$23` | 2 | *Unknown* (word, LE) | |
| `$24` | 1 | **Armor Class** | Effective AC (temp, modified by equipment) |
| `$25` | 1 | **Food** | Ration count |
| `$26` | 1 | **Condition** | 0=Good, 1=Cursed, 2-3=Silenced, 4+=Poisoned, $80+=Dead/Stone/Eradicated |
| `$27` | 1 | **Endurance** (current) | Copied to `$73` on party load |
| `$28`–`$39` | 18 | **Equipped items** | 6 slots × 3 bytes: (item_id, bonus, flags) |
| `$3A`–`$4B` | 18 | **Backpack items** | 6 slots × 3 bytes: (item_id, bonus, flags) |
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
| `$76`–`$81` | 12 | *Padding / unknown* | Usually zero |

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
|-------|-------|
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
| 2 | Sandsobar |
| 3 | Vulcania |
| 4 | Atlantium |
| 5 | Tundara |

## ASM Cross-References

- **Record accessor** (`LAB_47A6` at `$47A6`): Takes party slot index, looks up
  roster index from table at A4-$8696, multiplies by $82, adds base A4-$D5C2.
- **Character sheet display** (`$3A2C`–`$3B10`): Reads $C (sex), $6A (alignment),
  $E (race), $F (class), $71 (level), $5E (HP max), $74 (HP current).
- **Class branch** (`$7B36`): Checks `$F == 2` (Archer) or `$F == 4` (Sorcerer) for
  INT-based spell point calculation; else PER-based (Paladin/Cleric).
- **Party setup** (`$520E`–`$528C`): Copies 8 roster indices from A4-$8696 → A4-$A1A2,
  then iterates characters copying `$15`→`$70` and `$27`→`$73`.
- **Filename** embedded at code offset `$366`: ASCII `"roster.dat\0"`.

## Notes

- Current vs Base stats: Fields `$10`–`$15` / `$27` hold *effective* (possibly
  buffed) values. Fields `$6B`–`$70` / `$73` hold creation-time base values.
  They can diverge through training, items, or temporary effects.
- The `$6A` alignment (used for display) and `$0D` alignment (current) normally
  match but can diverge if alignment shifts during gameplay.
- The `$0B` town byte's bit 7 appears to mark characters currently in the active
  party (cleared when dismissed to an inn).
