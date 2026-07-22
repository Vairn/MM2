# Global Game-State Struct (`A4` workspace)

MM2 keeps almost all mutable runtime state in **one contiguous RAM block**
anchored by address register **`A4`**. At startup the engine executes

```text
024920  49f9 0000 7ffe   LEA.L $7FFE, A4      ; A4 = $7FFE, never reloaded
```

so every `d(A4)` operand in the disassembly is a field at a **fixed offset**
from that anchor. Conceptually this is a single C struct and `A4` is its base
pointer (`gs->field` ≡ `disp(A4)`). This document decodes the data fields; the
companion C/Python implementations live at
`EXTRACTED/decomp/mm2_gamestate.h` and `tools/mm2_gamestate.py`.

> `A4` is **dual-purpose**: the far-negative range (`-$7B48`, `-$7C3E`,
> `-$7F20`, `-$7FF8`, …) is a *library jump table* reached by `JSR d(A4)`, not
> data. Those 235 thunk offsets are catalogued by `tools/scan_a4_jsr.py`. This
> doc covers only the **data** fields (905 distinct offsets found by
> `tools/scan_a4_state.py`).

## Two offset notations (important)

68000 `d(An)` displacements are **signed 16-bit** (`-$8000..+$7FFF`). Existing
docs use two different conventions for the same field, which caused real
mislabels:

| Notation | Example | Meaning |
|----------|---------|---------|
| **True signed offset** (IRA `-31057(A4)`, Capstone `-$7951(a4)`) | `-$7951` | The actual displacement. **Canonical here.** |
| **Raw displacement word** (older docs `A4-$86AF`) | `$86AF` | The encoded word `= 0x10000 + offset`. |

Conversion: `word = offset & 0xFFFF`; `offset = word - 0x10000` (when `word ≥ 0x8000`).
Absolute address: `EA = 0x7FFE + offset`. Example: `-$7951` ⇄ word `$86AF` ⇄ EA `$06AD`.

All confirmed fields sit in low memory **EA `$05E4`–`$6EF6`** — one block.

## Confirmed fields (ASM-verified)

Sizes/access columns are from `tools/scan_a4_state.py`. "doc word" is the
legacy raw-word alias so older notes cross-reference.

### Session / control

