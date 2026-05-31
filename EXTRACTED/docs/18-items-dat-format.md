# items.dat — MM2 Item Record Format

256 records × **20 bytes (0x14)** = 5120 bytes.

File byte order: **little-endian** (matches the Blitz3D reference editor
`Item_ED.bb` and the PC original; the Amiga 68000 port byte-swaps the gold
field on load at asm `0x26030`).

## Record Layout

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| `0x00`–`0x0B` | 12 | **Name** | ASCII, space-padded |
| `0x0C` | 1 | separator/pad | editor writes `0` |
| `0x0D` | 1 | **Forbidden-class** bitmask | `K P A C S R N B` = `0x80 .. 0x01`; **set bit = that class CANNOT use the item** (`0x00` = usable by all). See below. |
| `0x0E` | 1 | **Bonus** (equipped) | `hi nibble = type`, `lo nibble = amount` |
| `0x0F` | 1 | **Effect** (used) | `hi nibble = type`, `lo nibble = amount` |
| `0x10` | 1 | **Damage** | weapon damage component |
| `0x11` | 1 | pad/unused | editor writes `0` |
| `0x12`–`0x13` | 2 | **Gold** | `uint16` LE (shop price in gp) |

## Class byte `0x0D` — class **restriction** mask (who CANNOT use)

This byte is an **exclusion** mask, *not* a "usable by" mask. A **set** bit means
the corresponding class is *forbidden* from using the item; a clear bit means the
class **can** use it. So the predicate is:

```
class X can use item  <=>  (mask & class_bit_X) == 0
```

Bit order (MSB→LSB): `Knight 0x80, Paladin 0x40, Archer 0x20, Cleric 0x10,
Sorcerer 0x08, Robber 0x04, Ninja 0x02, Barbarian 0x01`.

> The legacy Blitz3D editor (`Item_ED.bb`) labels this field "Useable By" and
> swaps the middle bits (`Robber 0x10, Cleric 0x08, Sorcerer 0x04`). Both are
> wrong; the order/semantics above are validated directly against `items.dat`.

Confirmation from the real `items.dat` (an exclusion mask is the only reading
that fits, and it pins the bit order):

| Item(s) | byte | clear bits → can use | sanity check |
|---------|------|----------------------|--------------|
| Small Club, Large Club | `0x00` | everyone | basic weapon, no restriction |
| Katana, Wakizashi, Nunchakas | `0x7D` | Knight + Ninja | classic ninja weapons |
| Holy Cudgel | `0xAF` | Paladin + Cleric | holy/personality item |
| Sage Dagger | `0xD7` | Archer + Sorcerer | intellect item |
| Dagger / all swords | `0x10`+ | Cleric bit **set** | Clerics can't wield blades |
| Mace, Maul, Cudgel | `0x10` clear | Cleric **can** | blunt weapons allowed |

Every bladed weapon sets `0x10` (Cleric forbidden) while every blunt weapon
leaves it clear — the canonical "clerics use blunt weapons only" rule — which is
what fixes Cleric=`0x10` (vs the legacy editor's Robber=`0x10`). The game enforces
the restriction silently on equip (no dedicated message string in `str.dat`).

## Bonus byte `0x0E` — "special power" (passive, while equipped)

The high nibble is **a category table index**, not a free value; the low
nibble is the magnitude (**amount 0 == no bonus**, so plain gear reads `0x00` =
Might+0). The runtime calls this the item's *special power* (message
`"No special power"` at code `0xe1e8`).

Decoded by clustering all 256 item names, then **cross-validated against the
RPGClassics / Fandom MM2 item tables** (which list each item's
"Equipment Bonus"). Three anchors fix the whole table:

```
elemental shields:  Magic/Fire/Elec/Cold/Acid Shield -> 6/7/8/9/13
metal armor tiers:  Silver/Iron/Bronze/Gold          -> 10/11/12/5
"+N Mgt" gear (Power Club, Mauler Mace, Force Sword)  -> type 0  = Might
"+N AC"  gear (Emerald Ring, Elven Cloak, Defense Ring) -> type 15 = AC
```

