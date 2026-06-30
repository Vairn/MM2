# Data Hunk Reference — `mm2_data_00.bin`

Annotated reference for the engine's **initialized data hunk**,
`EXTRACTED/ghidra/mm2_data_00.bin` (also mirrored at
`EXTRACTED/hunks/mm2_data_00.bin`). This is the static, on-disk image of the
low part of MM2's global game-state RAM block — the region addressed through
`A4` — plus the engine's **library jump table** that lives in the same hunk.

This doc consolidates and extends:

- [`14-game-state-struct.md`](14-game-state-struct.md) — the `A4` field decode
  (the backbone; this doc does not contradict it, it grounds it in the bytes).
- [`02-runtime-memory-map.md`](02-runtime-memory-map.md) — memory map + thunks.
- [`13-time-era-calendar.md`](13-time-era-calendar.md) — calendar tables.
- [`07-event-script-opcodes.md`](07-event-script-opcodes.md) — the opcode-length
  table consumer.

Every value below was read directly from the bin and cross-checked against
`EXTRACTED/mm2.capstone.annotated.asm`. Confidence is marked **Confirmed**
(byte-verified + ASM-traced), **Inferred** (derived but not fully traced), or
**Unknown**.

---

## 1. Overview

| Property | Value |
|----------|-------|
| File | `EXTRACTED/ghidra/mm2_data_00.bin` |
| Size | **8604 bytes** = `0x219C` |
| Origin | Ghidra-exported **DATA hunk 0** of the MM2 Amiga executable (the initialized-data segment loaded by the AmigaDOS hunk loader). |
| Loads at | Absolute address **0** (hunk base 0). |
| Endianness | **Big-endian** (Motorola 68000). Unlike the `.dat` files (little-endian on disk), every multi-byte field in this hunk is big-endian. |
| Contains | (a) the engine **library jump table** (`JSR d(A4)` thunks), and (b) the **initialized portion** of the `A4` global game-state block. |

### `A4`, displacements, and the `file_offset == EA` mapping

At startup the engine runs (asm `0x24920`, `set_a4_workspace`):

```text
024920  49f9 0000 7ffe   LEA.L $7FFE, A4      ; A4 = $7FFE, never reloaded
```

So a field referenced as `disp(A4)` has **effective address** `EA = 0x7FFE + disp`
(`disp` is a signed 16-bit displacement). Because the hunk loads at address 0,
the **file offset inside `mm2_data_00.bin` equals the EA**:

```text
file_offset(field) == EA(field) == 0x7FFE + disp
```

**Confirmed** by spot-checking the night-sky star tables:

| Field | disp | EA = file offset | Bytes read (big-endian words) |
|-------|------|------------------|-------------------------------|
| star X | `-$73A8` | `0x0C56` | `{3,9,13,13,2,4,9,12,2,4,9,13,3,5,7,10}` ✓ |
| star Y | `-$7388` | `0x0C76` | `{30,53,11,12,23,54,56,20,55,22,50,10,37,18,25,45}` ✓ |

Both match the expected values exactly, so `file_offset == EA` holds.

### The file ends where RAM/BSS begins

The hunk is exactly `0x219C` bytes. `0x219C` is the EA of `manx_pool`
(`-$5E62`, the MANX/C-runtime arena base — see doc 14). **Everything at
EA ≥ `0x219C` is uninitialized RAM (BSS) and is *not* present in this file.**

That means the large doc-14 buffers — `party_slots` (`0x21A0`),
`event_work_buf` (`0x3836`), the found-item buffer (`0x40E2`), `roster_base`
(`0x55C0`), `map_blob`/`event_blob` (`0x6EF2`/`0x6EF6`), the encounter slots
(`0xEE22`), etc. — live **beyond** this file. This doc decodes only what is
actually stored in the bin (EA `0x0000`–`0x219B`); BSS fields are cross-linked
to doc 14 but not "decoded from bytes" because there are no bytes here.

---

## 2. Conventions

