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
  - `0x0D` - usable-by bitmask
  - `0x0E` - bonus packed nibble (`hi=bonus_type`, `lo=bonus_amount`)
  - `0x0F` - effect packed nibble (`hi=effect_type`, `lo=effect_amount`)
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
- Record shape currently modeled as:
  - `0x00..0x0D` - monster name (14-byte fixed field)
  - `0x0E..0x19` - 12-byte stat/behavior payload (still being field-lifted)

Notes:

- This is sufficient for byte-perfect load/edit/save and safe round-tripping.
- Field-level semantics for the 12-byte payload remain TODO pending deeper combat call-site traces.

## `str.dat` (confirmed encoding, partial semantics)

Encoding:

- Byte transform: decoded char = `(encoded + 0x1C) & 0xFF`
- `0x01` acts as newline/line break

Observed content:

- Contains many in-game text strings (jokes, prompts, shop/guild text, menu/help text).

## Partially Characterized / TODO

These files are inventoried and size-confirmed, but structure still needs field-level confirmation against ASM call sites:

- `event.dat` (95687 bytes)
- `map.dat` (30720 bytes)
- `spells.dat` (256 bytes)
- `attrib.dat` (3840 bytes)

Working rule:

- Use ASM first (`EXTRACTED/mm2.capstone.asm`, `EXTRACTED/mm2.asm`) to identify true runtime reads/writes.
- Use legacy Blitz3d reverse code as a cross-check.
- Resolve conflicts in favor of observed assembly behavior.
