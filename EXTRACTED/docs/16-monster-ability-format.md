# monsters.dat — ability/attack decode (ASM-confirmed)

Record geometry: 256 records × 26 bytes (`0x1A`). Loader/accessor at asm `0x99C4`
(`mulu.w #$1a`, "monsters.dat"). The authoritative per-record **stat unpacker**
is at `0x4C8E`: it copies one record (via `0x99C4`) into a local buffer and
explodes the bytes into a block of A4-relative "current monster" combat globals.

## Byte map (offsets within the 26-byte record)

| Off  | Name    | Meaning (ASM-confirmed) |
|------|---------|--------------------------|
| 0x00 | name    | 14 bytes, each char masked `& 0x7F` (stored as char+128) |
| 0x0E | hp code | `HP = ((c & 0x3F)+1) * hpmul[(c>>6)&3]` (table at `A4-$746C`) |
| 0x0F | xp code | `XP = ((c & 0x1F)+1) * xpmul[(c&0x60)>>5]`, ×1000 if bit7 |
| 0x10 | treasure| reward pack (gold/gems/item/XP-bonus). **Not a combat ability.** Decoded by the reward routine at `0x10B74` |
| 0x11 | Pabil   | **group attack**: low5 = verb index, bits5-7 = use-chance tier |
| 0x12 | Sabil   | **single attack**: low5 = status effect, bit5 misc, bit6 archer, bit7 undead |
| 0x13 | Oabil   | low nibble+1 (×10 if bit4) = "adds friends" count; bits5-6 = flee tier; bit7 = multiplies |
| 0x14 | speed   | low nibble+1, high nibble+1 |
| 0x15 | picture | `& 0x7F` -> `NN.anm`; bit7 = placement/size flag |
| 0x16 | AC      | low5+1 (×10 if bit5), bit6/bit7 flags |
| 0x17 | damage  | low5+1 (×10 if bit5, capped 250) |
| 0x18 | speed2  | low5+1 (×10 if bit5, capped 250) |
| 0x19 | mres    | bits0-2 flags, bits3-4, bits5-7 -> table `A4-$7464` |

## Message tables (data hunk)

A single master pointer table lives in the data hunk (`mm2_data_00.bin`, data
load addr `0x25934`) at file offset `0x1108`, 72 entries of big-endian code-hunk
pointers. `A4 = data_base + 0x7FFE = 0x2D932` (standard small-data model),
verified by the offset arithmetic below.

Master table layout (index : string):

```
 0..9   melee/turn verbs: attacks, fights, charges, battles, thrusts at,
        slashes at, strikes at, engages, " waits for opening!", " adds friends!"
10..39  victim status messages (Sabil effect table): lost gold, lost gems,
        is poisoned, is diseased, falls asleep, is cursed, is silenced,
        is paralyzed, collapses, dies, turns to stone, is eradicated!!,
        lost item, lost backpack, lost food, lost all food, lost all gold,
        lost all gems, lost valuables, is aged, is aged, lost statistics x3,
        lost level x2, lost experience, items scrambled, lost spell points,
        is assassinated
40..71  group attack verbs (Pabil verb table): sprays poison, sprays acid,
        casts a curse, breathes fire, breathes lightning, breathes cold,
        breathes energy, breathes gas, breathes acid, explodes, gazes,
        drains magic, drains spell level, vaporizes valuables, juggles party,
        energy blast, sleep, lightning bolts, fireballs, fingers of death,
        disintegrate, super shock, dancing sword, incinerate, invokes power,
        implosion, inferno, pain, silence, frenzies, paralyze, swarms
```

### Pabil — group attack (byte 0x11)
- Dispatcher `0x10002` reads `Pabil & 0x1F` and indexes the verb table at
  `A4-$6E56` **directly (0-based)**. `A4-$6E56` = master `[40]` = "sprays poison".
