# Commerce and World Services (MM2)

Covers training/healing cost formulas, the magic-item day-bonus cycle, bar
food/drink effects, inter-town portal connections, and the Fly spell landing
grid. All data is **FAQ-sourced** (Schultz v2.2) unless tagged **ASM-confirmed**.

Cross-references:
- [`28-town-services.md`](28-town-services.md) — service tile layout, OP_0E dispatch, ASM handlers
- [`32-character-mechanics.md`](32-character-mechanics.md) — HP/level, XP tables
- [`19-spells-and-item-use.md`](19-spells-and-item-use.md) — item bonus encoding

---

## 1. Training costs

**FAQ §3-6 (lines 1171-1173). FAQ-sourced; ASM handler `0x9B48` / `0x9BCA`
(doc 28, §1.3) needs cost-formula trace to confirm.**

```
Training cost = current_level × town_index × 50 gp
```

| Town       | Town index |
|------------|------------|
| Middlegate | 1          |
| Tundara    | 2          |
| Sandsobar  | 2          |
| Vulcania   | 4          |
| Atlantium  | 5          |

Example: Training at Atlantium at level 5 costs 5 × 5 × 50 = **1,250 gp**.

> FAQ note: Atlantium gives almost double the hit points of Middlegate. Worth
> the extra cost — try to train there from as early as level 5.

---

## 2. Healing costs

**FAQ §3-6 (line 1174). FAQ-sourced; ASM handler `0x1D208` (temple) — cost
formula not yet traced.**

```
Healing cost = current_level × town_index × 10 gp
             (×10 if character is dead)
             (×100 if character is eradicated)
```

Town indices same as training (Middlegate=1 … Atlantium=5).

Example: Resurrecting (dead) a level 10 character at Middlegate =
10 × 1 × 10 × 10 = **1,000 gp**.

---

## 3. Magic item day-bonus cycle

**FAQ §3-6 (lines 1181-1193). FAQ-sourced; ASM calendar mechanism not yet
traced.**

Magic item bonus (`+N`) shops stock items on a 30-day cycle. The bonus `+`
value for items bought on day D:

| D  | + | D  | + | D  | + | D  | + | D  | + | D  | + |
|----|---|----|---|----|---|----|---|----|---|----|---|
| 1  | 1 | 6  | 1 | 11 | 1 | 16 | 1 | 21 | 1 | 26 | 1 |
| 2  | 0 | 7  | 0 | 12 | 0 | 17 | 0 | 22 | 0 | 27 | 0 |
| 3  | 1 | 8  | 1 | 13 | 1 | 18 | 1 | 23 | 1 | 28 | 1 |
| 4  | 2 | 9  | 2 | 14 | 3 | 19 | 4 | 24 | 5 | 29 | 5 |
| 5  | 0 | 10 | 0 | 15 | 0 | 20 | 1 | 25 | 0 | 30 | * |

Special milestone days:
- Day 30: **+5**
- Day 60: **+6**
- Day 90: **+7**
- Day 120: **+8**
- Day 150: **+9**
- Day 180: **+12**

> FAQ note: the cycle resets modulo 30 for normal days; the milestone bonuses
> accumulate beyond day 30.

---

## 4. Bar food (entrees) and trigger effects

**FAQ §3-10-1 (lines 1964-1987). FAQ-sourced.**

Three entrees per town; ordering certain meals and then visiting a specific
square may trigger an event or unlock a path (overworld/dungeon).

### Middlegate — Slaughtered Lamb

| Menu | Name | Cost | Effect / trigger |
|------|------|------|-----------------|
| A | Horrors d'oeuvres | 10 gp | Eat → visit C1 (2,10): don't flee (past undead) |
| B | Soup de Ghoul w/garlic toast | 50 gp | Eat → visit C1 (2,6): past undead |
| C | Dragon Steak Tartar | 100 gp | Eat → visit D1 (2,7): dragons will fight you |

### Atlantium — Boar's Tongue Tavern

| Menu | Name | Cost | Effect / trigger |
|------|------|------|-----------------|
| A | Lightly salted tongue of toad | 1000 gp | (unknown trigger) |
| B | Puree of Gnome | 2000 gp | Eat → A2 (14,10): gnome attacks |
| C | Devils Food Brownie | 3000 gp | Enables C1 (1,8): 1/8 fight; also prerequisite for S5-2 spell |

### Tundara — Lucky Dog Saloon

