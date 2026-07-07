# PC DOS graphics (`.4` / `.16`)

8086 / 16-bit real-mode formats for the GOG **Might and Magic II** PC build.
**Not Amiga** — see [`06-gfx-loading.md`](06-gfx-loading.md) for `.32` planar sheets.

| Artifact | Path |
|----------|------|
| Disassembly | [`EXTRACTED/pc/`](../pc/README.md) — `mm2.capstone.asm`, `cga.capstone.asm`, `ega.capstone.asm` |
| Decoder | `tools/decode_pc_gfx.py` |
| Encoder + round-trip validation | `tools/encode_pc_gfx.py` (`--roundtrip`) |
| Gallery export | `tools/pc_gfx_export.py` → `wiki/public/gallery/pc/` |
| Extracted PNGs | `EXTRACTED/pc_gfx/{cga,ega}/` walls; `EXTRACTED/pc_gfx/monsters/{cga,ega}/` combat |

Regenerate listings:

```powershell
python tools\disasm_pc_x86.py "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2\MM2.EXE" `
  "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2\CGA.DRV" `
  "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2\EGA.DRV"
python tools\decode_pc_gfx.py "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2" --batch
python tools\pc_gfx_export.py
```

---

## File inventory

| Extension | Mode | Driver | Examples |
|-----------|------|--------|----------|
| `.4` | CGA 320×200×4 (2 bpp) | `CGA.DRV` | `CASTLE.4`, `TOWN.4`, `MONSTERS.4`, `SKY.4` |
| `.16` | EGA 16-colour (4 bpp linear on disk) | `EGA.DRV` | `CASTLE.16`, `MONSTERS.16`, … |

Filename table in `MM2.EXE` @ `0x8815` (same stems as Amiga `.32` list, different extension).

---

## LZW container (all `.4` / `.16` blobs)

| Field | Size | Notes |
|-------|------|-------|
| `uncompressed_size` | u32 LE | Expected decompressed byte count |
| LZW stream | rest | `lzw_decompress` @ `MM2.EXE 0x2A42` |

Decompressor uses 9→12 bit codes, early-change dictionary growth, clear code `0x100`.
Root character for new dictionary entries is `expand(code)[0]` (not the last char).

---

## Wall / environment sheets

Used for first-person view walls, floors, skies, UI sprites (`THROW`, `TOWN`, `SKY`, …).

### Decompressed layout

| Offset | Field |
|--------|-------|
| `+0` | `frame_count` = `dec[0] & 0x3F`; **class** = `dec[0] >> 6` (`gfx_format_class_check` @ `0x5C5A`) |
| `+1` | `flags` — low nibble + bits 4–5 (`frame_index_scale` @ `0x42EA`) |
| `+2` | Offset table: **u32 LE** `(end<<16)|start` pairs or plain u32 offsets × `frame_count` (auto-scored) |
| per offset | Frame blob |

### Per-frame blob

| Field | Size |
|-------|------|
| `width` | u16 LE |
| `height` | u16 LE |
| pixels | `(width+3)/4` bytes/row (CGA) or `(width+1)/2` bytes/row (EGA) × height |

**CGA:** 2 bpp, MSB-first, 4 pixels/byte (`CGA.DRV` plot @ `0x34C`).  
Palette 1 (black / cyan / magenta / white) via `INT 10h AH=0Bh BX=0x0101` in `drv_init` @ `0x2A8`.

**EGA:** 4 bpp **linear** on disk (hi nibble = left pixel). Not planar despite EGA blit using
map-mask for screen writes.

### Runtime frame selection (`pick_frame_from_sheet` @ `0x5D4A`)

1. `load_wall_gfx_sheet` @ `0x5234` LZW-decompresses the file into a resource record.
2. `frame_index_scale` @ `0x42EA` derives a frame index from `dec[1]` flags and viewport state
   (`word ptr [0x9E0C]`). For sheets with `dec[1]=0` and scale 0, the default logical frame
   budget is **8** (`mov word ptr [0x9e0c], 8` @ `0x433A`) — matches `MASTER.16`.
3. `pick_frame_from_sheet` walks driver token stream (`parse_resource_token` @ `0x366E`) to
   resolve the sheet slot, then indexes the offset table at **`logical_index × 2`**
   (`shl ax, 1` @ `0x5E60` before the table pointer add).
4. `CGA.DRV` blit @ `0x65A` / EGA equivalent copies rows to video or off-screen buffer.

