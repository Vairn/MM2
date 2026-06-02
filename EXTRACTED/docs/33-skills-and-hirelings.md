# MM2 Skills and Hirelings

Source: **[FAQ]** = `Might and Magic FAQ.txt` §§3-3/3-4 (Andrew Schultz v2.2.0).
Cross-references: [`32-character-mechanics.md`](32-character-mechanics.md),
[`06-roster-format.md`](06-roster-format.md) (roster bytes), [`28-town-services.md`](28-town-services.md).

---

## 1. Skill sellers (15 total, 3 per town) [FAQ §3-3-1]

All entries are **FAQ-sourced**. Town interior coordinates use **(x, y)** notation as in the FAQ.

### Middlegate

| Skill | NPC / Location name | Tile (x,y) | Cost (gp) | Effect |
|-------|---------------------|-----------|-----------|--------|
| Cartographer | Otto Mapper | (0,15) | 10 | Auto-maps current area |
| Mountaineering | Sir Edmund Hilary | (3,12) | 2,000 | Party can traverse mountains (2 needed) |
| Pathfinder | Track and Trail | (1,9) | 1,000 | Party can traverse forests (2 needed) |

### Atlantium

| Skill | NPC / Location name | Tile (x,y) | Cost (gp) | Effect |
|-------|---------------------|-----------|-----------|--------|
| Athlete | The Olympic Trial | (6,2) | 500 | +5 Speed |
| Heroism | Hippomenes + Atalanta | (9,2) | 1,000 | +1 to ALL stats |
| Linguist | Odysseus' Tongue | (8,4) | 500 | +5 Intelligence; can read glyphs |

### Tundara

| Skill | NPC / Location name | Tile (x,y) | Cost (gp) | Effect |
|-------|---------------------|-----------|-----------|--------|
| Crusader | Saracen's Denial | (2,14) | 250 | — (cleric/paladin benefit) |
| Merchant | International Market | (5,13) | 1,000 | Buy/sell at 2× price instead of 4×/¼× |
| Navigator | Colombus's Sextant | (8,13) | 750 | Navigation aid |

### Vulcania

| Skill | NPC / Location name | Tile (x,y) | Cost (gp) | Effect |
|-------|---------------------|-----------|-----------|--------|
| Soldier | Sergeant Pain School | (0,3) | 500 | +5 Endurance |
| Gladiator | Disembowlments R Us | (3,9) | 500 | +5 Strength/Might |
| Arms Master | Proficiency Expert | (15,3) | 500 | +5 Accuracy |

### Sandsobar

| Skill | NPC / Location name | Tile (x,y) | Cost (gp) | Effect |
|-------|---------------------|-----------|-----------|--------|
| Gambler | Sandy Dunes | (3,0) | ? | +5 Luck |
| Diplomacy | Embassy | (3,4) | ? | +5 Personality |
| Pickpocket | Sly's Opportunities | (0,5) | ? | +5 Thievery |

> Costs for Sandsobar skills not listed in the FAQ. [FAQ §3-3-1]

---

## 2. Thievery sellers in Sandsobar Dungeon [FAQ §3-3-1]

Located underground (Sandsobar cavern):

| NPC | Tile (x,y) | Cost (gp) | Effect | Notes |
|-----|-----------|-----------|--------|-------|
| **Rinaldo Jr.** | (0,12) | 700 | **+10 Thievery** | Repeatable ("any number of times"); behind some ninjas |
| Maxwell | (13,9) | 250 | **−5 Thievery** | **AVOID** |

> "I've been able to enlist Rinaldo Jr.'s services several times. Maybe it's that I went to see Maxwell
> afterwards. I haven't experimented enough. In any case, it's worth it to visit Rinaldo Jr." [FAQ §3-3-1]
>
> The FAQ specifically notes: **"THE ONES ABOVE CAN OCCUR ANY NUMBER OF TIMES."** [FAQ §3-3-1]

---

## 3. Skill mechanics [FAQ §3-3-2]

- **Pathfinder**: Two party members with Pathfinder → party can pass through all forests.
- **Mountaineering**: Two with Mountaineering → party can cross any mountain.
  Combined with Walk on Water (C3/6), these let the party go anywhere on the outdoor map.
- **Cartographer**: Enables auto-map in dungeons/towns.
- **Critical rule**: If the skill holder is **unconscious or dead, the skill does not work**. [FAQ §3-3-2]
- All other skills give +5 to a stat, except **Heroism** which gives +1 to all stats.

---

## 4. Hirelings roster (A–X, 24 total) [FAQ §3-4-1]

