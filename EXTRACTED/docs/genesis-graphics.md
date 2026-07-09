# Genesis graphics (Might and Magic II)

Initial RE notes for the **1991 Sega Genesis / Mega Drive** port (*Might and Magic II: Gates to Another World*). **Confirmed facts only**; gaps marked explicitly.

| Artifact | Path |
|----------|------|
| ROM image | `EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen` |
| Static analysis | `EXTRACTED/genesis/analysis/gfx_static.json` |
| Gfx scan | `EXTRACTED/genesis/analysis/gfx_scan.json` |
| Gfx trace | `EXTRACTED/genesis/analysis/gfx_trace.json` |
| Boot disasm | `EXTRACTED/genesis/asm/boot_0200.asm`, `boot_710a.asm` |
| A5 vectors | `EXTRACTED/genesis/asm/a5_vectors_0312.asm` |
| VDP init | `EXTRACTED/genesis/asm/vdp_init_0706.asm` |
| VDP upload | `EXTRACTED/genesis/asm/vdp_helper_2dba.asm` |
| Resource / LZ | `EXTRACTED/genesis/asm/resource_29954.asm` |
| VDP DMA | `EXTRACTED/genesis/asm/decompress_29f7e.asm` (misnamed — see below) |

---

## RE approach

**Trace the Genesis VDP path, not the Amiga/PC asset containers.**

Boot chain (confirmed):

`0x200` TMSS → `0x300` checksum → `0x6C9AA` sets **`A5 = 0x312`** → `0x710A` → **`JSR 0x706`** (VDP init).

Gfx I/O pipeline:

| Step | Routine | Role |
|------|---------|------|
| `$0C(A5)` | `0x29954` | Resource load — **custom LZ** decompress from inline table payload |
| `$06(A5)` | `0x29F7E` | **VDP DMA** upload (source = 68k address, typically RAM `0xFF0002` / `0xFF1000`) |
| `$2DBA` | `0x2DBA` | CPU word loop to VDP data port `$C00000` |

**Not applicable:** Amiga blitter/planar `.32`, PC CGA/EGA, x86 LZW, blind Kosinski.

---

## ROM provenance (confirmed)

| Check | Result |
|-------|--------|
| File | 768 KiB (786,432 bytes) |
| MD5 | `3238047522caa4e27be2720da924777e` |
| Serial | `T-50166` (Electronic Arts) |
| Identity | `MM2` @ `0xA50A`; subtitle `Gates to Another` @ `0xB604` |

---

## A5 globals / jump vectors (confirmed)

Runtime **`A5 = 0x312`** (`MOVEA.L #0x312,A5` @ `0x6C9AA`, `0x710A` path).

`JSR d(A5)` uses a **ROM jump vector table** at `0x312` (not a RAM struct). Each slot is `JMP abs`:

| A5+disp | Target | Role |
|---------|--------|------|
| `+0x00` | `0x29DE0` | I/O read helpers |
| **`+0x06`** | **`0x29F7E`** | **VDP DMA setup** (regs `$93`–`$97`) |
| **`+0x0C`** | **`0x29954`** | **Resource / LZ decompress** |
| `+0x66` | `0x29A22` | LZ variant (multi-chunk / `$68`-stride tables) |

Disassembly: `EXTRACTED/genesis/asm/a5_vectors_0312.asm`.

Negative offsets from `A5` (`-$3778(A5)`, etc.) are **game-state fields** in RAM below the vector table base address convention used throughout the port.

---

## Resource table layout (confirmed)

Tables are referenced via `PEA (table,PC)` before `JSR $0C(A5)`.

| Table (ROM) | word0 | word1 (out size) | Boot use |
|-------------|-------|------------------|----------|
| `0xC52` | `0x607` | `0x942` | Tiles → VRAM `$4080` via `$2DBA` |
| `0x13D8` | `0xBFC` | `0x1342` | LZ → RAM → DMA VRAM `$49C0` |
| `0x1262` | `0x16D` | `0x18C` | LZ → RAM `$FF1000`, nametable patch, `$2DBA` → VRAM `$2000` |
| `0xB12` | `0x98` | `0x182` | Palette → CRAM `$0000` |
| `0xBB2` | `0` | `0` | Secondary `$06(A5)` call @ `0x7F2` (flags/size on stack) |
| `0xBD2` | — | — | Small VRAM upload @ `0x87E` |
| `0x1FDC` | — | — | VRAM `$5D20` @ `0x924` |

