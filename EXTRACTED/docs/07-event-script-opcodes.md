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
| `0B` | `0x15DB0` | 2 | service signboard sprite: `u8` **sign index** (mapped to a sign `.anm` id by `0x15756`, **not** a string-table index), `u8` placement; **only VM opcode that blits a `.anm` signboard** — see [45-event-graphics-opcodes.md](45-event-graphics-opcodes.md) |
| `0C` | `0x15E12` | 2 | map transition to `(u8 dest, u8)` |
| `0D` | `0x15EC4` | 1 | `u8` index 0..9 -> thunk `-$7E42` -> `0x06FB8`: play canned on-screen sequence (table `-$7232[idx]`, glyph table `-$7252`, render `0x77AA`); **no game-state side effects** |
| `0E` | `0x160C2` | 1 | selector dispatch (see selector table below) |
| `0F` | `0x162A6` | 0 | **end/return script** (sets stop flag, calls cleanup `0x171AC`) |
| `10` | `0x162B8` | var | IF(cond) skip N token-stream entries |
| `11` | `0x162DC` | var | IF(!cond) skip N token-stream entries |
| `12` | `0x16300` | **12** | fixed encounter: 10 monster-type ids → `A4-$11DE[0..9]`; mode `A4-$796B`=`0x80`; tail1 → `A4-$11D4` (overflow/breed type), tail2 → `A4-$77BE` (live count); then `jsr -$7EDE` combat + abort. See §"OP_12/OP_13 encounter setup" |
| `13` | `0x16386` | **10** | seeded-random encounter: calls `0x16300` with flag=1 → mode `A4-$796B`=`0`, no tail bytes (clears `-$11D4`/`-$77BE`); random picker `0x1213E` augments the 10 seeded slots |
| `14` | `0x16398` | 0 | clear current tile event high-bit / clear runtime event flag |
| `15` | `0x16426` | **3** | apply state to party members (`u8 count, u8, u8`; 4th byte only when flag=1) |
| `16` | `0x16520` | 2 | scan party members for item arg2: reads 2 bytes (arg1 discarded, only arg2 used); for each member (bound `A4-$795A`, ptr via `0x477E`) `cond += Σ_{m=0..5}(rec[0x3A+m]==arg2 ‖ rec[0x28+m]==arg2)`, break at first matching member. `+0x28`/`+0x3A` = equipped/backpack item-id runs |
| `17` | `0x165A4` | 2 | load condition byte from resolved variable pointer |
| `18` | `0x165C6` | **4** | `OP_15` handler with flag=1 (`u8 count, set, and, or`) -> masked party apply |
| `19` | `0x165D8` | 4 | **give item to party**: `arg1, id, charges, flags`. cond=0 on entry; if `arg1>=0x80` id = cond_flag (pre-clear). Place in first member's first empty backpack id slot: `id→rec[0x3A+m]`, `charges→rec[0x40+m]`, `flags→rec[0x46+m]` (SoA), cond=1. All-full → found-item buffer `A4-$3F1C`/`A4-$794C` |
| `1A` | `0x166F8` | 2 | store `u8` into resolved variable pointer |
| `1B` | `0x16724` | 1 | clear cond unless `cond >= u8` (threshold gate) |
| `1C` | `0x16742` | 1 | `cond_flag = rng_roll(1,u8)` via `-$7BB4`→`0x22BC6` (raw roll byte, not boolean) |
| `1D` | `0x16762` | 1 | `(*A4-$7E84)(u8*7+1)` — engine dispatch via runtime A4 pointer; no VM-level state change |
| `1E` | `0x16780` | 1 | busy-wait `u8` iterations: each `(*A4-$7BC0)(10)` delay + `(*A4-$7BD2)` input poll, break on keypress. Presentation/timing only |
| `1F` | `0x1690E` | 6 | apply effect to party members (selector + values via `0x167B0`) |
| `20` | `0x16A22` | **6** | party-effect variant (calls `0x1690E` with mode=1) |
| `21` | `0x16A34` | 3 | set tile data at `(y,x)` (arg1 nibbles) into arrays `-$55BA`/`-$54BA` |
| `22` | `0x16A9E` | 2 | set condition if **current era** (`-$79B5`, low byte of era word) is in `[lo, hi]` — era/time-range gate |
| `23` | `0x16ADA` | 2 | day-of-year gate. day = low byte of `-$79DE[era]`; **arg1=`0xB5`** → cond=(day odd), **arg1=`0xB6`** → cond=(day even), else cond=(`arg1<=day<=arg2`). See §OP_23 |
| `24` | `0x16B54` | 2 | party gold pool-pay (`-$7E6C`→`0x6ACE`): sum `+$66`, deduct amount, pool remainder → cond |
| `25` | `0x16B82` | 2 | party gems pool-pay (`-$7E66`→`0x6B9A`): sum `+$5C`, deduct amount, pool remainder → cond (`hi,lo`) |
| `26` | `0x16BC0` | 0 | prompt to select a party member (ESC `0x1B` ends script) |
| `27` | `0x16BC0` | 0 | select-party-member variant (input mode 1) |
| `28` | `0x16C86` | 2 | consume item id (arg1) from party inventory -> condition flag |
| `29` | `0x16D08` | 0 | force abort/exit flag |
| `2A` | `0x16D16` | 14 | fill found-item/treasure buffer: `u24 gold/exp→A4-$3F10, u16 gems→A4-$3F12, 3×(id→A4-$3F1C, charges→A4-$3F16, flags→A4-$3F19)`, sentinel `A4-$794C=$FF` (does NOT distribute; see §Found-item buffer) |
| `2B` | `0x16D74` | var | `u8` + token-skip walk |
| `2C` | `0x16D98` | 1 | add `u8` to state var `-$79B8`, flag redraw |
| `2D` | `0x16DBA` | 2 | match party attribute vs nibble -> cond. arg1: bit7=race(`+0xE`)/bit6=sex(`+0xC`)/else class(`+0xF`); bit5=any-mode(else all); low nibble=val1; if `arg1&0xE0==0`, `arg2&0xF`=val2 |
| `2E` | `0x16F50` | 2 | OR arg2 into member `+(uint8)(arg1-0x6E)+0x51` for classes {4,2} (or {3,1} if arg1>=0x80, arg1&=0x7F) |
| `2F` | `0x16FEA` | 0 | clear/space-fill input buffer `-$5C50` |
| `30` | `0x17034` | **10** | **answer/password check**: 10 bytes vs player input; `char = 0x11A - byte` (`0xFA`=space) |
| `31` | `0x170BC` | 3 | sets `EXIT_FLAGS` bit1; reads member-spec byte + u16 value (`arg1>=0x80` → value from cond_flag); per resolved member calls engine op `(*A4-$7F08)(rec, value, &out×3)`, then `(*A4-$7F14)` → sets script-abort if nonzero |
| `32` | `0x17190` | 1 | cond = party class-nibble count (`-$7F2C`→`0x04614`/`0x45C4`; record+0x50 nibbles == id, over living members) |

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

**Table now extracted byte-exact (2026-07)** from the data hunk (`opcode_len_tbl`
@ `A4-$6CC8`, file offset `0x1336`, 51 big-endian words) via
`tools/dump_event_token_table.py` → `EXTRACTED/event_token_len_table.json`.
It is **not** simply `1 + argc` for every opcode — 2 genuine ROM discrepancies:

- **`OP_00` (invalid):** ROM skip delta is **`0`**, not `1`. Only matters if a
  corrupt/pathological script skip-walks over a literal `0x00` byte (never
  legitimately emitted).
