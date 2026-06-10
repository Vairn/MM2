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
| `0B` | `0x15DB0` | 2 | open service/title window (`u8` string via `0x15756`, `u8` position); **only VM opcode that blits a `.anm` signboard** — see [45-event-graphics-opcodes.md](45-event-graphics-opcodes.md) |
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

## Text Display Opcodes (`OP_01`–`OP_06`)

All six read a `u8` string index via `0x15884` → `0x155BE`, copy the resolved
string into the draw buffer at `A4-$5D3C` via `0x158C8` (which maps `@` / `0x40`
to newline `0x0A` for the renderer), then paint. Font: 8×8 bitmap **font 8**
(`EXTRACTED/fonts/mm2/8`); atlas export `editor/assets/fonts/mm2_8.png` /
`mm2_8.json` via `tools/export_mm2_bitmap_font.py`.

| Op | Handler | Typical use (from `event.dat` usage) |
|----|---------|--------------------------------------|
| `01` | `0x15924` | Basic one-shot message; sets exit-flag bit 0 (`A4-$7950`); draws via `-$7EC0` |
| `02` | `0x15942` | Multi-line text block; sets exit-flag bit 1; positions with `-$7F62`/`-$7BFC`, draws glyphs via `-$7C62` until `0x0A` or `0xFF` |
| `03` | `0x159CE` | Prep (`-$7F5C`, `-$7ED8`) then delegates to `OP_02` with window height const `0x11` |
| `04` | `0x159F4` | **Door label** — centered caption above a door tile; single `-$7BE4` text draw after width centering |
| `05` | `0x15A46` | **Popup A** — large plain overlay (see below) |
| `06` | `0x15AEE` | **Popup B** — outdoor signpost (font-glyph frame + post; pen `A4-$7A50`) |

### Popup A vs Popup B (`OP_05` / `OP_06`)

