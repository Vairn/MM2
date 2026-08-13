# `.dat` Files and Formats

MM2-local inventory and current format status for data files in this workspace.

## MM2-Local Inventory

Canonical set in project root (same bytes are mirrored under `EXTRACTED/` and `MM2BOOT/`):

- `attrib.dat` - 3840 bytes
- `event.dat` - 95687 bytes
- `items.dat` - 5120 bytes
- `map.dat` - 30720 bytes
- `monsters.dat` - 6656 bytes
- `roster.dat` - 8320 bytes
- `spells.dat` - 256 bytes
- `str.dat` - 7808 bytes

## GOG/PC `.DAT` compression (confirmed against a real GOG install)

The GOG DOS build wraps **some** `.DAT` files in the same custom LZW codec
used by the PC `.4`/`.16` graphics (`lzw_decompress` traced to `MM2.EXE
@0x2A42`; see [`54-pc-dos-graphics-formats.md`](54-pc-dos-graphics-formats.md)).
Detection/decompression: Python `tools/pc_dat_lzw.py` (canonical LZW impl
shared via `tools/mm2_lzw.py`); C++ `editor/src/core/PcDatLzw.{h,cpp}`, wired
into `AttribFile`/`MonstersFile`/`StrFile`/`MapFile`/`EventFile`'s `load()`.

| GOG file | Container | Result vs Amiga file |
|----------|-----------|------------------------|
| `ATTRIB.DAT` | flat (`u32 LE size` + LZW, whole file) | **byte-identical** to `attrib.dat` |
| `MONSTERS.DAT` | flat | **byte-identical** to `monsters.dat` |
| `STR.DAT` | flat | decodes cleanly to 7707 bytes (Amiga is 7808 — different platform string table, not a decode error) |
| `MAP.DAT` | table: `u16 LE[60]` offset table (one 512-byte screen blob per entry, each `u32 LE size` + LZW) | reassembled 60 screens **byte-identical** to `map.dat` |
| `EVENTSI.DAT` + `EVENTSO.DAT` | table: `u32 LE[71]` offset table each (indoor / outdoor halves of the 71 `event.dat` locations; mutually exclusive per slot) | merged (71x6-byte BE header + concatenated blobs) reproduces `event.dat` to within **18/95687 bytes** — all single-byte platform content differences (same offsets/lengths; verified `0x02` Amiga vs `0x01` PC at each diff), not decode errors |
| `ITEMS.DAT` | plain (not compressed) | **byte-identical** to `items.dat` |
| `ROSTER.DAT` / `SPELLS.DAT` / `DEFAULT.DAT` | plain (not compressed) | **ROSTER.DAT**: same 130-byte records and 2052-byte global stream as Amiga; **8292** bytes vs Amiga **8320** (28 zero pad bytes at EOF on Amiga only). Editor/`mm2_roster_codec` accept both. `SPELLS.DAT` still differs in record count (192 vs 256 bytes) — not handled here. |

