# MM2 Graphics File Format Specification

Binary layout for Might and Magic II graphics on **Amiga** (`.32`, `.anm`) and
**PC DOS** (`.4`, `.16`).

Decoders: `editor/src/core/Gfx.cpp`, `tools/decode_pc_gfx.py`, `tools/decode_anm.py`.

Legend for field tables:

| Confidence | Meaning |
|------------|---------|
| **Confirmed** | Matches retail files + traced runtime behaviour |
| **Observed** | Consistent across files; runtime use not fully traced |
| **Unknown** | Present on disk; preserve on round-trip; do not infer |

---

## 1. Format overview

| Extension | Platform | Contents |
|-----------|----------|----------|
| `.32` | Amiga | Multi-frame 32-colour bitmap sheets (walls, floors, UI) |
| `.anm` | Amiga | Animated sprites — TV header + same bitmap codec as `.32` |
| `.4` | PC CGA | LZW-compressed sheets; 2 bpp packed pixels |
| `.16` | PC EGA | Same as `.4`; 4 bpp packed pixels |

**Endianness:** Amiga = big-endian 16-bit (`u16be`). PC = little-endian (`u16le`, `u32le`).

**Monster picture id:** `monsters.dat` byte @ `0x15` (mask `0x7F`).

- Amiga → file `NN.anm` (picture id `N`).
- PC → `MONSTERS.4` / `MONSTERS.16` table slot `N − 1`.

---

## 2. Shared Amiga structure: Image Chunk

Used by `.32` (at offset 0) and `.anm` (after the TV header). This is the actual
bitmap payload in both formats.

### 2.1 Byte layout

```
Offset   Type              Field
------   ----------------  -----
+0       u16be             frame_count
+2       u16be             depth_or_mode
+4       FrameInfo[]       frame_count × 6 bytes
+4+N*6   u16be[32]         palette
+P       byte[]            nibble-RLE data (frame 0, then frame 1, …)
```

**FrameInfo** (6 bytes each):

```
+0   u16be   width
+2   u16be   height
+4   u16be   flags
```

### 2.2 Image chunk — field reference

#### `frame_count` (u16be)

| | |
|---|---|
| **Meaning** | Number of stored bitmaps in this chunk. |
| **Range** | 1–1024 in valid files; typical 1 (floor/UI), 8–36 (outdoor), 27 (ceiling), 32 (dungeon walls). |
| **Decoder use** | Loop `frame_count` times: read one FrameInfo, then decode one nibble-RLE stream. |
| **Confidence** | **Confirmed** |

Each index is one **stored** frame. In `.anm`, stored frame 0 is the base sprite;
stored frames 1…N−1 are patch bitmaps (composed at runtime — section 4.6).

---

#### `depth_or_mode` (u16be)

| | |
|---|---|
| **Meaning** | Sheet category / export mode from the original art pipeline. |
| **Does NOT mean** | Bitplane count — decoding always uses **5 planes**. |
| **Confidence** | **Observed** (values); runtime loader semantics **partially traced**. |

Retail values:

| Value | Typical files | Interpretation |
|-------|---------------|----------------|
| `0` | `townf.32`, `sky.32`, `book.32`, `nwcp.32`, `throw.32`, `*f.32` floors | Single-strip or UI sheets (simple layout) |
| `3` | `town.32`, `castle.32`, `cave.32`, `townt.32`, `desert.32`, `outdoor*.32`, … | Standard multi-frame environment atlases |

Other values not seen in this workspace. Safe to round-trip unchanged.

---

#### `FrameInfo.width` / `FrameInfo.height` (u16be each)

| | |
|---|---|
| **Meaning** | Pixel size of **this stored frame** before decompression. |
| **Decoder use** | Compute `rassize(width, height)` → size of one plane; ×5 for nibble-RLE output length. |
| **Confidence** | **Confirmed** |

Frames in one sheet often share no fixed size (e.g. `town.32` frame 0 = 160×92,
frame 3 = 16×10) — the engine picks frames by index, not by uniform dimensions.

---

#### `FrameInfo.flags` (u16be)

| | |
|---|---|
| **Meaning** | Blit variant hint passed with the frame index into the 3D wall draw path. |
| **Decoder use** | **Ignored** for pixel decode; must be preserved on encode/round-trip. |
| **Confidence** | **Observed** (values); bit semantics **partially traced**. |

Retail patterns:

