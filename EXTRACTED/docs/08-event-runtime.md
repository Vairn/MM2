# event.dat Runtime

How MM2 loads, triggers, and interprets `event.dat` scripts.

## Source of truth

| Layer | Role |
|-------|------|
| **`game/src/events/EventRuntime.cpp`** | **Authoritative remake runtime** (~99% ASM-faithful): loader, init, tile scanner, VM loop, waits, queued/overlay dispatch |
| Helpers | `EventVmHelpers.cpp` (token table, vars, gold/gems, Search, arena), `EventTownServices.cpp` (`OP_0E`), `EventCombatEncounter.cpp` (`OP_12`/`13`), `EventPartyEffects.cpp`, `ServiceSignResolver.cpp` (`OP_0B`) |
| Opcode semantics | [07-event-script-opcodes.md](07-event-script-opcodes.md) — table rewritten to match `dispatchOp` |
| File layout | [06-event-dat-format.md](06-event-dat-format.md) |

ASM addresses below are provenance for the C++ port. When docs and C++ disagree, **fix the doc**.

### ASM → C++ map

| ASM | Label | C++ |
|-----|-------|-----|
| `0x92F2` | `event_dat_loader` | `EventRuntime::enterLocation` (copy into work buf) |
| `0x1754A` | `event_system_init` | `EventRuntime::initParsed` |
| `0x175E2` | `event_tile_scanner` | `EventRuntime::scanAndRun` |
| `0x17262` | `event_handler_pool_seek` | `poolSeek` / `poolSeekWorkBuf` |
| `0x172CA` / `0x1750C` | interpreter / fetch | `runVmLoop` + `dispatchOp` |
| `0x157FC` | token skip | `skipTokens` → `eventVmTokenDelta` |
| `0x15884` | text resolve | `resolveString` |
| `0x171AC` | cleanup | `endScript` / `abortScript` |
| `0x176B6` | queued dispatch | `runQueuedDispatch` |
| `0x15EDC` | OP_0E default-range bin | `eventVmBinExecSelector` + `runDefaultRangeOverlay` |

## Core routines (ASM)

| Address | Label | Role |
|---------|-------|------|
| `0x92F2` | `event_dat_loader` | Copy current location record into `A4-$47C8` (max 2220 bytes) |
| `0x1754A` | `event_system_init` | Scan triplet table; compute string anchor + script start |
| `0x175E2` | `event_tile_scanner` | Match party tile + cond flags; dispatch matching event id |
| `0x17262` | `event_handler_pool_seek` | Skip N `0xFF`-delimited records in script pool |
| `0x172BC` | (era gate) | Compare `era_low` vs cached `attrib+0x0F` before VM runs |
| `0x172CA` | `event_script_interpreter` | Opcode dispatch via table @ `0x17494` |
| `0x1750C` | `event_script_fetch_next` | Read next byte; loop or cleanup on `0xFF` |
| `0x155BE` | (read helper) | `work_buf[parse_pos++]` — script byte fetch |
| `0x157FC` | `event_token_skip_helper` | Skip N tokens using length table @ `A4-$6CC8` |
| `0x15884` | `event_text_resolve_u8` | Map u8 string index → string table pointer |
| `0x171AC` | `event_script_cleanup` | End script (`OP_0F`, `0xFF`, abort) |
| `0x1280` | main scheduler | If tile runtime `< $80` and pending latch → scanner thunk |

Thunk `JSR -$7D3A(A4)` from the main loop reaches `0x175E2` (vector @ `$25C6E`).

## Loader (`0x92F2` / `enterLocation`)

1. `screen_mode_id = A4-$79F2` (byte).
2. Index header: `entry = header[screen_mode_id]` (6 bytes/entry, BE offset + BE length).
3. Clamp length to `0x8AC`; memcpy into `A4-$47C8` (`event_work_buf`).
4. Blob pointer from `A4-$1108` (`event_blob`).

Locations **0–59** align with `map.dat` / `attrib.dat` screens. Locations **60–70**
use the same index but hold global quest banks, castle blobs, and meta reference
text (see inventory in `EXTRACTED/event_location_inventory.json`).