- `-$6E56[29]` = master `[69]` = **"frenzies"**.
- Example: **Cuisinart** (monster #212) has `Pabil = 0x3D` → `0x3D & 0x1F = 0x1D = 29`
  → prints **"frenzies"**.
- bits 5-7 (`(Pabil & 0xE0) >> 4`) index a use-frequency table at `A4-$7464`
  (how often the group attack fires). This is **not** a flee flag.

### Sabil — single-target attack (byte 0x12)
- Routine `0xFEEA` reads `Sabil & 0x1F`, indexes `A4-$6ECE` by `(effect - 1)`.
  `A4-$6ECE` = `A4-$6E56 - 0x78` = master `[10]` = "lost gold". So the single
  effect list is the victim-status table (effect 1 = lost gold, 3 = poisoned, …).
- bit6 = archer (`0x107A4`), bit7 = undead, bit5 = misc flag.

### Oabil — other behavior (byte 0x13)
- low nibble `+1` (×10 if bit4) = number of reinforcements for the "adds friends!"
  action (`0x11F0A`).
- bits 5-6 = flee/run chance tier; gates the "<monster> runs" path (`0x10DFC`).
- bit7 = multiplies/breeds: doubles the on-screen count (`0x100B0`).

## Offset verification

```
data_base                = 0x25934
master table file offset = 0x1108  -> entry[0] = "attacks" (0xf9ae)
party verb base A4-$6E56 -> entry[40] "sprays poison"
   file offset = 0x1108 + 40*4 = 0x11A8
   => A4 - 0x6E56 = data_base + 0x11A8 = 0x26ADC
   => A4 = 0x26ADC + 0x6E56 = 0x2D932 = data_base + 0x7FFE   (consistent)
party verb [29] = master[69] = "frenzies"                    (Cuisinart)
```

---

## Appendix A — FAQ monster reference table (validation source)

**FAQ §3-8 (lines 1582-1839). FAQ-sourced player-visible stats.**

This table is the primary validation source for decoded HP/XP/AC/undead values.
It lists all named monsters in encounter order (roughly index order in
`monsters.dat`). Columns: `HitPts` | `Exp.Pts.` | `U` (undead Y/N = Sabil bit7)
| `AC`.

The undead `Y` column directly corresponds to `Sabil` byte 0x12 bit 7 —
**ASM-confirmed** via `0xFEEA` / `0x4C8E`. AC values are player-visible AC;
the relationship to the encoded `AC byte` 0x16 (`low5+1`, ×10 if bit5) is
**not yet fully validated** across all 256 entries.

Selected entries for quick cross-check (indexes approximate — exact index
mapping to `monsters.dat` records requires a full name-match scan):

| Name           | HitPts | Exp.Pts. | U | AC  |
|----------------|--------|----------|---|-----|
| Creepy Crawler | 5      | 150      | N | 4   |
| Giant Beetle   | 10     | 200      | N | 7   |
| Skeleton       | 6      | 200      | Y | 50  |
| Flesh Eater    | 6      | 200      | Y | 40  |
| Zombie         | 20     | 400      | Y | 7   |
| Carnage Spirit | 25     | 1000     | Y | 80  |
| Cuisinart      | 1000   | 20000000 | N | 60  |
| Lich Lord      | 2000   | 6000000  | Y | 40  |
| Mega Dragon    | 64000  | 32000000 | N | 250 |
| Ghost          | 200    | 40000    | Y | 170 |
| Vampire        | 250    | 25000    | Y | 240 |
| Devil King     | 5000   | 30000000 | N | 60  |
| Time Lord      | 3000   | 15000000 | N | 11  |
| Orc God        | 50000  | 15000000 | N | 40  |

> **Full table** in FAQ lines 1584-1839 (256 entries). Prefer extracting and
> cross-validating the full decoded dataset via `tools/` against `monsters.dat`
> rather than duplicating all 256 rows here. The FAQ table is the authoritative
> **player-visible** reference; the codec (byte 0x0E hp-code, 0x0F xp-code)
> produces these values.