**Packed u32 tables:** sheets like `MASTER.16` store each entry as **`(frame_end << 16) | frame_start`**
(u32 LE). `dec[0] & 0x3F` is the **frame count** (15 on GOG `MASTER.16`). Frame 14 is the
**320×196 EGA title screen** ("Might and Magic Book Two" + pegasus); frames 0–13 are menu/UI
sprites stored in the same blob. The region after the first eight small frames is **not** a
separate script block — it holds additional packed frame records and pixel data until the title
bitmap @ `0x2B10`. MM2.EXE indexes with `index×4` @ `0x7018`.

**Grouped u16 view:** the same bytes can be read as interleaved u16 `[start, end]` pairs (the
first eight frames only). Runtime `pick_frame_from_sheet` uses `index×2` for that view @ `0x5E60`.
Tools prefer the packed-u32 interpretation when it decodes more frames with matching end offsets.

There is **no compositing** within a wall sheet — each frame is a complete image. Extraction writes:

- `raw/frameNN.png` — individual frames as stored
- `sheet.png` — all frames tiled in a grid (reconstructed atlas view)

---

## Monster combat atlas (`MONSTERS.4` / `MONSTERS.16`)

Indexed by **picture number** — the same id Amiga uses for `NN.anm` combat sprites
(`monsters.dat` byte @ `0x15 & 0x7F`). PC and Amiga share the same `monsters.dat`
(GOG `MONSTERS.DAT` is LZW-compressed on disk but decompresses to the Amiga file).

**Gallery tools**

| Output | Command |
|--------|---------|
| Per-slot PNG/GIF (`01/…`) | `python tools/decode_pc_gfx.py MONSTERS.16 --monsters --extract-all` |
| Slot compare 01–74 | `python tools/gen_gfx_compare_html.py` → `EXTRACTED/pc_gfx/compare/` |
| Per-monster Amiga/CGA/EGA GIFs | `python tools/export_monster_variants.py` |
| Wiki table (256 monsters) | `python wiki/scripts/export-monster-variants.py` |

### File header (GOG)

The prefix is **not** `u16 count + u16[count]`. It is a flat table of **u32 LE blob
file offsets**:

| Entry | File bytes | GOG value | Meaning |
|-------|------------|-----------|---------|
| `entry[0]` | `0..3` | `300` | Header size in bytes **and** offset of first blob |
| `entry[1]` | `4..7` | `3134` | Blob for picture id **2** |
| `entry[N−1]` | `(N−1)×4 …` | `0` or offset | Blob for picture id **`N`** |

- **75 entries** on GOG (`300 / 4`); bytes `0..299` are the table, first LZW blob @ `300`.
- **`entry[k] = 0`** → no blob for picture id **`k + 1`** in this file.
- **`monsters.dat` picture id `N`** → seek **`u32[(N − 1) × 4]`** (1-based picture id,
  0-based table index). Confirmed: pic `1` (spider) → entry `0`; pic `19` (cat) → entry `18`.

### Runtime loader (`MM2.EXE` @ `0x6824`)

1. Open `MONSTERS.4` or `MONSTERS.16` (filename patched @ `DS:0x4D0`).
2. Read **`0x11C`** (284) bytes into RAM @ `0x6052` — **71** u32 entries in the buffer.
3. `bl = byte [0x392]`; **seek `u32[bl × 4]`** from the buffered header.
4. Read u32 decompressed size @ blob start, LZW-decompress.

The loader has **no zero-check**; zero offset seeks to file start and fails decompression.
When a slot is empty, the **caller** advances to the next non-empty picture (below).

`0x68ED` opens a **different** file (path @ `DS:0x52D2`) with a **u16** table — not the
primary `MONSTERS.*` u32 header. Do not use it for combat atlas export.

### Missing-slot fallback (observed in-game)

When header entry is **`0`**, the DOS build advances to the **next non-empty** picture:

| Monster | `monsters.dat` pic | PC slot | In-game PC sprite |
|---------|-------------------|---------|-------------------|
| Merchant | 34 | empty | pic **35** (mage) |
| Mountain Man | 43 | non-zero | blob = Amiga **`42.anm`** (amazon); **`43.anm`** absent from PC |

Some non-empty slots also **differ from Amiga** for the same picture id (port reshuffle).
Export marks fallback rows **`→ pic NN`**. See [Monster variants](/docs/gallery/monster-variants).

### Per-picture LZW blob