## Init (`0x1754A` / `initParsed`)

Walk `work_buf` from offset 0:

1. Read 3-byte triplets `(pos, event, cond)` until `00 00 00`.
2. Store terminator index in `A4-$7954` (`event_script_anchor`).
3. Read LE u16 at terminator → add to anchor → final string-table base in `$7954`.
4. Store `terminator + 5` in `A4-$5C44` (`event_script_start`).

If no `00 00 00` group exists (locations 63, 65, 68), init never completes
normally — those records use the **queued dispatch path** instead. Port leaves
`event_script_anchor=$FFFF` for `MM2_EVENT_KIND_CASTLE_BLOB`.

## Tile scanner (`0x175E2` / `scanAndRun`)

Each pending step:

```
context = byte_table_6BE6[ facing_index_55D7 ]
party_tile = (coord_a << 4) | coord_b     // Y in high nibble, X in low
if event_script_anchor == $FFFF → call 0x1754A
parse_pos = event_script_start
for each triplet (pos, event_id, cond):
    if pos != party_tile → continue
    if (cond & context) == 0 → continue
    pool_seek(event_id) → optional era gate → interpreter
    clear triplet bytes (one-shot)
if queued_event_id != $FF:
    rebuild anchor from work_buf[0..1] as LE u16 (buf[1]<<8|buf[0]); parse_pos=2;
    pool_seek(queued_id) → interpreter;
    -$7DFA event_dat_loader + 0x1754A re-init
```

**OP_0E default-range** (`0x15EDC` / `0x160A2`): temporarily sets `screen_mode_id` to
the overlay category, calls `-$7DFA` (loader), restores `screen_mode_id`, stores
the adjusted index in `queued_event_id` (`A4-$5D46`), then returns. The **scanner
epilogue** (not the OP_0E handler) runs that queued script. Port:
`eventVmBinExecSelector` + `runDefaultRangeOverlay` / `runQueuedDispatch`.

**Castle blobs** (locs 63/65/68): no `00 00 00` terminator — `0x1754A` never
completes; leave `event_script_anchor=$FFFF` and use queued dispatch only.

```
# (scanner epilogue continued)
if left trigger tile → clear visited bit7 + tile_rt bit7
else → pending_event_latch = 1
```

**Condition flags** (triplet byte 2, AND-masked with `context_mask_tbl[facing_index]`):

| Value | Meaning |
|-------|---------|
| `0x10` | DIR_W? (west-facing context) |
| `0x20` | DIR_S? (south-facing context) |
| `0x40` | DIR_E? (east-facing context) |
| `0x80` | DIR_N? (north-facing context; also used for ENTER tiles) |
| `0xC0` | DIR_N? + DIR_E? |
| `0xF0` | ANY_DIR (all four facing bits set) |

`context_mask_tbl` @ `A4-$6BE6` (ghidra `mm2_data_00.bin` off `0x1418`):
`[0x10, 0, 0x20, 0, 0x40, 0, 0x80, 0]` indexed by facing index `0/2/4/6` (W/S/E/N).
There is no “match all facings” cond byte — `0x10` door labels require west-facing only.
Port seeds this table in `initContextMaskTable`.

## Interpreter loop

```
172CA: op = read_script_byte()
       dispatch jump_table[0x17494][op]  // op in 0..0x32
1750C: if op == 0xFF → cleanup
       if script_abort → exit
       goto 172CA
1748C: script_abort = 1
```

Invalid opcode (`>= 0x33`) branches to `0x1748C`.

Port (`runVmLoop`): same fetch/dispatch; `EventVmWait` pauses for SPACE / Y/N /
member / answer / hex / letter; `OP_0E` sets `SCRIPT_ABORT` at entry so the
host script ends after the selector (and any async wait) returns.

## Era gating (two layers)

1. **Screen gate** @ `0x172BC`: `era_low` (`A4-$79B5`) must equal cached
   `attrib.dat` byte `0x0F` for the current screen (`A4-$A9F5`).
2. **Script ops** `OP_22` / `OP_23`: range-check era or day-of-year into
   `cond_flag` (`A4-$7951`).