### Two displacement notations

| Notation | Example | Meaning |
|----------|---------|---------|
| **True signed offset** (Capstone `-$7951(a4)`, IRA `-31057(A4)`) | `-$7951` | The actual `d16` displacement. **Canonical.** |
| **Raw displacement word** (older docs, `mm2_gamestate.h` `(...)` comments) | `$86AF` | The encoded 16-bit word = `disp & 0xFFFF`. |

Conversion: `word = disp & 0xFFFF`; `disp = word - 0x10000` (when `word ≥ 0x8000`).
EA: `EA = 0x7FFE + disp`. Example: `-$7951` ⇄ word `$86AF` ⇄ EA `$06AD`.

> Note: the parenthetical hex in `EXTRACTED/decomp/mm2_gamestate.h`
> (e.g. `($860E)`, `($C0E4)`) is the **raw word**, *not* the EA. To get a file
> offset from those, subtract `0x10000` then add `0x7FFE` (or just compute
> `0x7FFE + disp`).

### Endianness

All multi-byte fields are **big-endian** (68000). Word at file offset `f` =
`(bin[f] << 8) | bin[f+1]`. The accessors in `mm2_gamestate.h`
(`mm2_gs_u16/u32`) read big-endian regardless of host.

### How to read a field

1. Take the displacement `disp` (e.g. `-$711C`).
2. `EA = 0x7FFE + disp` → that is the file offset.
3. Read `type` bytes big-endian.

---

## 3. Memory regions

The hunk has two distinct regions:

| File range (= EA) | Region | Contents |
|-------------------|--------|----------|
| `0x0000`–`~0x05A6` | **Library jump table** | ~237 `JMP`/`BSR` stubs reached by `JSR d(A4)`. **Not data.** See §6. |
| `~0x05A8`–`0x219B` | **Initialized game state** | scalars/flags, calendar arrays, lookup-table bases, static decode tables. See §4–§5. |
| `0x219C`+ | *(not in file)* | BSS / runtime RAM (party, roster, event work buffer, blobs). See doc 14. |

The boundary at `~0x05A7/0x05A8` is **Confirmed** from the bytes: the last
thunk stubs (`4E F9 00 02 47 92`, a `BSR` pair `61 00 …`) sit at `0x0588`–`0x05A6`,
and the first game-state byte read by the engine (`star_color`, `-$7A52`) is at
`0x05AC`.

A4's far-negative displacements (`-$7B48`, `-$7C3E`, `-$7F20`, `-$7FF8`, …) all
land in the **low** file offsets `0x0006`–`0x0590` — i.e. they index the jump
table, not data. Confirmed data fields sit at EA `~$05A8`–`$219B` in-file (and
the rest of the `A4` struct in BSS up to `$6EF6`).

---

## 4. Field catalogue (in-file scalars & arrays)

Offsets/types reconciled with doc 14 and `mm2_gamestate.h`; the **EA (= file
offset)** column is recomputed from the bytes. Only fields that physically live
in this bin (EA `< 0x219C`) are listed; BSS fields are in doc 14.

`init` = the static value actually stored in the bin (big-endian), where
meaningful.

### Session / control

