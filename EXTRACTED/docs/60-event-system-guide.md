# The MM2 Event System — A Human-Friendly Guide

A plain-language tour of how Might & Magic II (Amiga) turns "the party steps on
a tile" into dialogue, shops, combats, teleports, and quest rewards. This is a
reading guide, not a reference — the deep technical tables are linked at every
step. If you only read one section, read §1 and §3.

**Source of truth:** the 68k disassembly (`EXTRACTED/mm2.capstone.asm`) and the
C++ remake (`game/src/events/EventRuntime.cpp`, ~99% ASM-faithful). When any
doc disagrees with those, the code/ASM wins.

---

## 1. The 30-second overview

Everything interactive in MM2's world lives in one file: **`event.dat`**.

Each map (town, overland sector, dungeon, castle, ...) has a *location record*
inside it. A record contains three things:

1. **Trigger table** — "at tile (5,7), when facing south, run event #1."
2. **Bytecode scripts** — one small program per event, built from ~51 opcodes.
3. **String table** — all the text those scripts display.

At runtime the engine does, in order:

```
enter map ──► load record ──► step onto a tile ──► trigger matches? ──► run bytecode
                                                      │
                                                      ▼
                                        show text / ask y/n / take gold /
                                        give item / start combat / teleport /
                                        open a shop menu / change the map…
```

That's the whole system. The rest of this doc walks each stage.

---

## 2. The data file: how `event.dat` is laid out

`event.dat` is 95,687 bytes holding **71 locations**: a 71×6-byte header, then
the location records back-to-back.

```
Header entry (big-endian):  u32 data_offset  +  u16 data_length (clamped to 2220 at runtime)
```

Locations **0–59** line up with `map.dat` screens (0–4 towns, 5–16 and 33–40
overland sectors, 17–32 dungeons, 41–44 elemental planes, 45–52 castle
basements, 53–59 castles). Locations **60–70** are *not* maps — they are
overlay banks: shared quest text, castle blobs, the Hall of Spells pool (see §8).

Each normal location record is:

```
┌─────────────────────────────────────────────┐
│ Trigger triplets: (pos, event_id, cond)     │   3 bytes each
│ ...                                         │
│ 00 00 00                  (terminator)      │
├─────────────────────────────────────────────┤
│ String-table offset (2 bytes, little-endian)│   relative to its own position
├─────────────────────────────────────────────┤
│ Script bytecodes (0xFF-terminated segments) │
├─────────────────────────────────────────────┤
│ String table (0xFF-terminated strings)      │
└─────────────────────────────────────────────┘
```

- **pos** packs a tile coordinate as `(y << 4) | x` in a 16×16 grid.
- **event_id** is which script segment to run (the Nth `0xFF`-terminated
  block in the script pool).
- **cond** is a bitmask of which party *facing(s)* trigger the event.

Strings are terminated by `0xFF` (not `0x00`), referenced positionally
(`str[0]`, `str[1]`, ...), and use `@` (0x40) as a line break.

Full byte-level detail: [`06-event-dat-format.md`](06-event-dat-format.md).

---

## 3. How an event fires: the tile scanner

Every time the party moves, the engine asks: *"is there a trigger on the tile
we just entered, and are we facing the right way?"*

```
context = facing_context[facing_index]     # W=0x10, S=0x20, E=0x40, N=0x80
party_tile = (y << 4) | x
for each triplet (pos, event_id, cond):
    if pos == party_tile and (cond & context) != 0:
        run script number event_id
```

So a shop door labeled when *facing south* has cond `0x20`; a teleport pad
that fires no matter which way you walk in has cond `0xF0` (all four bits).
There is no "always fires regardless of facing" value other than setting all
four bits — a `0x10` door label genuinely requires facing west.

Before a script runs there is also an **era gate**: the current era must match
the screen's `attrib.dat` era byte, which is how time-travel areas appear and
disappear.

ASM: loader `0x92F2`, init `0x1754A`, scanner `0x175E2`.
Port: `EventRuntime::enterLocation`, `initParsed`, `scanAndRun`.
Reference: [`08-event-runtime.md`](08-event-runtime.md).

---

## 4. The script interpreter (a tiny stackless VM)