**Header (8 bytes, big-endian):**

| Offset | Field |
|--------|-------|
| `+0` | `u32` **compressed payload length** (bytes consumed from the stream) — confirmed byte-exact for all 4 boot tables |
| `+4` | `u32` decompressed output byte count (the `D0` loop counter) |
| `+8` | **Inline LZ bitstream** (confirmed: `ADDQ #8,A0` @ `0x299A4`) |

The `+0` word is **not** a stream pointer (as previously guessed) — it is the
exact compressed byte count. Decoding `+4` output bytes consumes exactly `+0`
input bytes for every boot table (see validation below). It is not read by
`0x29954` itself but is used by the `0x29A22`/`0x29AE8` variants (`MOVE.W (A3),D1`
after `SUBQ #4,A3`) as a per-chunk stride counter.

Parsed JSON: `gfx_static.json` → `resource_tables`.

---

## Boot VDP uploads @ `0x706` (confirmed)

From `vdp_init_0706.asm` / `gfx_static.json` → `boot_vdp_uploads_confirmed`:

| Call | Dest | Length | Path |
|------|------|--------|------|
| `0x776` | VRAM `$4080` | `0x940` | `$0C` + `$2DBA` |
| `0x7A0` | VRAM `$49C0` | `0x1340` | `$0C` + `$06` DMA from `0xFF0002` |
| `0x7DA` | RAM `$FF1000` | `0x2000` | `$0C` + `$06` |
| `0x83E` | VRAM `$2000` | `0x2000` | `$2DBA` from `$FF1000` (after nametable patch @ `0x800`) |
| `0x8AA` | CRAM `$0000` | `0x180` | `$0C` + `$2DBA` (`dest_type = 0xC000`) |
| `0x924` | VRAM `$5D20` | `0x12E0` | `$0C` + `$2DBA` |
| `0x976` | VRAM `$0F00` | `0x380` | `$2DBA` from `$FFFF0004` |

`$2DBA` helper: set `$8F02` @ `$C00004`, latch VRAM/CRAM address, word loop to `$C00000`. See `vdp_helper_2dba.asm`.

All `$2DBA` BSR sites (static scan): `gfx_static.json` → `vdp_upload_calls_all`.

---

## Compression (confirmed — Okumura LZSS, byte-exact)

The codec is **Haruhiko Okumura LZSS**, decoded bit-exact from
`resource_29954.asm`. All parameters derived directly from the 68000.

| Item | Detail | ASM anchor |
|------|--------|-----------|
| Codec | Okumura LZSS | `0x29954` |
| Ring buffer | **4096 bytes** (`N`), mask `0xFFF` | `ANDI.W #$FFF` @ `0x299FA` |
| Ring pre-fill | `0x20` (space) for first `0xFEE` bytes | fill loop `0x29966` |
| Write pointer start | `r = 0xFEE` (`N − F`) | `MOVE.L #$FEE,D1` @ `0x299A6` |
| Control | 8-bit flag stream, **LSB first**, refilled `ORI.W #$FF00` | `0x299BE` |
| Literal | flag bit `1` → one raw byte | `0x299CA` |
| Match | flag bit `0` → 2-byte token `(b0,b1)` | `0x299DE` |
| Match offset | 12-bit = `b0 \| ((b1 & 0xF0) << 4)` | `0x299E4–0x299EC` |
| Match length | **`(b1 & 0xF) + 3`** (`THRESHOLD 2`, min match 3) | inner loop `0x299F6` copies `d4+1`, `d4=(b1&0xF)+2` |
| Copy source | `ring[(offset + k) & 0xFFF]`, written back to `ring[r]` | `0x299FE–0x29A04` |

> **Correction:** the match length is `(b1 & 0xF) + 3`, **not** `+2`. The inner
> loop at `0x299F6` executes `d4 + 1` times (`CMP.W D5,D4 / BGE`), giving
> `(b1 & 0xF) + 3` copied bytes — the classic LZSS minimum match of 3.

### Variants in the same routine

