# Might and Magic II (`mm2`) — executable analysis

Working notes for `mm2.capstone.asm` (Capstone 68000 listing) cross-checked with `mm2.asm` (IRA) and the raw `mm2` hunk binary.

> Split docs are now under `EXTRACTED/docs/` and first-pass C lift scaffolding is under `EXTRACTED/decomp/`.

**Status:** Parts 1–12 documented (startup → main loop → map view → party setup). Part 13 lists next traces.

---

## How to read `mm2.capstone.asm`

| Item | Detail |
|------|--------|
| **Addresses** | Listed as **load addresses** in code segment 0, base **0**. File offset in disk image = **0x20 + address** (32-byte hunk header). |
| **Two segments** | Lines 5–~44935: main code (153,916 bytes). Line ~44936+: overlay code at **0x2593c** (Capstone concatenates CODE hunks only). |
| **IRA vs Capstone** | IRA uses `ORG $0` and includes 0x00–0x3f of hunk metadata as fake instructions. Capstone starts at hunk data but still shows the first **8 bytes** of reloc overlay as code — **skip lines 6–11** (see below). |
| **Address skew** | For code in segment 0: **IRA_line ≈ Capstone_line + $20** (IRA counts from file start; Capstone from code hunk). The `JMP $248AE` lands on **`JSR` at Capstone `248AE`** (bytes at file `0x248CE`); IRA’s label `LAB_248AE` on a `MOVE.B` is **$20 bytes earlier** — trust Capstone/raw bytes for that entry. |
| **Embedded data** | `skipdata` lets Capstone continue through ASCII; bytes still decode as bogus `moveq`/`ori` — treat long runs of “instructions” spelling English as **data**, not code. |
| **Calls via A6** | After init, **`A6` = ExecBase**. `jsr -198(a6)` = **AllocMem**, `jsr -408(a6)` = **OpenLibrary**, etc. |
| **Calls via A4** | **`A4` = global game workspace**, set to **$7FFE** (`lea.l $7ffe.l,a4`). Most game state is `offset(a4)`. |

---

## Part 1 — Image layout and cold start (addresses 0x00000–0x00200)

### 1.1 Hunk prologue (not real code) — capstone lines 6–11

```
000000  000003e9   ; reloc / hunk overlay tag (must not execute)
000004  0000964f   ; hunk size field from load header
000008  60000016   ; first real instruction: BRA.W +0x16
```

On disk the code hunk begins with the loadable overlay table; the CPU **does not** enter at 0. The first branch is at **0x08**.

### 1.2 Reset stub — 0x08–0x1f

| Addr | Bytes | Meaning |
|------|-------|---------|
| **0x08** | `60 00 00 16` | `BRA.W` → PC = **0x18** (skip padding) |
| **0x0c** | `00 00 ab cd` | Magic **`0xABCD`** (common Amiga executable marker) |
| **0x10–0x1f** | zeros | Padding |

### 1.3 Primary entry — **0x20**

```
000020  4ef9 0002 48ae     JMP $248AE.L
```

All normal launches land in the **main initialization** routine at **0x248AE** (Part 2). There is no return from this jump.

### 1.4 Embedded configuration strings — 0x26 onward

Capstone lines 12–120 are mostly **inline ASCII**, not instructions. Decoded from the binary:

| Addr | String | Role (inferred) |
|------|--------|-----------------|
| 0x26 | `Version 1.01` | Build / version label |
| 0x34 | `MM2Boot:` | Assign name for boot disk volume |
| 0x3e | `MM2Play:` | Assign name for play/data disk |
| 0x48+ | `Middlegate`, `Atlantium`, `Vulcania`, `Sansobar`, … | World / town names (data tables for UI or lore) |
| (later) | `Knight`, `Paladin`, `Sorcerer`, … | Class names |
| (later) | `Human`, `Elf`, `Dwarf`, … | Races |
| (later) | `Gnome`, `Half-Orc`, `Goofball`, … | More races / humour row |
| (later) | `Female`, `Male` | Sex labels |

These sit in the **code hunk** as read-only constants (typical of early 1990s Amiga ports compiled with DICE/MANX).

