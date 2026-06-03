# Encounter tables (random + fixed)

Trace of how MM2 chooses monsters for combat. Addresses are code-hunk load
addresses (base 0). `A4 = data_hunk_base + 0x7FFE`; embedded tables map to
**data hunk** file offsets via `off = 0x7FFE - a4_disp` in
`EXTRACTED/ghidra/mm2_data_00.bin` (8604 bytes). Do **not** use code-hunk
offset `0x4874` — that address is 68000 code, not the table.

Codec: `tools/decode_encounters.py`, `EXTRACTED/decomp/mm2_encounter.{h,c}`.

## Three combat entry paths

| Path | Trigger | Monster IDs | Key ASM |
|------|---------|-------------|---------|
| **Random step** | Walking; roll @ `0x10A2` | Built in `0x11F0A` loop `0x1213E` | `0x12C6E` combat enter |
| **Arena / ticket** | Items `0xD0`–`0xD3` after arena win | `A4-$718A[screen]` + RNG @ `0x9E96` | `0x9F1E` class-quest chain |
| **Fixed script** | Tile event | 10 bytes from OP_12/13 | `0x16300` → `A4-$11DE[]` |

`monsters.dat` supplies stats only (unpacker `0x4C8E`) after type ids land in
`A4-$11DE[0..10]`. FAQ coordinate “Encounters:” lists are **event.dat OP_12**
fights, not random pools.

## `attrib.dat` tuning (per screen, bytes `0x09`–`0x0D`)

Copied into the current-screen buffer `A4-$561A` on enter. Field reads use
negative offsets from that buffer (e.g. byte `0x09` → `A4-$5611`).

| File off | Attrib buffer | ASM | Role |
|----------|---------------|-----|------|
| `0x09` | `-$5611` | `0x10A2` | **Step rate**: `RNG(1, byte) == 1` triggers encounter check |
| `0x0A` | `-$5610` | `0x12124` | **Group-size gate** vs live monster count `A4-$77BE` |
| `0x0B` | `-$560F` | `0x11FA2` | **Max monsters** in random picker |
| `0x0C` | `-$560E` | `0x11FB2` | **Min monsters** floor |
| `0x0D` | `-$560D` | `0x116CA`, `0x13148` | **Retreat/run difficulty** (not spawn rate) |

Typical values: rate `0x64` (1-in-100 step roll), retreat `30..200` (multiples
of 10). Dump all 60 screens:

```bash
python tools/decode_encounters.py
python tools/decode_encounters.py --json EXTRACTED/encounter_tables.json
```

## Embedded exe tables (data hunk)

| A4 field | Data-hunk off | Size | ASM | Role |
|----------|---------------|------|-----|------|
| `-$718A` | **`0xE74`** | 60 B | `0x9E96` | Per-area byte added to arena monster base (`ticket*3 + table[screen]`) |
| `-$6FC0` | `0x103E` | 4 B | `0x11F14` | Disposition modifier indexed by `A4-$79AE` |
| `-$6FCA` | `0x1034` | 4 B | `0x12110` | Running party XP budget vs proposed group |

**Partial:** the master monster **pool blob** (16-byte rows indexed by
`base*16 + RNG` in `0x11F0A`) is not fully mapped. The picker at `0x11F0A` uses
`A4-$6FC0`, attrib min/max, and RNG tiers — not `A4-$718A` (arena-only).

## Random encounter flow

```
step on map (3D view, not town mode)
  -> 0x10A2: gates (view -$79E2, env -$7660, tile type, rate byte 0x09)
  -> on success: combat setup via -$7EDE
  -> 0x12C6E: if -$796B lacks 0x80 (not pre-filled fight)
       -> 0x1213E: loop 0x12072 (XP budget) + 0x11F0A (fill -$11DE)
  -> 0x11C82 / 0x4C8E: unpack monsters.dat stats
  -> 0x12A22: round loop
```

Other paths: **rest ambush** (`0x19D64`, mode `-$796B = 3`), **encounter_mod**
`A4-$79A5` (post-combat tiering @ `0x114D6`).

## Related globals

| A4 | Use |
|----|-----|
| `-$79F2` | Current screen 0..59 |
| `-$796C` | Move counter (rest ambush) |
| `-$796B` | Encounter mode (`0x80` = event/pre-filled) |
| `-$11DE[]` | Monster type ids for this fight |
| `-$77BE` | Live monster count |

## Open work

1. Trace `0x11F0A` pool base pointer (16-byte stride) and dump full blob.
2. Correlate `A4-$718A` bytes with arena difficulty tiers (raw dump shows
   zeros/low values on towns, `5` on late castles — likely pool tier, not
   monster id).
3. Grep all `event.dat` OP_12 blocks for fixed fight catalog.