| Entry | Role | Difference from `0x29954` |
|-------|------|----------------------------|
| `0x29954` | plain decode → linear dest | baseline |
| `0x29A22` | `$68`-stride banked output | resets `A1 = A4` and `A4 += $68` every chunk (chunk count = `(A3)` = header `+0`); identical bitstream |
| `0x29AE8` | masked / nibble-expanded write | maps each byte through table `@0x28E52` (`00 10 20 30 …`) and `AND`/`OR`s into dest (CRAM / attribute writes); identical bitstream |

### Validation (static, no emulator) — `analysis/lz_validation.json`

Two independent checks, both pass for all 4 boot tables:

| Table | `+0` (compressed) | `+4` (out size) | decoded len | consumed len | out match | consumed==meta |
|-------|-------------------|-----------------|-------------|--------------|-----------|----------------|
| `0xB12` | `0x98` | `0x182` | `0x182` | `0x98` | yes | yes |
| `0xC52` | `0x607` | `0x942` | `0x942` | `0x607` | yes | yes |
| `0x1262` | `0x16D` | `0x18C` | `0x18C` | `0x16D` | yes | yes |
| `0x13D8` | `0xBFC` | `0x1342` | `0x1342` | `0xBFC` | yes | yes |

The **consumed == metadata** match is the decisive test: decoding exactly `+4`
output bytes consumes exactly `+0` input bytes, i.e. token parsing lands on the
recorded compressed size. A wrong match-length (e.g. `+2`) misaligns and lands
at `0xC7 / 0x674 / 0x171 / 0xE0F` instead — none match. The `tools/…` encoder
also round-trips (`--self-test`), including RLE overlap matches.

### Rendered graphics — `EXTRACTED/genesis/gfx/`

Decoded blobs rendered as standard Genesis 4bpp planar tiles (32 B/tile) with
the `0xB12` palette decoded as CRAM 9-bit BGR (`palette_B12.png`). Individual
tiles show **coherent structure** — regular repeating tile/font patterns,
background-index (0) separation, vertical strokes — not random noise, and
decoded entropy (2.8–4.4 bits) is far below the compressed stream (5.6–7.2).
Fully recognizable on-screen images require **VDP nametable assembly**, which is
not applied in these raw linear tile grids.

### Scanned resource pool — `analysis/lz_scan_30000.json`

Scanning `0x30000`–`0xC0000` for `+0`/`+4` headers where decode consumes exactly
`+0` bytes **and** entropy drops ≥0.3 bits found **169 non-overlapping tables**
spanning `0x37C00`–`0x94F40` (ratios ~1.8–4x). Spot-rendered samples
(`0x37C00`, `0x401DA`) show tile/font structure. This is the environment/gfx
resource pool.

---

## Resource catalog — all 169 LZ tables classified

Tool: `tools/catalog_genesis_lz.py` → `analysis/tile_catalog.json` + `gfx/catalog/*.png`.
Every table from `lz_scan_30000.json` is decoded with the ASM-exact codec and
classified by **decoded byte structure** (not guesswork):

| Class | Count | Signal (confirmed) | Content |
|-------|-------|--------------------|---------|
| `palette` | 2 | ≥97% valid 9-bit BGR555 words, ≤512 B | CRAM palettes (4×16 each) |
| `chr` | 24 | dense 4bpp pixels: high-byte-of-word rarely 0, many bytes ≥0x80 | **graphics tile banks** |
| `tilemap` | 48 | 16-bit index arrays: high byte ≈0 for ~all words, low mean | screen/view tile-arrangement layers |
| `text` | 90 | ≥85% printable ASCII, ~no bytes ≥0x80, in `0x804D2`–`0x910E8` | length-prefixed strings |
| `data` | 5 | in the string pool, not printable | string-table index/record blobs |

169 = 2 + 24 + 48 + 90 + 5. Region split is confirmed by content: graphics pool
`0x37C00`–`0x6F878`, string pool `0x804D2`–`0x910E8`, trailing gfx group `0x9389E`–`0x94F40`.

### Text pool confirmed (was previously unclassified "tables")

Decoding `0x80000+` tables yields length-prefixed ASCII, e.g.:
`"Awaken / Detect Magic / Energy Blast / Flame Arrow …"` (spell names, `0x81092`),
`"Small Club / Small Knife / Dagger …"` (items, `0x8233C`),
`"Town of Middlegate / Town of Atlantium …"` (`0x82B9A`),
`"Stairs going up. Ascend?"` (dialog, `0x910E8`). **~90 of the 169 tables are
text, not graphics** — an important correction to the earlier "environment/gfx
resource pool" framing of the whole `0x30000+` range.