Both table containers reuse the `MONSTERS.4`/`.16` combat-atlas convention:
`entry[0] == table_byte_size` (also slot 0's blob offset) for `MAP.DAT`
(which never has an empty first screen); `EVENTSI.DAT`/`EVENTSO.DAT` instead
use a fixed 71-slot count since slot 0 can legitimately be empty in either
half. `entry[k] == 0` always means "no blob for slot k".

The editor's `EventFile::load()` tries plain `event.dat` first, then falls
back to `EVENTSI.DAT`+`EVENTSO.DAT` auto-merge if only those exist. Saving
from the editor always writes plain Amiga-format `.dat` files (round-trip
back to the GOG-compressed shape is not wired into the editor's save path);
`tools/pc_dat_lzw.py --compress` re-compresses the flat container for
offline round-trip testing, reusing the verified LZW compressor from
`tools/encode_pc_gfx.py`.

## Confirmed Formats

## `items.dat` (confirmed)

Cross-validated with:

- MM2 asset filename references in binary (`items.dat` appears with `spells.dat` near code offset `0x29868`)
- Legacy reverse code: `C:\_20260421_\D-REC\development\Amiga\Smite-and-Magic\Blitz3d\b3dmm2\Item_ED.bb`

Layout:

- 256 records
- 20 bytes per record (`0x14`)
- Record fields:
  - `0x00..0x0B` - item name (12-byte ASCII, space-padded)
  - `0x0C` - separator/pad (editor writes `0`)
  - `0x0D` - **forbidden-class** bitmask (set bit = that class **cannot** use the item; `K P A C S R N B` = `0x80..0x01`) — not a "usable-by" mask
  - `0x0E` - bonus packed nibble (`hi=bonus_type`, `lo=bonus_amount`)
  - `0x0F` - **use-power byte** — flat spell/effect index, **not** a `hi/lo` nibble pair
  - `0x10` - damage byte
  - `0x11` - pad/unused byte (editor writes `0`)
  - `0x12..0x13` - gold (`uint16` little-endian on disk; shop price in gp; game
    byte-swaps each record word on load, asm `0x26030`)

Notes:

- `5120 = 256 * 20`, which matches the editor loader/saver loops.
- Item names align 1:1 with `b3dmm2/items.txt`.

## `roster.dat` (confirmed layout + C loader/saver)

Cross-validated with:

- MM2 executable ASM (`EXTRACTED/mm2.asm`, `EXTRACTED/mm2.capstone.asm`)
- New C codec in `EXTRACTED/decomp/mm2_roster_codec.h` + `.c`

Layout:

- 64 records
- 130 bytes per record (`0x82`)
- Key fields:
  - `0x00..0x0A` - character name (11-byte ASCII)
  - `0x0B` - town/inn byte (`low7=town`, `bit7=party flag` inferred)
  - `0x0C` - sex
  - `0x0D` - alignment (current)
  - `0x0E` - race
  - `0x0F` - class
  - `0x10..0x15` - current core stats
  - `0x27` - current endurance
  - `0x28..0x39` - equipped items (6 slots x 3 bytes)
  - `0x3A..0x4B` - backpack items (6 slots x 3 bytes)
  - `0x4C..0x57` - spellbook bytes
  - `0x58..0x61` - SP/HP/Gems words (little-endian)
  - `0x62..0x65` - XP (little-endian dword)
  - `0x66..0x69` - Gold (little-endian dword)
  - `0x6A..0x73` - base alignment/stats/level/spell-level/endurance
  - `0x74..0x75` - HP current (word)
  - `0x76..0x81` - unresolved tail bytes

Notes:

- Loader/saver round-trips unknown spans byte-for-byte.
- The codec treats numeric multibyte fields in `roster.dat` as little-endian on disk.

## `monsters.dat` (confirmed record geometry + C loader/saver)

Cross-validated with:

- MM2 executable ASM loader path (allocates/reads `0x1A00` bytes for `monsters.dat`)
- MM2 accessor routine around `0x99C8` (`mulu #$1a`, then copies exactly 26 bytes)
- C codec in `EXTRACTED/decomp/mm2_monsters_codec.h` + `.c`

Layout:

- 256 records
- 26 bytes per record (`0x1A`)
- Record shape (confirmed by the unpacker @ ASM `0x4C8E`, `MM2_MON_OFF_*` in
  `EXTRACTED/decomp/mm2_monsters_codec.h`):
  - `0x00..0x0D` - monster name (14-byte fixed field; high bit of first byte used elsewhere as a flag — see [`16-monster-ability-format.md`](16-monster-ability-format.md))
  - `0x0E` HP · `0x0F` XP (same lookup table as HP, `A4-$746C`) · `0x10` treasure ·
    `0x11` Pabil (group attack) · `0x12` Sabil (single-target attack) ·
    `0x13` Oabil (other behaviour / friend-add) · `0x14` speed · `0x15` picture ·
    `0x16` AC · `0x17` damage · `0x18` speed2 · `0x19` magic resist

Notes:

- Byte-perfect load/edit/save and round-tripping are solid.
- Full field semantics (HP/XP/treasure/Pabil/Sabil/Oabil/speed/AC/damage/mres)
  are decoded — see [`16-monster-ability-format.md`](16-monster-ability-format.md)
  for the per-field breakdown and status-effect/verb tables.

## `str.dat` (confirmed encoding, partial semantics)

Encoding:

- Byte transform: decoded char = `(encoded + 0x1C) & 0xFF`
- `0x01` acts as newline/line break

Observed content:

- Contains many in-game text strings (jokes, prompts, shop/guild text, menu/help text).

## `map.dat` (confirmed)

Cross-validated with:

- MM2 executable ASM (flat load into `A4-$EEF4`, 3D hood reads page 0 @ `0x2900`)
- `editor/src/core/MapFile.{h,cpp}` — 60 screens × 512 bytes
- Collision `0x80` event alignment vs `event.dat` triplets

Layout:

- 60 map screens
- 512 bytes per screen (`0x200`)
- Per screen:
  - `0x000..0x0FF` — **page 0 (visual)**: 16×16 grid; four 2-bit wall fields per cell (N/E/S/W)
  - `0x100..0x1FF` — **page 1 (collision)**: same geometry; `(dark<<1)|wall` per direction; bit `0x80` = event flag

Notes:

- `30720 = 60 × 512`, which matches the editor loader/saver loops.
- Row 0 on disk = south; MM2ED and wiki grids draw north-up.
- Full field decode: [map.dat format](21-map-dat-format.md).

## Decoded formats

- **event.dat** (95687 bytes) — 71 location records; see [06-event-dat-format.md](06-event-dat-format.md), [07-event-script-opcodes.md](07-event-script-opcodes.md), [08-event-runtime.md](08-event-runtime.md)
- **spells.dat** (256 bytes) — see [19-spells-and-item-use.md](19-spells-and-item-use.md)
- **attrib.dat** (3840 bytes) — see [12-attrib-dat-format.md](12-attrib-dat-format.md)

## Partially Characterized / TODO

These files still need additional field-level work against ASM call sites:

- (none of the primary `.dat` containers remain fully TODO)

Working rule:

- Use ASM first (`EXTRACTED/mm2.capstone.asm`, `EXTRACTED/mm2.asm`) to identify true runtime reads/writes.
- Use legacy Blitz3d reverse code as a cross-check.
- Resolve conflicts in favor of observed assembly behavior.
