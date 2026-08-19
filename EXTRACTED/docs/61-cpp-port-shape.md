# C++ remake shape — 68k transcribed vs readable C++

Reading notes on `game/` (and a bit of `editor/`). This is **not** a bug list
and **not** a rewrite plan. The 68k disassembly is still the source of truth
(`CLAUDE.md`). The remake is supposed to look like the ROM in places; that is
the point. What follows is where that choice makes the C++ *strange to a C++
reader*, so the next person does not “clean it up” into something that desyncs
scripts.

Companion: opcode table in [07-event-script-opcodes.md](07-event-script-opcodes.md),
runtime map in [08-event-runtime.md](08-event-runtime.md), human tour in
[60-event-system-guide.md](60-event-system-guide.md). The *readable* script
layer is the editor DSL ([42-event-dsl-format.md](42-event-dsl-format.md)),
not `EventRuntime`.

---

## 1. Thesis

`EventRuntime::dispatchOp` is the ROM jump table at `0x17494` written as a
dense `switch (EventOp)` whose values **are** the bytecode (`EventOp.h`).
Each arm calls `EventRuntime::opGiveItem` / `opScanPartyItems` / … — same
`readU8` order, same A4 writes. Names match `editor/.../OpcodeTable.cpp`.

Side effects still go through A4 (`eventCond` / `setEventCond` /
`orEventExit` / `setEventAbort` in `EventVmRegs.h` wrap `mm2_gs_*`; they
do not add shadow state). Backpack access is SoA byte runs
(`rosterBackpackId` → `+$3A`), not `rec.backpack[m].id`.

That is ASM-as-C with names. It is also how you keep `event.dat` byte-exact.

A “prettier” VM would look like `bool cond`, a program-counter object, and
`Character&`. Several scripts would then break, because `cond` is **not a
bool** in the ROM. Do not replace the switch with a C++ vtable.

---

## 2. Event VM — what looks wrong on purpose

All of this lives in `game/src/events/EventRuntime.cpp` unless noted.

### 2.1 The CPU is `uint8_t *a4`

`GameStateView` (`game/include/mm2/GameState.h`) is a pointer into the A4
workspace plus a few named getters. The VM does not own a `struct EventCpu`.
Parse position, cond, abort, exit flags, overlay latch, victory latch, and
the 2220-byte work buf are A4 slots (`EXTRACTED/decomp/mm2_gamestate.h`).

`MM2_GS_COND_FLAG` is `-0x7951`. Negative macros look like a sign error. They
are signed A4 displacements; canonical form is documented at the top of
`mm2_gamestate.h`.

### 2.2 `cond` is a byte register, not a boolean

| Opcode | What it writes to `-$7951` |
|--------|----------------------------|
| `OP_17` | raw variable byte (`move.b (a0),-$7951`) |
| `OP_1C` | raw `rng(1, hi)` |
| `OP_32` | party nibble-match **count** |
| `OP_16` | match count on the first member that has the item |
| `OP_1B` | zero the byte if it is `< threshold` |

`OP_10` / `OP_11` treat it as truthy/falsy. `OP_1B` treats it as magnitude.
A previous port booleanized `OP_17` and broke threshold gates. Comment is
still on `opLoadVarRawToCond`.

### 2.3 Two different “how long is this opcode” answers

- **Execute:** the handler `readU8`s however many bytes the ROM handler reads.
- **Skip (`OP_10` / `OP_11` / `OP_2B`):** `skipTokens` → `eventVmTokenDelta`,
  copied from `A4-$6CC8`.

Those disagree for `OP_00` (skip 0, not 1) and `OP_25` (execute 3 bytes, skip
table says 2). Skipping *over* an `OP_25` in the original game desyncs the
stream by one byte. The port copies the bug. See `EventVmHelpers.h`
`eventVmTokenDelta`.

### 2.4 Discarded argument bytes

The ROM often reads a byte and throws it away. The port must still consume it
or every later opcode in that script shifts.

| Op | Strange consume |
|----|-----------------|
| `0x16` | first byte read, overwritten, **unused** |
| `0x17` | second byte read and discarded (resolver keys on group id only) |
| `0x1A` | **two** bytes only (id, value). A 3rd-byte read used to desync scripts |
| `0x1D` | argc=1, no GS write (audio wait stub) |
| `0x28` | first arg discarded; item id is the second |

### 2.5 Magic high bits instead of extra opcodes

The ROM packs modes into unused bits of an existing argument.

| Pattern | Meaning |
|---------|---------|
| `OP_0C` dest `& 0x40` | random dest screen (`rng(1,20)+5`, then `>=0x11` add `0x10`, set bit7) |
| `OP_0C` dest `>= 0x80` | random dest tile |
| `OP_19` arg1 `>= 0x80` | item id comes from **current cond**, not the literal id byte |
| `OP_23` arg1 `0xB5` / `0xB6` | odd/even day, not a range compare |
| `OP_2D` arg1 bits 7/6/5 | race / sex / “any member” |
| `OP_2E` arg1 `>= 0x80` | cleric pair `{3,1}` instead of sorcerer `{4,2}` |

None of these have names in the bytecode. The editor DSL is where they get
names.

### 2.6 Roster as a byte array

`Mm2RosterRecord` is an AoS struct. Several event ops do:

```cpp
auto *rec = reinterpret_cast<uint8_t *>(&roster_->records[slot]);
rec[0x3A + m] = id;   // backpack id run
rec[0x40 + m] = attr2;
rec[0x46 + m] = attr3;
```