### Palettes (confirmed)

| Source | Bytes | Notes |
|--------|-------|-------|
| **`0x6FBEA`** (raw CRAM) | 4×`$20` | **Env / first-person view palette bank** — ASM @ `0x8140` |
| `0x6D03C` (raw CRAM) | `$20` | UI / alternate line — loaded when `-$4C4(A5)≠1` @ `0x81A8` |
| `0x6F858` (raw CRAM) | `$20` | sits immediately before tilemap `0x6F878` |
| `0x9389E` / `0x94F40` (LZ) | 128 | trailing group — **not** the live env CRAM path |
| `0xB12` (boot LZ) | 386 | boot/title CRAM |

**Env CRAM bank (ASM-exact):** `MOVE.L #$6FBEA,(A7)` then
`ADD.L #line*$20` + `JSR $72(A5)` @ `0x8140`, selected by `-$4C6(A5)` /
`-$4C4(A5)`:

| Line | Addr | `-$4C6` | Theme (strip pool) |
|------|------|---------|---------------------|
| L0 | `0x6FBEA` | 2 or 5 | **castle** grey brick (`0x6FFAE`–`0x776F4`) |
| L1 | `0x6FC0A` | 0 | **town** rough stone (`0x7780A`–`0x803A8`) |
| L2 | `0x6FC2A` | 1 | **cavern** — **same strips as town**, recolour |
| L3 | `0x6FC4A` | (outdoor via `-$4C4==1`) | **outdoor** greens (`0x9E846`–`0xA5896`) |

Loc-id → theme mapper @ `0xFB86`; strip fill jump table @ `0xFC56`
(themes 0/1 share town art; 2/5 castle; 3/4/6 outdoor). **No separate cavern
strip pool** on Genesis (unlike Amiga `cave.32`).

Swatch: `gfx/catalog/env/atlases/palettes_L0_L1_L2_L3.png`.

### CHR graphics banks (24) — `gfx/catalog/chr/*.png`

Coherent tile material is visible with a real palette (not noise; decoded entropy
2–4.8 vs 5.6–7.2 compressed). Content guesses below are from **eyeballing raw
tile grids** and are honest — exact on-screen images still need tilemap assembly.

| CHR table | Tiles | Content (honest) |
|-----------|-------|------------------|
| `0x943B8` | 184 | **brown rocky / cave-stone texture w/ orange embers** — strong wall/dungeon-surface candidate |
| `0x57DF0` | 327 | brown stonework + green foliage + blue — outdoor/castle scene, largest bank |
| `0x47090` | 215 | dense irregular stone/rock texture — environment/wall candidate |
| `0x5AB32` | 259 | textured environment tiles |
| `0x44344` | 201 | textured environment tiles |
| `0x6B834` | 192 | textured environment tiles |
| `0x6A032`, `0x62788`, `0x4AE9A`, `0x41042`, `0x5E0BE`, `0x61A0C`, `0x4365C`, `0x57326`, `0x4CDB0`, `0x42ABE`, `0x5BF3C`, `0x4D8E8`, `0x5FA74`, `0x4865A`, `0x66642`, `0x5D808`, `0x6F2E0` | 53–189 | environment / sprite / UI tile banks — **unconfirmed** |
| `0x6E9A2` | 256 | **font / charset** — digits + letters clearly visible (confirmed by eye) |

### Tilemaps (48) — UI / secondary layouts (not the 3D walls)

The 48 `tilemap` tables (`0x37C00`–`0x6D364`) are 16-bit tile-index arrays.
Gameplay loads confirmed via `MOVE.L #imm,(A7)` + `JSR $0C(A5)`:

| Table | Class | Use (ASM) |
|-------|-------|-----------|
| `0x6E9A2` | CHR (font) | → VRAM `$2000` @ `0x4374` |
| `0x6F520` / `0x6F878` | tilemap | UI / overlay maps (read with row stride `$80` = 64 tiles @ `0x43DA`) |
| `0x6D05C` | tilemap | DMA path @ `0x4686` |
| `0x6F2E0` | CHR | paired with `0x6D05C` @ `0x46B6` |

