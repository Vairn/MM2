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

**Superseded — see "Resolved" below.** An earlier pass here claimed a
"master monster pool blob" that wasn't fully mapped; the fully-traced picker
section further down established there is **no such blob** — the picker
selects type ids directly from `monsters.dat` tiers. This paragraph is kept
only as a historical marker that the pool-blob theory was retired.

## Random encounter flow

```
step on map (3D view, not town mode)
  -> 0x10A2: gates (view -$79E2, env -$7660, tile type, rate byte 0x09)
  -> on success: combat setup via -$7EDE -> 0x051C2 (combat_encounter_entry,
     see mm2.capstone.annotated.asm; prints "Encounter!", preps HUD/turn
     order — see docs/17-combat-system.md for the round loop it hands off to)
  -> 0x12CE0: recompute live count (-$77BE) from filled slots + overflow
  -> if -$796B lacks 0x80 (not pre-filled fight):
       -> 0x1213E: loop 0x12072 (budget/gate check) + 0x11F0A (fill -$11DE)
  -> 0x11C82 / 0x4C8E: unpack monsters.dat stats
  -> 0x12A22: round loop
```

Other paths: **rest ambush** (`0x19D64`, mode `-$796B = 3`), **encounter_mod**
`A4-$79A5` (post-combat tiering @ `0x114D6`).

## Random picker — fully traced (`0x1213E` / `0x12072` / `0x11F0A`)

**Resolved:** there is no separate "monster pool blob". The picker selects
monster **type ids directly from `monsters.dat`**, which is organized in
16-entry tiers: `type_id >> 4` = tier (difficulty band), `type_id & 0xF` =
index within the tier. This is confirmed by `0x12072` summing
`(type_id >> 4) + 1` per filled slot as the "XP" cost, and by `0x11F0A`
constructing new type ids as `tier * 16 + rng(1,16) - 1`.

### `0x1213E` — `encounter_random_picker` (outer loop)

```
loop:
  0x12072()                      ; recompute done flag (-$6FC1)
  if -$6FC1: break
  0x11F0A()                      ; monster_adds_friends: add one "friend group"
  0x12072()                      ; recompute done flag again
  loop while !-$6FC1
```

### `0x12072` — `encounter_picker_budget_check`

Sums `(A4-$11DE[i] >> 4) + 1` for `i` in `[0, min(live_count,10))`, plus (if
`A4-$11D4` overflow type is set and `live_count > 10`) the overflow type's
tier cost × overflow count. Sets `A4-$6FC1` (done) when either:

- running total ≥ `A4-$6FCA` (`party_xp_budget`), or
- `A4-$77BE` (live count) ≥ `A4-$5610` (attrib byte `0x0A`, group-size gate)

### `0x11F0A` — `monster_adds_friends`

**CORRECTED** (a first pass at this trace had the attrib clamp and the friend-
count roll swapped — the min/max clamp applies to the *tier pick*, not the
final copy count; fixed after a full re-read of `0x11F9E`-`0x11FBE`):

1. **Roll a tier-pick ceiling:** `base = disposition_mod[A4-$79AE] + A4-$6FC2
   + 1` (`disposition_mod` = data-hunk bytes at `A4-$6FC0`, confirmed
   `{0,1,2,5}`); roll `rng(1,100)` and add a bonus via thresholds `<0x3D`→+0,
   `<0x51`→+1, `<0x60`→+2, else →+3; if the sum exceeds `13` (`0xD`), it is
   **replaced** with exactly `14` (`0xE`, not clamped down to 13).
2. **Pick the tier:** `tier_pick = rng(1, base)`, then clamped to
   `[attrib min (-$560E = byte 0x0C), attrib max (-$560F = byte 0x0B)]`
   (ceiling clamp against max first, then floor clamp against min).
   `tier_index = tier_pick - 1`.