| disp | raw word | EA = file off | type | field | init | notes |
|------|----------|---------------|------|-------|------|-------|
| `-$7A1A` | `$85E6` | `0x05E4` | ptr | `draw_ctx` | `0x00000000` | draw-context pointer (filled at runtime). |
| `-$79F2` | `$860E` | `0x060C` | byte | `screen_mode_id` | — | screen/mode selector. |
| `-$79F1` | `$860F` | `0x060D` | byte | `coord_b` | — | map/player coord. |
| `-$79F0` | `$8610` | `0x060E` | byte | `coord_a` | — | map coord (nibble). |
| `-$79EA` | `$8616` | `0x0614` | byte | `script_abort` | — | event abort flag. |
| `-$79E9` | `$8617` | `0x0615` | byte | `first_time_flag` | — | first-time/modal control. |
| `-$79E6` | `$861A` | `0x0618` | byte | `screen_mode_prev` | — | previous screen id. |
| `-$79E5` | `$861B` | `0x0619` | byte | `busy_status` | — | busy/modal latch. |
| `-$79B2` | `$864E` | `0x064C` | byte | `new_game_flag` | — | right-panel mode. |
| `-$79B1` | `$864F` | `0x064D` | byte | `last_move_key` | — | `N/S/E/W` movement key. |
| `-$7956` | `$86AA` | `0x06A8` | word | `event_parse_pos` | — | event-script cursor. |
| `-$7954` | `$86AC` | `0x06AA` | word | `event_script_anchor` | — | string-table base; `$FFFF` = init. |
| `-$7951` | `$86AF` | `0x06AD` | byte | `cond_flag` | — | event predicate result. |
| `-$7950` | `$86B0` | `0x06AE` | byte | `exit_flags` | — | ESC/exit bit flags. |

### Static star-field / color bytes (night-sky renderer)

| disp | EA = file off | type | field | init | notes |
|------|---------------|------|-------|------|-------|
| `-$7A52` | `0x05AC` | byte | `star_color_alt` | `1` | alt pen, night-sky loop @ `0x6966`. |
| `-$7A4D` | `0x05B1` | byte | `star_color` | `20` (`0x14`) | base star pen, `0x68AA`/`0x6972`. |

### Calendar / era (see [`13-time-era-calendar.md`](13-time-era-calendar.md))

| disp | raw word | EA = file off | type | field | init | notes |
|------|----------|---------------|------|-------|------|-------|
| `-$79DE` | `$8622` | `0x0620` | word[10] | `day[era]` | all `1` | day-of-year per era 0..9. |
| `-$79CA` | `$8636` | `0x0634` | word[10] | `year[era]` | `{0,100,200,300,400,500,600,700,800,900}` | year per era (cap 999). |
| `-$79B8` | `$8648` | `0x0646` | word | `script_counter` | — | OP_2C global counter (`-$79B8`). |
| `-$79B6` | `$864A` | `0x0648` | word | `era` | `9` | timeline index 0..9. |
| `-$79B5` | `$864B` | `0x0649` | byte | `era_low` | `9` | low byte of `era` (event gating reads this). |
| `-$79B4` | `$864C` | `0x064A` | word | `time_subday` | — | sub-day accumulator (256 = 1 day). |

> The static image ships with `era = 9`, `day[*] = 1`, and `year[era]` pre-seeded
> at `era*100` (`0,100,…,900`). These are the compile-time initializers in the
> hunk; the new-game / load paths overwrite them at runtime. **Confirmed** (bytes
> read at `0x0620`/`0x0634`/`0x0648`).

### Lookup-table bases (in-file)

| disp | raw word | EA = file off | type | field | notes |
|------|----------|---------------|------|-------|-------|
| `-$796A` | `$8696` | `0x0694` | word[…] | `roster_index_tbl` | party→roster slot map. Init `{0,1,2,3,4,5,FFFF,FFFF,6,0,FFFF,FFFF,0x0100,0,0,0}`. |
| `-$7928` | `$86D8` | `0x06D6` | long[8] | `class_name_tbl` | 8 class-name string pointers (see §5). |
| `-$7908` | `$86F8` | `0x06F6` | long[5] | `race_name_tbl` | 5 race-name string pointers. |
| `-$78F4` | `$870C` | `0x070A` | long[3] | `align_name_tbl` | 3 alignment-name string pointers. |
| `-$73A8` | `$8C58` | `0x0C56` | word[16] | `star_x_tbl` | night-sky X positions (see §5). |
| `-$7388` | `$8C78` | `0x0C76` | word[16] | `star_y_tbl` | night-sky Y positions. |
| `-$711C` | `$8EE4` | `0x0EE2` | word[13] | `month_tbl` | month-boundary days-of-year. |
| `-$7102` | `$8EFE` | `0x0EFC` | byte[13] | `season_tbl_a` | per-month value → `-$79F2`. |
| `-$70F5` | `$8F0B` | `0x0F09` | byte[13] | `season_tbl_b` | per-month nibble pair → `-$79F1`/`-$79F0`. |
| `-$6CC8` | `$9338` | `0x1336` | word[51] | `opcode_len_tbl` | event-script token lengths (`0x00..0x32`). |
| `-$6BE6` | `$941A` | `0x1418` | byte[8] | `context_mask_tbl` | facing → direction mask. |