| Value | Where seen | Likely role |
|-------|------------|-------------|
| `0` | Front-wall frames (0–3), torch-base frames (16–19), ceilings, UI | Default / forward-facing blit |
| `3` | Side-wall frames (4–11, 20–27) in `town.32`, `castle.32`, `cave.32` | Lateral wall geometry (left/right near & far) |
| `3` | **All** frames in outdoor horizon sheets (`desert.32`, `ocean.32`, `outdoor1.32`, …) | Outdoor / horizon blit path |

Wall draw routines @ `0x2C1A` (front), `0x2C9A` (left), `0x2DB4` (right) take
`(depth_slot, frame_index)` and use sheet metadata when blitting from `A4-$7A06`.
The flags word travels with the frame table entry; exact bit breakdown is not
fully documented — treat as an opaque per-frame tag.

---

#### Palette (`u16be[32]`)

| | |
|---|---|
| **Meaning** | 32 colour registers for this chunk, in Amiga **0x0RGB** form (4 bits per channel). |
| **Index 0** | Mask colour — **transparent** at blit time (skipped by blitter @ `-$7C20`). |
| **Indices 1–31** | Opaque pens. |
| **8-bit RGB** | `R = ((word >> 8) & 0xF) * 17` (same for G, B nibbles). |
| **Confidence** | **Confirmed** |

Each `.32` / `.anm` chunk carries its **own** palette. Environment sets (`town.32`,
`castle.32`, …) define the in-dungeon colours; `.anm` combat sprites use per-monster
palettes. Viewport `.anm` overlays (signboards) may load only pens 3–17 into hardware
(colours 4–18) at runtime.

---

### 2.3 Uncompressed frame size

Five bitplanes, word-aligned rows:

```
bytes_per_row = ((width + 15) >> 3) & ~1
rassize(w, h) = height * bytes_per_row
frame_bytes   = 5 * rassize(width, height)
```

Planes are stored back-to-back: plane 0, plane 1, … plane 4.

**Pixel index** at `(x, y)` — bit `7 - (x & 7)` within each plane byte:

```
index = sum over plane p in 0..4 of:  (plane[p][byte_off] >> bit) & 1  << p
```

### 2.4 Amiga nibble-RLE codec

Expands to exactly `frame_bytes` per frame.

```
read byte p
if (p & 0xF0) is 0x00 or 0xF0:
    emit nibble (p >> 4) repeated (p & 0x0F) + 1 times   // only 0x0 and 0xF run
else:
    emit nibble (p >> 4), then nibble (p & 0x0F)           // literal pair
```

Nibbles pack into bytes MSB-first: `(n0 << 4) | n1`.

---

## 3. `.32` — Amiga image sheet

### 3.1 File structure

```
[ Image Chunk @ offset 0 ]
```

Fallback: if offset 0 fails header checks, scan for `FF 00` marker (same as `.anm`).

Each stored frame = one complete bitmap. **No** delta composition.

### 3.2 Typical sheets

| File | Frames | `depth_or_mode` | Role |
|------|--------|-----------------|------|
| `town.32` | 32 | 3 | Walls 0–15; torch variants 16–31 (`+0x10`) |
| `townf.32` | 1 | 0 | Floor backdrop strip |
| `townt.32` | 27 | 3 | Ceiling pieces |
| `intro.32` | 1 | 3 | Title background (single full-screen bitmap) |
| `introclips.32` | 11 | 3 | Title animation cels (pegasus, peekers) |
| `desert.32` | 20 | 3 | Outdoor horizon layers |

Combat art is **`NN.anm`**, not `.32`.

### 3.3 Non-image `.32` files

| File | Content |
|------|---------|
| `globe.32` | XOR-obfuscated copy-protection strings |
| `disk.32` | XOR-obfuscated data blob |

---

## 4. `.anm` — Amiga TV animation file

### 4.1 File structure

```
[ TV fixed header  0x00..0x32 ]
[ Sequence stream  0x33 .. ]
[ FF 00 marker ]
[ Image Chunk      (header starts at byte after FF) ]
```

### 4.2 TV header — field reference

#### Bytes `0x00`–`0x01` (u16be = 0)

| | |
|---|---|
| **Meaning** | Reserved / padding. Always zero in retail files. |
| **Confidence** | **Observed** |

#### Bytes `0x02`–`0x03` — magic `"TV"` (0x54 0x56)

| | |
|---|---|
| **Meaning** | Identifies an animation container (distinct from a plain `.32` sheet). |
| **Confidence** | **Confirmed** |

#### Prelude table `0x04`–`0x2F` (11 slots × 4 bytes)