| Menu | Name | Cost | Effect / trigger |
|------|------|------|-----------------|
| A | Sizzling Swine Soup | 200 gp | (no trigger noted) |
| B | Red Hot Wolf Nipple Chips | 100 gp | Eat → C3 (1,9): unlocks C2-3 Nature's Gate |
| C | Roast Leg of Wyvern | 1000 gp | (experience) |

### Vulcania — Belinthra's Bar

| Menu | Name | Cost | Effect / trigger |
|------|------|------|-----------------|
| A | Pickled Pixie Brains | 5000 gp | (unknown) |
| B | Deep Fried Troll Liver | 500 gp | Eat → hireling encounter D/E (Thund R, Aeriel) in Vulcania |
| C | Cream of Kobold Soup | 1000 gp | Eat → E1 (3,1): "You killed our pal!" kobold fight |

### Sandsobar — Red Lantern Tavern

| Menu | Name | Cost | Effect / trigger |
|------|------|------|-----------------|
| A | Gourmet Dinner / Wyrm Chop Suey | 20 gp | Eat → (3,11) in Sandsobar: H K Phooey hireling |
| B | Roast Peasant under glass | 50 gp | Eat → E4 (3,10): fight peasants |
| C | Phantom Pudding (Very Low-cal) | 250 gp | Eat → Sarakin's Mine (7,3): cave-in event |

---

## 5. Bar drinks sub-game

**FAQ §3-10-3 (lines 2061-2070). FAQ-sourced; in-game drink mechanic not
traced in ASM.**

Drinks are ordered at the bar. Each has a "sickness" threshold — drinking too
many at once resets all stats.

| Drink | Bonus (per successful drink after threshold) | Threshold |
|-------|----------------------------------------------|-----------|
| Orc Beer | +5 Might | After 1st |
| Straight Shot | +20 Accuracy | After 2nd |
| ID Elixir | +10 Personality | After 2nd |
| Academic Ale | +10 Intelligence | After 2nd |
| Rare Vintage | +3 levels | After 2nd |
| Mystic Brew | +1 spell level | After 4th |

> "If you get sick drinking a certain beverage, you have all stats reset."
> (FAQ line 2063.) Rare Vintage and Mystic Brew are extremely powerful;
> Rare Vintage +3 levels compounds if repeated.

---

## 6. Inter-town portal connections

**FAQ §4-2 (lines 2178-2182). FAQ-sourced; portal selector bytes `0x64`,
`0x7E`–`0x83` documented in doc 28 §1.2.**

| From (coord) | Cost | Direction | To (coord) | Cost |
|---|---|---|---|---|
| Middlegate (0,5) | 10 gp | ↔ | Sandsobar (6,1) | 20 gp |
| Sandsobar (4,14) | 50 gp | ↔ | Tundara (6,11) | 10 gp |
| Tundara (6,9) | 50 gp | ↔ | Vulcania (6,3) | 30 gp |
| Vulcania (8,3) | 100 gp | ↔ | Atlantium (3,0) | 50 gp |
| Atlantium (12,0) | 25 gp | → | Middlegate (0,5) | — |

The **Atlantium → Middlegate** connection is **one-way only** (FAQ line 2184).

Navigation hints from FAQ (lines 2187-2193):
- From Middlegate portal, exit at Sandsobar: go 1S 1W, N several, 1W 1N.
- From Tundara portal: go 1S through secret door, 1E 1N 1E 2S 1W 1S 1W 1N.
- From Vulcania portal: go 1S 2E 1N.
- From Atlantium: go east along path.

---

## 7. Fly spell (S3-2) landing coordinates

**FAQ §3-2-3 (lines 922-927). FAQ-sourced.**

`S3-2 Fly` sends the party to a landing square in the chosen sector (A1–E4).
Sectors are 5×4 (A–E = east/west columns, 1–4 = north/south rows).

|      | 1        | 2        | 3        | 4        |
|------|----------|----------|----------|----------|
| **A** | (13,3)  | (8,7)    | (14,14)  | (14,10)  |
| **B** | (14,3)  | (13,7)   | (15,3)   | (13,15)  |
| **C** | (3,3)   | (8,4)    | (14,3)   | (3,14)   |
| **D** | (5,3)   | (4,4)    | (13,3)   | (13,10)  |
| **E** | (1,3)   | (3,7)\*  | (0,0)    | (3,10)   |

\* E2 = oasis area.

Town approximate fly-in sectors (FAQ lines 2197-2202, approximate):
- Middlegate: C2 sector
- Atlantium: A4 sector
- Tundara: A1 sector
- Vulcania: E1 sector
- Sandsobar: E4 sector