Blind VDP-nametable assembly of these against CHR banks still speckles for the
non-UI maps — they are **not** the first-person wall path.

Trailing self-contained group `0x9389E · 0x940C2 · 0x943B8 · 0x94C4A · 0x94F40`
(palette · tilemap · CHR · tilemap · palette) yields coherent **11×11** face
blocks when the 1452-word maps are sliced at width 11
(`gfx/catalog/walls/wall_faces_{A,B}_11x11.png`) — useful cave/rock material,
but secondary to the strip pool below.

### First-person walls — **found** (env strip pool)

Genesis MM2 does **not** compose the 3D view from nametable tilemaps. Walls and
doors are **pre-rendered 4bpp perspective strips**, loaded through the banked LZ
entry `$66(A5)` = `0x29A22`.

| Item | Detail | ASM anchor |
|------|--------|------------|
| Env table fill | writes ROM ptrs into `-$35D2(A5)` … | `0xFC40`–`0x102xx` |
| Load | `MULU #$50,D0` + `MOVE.L d8(A4,D0),-(A7)` + `JSR $66(A5)` | `0x3E5C`… |
| Banked write stride | `$68` bytes | `ADDA.W #$68,A4` @ `0x29A76` |
| Resource prefix | `u16 chunk_bytes` @ `table-4`, `u16 row_count` @ `table-2` | read via `A3=table; SUBQ #4,A3; MOVE.W (A3),D1` |
| Bitmap | linear 4bpp, `width = chunk_bytes*2`, `height = out_size/chunk_bytes` | — |

**86 resources** in three strip pools (castle / town+cavern / outdoor).
Catalog: `analysis/env_lz_catalog.json`.

| Chunk (B/row) | Size (px) | Content |
|---------------|-----------|---------|
| 8 / 12 / 16 | 16–32 × 120 | Side-wall perspective strips |
| 24 / 28 / 29 / 30 / 48 | 48–96 × 28–55 | Near-field wall / door / mountain faces |
| 80 | 160 × 91–96 | Full door / outdoor panels |
| 13 | 26 × 91 | Narrow filler / edge strips |

Tools: `tools/rip_genesis_walls.py`, `tools/atlas_genesis_walls.py`.

**Atlases** (`tools/atlas_genesis_walls.py` → `gfx/catalog/env/atlases/`):

| Atlas | Theme / palette |
|-------|-----------------|
| `atlas_castle_*_L0.png` | Castle grey brick — **L0** (user-confirmed) |
| `atlas_town_*_L1.png` | Town bluish stone — **L1** (user-confirmed) |
| `atlas_cavern_*_L2.png` | Caverns — **same art as town**, **L2** (user-confirmed) |
| `atlas_outdoor_*_L3.png` | Outdoor mountains/trees — **L3** (user-confirmed) |
| `atlas_themes_doors.png` | One door/panel contact sheet across all four themes |
| `palettes_L0_L1_L2_L3.png` | All four CRAM lines |

Per-strip rips: `strip_*_L0.png` / `_L1_town.png` / `_L2_cavern.png` / `_L3.png`.

### Monsters / combat graphics — **partial** (catalog `$11A`)

Picture art lives in the **`$11A(A5)` = `0x33CF4`** resource catalog (base
`0x33D4A`), not in the env-strip / event-VM / SFX paths ruled out earlier.

| Piece | Detail | ASM |
|-------|--------|-----|
| Catalog walk | `u16 id` → CHR LZ + layout LZ + 16-colour CRAM | `$11A` @ `0x33CF4` |
| Meta | `0x913B4` via `0x74CE` — `w2` / `w3` / extras | `0x74CE` |
| Anim rows | `0x91738` via `0x7512` — keyed by meta `w3`; each word **hi=layout row, lo=duration** | `0x7512` |
| Combat still | Row **0** pushed into blit `@0x7788` (`@0x7756`) | `@0x7756` |
| Plane stride | `$F2` bytes = **121** tile words (linear upload stream) | blit `@0x7788` / `@0x7900` |
| SAT mosaic | Hardcoded **9 sprites** (sizes sum to 121) for **every** `$11A` load | `@0x761A` |
| Combat entry | `$240(A5)=0x7556` → same loader; base XY `$AE`/`$CC` | e.g. `@0x2676C` |

