# MM2 Copy-Protection Challenge Table

Decoded from Amiga ASM flow around `0x26D38..0x26ED4` and extracted from
`EXTRACTED/mm2.asm` data bytes.

## Where It Is

- UI prompt text ("Please look at page ...") is in decoded `globe.32`
  string table 6 (`A4-$F7C` pointers).
- Challenge records are in a separate word table read through:
  - `lea -$6476(a4), a0` at `0x26D64`, `0x26DD4`, `0x26E44`

Current extracted table location in `mm2.asm`:

- start: `0x27530`
- count: `0x33` entries (51)
- entry size: `3 * u16` = 6 bytes

## Record Format

For each entry words `(w0, w1, w2)`:

- `w0`: page digits packed in low nibbles:
  - page tens = `(w0 >> 8) & 0x0F`
  - page ones = `w0 & 0x0F`
  - page = `tens*10 + ones`
- `w1`:
  - paragraph = `(w1 >> 8) & 0x0F`
  - line = `w1 & 0x7F`
- `w2`: target checksum used for answer verification

The answer path computes a running value from typed chars and compares against
`w2` (`cmp.w -$7a48(a4), d0` near `0x26ED0`).

## Tooling

- Python: `tools/decode_copyprot_words.py`
  - extracts bytes from `EXTRACTED/mm2.asm` `DC.L` lines
  - decodes all 51 entries
  - supports raw export + JSON export
- C codec: `EXTRACTED/decomp/mm2_copyprot_codec.{h,c}`
  - struct + decode/encode + load/save for raw 6-byte records