Both handlers **skip all drawing** when `A4-$79E1` (can't-see / darkness overlay)
is non-zero — same gate as `OP_04`.

**`OP_05` @ `0x15A46` — plain overlay (Popup A)**

- Allocates a text window via `-$7C74(4, 3, 24, 13)` → `(x, y, w, h)` in
  character cells (large, borderless).
- One `-$7BE4` text draw of the full string into that window.
- Vertical position is adjusted by party facing: calls `0x1582e` (counts
  wrapped text lines from `0x0A` newlines and 14-char rows), halves the count,
  and if the result is `< 5`, shifts the window up by `(4 − lines)` via
  `-$7BFC`.
- **Observed content:** longer narrative plaques — missing-person posters,
  quest hints, arena notices, radiation warnings (~40 uses in locations 0–59).

**`OP_06` @ `0x15AEE` — outdoor signpost (Popup B)**

- **Not** the red console frame used by character UI (`JSR -$809E`, pen `$17`).
- **Preprocesses** the string in place: every `-` (`0x2D`) is rewritten to `{`
  (`0x7B`, full-width horizontal bar glyph) before draw.
- Smaller window `-$7C74(8, 8, 18, 9)`.
- Draws a **signpost** by blitting font control glyphs through `-$7C62` at pen
  **`A4-$7A50`** (gold/yellow border on Amiga; outdoor init @ `0x26814` stores
  pen `$12`): `0x10`/`0x11` top corners, `0x0E` ×11 top bar, `0x14`/`0x15` sides,
  `0x7E` post core, `0x12`/`0x13` bottom corners, `0x0F` ×11 bottom bar (see
  control-glyph table below). Vertical **post** below the board at cols 12..14.
- **Observed content:** compact directional signs and outdoor labels (~110 uses)
  — e.g. B2 **"Archers / Only"** (`06 01`), **"Yellow / Message 2"** (elemental
  planes), `"< Shortcut\nPinehurst"`, `"Dead Zone\nRadiation!"`.

**`OP_04` vs popups in practice:** `OP_04` is the shop/door nameplate fixed
above a door (dominant in town maps — often paired with `OP_0B` service window
and `OP_0E` shop selector). Use **`OP_05`** for long read-once lore/warning
text without a frame; **`OP_06`** for short **outdoor signposts** (yellow/gold
glyph frame + post over the 3D view); **`OP_04`** strictly for door
labels on building entrances. **`OP_0B`** is a separate **service signboard
sprite** over shop/inn/pub tiles (see §`OP_0B` below / doc 28 §2.1) — not the
same as OP_06 outdoor signs.

### MM2 8×8 Text Format Characters

Reference atlas: `editor/assets/fonts/mm2_8.png` (128×64, 16×8 cells, codepoints
`0x00`–`0x7F`). Strings in `event.dat` are **`0xFF`-terminated** on disk.
The copy helper `0x158C8` converts `@` (`0x40`) → `0x0A` for rendering; decoded
tool output may show either form — see [06-event-dat-format.md](06-event-dat-format.md).

**Control glyphs (`0x00`–`0x1F`) — UI chrome** (drawn via `-$7C62`, especially
Popup B border builder @ `0x15AEE`):

| Code | Char | Role |
|------|------|------|
| `0x0A` | (LF) | Full-frame box tile (also newline in text streams) |
| `0x0E` | (SO) | Top horizontal bar |
| `0x0F` | (SI) | Bottom horizontal bar |
| `0x10` | (DLE) | Top-left corner |
| `0x11` | (DC1) | Top-right corner |
| `0x12` | (DC2) | Bottom-left corner |
| `0x13` | (DC3) | Bottom-right corner |
| `0x14` | (DC4) | Left vertical side |
| `0x15` | (NAK) | Right vertical side / interior column pad |

Other control codes in `0x00`–`0x0F` are present in the atlas but are not used
by the Popup B border prologue; they may appear in other UI thunks (`OP_02`
glyph loop, service windows).

**Printable format characters in event strings** (especially `OP_06` signs):

| Code | Char | Visual / behaviour |
|------|------|-------------------|
| `0x2D` | `-` | Thin dash in source; **`OP_06` only** rewrites to `0x7B` before draw |
| `0x7B` | `{` | Full-width horizontal bar (decorative rule) |
| `0x5B` | `[` | Left arrow (same glyph as `<`) |
| `0x3C` | `<` | Left arrow |
| `0x5D` | `]` | Right arrow (same glyph as `>`) |
| `0x3E` | `>` | Right arrow |
| `0x24` | `$` | Diamond / bullet marker |
| `0x7E` | `~` | Solid block (also used as border fill in Popup B) |
| `0x5F` | `_` | Underline bar |
| `0x40` | `@` | Line break (stored in strings; renderer sees `0x0A`) |

## Semantics Confirmed in This Pass

- `0x22` (`0x16A9E`) range-checks current era (`A4-$79B5` low byte) against
  script `[lo, hi]` args and writes boolean to condition flag `A4-$7951`.
- `0x10`/`0x11` branch via token-skip helper `0x157FC`, gated by `A4-$7951`.
- `0x14` clears current tile's visited/event bit in `A4-$54BA` and clears runtime flag byte `A4-$55D6` bit 7.
- `0x29` sets abort flag `A4-$79EA = 1`.
- `0x32` copies a variable byte (via engine thunk `-$7F2C`) into condition flag `A4-$7951`.

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
`SELECTOR_NAMES` map in `tools/decode_event.py`. Per-town tiles, strings,
donation quest, blacksmith specials, and training HP formula:
[`28-town-services.md`](28-town-services.md).

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

## Opcode Verification Status (gap inventory)

Classification: **Verified** = dispatch + core behaviour confirmed from ASM
(and often gameplay); **Partial** = structure/argc known, gameplay semantics
incomplete; **Unknown** = behaviour not traced.

| Op | Status | Gap summary |
|----|--------|-------------|
| `00` | Verified | Invalid opcode → abort |
| `01` | Partial | Basic text draw (`-$7EC0`); when used vs `02`/`03` in scripts unclear |
| `02` | Partial | Multi-line glyph loop via `-$7C62`; window mode const `0x13` |
| `03` | Partial | Engine prep then `OP_02`; exact prep thunks unmapped |
| `04` | Verified | Door label above door; skips if can't-see |
| `05` | Verified | Popup A — plain overlay, facing-adjusted |
| `06` | Verified | Popup B — framed sign, `-`→`{` preprocess |
| `07` | Verified | Wait for SPACE |
| `08` | Partial | Wait for key; sets cursor mode `0xFD` |
| `09` | Verified | Y/N prompt → `cond_flag` |
| `0A` | Partial | Y/N variant (mode `0xFD` wrapper) |
| `0B` | Partial | Service/title window (`-$7FBC`/`-$7FC2`); arg2 position byte semantics |
| `0C` | Partial | Map transition (dest screen + packed entry coord); bit6 remap path |
| `0D` | Unknown | `u8` index → generic thunk `-$7E42`; per-index effects untraced |
| `0E` | Partial | Selectors `1`–`8` = town services; `0x64`/`0xC9`–`0xCF`/`0xFD` specials; default range bins `0x09`–`0xFB` → category table only |
| `0F` | Verified | End script / cleanup `0x171AC` |
| `10`/`11` | Partial | Conditional token skip verified; static disasm needs full `-$6CC8` length table |
| `12` | Partial | 10× monster slot bytes + 2 tail bytes → combat; tail meaning unknown |
| `13` | Partial | Same as `12` with internal flag=1 (10-byte block only) |
| `14` | Verified | Clear tile event / visited bit7 |
| `15` | Partial | Apply byte to N party members via `0x163CA`; op/val/mask semantics |
| `16` | Partial | Scan ≤6 roster slots; match field `+0x3A` or `+0x28` |
| `17` | Partial | Load script variable group/index → cond |
| `18` | Partial | Masked party apply (`set`/`and`/`or` bytes) |
| `19` | Partial | Add entity to party (`+0x3A/+0x40/+0x46`) |
| `1A` | Partial | Store `u8` into script variable |
| `1B` | Partial | Clear cond unless `cond >= threshold` |
| `1C` | Partial | Engine query `-$7BB4(1, u8)` → cond |
| `1D` | Partial | Engine call table `-$7E84` indexed by `u8×7+1` |
| `1E` | Partial | Timed delay loop (`-$7BC0`/`-$7BD2`) |
| `1F`/`20` | Partial | Party effect dispatcher `0x167B0`; 6-byte payload; mode flag on `20` |
| `21` | Partial | Write `(y,x)` tile bytes into `-$55BA` / visited `-$54BA` |
| `22` | Verified | Era range gate (`-$79B5`) |
| `23` | Partial | Day-of-year gate via `-$79DE[era]`; special cases at args `0xB5`/`0xB6` |
| `24` | Partial | Gold predicate via `0x155DA` + `-$7E66`; amounts 10/25/50/200/500 observed; exact compare op untested |
| `25` | Partial | Non-gold `u16` code via `-$7E66`; decoded values `0`/`1`/`2` only |
| `26`/`27` | Partial | Party-member select; `27` uses alternate input path when arg mode=1 |
| `28` | Partial | Consume item if present → cond; keys/tickets user-verified |
| `29` | Verified | Force script abort |
| `2A` | Partial | 14-byte treasure/reward block (gold/exp, gems, 3× item triplets) |
| `2B` | Partial | Token skip only when `A4-$77BD` set (combat-related flag) |
| `2C` | Partial | Add to global counter `-$79B8`; sets redraw exit flag |
| `2D` | Partial | Check member attribute fields `+0x0C/+0x0E/+0x0F` |
| `2E` | Partial | OR attribute bits onto matching class field `+0x51` |
| `2F` | Verified | Clear input buffer `-$5C50` |
| `30` | Verified | 10-byte encoded password check |
| `31` | Partial | Iterate party members matching mask; per-member engine call |
| `32` | Verified | Load cond from script variable id |

**Overlay locations 60–70** (see `event_location_inventory.json`): most are
`string_bank` records (no tile scripts) referenced by index from other code;
`63`/`65`/`68` are `castle_blob` (no `00 00 00` terminator); `67` is a
`mixed_pool` with non-standard opcodes (`0x41`+) that are **outside** the
`0x00`–`0x32` dispatcher — runtime trigger path for these banks is still
partially traced (queued dispatch @ `0x176B6` vs standard scanner).

## Gameplay confirmation quiz (pick A/B/C/D or correct us)

Answer key for the RE team — fill in **Your answer:** as you play-test. Options
are mutually exclusive unless noted.

---

### 1. Text placement opcodes (`OP_04` / `OP_05` / `OP_06`)

**Scenario:** You step on an outdoor arrow sign vs a building nameplate vs a long quest plaque.

- **A.** `OP_06` = framed signposts with arrows/rules; `OP_05` = long borderless lore/warnings; `OP_04` = door/shop labels only (never dungeon wall fluff).
- **B.** `OP_05` and `OP_06` are interchangeable; only string length differs.
- **C.** `OP_04` is used for both doors and outdoor signs; `OP_06` is dungeons only.
- **D.** `OP_05` is the default signpost; you have seen it with arrow glyphs where we expect `OP_06`.

**RE guess:** A — `event.dat` usage (~110 `OP_06` sign strings vs ~40 `OP_05` plaques; `OP_04` clusters on town service tiles).

**Your answer:** ___

---

### 2. Basic vs block text (`OP_01` / `OP_02` / `OP_03`)

**Scenario:** Nordon/Nordonna intro dialogue vs a one-line “Travel (y/n)?” prompt vs a multi-paragraph NPC speech box.

- **A.** `OP_01` = single-line / simple prompts; `OP_02` = multi-line scroll block; `OP_03` = same block as `OP_02` after extra window prep.
- **B.** Nordon intro uses `OP_01`; all y/n gates use `OP_02`.
- **C.** All three look identical on screen — opcode choice is arbitrary.
- **D.** `OP_03` is the only opcode that can show more than one paragraph.

**RE guess:** A — loc 60 Nordon strings are referenced as blocks; town y/n tiles commonly decompile to `OP_01` + `prompt_yes_no`.

**Your answer:** ___

---

### 3. Engine thunks (`OP_0D`)

**Scenario:** A tile runs script bytes but never opens a shop window (`OP_0E`).

- **A.** Never — `OP_0D` is always followed by visible text you remember.
- **B.** Yes — e.g. cruise/arena return: `engine_call(0x09)` then `map_transition`, with no shop selector.
- **C.** Yes — chest/treasure UI opens directly from `OP_0D` alone.
- **D.** `OP_0D` only appears in overlay banks 60–70, not walkable maps.

**RE guess:** B — multiple overworld scripts call `engine_call(0x09)` paired with `map_transition` (e.g. cruise ride loc 14, Corak's Cave door loc 22); per-index table still **Unknown**.

**Your answer:** ___

---

### 4. Service title window position (`OP_0B` arg2)

**Scenario:** Same inn/blacksmith tile, different party facing or roster size.

- **A.** Title box always appears in the same screen slot; arg2 is a mode/id, not facing math.
- **B.** Box shifts vertically based on party facing (like `OP_05` popups).
- **C.** Box shifts when you have more than four live members.
- **D.** You have never noticed the service title box at all — only the shop UI.

**RE guess:** A — arg2 is passed to `-$7FBC` as a mode byte; no facing hook traced in the `OP_0B` handler (**Partial**).

**Your answer:** ___

---

### 5. Unknown `OP_0E` selectors (default range bins)

**Scenario:** Arena entrance, Monster Bowl, guild enrollment beyond mage guild (`0x0D`).

- **A.** Arena combat is **not** `OP_0E` — entirely separate combat trigger; selectors `0x09`–`0x10` are misc shops only.
- **B.** Town arena tiles call unnamed selectors (e.g. Middlegate `0x10`, Sandsobar `0x49`) that enter combat/ticket flow inside the engine thunk.
- **C.** Every selector `0x09`–`0xFB` opens the same generic shop UI with different inventory tables only.
- **D.** Arena entry uses selector `0x64` (portal travel).

**RE guess:** B — arena entrance tiles decompile to `exec_selector(0x10)` / `0x49` / etc., not `OP_12` in the visible script (**Partial**).

**Your answer:** ___

---

### 6. Special `OP_0E` selectors (`0x64`, `0xC9`–`0xCF`, `0xE2`, `0xFD`)

**Scenario:** Mystic portal, quest milestones, endgame.

- **A.** `0x64` = long-distance portal travel; `0xC9`–`0xCF` = quest-specific handlers; `0xFD` = late-game / unused in normal play.
- **B.** All of these are just renamed blacksmith shops.
- **C.** `0xE2` = arena only; `0x64` = boat ferry.
- **D.** You fire `0xFD` regularly in early game.

**RE guess:** A — names in `SELECTOR_NAMES` + portal tiles using high selectors; `0xFD` seen rarely (**Partial**).

**Your answer:** ___

---

### 7. Encounter tail bytes (`OP_12` / `OP_13`)

**Scenario:** Same monster IDs, different fight feel (arena backdrop, boss tone, no escape).

- **A.** Tail byte 1 = terrain/backdrop id; tail byte 2 = flags (escape, arena, difficulty band).
- **B.** Both bytes are fixed `0x00` in all real fights — purely padding.
- **C.** Tail bytes set party level / scaled HP only.
- **D.** Tail byte 1 = gold reward multiplier; byte 2 unused.

**RE guess:** A — scripts emit varied non-zero tails (`06 02`, `11 5C`, `05 F0`, …) with stable 10-byte monster blocks (**Partial**, tail table not dumped).

**Your answer:** ___

---

### 8. Party byte apply (`OP_15` / masked `OP_18`)

**Scenario:** Olympic Trial skill purchase, hire restrictions, quest progress bits.

- **A.** Sets per-member **flag/field bytes** (skills owned, class gates, quest bits) — not direct HP damage.
- **B.** Always deducts gold when `OP_15` runs (same opcode as payment).
- **C.** Only used for alignment/reputation changes you see in a stat panel.
- **D.** You have never noticed any effect from these ops in play.

**RE guess:** A — Atlantium Olympic Trial uses `apply_party` / `apply_party_masked` on field `0x6D` before `party_effect` skill grant (**Partial**).

**Your answer:** ___

---

### 9. Party effects (`OP_1F` / `OP_20`)

**Scenario:** Fountain heals, temple cure, trainer spell/skill grant, stat shrine.

- **A.** Selector byte chooses effect class (heal, cure, teach skill/spell, buff); `OP_20` is a variant mode of the same dispatcher.
- **B.** Only restores HP — poison/cure uses a different opcode family entirely.
- **C.** Purely cosmetic — no stat or HP change.
- **D.** Always grants a random item.

**RE guess:** A — `party_effect(sel=0x09, …)` follows skill prompts; `party_effect(sel=0x80, …)` follows shrine/fountain chains (**Partial**).

**Your answer:** ___

---

### 10. Map tile mutation (`OP_21`)

**Scenario:** Barrier drops, door opens, wall becomes floor — still there after leaving the tile.

- **A.** Yes — e.g. Corak's Cave barriers lowered with visible map change (`set_tile` on adjacent cells).
- **B.** Only clears the event bit (`OP_14`); graphics never change.
- **C.** Visual change is session-only — resets on town exit.
- **D.** You recall permanent changes but they were cut-scene transitions, not tile writes.

**RE guess:** A — loc 22 Hero's Tomb script writes collision/visual bytes via `OP_21` then `OP_14` (**Partial**).

**Your answer:** ___

---

### 11. Non-gold predicate codes (`OP_25` values `0`, `1`, `2`)

**Scenario:** Our decoder labels `flag_clear` / `flag_set` / `flag_alt`.

- **A.** Generic script/state flags (quest done, already paid, etc.) — **not** item IDs.
- **B.** Directly encode ticket colors (0=green … 2=black).
- **C.** Bishop freed / arena won / goblet returned map 1:1 to codes 0/1/2.
- **D.** You have never hit a gate you would attribute to `OP_25` — likely wrong decode.

**RE guess:** A — `--predicates` finds **zero** recoverable `OP_25` hits in `event.dat`; small integer codes fit flag words, not items (**Partial**, high uncertainty).

**Your answer:** ___

---

### 12. Tickets/keys: check vs consume (`OP_25` vs `OP_28`)

**Scenario:** Arena ticket turn-in vs bishop cage key.

- **A.** Tickets: `OP_25` check then separate `OP_28` consume in the same script step.
- **B.** Tickets: handled inside `OP_0E` arena selectors (engine inventory logic); keys: explicit `OP_28` in castle scripts.
- **C.** Both tickets and keys use only `OP_25`; `OP_28` is for quest discs/rings.
- **D.** Keys check without consume — you keep the key after freeing a bishop.

**RE guess:** B — `OP_28` consume hits map to keys/discs/rings; no ticket item IDs (208–211) appear in `OP_28` contexts; arena tiles call `exec_selector` instead (**Partial**).

**Your answer:** ___

---

### 13. Gold predicate (`OP_24`)

**Scenario:** “Travel for 25 gold (y/n)?” — you accept and have enough gold.

- **A.** `OP_24` is **check only** (gold ≥ amount); a later op/engine call deducts if you proceed.
- **B.** `OP_24` both checks and subtracts gold immediately when true.
- **C.** `OP_24` checks individual member gold, not party total.
- **D.** Failed check still deducts partial gold.

**RE guess:** A — scripts pair `check_gold_at_least(N)` with `if not cond: skip` then `map_transition` / `party_effect`; no subtract in the `OP_24` handler (**Partial**).

**Your answer:** ___

---

### 14. Combat-gated token skip (`OP_2B`)

**Scenario:** When does the skip walk actually run? (`A4-$77BD` must be set.)

- **A.** Primarily after combat victory / arena return cleanup (flag set @ `0x12438` and related combat paths).
- **B.** Any time you skip a dialog with ESC.
- **C.** Whenever a y/n prompt appears.
- **D.** You have noticed `OP_2B` branches firing outside combat — e.g. shops, travel.

**RE guess:** A — `$77BD` is set on victory (`combat_victory_rewards`) and cleared on many map/event exits; `OP_2B` no-ops when clear (**Partial**).

**Your answer:** ___

---

### 15. Calendar / day gate (`OP_23`)

**Scenario:** NPCs or tiles that only work on certain days or moon phases.

- **A.** Yes — obvious day-locked quests you can name (matches `OP_23` / day-of-year table).
- **B.** Only subtle — benefits “once per moon phase” feel calendar-driven but may use vars, not `OP_23`.
- **C.** Era gates (`OP_22`) cover all time travel; `OP_23` never matters in normal play.
- **D.** Unsure — never noticed any day-specific behavior.

**RE guess:** B — moon-phase shrine text uses `load_var8` + `store_var8`, not `OP_23`; `OP_23` reads `-$79DE[era]` (**Partial**).

**Your answer:** ___

---

### 16. Location 60 — Nordon intro bank

**Scenario:** Corak prologue / Nordon / Nordonna dialogue.

- **A.** Shown only during the opening castle/tutorial sequence — not re-triggerable from a town tile later.
- **B.** Re-accessible anytime by returning to Middlegate castle tiles.
- **C.** Random encounter text anywhere in the world.
- **D.** Visible only in the Hall of Spells UI.

**RE guess:** A — loc 60 is a `string_bank`; most event segments are missing/trigger-only placeholders, not normal map scripts (**Partial**).

**Your answer:** ___

---

### 17. Location 61 — spell / hireling tables

**Scenario:** `%` / letter garbage vs normal prose.

- **A.** Never seen in gameplay — metadata for shop/HoS/hire UI only.
- **B.** Shows as garbled text if you walk the wrong tile in a town.
- **C.** Appears during spell purchase windows as formatting codes.
- **D.** Same strings as location 70, duplicated for testing.

**RE guess:** A — loc 61 decodes as encoded tables; no readable quest prose in the string list (**Partial**).

**Your answer:** ___

---

### 18. Location 67 — Hall of Spells mixed pool

**Scenario:** Spell purchase / class check scripts vs dead data.

- **A.** Dead — superseded entirely by loc 70 strings.
- **B.** Still executed via queued dispatch (`0x176B6`) for HoS/class flows; contains opcodes outside `0x00`–`0x32`.
- **C.** Only runs during endgame (`OP_22` era gate).
- **D.** You have bought spells from a tile whose script clearly lives in loc 67.

**RE guess:** B — loc 67 has runnable segments (`prompt_yes_no` + `map_transition`) plus non-standard opcodes; overlay path differs from tile scanner (**Partial**).

**Your answer:** ___

---

### 19. Location 70 — meta bank (bishops, puzzles, HoS)

**Scenario:** Bishop cage text, “Yellow Interleave” puzzle, Triple Crown hints.

- **A.** Pulled during castle/bishop events; all four bishops use the same key-check/consume pattern (color-matched key).
- **B.** Bishops use tickets; keys are Sandsobar-only.
- **C.** Puzzle text is shown as a normal `OP_06` sign in Pinehurst overworld only.
- **D.** Loc 70 is never read at runtime — editor artifact.

**RE guess:** A — loc 70 strings include Green Bishop + Yellow Interleave; castle overlays consume Yellow/Red/Black keys via `OP_28` matching bishop color (**Partial**).

**Your answer:** ___

---

### 20. Castle blobs (locations 63 / 65 / 68)

**Scenario:** Runtime behavior vs opaque static data.

- **A.** Normal tile events fire from these records today (bishop cages, sieges).
- **B.** Partial — engine reads blob bytes directly; not via standard triplet scanner (`00 00 00` missing).
- **C.** Completely unused on Amiga retail — dead padding.
- **D.** Only used if you hack `event.dat` — vanilla never touches them.

**RE guess:** B — flagged `castle_blob` in inventory; queued dispatch likely consumes bytes without normal init (**Partial**).

**Your answer:** ___

---

### 21. Olympic Trial (Atlantium, event @ “Hurl the spear…”)

**Scenario:** Text-only today — ever combat later?

- **A.** Permanently non-combat — pays 500 gp to teach **Athlete** skill via `apply_party` / `party_effect`.
- **B.** Starts combat if you refuse the y/n prompt.
- **C.** Becomes a combat arena after a quest flag or era change.
- **D.** Was combat in an earlier retail build — patch changed it.

**RE guess:** A — loc 01 event 27 decompiles to skill purchase, not `encounter_setup`; user already reported text-only (**Partial**).

**Your answer:** ___

---

### 22. Arena ticket color per town

**Scenario:** Which colored ticket item gates each town arena? (`items.dat`: Green 208, Yellow 209, Red 210, Black 211 — note typo “Yellow Tickt”.)

- **A.** Middlegate=Green, Atlantium=Yellow, Sandsobar=Black, Vulcania=Red.
- **B.** Middlegate=Yellow, Atlantium=Green, Sandsobar=Red, Vulcania=Black.
- **C.** All four towns accept any ticket color.
- **D.** Tundara sells tickets but has no arena — ignore for pairing.

**RE guess:** A — Vulcania=Red is user-verified; four ticket items match four arena towns; Sandsobar Triple Crown text highlights **Black** ticket for cross-arena quest (**Partial** — exact shop→color mapping not in decoded scripts).

**Your answer:** ___

---

*After you fill this in, please note any wrong **RE guess** lines inline — that is the highest-value feedback for the opcode gap table.*

**Interactive quiz:** `python tools/mm2_event_quiz.py` (saves answers to `EXTRACTED/event_quiz_answers.json`).

## Tooling

`tools/decode_event.py` now includes:
- opcode map for `0x00..0x32`
- raw script preview
- marker-based disassembly pass starting at opcode `0x22` candidates