| Offset | doc word | EA | Type | Field | Notes (ASM) |
|--------|----------|----|------|-------|-------------|
| `-$79F2` | `$860E` | `$060C` | byte | `screen_mode_id` | set `#$ff`; `cmpi #$29/#$2c`; mode selector. |
| `-$79F1` | `$860F` | `$060D` | byte | `coord_b` | paired with `$8610` (player/map coord). |
| `-$79F0` | `$8610` | `$060E` | byte | `coord_a` | `andi #$f`, `cmpi #$10/#$d/#$3`; map coord (nibble). |
| `-$79EA` | `$8616` | `$0614` | byte | `script_abort` | set `=1` by event opcodes `0x29`/invalid (`0x1748C`). |
| `-$79E9` | `$8617` | `$0615` | byte | `first_time_flag` | first-time/modal control. |
| `-$79E6` | `$861A` | `$0618` | byte | `screen_mode_prev` | previous screen/mode id. |
| `-$79E5` | `$861B` | `$0619` | byte | `busy_status` | `clr`/`=1` busy/modal latch. |
| `-$79B6` | `$864A` | `$0648` | word | `era` | timeline index 0..9 (see §calendar). |
| `-$79B5` | `$864B` | `$0649` | byte | `era_low` | low byte of the era word `-$79B6` (only ever written via word writes to `era`). Read as the current era by event gating: OP_22 (`0x16A9E`) range-checks it and the dispatch (`0x172BC`) compares it to `attrib.dat` byte `0x0F`. (Formerly mislabeled `cur_event_id`.) |
| `-$79B2` | `$864E` | `$064C` | byte | `new_game_flag` (**stale name — right-panel mode**) | `cmpi #$1/#$2`; **not** a new-game/load flag. `0`=OPTIONS panel, `1`=PROTECT panel, `2`=combat. `MM2_GS_NEW_GAME_FLAG` keeps this stale define name in code; offset/EA are correct. See `show_command_reference` `0x5D54` / protect `0x5E28`. |
| `-$79B1` | `$864F` | `$064D` | byte | `last_move_key` | `cmpi 'N'/'S'/'E'/'W'`; movement/facing key. |
| `-$7956` | `$86AA` | `$06A8` | word | `event_parse_pos` | event-script cursor (`addq #3`). |
| `-$7954` | `$86AC` | `$06AA` | word | `event_script_anchor` | String-table base; `$FFFF` = need init @ 0x1754A. **(EA corrected 2026-07-17: was mislisted `$06A6`, which collides with `event_parse_pos`'s EA — arithmetic `0x7FFE-0x7954=0x6AA`.)** |
| `-$7952` | `$86AE` | `$06AC` | byte | `pending_event_latch` | Set on movement; cleared @ 0x1280 before scanner. **(EA corrected: was mislisted `$06A8`, duplicating `event_parse_pos` — `0x7FFE-0x7952=0x6AC`.)** |
| `-$7951` | `$86AF` | `$06AD` | byte | `cond_flag` | event predicate result; gates `OP_10/11`. |
| `-$7950` | `$86B0` | `$06AE` | byte | `exit_flags` | `bset/btst/clr`; ESC/exit + bit flags. |
| `-$799D`..`-$7999` | `$8663`..`$8667` | `$0661`..`$0665` | byte×5 | `input_state[0..4]` (**stale name — spell-effect counters**) | **Not** a generic "input state" span. Five independent one-shot counters: `-$799D` Bless (`addq` @ `0xBFC4`), `-$799C` Invisibility (`0xB9C4`), `-$799B` Shield (`0xBB5C`; half-damage check `0xFD48`), `-$799A` Power Shield (`0xBEC8`; half-damage `0xFD2C`), `-$7999` Holy Bonus (`0xC320`). Code still names the umbrella `MM2_GS_INPUT_STATE` / `_END` as aliases; prefer `MM2_GS_BLESS_COUNTER` / `INVIS_COUNTER` / `SHIELD_COUNTER` / `POWER_SHIELD_CTR` / `HOLY_BONUS_CTR`. |

### Calendar / era (see `13-time-era-calendar.md`)

| Offset | doc word | EA | Type | Field | Notes |
|--------|----------|----|------|-------|-------|
| `-$79DE` | `$8622` | `$0620` | word[10] | `day[era]` | day-of-year 1..180 per era (`lea` base). |
| `-$79CA` | `$8636` | `$0634` | word[10] | `year[era]` | year per era, cap 999 (`lea` base). |
| `-$79B4` | `$864C` | `$064A` | word | `time_subday` | sub-day accumulator, 256 = 1 day. |
| `-$798C` | `$8674` | `$0672` | byte | `period_flag_a` | cleared at day 60/120/180. |
| `-$798D` | `$8673` | `$0671` | byte | `period_flag_b` | cleared at day 60/120/180. **(EA corrected: was mislisted `$0673`, colliding with `period_flag_a`'s offset+1 — arithmetic `0x7FFE-0x798D=0x671`.)** |

> **Correction:** `13-time-era-calendar.md` listed `-$79F2/-$79F1/-$79F0` as
> "month/season display" and `-$79B1` as a "month/sign byte". The ASM shows
> these are the session screen-id / coordinate / movement-key bytes above. The
> calendar's month/season values are derived on the fly from the day-of-year
> via the word tables at `-$711C` (13 entries), `-$7102`, `-$70F5`
> (routine `0x0B0EA`); they are not those bytes.

### Lookup-table bases (`lea`-addressed arrays)

| Offset | doc word | EA | Type | Field | Notes |
|--------|----------|----|------|-------|-------|
| `-$796A` | `$8696` | `$0694` | word[…] | `roster_index_tbl` | party→roster slot map (81× `lea`). |
| `-$7928` | `$86D8` | `$06D6` | ptr/tbl | `class_name_tbl` | class string table base. |
| `-$7908` | `$86F8` | `$06F6` | ptr/tbl | `race_name_tbl` | race string table base. |
| `-$78F4` | `$870C` | `$070A` | ptr/tbl | `align_name_tbl` | alignment string table base. |
| `-$711C` | `$8EE4` | `$0EE2` | word[13] | `month_tbl` | month boundaries (`cmpi #$d`). |
| `-$7102` | `$8EFE` | `$0EFC` | word[…] | `season_tbl_a` | season/display derivation. |
| `-$70F5` | `$8F0B` | `$0F09` | byte[…] | `season_tbl_b` | season/display derivation. |
| `-$6CC8` | `$9338` | `$1336` | tbl | `opcode_len_tbl` | event-script token length deltas (word indexed). |
| `-$6BE6` | `$941A` | `$1418` | byte[4] | `context_mask_tbl` | Facing index `0/2/4/6` (W/S/E/N) → cond context byte `0x10/0x20/0x40/0x80`. **(EA corrected: was mislisted `$1428`; byte-verified in `mm2_data_00.bin` at file offset `0x1418` — arithmetic `0x7FFE-0x6BE6=0x1418`.)** See [`06-event-dat-format.md`](06-event-dat-format.md) for the condition-flag semantics this table drives. |
| `-$560B` | `$A9F5` | `$29F3` | byte | `attrib_era_gate_cache` | Screen `attrib+0x0F`; compared @ 0x172BC (`102c a9f5` = `move.b -$560b(a4),d0`). |

### Buffers & pointers

| Offset | doc word | EA | Type | Field | Notes |
|--------|----------|----|------|-------|-------|
| `-$7A1A` | `$85E6` | `$05E4` | ptr | `draw_ctx` | draw context pointer. |
| `-$5E62` | `$A19E` | `$219C` | ptr | `manx_pool` | MANX/C-runtime arena base. |
| `-$5E5E` | `$A1A2` | `$21A0` | byte[8] | `party_slots` | 8 active party roster indices. |
| `-$5D46` | `$A2BA` | `$2528` | byte | `queued_event_id` | `$FF` = none; secondary dispatch @ 0x176B6. |
| `-$5D44` | `$A2BC` | `$252A` | word | `string_walk_index` | String resolver walk @ 0x15884. |
| `-$5D42` | `$A2BE` | `$252C` | byte | `saved_cond_flag` (**stale name — selected party slot**) | This byte is the **primary selected party slot (1..8)**, read first by OP_15 member-spec 9 @ `0x163CA`, falling back to `-$5D3F` if unset. The *actual* saved-cond snapshot / party-select mirror is a **different** byte, **`-$5D3F`** (`$A2C1`/`$252F`), written by OP_15 entry. Code: `MM2_GS_SELECTED_MEMBER` (`-0x5D42`) vs `MM2_GS_SAVED_COND_FLAG`/`MM2_GS_PARTY_SELECT_CTX` (`-0x5D3F`, aliases of the same byte). |
| `-$5C44` | `$A3BC` | `$2626` | word | `event_script_start` | Saved at init; restored to parse pos @ 0x17628. |
| `-$55D7` | `$AA29` | `$2A27` | byte | `facing_index` | **`N=6, E=4, S=2, W=0`** (not "0/2/4/6 N/E/S/W" — that ordering was backwards). Set by `map_facing_from_key` @ `0x5636`. Indexes `context_mask_tbl` above. |
| `-$55D6` | `$AA2A` | `$2A28` | byte | `tile_runtime_flags` → **`current_cell_collision_latch`** | **Not** a per-tile array — a single scalar latch for the party's *current* cell (bit7 etc.), set @ `0x1B1C`. Rest-ambush and "Too dangerous!" gating read this one byte, not an indexed table. |
| `-$55C8` | `$AA38` | `$2A36` | byte | `event_busy_sentinel` | `$FF` during active script. |
| `-$55BA` | `$AA46` | `$2A44` | byte[…] | `tile_table_a` | tile source table (drawn). |
| `-$54BA` | `$AB46` | `$2B44` | byte[256] | `tile_visited_flags` → **collision-page copy** | Naming drift: `MM2_GS_TILE_VISITED` in code, but the comment/usage is a **copy of the collision page**, not a visited/event-bit tracker. |
| `-$47C8` | `$B838` | `$3836` | byte[2220] | `event_work_buf` | decoded current-location event buffer. |
| `-$2A3E` | `$D5C2` | `$55C0` | byte[] | `roster_base` | roster records base (stride `$82`). |
| `-$110C` | `$EEF4` | `$6EF2` | ptr | `map_blob` | loaded `map.dat` screen pointer. |
| `-$1108` | `$EEF8` | `$6EF6` | ptr | `event_blob` | loaded `event.dat` blob pointer. |

## Full offset catalogue

The complete machine-readable map of all 905 data offsets (access count,
read/write split, `lea` count, operand sizes, mnemonics, example addresses) is
regenerated by:

```bash
python tools/scan_a4_state.py        # -> EXTRACTED/a4_state_scan.txt + .json
```

Use it to promote additional offsets into the confirmed table above as they
are reverse-engineered. The `lea`/`pea`-dominant offsets are array/struct
bases; high write-count byte offsets are flags; `long` move/tst offsets are
pointers.

## Endianness

All multi-byte fields are **big-endian** (68000). The C and Python helpers read
words/longs big-endian regardless of host.