---

## Part 2 — Main initialization (`0x248AE` region)

Capstone ~lines 44980–45130; IRA labels `LAB_248xx` / `LAB_249xx` (often **$20 lower** than Capstone addresses — see table above).

**True target of `JMP $248AE` (Capstone `0x248AE`):**

```
0248ae  4eba054c     JSR $24DF4(PC)    ; I/O helper → eventually 24E1C
```

The routines just above (`0x2487E`–`0x248AC`) are **print-string / newline** helpers that call the same I/O core.

### 2.1 Character output helper loop — `0x2487E`–`0x248AC`

Small routines at the end of the init cluster call **`JSR` to `0x24E1C` (PC-relative)** with a character or `10` (`LF`) on the stack. That wrapper eventually performs buffered I/O (see §3.2).

Pattern:

1. Walk string at `A2` until `NUL`.
2. For each byte, push char → `JSR 24E1C` → check `D0 == -1` for error.
3. If string empty, emit newline (`#$000A`).

Used for boot banners / error text before the game proper.

### 2.2 Set global pointer — `0x24920`

```
024920  49f9 0000 7ffe     LEA.L $7FFE, A4
024926  4e75                 RTS
```

Called at **`0x24920`** via `BSR` from **`0x248D6`** (Capstone) / IRA `LAB_248D6` — sets **`A4 = $7FFE`** before the MANX clear and `OpenLibrary`.

**`A4` points at fixed workspace address `$7FFE`**, not the end of BSS. All `xxx(A4)` globals in this file are relative to that anchor.

### 2.3 Zero MANX heap arena — `0x248D8`–`0x248CA`

```
LEA -$5E62(A4), A1    ; pool base ≈ $7FFE - $5E62 = $219C
LEA -$5E62(A4), A2
...
MOVE.W #$16D7, D1    ; 5847 longwords ≈ 23 KB
MOVE.L D2, (A1)+     ; clear loop (DBRA)
```

**`0x4D414E58` ('MANX')** is written later at the arena end — signature of the **MANX/DICE runtime** memory pool.

### 2.4 Capture Exec and open DOS — `0x248CE`–`0x24912`

| Step | Code | Amiga API |
|------|------|-----------|
| Save stack | `MOVE.L A7, -$34A(A4)` | |
| Exec base | `MOVEA.L $4.W, A6` then store at `-$346(A4)` | `SysBase` from low memory |
| Forbid? | `BTST #4, $129(A6)` → maybe schedule setup | |
| Open DOS | `LEA dos.library(PC), A1` ; `JSR -$198(A6)` | **OpenLibrary** |
| Fail path | `MOVE.L #$38007, D7` ; `JSR -$6C(A6)` | **Alert** (cannot open DOS) |
| Success | `JSR 24928(PC)` | Continue init (below) |

String at **0x24914**: **`dos.library`** (null-terminated, bytes `64 6F 73 2E 6C 69 62 72 61 72 79 00`).

### 2.5 Memory allocation and process setup — `0x24928`–`0x24A5C`

High-level flow of `LAB_24950` (IRA):

1. **`LINK A5`**, save `A2`.
2. **`PEA $10000`** — allocation flags `MEMF_CLEAR` (typical).
3. Read word at **`-$5E6E(A4)`**, multiply by 6 → size; **`JSR LAB_2567C`** → **AllocMem**.
4. Store block at **`-$34E(A4)`**; init fields at offsets 4, 6, 10, `$A` in that block (process / task-like struct).
5. Compute size from stack vs struct → **`-$33E(A4)`**.
6. Write **`'MANX'`** to block; **`JSR LAB_2568E`** (internal helper).
7. **Branch on `tst.l $AC(A2)`** — two paths:
   - **Fast path:** call `24A60`, set flags at `-$33A(A4)`, OR in place bits in the alloc block.
   - **Slow path:** `PEA $5C(A2)`, calls `25744` / `256C6`, optional indirect call via pointer at `+24`, then `24C98`.