| Offset | Field |
|--------|-------|
| `+0` | u32 LE `decompressed_size` |
| `+4` | LZW bitstream |
| dec `+0` | `frame_count` = `dec[0] & 0x3F` |
| dec `+1` | `flags` |
| dec `+2` | u16 LE `[frame_count]` inner frame offsets |
| mid | Animation **script sequences** (see below) |
| per offset | Frame records |

### Animation scripts

Between the u16 offset table and the first frame offset, the blob contains one or more
**script sequences**:

- Each sequence: byte pairs `(frame_index, delay)` … terminated by `0xFF`
- End of all scripts: `0xFF 0xFF`
- `delay` is a combat tick hold count (used by combat overlay timing)
- Typical pattern: script 0 = idle sway overlays; script 3 = walk cycle `(3,0)(4,0)…(7,0)…`

Combat load path: `load_combat_monster_gfx` @ `0x6544` → `pick_frame_from_sheet` → overlay
draw thunk @ `0x78C6` → `CGA.DRV`/`EGA.DRV` sprite decode.

### Per-frame record

| Offset | Field |
|--------|-------|
| `+0` | `x` — blit X on 96×96 combat canvas |
| `+1` | `y` — blit Y |
| `+2` | `width` |
| `+3` | `height` |
| `+4` | Nibble-RLE token stream |

**Frame 0** is the full base sprite (usually `x=0, y=0`). Later frames are **delta patches**.
Header `(x,y,w,h)` matches Amiga **TV-prelude** slot `N−1` for frame `N` (verified on spider pic `1`).

### Compositing (export + runtime)

Same rule as Amiga `composite_anm_frame` ([`07-anm-tv-format.md`](07-anm-tv-format.md)):

1. Blit frame **0** onto the 96×96 canvas.
2. For delta **N**: **clear** `(x,y,w,h)` to transparent, then blit patch.
3. Script step `(frame_idx, delay)`: base only if `frame_idx == 0`, else base + delta.

GIF export flattens to opaque `(13,3,3)` with disposal `2`.

### Nibble-RLE sprite codec

Implemented in **`CGA.DRV 0xAF2`** and **`EGA.DRV 0x1422`**. Draws into a 96×96 off-screen
buffer (`0x900` bytes CGA / `0x1200` EGA per slot), then op `0x17` blits to screen.

Each token byte:

| Bits | Meaning |
|------|---------|
| high nibble | Run count; run length = `count + 1` |
| low nibble | Code (see below) |

**CGA (`.4`):** code `0–3` = literal colour run; `4–15` = transparent run (skip pixels).  
**EGA (`.16`):** code `5` = transparent; else colour = `xlat[code]` where xlat @ `EGA.DRV 0x121B`
= `[0,1,2,9,6,8,10,3,4,5,7,11,12,13,14,15]`.

Runs wrap across scan-lines; decode stops after `height` rows. CGA 2bpp shift table @ `0xAE4`
= `06 04 02 00` (MSB-first within byte).

### Extraction layout

```
EXTRACTED/pc_gfx/monsters/ega/
  01.gif
  01/
    frames/f00.png
    base.png
    s5_step00.png
    s5.gif
```

### GOG coverage (picture ids 1–74)

| File | Thumbs | Empty in both |
|------|--------|---------------|
| `MONSTERS.4` | 53 / 74 | + CGA-only gaps at 5,6,29,36,45,71 |
| `MONSTERS.16` | 59 / 74 | 7,23,28,30,32,34,38,42,47,49,52,54,64,70,73,75 |

Amiga repo: **72** `.anm` files (`01`–`74` with gaps).

**Blob sharing** (multiple picture ids → same LZW blob):

| EGA blob @ | Picture ids |
|------------|-------------|
| `0x287F` | `03`, `06` |
| `0x35CC` | `04`, `72` |
| `0x3F2E` | `05`, `74` |
| `0x778F` | `11`, `22` |
| `0xEE6F` | `23`, `46` |
| `0xF99C` | `24`, `48` |

---

## Title screen (PC DOS)

There is **no** ``intro.4`` / ``intro.16`` on the GOG PC install (Amiga ``intro.32`` pegasus
background is not a separate PC sheet). Title/menu presentation uses:

| Asset | Role |
|-------|------|
| ``NWCP.4`` / ``NWCP.16`` | “New World Computing Presents” logo (320×83, filename table index 8) |
| ``BOOK.4`` / ``BOOK.16`` | Menu book ornament frames |
| ``1MENU1.OVL`` / ``1MENU2.OVL`` | Title/menu overlay code (text, input, flow) |

