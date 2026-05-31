# event.dat Script Interpreter (Opcode Map)

This note decodes the bytecode dispatcher used by the `event.dat` script section.

Derived from:
- dispatch loop at `0x172CA` / `0x1750C`
- opcode jump table at `0x17494`
- handler stubs at `0x172D8..0x17484`

## Core Behaviour

- Bytecode opcodes are in range `0x00..0x32`.
- `0xFF` terminates a script block.
- Invalid opcodes branch to `0x1748C` and set abort flag `A4-$8616`.
- After each opcode, control returns to `0x1750C` to fetch next byte.

## Verification Against ASM (cross-checked)

The following were re-confirmed directly from `mm2.capstone.asm`:

- **Tile-event scanner `0x1754A`**: triplet table is 3-byte groups `(pos, event, cond)`
  read from offset 0, terminated by the first `00 00 00` group. String-table
  offset = `(terminator+3) + str_rel_word`; script PC start = `terminator+5`.
  This matches `decode_event.decode_location` exactly.
- **Position byte** = `(row<<4)|col` i.e. `(y<<4)|x` (`0x17608`-`0x17618`),
  confirming the `(y,x)` trigger order.
- **Cond byte** is AND-masked with the direction/context byte and the event is
  skipped when the result is zero (`0x17682` `and.b` + `beq`).
- **Event id** selects a record in one `0xFF`-delimited pool (skip loop at
  `0x17262`). The pool mixes opcode scripts and plain-text records.
- **Dispatch** `0x172CA` -> table `0x17494`; valid opcode range `0x00..0x32`
  (`cmp.l #$33` at `0x174FA`).
- **`argc` fix**: `OP_03` reads 1 byte (was previously logged as 0).
- **Special records**: locations 63, 65, 68 contain no `00 00 00` terminator and
  are *not* processed by the tile-event scanner (castle / Hall-of-Spells style
  data); the decoder now flags these as unreliable.

## Opcode Table (Current Decode)

`argc` is inferred from `read_u8/read_u16/read_u24` helper usage in each handler.

| Op | Handler | argc | Notes |
|----|---------|------|-------|
| `00` | `0x1748C` | 0 | invalid/abort |
| `01` | `0x15924` | 1 | show text; `u8` string index (via `0x15884`->`0x155BE`) |
| `02` | `0x15942` | 1 | show text block; `u8` string index (stub also pushes const `0x13`) |
| `03` | `0x159CE` | 1 | show text; calls `0x15942` (const `0x11`) which reads `u8` string index |
| `04` | `0x159F4` | 1 | show text above door; `u8` string index |
| `05` | `0x15A46` | 1 | show text popup A; `u8` string index |
| `06` | `0x15AEE` | 1 | show text popup B; `u8` string index |
| `07` | `0x15CE6` | 0 | wait-loop until SPACE key, then continue |
| `08` | `0x15D26` | 0 | wait for key (sets cursor mode `0xFD`, calls wait helper `0x15CE6`) |
| `09` | `0x15D3C` | 0 | y/n prompt, writes result into condition flag |
| `0A` | `0x15D9A` | 0 | y/n prompt variant (mode `0xFD`, calls `0x15D3C`) |
| `0B` | `0x15DB0` | 2 | open service/title window (`u8` string via `0x15756`, `u8` position) |
| `0C` | `0x15E12` | 2 | map transition to `(u8 dest, u8)` |
| `0D` | `0x15EC4` | 1 | `u8` -> engine thunk `-$7E42` (generic engine call) |
| `0E` | `0x160C2` | 1 | selector dispatch (see selector table below) |
| `0F` | `0x162A6` | 0 | **end/return script** (sets stop flag, calls cleanup `0x171AC`) |
| `10` | `0x162B8` | var | IF(cond) skip N token-stream entries |
| `11` | `0x162DC` | var | IF(!cond) skip N token-stream entries |
| `12` | `0x16300` | **12** | encounter/combat setup: 10-byte block + 2 bytes (dispatch pushes 0) |
| `13` | `0x16386` | **10** | encounter setup variant (calls `0x16300` with flag=1, 10-byte block) |
| `14` | `0x16398` | 0 | clear current tile event high-bit / clear runtime event flag |
| `15` | `0x16426` | **3** | apply state to party members (`u8 count, u8, u8`; 4th byte only when flag=1) |
| `16` | `0x16520` | 2 | scan up to 6 monster slots; set cond if field `+0x3A`/`+0x28` matches arg2 |
| `17` | `0x165A4` | 2 | load condition byte from resolved variable pointer |
| `18` | `0x165C6` | **4** | `OP_15` handler with flag=1 (`u8 count, set, and, or`) -> masked party apply |
| `19` | `0x165D8` | 4 | add entity to party/roster (writes fields `+0x3A/+0x40/+0x46`) |
| `1A` | `0x166F8` | 2 | store `u8` into resolved variable pointer |
| `1B` | `0x16724` | 1 | clear cond unless `cond >= u8` (threshold gate) |
| `1C` | `0x16742` | 1 | engine query `-$7BB4(1, u8)` -> cond |
| `1D` | `0x16762` | 1 | engine call indexed by `u8*7+1` (`-$7E84`) |
| `1E` | `0x16780` | 1 | timed wait/delay loop for `u8` iterations (`-$7BC0`/`-$7BD2`) |
| `1F` | `0x1690E` | 6 | apply effect to party members (selector + values via `0x167B0`) |
| `20` | `0x16A22` | **6** | party-effect variant (calls `0x1690E` with mode=1) |
| `21` | `0x16A34` | 3 | set tile data at `(y,x)` (arg1 nibbles) into arrays `-$55BA`/`-$54BA` |
| `22` | `0x16A9E` | 2 | set condition if **current era** (`-$79B5`, low byte of era word) is in `[lo, hi]` — era/time-range gate |
| `23` | `0x16ADA` | 2 | reads day-of-year (`-$79DE[era]`) and range-checks it (calendar/day gate) |
| `24` | `0x16B54` | 2 | check `u16` gold amount -> condition flag (strong evidence) |
| `25` | `0x16B82` | 2 | check non-gold 16-bit code -> condition flag (`hi,lo` bytes) |
| `26` | `0x16BC0` | 0 | prompt to select a party member (ESC `0x1B` ends script) |
| `27` | `0x16BC0` | 0 | select-party-member variant (input mode 1) |
| `28` | `0x16C86` | 2 | consume item id (arg1) from party inventory -> condition flag |
| `29` | `0x16D08` | 0 | force abort/exit flag |
| `2A` | `0x16D16` | 14 | set treasure/reward block: `u24 gold/exp, u16 gems, 3x(id,?,?)` |
| `2B` | `0x16D74` | var | `u8` + token-skip walk |
| `2C` | `0x16D98` | 1 | add `u8` to state var `-$79B8`, flag redraw |
| `2D` | `0x16DBA` | 2 | check party-member attribute (arg1 bits select fields `+0xC/+0xE/+0xF`) -> cond |
| `2E` | `0x16F50` | 2 | set attribute bit (`or` arg2) on matching party class -> field `+0x51` |
| `2F` | `0x16FEA` | 0 | clear/space-fill input buffer `-$5C50` |
| `30` | `0x17034` | **10** | **answer/password check**: 10 bytes vs player input; `char = 0x11A - byte` (`0xFA`=space) |
| `31` | `0x170BC` | 3 | iterate target party members and call engine op |
| `32` | `0x17190` | 1 | load condition from script variable id |