**Column key:**
- **GARC** = Gender / Alignment / Race / Class
  - Gender: F = Female, M = Male, N = Neuter
  - Alignment: N = Neutral, E = Evil, G = Good
  - Race: H = Human, D = Dwarf, G = Gnome, E = Elf, O = Half-Orc
  - Class: K = Knight, P = Paladin, A = Archer, C = Cleric, R = Robber, S = Sorcerer, N = Ninja, B = Barbarian
- **Town**: 1 = Middlegate, 2 = Atlantium, 3 = Tundara, 4 = Vulcania, 5 = Sandsobar
- **TSL** = Total Spell Levels (spell points not listed — computed from spell level × INT/PER bonus)
- **Gems**: starting gems
- **Age** = Level + 17 in all cases
- **Cost**: base hire cost; increases by 50% (rounded down) per level gained; max 50,000 gp
- All hirelings start with **40 food**

| # | Name | GARC | Twn | Mig | Int | Per | End | Spd | Acc | Luc | HP | TSL | Lv | Age | Gem | Cost | Skills |
|---|------|------|-----|-----|-----|-----|-----|-----|-----|-----|----|-----|----|-----|-----|------|--------|
| A | Sir Hyron | MNHK | 1 | 15 | 8 | 5 | 15 | 15 | 17 | 10 | 14 | 1 | 1 | 18 | 0 | 2 | — |
| B | Drog | MNHB | 1 | 17 | 4 | 8 | 17 | 12 | 13 | 11 | 18 | 1 | 1 | 18 | 0 | 2 | — |
| C | H K Phooey | MGON | 5 | 13 | 13 | 13 | 13 | 13 | 13 | 13 | 9 | 1 | 1 | 18 | 0 | 2 | — |
| D | Thund R | MGHB | 4 | 19 | 5 | 13 | 17 | 15 | 18 | 21 | 42 | 3 | 3 | 20 | 0 | 10 | Cru |
| E | Aeriel | FGHS | 4 | 10 | 19 | 15 | 13 | 17 | 13 | 13 | 17 | 3 | 3 | 20 | 30 | 12 | Car |
| F | Big Bootay | MNGC | 2 | 15 | 7 | 19 | 17 | 9 | 16 | 11 | 41 | 5 | 5 | 22 | 0 | 35 | Lin |
| G | Cleogotcha | FEEA | 2 | 15 | 16 | 12 | 16 | 19 | 19 | 13 | 44 | 5 | 5 | 22 | 0 | 35 | Ath |
| H | Harry Kiri | MNDN | 1 | 17 | 13 | 13 | 15 | 17 | 17 | 14 | 42 | 6 | 6 | 23 | 0 | 25 | Arm |
| I | No Name | MGDA | 1 | 17 | 7 | 17 | 17 | 17 | 17 | 17 | 53 | 6 | 6 | 23 | 0 | 55 | Cru |
| J | Gertrude | FEOB | 3 | 18 | 5 | 2 | 17 | 15 | 15 | 18 | 90 | 7 | 7 | 24 | 0 | 55 | Mou Pat |
| K | Rat Fink | MEGR | 3 | 15 | 15 | 3 | 16 | 18 | 16 | 19 | 50 | 7 | 7 | 24 | 0 | 45 | Gam Pic |
| L | Friar Fly | MEHC | 5 | 10 | 15 | 18 | 15 | 18 | 13 | 9 | 60 | 9 | 9 | 26 | 0 | 200 | Dip Nav |
| M | Dark Mage | MEES | 5 | 9 | 20 | 13 | 17 | 18 | 12 | 15 | 57 | 9 | 9 | 26 | 80 | 250 | Lin Mer |
| N | Red Duke | MEHP | 4 | 21 | 15 | 19 | 17 | 17 | 17 | 13 | 95 | 11 | 11 | 28 | 90 | 500 | Cru Sol |
| O | Dead Eye | MEEA | 4 | 19 | 19 | 14 | 17 | 19 | 21 | 12 | 94 | 11 | 11 | 28 | 110 | 600 | Arm Pat |
| P | Nakazawa | MGHN | 2 | 19 | 15 | 15 | 17 | 17 | 21 | 19 | 110 | 13 | 13 | 30 | 110 | 700 | Dip Her |
| Q | Sherman | MGHP | 2 | 17 | 9 | 19 | 15 | 19 | 17 | 25 | 95 | 13 | 13 | 30 | 0 | 1,200 | Cru Mer |
| R | Flailer | NNHK | 2 | 35 | 3 | 2 | 26 | 7 | 8 | 27 | 180 | 14 | 14 | 31 | 130 | 2,000 | Gla Sol |
| S | Fumbler | MNOR | 2 | 17 | 13 | 13 | 19 | 23 | 17 | 30 | 132 | 16 | 16 | 33 | 0 | 2,000 | Ath Gam |
| T | Sir Kill | MEHK | 3 | 30 | 15 | 15 | 27 | 21 | 26 | 19 | 200 | 15 | 15 | 32 | 0 | 4,000 | Arm Gla |
| U | Jed I | MGHA | 3 | 22 | 25 | 17 | 19 | 23 | 29 | 21 | 165 | 15 | 15 | 32 | 160 | 6,000 | Arm Mou |
| V | Holey Moley | MNGC | 2 | 17 | 17 | 27 | 19 | 17 | 17 | 25 | 156 | 19 | 19 | 36 | 190 | 15,000 | Dip Gam |
| W | Slick Pick | MNGR | 2 | 25 | 25 | 30 | 25 | 25 | 25 | 75 | 260 | 25 | 25 | 42 | 0 | 25,000 | Gam Nav |
| X | Mr Wizard | MGHS | 5 | 21 | 90 | 21 | 45 | 60 | 21 | 50 | 220 | 21 | 21 | 38 | 210 | 50,000 | Lin Mer |