See Amiga title docs ([`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md))
for the full pegasus + ``introclips`` attract sequence — PC CGA/EGA uses the assets above instead.

---

## Overlays (`.OVL`)

Combat logic lives in external **`2COMBAT.OVL`** (flat code, no MZ header). `MM2.EXE` overlay
manager @ `077D:0344` loads `.OVL` files listed in the descriptor table (seg `0x6BF`).
Title/menu flow uses **`1MENU1.OVL`** / **`1MENU2.OVL`** (see title screen table above).

The sprite **decode** itself is in the video drivers (`CGA.DRV` / `EGA.DRV`), not the overlay.

---

## Encoder + round-trip validation (`tools/encode_pc_gfx.py`)

Inverse operations for everything above, plus a `--roundtrip` mode that
decodes every real `.4`/`.16` file in a GOG install, re-encodes, decodes
again, and compares:

```powershell
python tools\encode_pc_gfx.py --roundtrip
python tools\encode_pc_gfx.py --roundtrip --gog "C:\...\Might and Magic 2" --out-root EXTRACTED\pc_gfx_roundtrip
```

| Codec | Status | Verified against |
|-------|--------|-------------------|
| LZW compressor (inverse of `lzw_decompress` @ `MM2.EXE 0x2A42`) | **Byte-identical**, 60/60 wall sheets + 111/111 monster picture blobs | full decompressed payload of every real GOG `.4`/`.16` |
| CGA 2bpp / EGA 4bpp pixel pack | **Byte-identical**, 756/756 frames | every wall-sheet frame in the GOG install |
| Monster nibble-RLE encoder | **Pixel-identical**, 807/807 frames | every combat-sprite frame (base + delta) |
| Monster atlas container rebuild | **Pixel-identical**, 111/111 pictures | full `MONSTERS.4`/`MONSTERS.16` rebuild (fresh blob offsets; frame content decodes identically) |
| Wall-sheet container rebuild (frame-level, byte-exact) | 28/60 sheets byte-identical; rest have a documented gap (below) | `parse_wall_sheet` frame list rebuilt at original offsets |

The LZW compressor's key subtlety (found by fuzzing + testing against every
real file): the decompressor's dictionary is always exactly one entry behind
the compressor's own (the `code >= dict_next` "KwKwK" branch is what makes
that safe for *code values*), so the compressor's code-width growth check
must trigger one entry later than the decompressor's own check
(`dict_next > dict_limit`, not `>=`) to stay bit-aligned. Getting this wrong
silently corrupts output starting at the first width transition (~code 256)
while looking correct on small test files that never reach it.

### Known gap: wall-sheet payload rebuild (encode path)

Packed-u32 wall sheets (`CASTLE`, `CAVE`, `TOWN`, `MASTER`, outdoor biomes, `*B` border sets, …) decode **all** frames correctly via `parse_wall_sheet` / `PcGfx.cpp`, but `encode_pc_gfx.rebuild_wall_sheet_payload` is not yet byte-identical to the original decompressed blob for those sheets (interstitial bytes between packed table entries). This affects **container rebuild only** — LZW round-trip and per-frame pixel encode/decode remain correct.

Grouped-u16 sheets (`CASTLET`, `CAVET`, `TOWNT` on some builds) share the same `[start, end]` interleaving; tools pick grouped u16 or packed u32 by score. Every frame the decoder extracts still re-encodes pixel-perfectly.

**Maze walker export** (`tools/export_map_walker.py`) uses `parse_wall_sheet` for PC `.4`/`.16` PNGs under `wiki/maze-walker/pc/` (386 frames per CGA/EGA variant on GOG). Side-wall cones (frames 4..11) key EGA pen 8 / CGA pen 1 for void; where PC still stores palette **0** inside the Amiga silhouette, export fills colour from the resampled matching `.32` frame (PC index 0 is not always pen-0 void on those frames). Bump `PC_GFX_BUILD_ID` in `wiki/maze-walker/version.js` after re-export.

---

## Related Amiga docs

| Topic | Doc |
|-------|-----|
| `.32` planar sheets | [`06-gfx-loading.md`](06-gfx-loading.md) |
| 3D view composition | [`15-3d-view-and-game-screen.md`](15-3d-view-and-game-screen.md) |
| Combat overview | [`26-combat-overview.md`](26-combat-overview.md) |
| Monster records | [`16-monster-ability-format.md`](16-monster-ability-format.md) |
