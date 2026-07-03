# Combat system (ASM trace)

Trace of the encounter/battle engine in `mm2.capstone.asm`. All addresses are
code-hunk load addresses (base 0). `A4 = data_base + 0x7FFE`, so an `A4`
displacement `-d` maps to data-hunk offset `0x7FFE - d` (see doc 16). Confidence
is high for control flow and data layout; the exact HP/to-hit/damage arithmetic
is only partially reduced (noted inline).

## Top-level flow

```
encounter start
  -> 0x11C82  build battle arrays (per monster slot: 0x11C2C)
  -> 0x12A22  COMBAT ROUND LOOP  (repeats each round)
       -> per round: 0x135BE / 0x11D0C / 0x11C82 (display + recompute)
       -> initiative select (highest speed not-yet-acted)
            -> 0x119C2  PLAYER turn   (character -$4F5)
            -> 0x1064C  MONSTER turn  (monster slot -$4F7)
       -> 0x13282  round/over check
       -> 0x12430  VICTORY + rewards   (when -$77BE monster count == 0)
       -> 0x11646  party defeat / retreat
```

## Battle data model

### Monster slots (parallel arrays, index 0..10, max 11 on screen)
The encounter holds up to ~11 active monsters. Per-instance state is kept in
parallel `A4` arrays keyed by slot; full stats are re-derived from the type id
by the unpacker at `0x4C8E` when needed (see doc 16).

| Array (A4)  | Element | Meaning |
|-------------|---------|---------|
| `-$11DE[i]` | byte    | monster **type id** (record index into monsters.dat) |
| `-$53A[i]`  | word    | current **HP** (set from unpacked `-$11A0`) |
| `-$519[i]`  | byte    | **status** flags (bit0 = awake/active; `&0xFE` = afflictions) |
| `-$50E[i]`  | byte    | **speed** / initiative (set from unpacked `-$11B1`, record byte 0x18) |
| `-$503[i]`  | byte    | secondary stat (from unpacked `-$11BA`, record byte 0x14 hi nibble) |
| `-$77BE`    | byte    | **live monster count** |
| `-$524`     | byte    | active party count (clamped to 10 for menus) |
| `-$5E4A[i]` | byte    | monster "has acted this round" flag |
| `-$5E40[i]` | byte    | party "has acted this round" flag |

`0x11C2C` instantiates one slot from the scratch unpack globals; `0x11C82`
loops it over all monsters at battle/round start.

### Party (character objects)
Characters are heap objects fetched by index via `JSR -$7F20(A4)` (`-$4F5` =
current character, `-$7959` = party size, `-$5E4D` = front-rank cutoff). Field
offsets observed in combat:

| Off  | Field |
|------|-------|
| `$0F`| class (`2` gets a ranged option) |
| `$26`| status bits (bit1 = silenced) |
| `$4E`| has bow / ranged weapon |
| `$58`| spell points (word) |
| `$6E`| speed (initiative) |
| `$72`| spellcaster flag |

## Round loop (0x12A22)

1. Reset round flags; clear `-$5E4A[0..9]` and `-$5E40[0..7]` (acted flags).
2. For each monster, roll RNG and update `-$519[i]` (wake/initiative bit).
3. **Initiative**: scan monsters for the highest `-$50E[i]` among not-acted ->
   `-$5E38` (best monster speed) / `-$4F7` (slot); scan party for highest
   `char.$6E` among not-acted -> `-$5E37` / `-$4F5`.
4. If best character speed > best monster speed (and nonzero) -> `0x119C2`
   (player turn). Else if a monster can act -> `0x1064C` (monster turn). Else
   nobody left -> advance.
5. `0x13282` decides whether the round/combat continues.
6. End: `-$77BE==0` -> `0x12430` victory; else defeat/retreat via `0x11646`.

RNG helper: `JSR -$7BB4(A4)` with `(1, max)` returns a roll in `[1,max]`.

## Player turn (0x119C2)