> **EA correction (flagged, not a contradiction of doc 14's intent):** doc 14
> lists `context_mask_tbl` (`-$6BE6`) at EA `$1428`. The arithmetic
> `0x7FFE - 0x6BE6` and the bytes both give **`0x1418`**; the table is
> immediately after the opcode-length table. Treat `$1418` as the byte-verified
> EA. (All other doc-14 EAs reconciled exactly.)

---

## 5. Notable static data tables decoded from the bin

All values below were read from the file and the **consumer ASM** was located.

### 5.1 Night-sky star tables (`star_x_tbl` / `star_y_tbl`)

- `star_x_tbl` @ `0x0C56` = 16 big-endian words:
  `{3,9,13,13,2,4,9,12,2,4,9,13,3,5,7,10}`
- `star_y_tbl` @ `0x0C76` = 16 big-endian words:
  `{30,53,11,12,23,54,56,20,55,22,50,10,37,18,25,45}`
- pens: `star_color = 20` (`-$7A4D`), `star_color_alt = 1` (`-$7A52`).

**Consumer (Confirmed):** the night-sky / compass renderer @ `0x68AA`–`0x6980`.
It picks a base from the facing key `-$79B1` (`'N'`=0, `'W'`=1, `'S'`=2) ×4, adds
a per-star counter, doubles it for the word stride, and indexes both tables:

```text
0068f6  lea.l  -$73a8(a4), a0      ; star_x_tbl
0068fa  move.w (a0,d0.l), -$6(a5)  ; star_x[i]
00690c  lea.l  -$7388(a4), a0      ; star_y_tbl
006910  move.w (a0,d0.l), -$8(a5)  ; star_y[i]
...     toggles pen between -$7A4D and -$7A52 per star
```

The 16 entries = 4 facing groups × 4 stars (Inferred from the `×4` base + the
`cmpi #$10` wrap @ `0x694E`).

### 5.2 Event-script opcode-length table (`opcode_len_tbl`)

@ `0x1336`, **51 big-endian words** (one per opcode `0x00..0x32`):

```
idx 00..0F: 0  2  2  2  2  2  2  1  1  1  1  3  3  2  2  1
idx 10..1F: 2  2 13 11  1  4  3  3  5  5  3  2  2  2  2  7
idx 20..2F: 7  4  3  3  3  2  1  1  3  1 15  2  2  3  3  1
idx 30..32: 11  4  2
```

**Consumer (Confirmed):** the IF/ELSE token-skip helper @ `0x157FC` (used by
`OP_10`/`OP_11`/`OP_2B`). It reads a token byte from the decoded script buffer
(`-$47C8`), doubles it, indexes this table, and advances the parse cursor
(`-$7956`) by the resulting word:

```text
015816  move.b (a0,d0.w), d1       ; token (opcode) from event_work_buf
01581a  asl.l  #$1, d1             ; ×2 (word index)
01581c  lea.l  -$6cc8(a4), a0      ; opcode_len_tbl
015820  move.w (a0,d1.l), d0       ; length delta
015824  add.w  d0, -$7956(a4)      ; advance parse cursor
```

So `opcode_len_tbl[op]` = the **total token size** (opcode byte + inline args).
This **cross-validates the `argc` table in doc 07** byte-exactly for the
fixed-width opcodes — e.g. `OP_12`→13 (argc 12), `OP_13`→11 (argc 10),
`OP_15`→4 (argc 3), `OP_18`→5 (argc 4), `OP_1F`/`OP_20`→7 (argc 6),
`OP_2A`→15 (argc 14), `OP_30`→11 (argc 10), `OP_32`→2 (argc 1). `OP_00`→0 is
the invalid/terminator slot.

> Minor flag (Inferred): a couple of variable/predicate opcodes differ from
> doc 07's `argc` column by one (e.g. `OP_25` length `2` here vs argc `2`
> there). These are the variable-length / predicate ops where the token-skip
> length and the runtime read count need not match; doc 07 already marks them
> Partial. The fixed-width opcodes all agree.