- **`OP_25` (check-code16):** the handler reads **2** argument bytes when
  *executed* (own length 3 = `event_op25_code_check` @ `0x16B82`, two `jsr
  $155BE` byte reads combined into a `u16`), but the ROM's skip-table entry
  is only **2** (opcode + 1 byte) — a genuine off-by-one in the original
  game's static length table. A skip walk (`OP_10`/`OP_11`/`OP_2B`) that
  passes **over** an `OP_25` token (rather than executing it) under-counts
  by one byte and permanently desyncs the parse cursor by 1 for the rest of
  that script. Confirmed **not reachable** in the shipped `event.dat` (no
  location's `OP_10`/`OP_11`/`OP_2B` skip range ever contains an `OP_25`
  token — verified by scanning every location/segment with the node-based
  decoder), so this is a byte-exactness fix with no observable gameplay
  effect today, kept for 100% ASM fidelity per CLAUDE.md.

**Port:** `eventVmTokenDelta()` (`game/include/mm2/events/EventVmHelpers.h` /
`.cpp`) is the byte-exact table, consumed by `EventRuntime::skipTokens` (via
`tokenDelta()`); the maze-walker mirrors it as `OP_TOKEN_DELTA` in
`wiki/maze-walker/eventVm.js` (currently unused there — the walker's skip
step operates on already-decoded opcode **nodes**, not raw bytes, so it is
naturally opcode-aligned and cannot reproduce the `OP_25` byte-level quirk;
`tools/decode_event.py`'s `ROM_SKIP_LEN` dict + `_token_skip_len()` already
modeled this correctly for static disassembly/branch-target recovery).

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

`OP_0E` reads one selector byte and runs a chained-subtraction switch
(jump body at `0x161fa`, after setting abort flag `-$79EA=1` and `-$7946=2`).

**Exact explicit-case dispatch (from `0x160C2`)** — each case `jsr`s a handler
then `bra $162a2` (return). `-$7Dxx`/`-$7Cxx` targets are **runtime A4 vtable
thunks** (the shop/temple/training/guild engine, unported); `$1xxxx` targets are
static pc-relative calls (statically resolvable):

| selector | handler | note |
|----------|---------|------|
| `1` (`0x01`) | sets `-$7946=1`; `jsr 0x1a132` | inn / rest |
| `2` (`0x02`) | `jsr -$7CD4` | training hall |
| `3` (`0x03`) | `jsr 0x1d208` | (static) tavern / food |
| `4` (`0x04`) | `jsr -$7D16` | temple / healer |
| `5` (`0x05`) | `jsr -$7D10` | mage guild |
| `6` (`0x06`) | `jsr 0x1c54a` | (static) blacksmith |
| `7` (`0x07`) | `jsr -$7DB8` | general store |
| `8` (`0x08`) | `jsr -$7DBE` | arena / special shop |
| `100` (`0x64`) | `jsr -$7D9A` | portal/travel |
| `126` (`0x7E`) | `jsr -$7DB2` | special |
| `127` (`0x7F`) | `jsr -$7DAC` | special |
| `128` (`0x80`) | `jsr -$7DA6` | special |
| `129` (`0x81`) | `jsr -$7DA0(0)` | special (arg 0) |
| `130` (`0x82`) | `jsr -$7DA0(1)` | special (arg 1) |
| `131` (`0x83`) | `jsr -$7DA0(2)` | special (arg 2) |
| `201` (`0xC9`) | `jsr 0x19ab4` | (static) quest handler |
| `202` (`0xCA`) | `jsr 0x19ac4` | (static) quest handler |
| `203` (`0xCB`) | `jsr 0x1445a` | (static) |
| `204` (`0xCC`) | `jsr 0x1456e` | (static) |
| `205` (`0xCD`) | `jsr 0x145e2` | (static) |
| `206` (`0xCE`) | `jsr 0x146e4` | (static) |
| `207` (`0xCF`) | `jsr 0x1480a` | (static) |
| `226` (`0xE2`) | `jsr 0x18204` | (static) |
| `253` (`0xFD`) | `jsr 0x1493c`; then if `-$79EA==2` copy `-$79AC`→`-$79F2`, `-$7946=1`, `jsr 0x1a1f8`; elif `==3` `jsr -$7ED2`; else `-$79EA=1` | endgame/transition |
| *default* | `jsr 0x15EDC` (selector→category binner, table below) | shops/temple/training engine |

The static `$1xxxx` handlers are tractable to port; the `-$7Dxx` vtable thunks are
the town shops/temple/training/guild engine (doc 28), **not yet ported**.