## A4 memory map (event-related)

Anchor `A4 = $7FFE`. Canonical signed offsets (see `mm2_gamestate.h`).

### Session / interpreter

| Offset | Field | Notes |
|--------|-------|-------|
| `-$79F2` | `screen_mode_id` | Indexes event.dat header |
| `-$79F1` | `coord_b` | Party X (column) |
| `-$79F0` | `coord_a` | Party Y (row) |
| `-$7956` | `event_parse_pos` | Script PC in work buf |
| `-$7954` | `event_script_anchor` | String base; `$FFFF` = need init |
| `-$7952` | `pending_event_latch` | Set on movement; cleared @ 0x1280 |
| `-$7951` | `cond_flag` | Predicate result; gates OP_10/11 |
| `-$7950` | `exit_flags` | ESC / modal bits |
| `-$79EA` | `script_abort` | Set by OP_29 / invalid op / OP_0E entry |
| `-$79B5` | `era_low` | Current era index |
| `-$79B1` | `last_move_key` | `'N'/'S'/'E'/'W'` |

### Queued / secondary dispatch

| Offset | Field | Notes |
|--------|-------|-------|
| `-$5C44` | `event_script_start` | Restored to parse_pos each scan |
| `-$5D46` | `queued_event_id` | `$FF` = none; path @ 0x176B6 |
| `-$5D42` | `saved_cond_flag` | OP_26/27 party select |
| `-$5D44` | `string_walk_index` | String resolver @ 0x15884 |

### Tile / map runtime

| Offset | Field | Notes |
|--------|-------|-------|
| `-$55D7` | `facing_index` | 0/2/4/6 → context table |
| `-$55D6` | `tile_runtime_flags` | Bit7 active event; `<$80` allows scanner |
| `-$55C8` | `event_busy_sentinel` | `$FF` during script |
| `-$55BA` | `tile_table_a` | OP_21 writes |
| `-$54BA` | `tile_visited_flags` | OP_14 clears bit7 |
| `-$6BE6` | `context_mask_tbl` | Facing → cond context byte |

### Buffers / pointers

| Offset | Field | Size |
|--------|-------|------|
| `-$47C8` | `event_work_buf` | 2220 |
| `-$6CC8` | `opcode_len_tbl` | 51×16-bit deltas (`0x00..0x32`) |
| `-$1108` | `event_blob` | ptr → event.dat |
| `-$110C` | `map_blob` | ptr → map.dat |
| `-$A9F5` | `attrib_era_gate_cache` | byte |

## Token skip table (`A4-$6CC8`)

Helper @ `0x157FC` for `OP_10`, `OP_11`, `OP_2B`:

1. Read count byte N.
2. Repeat N times: read token byte B at parse_pos; `parse_pos += word_table[B]`.

Table is indexed as **word[B×2]** (68000 `move.w (a0,d1.l), d0` after `asl.l #1`).
Dump / rebuild: `python tools/dump_event_token_table.py`.
Port: `eventVmTokenDelta` (ROM quirks: `OP_00` delta 0, `OP_25` delta 2 ≠ execute length 3).

## Tools

| Tool | Purpose |
|------|---------|
| `tools/decode_event.py` | Decode, decompile, predicates, consumed items |
| `tools/event_location_inventory.py` | JSON inventory of 71 locations |
| `tools/validate_event_map.py` | Triplet vs map collision `0x80` check |
| `tools/mm2_event_sim/` | Interactive TUI simulator |
| `EXTRACTED/decomp/mm2_event_codec.{h,c}` | C load/decode/encode |
| `game/tests/event_*.cpp` | Remake VM regression tests |

## See also

- [07-event-script-opcodes.md](07-event-script-opcodes.md) — opcode table (C++-matched)
- [14-game-state-struct.md](14-game-state-struct.md) — full A4 workspace
- [21-map-dat-format.md](21-map-dat-format.md) — collision event bit
- [12-attrib-dat-format.md](12-attrib-dat-format.md) — era gate byte `0x0F`
- [game/README.md](../../game/README.md) — remake layout