Roster breakdown: 20 males, 3 females, 1 neuter; 8 of each alignment; 3 of each class except 2 paladins and 4 archers. [FAQ §3-4-1]

**Skill abbreviations:** Arm = Arms Master, Ath = Athlete, Car = Cartographer, Cru = Crusader, Dip = Diplomat, Gam = Gambler, Gla = Gladiator, Her = Hero, Lin = Linguist, Mer = Merchant, Mou = Mountaineering, Nav = Navigator, Pat = Pathfinder, Pic = Pickpocket, Sol = Soldier.

---

## 5. Hireling locations [FAQ §3-4-2]

| Hirelings | How to find |
|-----------|-------------|
| **A (Sir Hyron) + B (Drog)** | (0,15) under Middlegate. Requires Nordon's quest completed and Nordonna to ask for help. Can jump over most combats en route. |
| **C (H K Phooey)** | Sandsobar: order Chop Suey at the inn and exit by the door. |
| **D (Thund R) + E (Aeriel)** | Vulcania: buy Troll Liver from the inn; fight (6 old misers, 6 cripples), then they introduce themselves. |
| **F (Big Bootay) + G (Cleogotcha)** | Atlantium jail. Two fights: first random, second = four giant ogres. Skill potion recommended. |
| **H (Harry Kiri) + I (No Name)** | Vulcania dungeon (1,14). Can teleport directly; Levitate recommended if walking. |
| **J (Gertrude) + K (Rat Fink)** | Tundara (15,10), end of anti-magic corridor. Teleport from one square north to avoid Snowbeast. |
| **L (Friar Fly) + M (Dark Mage)** | Hillstone Castle (8,4). Give #1 player all skill potions (~level 30) to make guards flee. |
| **N (Red Duke) + O (Dead Eye)** | D2 (11,1) — held by Bozorc the Orc (with Carnage Spirit + 248 orcs). Use skill potions + Lightning Bolt. |
| **P (Nakazawa) + Q (Sherman)** | B4 (10,1) among Crazed Natives. Can kill unprepared parties. |
| **T (Sir Kill) + U (Jed I)** | Sarakin's Mine (7,2/3). Many undead; use Holy Word. |
| **V (Holey Moley) + W (Slick Pick)** | Dawn's Mist Bog D4 (3,7) at (4,11). Toughest pickup. |
| **X (Mr Wizard)** | E2 (1,14), approach from south to fight a great beetle (vs lich lord from north). |

---

## 6. Hireling rules [FAQ §3-4]

- Hirelings only charge gold **when they rest** (at the inn). Possible to use a hireling for spells then dismiss without paying. [FAQ §3-4-2]
- Hirelings have **all spells** for their class/spell level. [FAQ §3-2]
- Hireling cost increases by **50% (rounded down)** per level gained; maximum cost = **50,000 gp**. [FAQ §3-4-1]
- Hirelings are barred from some locations (Murray's Cave uses `OP_15 val=0x80` — "No hirelings allowed!"). [ASM: doc 28 §5.3]
- Hireling flag: roster `$0B` bit 7. Dead/eradicated hirelings are skipped by party iterators. [ASM: doc 06]
