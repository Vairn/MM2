# MM2 Spells & Item "Use Power"

## Spell organisation

Two schools — **Sorcerer** and **Cleric** — each with 9 levels. Each level has
a different number of spells (the counts are identical for both schools):

| Level | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | total |
|-------|---|---|---|---|---|---|---|---|---|-------|
| # spells | 7 | 7 | 6 | 6 | 5 | 5 | 4 | 4 | 4 | **48** |

Within a level the spells are numbered in the order below (alphabetical, which
matches the in-game numbering used by item "use powers").

### Sorcerer

| L | Spells (1..n) |
|---|---------------|
| 1 | Awaken, Detect Magic, Energy Blast, Flame Arrow, Light, Location, Sleep |
| 2 | Eagle Eye, Electric Arrow, Identify Monster, Jump, Levitate, Lloyd's Beacon, Protection from Magic |
| 3 | Acid Stream, Fly, Invisibility, Lightning Bolt, Web, Wizard Eye |
| 4 | Cold Beam, Feeble Mind, Fire Ball, Guard Dog, Shield, Time Distortion |
| 5 | Disrupt, Fingers of Death, Sand Storm, Shelter, Teleport |
| 6 | Disintegration, Entrapment, Fantastic Freeze, Recharge Item, Super Shock |
| 7 | Dancing Sword, Duplication, Etherealize, Prismatic Light |
| 8 | Incinerate, Mega Volts, Meteor Shower, Power Shield |
| 9 | Implosion, Inferno, Star Burst, Enchant Item |

### Cleric

| L | Spells (1..n) |
|---|---------------|
| 1 | Apparition, Awaken, Bless, First Aid, Light, Power Cure, Turn Undead |
| 2 | Cure Wounds, Heroism, Nature's Gate, Pain, Protection From Elements, Silence, Weaken |
| 3 | Cold Ray, Create Food, Cure Poison, Immobilize, Lasting Light, Walk on Water |
| 4 | Acid Spray, Air Transmutation, Cure Disease, Restore Alignment, Surface, Holy Bonus |
| 5 | Air Encasement, Deadly Swarm, Frenzy, Paralyze, Remove Condition |
| 6 | Earth Transmutation, Rejuvenate, Stone to Flesh, Water Encasement, Water Transmutation |
| 7 | Earth Encasement, Fiery Flail, Moon Ray, Raise Dead |
| 8 | Fire Encasement, Fire Transmutation, Mass Distortion, Town Portal |
| 9 | Divine Intervention, Holy Word, Resurrection, Uncurse Item |

(Spell names/levels transcribed from the RPGClassics MM2 spell list.)

## Item "use power" = a flat spell index

The `items.dat` effect byte (`0x0F`) is a **flat spell index**, not a
type/amount pair — it pins one exact spell. Verified byte-exact against the
RPGClassics "Use Ability" column for 49 items (0 mismatches), and corroborated
by the perfect thematic match of every item to its spell.

```
0x00          no use power
0x01..0x7F    non-spell stat boost (hi nibble = kind, lo = amount)
                kind: 0 Max HP, 1 Might, 2 Speed, 3 Accuracy,
                      5 Level (Enchant), 6 Spell-Level
0x81..0xB0    Sorcerer spell #(byte-0x80)   (flat, 1-based)
0xB1..0xE0    Cleric   spell #(byte-0xB0)
```

Flat-index → `level/number`: subtract the per-level counts (7,7,6,6,5,5,4,4,4)
until the running value fits inside a level.

Examples that show the 1:1 design intent:

| Item | byte | Spell | Item | byte | Spell |
|------|------|-------|------|------|-------|
| Wakeup Horn | `0x81` | S1/1 Awaken | Magic Herbs | `0xB4` | C1/4 First Aid |
| Lantern / Torch | `0x85` | S1/5 Light | Hero Medal | `0xB9` | C2/2 Heroism |
| Slumber Club | `0x87` | S1/7 Sleep | Quiet Sling | `0xBD` | C2/6 Silence |
| Monster Tome | `0x8A` | S2/3 Identify Monster | Magic Meal | `0xC0` | C3/2 Create Food |
| Witch Broom | `0x90` | S3/2 Fly | Antidote Ale | `0xC1` | C3/3 Cure Poison |
| Invisocloak | `0x91` | S3/3 Invisibility | Air Disc | `0xC6` | C4/2 Air Transmutation |
| Web Caster | `0x93` | S3/5 Web | Air Talon | `0xCB` | C5/1 Air Encasement |
| Dog Whistle | `0x98` | S4/4 Guard Dog | Cure-all Wand | `0xCF` | C5/5 Remove Condition |
| Hourglass | `0x9A` | S4/6 Time Distortion | Water Talon | `0xD3` | C6/4 Water Encasement |
| Disruptor | `0x9B` | S5/1 Disrupt | Earth Disc | `0xD0` | C6/1 Earth Transmutation |
| Lich Hand | `0x9C` | S5/2 Fingers of Death | Earth Talon | `0xD5` | C7/1 Earth Encasement |
| Energizer | `0xA3` | S6/4 Recharge Item | Moon Rock | `0xD7` | C7/3 Moon Ray |
| Magic Mirror | `0xA6` | S7/2 Duplication | Ruby Ankh | `0xD8` | C7/4 Raise Dead |
| Meteor Bow | `0xAB` | S8/3 Meteor Shower | Fire Talon | `0xD9` | C8/1 Fire Encasement |
| Star Bow | `0xAF` | S9/3 Star Burst | Divine Mace | `0xDD` | C9/1 Divine Intervention |

Discrepancy worth noting: the byte gives **Dove's Blood = C4/3 Cure Disease**
where RPGClassics lists "C4/2"; the byte-derived value is thematically correct
and preferred.

## spells.dat (decoded)

`spells.dat` is 256 bytes = **96 spells × 2 bytes** (Sorcerer records 0..47,
Cleric records 48..95, in the same flat order as the item effect index) plus
**64 trailing bytes** of leftover/unused data (stray item-name bytes), which are
preserved verbatim on round-trip.

Each 2-byte record encodes the spell's runtime **cost and cast restrictions**.
The decode was cross-validated against the manual (Appendix B) cost/gem/type
columns: **gems 96/96, cast 96/96, outdoor 12/12, SP 95/96** (only Meteor
Shower differs — its base SP is computed per-monster in game code).

### byte0 — cast restriction + gem cost

| bits | meaning |
|------|---------|
| `0x40` | combat-only (cannot cast outside combat) |
| `0x80` | non-combat-only (cannot cast in combat) |
| neither | castable **Anytime** |
| `0x10` | **special / computed cost** — gem cost is hard-coded in game code (>15 gems, per-plus, per-monster). The low nibble is *not* the real gem count for these: Duplication, Star Burst, Enchant Item, Divine Intervention, Uncurse Item. |
| `0x20` | observed only on the *non-combat* special-cost spells (alongside `0x10`) |
| low nibble `0x0F` | gem cost (0..15) when not special |

### byte1 — outdoor flag + spell-point cost