8. Further setup: `25494`, `254CC`, maybe register callback with **`PEA $3ED` / `PEA string` / `254BE`**.
9. Final calls **`JSR -$7FF8(A4)`** and **`JSR -$7B48(A4)`** — game-specific inits (not Exec; relative to **A4** jump table).

Returns with **`UNLK A5` / `RTS`**.

This block is the **AmigaDOS + MANX runtime glue** before the RPG engine takes over.

---

## Part 3 — Key runtime thunks (scattered; used everywhere)

### 3.1 `LAB_2567C` — AllocMem

```asm
2567c  MOVEM.L 4(A7), D0-D1    ; pop size, flags from stack
25682  MOVEA.L -838(A4), A6    ; ExecBase
25686  JMP -198(A6)            ; AllocMem
```

Any `JSR` to `2567C(PC)` in the listing is **heap alloc** with arguments pushed beforehand.

### 3.2 `LAB_24E1C` — character / line output

Thin wrapper:

- Pushes buffer pointer at **`-$5E10(A4)`** (approx; IRA: `486c9ff0`).
- Calls **`24E32`** with length / pointer from stack frame.

**`24E74`** implements the actual put-char:

- Treats argument as **FILE***-like struct at `A2`.
- If position `<` end: store byte, advance pointer.
- Handles tab (`#$0A`) and **`#$FF`** special case via **`24F5C`**.

So early “print string” loops are **stdio-style**, not Intuition.

### 3.3 `LAB_24F5C` — flush / special write

Linked with **`24EB0`** code fragment; checks flag bits in the file structure (`BTST #4`, `#2` on `+$C`). Likely **flush buffer** or **console raw write**.

### 3.4 Calls through `A4` (`JSR -$xxxx(A4)`)

**Yes — these are mostly DICE/MANX library stubs and stdio wrappers**, not a separate “Amiga tables” resource file on disk. The linker places hundreds of tiny thunks; each `JSR -$xxxx(A4)` uses the same convention as AmigaOS **LVO calls through a library base**, but with **`A4` as the anchor** instead of `A6`.

#### Anchor and globals

| Item | Detail |
|------|--------|
| **Anchor** | `LEA.L $7FFE, A4` at **`0x24948`** — label `LAB_7FFE` at end of near-data in the image |
| **ExecBase** | `MOVEA.L $4.W, A6` → stored at **`-$346(A4)`** / **`-838(A4)`** |
| **DosBase** | `OpenLibrary("dos.library")` → **`-$342(A4)`** / **`-834(A4)`** |
| **MANX block** | Allocated pointer at **`-$34E(A4)`** / **`-846(A4)`** |

Real OS entry points still use **`A6` + negative LVO** inside the stub farm (see below).

#### How to decode a call

For a static image with `A4 = $7FFE`:

```text
target_address = $7FFE - offset
```

Example: **`JSR -$7B48(A4)`** → **`$04B6`** (real 68000 stub code in segment 0).  
There are **235 distinct negative offsets**; list them with `tools/scan_a4_jsr.py`.

**Do not** confuse offset size with the address of the same hex value in the file (e.g. file offset **`$7C3E`** is unrelated to **`-$7C3E(A4)`**).

#### DOS stub cluster (~`$254C4`–`$25900`)

Dense **resident-library** thunks: load `DosBase` from **`-834(A4)`** into `A6`, then `JMP -LVO(A6)`:

| Address | Pattern | Amiga DOS LVO (typical) |
|---------|---------|-------------------------|
| `0x254D0` | `JMP -30(A6)` | **Open** |
| `0x25468` | `JMP -36(A6)` | **Close** |
| `0x254E6` | `JMP -42(A6)` | **Read** |
| `0x25532` | `JMP -48(A6)` | **Write** |
| `0x254A0` | `JMP -54(A6)` | **Seek** |
| `0x25686` | `JMP -198(A6)` via ExecBase | **AllocMem** |

Game code usually reaches these with **`JSR $254xx(PC)`** (PC-relative), not directly via `A4`. Init at **`0x24A4C`** calls **`JSR -$7FF8(A4)`** and **`JSR -$7B48(A4)`** after setup — those land in the low stub pages (`$0006`, `$04B6` with `A4=$7FFE`).