**Per-entry layout:**
1. CHR = Okumura LZ @ `entry+2` (standard 8-byte header).
2. Layout = LZ words: **121 × N** (= animation frames). Each row is a **linear**
   tile stream consumed in SAT sprite order (not a 2D nametable).
3. Trailing **32 bytes** = CRAM line (per-picture palette).
4. Meta.`w2` is **not** read for placement; `0x91738` (via `w3`) lists anim
   **layout rows** (high byte of each u16); low byte = tick duration.
5. Composer = fixed SAT mosaic `@0x761A` (VDP size bytes `$0F`/`$0E`/`$0B`/`$0A`):
   **4×4 / 4×4 / 3×4 / 4×4 / 4×4 / 3×4 / 4×3 / 4×3 / 3×3** (W×H tiles).
   Static combat/UI portrait = **layout row 0** via `@0x7788`.
6. Blit simulator (`simulate_blit7788`) matches direct compose (pixel-identical);
   place table does **not** encode SAT slot visibility.

**69 catalog ids** (1–31, 33–49, 51, 53–63, 65–69, 72, 74, 75, 158). Missing
from 1–60: **32, 50, 52**.

Tool: `tools/rip_genesis_monsters.py` → `gfx/catalog/monsters/` +
`atlases/atlas_monsters.png`. Catalog JSON:
`analysis/monster_catalog.json`.

**Caveat:** Visual fidelity of the universal mosaic for all 69 ids still needs
emulator cross-check; some creatures may look fragmented if their art was
authored for a different packing.

**Still ruled out:** depth panels `0xA59B0+`, globe `$14A`, event VM `@0x2C18C`,
combat SFX `$22E`.

---

## Emulator validation

### Kega Fusion (user-provided)

| Item | Result |
|------|--------|
| Path | `C:\Users\Adam Templeton\Downloads\Genesis + Roms\New Folder\Fusion.exe` |
| CLI | `Fusion.exe rom.gen [-gen] [-usa\|-eur] [-fullscreen]` — ROM path only; no VRAM flags |
| Save states | F5/F8 quick slots (`.GS0`–`.GS9`); File menu `.GSX` |
| VRAM export | **Not available** in shipped manual / menus |
| Debug | `DebugFlags` in `Fusion.ini` only; no documented debugger UI |
| Automated RE run | **Failed** on RE host (DirectX); **manual F5** save → `emulator/mm2_boot.gst` (140408 B, `GST@`) |

Manual workflow and output folder: `EXTRACTED/genesis/emulator/README.md`.

Compare helper:

```powershell
python tools\genesis_vram_dump_stub.py --manual
python tools\genesis_vram_dump_stub.py --fusion-state EXTRACTED\genesis\emulator\mm2_boot.gsx --compare-lz EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen"
python tools\genesis_vram_dump_stub.py --dump EXTRACTED\genesis\emulator\vram_boot.bin --compare-lz EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen"
```

### LZ status (static validation)

| Check | Status |
|-------|--------|
| Codec bit-exact vs `resource_29954.asm` | **Yes** — all params derived from ASM (match length corrected to `+3`) |
| Output length == header `+4` | **Yes** — 4/4 boot tables |
| Consumed length == header `+0` | **Yes** — 4/4 boot tables (decisive test) |
| Encoder round-trip (`--self-test`) | **Yes** — incl. RLE overlap |
| Decoded output is coherent tiles | **Yes (structure)** — real tile/font patterns + entropy drop; full images need nametable |
| Emulator VRAM byte-match | **Not attempted** (not required for static validation) |

The codec is now considered **confirmed** by static analysis. Prior emulator
GST comparisons that showed "no match" predate the `+3` length fix and used a
stub decoder; they are superseded.

---

## Tools

```powershell
python tools\analyze_genesis_gfx.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" -o EXTRACTED\genesis\analysis\gfx_static.json
python tools\scan_console_gfx.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" -o EXTRACTED\genesis\analysis\gfx_scan.json
python tools\trace_console_gfx.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" -o EXTRACTED\genesis\analysis\gfx_trace.json
python tools\disasm_genesis_m68k.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" --offset 0x706 --length 0x300 -o EXTRACTED\genesis\asm\vdp_init_0706.asm
python tools\genesis_lz_decompress.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" --offset 0xC52 -o out.bin
python tools\genesis_vram_dump_stub.py --manual
```