Fixed array of **11** patch descriptors. Slot `i` describes **stored image frame `i + 1`**.
Frame 0 (base sprite) has no prelude entry.

| Offset in slot | Field | Meaning |
|----------------|-------|---------|
| +0 | `x` (u8) | Horizontal offset on the composed canvas |
| +1 | `y` (u8) | Vertical offset |
| +2 | `width` (u8) | Width of rectangle to **clear** before blitting the patch; also max blit width |
| +3 | `height` (u8) | Same for height |

**Unused slot:** all four bytes = `0xFF`.

| | |
|---|---|
| **Runtime use** | Before blitting patch frame N: clear `(x,y,width,height)`, then blit patch at `(x,y)`. |
| **Confidence** | **Confirmed** (composition matches game + `gfxAnmCompositeFrame()`) |

Most combat `.anm` files use 5–11 active prelude slots (one per patch frame).

---

#### Byte `0x30` — `seq_a` (u8)

| | |
|---|---|
| **Meaning** | Unknown control byte from the original export tool. |
| **Observed range** | 19–75 across retail `.anm`; varies per monster, **not** equal to canvas width/height. |
| **Runtime use** | Not used by current decoders or the game remake overlay path. |
| **Confidence** | **Unknown** — preserve on round-trip. |

---

#### Byte `0x31` — `seq_b` (u8)

| | |
|---|---|
| **Meaning** | Sequence-table hint + flag bit. |
| **Confidence** | **Observed** (correlation); **do not use for parsing**. |

| Bits | Meaning |
|------|---------|
| 0–6 | Hint for number of `FF`-delimited sequence blocks. Often matches parsed count when bit 7 clear (e.g. `0x03` → 3 sequences, `0x85` → 5 sequences). |
| 7 (`0x80`) | Unknown flag. Set in ~46% of files, often when multiple sequences exist. |

**Unreliable examples:** `02.anm` has `seq_b = 0x01` but **5** parsed sequences;
`06.anm` has `seq_b = 0x01` but **4** sequences. Always parse by scanning for
`FF 00`, not by this byte.

---

#### Byte `0x32` — `seq_c` (u8)

| | |
|---|---|
| **Meaning** | Unknown. |
| **Observed** | `0` in 57/72 files; `5` in 14 files; `30` in 1 file (`15.anm`). |
| **Confidence** | **Unknown** — preserve on round-trip. |

---

### 4.3 Sequence stream (offset `0x33` …)

Runs until the `FF 00` image-chunk marker.

#### Sequence block grammar

```
(frame_index, delay) (frame_index, delay) …
0xFF                          // end this sequence → start next block
…
0xFF 0xFF                     // optional: end of all sequences
FF 00                         // image chunk marker (not part of sequences)
```

#### `(frame_index, delay)` pair

| Field | Meaning |
|-------|---------|
| `frame_index` (u8) | **Composed** animation state (section 4.5). `0` = base frame only. `3` = base + apply patch 3 after prelude clear. **Not** a raw stored-frame index for patches (except index 0). |
| `delay` (u8) | Hold time in **game ticks** (~60 Hz). `0` = advance on next tick (editor treats as minimum 1 tick). |

#### Typical sequence roles (combat `.anm`, **observed**)

| Block index | Typical content | Example use |
|-------------|-----------------|-------------|
| 0 | Small oscillation through patch indices 1–5 | Idle / breathing |
| 1 | Reverse oscillation back to 0 | Idle return |
| 2 | — | Hit / hurt (varies) |
| 3 | Cycle through frames 3–7 with delay 0 | Walk / sway cycle |
| 4+ | Attack, cast, etc. | Varies per monster |

Which block the combat engine selects for idle vs attack is **context-dependent**
(not fully traced here). Export tools pick the longest cycle for showcase GIFs.

Signboard / world overlays (e.g. `62.anm`) often use **sequence block 0 only**;
`ViewportAnmOverlay` plays block 0 when present, else flipbook through composed frames.

---

### 4.4 Image chunk marker `FF 00`

| | |
|---|---|
| **Meaning** | End of TV metadata; start of standard Image Chunk (section 2). |
| **Layout quirk** | Marker is two bytes: `FF` then `00`. Chunk header `frame_count` begins at the **`00`** byte (offset of second marker byte). |
| **Confidence** | **Confirmed** |

---

### 4.5 Composed frame algorithm

Stored frames ≠ displayed frames (except frame 0).