### 5.3 Facing → context-mask table (`context_mask_tbl`)

@ `0x1418`, **8 bytes**: `{0x10, 0x00, 0x20, 0x00, 0x40, 0x00, 0x80, 0x00}`.

**Consumer (Confirmed):** the tile-event scanner @ `0x175FE` indexes it by the
facing index `-$55D7` (values `0/2/4/6` = N/E/S/W):

```text
0175fa  move.b -$55d7(a4), d0      ; facing_index (0/2/4/6)
0175fe  lea.l  -$6be6(a4), a0      ; context_mask_tbl
017602  move.b (a0,d0.l), -$4(a5)  ; direction mask
```

So effective masks are **N=`0x10`, E=`0x20`, S=`0x40`, W=`0x80`** (odd slots are
unused `0x00`). This mask is AND-ed with each event triplet's `cond` byte to gate
direction-sensitive triggers (see doc 07 §"Cond byte").

### 5.4 Calendar tables (`month_tbl` / `season_tbl_a` / `season_tbl_b`)

- `month_tbl` @ `0x0EE2` = **13 big-endian words**:
  `{20,40,60,80,93,94,100,120,130,140,150,181,255}` — month-boundary days.
- `season_tbl_a` @ `0x0EFC` = **13 bytes**:
  `{11,14,33,9,37,8,37,6,14,39,8,11,11}`.
- `season_tbl_b` @ `0x0F09` = **13 bytes**:
  `{0xB5,0x61,0x74,0x2C,0x55,0x65,0x55,0xD4,0x0F,0x73,0xEC,0x37,0xB8}`
  (each byte packs two nibbles).

**Consumer (Confirmed):** the month/season derivation @ `0x0B12C`–`0x0B19E`.
It walks `month_tbl` counting how many boundaries the current day-of-year exceeds
(`cmpi #$d` = 13 entries), giving a month index, then:

```text
00b16e  lea.l  -$7102(a4), a0          ; season_tbl_a
00b172  move.b (a0,d0.w), -$79f2(a4)   ; -> screen/mode byte
00b17c  lea.l  -$70f5(a4), a0          ; season_tbl_b
00b180  move.b (a0,d0.w), d1
00b184  and.b  #$f, d1                 ; low nibble
00b188  move.b d1, -$79f1(a4)
...     lsr.b #4 -> high nibble -> -$79f0(a4)
```

So the month index selects one byte from each table; `season_tbl_b`'s low/high
nibbles feed `-$79F1`/`-$79F0`. **Open item (flagged):** doc 14 labels the
destinations `-$79F2/-$79F1/-$79F0` as session screen/coord bytes; this routine
also writes them from the season tables. The tables themselves are byte-verified;
the dual-use of those three destination bytes is noted here for reconciliation
and is **not** asserted to override doc 14.

### 5.5 Class / race / alignment name-pointer tables

These are arrays of **32-bit pointers** (one per id), indexed by character-record
fields and dereferenced to draw the name string. **Consumer (Confirmed)** in the
character-sheet formatter @ `0x3A24`–`0x3A92`:

```text
003a2a  move.b $6a(a0), d0   ; record+0x6A = alignment id
003a30  lea.l  -$78f4(a4), a0 ; align_name_tbl ; (a0,d0*4) = char* name
003a5c  move.b $0e(a0), d0   ; record+0x0E = race id  -> -$7908
003a80  move.b $0f(a0), d0   ; record+0x0F = class id -> -$7928
```