**Port status (`game/src/events/EventTownServices.cpp`, audited & de-fabricated).**
The earlier MVP placeholder prose + auto heal/train/rest/gold-deduction for
selectors 1–8 were fabrications (the engine never auto-applies a whole-party
transaction without the player's menu selection) and have been **removed**. Each
selector now presents the **real on-screen entry text transcribed byte-for-byte
from `str.dat`** (`11-str-decoded.txt`) and either opens the ported menu (when
`EventRuntime::bindTownServiceUi` is set) or **defers** the interactive engine:

| selector | faithful entry now shown | str.dat lines | port / deferred engine (handler) |
|----------|--------------------------|---------------|----------------------------------|
| `0x01` inn | OP_0B sign title + SPACE | (no NPC intro) | rest/dismiss (`0x1A132`); partial inn menu in walker |
| `0x02` training | OP_0B sign title + SPACE | (no NPC intro) | **PORTED** level-up menu (`-$7CD4`→`0x8050`); cost = `level×index×50` |
| `0x03` tavern | NPC y/n intro → menu on **Y** | 89–108 | **PARTIAL** A–E menu (`0x1D208`); food/drink RNG leaves deferred |
| `0x04` temple | priest y/n intro → menu on **Y** | 343–357 | **PORTED** heal/cond/donate/cleric spells (`-$7D16`); healing = `level×index×10` |
| `0x05` guild | hall y/n intro (no backend) | 328–342 | **PORTED** spell shop when member (`-$7D10`→`0x1E3E6`); gate `0x1E410` |
| `0x06` smith | **direct shop** (no y/n when UI bound) | 255–273 (defer path only) | **PORTED** A–D buy (`0x1C54A`/`0x1BE44`); Sell/Identify deferred |
| `0x07` store | OP_0B sign title + SPACE | (generic shell) | general store `-$7DB8`→`0xA62C` traced, buy loop deferred |
| `0x08` arena | **PORTED** ticket-check/combat engine (`0x9D76`) | "Sorry, but you must have a ticket…" / "The games master accepts…" | **Arena ticket** items `0xD0`–`0xD3`; Middlegate `(2,13)`. **Not** farthings. |
| `0x0A` fountain | NPC y/n intro ("Fanciful Feldecarb Fountain…") | shared string bank (loc 60) str[23] | **Mislabel in older notes** — Feldecarb is selector **`0x0E`**, not `0x0A`. `0x0A` is Nordon goblet. |
| `0x0D` enroll | NPC y/n intro ("A sleepy conjurer yawns…") | shared string bank (loc 60) str[21] | 20gp deduct + roster+$0B write transaction (`-$7DA0` → `0xD89C` → `0x1A1B2`) deferred |

**Intro y/n → menu (2026-07):** pub (`0x03`) and temple (`0x04`) always show the
str.dat greet and wait for y/n before `townSvcRunTavern` / `townSvcRunTemple`
(`EventRuntime::PendingTownMenu` + `eventTownServiceRunBoundMenu`). Blacksmith
(`0x06`) skips the greet and opens the category buy menu immediately when a UI
backend is bound. See doc 28 §1.4.3.

**Correction (2026-07, revised):** `-$7DB8` (`0x07`), `-$7DBE` (`0x08`), and `-$7DFA`
(default-range) are three *different* fixed A4-relative addresses (a direct
`jsr disp(a4)`, not a vtable lookup). A first pass correctly separated them but
then **misidentified which one is the arena engine**, claiming `-$7DFA` (the
default-range dispatch thunk) was `0x9D76`. Byte-reading the A4 vtable
trampoline table directly out of `EXTRACTED/ghidra/mm2_data_00.bin` (file
offset `0x7FFE+disp`) settles it definitively:

```text
-$7DB8 (file off 0x0246): 4EF9 0000A62C  -> 0x0A62C  general store buy loop
-$7DBE (file off 0x0240): 4EF9 00009D76  -> 0x09D76  Arena Games ticket engine
-$7DFA (file off 0x0204): 4EF9 000092F2  -> 0x092F2  event_dat_loader (!)
```

So **explicit selector `0x08` is the arena engine**, not the default-range bin.
The default-range dispatch instead **re-enters the event loader** with a
synthetic index — most likely to load one of event.dat's non-map "string bank"
pseudo-records (e.g. decoder location 60, "Nordon/Nordonna/Corak" — see below)
keyed by the category/index pair; that reinvocation is not reverse-engineered
yet. The practical effect of the earlier mistake: every default-range selector
(Atlantium Olympic trials, post-victory reward tiers, the game-start Corak
monologue, ...) was showing the arena's "you must have a ticket to compete"
text, which is why unrelated quests appeared to demand a ticket. Fixed in both
`game/src/events/EventTownServices.cpp` (ticket engine moved to `case 0x08`,
`default:` no longer fabricates ticket text) and the web walker's
`selectorBin.js`/`eventVm.js`.

Cost formulas are kept as documented helpers `eventVmTrainingCostPerChar` /
`eventVmHealingCostPerChar` (FAQ §3-6, doc 34 §13.2; map→town-index `[1,5,2,4,2]`)
in `EventVmHelpers`, ready for the engine port and asserted in
`event_middlegate_test`. The dead (×10) / eradicated (×100) healing multipliers
are still a **known gap**: roster condition byte `$26` only groups `$80+` =
Dead/Stone/Eradicated (doc 06) and the dead-vs-eradicated threshold is not yet
ASM-confirmed.

**Town-service transaction layer — now ported (byte-exact leaf ops).** The
faithful per-character mutations the temple/training engine performs are
implemented in `game/src/events/TownServiceTransactions.{h,cpp}` and driven by a
**swappable UI backend** `ITownServiceUi` (`TownServiceMenu.{h,cpp}`) +
`PlayTownServiceUi` (`game/src/ui/PlayTownServiceUi.cpp`), so the
LOGIC/costs/state are ASM-canonical while interaction is pluggable (CLAUDE.md).
`eventExecTownSelector` runs temple (`0x04`), training (`0x02`), smith (`0x06`),
guild (`0x05`), and tavern (`0x03`) menus when a UI backend is bound; pub/temple
require y/n on the str.dat intro first (§ above). With no backend it falls back
to intro + deferral (no fabrication). Ported leaves:

| leaf (transaction) | ASM | behavior |
|--------------------|-----|----------|
| `townSvcCharGoldDeduct` | `0x1C9C0` / `0x1D90C` | per-char gold `record+0x66`: deduct iff affordable (`scc`), else no change |
| `townSvcRestoreHp` | `0x1DD48` | if `$5E` (work max) < `$60` (perm max): set `$5E` and `$74` (cur HP) = `$60` |
| `townSvcRestoreCondition` | `0x1D736` | `clr.b $26` |
| `townSvcHeal` | `0x1D716` | gold check → restore HP → clear condition; cost = `level×index×10` (living) |
| `townSvcTrainStat` | `0x1C898` (+ jump tbl `0x1C86C`) | base stat (id→`$6B/$6F/$6D/$6C/$71/$72/$6E`) += `A4-$6720[map]`, write iff `add ≤ rolled < A4-$671A[map]` |
| `townSvcTrain` | `0x1C898`+`0x1C9C0` | deduct `level×index×50`, then stat raise |
| `townSvcTempleDonate` | doc 28 §5.2 | deduct `A4-$6742[map]`, OR `A4-$66B1[map]` into `A4-$799E`; `0x1F` = all five |
| `townSvcSmithBuy` | `0x1BE44` (price `0x1BF16`, gold `0x1BDD6`) | reject if `$26≠0` → first empty backpack `+$3A` → deduct price from `+$66` → write id/charges/flags into `+$3A`/`+$40`/`+$46` |

Per-town constants + the blacksmith static item-id/meta matrices live in
`EXTRACTED/decomp/mm2_town_tables.{h,c}` + `tools/mm2_town_tables.py` (C struct +
codec, per CLAUDE.md). Smith pricing reads the items.dat gold field (record
offset `0x12`) via `mm2_smith_price` (`0x1BF16`: `meta==0 → gold`, else
`gold*2 + (meta-1)*1000`). The blacksmith **buy** menu (Weapons/Specials/Armor/
Misc) now runs through `townSvcRunSmith` when both a UI backend
(`bindTownServiceUi`) and items.dat (`bindItems`) are bound. Map index `0x11`
Poorman's Portal (Middlegate) remains a self-contained fixed-cost transaction
(10 gp → Sandsobar).

**Still engine-deferred (not fabricated), with ASM addresses:**
- Smith **Sell** (`0x1BC26`) / **Identify** (`0x1B6E0`) leaves (presentation-heavy);
  Today's-Specials **date-roll** bonus (`0x1C146`, `A4-$79B6`→`$681C`/`$6816`);
  Merchant-skill half-price discount (`-$7F32` @ `0x1BFB4`, FAQ "cut in half").
- **Tavern** (`0x03`): intro `0x1D208`; food/drink submenus enter via selectors
  `0xC9`/`0xCA` → `0x1980A` through vtable `-$7D40`; cost leaves `0x18EC0`/`0x18F78`
  are **RNG-gated** (`-$7BB4`) and write an encoding to roster `+$78`/`+$7C`
  (`0x019030`) rather than deducting gp; drink stat bonuses are applied by
  unported VM opcode handlers; sick/success roll `0x19D64`/`0x19B28`; rumors
  `0x97FC` walk per-location bytecode `A4-$119A` (display-only).
- **Store** (`0x07`, thunk `-$7DB8` → `0xA62C`) — byte-verified, own fixed
  handler; separate from the arena engine below. The store buy loop `0xA62C`
  uses a hard `record+$66 ≥ 100 gp` gate (`0xA75E`) and an effect jump table
  `0xA3AE` keyed on roster `+$50` nibbles; item pools `A4-$7136`/`-$713C`.

### Arena Games ticket engine (`0x9D76`, explicit selector `0x08`)

**Revised 2026-07** — byte-verified by reading the A4 vtable trampoline table
directly out of `EXTRACTED/ghidra/mm2_data_00.bin` (file offset `0x7FFE+disp`,
doc 49): `-$7DBE` (file off `0x0240`) is `4EF9 00009D76`, i.e. **explicit OP_0E
selector `0x08`** — NOT the default-range dispatch — is the sole path into
this routine. (`-$7DFA`, the default-range thunk, is `4EF9 000092F2` = the
SAME `event_dat_loader` used to enter a normal on-map location — an earlier
pass misread the table and swapped these two, which made the port fabricate
"you must have a ticket to compete" for every unrelated default-range selector
including the game-start Corak monologue; see the correction note above.) This
is **not** a generic shop, and there is no per-category branching inside it —
selector `0x08` runs the exact same code, unconditionally, wherever an
event.dat script encodes it (e.g. Middlegate's arena-entrance tile, event 9 in
`EXTRACTED/docs/events/loc_00_middlegate.md`, decoder-labeled
`open_arena_shop`):

1. Scans every party member's **backpack only** (record `+0x3A..0x3F`, NOT
   equipped slots), member-major then slot-minor (asm `0x9D9C-0x9DDA`), for a
   ticket item `0xD0..0xD3` (Green/Yellow/Red/Black).