```
compose(k):
    draw stored_frame[0] full size onto canvas

    if k == 0: return

    slot = prelude[k - 1]
    clear canvas[slot.x : slot.x+slot.w, slot.y : slot.y+slot.h]
    blit stored_frame[k] at (slot.x, slot.y), clip to slot.w × slot.h
    // pen 0 in patch is transparent
```

Sequence `frame_index` values are **`k`** in this function.

---

## 5. PC LZW wrapper

All `.4` / `.16` files start with:

| Offset | Field | Meaning |
|--------|-------|---------|
| +0 | u32le `uncompressed_size` | Exact byte count after decompression |
| +4 | LZW bitstream | Payload compressed with MM2 custom LZW |

**LZW:** codes 9→12 bits, clear = `0x100`, dictionary growth one entry behind encoder
(see `tools/mm2_lzw.py`). Decompress, then parse as wall sheet (§6) or monster blob (§7).

---

## 6. `.4` / `.16` — PC wall / environment sheet

Decompressed layout is **identical** for CGA and EGA; only pixel packing differs (§6.4).

### 6.1 Decompressed header — field reference

#### Byte +0 — `control` (u8)

| Bits | Field | Meaning |
|------|-------|---------|
| 0–5 | `table_slot_count` | Number of entries in the offset table at +2. **Not always** equal to decodable frame count on grouped layouts. Max 63. |
| 6–7 | `format_class` | Passed to `gfx_format_class_check` @ `0x5C5A` — selects which decode/dispatch path runs. **All 0** on GOG retail. |

| | |
|---|---|
| **Confidence** | Low 6 bits **confirmed**; `format_class` **observed** (always 0 here). |

---

#### Byte +1 — `flags` (u8)

| | |
|---|---|
| **Meaning** | Viewport / frame-index scaling controls for `pick_frame_from_sheet` @ `0x5D4A`. |
| **Confidence** | **Partially traced** |

| Bits | Role |
|------|------|
| 0–3 | Low nibble. Always `0` on GOG wall sheets. Purpose unknown. |
| 4–5 | Used by `frame_index_scale` @ `0x42EA` with runtime word `0x9E0C` to map viewport depth/facing → logical frame index into the offset table. |
| 6–7 | Unused in traced path. |

When `flags = 0` and scale = 0, runtime defaults logical frame budget to **8**
(`mov word ptr [0x9E0C], 8`). This matches UI sheets like `MASTER.16` menu sprites.

Decoders can ignore this byte for **static extraction** of all frames; the game
uses it for **which single frame** to show per wall slot.

---

### 6.2 Frame offset table (offset +2)

Three on-disk layouts — pick the interpretation that yields the most valid frames.

| Variant | Entry format | Notes |
|---------|--------------|-------|
| **packed u32** | `(end << 16) \| start` per slot | Common on `TOWN`, `CASTLE`, `DESERT`, `MASTER.16` |
| **plain u32** | byte offset per slot | Some small sheets (`THROW`, …) |
| **grouped u16** | `[start, end]` pairs | Even slots = frame starts; odd slots often = end offset of previous frame |

Runtime indexing: u16 grouped view uses `logical_index × 2`; packed u32 uses `index × 4`.

---

### 6.3 Per-frame record (walls)

| Offset | Field | Meaning |
|--------|-------|---------|
| +0 | u16le `width` | Pixel width |
| +2 | u16le `height` | Pixel height |
| +4 | bytes | Packed pixel rows — **full frame**, no composition |

### 6.4 Pixel packing

**CGA (`.4`):** 2 bpp, 4 pixels/byte, MSB first. Palette 1: black, cyan, magenta, white.

**EGA (`.16`):** 4 bpp **linear** (hi nibble = left pixel). Standard IBM 16-colour indices.
Not EGA planar on disk.

---

## 7. `MONSTERS.4` / `MONSTERS.16` — PC combat atlas

### 7.1 Atlas file header

| Entry | Meaning |
|-------|---------|
| `u32le[0]` | Header byte size **and** file offset of first blob (GOG: **300**) |
| `u32le[k]` | File offset of LZW blob for picture id **`k + 1`** |
| `0` | Empty slot — no art for that picture id |

Picture id `N` → seek `entry[N − 1]`, LZW-decompress (§5), parse §7.2.

GOG: 75 entries. Some picture ids share the same blob offset (duplicate art).

---

### 7.2 Decompressed monster blob — field reference

#### Byte +0 — `control` (u8)

Same layout as wall sheets:

| Bits | Field |
|------|-------|
| 0–5 | `frame_count` — number of stored frames (base + patches), 3–12 typical |
| 6–7 | `format_class` — always 0 on GOG |

---

#### Byte +1 — `flags` (u8)

| | |
|---|---|
| **Meaning** | Unknown runtime flags for combat sprite load. |
| **Observed** | Always **0** on all GOG `MONSTERS.16` blobs. |
| **Confidence** | **Unknown** — preserved by `encode_pc_gfx.py`. |

---

#### Bytes +2 … — `frame_offset[]` (u16le × `frame_count`)

| | |
|---|---|
| **Meaning** | Byte offset from **start of decompressed blob** to each frame record (§7.4). |
| **Order** | Index 0 = base sprite; 1…N−1 = delta patches (same order as Amiga prelude slots). |
| **Gap** | Bytes from end of this table to first `frame_offset` hold **animation scripts** (§7.3). |

---

### 7.3 Animation scripts (between offset table and first frame)

Same encoding as Amiga §4.3:

```
(frame_index, delay) pairs …
0xFF              // end script
0xFF 0xFF         // end all scripts
```

| Field | Meaning |
|-------|---------|
| `frame_index` | Composed state on 96×96 canvas (§7.5) |
| `delay` | Combat tick hold count |

**Observed script roles** (same as Amiga):

- Script **0** — idle sway through low patch indices.
- Script **3** — walk cycle `(3,0)(4,0)…(7,0)…` on many monsters.
- Longest script often chosen for export GIFs (`_pick_primary_script` in tools).

---

### 7.4 Frame record — field reference

| Off | Size | Field | Meaning |
|-----|------|-------|---------|
| +0 | 1 | `x` | Blit X on **96×96** combat canvas |
| +1 | 1 | `y` | Blit Y |
| +2 | 1 | `width` | Patch width (decode grid width) |
| +3 | 1 | `height` | Patch height |
| +4 | var | `stream` | PC monster nibble-RLE (§7.6) |

| Frame | Typical values | Role |
|-------|----------------|------|
| 0 | `x=0, y=0`, full sprite size | Complete base monster bitmap |
| N > 0 | Non-zero `x,y`, smaller `w×h` | Delta patch; `(x,y,w,h)` = Amiga prelude slot `N−1` for same picture |

---

### 7.5 Composed frame (PC combat)

Fixed **96×96** canvas (`CGA.DRV` / `EGA.DRV` off-screen buffer).

```
compose(k):
    canvas = 96×96 transparent
    decode & blit frame 0
    if k > 0:
        rec = frame[k]
        clear (rec.x, rec.y, rec.w, rec.h)
        decode & blit stream at (rec.x, rec.y)
```

---

### 7.6 PC monster nibble-RLE

**Not** the Amiga bitplane codec (§2.4). Each token:

```
run_length = (byte >> 4) + 1
code       = byte & 0x0F
```

Runs **wrap across rows** until `height` rows decoded.

**CGA:** code 0–3 = colour; code ≥ 4 = transparent skip.

**EGA:** code 5 = transparent; else `colour = XLAT[code]`:

```
XLAT = [0,1,2,9,6,8,10,3,4,5,7,11,12,13,14,15]
```

---

## 8. Quick reference

| Format | Composition? | Transparency |
|--------|--------------|--------------|
| `.32` | No | Pen 0 |
| `.anm` | Yes — base + patches | Pen 0 |
| PC walls | No | Opaque |
| `MONSTERS.*` | Yes — base + patches | RLE transparent codes |

---

## 9. Validation tools

```powershell
python tools\encode_image32.py --roundtrip
python tools\decode_anm.py path\to\54.anm
python tools\decode_pc_gfx.py TOWN.16 --extract-all
python tools\decode_pc_gfx.py MONSTERS.16 --monsters --extract-all
python tools\encode_pc_gfx.py --roundtrip
```

---

## 10. Still unknown (preserve on round-trip)

| Field | Notes |
|-------|-------|
| `seq_a`, `seq_c` | `.anm` TV header — no confirmed runtime consumer |
| `seq_b` bit 7 | Flag only; do not parse sequences from this byte |
| `FrameInfo.flags` bit layout | Values 0 vs 3 observed; individual bits not decoded |
| PC wall `flags` bits 4–5 | Partially traced via `frame_index_scale`; full table TBD |
| PC wall / monster `format_class` | Always 0 on GOG; other builds untested |
| Monster blob `flags` | Always 0 on GOG |
| Combat script selection | Which sequence index = idle / walk / attack per engine state |
