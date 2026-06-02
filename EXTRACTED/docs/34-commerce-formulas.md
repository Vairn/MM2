# MM2 Commerce Formulas

Sources: **[FAQ]** = `Might and Magic FAQ.txt` §§3-6/3-10/4-1/3-2-2 (Andrew Schultz v2.2.0);
**[ASM]** = `mm2.capstone.asm`; **[DATA]** = data hunk tables.

Cross-references: [`28-town-services.md`](28-town-services.md) (ASM handlers/tables),
[`32-character-mechanics.md`](32-character-mechanics.md) (level/stat context).

---

## 1. Training cost formula [FAQ §3-6]

```
Training cost (gp) = current_level × training_town_index × 50
```

**Training town index** (not the same as the map/location index):

| Training index | Town | Map index (`A4-$79F2`) |
|----------------|------|------------------------|
| 1 | Middlegate | 0 |
| 2 | Sandsobar | 4 |
| 2 | Tundara | 2 |
| 4 | Vulcania | 3 |
| 5 | Atlantium | 1 |

Map index → training index array: **`[1, 5, 2, 4, 2]`** (map 0..4). [ASM confirmed: `28-town-services.md` §9.1]

**Healing cost (FAQ §3-6):**

```
Healing cost (gp) = current_level × training_town_index × 10
```

Multipliers for condition:
- ×10 if **dead** (instead of base ×10, effectively a ×100 of living cost — see note)
- ×100 if **eradicated**

> The FAQ reads: "Healing current level * town index * 10 * (10 if dead, 100 if eradicated)".
> Interpretation: base is `level × index × 10`; multiply by 10 for dead, 100 for eradicated.
> So dead healing = `level × index × 100` and eradicated = `level × index × 1000`. [FAQ §3-6]
> **Status: FAQ-sourced.** ASM healing formula not yet fully confirmed in current trace.

---

## 2. Day-based shop bonus cycle [FAQ §3-6]

Item bonus `+` value in shops follows a ~30-day cycle (same pattern repeats with escalation):

| Day mod 30 | Bonus + |
|------------|---------|
| 1 | 1 |
| 2 | 0 |
| 3 | 1 |
| 4 | 2 |
| 5 | 0 |
| 6 | 1 |
| 7 | 0 |
| 8 | 1 |
| 9 | 2 |
| 10 | 0 |
| 11 | 1 |
| 12 | 0 |
| 13 | 1 |
| 14 | 3 |
| 15 | 0 |
| 16 | 1 |
| 17 | 0 |
| 18 | 1 |
| 19 | 4 |
| 20 | 1 |
| 21 | 1 |
| 22 | 0 |
| 23 | 1 |
| 24 | 5 |
| 25 | 0 |
| 26 | 1 |
| 27 | 0 |
| 28 | 1 |
| 29 | 5 |
| 30 | special* |

Special multiples-of-30 bonus:

| Day | Bonus + |
|-----|---------|
| 30 | +5 |
| 60 | +6 |
| 90 | +7 |
| 120 | +8 |
| 150 | +9 |
| 180 | +12 |

> [FAQ §3-6] These are the best shop days to buy items if you want high magic bonuses.
> **Status: FAQ-sourced.** ASM calendar date index at `A4-$79B6`; Today's Specials date
> index from `A4-$681C`/`$6816` (see `28-town-services.md` §4.1). Whether the above bonus
> cycle is from the specials table or the general bonus system is **not yet ASM-confirmed**.

---

## 3. Portal table [FAQ §4-1]

Town-to-town portals (in-town teleport tiles). All are **bidirectional** except the last Atlantium → Middlegate leg.

| From | Tile (x,y) | Cost (gp) | To | Tile (x,y) | Direction |
|------|-----------|-----------|----|-----------  |-----------|
| Middlegate | (0,5) | 10 | Sandsobar | (6,1) | 20 |
| Sandsobar | (4,14) | 50 | Tundara | (6,11) | 10 |
| Tundara | (6,9) | 50 | Vulcania | (6,3) | 30 |
| Vulcania | (8,3) | 100 | Atlantium | (3,0) | 50 |
| Atlantium | (12,0) | 25 | Middlegate | (0,5) | one-way |

The last leg (Atlantium → Middlegate) is **one-way only**. [FAQ §4-1]

Portal selectors in `event.dat` (`OP_0E` dispatch): `0x64`, `0x7E`–`0x83`, etc. (see `28-town-services.md` §1.2).

---

## 4. Fly spell landing coordinates [FAQ §3-2-2-2]

S3/2 Fly lands the party at a fixed coordinate in any of the 20 outdoor sectors.
Coordinates are in the overland map's local tile system **(x,y)**:

|   | A | B | C | D | E |
|---|---|---|---|---|---|
| 1 | (13,3) | (14,3) | (3,3) | (5,3) | (1,3) |
| 2 | (8,7) | (13,7) | (8,4) | (4,4) | (3,7)* |
| 3 | (14,14) | (15,3) | (14,3) | (13,3) | (0,0) |
| 4 | (14,10) | (13,15) | (3,14) | (13,10) | (3,10) |

> \* E2 (3,7) = oasis area. [FAQ §3-2]
>
> Chain two fly spells together for a bigger jump (e.g. fly to A4 pool, then fly to target sector). [FAQ §3-2]

Town locations reachable via fly (approximate sector):

| Town | Sector | Notes |
|------|--------|-------|
| Middlegate | C2 | 1 west + 1 south from fly landing |
| Atlantium | A4 | ~½ west from fly landing |
| Tundara | A1 | ~½ west from fly landing |
| Vulcania | E1 | 1 east + 1 north |
| Sandsobar | E4 | ~2 east |

[FAQ §4-1 — approximate; exact approach paths vary]

---

## 5. Bar drinks sub-game [FAQ §3-10-3]

Available at all town pubs (handler `0x1A132`, menu B). Six drinks; getting sick resets all stats.

| Drink | Condition for bonus | Effect per success |
|-------|--------------------|--------------------|
| Orc Beer | 1st drink successful | +5 Might per success after the first |
| Straight Shot | 2 drinks successful | +20 Accuracy per success after the second |
| ID Elixir | 2 drinks successful | +10 Personality per success after the second |
| Academic Ale | 2 drinks successful | +10 Intelligence per success after the second |
| Rare Vintage | 2 drinks successful | +3 Levels per success after the second |
| Mystic Brew | 4 drinks successful | +1 Spell Level per success after the fourth |

> If you **get sick** drinking any beverage, **all stats are reset**. [FAQ §3-10-3]
>
> Same drink names appear in all five towns. [ASM: `28-town-services.md` §13.3]

**Status: FAQ-sourced.** Drink stat effects not yet ASM-confirmed (drink handler at `0x18F78`).

---

## 6. Bar food entrees + event trigger coordinates [FAQ §3-10-1]

After eating an entree (roster `$78` flag), various event tiles become active in the overland or dungeon.

| Town | Menu | Name | Cost (gp) | Event trigger / effect |
|------|------|------|-----------|------------------------|
| Middlegate | A | Horrors d'oeuvres | 10 | C1 (2,10) — undead don't flee |
| Middlegate | B | Soup de Ghoul w/Garlic Toast | 50 | C1 (2,6) — get past undead |
| Middlegate | C | Dragon Steak Tartar | 100 | D1 encounter? (unconfirmed) |
| Atlantium | A | Lightly Salted Tongue of Toad | 1,000 | C4 (14,8)? — for the undead wood (unconfirmed) [FAQ hint] |
| Atlantium | B | Puree of Gnome | 2,000 | A2 (14,10) — gnome attack (a dud) [FAQ hint] |
| Atlantium | C | Devils Food Brownie | 3,000 | C1 (1,8) — grants S5/2 Fingers of Death |
| Tundara | A | Sizzling Swine Soup | 200 | (effect unconfirmed) |
| Tundara | B | Red Hot Wolf Nipple Chips | 100 | C3 (1,9) — grants C2/3 Nature's Gate |
| Tundara | C | Roast Leg of Wyvern | 1,000 | (effect unconfirmed) |
| Vulcania | A | Pickled Pixie Brains | 5,000 | (effect unconfirmed) |
| Vulcania | B | Deep Fried Troll Liver | 500 | Vulcania NPC event — unlocks Thund R + Aeriel |
| Vulcania | C | Cream of Kobold Soup | 1,000 | E1 (3,1) — "You killed our pal!" kobold encounter |
| Sandsobar | A | Gourmet Dinner B Wyrm Chop Suey | 20 | Sandsobar inn (3,11) — unlocks H K Phooey |
| Sandsobar | B | Roast Peasant under Glass | 50 | E4 (3,10) — fight peasants |
| Sandsobar | C | Phantom Pudding (Very Low-cal) | 250 | Sarakin's Mine (7,3) — cave-in event |

> Costs confirmed by ASM data hunk `A4-$6760` (BE u16 per town × menu slot):
> see [`28-town-services.md`](28-town-services.md) §13.3. Event coordinates from **[FAQ §3-10-1]**.
> Bar tip hints in the FAQ (§3-10-2) cross-reference these coords — e.g. "Meal C, then C1 1,8".

Bar rumor hints also appear in pub menu E (listen for rumors); same hints as seen in
[`EXTRACTED/embedded_strings.txt`](../embedded_strings.txt) and `str.dat`.