| bits | meaning |
|------|---------|
| `0x80` | **outdoor-only** spell (matched 12/12 the manual's "Outdoor" type) |
| bits 6-4 (`0x70`) | per-level SP multiplier (the *X* in "X/L"), used when the low nibble is 0 |
| low nibble `0x0F` | flat spell-point cost (1..10); **0 ⇒ cost is per caster level** |

Example records (file bytes → decode):

| Spell | bytes | decode |
|-------|-------|--------|
| S1/3 Energy Blast | `41 10` | combat, 1 gem, 1/L |
| S2/1 Eagle Eye | `80 A0` | non-combat, **outdoor**, 2/L |
| S3/5 Web | `42 03` | combat, 2 gems, 3 SP |
| S8/3 Meteor Shower | `48 80` | combat, **outdoor**, 8 gems, +per-monster SP (special) |
| S9/3 Star Burst | `54 C0` | combat, outdoor, **special-cost** (20 gems hard-coded) |
| C2/4 Pain | `40 02` | combat, 2 SP |
| C9/4 Uncurse Item | `B2 0A` | non-combat, **special-cost** (50 gems hard-coded), 10 SP |

The manual's prose (cost / TYPE / OBJECT / description) for all 96 spells is
transcribed into the reference tables in `editor/src/core/Spells.cpp` and
`tools/mm2_spells.py` (`SPELL_META`).

## FAQ cross-check (`Might and Magic FAQ.txt` §3-7)

The Schultz FAQ item table (v2.2) matches our `items.dat` decode for almost all
256 entries. Two use-power typos in the FAQ — trust `items.dat` bytes:

| Item | FAQ lists | `0x0F` byte | Correct spell |
|------|-----------|-------------|---------------|
| Hourglass (`0x9A`) | S5-0 (invalid slot) | `0x9A` | S4/6 Time Distortion |
| Cure-all Wand (`0xC3`) | C6-1 Earth Transmutation | `0xCF` | C5/5 Remove Condition |

Spell names, school order (7/7/6/6/5/5/4/4/4), and class-forbidden columns in
the FAQ align with this doc. Guild/temple **prices** in the FAQ are approximate
and often wrong — use [`31-spell-sources.md`](31-spell-sources.md) /
[`shop_tables.json`](../shop_tables.json) (ASM tables).

## Runtime effect state — Eagle Eye / Wizard Eye (vision timers)

Two of the few spell effects already wired into the event VM are the overhead
"eye" vision spells, which persist as **per-step duration timers** in game state:

| Spell | Type | A4 timer | Event var group |
|-------|------|----------|-----------------|
| S2/1 Eagle Eye | Outdoor 5×5 overhead | `-$79A0` | `0x2B` |
| S3/6 Wizard Eye | Indoor/maze 5×5 overhead | `-$799F` | `0x2C` |

- **Cast:** Eagle Eye handler @ `0xA91C` sets `-$79A0`; Wizard Eye @ `0xAD20` sets
  `-$799F`. Each adds `5` per caster level (manual "5 steps/L"), clamped to
  `0xFA`/`0xFF`.
- **Tick:** `0x4672` runs per step — picks the timer for the current view mode
  (`-$79E2`: outdoor → Eagle Eye, dungeon → Wizard Eye), `subq #1` while nonzero,
  and renders the overhead while active.
- **Fountain of Clairvoyance** (Middlegate event 42) writes both timers to `50`
  via `store_var8`/`OP_1A` (groups `0x2B`/`0x2C`) — see
  [`06-roster-format.md`](06-roster-format.md).
- **Port TODO:** `mm2_gamestate.h` still names these `MM2_GS_CLASS_QUEST_CNT` /
  `MM2_GS_QUEST_COUNTER_B` (correct offsets, stale names). Rename + implement the
  decrement/overhead render when the spell-effect system is built.

## Tools / code

- `tools/mm2_spells.py` — spell tables, `decode_effect(byte)` (items), full
  `SPELL_META`, and the spells.dat record codec (`decode_record`,
  `encode_record`, `load_spells_dat`).
- `tools/decode_spells.py` — dumps `spells.dat` divided per spell with
  cost/gems/cast/outdoor; `--check` validates the bytes against the manual.
- `editor/src/core/Spells.{h,cpp}` — 96-spell `kSpells[]` reference table +
  flat-index/item-effect helpers.
- `editor/src/core/SpellsFile.{h,cpp}` — `SpellsFile` model (2-byte record
  decode/encode/load/save), used by the editor's **Spells** section.
- Effect dispatch in ASM: `combat_use_item_handler` `0x133EC`.