#### Library name strings (not a table file)

Embedded **`.library` names** and error text (for `OpenLibrary` failure alerts), found in the binary:

| String | File offset (approx) | Use |
|--------|----------------------|-----|
| `dos.library` | `0x2493C` | Startup (`OpenLibrary` at `0x2491E`) |
| `icon.library` | `0x24D5E` | Icon / Workbench glue (`JSR $256D6(PC)` region) |
| `diskfont.library` | `0x2975A` | Setup / font loading |
| `intuition.library` | `0x297B5` | Intuition open + “Couldn’t open…” |
| `graphics.library` | `0x297E8` | Graphics open + “Couldn’t open…” |

Also: `mm2.font`, `MM2.FONT not in FONTS:`, setup menu text (`Game Setup Characters`, etc.) in the same **`0x297xx`** block (overlay tail / setup UI).

There is **no** separate “Amiga tables” or `.fd` resource in `MM2BOOT` / `EXTRACTED` — use **`include_i86/dos/dos.h`** / **`intuition.h`** LVO constants from the Amiga NDK, or the stub scan above.

#### High-traffic `A4` stubs (inferred)

| Call | Typical args | Likely role |
|------|----------------|-------------|
| **`-$7C3E(A4)`** | word: 0, 1, 2, 4 | Console / event flag (early game + overlay) |
| **`-$7ED8(A4)`** | word: 1, 2, 3 | Related selector (often paired with `-$7C3E`) |
| **`-$7BFC(A4)`**, **`-$7BE4(A4)`** | format IDs + string ptrs | **printf-style** output (MANX stdio) |
| **`-$7B48(A4)`** | word: 0 or 1 | Init / shutdown helper (with `-$7FF8` at end of `0x24A5C`) |
| **`-$7F20(A4)`** | index word | **FILE\***-like object; uses offset **`$66`** in struct (heap list walk at `0x7FF8` region) |

#### `28E3A(PC)` (overlay teardown)

Still a **PC-relative** call into overlay code (~`0x28208` in segment 1), not an `A4` library vector — save/exit UI path at **`0x2951A`–`0x295F2`**.

#### Mapping workflow