2. On miss: shows `"Sorry, but you must have a ticket to compete in these
   games."` (code strings `0xA082`/`0xA0A7`, byte-exact) and ends — no combat.
3. On hit: consumes the ticket (thunk `-$7F26`), shows `"The games master
   accepts your ticket.  Let the battle begin!"` (`0xA0BF`/`0xA0E5`), then
   arms a **fixed encounter** through the *same* battle-slot array (`A4-$11DE`)
   and combat thunk (`-$7EDE`) as OP_12, with
   `monster_type = ((color*3 + area[screen]) * 16) + rng(1,16)`
   (asm `0x9E86-0x9EC2`; `area[5]` = data hunk `0xE74` = `{0,2,0,0,1}` for
   Middlegate/Atlantium/Tundara/Vulcania/Sandsobar).
4. On victory (`A4-$77BD`): grants gold from a 4×3 table (data hunk `0xE7A`,
   ticket color × town tier `min(screen,2)`) to the **first eligible** party
   member and prints `"Winner, you receive N gold"` (`0xA0FC`/`0xA111`):

   | color \\ tier | Middlegate | Atlantium | elsewhere |
   |------|-----------:|----------:|----------:|
   | Green (`0xD0`)  | 200   | 1,500  | 500    |
   | Yellow (`0xD1`) | 2,000 | 5,000  | 3,000  |
   | Red (`0xD2`)    | 7,000 | 15,000 | 10,000 |
   | Black (`0xD3`)  | 20,000| 50,000 | 30,000 |

   The same reward loop also has a **documented ROM bug** that corrupts
   `record+0x79` instead of setting a class-quest flag cleanly — see
   [`36-class-quest-hp-bug.md`](36-class-quest-hp-bug.md).

Port status: `game/src/events/EventVmHelpers.{h,cpp}` implements the
deterministic ticket scan/consume + monster-type/gold-table helpers
(`eventVmFindArenaTicket`, `eventVmConsumeArenaTicket`,
`eventVmArenaMonsterType`, `eventVmArenaGoldReward`), and
`EventTownServices.cpp`'s **`case 0x08`** shows the real deny/accept text and
arms the encounter via `eventRunFixedEncounter` (same as OP_12). The `default:`
case (true default-range selectors) no longer fabricates ticket-check text —
see the correction above; it falls back to the generic OP_0B/door sign title
until the event-loader-reinvocation mechanism is reverse-engineered. Reward
granting on victory is **not yet ported** (no combat engine / victory
callback exists — same fidelity level as OP_12/13; see
`EventCombatEncounter.h`). Codec/tables mirrored in
`tools/mm2_selector_bin.py` and `wiki/maze-walker/selectorBin.js` (the web
walker DOES run the full ticket→combat→reward loop synchronously for selector
`0x08`, since it has no such engine gap).
- Temple cleric-spell purchase D–F (`0x1DAC6` → spellbook bit; spell system), the
  donation `0x1F` reward grant (found-item buffer `A4-$794C`/`A4-$3F1C`, stat bump
  `A4-$5770`), the training **HP path** (`0x9BCA`, calendar mode `0x9B48`),
  inn (`-$7CD4`), guild enroll gold-deduct + roster+$0B write (`-$7DA0` →
  `0xD89C` → `0x1A1B2`), **Feldecarb `0x0E` farthing→castle key ported**
  (**Nordon `0x0A` goblet turn-in ported** — FAQ/loc-60; `0xD634` is selector
  `0x7F` combat, not Nordon), `0x64` portal (`-$7D9A`), plus the interactive
  presentation (member-select UI, RNG caption tables).

### `event.dat` shared cross-town "string bank" (decoder location 60)

Decoder location 60 (`EXTRACTED/docs/events/loc_60_quest_nordon_nordonna_corak.md`,
`record kind: string_bank`) has **no script segments of its own** — every
"event" in it is `(missing script segment)` — it is a pure string pool. Its 28
strings are the **real, town-invariant text** for several generic selectors
that would otherwise show a fabricated/misleading placeholder:

| str idx | text | used by |
|---------|------|---------|
| `[08]` | "The spirit of Corak proclaims, 'Fantastic adventure awaits you...'" | game-start Corak monologue |
| `[09]`–`[15]` | Nordon's Goblet-quest dialogue | **Nordon @ Middlegate `(10,2)`** — accept quest, return goblet, visit Nordonna (see [`53-nordon-nordonna-quests.md`](53-nordon-nordonna-quests.md)) |
| `[16]`–`[20]` | Nordonna's kobold-rescue + hireling reward | **Nordonna @ `(1,2)`**; sons rescued cavern loc 17 `(15,0)`; hire **A/B** at inn (doc 53) |
| `[21]` | "A sleepy conjurer yawns, 'My friends, for only 20 gold I can enroll you in the local mage guild.' Buy in (y/n)?" | `0x0D` enroll mage guild |
| `[22]` | "Not enough gold!" | shared rejection |
| `[23]` | "Fanciful Feldecarb Fountain flows full as fluttering faeries frolic fastidiously. Flick a farthing (y/n)?" | `0x0A` **Feldecarb Fountain** @ Middlegate **`(2,10)/W`** (top-right of town) |
| `[24]` | "You find a fabulous castle key!" | fountain success |
| `[25]` | "Fool, you have no farthing to flick!" | fountain failure when no farthing item — **NOT** arena tickets (`0x08` @ `(2,13)`) |
| `[26]` | key-purchase prompt (500gp) | locksmith |

Note that Middlegate's OWN local string table happens to also contain "Fool,
you have no farthing to flick!" at `str[20]` (coincidence: Nordon's farthing
theme recurs locally too) — this is what the OP_0B str_idx/caption bug (see
"`OP_0B` does not display text" below, and doc 28 §2.1) was accidentally
displaying for the enroll-guild and goblet-quest doorways, since both those
tiles' `OP_0B` calls happen to pass sign id `0x14`, which collided with local
`str[20]` under the old (wrong) "str_idx doubles as a caption index" logic.

### `OP_0B` does not display text (corrected 2026-07)

Direct trace of `event_op0b_service_window` @ `0x15DB0` (asm, full listing in
`EXTRACTED/mm2.capstone.annotated.asm`) shows it: (1) resolves `str_idx` (its
first byte argument) through the per-env sign table @ `0x15756` into an `.anm`
sign/portrait id, (2) draws the window frame + portrait bitmap via thunks
`-$7FBC`/`-$7FC2`, and (3) sets exit-flag bit `2`. There is **no `jsr` to any
text/string routine anywhere in the handler** — `str_idx` is a **sign lookup
key only**, never a `str.dat` text index, despite the name. A prior port pass
ALSO fed `str_idx` into `resolveString()` to fabricate a "service title"
caption; because the sign-table key and the local string-table index share the
same byte range, this produced byte-for-byte wrong text whenever a location
happened to reuse a sign id that collided with an unrelated string (see the
Middlegate `0x14`/`str[20]` and `0x03`/`str[3]` examples above). Fixed by
removing the fabricated capture in `EventRuntime.cpp`'s `case 0x0B` — the sign
graphic is still drawn (that part is real and ASM-confirmed), but no caption
text is derived from it anymore.

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

## `OP_23` Day Gate (Verified)

`OP_23` @ `0x16ADA` gates on the day-of-year of the **current era**:

1. `era = -$79B6` (word). `day_byte = -$79DE[era*2 + 1]` — i.e. the **low byte of
   the current era's day word** (`-$79DE` is `word[10]`; 68k RAM is big-endian so
   the `+1` byte is the low 8 bits; days are 1..180 so this equals `day & 0xFF`).