| Type | Meaning | Representative items (RPGClassics bonus) |
|------|---------|------------------------------------------|
| 0  | **Might** | Power Club (+3), Mauler Mace (+6), Force Sword (+15) |
| 1  | Intellect | Genius/Wizard/Sage Staff, Amber Skull |
| 2  | Personality | Ego Scimitar (+12), Holy Cudgel (+15), Hero Medal (+4) |
| 3  | Speed | Fast/Quick/Rapid/Swift, Speed Boots, Elven Boots |
| 4  | Accuracy | Accurate Swd (+10), Exacto Spear (+6), Ruby Tiara |
| 5  | Luck | Lucky Knife, Chance Sword, **Gold Shield** |
| 6  | Magic resist | **Magic Shield**, Magic Charm, Soul Scythe |
| 7  | Fire resist | **Fire Shield**, Fiery/Blazing/Flaming weapons |
| 8  | Electricity resist | **Electric Shd**, Shock/Voltage/Storm |
| 9  | Cold resist | **Cold Shield**, Cold Blade, Ice Scimitar, Freeze Wand |
| 10 | Energy resist | **Silver Shld**, Energy/Flash/Photon, Star Bow |
| 11 | Sleep resist | **Iron Shield**, Slumber Club, Quiet Sling |
| 12 | Max HP (PHP) | **Bronze Shld**, Sonic Whip, Giant Sling, Cure-all Wand |
| 13 | Acid resist | **Acid Shield**, Acidic Sword |
| 14 | Thievery | Looter Knife, Thief's Pick, Skeleton Key, Stealth Cape |
| 15 | **Armor Class** | Emerald Ring (+15), Elven Cloak (+5), Defense Ring (+2) |

Notes:
- 0 = Might and 15 = AC bracket the table with the two physical combat stats;
  `amount == 0` is the universal "no bonus" sentinel.
- Types 6–13 are elemental/condition **resistances**; 1–5 are secondary stat
  bonuses; 14 is the Thievery skill.
- **Type 12 source conflict**: RPGClassics consistently labels the type-12
  items "PHP" (Max HP / Permanent Hit Points); the Fandom list calls them
  "Poison". Treated as Max HP here; resolve with an ASM resistance-apply trace.
- Endurance has no observed equip-bonus item.

## Effect byte `0x0F` — "use power" (charged, via U-Use)

**Unlike the bonus byte, this is NOT a (type, amount) pair.** The whole byte is
a **flat spell index** that pins one exact spell. Triggered by U-Use ("No
charges" message @ code `0xe1f9`), dispatched through `combat_use_item_handler`
(`0x133EC`). Encoding (verified byte-exact vs the RPGClassics "Use Ability"
column for 49 items, 0 mismatches):

```
0x00          no use power
0x01..0x7F    non-spell stat boost (nibble packed):
                hi = kind {0 Max HP, 1 Might, 2 Speed, 3 Accuracy,
                           5 Level(Enchant), 6 Spell-Level}
                lo = amount
0x81..0xB0    Sorcerer spell, flat index = byte - 0x80
0xB1..0xE0    Cleric   spell, flat index = byte - 0xB0
```

The flat index walks the spell list level by level. **Each level has a
different number of spells** (same counts for both schools), so you need this
table to convert a flat index to `school level/number`:

| Level | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | total |
|-------|---|---|---|---|---|---|---|---|---|-------|
| # spells | 7 | 7 | 6 | 6 | 5 | 5 | 4 | 4 | 4 | 48 |

So `flat → level/number`: subtract 7,7,6,6,5,5,4,4,4 in turn until the
remainder fits the level; the remainder (1-based) is the spell number.

Worked examples (item → effect byte → decoded spell), all thematically exact:

| Item | byte | Spell | Item | byte | Spell |
|------|------|-------|------|------|-------|
| Wakeup Horn | `0x81` | S1/1 Awaken | Sonic Whip | `0xBB` | C2/4 Pain |
| Lantern | `0x85` | S1/5 Light | Antidote Ale | `0xC1` | C3/3 Cure Poison |
| Web Caster | `0x93` | S3/5 Web | Cure-all Wand | `0xCF` | C5/5 Remove Condition |
| Hourglass | `0x9A` | S4/6 Time Distortion | Moon Rock | `0xD7` | C7/3 Moon Ray |
| Disruptor | `0x9B` | S5/1 Disrupt | Ruby Ankh | `0xD8` | C7/4 Raise Dead |
| Magic Mirror | `0xA6` | S7/2 Duplication | Divine Mace | `0xDD` | C9/1 Divine Intervention |
| Meteor Bow | `0xAB` | S8/3 Meteor Shower | Holy Cudgel | `0xDE` | C9/2 Holy Word |

The full spell lists (alphabetical within each level = the in-game numbering)
live in `tools/mm2_spells.py` and `core/ItemsFile.cpp` (`describeItemEffect`).
See `EXTRACTED/docs/19-spells-and-item-use.md`.

Note: the byte gives **Dove's Blood = C4/3 Cure Disease** (RPGClassics lists
"C4/2"); the byte-derived value is thematically correct and is preferred.

## Cross-references

- C model + tables: `editor/src/core/ItemsFile.{h,cpp}`
  (`kItemBonusTypeNames`, `kItemEffectTypeNames`).
- Python decoder: `tools/decode_items.py`.
- UI: `editor/src/sections/ItemsSection.cpp` (named type combos).
- Blitz3D reference: `b3dmm2/Item_ED.bb`, `b3dmm2/items.txt`.
- ASM: gold byte-swap `0x26030`; use dispatch `0x133EC`; messages `0xe1e8`
  ("No special power") / `0xe1f9` ("No charges").