Stored pointer values (big-endian longs, **unrelocated** offsets — the strings
themselves are in another hunk, not this bin):

| table | EA | entries | stored long values |
|-------|----|---------|--------------------|
| `class_name_tbl` | `0x06D6` | **8** | `70 77 7F 86 8D 96 9D A3` |
| `race_name_tbl` | `0x06F6` | **5** | `AE B4 B8 BE C4` |
| `align_name_tbl` | `0x070A` | **3** | `CA CF D7` |

**Validation (Confirmed):** the deltas between consecutive class pointers are
`7,8,7,7,9,7,6`, which equal the null-terminated lengths of
`Knight`/`Paladin`/`Archer`/`Cleric`/`Sorcerer`/`Robber`/`Ninja` (6+1, 7+1,
6+1, 6+1, 8+1, 6+1, 5+1) — confirming **8 classes** in the order
Knight, Paladin, Archer, Cleric, Sorcerer, Robber, Ninja, Barbarian; **5 races**
(Human, Elf, Dwarf, Gnome, Half-Orc — Inferred from 5 entries); **3 alignments**
(Good, Neutral, Evil). The pointer *targets* are not resolvable from this file
alone (they relocate to a string hunk).

---

## 6. Library jump-table / thunk region (`0x0000`–`~0x05A6`)

The low part of the hunk is **not data** — it is a table of call stubs reached by
`JSR d(A4)` (opcode `4EAC`). Each stub is 6 bytes, almost all `4E F9 hh hh ll ll`
(`JMP absolute`), with a few `61 00 …` (`BSR`) forms. There are **237** `4E F9`
stubs in `0x0000`–`0x0598` (matching the "~235 thunks" catalogued by
`tools/scan_a4_jsr.py`). The first slot is `4E F9 00 00 00 00` (`JMP $0`, off
`0x0000` = `-$7FFE`, effectively null).

The full offset → target map is in
[`EXTRACTED/tmp_mm2_thunk_map.txt`](../tmp_mm2_thunk_map.txt) (regenerated by
`tools/scan_a4_jsr.py`); names are in `EXTRACTED/mm2_symbols.yaml`
(`a4_thunks:`). Targets are **Confirmed** (decoded from the stub bytes);
the names range from Confirmed to Inferred.

Representative thunks (offset = EA = file offset):

| disp | EA = file off | stub | target | name / role | conf. |
|------|---------------|------|--------|-------------|-------|
| `-$7FF8` | `0x0006` | `4EF9` JMP | `0x0036A` | early init helper | Inferred |
| `-$7FD4` | `0x002A` | `4EF9` JMP | `0x01D0A` | `walk_beep` (footstep) | Inferred |
| `-$7F20` | `0x00DE` | `4EF9` JMP | `0x0477E` | `get_party_member_ptr` (×22 calls) | Confirmed |
| `-$7EDE` | `0x0120` | `4EF9` JMP | `0x051C2` | combat-engine entry (OP_12/13 `jsr -$7EDE`) | Confirmed |
| `-$7E42` | `0x01BC` | `4EF9` JMP | `0x06FB8` | OP_0D canned on-screen sequence | Confirmed |
| `-$7DFA` | `0x0204` | `4EF9` JMP | `0x092F2` | `event_dat_loader` | Confirmed |
| `-$7D3A` | `0x02C4` | `4EF9` JMP | `0x175E2` | `event_tile_scanner` thunk | Confirmed |
| `-$7C62` | `0x039C` | `4EF9` JMP | `0x218EA` | draw-char/glyph (×92 calls) | Inferred |
| `-$7BE4` | `0x041A` | `4EF9` JMP | `0x22376` | print-string helper (×73 calls) | Inferred |
| `-$7BDE` | `0x0420` | `4EF9` JMP | `0x22480` | `play_tone` | Inferred |
| `-$7BD2` | `0x042C` | `4EF9` JMP | `0x22586` | spell/UI tone (×8 calls) | Inferred |
| `-$7BB4` | `0x044A` | `4EF9` JMP | `0x22BC6` | `rng_roll` (1..max) | Inferred |
| `-$7A6E` | `0x0590` | `61 00` BSR | `0xE0100`* | last/high stub (BSR form) | Inferred |