## Notes on `*_SKIPTOK` Opcodes

Opcodes `0x10`, `0x11`, and `0x2B` call token skip helper `0x157FC`.
That helper:

1. Reads a count byte.
2. Repeats `count` times:
   - reads current token byte from script stream,
   - looks up token length delta in table at `A4-$6CC8`,
   - advances script pointer by that delta.

So these opcodes are deterministic but variable-length and need token table
support for full static disassembly.

## Semantics Confirmed in This Pass

- `0x22` (`0x16A9E`) checks current event id (`A4-$864B`) against script range args and writes boolean to condition flag `A4-$86AF`.
- `0x10`/`0x11` branch via token-skip helper `0x157FC`, gated by `A4-$86AF`.
- `0x14` clears current tile's visited/event bit in `A4-$AB46` and clears runtime flag byte `A4-$AA2A` bit 7.
- `0x29` sets abort flag `A4-$8616 = 1`.
- `0x32` copies a variable byte (via engine thunk `-0x7F2C`) into condition flag `A4-$86AF`.

## `OP_0E` Selector Dispatch (`0x160C2`)

`OP_0E` reads one selector byte and runs a chained-subtraction switch:

- **Explicit cases** call dedicated engine thunks:
  - `1` tavern/food, `2..8` other town buildings (each a distinct `-$7Dxx` thunk),
  - `100 (0x64)`, `126..131 (0x7E..0x83)`, `201..207 (0xC9..0xCF)`, `226 (0xE2)`,
    `253 (0xFD)` are further specials (travel/portal/quest handlers).
- **Default** (any other selector) -> `0x15EDC`, which bins the selector into a
  *service category* using engine range-check `-$7E9C` and calls town handler `-$7DFA`:

  | selector range | category byte | adjusted index base |
  |----------------|---------------|---------------------|
  | `0x09..0x10`   | `0x3C`        | `-8`  |
  | `0x11..0x37`   | `0x3D`        | `-0x10` |
  | `0x38..0x4B`   | `0x3E`        | `-0x37` |
  | `0x4C..0x54`   | `0x3F`        | `-0x4B` |
  | `0x56..0x5B`   | `0x40`        | `-0x55` |
  | `0x5C..0x5E`   | `0x41`        | `-0x5B` |
  | `0x65..0x69`   | `0x42`        | `-0x64` |
  | `0x6A..0x7C`   | `0x43`        | `-0x69` |
  | `0x97..0x98`   | `0x44`        | `-0x96` |
  | `0xE3..0xF3`   | `0x45`        | `-0xE2` |
  | `0xF4..0xFB`   | `0x46`        | `-0xF3` |