1. Run `python tools/scan_a4_jsr.py` for offset → absolute address (with `A4=$7FFE`).
2. Disassemble the target in `mm2.capstone.asm` at that address.
3. If you see `MOVEA.L -834(A4), A6` / `JMP -xx(A6)`, match **`xx`** to [Amiga DOS/Exec LVO lists](https://wiki.amigaos.net/wiki/Autodocs).
4. For Gfx setup, grep **`intuition.library`** / **`graphics.library`** and follow **`JSR $258xx(PC)`** thunks from the setup cluster.

---

## Part 4 — Overlay segment (Capstone from **0x2593c**)

Second CODE hunk (15,544 bytes). Loaded **after** main code+data in the real hunk map (see `mm2_segments.json`); Capstone places it immediately after the first code hunk for disassembly continuity.

Contains additional **LINK/UNLK** helpers, more **`JSR -$xxxx(A4)`** engine calls, and tail routines such as:

- **`2951A`–`295F2`**: push mode words, call engine, **`JSR 28208(PC)`** — looks like **shutdown / save / exit UI** (many sequential pointer loads from `A4` globals in the `$F80–$1014` range).

Overlay code is probably **late-loaded or banked logic** (disk swap / memory overlay), not the first path from `JMP $248AE`.

---

## Part 5 — End of DOS init (`0x24A6C`–`0x24A84`)

Inside **`LAB_24928`**, after MANX/DOS setup, the game wires engine callbacks and enters higher-level init:

| Step | Address | What |
|------|---------|------|
| Store handles | `24A16`–`24A28` | Writes pointers into the MANX block at `A4-$34E` |
| Callback | `24A56`–`24A5E` | Optional `PEA $3ED` + string → `JSR 254E6` (register hook) |
| **Engine bind** | `24A74` | `JSR -$7FF8(A4)` — installs jump table / binds `A4` thunks |
| **Game boot** | `24A7A` | `JSR -$7B48(A4)` — loads resources, builds workspace |
| Return | `24A84` | `RTS` back toward OpenLibrary caller |

The **`JMP $248AE`** path does not use a normal stack frame for “main”; after `OpenLibrary` the `RTS` at `24912` returns into the **DOS/CLI** stub, while the game keeps running via **function pointers stored through `A4-$8008` / `A4-$7B48`** during that init.

---

## Part 6 — `A4` workspace map (partial)

`A4` is fixed at **`$7FFE`**. Offsets below are **signed 16-bit `(disp)(A4)`** as in the listing (e.g. `-$7FFE + 0x860E`).

### 6.1 Game state bytes/words

| Offset | Used for |
|--------|----------|
| `-$860E` | **Current screen / UI mode ID** (drives `LAB_256E`) |
| `-$861A` | Previous screen ID (skip reload if unchanged) |
| `-$8610` / `-$860F` | **Map party X / Y** (tile coords; also from packed byte `A4-$A9F4`) |
| `-$864F` | Last raw key character |
| `-$864E` | **New-game flag** (`1` = new game path in `545E` / `52FE`) |
| `-$8617` | Skip some first-time UI if set |
| `-$861B` | “Busy / modal” flag (`53AA` clears it) |
| `-$86B0` | **Exit request** (set on ESC `0x1B` in map loop) |
| `-$85E6` | **Draw context** — passed to tile/text blit (`83E0`) |
| `-$EEF4` | Pointer to **loaded map/event blob** (256-byte slices copied in `256E`) |
| `-$86BA` / `-$86AC` | Sub-mode / encounter flags (`01C38`, `01C58`) |

### 6.2 Engine thunks (`JSR offset(A4)`)

These are **not** Exec vectors; they are filled during `24A74` / `802C` setup.

| Offset | Call count | Likely role |
|--------|------------|-------------|
| `-$83C2` | Very high | **Pump / yield** — wait for frame or input poll |
| `-$83CE` | High | **Set UI mode** (args: `0`, `1`, mode word) |
| `-$83E0` | High | **Draw character/tile** — `(ctx, x, y, glyph)` |
| `-$842E` | Medium | **Read key** → `D0` = ASCII / scan |
| `-$8440` | Medium | **Delay** — arg `0xFA` ticks in map loop |
| `-$8392` / `-$8398` | Low | Map cell `0xFF` / box drawing helpers |
| `-$802C` | Once early | Large **engine init** (`052CA`, `01278`) |
| `-$8140` | Medium | **Print string** (stack arg = ptr) |
| `-$8158` / `-$815E` / `-$8146` | Medium | **Screen setup trio** (graphics/mode) |
| `-$8152` / `-$5D7C` | Branch | Alt path when `-$864E == 1` (new game) |

### 6.3 DOS thunks via `A6 = A4-$342`

| Routine | Code | Exec API |
|---------|------|----------|
| `254BC` | `JMP -$84(A6)` | DOS **Close** (approx) |
| `254CC` | `JMP -$D8(A6)` | DOS **Open** |
| `254F4` | `JMP -$3C(A6)` | DOS **Read** |
| `2567C` | `JMP -$C6(A6)` | **AllocMem** |

---

## Part 7 — Main loop hub (`LAB_1280` @ `0x1280`)

This is the **central scheduler** once the game is running (Capstone ~lines 1770+).

```
1280  poll A4-$AA2A (event/timer byte)
      if < $80 and flag A4-$86AE → refresh helper 82C6; goto 1280
      …
12CA  wait for key:
        if A4-$861E == 0 → 83C2 + 82F6 (wait)
        else → 842E (get key) → D0
12EA  dispatch key in D0 via 8488 → handler index
      if A4-$86B0 (ESC) → 82C0 cleanup
      83C2 pump
      jump table → LAB_14AA + (index * 4)
```

**Menu keys** (from `108A`–`10B8`): **`C` / `M` / `O` / `P` / `Q`** branch to `LAB_12B0` (party / magic / options / etc. — classic MM hotkeys).

**`LAB_12B0` → `151A`**: large switch for in-game commands (inventory, rest, …).

**`0127C` / `02566`**: call **`LAB_545E`** — (re)initialize play session / UI after load or map exit.

---

## Part 8 — Screen loader (`LAB_256E` @ `0x256E`)

Called from **`01C82`** when the **screen ID** at `A4-$860E` changes (UI state machine in `LAB_1Bxx`).

```
256E  if A4-$860E == A4-$861A → return (no reload)
      if A4-$EEF4 == 0 → branch 2768 (empty)
      save new ID to A4-$861A
      copy 0x100 bytes: (A4-$EEF4 + fn(ID)) → buffer A4-$AB46
      copy 0x100 bytes: second slice → A4-$AA46
      … more slices for other tables (A4-$AC46, A4-$A9EB, …)
      → eventually builds tile/map tables for new screen
28F2  return path when ID unchanged at entry
```

So **`-$EEF4`** is the base of **per-location game data** (events + map layer), indexed by **`screen_id * 0x200 + 0x100`**.

---

## Part 9 — Overland map view (`LAB_24C4` @ `0x24C4`)

Nested **16×16** loops draw the **world map** into the playfield.

| Piece | Behaviour |
|-------|-----------|
| Position | `A4-$8610` / `-$860F` + frame offsets on stack |
| Tile source | `A4-$AA46` indexed by `(row<<4)+col` |
| Draw | `JSR -$83E0(A4)` with `A4-$85E6` context |
| Movement keys | `A4-$864F`: **`N`→$20, `S`→$21, `E`→$22, `W`→$23** (`2456`–`2476`) |
| Idle | `0xFF` cell → `8392` “empty” glyph; else tile + optional overlay `8398` |
| Loop tail | `8440` delay `0xFA`; `842E` poll key; **`ESC (0x1B)`** sets `A4-$86B0` and exits |
| After exit | **`02566` → `LAB_545E`** (session refresh) |

This matches **overland navigation** in Might & Magic II (keyboard, not mouse).

---

## Part 10 — New game / party setup (`0x51F0`–`0x532C`)

**`LAB_51EA` region** (watch for misaligned strings in the listing — `"Encounter!"` at `532E`).

| Phase | Code | Purpose |
|-------|------|---------|
| Roster copy | `520E`–`5236` | Copy **8 words** from `A4-$8696` → `A4-$A1A2` (party slots) |
| Per-character | `524C`–`5280` | `JSR 47A6` / `449E` — load character record; copy bytes `+0x27`, `+0x15` (stats?) |
| Reverse copy | `5292`–`52BA` | Copy 8 words back; save count at `A4-$86A6` |
| Engine | `52C4`–`52EC` | `50A8`, **`802C`**, mode **`83C2`**, **`BSR 533A`** (format name tables), screen trio |
| Branch | `52F0` | If `A4-$864E==1` → `8152` else **`5D7C`** (continue game load) |
| Cleanup | `5302`–`532C` | `83CE` + `83C2`; clear **`A4-$8663`–`$8667`** (input state) |

**`LAB_533A`**: builds **four lookup bytes** from tables at `A4-$8BBF`…`$8BA4` and calls **`4304`** — likely **party member name / class string** for UI.

**`LAB_545E`**: “start play” — `83C2`, optional **`533A`+`8158`** vs **`83CE`+`533A`**, then **`53C8`** (mode 3 → maybe **`463C`** disk check → **`28F6`** title), **`5D7C`/`8152`**, final `83CE`/`83C2`.

---

## Part 11 — Command-line tokenizer (`LAB_24A88` @ `0x24A88`)

Registered as a **callback** (`24A5E` / `254E6`). Parses a **shell-style argv**:

- Skips spaces, tabs, CR, LF (`24B44`–`24B62`).
- **`"` quoted strings** (`24B66`–`24B9E`).
- Builds **argv pointer table** at `A4-$FCCC` via `AllocMem` (`2567C`).
- Uses **`24C7A`** (`memcpy`) and **`24C56`** (append substring).

Used for **debug/console commands** or script hooks, not the main game loop.

---

## Part 12 — Embedded data catalogue (code hunk ~`0x230`–`0x380`)

ASCII filenames and UI strings stored as data (use IRA/Capstone skipdata regions):

| Offset | String |
|--------|--------|
| `0x35E` | `map.dat` |
| `0x366` | `roster.dat` |
| `0x356` | `str.dat` |
| `0x25A` | `event.dat` |
| `0x230` | `disk.32` |
| `0x152C` | `Quit without game save (y/n)?` |
| `0x21295` | `Check disks and try again` |
| `0x231DE` | `save_buf failed` |
| `0xE138`+ | Item/inventory messages (“Already have weapon”, …) |

Loading uses **DOS Open/Read** thunks + engine buffers — search for pushes of these addresses in a future pass.

---

## Quick reference — control-flow graph (updated)

```
JMP 248AE
  → A4 = 7FFE, MANX pool, OpenLibrary(dos)
  → JSR 24928: AllocMem, MANX block, JSR 802C/7B48
  → RTS to DOS (loader); game continues via A4 thunks

… later, engine running …

LAB_1280  ← main hub (keys, menus, poll)
  → LAB_12B0 / LAB_151A  (commands C/M/O/P/Q…)
  → LAB_1Cxx  (screen ID change → LAB_256E)
  → LAB_24C4  (overland map + N/S/E/W, ESC → LAB_545E)
  → LAB_545E  (session / new game UI)
  → LAB_51xx  (party roster setup)
```

---

## Part 13 — Suggested next traces

| Topic | Start at |
|-------|----------|
| Combat / encounter | `53C8` → `28F6`, strings `"Encounter!"` |
| Disk load `map.dat` | Xrefs to `0x35E`, `254CC`/`254F4` |
| `A4` thunk init table | Break on `24A74`, dump memory below `$7FFE` |
| Overlay segment | `28208`, `28E3A` (shutdown lists at `294xx`) |

---

## Files in this repo

| File | Purpose |
|------|---------|
| `mm2` | Amiga hunk executable |
| `EXTRACTED/mm2.capstone.asm` | Capstone listing (this doc) |
| `EXTRACTED/mm2.asm` | IRA listing with labels (often clearer for data) |
| `EXTRACTED/ghidra/mm2_code_*.bin` | Flat binaries for Ghidra |
| `tools/disasm_m68k.py` | Regenerate capstone asm |
| `tools/RE-TOOLS.md` | Tooling / Ghidra import |

## Key docs index (EXTRACTED/docs/)

| Doc | Topic |
|-----|-------|
| `06-event-dat-format.md` | event.dat container (71 locations, header, script layout) |
| `07-event-script-opcodes.md` | opcode table 0x00–0x32 |
| `15-3d-view-and-game-screen.md` | 3D hood, auto-map, map.dat visual/collision pages |
| `16-monster-ability-format.md` | monsters.dat 26-byte record, Pabil/Sabil/Oabil; FAQ monster table appendix |
| `17-combat-system.md` | round loop, player/monster turn, XP/flee/frenzy rules (FAQ §3-8 notes) |
| `18-items-dat-format.md` | items.dat 20-byte record, forbidden-class mask, bonus nibble, use-power |
| `19-spells-and-item-use.md` | spells.dat 2-byte record, item use-power encoding |
| `28-town-services.md` | OP_0E dispatch, per-town service matrix, training/healing formulas, portal link |
| `31-spell-sources.md` | all 96 spells — where to acquire; world-grant cross-check vs FAQ §3-2-2 |
| `32-character-mechanics.md` | Race modifiers, HP/SP/AC formulas, XP tables, extra attacks, thievery (FAQ-sourced) |
| `33-skills-and-hirelings.md` | 15-skill table with sellers/coords/cost; 24-hireling roster A-X (FAQ-sourced) |
| `34-commerce-formulas.md` | Training/healing costs, day-bonus cycle, bar food/drinks, portals, Fly coords (FAQ-sourced) |

---

*Continue with Part 13 (combat, `map.dat` loader, or `A4` thunk table dump) when you pick a focus.*