Scripts are executed by a fetch/dispatch loop with a jump table for opcodes
`0x00..0x32`; `0xFF` ends the script, anything `>= 0x33` aborts it.

There are **no locals and no stack**. All state lives in a handful of
game-state bytes, the most important being:

| Byte | Name | Role |
|------|------|------|
| `A4-$7951` | **cond_flag** | The one and only "result register". Predicates write it; branches read it. |
| `A4-$7956` | parse_pos | The program counter inside the record. |
| `A4-$79EA` | script_abort | Set to stop the VM (combat, shops, invalid ops). |
| `A4-$7950` | exit_flags | Modal/ESC bits for the UI layer. |

**The cond_flag pattern** is the key to reading scripts. A *predicate* opcode
(item check, gold check, dice roll, era check, y/n answer...) writes a value
into `cond_flag`, and a *branch* opcode (`OP_10` = skip if set, `OP_11` = skip
if clear, `OP_2B` = skip if last combat was won) jumps over the "else" part.
Branches skip *tokens* — whole opcodes with their arguments — using a
byte-exact ROM length table (`eventVmTokenDelta` in the port).

**Waiting for the player.** When a script hits a prompt it pauses and the
engine resumes it on input. The port models this with `EventVmWait`:

- **Space** — "press space to continue" (`OP_07`/`OP_08`)
- **YesNo** — y/n answer goes into `cond_flag` (`OP_09`/`OP_0A`)
- **MemberSelect** — pick a party member (`OP_26`/`OP_27`)
- **Answer** — type up to 10 characters (`OP_2F`), checked by `OP_30`
- **HexDigit / LetterSelect** — teleport coordinates, quest difficulty A–D
- **Delay** — timed pause (`OP_1E`)

---

## 5. What opcodes actually do (by family)

All 51 opcodes are tabulated in [`07-event-script-opcodes.md`](07-event-script-opcodes.md)
(matched to `EventRuntime::dispatchOp`). Grouped by what they *feel like* in
game:

### Talking & signage (OP_01–OP_06)
All read one byte — a string index into the *current location's* string table:

- `OP_01` one-line message; `OP_02` multi-line block; `OP_03` tall block.
- `OP_04` **door label** centered above a shop door ("Middlegate Inn").
- `OP_05` borderless plaque popup (lore, warnings, missing-person posters).
- `OP_06` outdoor **signpost** with a glyph-drawn frame and post; it rewrites
  `-` to a full-width bar glyph before drawing.

`OP_0B` looks like a text op but isn't: it draws a **signboard sprite**
(`.anm`) over a shop tile. Its first byte is a sign-table key, never a string
index — an old guess that it printed captions was wrong and removed.

### Flow control (OP_07–OP_11, OP_0F, OP_29, OP_2B)
Waits, y/n, end-script, abort, and the three token-skip branches.

### Checking the party (OP_15, OP_16, OP_17, OP_1B, OP_2D, OP_32)
"Does anyone own item X?", "load a quest variable", "is anyone a Crusader?",
"clear cond unless it's at least N". Party fields are addressed through a
selector byte remapped by the field engine at `0x17766` — selector `0x3E` is
gold, `0x20` is current HP, etc. (`EventFieldMap.h`, extracted byte-exact).

### Money & items (OP_19, OP_24, OP_25, OP_28, OP_2A)
- `OP_24` **gold**: pools the whole party's gold, pays if affordable, re-shares
  the remainder equally; cond = success. `OP_25` is the same for gems.
- `OP_19` gives an item into the first free backpack slot (if all are full it
  goes to the shared **found-item buffer**).
- `OP_28` consumes an item from backpacks (bishop keys, quest discs).
- `OP_2A` fills the found-item buffer with a full treasure (gold + gems + up to
  3 items) — it does **not** hand anything out. The player picks it up with the
  **Search** key, which runs the "The Party Has found:" flow.

### Rolling dice & calendars (OP_1C, OP_22, OP_23)
`OP_1C` rolls `1..N` into cond. `OP_22` gates on the current **era**;
`OP_23` gates on the **day of year** (including odd/even-day modes `0xB5`/`0xB6`).

### Combat (OP_12, OP_13)
Both seed the encounter-setup block (up to 10 monster-group slots + overflow
type + count) and abort the script so the combat engine takes over. `OP_12` is
a fixed fight; `OP_13` lets the random picker augment the fight by XP budget.
Dungeon tiles typically read `OP_2B` (skip if you already won) / `OP_12` / `OP_14`.