\* `-$7A6E`'s stub is `61 00 00 0E 01 00 02 42 …`; the BSR form resolves
differently from the `4EF9` JMP stubs — see the thunk map entry.

Note: some opcodes (`OP_1C/1D/1E/31`) instead call through **runtime** function
pointers stored *in the data region* (`A4-$7Bxx/-$7Exx/-$7Fxx`, populated at
engine init), not these static stubs — see doc 07 §"Runtime A4 function
pointers". Those slots are zero in this static image.

---

## 7. Regeneration / tooling

| Artifact | Command / source |
|----------|------------------|
| The bin | Ghidra export of executable DATA hunk 0 → `EXTRACTED/ghidra/mm2_data_00.bin`. |
| Data-offset catalogue | `python tools/scan_a4_state.py` → `EXTRACTED/a4_state_scan.{txt,json}` (905 offsets: access counts, sizes, `lea` bases, mnemonics). |
| Thunk map | `python tools/scan_a4_jsr.py` → `EXTRACTED/tmp_mm2_thunk_map.txt` (235 thunk offsets → targets). |
| Field constants / codec | `EXTRACTED/decomp/mm2_gamestate.h`, `tools/mm2_gamestate.py` (big-endian accessors; `mm2_gs_base_from_image(image)` = `image + 0x7FFE`). |
| Symbols | `EXTRACTED/mm2_symbols.yaml` (`a4_offsets:` / `a4_thunks:` / `functions:`). |
| Cross-check | `EXTRACTED/mm2.capstone.annotated.asm`. |

To read a table from the bin: `EA = 0x7FFE + disp`, then read big-endian at that
file offset. (`mm2_gs_base_from_image` gives a base pointer such that
`base[disp]` works directly.)

---

## 8. Open / unknown regions

The bin is `0x219C` bytes; §4–§5 account for the named fields and tables, but
large spans of the initialized data region remain **unclassified**:

- **`~0x05A8`–`0x060B`** — a block of longs/words just after the jump table
  (`draw_ctx` and neighbours, plus the star-color bytes). Many are pointer/long
  slots that are zero at rest and filled at runtime (`scan_a4_state.py` marks
  them `l` with `clr/move/tst`); their precise roles are only partially traced.
- **`0x0715`–`0x0C55`** — between the name-pointer tables and the star tables.
  Largely unexamined here.
- **`0x0C96`–`0x0EE1`** — between the star tables and the calendar tables.
- **`0x0F16`–`0x1335`** — between the calendar tables and the opcode-length
  table.
- **`0x1420`–`0x219B`** — the bulk of the upper data region after
  `context_mask_tbl`. This contains additional flag banks, the talisman/quest
  byte banks, the event variable bank (`-$798B`), and many engine scalars listed
  in doc 14, but most have not been **byte-decoded** in this pass.

Other explicitly-flagged items:

- **`context_mask_tbl` EA** is `0x1418` (byte-verified), not `0x1428` as printed
  in doc 14 (§4).
- **Season-table destinations** `-$79F2/-$79F1/-$79F0` are written both by the
  session/coord code (doc 14) and by the calendar routine `0x0B12C` (§5.4) — the
  dual-use is unresolved.
- **Name-pointer targets** (class/race/align strings) relocate into another hunk
  and cannot be resolved from this file alone.
- **Thunk names** beyond the basic engine frame are Inferred; only targets are
  byte-confirmed.

For the full machine-readable enumeration of every offset (including the
unclassified spans), use `EXTRACTED/a4_state_scan.json`.
