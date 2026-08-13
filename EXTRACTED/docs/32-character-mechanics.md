# MM2 character mechanics (player reference)

**Source:** Schultz FAQ v2.2 §2-5 (lines 446–585), §3-9 (lines 1866–1958) —
[`Might and Magic FAQ.txt`](Might%20and%20Magic%20FAQ.txt).

Values below are **FAQ-sourced** unless marked **ASM-confirmed**. Use ASM traces
before treating formulas as on-disk or hard-coded addresses.

**Related:** [`06-roster-format.md`](06-roster-format.md) (`$16` thievery,
`$17`–`$1F` skills/flags, `$20` **working level** — not a skill/flag),
[`17-combat-system.md`](17-combat-system.md).

---

## Race stat modifiers (creation)

Net change is zero for all races except Half-Orc (FAQ lines 557–562).

| Race | Modifiers |
|------|-----------|
| Human | (none) |
| Elf | Str−, Int+, End−, Acc+ |
| Dwarf | Int−, End+, Spe−, Luc+ |
| Gnome | Spe−, Acc−, Luc++ |
| Half-Orc | Str+, Int−, Per−, End+, Luc− |

FAQ notes negative Speed/Endurance bonuses are recorded in-game.

---

## Attribute → bonuses per level

Spell points per level use **INT** (Sorcerer, Archer) or **PER** (Cleric, Paladin)
— see ASM branch @ `$7B36` in [`06-roster-format.md`](06-roster-format.md).

FAQ table (lines 446–467); values from 60+ verified via save-state comparison
(FAQ also notes MM3 uses the same curve):

| Attribute range | SP/level (INT or PER) | AC bonus (SPD) | HP bonus (END) |
|-----------------|----------------------|----------------|----------------|
| 11–12 | 3 | 0 | 0 |
| 13–14 | 4 | 1 | 1 |
| 15–16 | 5 | 2 | 2 |
| 17–18 | 6 | 3 | 3 |
| 19–21 | 7 | 4 | 4 |
| 22–25 | 8 | 5 | 5 |
| 26–29 | 9 | 6 | 6 |
| 30–44 | 10 | 7 | 7 |
| 45–59 | 11 | 8 | 8 |
| 60–74 | 12 | 9 | 9 |
| 75–89 | 13 | 10 | 10 |
| 90–104 | 14 | 11 | 11 |
| 105–119 | 15 | 12 | 12 |
| 120–134 | 16 | 13 | 13 |
| 135–149 | 17 | 14 | 14 |
| 150–174 | 18 | 15 | 15 |
| 175–199 | 19 | 16 | 16 |
| 200–224 | 20 | 17 | 17 |
| 225–249 | 21 | 18 | 18 |