2. Read `arg1`, then `arg2` (both always consumed — argc 2).
3. `cond_flag` (`-$7951`):
   - `arg1 == 0xB5` → `cond = (day_byte bit0 set)` — **odd-day** gate.
   - `arg1 == 0xB6` → `cond = (day_byte bit0 clear)` — **even-day** gate.
   - else → `cond = (arg1 <= day_byte <= arg2)` — inclusive byte range.

**Port** (`EventRuntime` `case 0x23`): reads `arg1`/`arg2`, uses
`gs.day() & 0xFF` (which already resolves `-$79DE[era]` via `mm2_gs_day`), and
implements all three branches. The `0xB5`/`0xB6` odd/even-day cases were previously
missing — they fell through to a bogus range compare against `0xB5/0xB6`.

## `OP_12` / `OP_13` Encounter Setup (Verified)

`OP_12` @ `0x16300` and `OP_13` @ `0x16386` both seed the **encounter-setup
block** that the combat engine (`-$7EDE` @ `0x051C2`) consumes, then set the
script-abort flag so the event interpreter yields to combat. `OP_13` just calls
`0x16300` with an internal flag of `1`.

Field layout (all `A4`-relative; decoded from the handler + the combat-setup at
`0x12CE0` and the XP-budget loop at `0x120E2`):

```c
/* OP_12/OP_13 encounter-setup block (game-state, A4-relative) */
struct Mm2EncounterSetup {
    uint8_t monster_slot[10];  /* A4-$11DE  up to 10 visible monster-type ids */
    uint8_t overflow_type;     /* A4-$11D4  monster type for the group BEYOND
                                *   10 slots; also the breed/multiply target
                                *   (0x100B0) and the XP-budget tier for the
                                *   overflow group (0x120E2). Doubles as a flag:
                                *   if !=0, `live_count` is added to the total. */
    uint8_t mode;              /* A4-$796B  0x80 = fixed/pre-filled fight (skip
                                *   random picker 0x1213E); 0 = seeded-random.
                                *   (engine also uses 3 = rest ambush @ 0x19D64) */
    uint8_t live_count;        /* A4-$77BE  live monster count */
};
```

**The 2 tail bytes (previously UNKNOWN) are now decoded:**

- **Tail byte 1 → `A4-$11D4` (`overflow_type`)** — the monster **type id** used
  for any monsters beyond the 10 visible slots, the target type checked by the
  breed/multiply path (`combat_multiply_breed` @ `0x100B0`: `cmp.b -$11D4,d1`),
  and the XP tier for the overflow group (`0x120E2`: `move.b -$11D4,d0; lsr.w
  #$4`). It is **also a presence flag**: the combat setup only adds the count
  byte when it is non-zero.
- **Tail byte 2 → `A4-$77BE` (`live_count`)** — the live monster count seed.

