# MM2 `ANM` (`TV`) Format Notes + C Loader/Saver

This documents the custom MM2 animation format used by files like `54.anm`..`74.anm`.

It is based on:
- Binary inspection of `EXTRACTED/*.anm`
- Runtime string/use evidence in `EXTRACTED/mm2.asm` (`00.anm` template)
- Legacy decode code from your older `gfxTool` project

## High-Level Layout

All samples start with file magic:

- `00 00 54 56` (`"TV"` marker at bytes 2..3)

Then:

1. **Prelude frame descriptors** (fixed 11 slots, 4 bytes each) at `0x04..0x2F`
   - Unused slots are `FF FF FF FF`
   - Used slot (current best model):
     - `u8 x_offset`
     - `u8 y_offset`
     - `u8 width`
     - `u8 height`
   - Working composition model from file behavior:
     - frame `0` = base sprite
     - frame `N>0` = patch sprite drawn at `prelude[N-1].(x_offset,y_offset)`
     - `width/height` can act as patch crop bounds
2. **Sequence header bytes** at `0x30..0x32`
   - `seq_header_b & 0x7F` ~= sequence-count hint (can under-report)
   - high bit in `seq_header_b` is often set (unknown flag)
3. **Sequence stream** at `0x33...`
   - Sequence blocks are delimited by `0xFF`
   - Payload is typically `(frame_index, delay)` byte pairs
   - Parse until image marker (`FF 00 ...`), do not hard-stop on header hint
4. **Image chunk marker + header**
   - Located by scanning for `FF 00` then interpreting big-endian words at the next byte
   - Header words:
     - `u16 frame_count`
     - `u16 depth_or_mode` (observed `3`)
5. **Per-frame image metadata** (`frame_count` entries):
   - `u16 width`
   - `u16 height`
   - `u16 flags` (observed mostly `0`)
6. **Palette**: 32 words (`u16`)
7. **Frame pixel stream**: packed nibble RLE/literal stream, decoded sequentially per frame

## Pixel Codec (Nibble Stream)

Each command byte:

- If high nibble is `0x0` or `0xF`:
  - Emit nibble value (`0` or `15`) repeated `(low_nibble + 1)` times
- Else:
  - Literal byte: emit two nibbles (high, then low)

Nibbles are packed to bytes in order (`n0,n1 -> byte (n0<<4)|n1`).

Decoded output for one frame is:

- `5 * RASSIZE(width, height)` bytes
- `RASSIZE(w,h) = h * ((((w) + 15) >> 3) & 0xFFFE)`
- Payload is 5 planar bitplanes concatenated (`plane0..plane4`)

## Loader/Saver Roundtrip Strategy

Because some control fields are still partially unknown, the safest saver strategy is:

- Parse and keep all known fields structurally.
- Preserve unknown bytes/flags exactly.
- Re-emit sequence section and prelude table byte-identically unless intentionally edited.
- Only recompute frame payload and any lengths/offsets that are explicitly required.

## C API (Recommended)

Use the reference implementation in:

- `EXTRACTED/decomp/mm2_anm_codec.h`
- `EXTRACTED/decomp/mm2_anm_codec.c`

Core API:

- `mm2_anm_load_file()` parses container + frame metadata and decodes frame planes.
- `mm2_anm_save_file()` writes parsed structure back.
- `mm2_anm_free()` releases owned buffers.

The saver currently targets **roundtrip/preservation** semantics; if you want canonical rebuild (re-encode stream from raw planes), add a dedicated encoder pass.

## Python Tool

`tools/decode_anm.py` now parses this format and writes:

- `EXTRACTED/anm_decoded/<name>/meta.json`
- `EXTRACTED/anm_decoded/<name>/frame_###/plane{0..4}.bin`

Example:

```powershell
python tools\decode_anm.py EXTRACTED\54.anm EXTRACTED\74.anm
```