**ASM-confirmed SP/level boundaries (2026-07-17):** the FAQ table above rounds
attribute ranges to even pairs; the real Rest-time lookup (`luck_bonus_table_walk`
@ **`0x4442`**, walking thresholds at **`A4-$7486`**, called from the Rest SP
recompute at **`0x19C30`** — see [`06-roster-format.md`](06-roster-format.md) and
[`57-user-help-trace-residuals.md`](57-user-help-trace-residuals.md) §4) uses
`attr <= threshold` (unsigned `bls`) against the table
`{4,6,9,13,15,17,19,22,26,30,45,60,75,90,105,120,135,150,175,200,225,250,255}`,
giving tier index `i` = SP/level directly. In ASM-confirmed terms the tier
boundaries are **`≤4→0, ≤6→1, ≤9→2, ≤13→3, ≤15→4, ≤17→5, ≤19→6, ≤22→7, ≤26→8,
≤30→9, ≤45→10, ≤60→11, ≤75→12, ≤90→13, ≤105→14, ≤120→15, ≤135→16, ≤150→17,
≤175→18, ≤200→19, ≤225→20, ≤250→21, ≤255→22`** — e.g. attribute **13** is
tier 3 (not 4 as the FAQ's "13–14→4" row implies); the FAQ/ASM boundaries are
off by one attribute point at every tier edge. This ASM table only drives the
**Rest SP recompute**; whether the same table also gates SP-at-level-up is
unconfirmed — treat the FAQ table as the level-up reference until traced.
Remake: `gameplay::restSpellBonusFactor()` in `game/src/gameplay/Movement.cpp`.

**Fighter-mage SP quirk (FAQ lines 477–481):** After promotion, Paladin/Archer
max SP ≈ `(points per level) × (level − 6)` — described as matching MM1 behaviour.
**Not yet ASM-confirmed** — the Rest recompute above does not special-case
Paladin/Archer beyond the INT/PER attribute choice at `$7B36`.

Example (FAQ line 500–501): Sorcerer with End 20 → 6 (base at End 11–12) + 4
(END bonus) = **10 HP per level** when training at Atlantium.

---

## HP per level by class (Endurance 11–12, no bonus)

FAQ lines 489–498:

| Class | HP/level |
|-------|----------|
| Barbarian | 15 |
| Knight | 12 |
| Paladin | 10 |
| Archer | 10 |
| Ninja | 10 |
| Robber | 8 |
| Cleric | 8 |
| Sorcerer | 6 |

---

## Extra attacks

FAQ lines 510–515:

```
extra_attacks = floor(character_level / divisor)
```

| Divisor | Classes |
|---------|---------|
| 4 | Knight, Paladin, Barbarian |
| 5 | Archer, Robber, Ninja |
| 7 | Cleric |
| 10 | Sorcerer |

*ASM: divisor table address not yet pinned.*

---

## Spell level gain

FAQ lines 580–585:

| Class type | Gains spell level X at character level |
|------------|----------------------------------------|
| Pure casters (Cleric, Sorcerer) | **2X − 1** |
| Fighter-mages (Paladin, Archer) | **2X + 5** |

Fighter-mages cap at spell level **7** naturally; a fountain can raise them to **9**.

---

## Thievery

FAQ lines 505–508; roster byte **`$16`** ([`06-roster-format.md`](06-roster-format.md)):

| Class | Starting % | Gain |
|-------|------------|------|
| Robber | 30% | +1% per level |
| Ninja | 10% | +1% per level |

100% does not guarantee safe chest opens; even **255%** does not (FAQ).

Trainers: [`33-skills-and-hirelings.md`](33-skills-and-hirelings.md) (Rinaldo Jr., Maxwell).

---

## Experience to next level

FAQ §3-9 (lines 1868–1958). Thresholds in **thousands of XP** to reach that level.

**Group A** (~25% less XP than B): Barbarian, Knight, Robber, Cleric.  
**Group B**: Paladin, Archer, Ninja, Sorcerer.

Party experience long: **`A4-$6FC6`** (runtime). Comparison table data-hunk address **not yet pinned**.

| Lvl | Group A | Group B | Doubled? |
|-----|---------|---------|----------|
| 2 | 1.5 | 2.0 | N |
| 3 | 3 | 4 | Y |
| 4 | 6 | 8 | Y |
| 5 | 12 | 16 | Y |
| 6 | 24 | 32 | Y |
| 7 | 48 | 64 | Y |
| 8 | 96 | 128 | Y |
| 9 | 192 | 256 | Y |
| 10 | 384 | 512 | Y |
| 11 | 576 | 768 | N |
| 12 | 768 | 1024 | N |
| 13 | 960 | 1280 | N |
| 14 | 1344 | 1728 | Y |
| 15 | 1728 | 2304 | N |
| 16 | 2496 | 3328 | Y |
| 17 | 3264 | 4352 | N |
| 18 | 4032 | 5376 | N |
| 19 | 4800 | 6400 | N |
| 20 | 5568 | 7424 | N |
| 21 | 7104 | 9472 | N |
| 22 | 8640 | 11520 | N |
| 23 | 10276 | 13568 | N |
| 24 | 11712 | 15616 | N |
| 25 | 13248 | 17664 | N |
| 26 | 14784 | 19712 | N |
| 27 | 16320 | 21760 | N |
| 28 | 17856 | 23808 | N |
| 29 | 19392 | 25832 | N |
| 30 | 20928 | 27856 | N |
| 31 | 24000 | 32000 | Y |
| 32 | 27072 | 36096 | N |
| 33 | 30144 | 40192 | N |
| 34 | 33216 | 44288 | N |
| 35 | 36288 | 48384 | N |
| 36 | 39360 | 52480 | N |
| 37 | 42432 | 56576 | N |
| 38 | 45504 | 60672 | N |
| 39 | 48576 | 64768 | N |
| 40 | 51648 | 68864 | N |
| 41 | 54720 | 72960 | N |
| 42 | 57792 | 77056 | N |
| 43 | 60864 | 81152 | N |
| 44 | 63936 | 85248 | N |
| 45 | 67008 | 89344 | N |
| 46 | 70080 | 93440 | N |
| 47 | 73152 | 97536 | N |
| 48 | 76224 | 101632 | N |
| 49 | 79296 | 105728 | N |
| 50 | 82368 | 109824 | N |
| 51 | 88512 | 118016 | Y |
| 52 | 94656 | 126208 | N |
| 53 | 100800 | 134400 | N |
| 54 | 106944 | 142592 | N |
| 55 | 113088 | 150784 | N |
| 56 | 119232 | 158976 | N |
| 57 | 125376 | 167168 | N |
| 58 | 131520 | 175360 | N |
| 59 | 137664 | 183552 | N |
| 60 | 143808 | 191744 | N |
| 61 | 149952 | 199936 | N |
| 62 | 156096 | 208128 | N |
| 63 | 162240 | 216320 | N |
| 64 | 168384 | 224512 | N |
| 65 | 174428 | 232704 | N |
| 66 | 180672 | 240896 | N |
| 67 | 186816 | 249088 | N |
| 68 | 192960 | 257280 | N |
| 69 | 199104 | 265472 | N |
| 70 | 205248 | 273664 | N |
| 71 | 211392 | 281856 | N |
| 72 | 217536 | 290048 | N |
| 73 | 223680 | 298240 | N |
| 74 | 229824 | 306432 | N |
| 75 | 235968 | 314624 | N |
| 76 | 252352 | 331008 | Y* |
| 77 | 268736 | 347392 | Y |

\*FAQ note at level 76+: Cleric and Sorcerer share a special increment (16384 from
that point per FAQ line 1957).

---

## Still unverified (ASM)

- Attribute bonus lookup routine (table above)
- Extra-attacks divisor table in data hunk
- Spell-level gain on level-up handler
- Per-class XP threshold array address
- Thievery +1%/level accrual at level-up