**Count recomputation @ `0x12CE0`** (combat setup): `j` = number of non-zero
entries in `monster_slot[0..9]`; if `overflow_type != 0` then `j += live_count`;
finally `live_count = j`. So the on-screen monster total = (# listed groups) +
(tail2 extra monsters of type tail1, only when tail1 ≠ 0).

**`OP_12` vs `OP_13` (mode):** at `0x12CFA` the engine reads `mode`. `>= 0x80`
(OP_12) → subtract `0x80` and **skip** the random picker `0x1213E` (use only the
listed slots — a fixed fight). `< 0x80` (OP_13, mode 0) → **run** the picker,
which augments the seeded slots per XP budget (`-$6FCA`) and attrib min/max.

ASM order in `0x16300`: set mode `0x80`; `clr.w -$4F4E` (combat/redraw flag);
read 10 monster bytes; if mode==0x80 read tail1→`-$11D4`, tail2→`-$77BE` else
clear both; `jsr -$7EDE` (combat); `move.b #$1,-$79EA` (abort); post-combat
pending-event latch via `-$7F1A` → `A4-$7952` (combat-engine territory).

**Port** (`game/src/events/EventCombatEncounter.cpp`,
`game/include/mm2/events/EventCombatEncounter.h`): seeds all of the above fields
exactly and sets `MM2_GS_SCRIPT_ABORT`. The combat run (`-$7EDE`) and its
post-latch (`-$7F1A`→`-$7952`) are the **unported combat engine**; combat
victory (`A4-$77BD`, read by `OP_2B`) is set by `combat_victory_rewards`
@ `0x12438`, **not** by this handler — so the old MVP that faked
"You are attacked!/Victory!" text and set the victory latch was a fabrication
and was removed. GS field constants: `MM2_GS_MONSTER_SLOTS` /
`MM2_GS_ENCOUNTER_OVERFLOW_TYPE` / `MM2_GS_ENCOUNTER_MODE` /
`MM2_GS_MONSTER_COUNT` / `MM2_GS_ENCOUNTER_REDRAW` in `mm2_gamestate.h`. See also
[`35-encounter-tables.md`](35-encounter-tables.md).

## `OP_19` Give-Item + `OP_1C/1D/1E/31` Engine Calls (audit pass)

**`OP_19` @ `0x165D8` — give item to party (Verified, ported).** Reads 4 bytes
`arg1, id, charges, flags`. Clears `cond_flag` (after capturing it), then for each
party member (count `A4-$795A`, ptr via `0x477E`) scans the backpack item-id run
`rec[0x3A+m]` (m=0..5) for the first empty (`==0`) slot and writes
`id→rec[0x3A+m]`, `charges→rec[0x40+m]`, `flags→rec[0x46+m]`, setting `cond_flag=1`.
If `arg1>=0x80` the id comes from `cond_flag` instead of the literal byte. If every
backpack is full it drops the item into the shared **found-item buffer** (overflow
tail @ `0x166A0`): it scans buffer `id[0]`,`id[1]` (`-$3F1C`) for the first empty
slot — else slot 2 — and writes `id→A4-$3F1C[i]`, `attr3(flags)→A4-$3F19[i]`,
`attr2(charges)→A4-$3F16[i]`, then raises sentinel `A4-$794C=0xFF`. `cond_flag`
stays 0 in the overflow case (item not placed on a member). This is the same buffer
`OP_2A` fills (see §Found-item / treasure buffer below). It **independently confirms
the SoA item layout** (6 ids @ `+0x3A`, 6 charges @ `+0x40`, 6 flags @ `+0x46`;
equipped likewise @ `+0x28/+0x2E/+0x34`) that OP_16 first revealed; the roster
struct (`mm2_roster_codec.h`) now models it as SoA arrays. **Port (Verified):** the
overflow now appends to the modeled buffer via `mm2_found_items_overflow_append`
(`EXTRACTED/decomp/mm2_found_items.{h,c}`); the buffer's "you found..."
pickup/distribution (Search payoff `0x1B19C`) is engine territory still deferred.

## Found-item / treasure buffer (`A4-$3F1C` / `A4-$794C`)

A 16-byte shared "pending loot" buffer in the A4 game-state block, written by
**OP_2A** (`0x16D16`, full treasure fill) and **OP_19** (`0x165D8`, backpack
overflow), consumed by the **Search** flow.

**Memory layout** (ascending A4 displacement; raw `lea`/`move` offsets verified in
`EXTRACTED/mm2.capstone.annotated.asm`). gold/gems are **big-endian in RAM**
(68k `move.l`/`move.w`), not little-endian — this is a runtime buffer, not a `.dat`
file, so the CLAUDE.md disk-LE default does not apply:

| A4 disp | raw word | type | field | written by |
|---------|----------|------|-------|------------|
| `-$3F1C` | `$C0E4` | `byte[3]` | item ids | OP_2A `(a0,i)`, OP_19 `(a0)` |
| `-$3F19` | `$C0E7` | `byte[3]` | item flags | OP_2A 3rd triplet byte, OP_19 `attr3` |
| `-$3F16` | `$C0EA` | `byte[3]` | item charges | OP_2A 2nd triplet byte, OP_19 `attr2` |
| `-$3F13` | `$C0ED` | `byte` | unused pad | — |
| `-$3F12` | `$C0EE` | `u16` (BE) | gems | OP_2A `move.w` |
| `-$3F10` | `$C0F0` | `u32` (BE) | gold/exp (24-bit) | OP_2A `move.l` |
| `-$794C` | `$86B4` | `byte` | sentinel | OP_2A / OP_19 = `$FF` |

**OP_2A fill** (`0x16D16`): reads via the script readers `$155fc` (u24, LE stream)
→ `move.l d0,-$3F10`; `$155da` (u16, LE stream) → `move.w d0,-$3F12`; then a 3-iter
loop reading `id`/`charges`/`flags` per triplet → `-$3F1C[i]`/`-$3F16[i]`/`-$3F19[i]`;
finally `move.b #$FF,-$794C`. 3+2+9 = 14 inline bytes (matches `argc`). It does
**not** distribute the reward to the party — distribution is the deferred Search
payoff.

**OP_19 overflow** (`0x166A0`): when no party backpack has a free slot, scan buffer
`id[0]`,`id[1]` (`cmpi.w #2`) for the first empty; if both occupied, use slot 2
(overwrite). Write `id→-$3F1C[i]`, `flags→-$3F19[i]`, `charges→-$3F16[i]`;
`move.b #$FF,-$794C`.

**Consumption / display (traced; distribution deferred).** The main loop
(`0x1276`) compares `-$794C` against `$FE`; when equal it falls into `0x1280`,
**bumps the sentinel to `$FF`** (`addq.b #$1`) then `jsr $4800` (Search). The **S**
key also invokes `0x4800` directly. Search (`0x4832`–`0x4870`) treats the buffer
as "has loot" when any `id[0..2]!=0` **OR** gold `-$3F10 != 0` **OR** gems
`-$3F12 != 0`; empty → `Nothing Here!` (`$48AB`) + `-$79E5=1`, otherwise
`jsr -$7D1C` → **`0x1B19C`** (the "you found..." pickup/distribution UI). The port
models the buffer state faithfully (`mm2_found_items.{h,c}`, codec
`tools/mm2_found_items.py`, GS constants `MM2_GS_FOUND_*`) and defers only that
display/distribution step. See §`$FE` sentinel writer and §`0x1B19C` below.

### `$FE` auto-search sentinel writer — LOCATED at `0x1d796`

The `$FE` value is written at **`0x1d7e8`** inside the object-interaction handler
**`0x1d796`** (one of a 3-handler family `0x1d716`/`0x1d758`/`0x1d796`, each taking
an object pointer in `$8(a5)` and gated by a yes/no prompt helper `0x1d90c`).
`0x1d796` is the **"place orb on pedestal" mechanic**: on success it ORs a bit into
the 5-bit accumulator `-$799e` (bit chosen by `-$66b1[-$79f2]`, indexed by the
current element/sector `-$79f2`). When **all five bits are set** (`-$799e == 0x1f`):

```
move.b #$FE, -$794C(a4)   ; arm the auto-search sentinel
move.b #$D4, -$3F1C(a4)   ; found-item id[0] = item 0xD4 (212) — the reward
clr.b  -$799e(a4)         ; reset the accumulator
```

So the next main-loop tick auto-runs Search, which "finds" item 212 and distributes
it via `0x1B19C`. **This corrects the prior guess** that `combat_victory_rewards`
(`0x12438`) sets `$FE`: that routine actually *clears* the sentinel
(`0x1243e clr.b -$794C`) so stale loot is not re-found after a fight. The `$FE`
writer is **not** combat-related — it is the elemental-orb collection quest reward
(engine/world-object territory, outside the event VM, so deferred in the port).

### `0x1B19C` Search payoff (the "you found…" UI) — structurally traced

`0x1B19C` is the found-item presentation **and** distribution flow:

1. **Entry split** `tst.b -$794C; bne $1b49a`. When the sentinel is **non-zero** it
   takes the short path `0x1b49a`: print the buffer (text via `-$7ec0`), fold a
   non-`$FF` sentinel byte into `id[0]` (`move.b -$794C,-$3F1C`), `clr.b -$794C`,
   then refresh (`-$7e54`/`-$7f6e`/`-$7f4a`/`-$7d40`) and set screen mode
   `-$7950=$3`. When the sentinel is **zero** it runs the full found-items UI.
2. **Full path** computes an item *count* (`-$9(a5)`): +1 if any of the 3 item ids
   set, +1 if gems `-$3F12` present, +1 if gold `-$3F10` present, then a max pass
   over item flags `-$3F19[0..2] & 0x3F`, clamps the count to `[1,8]`, and lays out
   a text window (`-$7c74(0x16,3,0x26,0xF)`).
3. Per item it calls the per-character field engine (`-$7f68` returns a field-kind
   code 0x31/0x32/0x33/0x34 → helpers `0x1aec2`/`0x1af6e`/`0x1afe8`/const) to
   format/grant gold/exp/gems/items, drawing each line with the UI thunks
   (`-$7be4` text, `-$7bfc` cursor, `-$7c02/-$7c08` window). Embedded strings:
   "Search…", "The Party Has found:", "Treasure!", "Identify", "(rating)",
   "equipables", "Item Charges =", stat names ("Might/Speed/Accuracy/…/MaxHP"), etc.

This is presentation + distribution built almost entirely on **runtime A4 vtable
thunks** (`-$7c74`, `-$7be4`, `-$7f68`, `-$7d40`, …) plus the character-record field
engine; it remains **engine/UI territory and is deferred** (the headless port models
only the buffer state). Documented here rather than stubbed with invented behaviour.

**Runtime A4 function pointers — `OP_1C` Verified; `1D`/`1E` presentation-resolved; `31` Partial (abort gate ported).**
`jsr (d16,a4)` (`0x4EAC`) hits the static JMP stubs in the data hunk (doc 49 /
`tmp_mm2_thunk_map.txt`), not an opaque runtime vtable for these slots:

- `OP_1C` `-$7BB4` → **`rng_roll` @ `0x22BC6`** (entropy `0x24048`, state
  `A4-$60E2`). Handler pushes `(1, u8)` and stores the **raw roll** into
  `cond_flag` (`move.b d0,-$7951`). Port: `rng_->range(1, arg)`.
- `OP_1D` `-$7E84` → **`audio_wait_helper` @ `0x6798`**: index `(u8*7+1)`,
  halves wait, polls `-$7BD2`, yields via `-$7B42`. **No VM/GS write** —
  presentation/audio only. Port: consume argc=1.
- `OP_1E` `-$7BC0`→`0x22B4A` delay(10) + `-$7BD2`→`0x22586` input poll,
  `arg` iterations, break on key. Timing only. Port: consume argc=1 (no-op).
- `OP_31` `-$7F08`→`0x4952` (per-member damage). OP_31 call site peals three
  zero out-flags → AC/resist prologue skipped; HP path ported. Full combat
  scratch branches unused here. Living abort `-$7F14`→`0x47EC` ported.
  See doc 56 §3.

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
- `OP_25` is party **gems** pool-pay (`0x6B9A`); small observed amounts `0/1/2` fit gem costs.
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
- `OP_25`: gems pool-pay (`+$5C`), same deduct+pool pattern as OP_24 gold.
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
| `0C` | Verified | Map transition @ `0x15E12`: bit6 → `rng(1,20)+5` (+0x10 if ≥0x11) \| bit7; dest≥0x80 → `rng(1,255)` tile; dest&=0x3F; then `-$7FDA` map load; sets pending latch. Ported |
| `0D` | Verified | `0x06FB8` canned on-screen sequence player (idx 0..9; idx 0 gated by `-$79AF`, 1..9 by `-$79B0`). Presentation-only, no GS writes → port stubs it logic-faithfully |
| `0E` | Partial | Dispatch + default-range bins **Verified**. **Ported:** temple/training/smith/guild/tavern menus; **Feldecarb `0x0E`**; **Nordon `0x0A`**; **Nordonna `0x0B`**. **Partial:** tavern food/drink RNG, store `0xA62C`, inn hireling unlock, guild enroll edges. Inn rest = world Rest (`0x19E20`), not OP_0E `0x01` (registry) |
| `0F` | Verified | End script / cleanup `0x171AC` |
| `10`/`11` | Verified | Conditional token skip; full `-$6CC8` length table now extracted byte-exact and ported (`eventVmTokenDelta`, includes the `OP_00`/`OP_25` ROM quirks — see §Notes on `*_SKIPTOK` Opcodes) |
| `12` | Verified* | 10 monster ids → `A4-$11DE`; mode=0x80; **tail1=`-$11D4` overflow/breed type, tail2=`-$77BE` live count**. Setup + `CombatSession::enter` ported; *fight-depth Partial (doc 35 / doc 56 §4) |
| `13` | Verified* | Same setup, mode=0 (seeded-random); clears tail fields; random picker `0x1213E` ported in `encounterRunRandomPicker`. *Same fight-depth gap as OP_12 |
| `14` | Verified | Clear tile event / visited bit7 |
| `15` | Verified | Party field test @ `0x16426`: member-spec 0=all (N..1), 1..8=one, 9=selected (`-$5D42`/`-$5D3F`); cond \|= (val? field&val : field). Field map via `0x17766` |
| `16` | Verified | Party item-presence scan (arg1 discarded, arg2 = item id). Behavior byte-exact; port now scans `rec[0x28+m]`/`rec[0x3A+m]` (m=0..5) over party. NOTE: the 6-byte stride-1 read implies equipped/backpack store **6 item-ids contiguously** (SoA), not the interleaved id/bonus/flags the roster struct currently models — flagged for the roster team |
| `17` | Verified | Load script variable group → cond (2nd byte discarded); raw byte |
| `18` | Verified | Masked party write `(f&and)\|or` via same `0x16426` engine (flag=1); clears cond on entry |
| `19` | Verified | Give item to party: fills first empty backpack SoA slot (`id`→`+0x3A+m`, `charges`→`+0x40+m`, `flags`→`+0x46+m`), cond=1. Ported byte-exact. All-backpacks-full overflow now appends to the modeled found-item buffer (`A4-$3F1C` ids / `-$3F19` flags / `-$3F16` charges, sentinel `A4-$794C=$FF`) via `mm2_found_items_overflow_append`; only the Search-payoff display (`0x1B19C`) is deferred. See §Found-item / treasure buffer |
| `1A` | Verified | Store `u8` into script variable (2 bytes: id, value — no index) |
| `1B` | Verified | Clear cond unless `cond >= threshold` |
| `1C` | Verified | `cond_flag = rng_roll(1,u8)` via `-$7BB4`→`0x22BC6` (entropy `0x24048` / `A4-$60E2`). Stores **raw roll**, not boolean. Port uses `gameplay::Rng`; `GameSession::start` reseeds so launches diverge |
| `1D` | Verified* | `-$7E84`→`0x6798` audio_wait((u8×7)+1). **No** VM/GS write. Port consumes argc=1. *Presentation-only — fidelity is "no invented state" |
| `1E` | Verified* | Busy-wait `u8`×{delay `-$7BC0`→`0x22B4A`(10), poll `-$7BD2`→`0x22586`}. Timing only. Port consumes argc=1 (headless no-op) |
| `1F`/`20` | Verified | Party effect @ `0x1690E`/`0x167B0`: add/sub with saturate; subtract clears cond on underflow. Selected member via `-$5D42`. `width_op` writeback width still uses field-map natural width (edge GAP) |
| `21` | Verified | Patch map tile + `EXIT_FLAGS \|= 4` (redraw bit2 @ `0x16A34`) |
| `22` | Verified | Era range gate (`-$79B5`) |
| `23` | Verified | Day-of-year gate (`0x16ADA`). day = low byte of `-$79DE[era]`. `arg1=0xB5`→odd-day, `arg1=0xB6`→even-day, else inclusive byte range `[arg1,arg2]`. Ported byte-exact (the `0xB5`/`0xB6` odd/even-day cases were previously missing). See §OP_23 |
| `24` | Verified | Gold pool-pay via `0x155DA` + `-$7E6C`→`0x6ACE`: sum party gold `+$66` (slots with roster idx `<0x18`), if ≥ amount deduct and pool remainder on first eligible. Cond = success |
| `25` | Verified | Gems pool-pay via `-$7E66`→`0x6B9A` (same pattern for `+$5C` u16). **Not** tickets/keys (those are OP_0E `0x08` / OP_28). Handler reads 2 arg bytes; ROM skip-table entry is 2 — see §Notes on `*_SKIPTOK` |
| `26`/`27` | Verified | Member select @ `0x16BC0`: on success `cond=slot`, `-$5D42=slot`, `-$5D3F=slot`; reject dead (`+$26≥$81`); ESC aborts. Input-path difference 26 vs 27 still presentation-only |
| `28` | Verified | Backpack-only consume (`+$3A`); 1st arg discarded; always consumes on hit → cond |
| `29` | Verified | Force script abort |
| `2A` | Verified | 14-byte treasure/reward block (u24 gold/exp, u16 gems, 3× id/charges/flags triplets). Fills the found-item buffer (`A4-$3F10` gold / `-$3F12` gems / `-$3F1C`-`-$3F19`-`-$3F16` items) + sentinel `A4-$794C=$FF`, byte-exact via `mm2_found_items_op2a_fill`. Does NOT distribute to the party (that is the deferred Search payoff `0x1B19C`); the old port's immediate member-0 deposit was a fabrication and was removed. See §Found-item / treasure buffer |
| `2B` | Verified | Token skip only when `A4-$77BD` (combat-victory latch) set; ASM-traced byte-exact @ `0x16D74`. Latch set by `CombatSession::finishVictory` @ `0x12438` (not by the opcode itself) |
| `2C` | Verified | Add `u8` to word `-$79B8`; `EXIT_FLAGS \|= 1` |
| `2D` | Verified | Match party class/sex/race nibble (any/all mode); 2-value variant when arg1 has no high bits |
| `2E` | Verified | OR arg2 into member `+(arg1-0x6E)+0x51` for two specific classes ({4,2}/{3,1}) |
| `2F` | Verified | **Not a silent clear** — `0x16FEA` calls `-$7F92` to read up to 10 chars into `-$5C50`, space-pads remainder, clears `-$5C46`. Port: `EventVmWait::Answer` fills the buffer then resumes |
| `30` | Verified | 10-byte compare: `toupper(input[i])` vs `(0x11A - expected[i])` for i=0..9; cond=1 iff all match. Port byte-exact |
| `31` | Partial | Sets `EXIT_FLAGS` bit1; living abort `-$7F14`→`0x47EC` ported. Per-member `-$7F08`→`0x4952`: OP_31 peals three **zero** out-flags so AC/resist prologue is skipped — HP path (`+$26`/`+$5E`) ported. Full AC/RNG branches unused by this opcode. Spec `9` = selected (`0x1710A`); **not** spec `8`. argc=3 |
| `32` | Verified | cond = party class-nibble count (`0x04614`/`0x45C4`, not a var load) |

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

**RESOLVED (2026-07, Verified):** B, precisely. Every default-range selector
(`0x09`–`0x10`, `0x11`–`0x37`, `0x38`–`0x4B`, `0x4C`–`0x54`, `0x56`–`0x5B`,
`0x5C`–`0x5E`, `0x65`–`0x69`, `0x6A`–`0x7C`, `0x97`–`0x98`, `0xE3`–`0xF3`,
`0xF4`–`0xFB`) is binned by `0x15EDC` and dispatched — via the SAME fixed
thunk `-$7DFA` — to the SAME `0x9D76` Arena Games **ticket-combat-reward**
engine (ticket scan/consume, fixed encounter, gold reward). It is not a
generic shop. See §"Default-range Arena Games ticket engine (`0x9D76`)"
below and `EventVmHelpers.h`.

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

**RESOLVED (Verified):** A. Tail byte 1 → `A4-$11D4` = overflow/breed monster
**type id** (also a presence flag: 0 ⇒ no overflow group, and the XP tier for
that group). Tail byte 2 → `A4-$77BE` = the live-monster **count** seed
(recomputed by the combat setup at `0x12CE0` from the actual listed-slot
count, so it is closer to a difficulty/count seed than "backdrop"). Neither
byte is a terrain/backdrop id or a gold multiplier. Full trace in
§"`OP_12` / `OP_13` Encounter Setup (Verified)" above.

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

> **CONFIRMED (`OP_1F` add / `OP_20` subtract) — `event_op1f_party_effect` @ `0x1690E`
> → per-member `event_party_effect_apply` @ `0x167B0`:**
> Byte layout after the opcode: `[member_spec][selector][width_op][value:3 LE]`
> — the value is a **3-byte little-endian** amount (`0x155FC` = word LE + 1 byte
> `<<16`). It is **pure field arithmetic**, NOT a heal/teach/levelup dispatcher:
> - `selector` → record field via the field map (§9b); `width_op` is the
>   writeback byte count (`3`→`4`).
> - **ADD** (`OP_1F`): `field = min(field + value, widthMax)` — saturates at the
>   field-width max (`0xFF`/`0xFFFF`/`0xFFFFFFFF`). **No** hp_max/sp_max clamp here.
> - **SUBTRACT** (`OP_20`): if `field < value` → set `cond_flag = 0` and write
>   nothing (atomic "can't afford", used for purchases); else `field -= value`.
> - `cond_flag` is set to **1** on entry; writeback is gated on `cond_flag` for
>   every member, so once a subtract underflows the rest of an all-party op is
>   also skipped.
> - `member_spec`: `0` = all party (1..count), `9` = currently-selected member
>   (`-$5d42`), `1..8` = that member, `>count` = no-op. Bit7 set on `member_spec`
>   selects the obscure "add the live `cond_flag`" byte variant.
>
> Port: `EventPartyEffects::eventApplyPartyEffect` (the previous
> teach-skill/level-up/workout/fountain-heal special cases were fabrications and
> were removed).

---

### 9b. Member field-selector map (`OP_15`/`18`/`1F`/`20`) — CONFIRMED

`OP_15`/`18`/`1F`/`20` all address a **character-record field** through the
shared field engine `event_member_field_ptr` @ **`0x17766`**, which dispatches a
selector byte (`0x00..0x7F`) via the **`0x80`-entry word jump table @ `0x17FEA`**
(`target = 0x180FE + int16(table[sel])`) to a record byte offset + access width.

Key point: **the selector byte is NOT the record offset.** It is remapped. The
map was extracted byte-exactly from the ROM and validated against the
`Mm2RosterRecord` (0x82 bytes) layout — e.g.:

| sel | offset | width | field |
|-----|--------|-------|-------|
| `0x20` | `0x74` | 2 | `hp_current` |
| `0x28` | `0x58` | 2 | `sp_max` |
| `0x35` | `0x5A` | 2 | `sp_current` |
| `0x38` | `0x5C` | 2 | `gems` |
| `0x3A` | `0x5E` | 2 | `hp_max` |
| `0x31` | `0x62` | 4 | `experience` |
| `0x3E` | `0x66` | 4 | `gold` |
| `0x2C` | `0x6D` | 1 | `personality_base` |
| `0x74` | `0x79` | 1 | `class_quest_guild_mask` (party quest bits, e.g. "seen Pegasus" `0x40`) |
| `0x00`/`0x01` | — | — | computed getters (base max HP/SP @ `0x181B0`) |

`OP_15`/`18` set bit7 on the selector (in `0x163CA`), so `0x18100` forces a
**byte** access (low byte of multi-byte fields); `OP_1F`/`20` use the natural
width.

- Full table + resolver: `game/include/mm2/events/EventFieldMap.h`
  (**generated** — `python tools/extract_event_field_map.py`; JSON dump in
  `EXTRACTED/event_field_map.json`).
- Runtime: `eventVmApplyPartyByteOp` (`OP_15`/`18`) and
  `EventPartyEffects::resolveField` (`OP_1F`/`20`) both resolve through this map.
- **Per-member storage (migration done):** selector `0x74` writes the real
  per-character record byte `0x79` (`class_quest_guild_mask`); bits `0x10`/`0x20`/
  `0x40` are Vulcania/Sandsobar/Pegasus "seen" markers. The old global
  `MM2_GS_PARTY_PROGRESS` bridge was **removed** (that A4 byte `-$79E8` is an
  unrelated engine flag the port had misappropriated). The `GameSession`
  Corak-intro one-shot — which is port-side scene scheduling, not a classic
  per-character quest bit — now uses its own `corak_intro_seen_` session flag
  instead of riding on `0x79` bit `0x01`.

---

### 9c. Variable load opcodes (`OP_17` / `OP_32`) — CONFIRMED

`OP_17` @ `0x165A4` resolves a single id byte via `event_op_var_resolve`
@ `0x15620` (id → A4 field pointer) and sets `cond_flag = *ptr` — the **raw
variable byte**, not a `0/1` boolean (then reads+discards a 2nd byte). `OP_1B`
later compares `cond_flag` against a numeric threshold, so the value matters.
The resolver id map (`-$798B` flag bank for `0x00..0x17`; singletons `0x23`,
`0x2B`, `0x2C`, `0x32`, `0x33`; ranges `0x27..0x2A`, `0x3B..0x3E`, `0x80..0x83`;
`0x84` = era byte) is ported in `eventVmResolveVarOffset`.

`OP_32` @ `0x17190` is **not** a variable load (despite the older "load cond
from variable" label). It calls thunk `-$7F2C`, which the A4 thunk map resolves
to **`0x04614`**: `cond_flag` = the count, summed over **living** party members
(condition byte `record+0x26` < `0x81`), of the nibbles of `record+0x50` equal to
the id arg (per-member helper `0x45C4` tests both nibbles → 0/1/2). The result is
stored as the raw count byte. `record+0x50` is a packed pair of
**alignment/profession-title nibbles** (ASM annotation "class nibble"); the
scripts gate it against titles — id `0x04` = Crusader (Hillstone "...only bestow
a quest unto a Crusader"), `0x08` = Heroic (Murray's Cave "You are not heroic!"),
`0x09` = druid/pagan (C3 Oak Grove). It is **not** the `spells[]` bitfield the
roster-struct label at `0x4C..0x57` implies (MM2 gates spells by spell level, not
per-spell flags — that struct label is a mis-guess for this byte). Ported in
`eventVmCountPartyNibbleMatches`; the prior `eventVmLoadVar`/`0x15756` reading was
incorrect.

`OP_17` was previously booleanizing `cond_flag` (now fixed to store the raw byte).

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