The on-disk / in-RAM layout is structure-of-arrays (six ids, then six
charges, then six flags). The C struct’s named item slots do **not** match
that walk. `case 0x19` says so. Do not “fix” this to `rec.backpack[m]` until
the codec layout is the same walk.

`EventFieldMap.h` is generated from the ROM jump table at `0x17FEA`. Selector
`0x74` is “class-quest / guild mask at record+0x79”, not a C++ enum.

### 2.7 `OP_0E` is one opcode, a dozen games

`case 0x0E` sets abort, sets `MM2_GS_OP0E_SUBMODE = 2`, reads a selector, and
calls `eventExecTownSelector`. That function is inns, temples, smith, guild,
tavern, arena tickets (`0x08` → `0x9D76`), general store, circus, hex
teleport `0x7E`, endgame `0xFD`, plus a default-range overlay reinvoke of
`event_dat_loader` (`-$7DFA` → `0x92F2`).

A human design would be separate opcodes or a `Service` interface. The ROM
has one handler and a byte. Splitting it in the VM would make `event.dat`
unreadable against the switch.

### 2.8 Overlay bank vs async combat (port invention)

ROM `OP_12` is `jsr -$7EDE` **inside** the overlay VM, then the scanner
epilogue reloads the home location. The remake’s combat is a multi-frame
`CombatSession`. `restoreOverlayIfIdle` memcpy’s the saved work buf back onto
A4 **without** waiting for combat, because a combat-active guard left `loc_`
on the overlay bank and every event-flagged tile fell through to ambient
combat.

That function is the strangest control-flow in the file. It exists because
the host is frame-pumped and the ROM is not.

Same split: town menus. ROM `keyread` blocks. `PlayTownServiceUi` capture
hooks return `false` so `townSvcRun*` exits, then `GameSession` pumps
`handleKey` / `render` across frames.

### 2.9 `OP_14` extra flag

`eventVmClearTileEventFlag` ANDs the collision page. Comment on `case 0x14`:
that is **not** enough to stop the triplet walk (example: cavern `(1,2)` is
already `0x41`). The port adds `markTileEventResolved` checked by
`scanAndRun`. Labelled `PORT DEVIATION (ASM unclear)`. Do not delete it
without a new ASM trace.

### 2.10 `OP_0B` is a sign id, not a string index

`str_idx` indexes a per-env `.anm` table (`0x15756`), not `str.dat`. Feeding
it to `resolveString` used to print the wrong line (Middlegate `0x14` =
“Fool, you have no farthing…” on guild/goblet doors). Handler draws a
portrait and sets exit-flag bit 2. No text.

---

## 3. Other C++ that has the same smell

| Area | File | What’s strange |
|------|------|----------------|
| Host scheduler | `game/src/GameSession.cpp` (~4.3k lines) | The `LAB_1280` main loop: move, latch, `scanAndRun`, overlay enum, combat, shops, death, scripted scenes. Not a small “session” class. |
| Combat | `game/src/combat/CombatSession.cpp` | Round loop / options / to-hit transcribed from `0x12A22` / `0x10478`. State enum is a list of ROM waits (`AwaitingBribeKind`, `AwaitingCastLevel`, …). |
| Movement / bash | `game/src/gameplay/ExploreActions.cpp` | Inclusive `rng(min,max)` = `min + r / (0x8000/span)` from `0x22BC6`. |
| Town leaves | `game/src/events/TownServiceTransactions.cpp` | Guard order copied from `0x1BE44` (condition, backpack, merchant half, gold). Reordering would be “cleaner C++” and wrong. |
| HUD | `game/src/gfx/PlayScreenChrome.cpp` | Cell columns/rows, glyph codes `0x0E..0x15`, `A4-$79AB` protect bytes. |
| Editor graph | `editor/src/sections/eventgraph/` | Human view of the **same** bytecode. Runtime stays numeric; the graph names things. |

`GameSession` knowing `OP_0B` stays up during shops, `OP_02` must be dropped
for menus, and funeral outranks `OP_0E FD` is the host having to re-implement
ROM draw order. That will never look like a textbook game loop.

---

## 4. What *does* look like C++

These are already the readable layer. Do not flatten them back into A4 pokes.

- `editor/src/eventlang/` — AST, DSL parse/emit, opcode names.
- `TownServiceMenu` / `TownServiceTransactions` — named options (`Heal`,
  `BuySpell`) wrapping the leaves.
- `GameStateView::setFacingKey` — still writes A4, but the switch is `N/E/S/W`.
- Codecs (`mm2_roster_codec.h`, `mm2_event_codec.h`) — file layout, not the VM.

---

## 5. Named jump table (allowed refactor)

`dispatchOp` is a dense `switch (EventOp)` whose values **are** the bytecode
(`EventOp.h`, names match `editor/.../OpcodeTable.cpp`). Each arm calls
`EventRuntime::opGiveItem` / `opScanPartyItems` / … — same `readU8` order,
same A4 writes. gcc on 68000 turns that switch into a jump table; do not
replace it with a C++ vtable or `std::function`.

Still keep:

1. Opcode **values** = event.dat bytes (`enum class EventOp : uint8_t`).
2. `cond` as `uint8_t` (`eventCond` / `setEventCond` in `EventVmRegs.h`).
3. Execute-length vs skip-table length as two functions.
4. Discarded argument reads.
5. High-bit modes (`kOp0cRandomScreen`, `kOp19ItemFromCond`, `kOp23OddDay`, …).
6. Backpack as SoA runs (`rosterBackpackId` → `+$3A`, not named item slots).

The DSL and editor graph remain the human-readable script layer. Do not
booleanize `cond` or invent extra opcodes.