- `0x11866` draws the command bar and sets capability flags for the active
  character:
  - `-$5E36` **melee/attack**: character is within front-rank cutoff `-$5E4D`.
  - `-$5E35` **shoot**: class `$0F==2` or front rank, **and** `$4E` (bow) set.
  - `-$5E34` **cast**: not silenced (`$26` bit1) **and** caster (`$72`) **and**
    spell points `$58 > 0`.
  - Each available command is printed via `0x1181E` (string pointer table at
    `A4-$6F9C`: "A-Attack F-Fight S-Shoot C-Cast U-Use B-Block R-Run E-Exch
    V-View", colors/labels at `-$6F4B`/`-$6F54`).
- `0x1175C` reads a key and accepts it only if valid (`B D P Q V U E R` always,
  `A`/`F` if `-$5E36`, `S` if `-$5E35`, `C` if `-$5E34`).
- Dispatch (jump table at `0x11BD0`, key normalised by `-0x41`):

| Key | Action | Handler |
|-----|--------|---------|
| A   | Attack (single target) | `0xCD90` |
| F   | Fight (auto/melee)      | `0xCFD0` |
| C   | Cast spell              | spell select -> dispatch table `0xD000..0xD256` |
| S   | Shoot (ranged)          | (ranged path) |
| B   | Block                   | `0x11B0A` (`-$7D5E`/`-$7C3E`) |
| R   | Run / retreat           | `0x11B1A` (`-$7CC2` party reorder) |
| U   | Use item                | `0x11B62` (`0x133EC`) |
| V   | View character          | `0x11B6E` (`-$7E00`) |

Target selection: `0x111DA` ("which (A - x)?"), `-$5E32` selects monster-side
(`-$77BE`) vs party-side (`-$524`) targeting; chosen index -> `-$51D`. The
Attack command has its own selector at `0xD43C`/`0xD390` (auto-targets when only
one monster remains).

### Spells (Cast)
`0xD000..0xD256` is a jump table of per-spell handlers (`JSR $bc3a`, `$bc7a`, …
in the `0xBC00..0xC800` block), indexed by spell number; entries `0xD186+` are
the `.w` offsets.

## Monster turn (0x1064C -> 0x106A0)

For monster slot `-$4F7` (status `-$519[i]`):
- Status byte `>= 0x80` (special/disabled) -> handled separately.
- **Flee**: `-$11B6` (Oabil bits 5-6) indexes probability table `A4-$6F1A`; on a
  successful roll the monster runs (`0x10DFC` prints "<name> runs").
- Otherwise it acts:
  - **Archer** (`-$11AE`, Sabil bit6): ranged attack (`0x10584`).
  - **Group/party attack** when the Pabil effect `-$11BC` is in range: `0x10002`
    prints "<name> <verb>" from the verb table at `A4-$6E56` (index 29 =
    "frenzies"); see doc 16.
  - **Melee** otherwise: `0x10118`, applying the single-target status effect
    `-$11B9` (Sabil low5) to a party member.
- **Multiplies/breeds** (`-$11A1`, Oabil bit7): `0x100B0` roughly doubles the
  on-screen count.
- **Adds friends** (`-$11B7`, Oabil low nibble): `0x11F0A` appends reinforcement
  monster ids to `-$11DE[]` ("<name> adds friends!").

## Status / afflictions
Battle status uses the `-$519[]` byte plus per-character bits. UI abbreviations
(table at `A4-$6FBC`): `Enca Mdls Held Aslp Afrd Weak Siln Hurt`
(encased / mindless / held / asleep / afraid / weak / silenced / hurt). Full
victim-effect names are the single-attack message table (doc 16, master[10..39]).

## Victory & rewards
On `-$77BE==0`, `0x12430` ends combat. Per defeated monster, the reward routine
`0x10B74` decodes record byte `0x10`:
- bit2 -> add RNG(1..10) to `-$3F12` (gems);
- bits3-4 -> gold tier into `-$3F10` (with `/16` or `/2` scaling + RNG);
- bits0-1 -> item-drop level (`-$5E28`/`-$5E29`);
- accumulated XP `-$119E` added to party total `-$6FC6`.
"Experience has multiplied three-fold." (string `0x14300`) handles the bonus
case.

## Key addresses

| Addr | Routine |
|------|---------|
| `0x12A22` | round loop |
| `0x119C2` | player turn |
| `0x11866` | build command bar + capability flags |
| `0x1175C` | read valid command key |
| `0x1181E` | print one command-bar entry |
| `0x111DA` | target selector ("which (A - x)?") |
| `0x1064C` / `0x106A0` | monster turn / AI dispatch |
| `0x10002` | monster group attack (verb table `A4-$6E56`) |
| `0xFEEA`  | monster single attack |
| `0x10118` | melee resolution |
| `0x10DFC` | flee ("runs") |
| `0x100B0` | multiply / breed |
| `0x11F0A` | adds friends (reinforcements) |
| `0x10B74` | per-monster reward decode |
| `0x12430` | victory / end combat |
| `0x11646` | defeat / retreat |
| `0x11C2C` / `0x11C82` | instantiate monster slot / all slots |
| `0x4C8E`  | monster stat unpacker (doc 16) |
| `0xD000`  | spell-effect jump table |

## Character combat stats (FAQ)

From FAQ §2-5 / §3-8 — detail in [`32-character-mechanics.md`](32-character-mechanics.md):

- **Extra attacks:** `floor(level / divisor)` — divisor 4 (Knight/Paladin/Barbarian),
  5 (Archer/Robber/Ninja), 7 (Cleric), 10 (Sorcerer). *ASM divisor table TBD.*
- **HP per level** at End 11–12: Barbarian 15 … Sorcerer 6 (see doc 32 table).

---

## Extra attacks formula [FAQ §2-5]

Extra attacks per round = `floor(level / x)` where `x` is class-dependent:

| Divisor `x` | Classes |
|-------------|---------|
| 4 | Knight, Paladin, Barbarian |
| 5 | Archer, Robber, Ninja |
| 7 | Cleric |
| 10 | Sorcerer |

**Status: FAQ-sourced.** ASM computation site not yet located in the current trace.
See also [`32-character-mechanics.md §4`](32-character-mechanics.md) for HP/level baseline.

## HP per level baseline [FAQ §2-5]

At Endurance 11–12 (no bonus): Knight 12, Paladin/Archer/Ninja 10, Cleric/Robber 8, Sorcerer 6, Barbarian 15 HP/level.
Endurance above 12 adds a bonus (see [`32-character-mechanics.md §3`](32-character-mechanics.md)).
Training at Atlantium vs Middlegate is ~6 HP/level difference. [FAQ §4-2]

## XP rules (FAQ §3-8)

- **Unconscious** party members: **receive XP** for the fight.
- **Stoned / dead** party members: **do not receive XP**.
- Characters who **flee** during combat: **do not receive their share** (but their share is **not forfeited** — it is distributed to the remaining party).
- Monsters that **frenzy or explode**: party **does receive** treasure + XP.
- Monsters that **flee**: party **does NOT receive** treasure/XP.
- **Friend summon cap**: only one friend-summon per fight; summon doubles the count of monsters not in the first 10; hard cap at 255 monsters total (>122 in back → summon impossible). [FAQ §3-8]

XP accumulation: `0x10B74` per-monster reward decode; accumulated in `-$119E`, folded
into `-$6FC6` at victory `0x12430`. [ASM confirmed] At `0x12430` the pool is
**divided evenly** among party members whose roster condition byte (`+$26`) is
**&lt; `0x80`** (dead/stoned excluded; unconscious with 0 HP still eligible).
Defeat vs continue uses `0x13282`: members with `+$26 <= 0x10` keep the party
"in fight."

## Open items (not fully reduced)
- Exact to-hit / AC interaction and physical damage formula inside
  `0xCD90`/`0xCFD0`/`0x10118`.
- ~~HP/XP code -> value tables (`A4-$746C`, `A4-$7464`) numeric contents~~ —
  **RESOLVED**, see doc 16 (`mm2_monster_decode_hp`/`_xp`). `A4-$7464` turned
  out to be the `mres` (byte 0x19) resistance-tier table, not xpmul.
- Full per-spell handler semantics (`0xBC00..0xC800`).

---

## FAQ cross-check notes (Schultz v2.2 §3-8)

The following notes are **FAQ-sourced** (lines 1841-1864) and have not been
independently traced in ASM — treat as player-visible behaviour pending code
confirmation.

### XP / treasure rules

- **Frenzy / explode**: the party **does receive** XP and treasure (FAQ line
  1841). Frenzy and explode are Pabil group-attack verbs (doc 16, verb table
  `A4-$6E56`).
- **Flee**: monsters that flee yield **no treasure and no XP share** (FAQ line
  1842-1843). `0x10DFC` prints "<name> runs".
- **Fleeing party members**: a party member who flees combat **does not receive
  XP for that encounter**. However, their XP share is **not forfeited** to the
  remaining party — it is simply lost (FAQ line 1843-1844).
- **Unconscious characters**: receive XP (FAQ line 1845).
- **Stoned / dead characters**: do **not** receive XP (FAQ line 1845).

### Monster friend-summon cap

- When a monster uses "adds friends" (`Oabil` low nibble via `0x11F0A`): the
  count of monsters **not in the front 10 doubles** (FAQ line 1846-1848).
  Example: 6 woodsmen in back → 12 after summon.
- Maximum total monsters: **255**. If there are > 122 monsters already in the
  back ranks, no more can be summoned (FAQ line 1848-1849).
- There is **only one "adds friends" event per fight** (FAQ line 1850).

### Undead `Y` column

The monster list in FAQ §3-8 includes a `U` (undead) column (Y/N). This
corresponds to `Sabil` bit 7 in `monsters.dat` byte `0x12` (doc 16). Undead
flag `Y` means: the monster is classified undead and can be affected by Turn
Undead (`C1-7`) and Holy Word (`C9-2`). Cross-reference: `Sabil` bit7
**ASM-confirmed** via `0xFEEA` / `0x4C8E` (doc 16).
