# event.dat Runtime (68k ASM)

Single reference for how the Amiga executable loads, triggers, and interprets
`event.dat` scripts. File layout is in [06-event-dat-format.md](06-event-dat-format.md);
opcode semantics in [07-event-script-opcodes.md](07-event-script-opcodes.md).

## Core routines

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

## Loader (`0x92F2`)

1. `screen_mode_id = A4-$79F2` (byte).
2. Index header: `entry = header[screen_mode_id]` (6 bytes/entry, BE offset + BE length).
3. Clamp length to `0x8AC`; memcpy into `A4-$47C8` (`event_work_buf`).
4. Blob pointer from `A4-$1108` (`event_blob`).

Locations **0–59** align with `map.dat` / `attrib.dat` screens. Locations **60–70**
use the same index but hold global quest banks, castle blobs, and meta reference
text (see inventory in `EXTRACTED/event_location_inventory.json`).

## Init (`0x1754A`)

Walk `work_buf` from offset 0:

1. Read 3-byte triplets `(pos, event, cond)` until `00 00 00`.
2. Store terminator index in `A4-$7954` (`event_script_anchor`).
3. Read LE u16 at terminator → add to anchor → final string-table base in `$7954`.
4. Store `terminator + 5` in `A4-$5C44` (`event_script_start`).

If no `00 00 00` group exists (locations 63, 65, 68), init never completes
normally — those records use the **queued dispatch path** instead.

## Tile scanner (`0x175E2`)

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
    rebuild anchor from work_buf[0..1] as BE u16; seek queued id; re-init
if left trigger tile → clear visited bit7 + tile_rt bit7
else → pending_event_latch = 1
```

**Condition flags** (triplet byte 2):

| Value | Meaning |
|-------|---------|
| `0x10` | ALWAYS |
| `0x20` | DIR_N? |
| `0x40` | DIR_SPECIAL |
| `0x80` | ENTER |
| `0xC0` | ENTER+SPECIAL |
| `0xF0` | ANY_DIR |

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
| `-$79EA` | `script_abort` | Set by OP_29 / invalid op |
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
| `-$6CC8` | `opcode_len_tbl` | 256×16-bit deltas |
| `-$1108` | `event_blob` | ptr → event.dat |
| `-$110C` | `map_blob` | ptr → map.dat |
| `-$A9F5` | `attrib_era_gate_cache` | byte |

## Token skip table (`A4-$6CC8`)

Helper @ `0x157FC` for `OP_10`, `OP_11`, `OP_2B`:

1. Read count byte N.
2. Repeat N times: read token byte B at parse_pos; `parse_pos += word_table[B]`.

Table is indexed as **word[B×2]** (68000 `move.w (a0,d1.l), d0` after `asl.l #1`).
Dump / rebuild: `python tools/dump_event_token_table.py`.

## Tools

| Tool | Purpose |
|------|---------|
| `tools/decode_event.py` | Decode, decompile, predicates, consumed items |
| `tools/event_location_inventory.py` | JSON inventory of 71 locations |
| `tools/validate_event_map.py` | Triplet vs map collision `0x80` check |
| `tools/mm2_event_sim/` | Interactive TUI simulator |
| `EXTRACTED/decomp/mm2_event_codec.{h,c}` | C load/decode/encode |

## See also

- [14-game-state-struct.md](14-game-state-struct.md) — full A4 workspace
- [21-map-dat-format.md](21-map-dat-format.md) — collision event bit
- [12-attrib-dat-format.md](12-attrib-dat-format.md) — era gate byte `0x0F`