| Tool | Output | Use |
|------|--------|-----|
| `analyze_genesis_gfx.py` | `gfx_static.json` | A5 vectors, resource headers, boot upload table |
| `genesis_lz_decompress.py` | `.bin` | LZSS decode/encode; `--self-test` round-trip (ASM byte-exact) |
| `render_genesis_lz_tiles.py` | `gfx/*.png` | Render decoded blobs as 4bpp tiles + CRAM palette |
| `scan_genesis_lz_tables.py` | `analysis/lz_scan_*.json` | Scan ROM for `+0`/`+4` header LZ tables |
| `catalog_genesis_lz.py` | `analysis/tile_catalog.json`, `gfx/catalog/*.png` | Decode + classify all 169 tables (palette/chr/tilemap/text/data); render CHR banks + contact sheet |
| `render_genesis_chr.py` | `gfx/catalog/chr/*.png` | Render the 24 CHR banks against all 8 detected palettes |
| `genesis_lz_bitmap_probe.py` | `gfx/catalog/probe/*_widths.png` | Probe a blob as a linear 4bpp bitmap at candidate widths |
| `genesis_lz_sprite_probe.py` | `gfx/catalog/probe/*_sprite.png` | Probe a blob as a column-major VDP sprite at candidate widths |
| `assemble_genesis_screen.py` | `gfx/catalog/screens/*.png` | Assemble a tilemap + CHR bank + palette into a screen (UI maps; not 3D walls) |
| `rip_genesis_walls.py` | `gfx/catalog/env/strip_*_L{0,3}.png` | Rip env strips with ASM CRAM `0x6FBEA` |
| `atlas_genesis_walls.py` | `gfx/catalog/env/atlases/*.png` | Grouped dungeon/outdoor wall atlases |
| `rip_genesis_monsters.py` | `gfx/catalog/monsters/`, `atlases/atlas_monsters.png` | `$11A` catalog CHR+layout → composed portraits |
| `genesis_vram_dump_stub.py` | stdout | Fusion / raw VRAM compare vs boot LZ blobs |
| `disasm_genesis_m68k.py` | `.asm` | Capstone 68000 listings |
| `scan_console_gfx.py` | entropy / VDP refs | Region survey |
| `preview_genesis_tiles.py` | `.png` | **Unvalidated** — do not use for location claims |

---

## Next steps

1. ~~**Finish LZ Python port**~~ — **done**: `0x29954` bit-exact, round-trips, 4/4 boot tables validated (`analysis/lz_validation.json`).
2. ~~**Catalog `0x30000+` tables**~~ — **done**: 169 tables catalogued (`analysis/lz_scan_30000.json`).
3. ~~**Catalog / classify all tables**~~ — **done**: 2 palette / 24 chr / 48 tilemap / 90 text / 5 data (`analysis/tile_catalog.json`). Text pool decoded and confirmed.
4. ~~**Find first-person walls**~~ — **done**: pre-rendered 4bpp strips in `0x717B6`–`0x7FC4E` via `$66(A5)` (`rip_genesis_walls.py`).
5. **Map strip → view slot**: recover which `A3` field / `$50`-stride record selects which strip for each depth/side (table fill @ `0xFC40` has multiple themes).
6. **Compose a full first-person frame** from the strip set (center door + L/R walls at depths) once slot mapping is known.
7. **Decode combat monsters** — **partial**: `$11A` catalog @ `0x33D4A` (69 ids); CHR+palette+linear layout ripped. Remaining: combat SAT XY (meta.`w2` packing was wrong); trace combat `+$6F` → `$11A`.
8. **Trace remaining `$0C` / `$2DBA` gameplay uploads** (outdoor / town CHR banks in the `0x37C00` pool).

---

## Regenerate artifacts

```powershell
python tools\analyze_genesis_gfx.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" -o EXTRACTED\genesis\analysis\gfx_static.json
python tools\disasm_genesis_m68k.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" --offset 0x200 --length 0x120 -o EXTRACTED\genesis\asm\boot_0200.asm
python tools\disasm_genesis_m68k.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" --offset 0x29954 --length 0x300 -o EXTRACTED\genesis\asm\resource_29954.asm
```