### Changing the world (OP_0C, OP_14, OP_21)
- `OP_0C` **map transition**: move the party to another screen/tile (with some
  randomized destination remaps).
- `OP_21` patches a map tile's visual + collision bytes (opened doors, lowered
  barriers) and flags a redraw.
- `OP_14` clears the tile's event bit so an ambient/one-shot event doesn't
  refire while you stand on it.

### Party effects & damage (OP_1F, OP_20, OP_31)
`OP_1F`/`OP_20` are pure field arithmetic — add/subtract a 3-byte value to any
character field (stats, XP, gold...), saturating at the field width; subtract
that underflows clears cond and aborts the rest of the writes ("can't afford").
`OP_31` iterates damage across the party and aborts the game if nobody lives.

---

## 6. A worked example: Middlegate

From location 00 (decoded with `python tools/decode_event.py event.dat 0`):

Trigger table excerpt:

| Tile | Facing | Event |
|------|--------|-------|
| (5,7) | south | **1** |
| (2,13) | any | **9** |
| (15,15) | north+east | **17** |

Event 1 is the simplest possible script — two bytes:

```
04 01   →   show_text_above_door(str[1])   →   "Middlegate Inn"
```

Walk up to the inn door facing south and the name appears above it. That's the
entire event.

Event 9 is the **arena entrance**: it checks for a ticket item (`OP_0E`
selector `0x08`), shows "Sorry, but you must have a ticket..." if you have
none, or consumes the ticket and starts a scaled arena fight if you do.

Event 17 is the **Feldecarb Fountain**: a farthing check, a y/n prompt, and a
castle key on success — with the text pulled from the shared quest bank
(location 60), not Middlegate's own strings.

A "shop tile" typically stacks several scripts: a door label (`OP_04`), a
signboard sprite (`OP_0B`), and a service selector (`OP_0E`) that opens the
actual menu. Per-location decoded pages for all 71 locations:
[`events/README.md`](events/README.md).

---

## 7. Town services: the `OP_0E` rabbit hole

