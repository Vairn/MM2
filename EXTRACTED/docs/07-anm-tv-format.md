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
   - **Runtime draw (game + editor):** never blit stored frame `N>0` directly. Always
     composite: blit frame `0` as the canvas, **clear** the prelude rect, then blit the
     patch (cropped by prelude w/h). Sequence-stream frame indices refer to these
     **composed** states. Implementation: `editor/src/core/Gfx.cpp`
     `gfxAnmCompositeFrame()` — the game port must call the same logic when drawing
     combat/world `.anm` sprites.
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

## Runtime composition

The on-disk image frames are **not** a flat flipbook of full sprites:

| Stored frame | Meaning at draw time |
|--------------|----------------------|
| `0` | Full base bitmap |
| `N > 0` | Patch only — composite over frame `0` at `prelude[N-1]` |

Draw algorithm (matches original blitter behavior):

1. Blit frame `0` onto the destination canvas (bounds = union of base + all prelude rects).
2. For composed state `N > 0`: clear `(x, y, w, h)` from prelude slot `N-1`, then blit
   patch frame `N` (crop to prelude w/h when smaller than the patch bitmap).
3. Pen `0` is transparent (mask colour); opaque pixels overwrite; cleared rects erase
   previous pixels in that region before the patch blit.

**Sequence playback:** bytes at `0x33..` list `(frame_index, delay)` pairs grouped by
`0xFF` delimiters. Each `frame_index` is a **composed** state index (0 = base only),
not a raw patch sheet index. Multiple sequences per file = idle / attack / hurt / etc.

**Editor:** Animations (`.anm`) section → **Composed frames** tab shows every runtime
state with a live preview when **Play** is on. **Play mode → Flipbook** cycles composed
frames 0..N-1; **Sequence** uses the embedded `(frame,delay)` stream (combat clips).
**Raw patches** tab shows decoded bitmaps before composition (debug only).

**Game port (`game/`):** use `gfxAnmCompositeFrame()` semantics (port from
`editor/src/core/Gfx.cpp` or share the module) for any `.anm` draw — do not upload
raw patch frames as sprites.

**PC DOS equivalent:** `MONSTERS.4` / `MONSTERS.16` combat blobs use the same prelude
semantics — per-frame header `(x,y,w,h)` matches Amiga prelude slot `N−1` for patch
frame `N`. Export: `tools/decode_pc_gfx.py` `composite_pc_combat_frame()`. Full notes:
[`54-pc-dos-graphics-formats.md`](54-pc-dos-graphics-formats.md). Side-by-side gallery:
[`monster-variants`](/docs/gallery/monster-variants) (wiki).

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