Gameplay-observed selector meanings (shops/temple/training/guilds) live in the
`SELECTOR_NAMES` map in `tools/decode_event.py`.

## `OP_30` Answer Check (Verified)

`OP_30` stores a 10-byte expected answer inline, encoded as `byte = 0x11A - ascii`
(`0xFA` = trailing space). Decoded answers found in `event.dat`:

- `"MEENU"`, `"KEYS"` (matching user-reported answer words), and numeric
  combination codes `"46"`, `"23"`, `"64"`, `"32"`.

## Disassembly Alignment Check

After the `argc` corrections (`OP_12=12`, `OP_13=10`, `OP_15=3`, `OP_18=4`,
`OP_30=10`), a linear walk of every real script segment now consumes each
segment exactly: **0 overruns, 0 underruns** (excluding token-skip and text
records). Previously there were 35 underruns from the wrong arg counts.

## Atlantium (Location 1) — User-Verified Examples

- **Shop/sign tiles** (`Carriage Inn`, `Drewnhald Ironworks`, `Boar's Tongue Tavern`, etc.):
  - Behavior: text only, retriggers when stepping onto tile again.
  - No immediate movement/state side effect observed.

- **Arena gate**:
  - Without ticket: message shown (`"Sorry, but you must have a ticket to compete in these games."`).
  - With ticket: immediate combat starts.
  - Post-fight: returned to same gate area.
  - Retrigger rule: step off and back onto trigger tile.
  - Ticket is consumed per fight.
  - Script chain in this location uses item-check style ops (`0x24` / `0x25`) around gate logic, consistent with ticket gating.

- **Olympic Trial**:
  - Current observed behavior: text only (`"Hurl the spear..."`), no immediate combat/transition.

## Global Ticket/Key Model (User-Verified)

- There are **4 ticket items** in `items.dat` (color-coded):
  - green
  - yellow
  - red
  - black
- There are **4 key items** (town-purchased), used to free bishops in castles.
- Arena world model:
  - Arenas exist in **Middlegate**, **Atlantium**, **Sandsobar**.
  - **Tundra** has no tickets.
  - **Volcania** uses the red ticket line.

### Reverse-Engineering Implications

- `OP_24` is strongly indicated as a **gold check** opcode (not item-id):
  - observed args include `10`, `25`, `50`, `200`, `500`, matching common payment gates.
- `OP_25` is a non-gold 16-bit predicate path (small observed codes `0/1/2` so far).
- Arena gate blocks likely follow:
  - predicate (`OP_24`/`OP_25`) -> condition flag
  - conditional skip (`OP_10`/`OP_11`)
  - combat/arena transition call
  - ticket consumption side effect
- Castle bishop gates should provide additional traces for key-specific checks using the same opcode family.

## `OP_24/OP_25` Evidence Snapshot

Extracted `OP_24` arguments across locations:
- `0x000A` (10) — Middlegate flow
- `0x0019` (25) — Atlantium flow
- `0x0032` (50) — Tundra/related flow
- `0x00C8` (200) — Sandsobar/related flow
- `0x01F4` (500) — Atlantium travel/training flow

These values align with payment-style prompts and are unlikely to be item IDs.

Observed `OP_24` contexts (decoded segments):
- `0x000A` (10): Middlegate mapper/travel style prompt flow.
- `0x0019` (25): Atlantium branch in arena/shop event chain.
- `0x0032` (50): Atlantium/Vulcania travel prompt family.
- `0x00C8` (200): training/skill purchase flow.
- `0x01F4` (500): larger payment gate.

Observed `OP_25` contexts:
- decoded values now include `0x0000`, `0x0001`, and `0x0002` (after endian correction).
- remaining `None` hits are in segments where token alignment is still uncertain after
  variable-length skip-token control flow.

Current best model:
- `OP_24`: gold predicate (amount check to condition flag).
- `OP_25`: non-gold code predicate (likely word/flag/class/item-code family).
- `OP_28`: consume-item path; maps cleanly to multiple quest items/keys/discs.

User-verified gate behavior supporting this model:
- bishop key gate: missing key -> blocked with message
- bishop key gate: with key -> allowed and key consumed
- arena tickets: consumed per fight

### Tooling

Use the decoder's predicate extraction mode:

```bash
python tools/decode_event.py --predicates EXTRACTED/event.dat
```

`--predicates` now runs a path-aware, heuristic walk across `OP_10/11/2B` skip-token
branches so predicate checks behind those branches are surfaced for review.

Use consume-item extraction mode:

```bash
python tools/decode_event.py --consumed EXTRACTED/event.dat --items items.dat --answers druids meenu keys
```

This reports:
- `OP_28` consume hits with mapped item IDs/names
- per-location context strings
- string hits for candidate word answers (`druids`, `meenu`, `keys`)

This prints every parsed `OP_24`/`OP_25` hit with:
- location + segment id
- decoded argument value (if recoverable)
- nearby opcode context
- nearby strings for gameplay correlation

## Tooling

`tools/decode_event.py` now includes:
- opcode map for `0x00..0x32`
- raw script preview
- marker-based disassembly pass starting at opcode `0x22` candidates
