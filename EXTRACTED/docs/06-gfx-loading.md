# Graphics Loading Path (`.32`)

## What Is Confirmed

- Graphics are loaded from external `.32` files (not embedded pixel blobs).
- The filename strings live in code hunk 0 as C strings at `0x0212..0x035E`.
- A relocated pointer table in the DATA hunk points at those strings.
- Runtime loading goes through the resource/file wrapper family near `0x25222`, `0x254CC`, and `0x25528`.

## Filename Pointer Table

In DATA hunk 0, offset `0x07B0..0x0838` is a contiguous array of relocated longwords that resolve to code-hunk string addresses (catalogue begins @ `DATA_gfx_and_dat_filenames` / `0x230`):

- `disk.32`
- `globe.32`
- `book.32`
- `throw.32`
- `intro.32` (title background — see [`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md), animation [`39-title-screen-animation.md`](39-title-screen-animation.md))
- `introclips.32` (title animated clips — pegasus overlays + peekers)
- `xfer.32`
- `sky.32`
- `event.dat`
- `nwcp.32`
- `master.32`
- `townf.32`
- `townt.32`
- `townb.32`
- `town.32`
- `cavef.32`
- `cavet.32`
- `townb.32` (reused)
- `cave.32`
- `castlef.32`
- `castlet.32`
- `townb.32` (reused)
- `castle.32`
- `outdoor1.32`
- `outdoor2.32`
- `outdoor3.32`
- `outb.32`
- `outf.32`
- `desert.32`
- `ocean.32`
- `tundra.32`
- `swamp.32`
- `endgame.32`
- `str.dat`
- `map.dat`
- `roster.dat`

The `.dat` files are in the same table, so this appears to be a unified asset/resource list, not a graphics-only list.

**Combat sprites are not a `.32` sheet.** Monster art is **`NN.anm`** (TV animation blobs) indexed by `monsters.dat` byte @ `0x15 & 0x7F`. On PC DOS the parallel atlas is **`MONSTERS.4` / `MONSTERS.16`** (see [`54-pc-dos-graphics-formats.md`](54-pc-dos-graphics-formats.md)). There is **no** `monsters.32` in the retail Amiga install or filename catalogue.

**Not planar graphics:** `globe.32` is an XOR-obfuscated blob of copy-protection / title
string tables (decoded by `tools/decode_globe_amiga.py`; UI prompts documented in
`20-copy-protection-table.md`). `disk.32` is a similar XOR blob. Both share the `.32`
extension and loader path but are **not** Amiga image-chunk tilesets.

## Loader Mechanics

- `0x25222` validates an index against a 6-byte-per-entry resource table (`-0x34E(A4)` base, count at `-0x5E6E(A4)`), then dispatches via `0x25528`.
- `0x251D6` uses `0x254CC` to test/open an entry's backing name pointer from that table.
- `0x25300` is a central cleanup/reload routine that iterates the resource table and tears down/rebuilds handles.

## Practical Model

1. Startup builds or restores the resource-entry table.
2. Entries reference names through the relocated filename pointer table.
3. Screen/game-mode transitions request resources by index.
4. The wrapper layer (`0x254CC`/`0x25528` family) performs open/read/close and handle lifecycle.

## Follow-Up: `ANM`/`TV` Decode

The numbered animation files (`NN.anm`) are now partially decoded as the same `TV` family used by the `.32` loader path.

- See `EXTRACTED/docs/07-anm-tv-format.md` for current format notes.
- See `tools/decode_anm.py` for a working parser that exports metadata and per-plane blobs.
- See `EXTRACTED/decomp/mm2_anm_codec.h` and `EXTRACTED/decomp/mm2_anm_codec.c` for a C loader/saver API.

## Still Unknown

- Exact semantics of some prelude/sequence control bytes (`0x30..` block).
- Full meaning of per-frame `flags` fields (mostly observed as zero in sampled files).
- Exact runtime mapping between wrapper call sites and playback behavior in-engine.