3. **Pick a type:** `type_id = tier_index * 16 + rng(1,16) - 1`. Temporarily
   writes this into `A4-$11DE[0]` and calls the unpack/lookup thunk
   `-$7EF6(a4)` with arg 0, which fills `A4-$11B7` (this monster's "friend
   count" field, sourced from `monsters.dat` Oabil low nibble) as a side
   effect, then restores the original `A4-$11DE[0]`.
4. **Pick a copy count:** `rng(1, A4-$11B7)` — **not** re-clamped by the
   attrib bytes (those only bound the tier pick in step 2).
5. **Fill slots:** starting at the current live count (`A4-$77BE`), write
   `type_id` into consecutive `A4-$11DE[]` slots (incrementing live count)
   until either the count is exhausted or 11 slots are filled; any remainder
   beyond 11 slots is folded into the live-count overflow accounting instead
   of `A4-$11D4` (only script-seeded overflow uses that field explicitly).

The per-monster "friend count" field written to `A4-$11B7` by `-$7EF6` is not
yet named in `mm2_gamestate.h` — flag for symbol harvesting.

## `0x11E58` — `encounter_xp_budget_init` (fully traced)

Called once at combat setup (`0x12CF4`, right before the mode check that
decides whether to run the picker) to (re)compute both budget-related
globals from the current party, confirming the last two "unnamed field"
questions above:

```
party_xp_budget = 0
max_half_level = 0
for i in 0..party_size-1:
    char = get_party_member_ptr(i)           ; -$7F20(a4)
    party_xp_budget += char.hp_current       ; record +0x74 (u16)
    half_level = char.level >> 1             ; record +0x71
    if half_level > max_half_level: max_half_level = half_level
party_xp_budget /= 8                          ; -$7B4E(a4) = long divide (0x24D9A)
if disposition == 0: party_xp_budget /= 4     ; total /32 (cautious -> weakest fights)
if disposition == 1: party_xp_budget /= 2     ; total /16
if disposition == 3: party_xp_budget *= 2     ; -$7B54(a4) = long multiply (0x24C74); total /4 (reckless -> hardest)
                                               ; disposition == 2: unchanged, total /8 (default)
A4-$6FC2 = max_half_level
```

So the encounter "budget" (`A4-$6FCA`) is **total party current HP / 8**,
scaled by the disposition setting (`A4-$79AE`, the same 0..3 "AI mood" cycled
by `GameStateView::cycleDisposition()`): disposition 0 (cautious) allows only
1/32 of total HP in monster-tier cost, disposition 1 is 1/16, the default (2)
is 1/8, and disposition 3 (reckless) is 1/4 — i.e. **disposition scales
encounter difficulty**, not just AI aggression. `A4-$6FC2` (the tier-roll
modifier consumed by `monster_adds_friends`) is simply **half the level of
the party's highest-level member** (rounded down).

## Related globals

| A4 | Use |
|----|-----|
| `-$79F2` | Current screen 0..59 |
| `-$796C` | Move counter (rest ambush) |
| `-$796B` | Encounter mode (`0x80` = event/pre-filled) |
| `-$11DE[]` | Monster type ids for this fight |
| `-$77BE` | Live monster count |
| `-$6FC1` | Picker "done" flag (budget/gate exhausted) |
| `-$6FC2` | Tier-roll modifier byte — `MM2_GS_PICKER_TIER_MOD` in the remake (read @ `0x11F20`) |
| `-$11B7` | Picked monster's "friend count" field — `MM2_GS_MONSTER_FRIEND_COUNT` (set by `-$7EF6`) |

## Open work

1. ~~Trace `0x11F0A` pool base pointer~~ — **Resolved**, see above: no blob,
   direct `monsters.dat` tier indexing.
2. Correlate `A4-$718A` bytes with arena difficulty tiers (raw dump shows
   zeros/low values on towns, `5` on late castles — likely pool tier, not
   monster id).
3. Grep all `event.dat` OP_12 blocks for fixed fight catalog.
4. `A4-$6FC2` is named (`MM2_GS_PICKER_TIER_MOD`) — remaining gap: confirm what `-$7EF6(a4)` unpacks besides `A4-$11B7`.