`OP_0E` reads one **selector byte** and hands control out of the script VM and
into the game's service engine: inns (`0x01`), training (`0x02`), taverns
(`0x03`), temples (`0x04`), mage guilds (`0x05`), blacksmiths (`0x06`), the
general store (`0x07`), the **arena** (`0x08`), plus special quest handlers
(`0xC9`/`0xCA` Hoardall & Lord Slayer, `0xCF` the Wayback machine, `0xE2` the
jester's joke-of-the-day, `0xFD` endgame transitions...).

Two important facts:

1. **Shop text does not come from `event.dat`.** The script only passes a
   selector byte; the service handlers draw their menus from **`str.dat`**
   or strings embedded in the executable. See
   [`30-event-to-string-path.md`](30-event-to-string-path.md).
2. **Unmatched selectors go to a default-range dispatcher** (`0x15EDC`) which
   bins the value into a category and re-enters the event *loader* to run an
   **overlay record** (the string banks of locs 60–70). This is how cross-town
   quests like Nordon's goblet share one copy of their dialogue.

In the remake, interactive menus are pluggable: `EventRuntime` runs faithful
transaction logic (costs, stat writes, gold checks — all ASM-verified leaves in
`TownServiceTransactions.cpp`) and a swappable `ITownServiceUi` backend does
the presentation. Unported engines show the real intro text and *defer* rather
than inventing behaviour. Deep dive: [`28-town-services.md`](28-town-services.md).

---

## 8. Overlay locations 60–70: the "not maps" records

| Loc | What it is |
|-----|------------|
| 60 | Shared quest string bank (Nordon / Nordonna / Corak intro) |
| 61 | Encoded spell/hireling index tables |
| 62 | Side quests (Chris cartography, Gertrude/Rat Fink) |
| 63 / 65 / 68 | **Castle blobs** — no trigger table at all; only reachable via queued dispatch |
| 64 | Lord Haart heirloom quest |
| 66 | Endgame (Corak / Murray / Horvath) |
| 67 | Hall of Spells mixed text/script pool (has opcodes outside the normal range) |
| 69 | Queen Lamanda storyline |
| 70 | Meta bank: Hall of Spells, bishops, puzzles |

They use the same header indexing as maps but different internal shapes, so
the runtime treats them as queued/overlay dispatches rather than tile scans.

---

## 9. The remake port (`game/`)

| Port file | Role |
|-----------|------|
| `game/src/events/EventRuntime.cpp` | **Authoritative opcode semantics** — loader, scanner, VM loop, waits, overlay dispatch |
| `EventVmHelpers.cpp` | Token-skip table, variables, gold/gems pool-pay, arena helpers |
| `EventTownServices.cpp` | `OP_0E` selector dispatch |
| `EventCombatEncounter.cpp` | `OP_12`/`OP_13` encounter seeding |
| `EventPartyEffects.cpp` | `OP_1F`/`OP_20` field arithmetic |
| `ServiceSignResolver.cpp` | `OP_0B` sign table |
| `TownServiceMenu.cpp` + `PlayTownServiceUi.cpp` | Pluggable shop/temple UI backend |
| `game/tests/event_*.cpp` | VM regression tests (e.g. `event_middlegate_test`) |

The host game loop integrates it as: `enterLocation` on screen change →
`scanAndRun` after each step when the pending-event latch is set →
`continueInput` feeds SPACE/y-n/member-select/typed answers back into a paused
script. `bindParty` / `bindCombat` / `bindTownServiceUi` / `bindRng` wire the
VM to the rest of the engine.

**Documented deviations** (ASM unclear, kept minimal):
- `OP_14` additionally sets a per-visit "resolved" flag so one-shot tile
  events don't refire; the original only clears a collision bit.
- Presentation-only ops (`OP_1D` audio wait, `OP_1E` delay) consume their
  arguments without inventing state.

Everything unported is *deferred with ASM addresses*, never fabricated — see
[`56-event-system-remaining-gaps.md`](56-event-system-remaining-gaps.md).

---

## 10. Fun quirks worth knowing

- **A real ROM bug in the skip table:** `OP_25` (gems) executes as 3 bytes but
  the static token-length table says 2. A branch that skips *over* an `OP_25`
  would desync the parser by one byte — verified unreachable in the shipped
  `event.dat`, but reproduced byte-exactly in the port for fidelity.
- **A real ROM bug in arena rewards:** the victory loop corrupts
  `record+0x79` instead of setting a class-quest flag cleanly
  ([`36-class-quest-hp-bug.md`](36-class-quest-hp-bug.md)).
- **Treasure waits for Search:** `OP_2A` only fills a buffer; picking it up is
  the Search key's job, exactly as on the Amiga.
- **Passwords are stored inverted:** `OP_30` compares each typed character
  against `0x11A - expected[i]`. Decoded answers include "MEENU" and "KEYS".
- **`OP_0D` is audio, not graphics** — older notes calling it a graphics
  sequence were wrong; it plays one of 10 sound sequences.

---

## 11. Tooling & further reading

```powershell
python tools\decode_event.py event.dat 0        # decompile Middlegate
python tools\decode_event.py --predicates …     # all gold/gem/item gates
python tools\build_event_location_docs.py       # regenerate events/*.md
python -m mm2_event_lang decompile -o events event.dat   # readable .mm2evt DSL
```

| Doc | What it answers |
|-----|-----------------|
| [`06-event-dat-format.md`](06-event-dat-format.md) | Exact file/record layout |
| [`07-event-script-opcodes.md`](07-event-script-opcodes.md) | Every opcode, argc, ASM address, verification status |
| [`08-event-runtime.md`](08-event-runtime.md) | Loader/scanner/VM ASM ↔ C++ map |
| [`30-event-to-string-path.md`](30-event-to-string-path.md) | Which text comes from where |
| [`44-event-text-rendering.md`](44-event-text-rendering.md) | Pixel-exact text draw paths |
| [`42-event-dsl-format.md`](42-event-dsl-format.md) | Human-readable `.mm2evt` script format |
| [`28-town-services.md`](28-town-services.md) | Shops/temples/training internals |
| [`events/README.md`](events/README.md) | Decoded scripts for all 71 locations |
| [`56-event-system-remaining-gaps.md`](56-event-system-remaining-gaps.md) | What's still unknown |
