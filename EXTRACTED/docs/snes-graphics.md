# SNES graphics (Might and Magic II)

Initial RE notes for the **1993 SNES** port (European release). **Confirmed facts only**; gaps marked explicitly.

| Artifact | Path |
|----------|------|
| ROM image | `EXTRACTED/SNES/Might and Magic II (Europe).sfc` |
| Gfx scan | `EXTRACTED/snes/analysis/gfx_scan.json` |
| Gfx trace | `EXTRACTED/snes/analysis/gfx_trace.json` |
| Gfx paths | `EXTRACTED/snes/analysis/gfx_paths.json` |
| DMA table | `EXTRACTED/snes/analysis/dma_table.json` |
| Game-logic cross-ref (not gfx) | `EXTRACTED/docs/06-gfx-loading.md`, `54-pc-dos-graphics-formats.md` — locations/monsters only |

---

## RE approach

**Trace the SNES PPU path, not the Amiga/PC asset containers.** This port was built for cartridge ROM + the SNES graphics subsystem. Expect:

- **CHR in ROM** → decompress to WRAM → upload to **VRAM** (`$2118` / DMA via `$4300`–`$437F`)
- **Palettes** → WRAM staging → **CGRAM** (`$2121` / `$2122` or DMA BBAD `$22`)
- **Tilemaps** in VRAM, scrolled/composited per **BG mode** (Mode 1 or 7 for first-person — TBD)
- **Port-specific packing** in ROM banks — not `.32` planar sheets or PC LZW `.4` blobs

Amiga/PC docs are useful for *what* gets drawn (environment list, monster count, map layout) but **not** for on-disk gfx layout or decode algorithms.

---

## ROM provenance (confirmed 2026-07)

| Check | Result |
|-------|--------|
| File | `Might and Magic II (Europe).sfc` — **1,048,576 bytes** (1 MiB LoROM) |
| MD5 | `b05bcb14251f61abf83d666a36bd4a0f` |
| Header title | `MIGHT AND MAGIC II` @ LoROM offset `0x7FC0` |
| Map mode | `0x20` — **LoROM**, FastROM-capable |
| ROM type | `0x02` — ROM + SRAM |
| SRAM size code | `0x03` — 8 KiB |
| License / region | `0x6E` (110) — Europe |
| Checksum | `0xB0F5` / complement `0x4F4A` |
| Game text | Full MM2 location set present (Middlegate, Castle Xabran, Luxus Palace, Elemental Planes, …) |
| Credits @ `0x65D34` | `Licensed by Nintendo.` · `Distributed in Europe by Elite Systems.` |
| Copyright | `TM and Original Copyright (C) 1988 New World Computing, Inc.` |

**Assessment:** Retail European SNES cartridge image. No crack / trainer strings found.

---

## Display hardware (confirmed)

Native SNES pipeline — all gfx ends up in PPU memory:

| Item | Value |
|------|-------|
| PPU registers | `$2100`–`$213F` (B-bus) |
| VRAM | 64 KiB — tiles, tilemaps, optional Mode 7 matrix |
| CGRAM | 512 bytes — 256 colours × 15-bit BGR |
| OAM | 544 bytes — sprites (combat UI, cursor, etc.) |
| Tile format | 4 bpp 8×8 = **32 bytes/tile** (bitplane interleave per SNES spec) |
| Upload sites | `STA $2118` @ `0x613E`, `0x31EFD`, `0x31F1A`, `0x31F6C`; DMA templates below |
| BG config | `STA $2107`/`$2108` — exploration init sets BG1SC=`$7C`, BG2SC=`$78` |

**Not applicable:** Amiga `.32` TV/planar sheets, PC `.4`/`.16` LZW, blitter-based Amiga 3D pipeline.

---

## Gfx pipeline (confirmed code paths)

### 0. CORRECTION (2026-07-07): there is NO graphics decompressor

Prior notes claimed `$00E289` was a "ROM packer/decompressor". **Static 65816
disassembly disproves this.** `$00E289` is a plain **16-bit word copy** that
stages a **BGR555 palette** into WRAM `$7E:2000`; there are **no control bytes,
back-references, or run codes**. MM2 SNES stores **CHR (4bpp tiles) and tilemaps
RAW in ROM** and DMAs them directly to VRAM. See
`EXTRACTED/snes/analysis/decompress_validation.json`.

| Routine | File | Actual role (from ASM) |
|---------|------|------------------------|
| `$00E289` | `0x6289` | **Palette word copy**: `REP #$30; loop LDA [$E8],Y / STA $7E2000,X` for `Y` words. No compression. |
| `$00E354` | `0x6354` | **Palette fade step**: nudges each BGR555 channel of `$7E:2000` ±1/±$20/±$400 toward a ROM target color. |
| `$00E2EE`/`$00E32F` | `0x62EE`/`0x632F` | vblank-synced fade loops (call `$E354` 31×). |
| `$00E2B4` | `0x62B4` | DMA `$7E:2000` (512 B) → CGRAM (BBAD `$22`). |
| `$00E4F1` | `0x64F1` | **Tilemap builder** (header w/h + tile-base/attr offsets), not a CHR codec. |

### 1. Palette load (`JSL $00E648` → `JSR $00E289`)

Call sites set a **24-bit ROM pointer** in direct-page **`$E8`–`$EA`**, then **`Y`** = **palette word count**. The ROM bytes at these pointers are **BGR555 palette tables**, *not* compressed graphics.

| File offset | ROM source ptr | `Y` | Actual content |
|-------------|----------------|-----|----------------|
| `0x2805C` | `$05:D3DD` (file `0x2D3DD`) | `$40` | Title palette (64 colors, dark blue) |
| `0x28070` | `$05:DE13` (file `0x2DE13`) | `$10` | Palette chunk (16 colors) |
| `0x31D2A` | `$06:9F7C` (file `0x31F7C`) | `$20` | Field palette (BGR555 gradient) |
| `0x31DFE` | `$06:9FBC` (file `0x31FBC`) | `$40` | Title/credits scene palette (64 colors) |

`$00F851` is a small single-tile buffer write (hardware-multiply lookup into `$7E:7000`), not part of any codec.

Full call-site list: `EXTRACTED/snes/analysis/gfx_paths.json` → `jSL_E648_sites`.

### 2. WRAM → VRAM DMA template (`$00E650`, file `0x6650`)

After decompress, a shared routine issues **multiple 3200-byte DMA transfers** (BBAD `$18` → VRAM `$2118`):

| WRAM staging | VRAM dest | Size |
|--------------|-----------|------|
| `$7E:7E00` | `$2D40` | 3200 |
| `$7E:4C80` | `$3380` | 3200 |
| `$7E:5900` | `$3940` | 3200 |
| `$7E:4000` | `$1420` | 960 (when `$C4` set) |

SNES DMA registers use **`$40:7Exx` / `$4C:xx` mirror banks** with **`A1M=$7E`** — resolved to `$7E:xxxx` above. Same template duplicated at file **`0x6684`+**.

### 3. ROM → VRAM DMA (boot, confirmed)

Manual decode @ file **`0x616C`** (anchor `STZ $4300`):

| Reg | Value | Meaning |
|-----|-------|---------|
| `$4301` BBAD | `$18` | VRAM write |
| `$4302`–`$4303` A1T/A1B | `$0000` / `$03` | Source **`$03:0000`** → file **`0x18000`** |
| `$4305`/`$4306` DAS | `$0220` | **544 bytes** |
| `$420B` MDMAEN | `$01` | Channel 0 |

**544 bytes is OAM-sized**, not a tile sheet — likely sprite table upload at boot. **Not validated as CHR.**

Other ROM→VRAM clusters appear in `dma_table.json` — treat as **candidates** until emulator VRAM compare.

### 4. Exploration VRAM CPU upload (`$069ED8`, file `0x31ED8`)

Called via **`JSR`** from **`$069D15`** (file `0x31D15`) after a decompress pass:

- Saves **`X`→`$17BE`**, **`Y`→`$17C0`**
- **`LDA [$E8]` / `XBA` / `LDA [$EB]`** → **`STA $2118`** (16-bit VRAM writes)
- Second loop: 8-bit **`LDA [$E8]`** with zero high byte
- **`$E8`–`$EA` / `$EB`–`$ED`** = paired ROM or WRAM read pointers during upload

Related **`STA $2118`** loops @ file `0x31EFD`, `0x31F1A`, `0x31F6C` inside same routine cluster.

### 5. Exploration PPU mode set (file `0x31F2A`–`0x31F4C`)

After upload, **`STA $2105`** with **`$07`**, **`STA $212C`**=`$01`, clears BG scroll regs, **`STZ $2116`** (VRAM addr 0). **Correction:** the actual in-game
exploration/scene compositor at `$00E9DC`–`$00ED1E` sets **BGMODE = `$02`
(Mode 2)** at `$00EBBB`, not Mode 7. The `$069DDD` path builds the
**title/credits** screen (uses `BGMODE=$F1`), which is a different scene than the
first-person view.

### 6. Palette / CGRAM (partial)

| Path | File | Status |
|------|------|--------|
| **WRAM → CGRAM DMA** | `$00E2B4` (`0x62B4`) | **512 bytes** from WRAM **`$7E:7E00`** (BBAD `$22`, CGADD `$00`) — full palette RAM fill |
| **Inline CPU** | `0x280A0`, `0x28CB1` | **`CGADD=$41`**, two **`CGDATA`** bytes **`$4A` `$29`** — patches one colour; **not a ROM palette table** |
| **Inline CPU** | `0x2841C` | CGADD + CGDATA sequence (needs trace) |
| **ROM palette blob** | — | **Not located** — do not use code-adjacent bytes as palette |

**Gap:** What decompress populates **`$7E:7E00`** before CGRAM DMA. **`$0119`** (facing/area index) adjusts at **`$058E25`** / **`$069DA3`** but is **not yet tied** to a pointer table stride.

---

## ROM layout (partial, confirmed)

| Region (file offset) | Entropy | Notes |
|----------------------|---------|-------|
| `0x00000`–`0x07FFF` | ~6.7 | Code + boot; VRAM/OAM DMA @ `0x6130` / `0x616C` |
| `0x18000` | ~6.4 | **544 B ROM→VRAM DMA source** (OAM-sized) |
| `0x20000`–`0x27FFF` | ~7.0 | High entropy — **likely compressed**; decompress input, not direct CHR |
| `0x28000`–`0x2FFFF` | ~6.5 | Mix of code + compressed blobs (`$05:D3DD`, `$05:DE13`, …) |
| `0x31000`–`0x31FFF` | ~6.0 | Exploration loader + packed blobs @ `0x31F7C` / `0x31FBC` |
| `0x50000`–`0x57FFF` | ~4.8–5.9 | Game text / event strings — **not CHR** |
| `0x65000`–`0x65FFF` | ~5.5 | Nintendo / Elite / NWC credit block |

### CHR status — **MAPPED & TRIAGED (2026-07-07)**

CHR is stored **RAW** in ROM and DMA'd straight to VRAM (no codec). Full map:
`EXTRACTED/snes/analysis/chr_catalog.json`; grayscale sheets in
`EXTRACTED/snes/gfx/catalog/` (4bpp) and `catalog8/` (8bpp).

#### DMA-extractor bug fixed (important)

The prior `dma_table.json` mis-assigned the source **bank**: it read `$4303`
(A1T0H, the address-high byte) as the bank when the real bank is **`$4304`**
(A1B0). Corrected: **source = `$4304:$4303:$4302`**. This alone made most ROM
sources wrong (e.g. the confirmed title CHR was mislabelled `$BA:065C`
instead of `$06:BA5C`). `extract_snes_dma.py` now resolves it correctly.

#### Complete ROM→VRAM CHR map (channel-0 immediate DMAs)

| ROM src | file | size | bpp | VRAM | Content | Confirmed |
|---------|------|------|-----|------|---------|-----------|
| `$06:8000` | `0x30000` | 3072 | 4 | `$4000` | title logo lettering | ✔ |
| `$06:8CC4` | `0x30CC4` | 4096 | 4 | `$5000` | animated candle/torch + arrow glyphs (title decor) | ✔ |
| `$06:BA5C` | `0x33A5C` | 32768 | 4 | `$4000` | **title/credits** (readable text) + tilemap tail | ✔ |
| `$0D:8000` | `0x68000` | 4288 | 4 | `$5920` | **outdoor landscape backdrop** (sky/clouds + terrain) | ✔ |
| `$0D:90C0` | `0x690C0` | 8192 | 4 | `$0000` | **game FONT** (full ASCII: symbols, 0-9, A-Z, a-z, accents) | ✔ |
| `$0D:B0C0` | `0x6B0C0` | 3872 | 4 | `$5190` | "Might and Magic" title lettering | ✔ |
| `$0D:BFE0` | `0x6BFE0` | 4480 | 4 | `$4010`/`$48D0` | **FIRST-PERSON WALL set A** (stone/brick), scene `$A2`=0/1 | ✔ |
| `$0D:D160` | `0x6D160` | 4480 | 4 | `$4010`/`$48D0` | **WALL/ground set B** (cracked masonry), scene `$A2`=2/5 | ✔ |
| `$12:EA3C` | `0x96A3C` | 2208 | 4 | scene | **starfield / night-sky** backdrop | ✔ |
| `$12:F420` | `0x97420` | 2208 | 4 | `$6180` | small scene figures/objects (identity open) | — |
| `$13:D084` | `0x9D084` | 4096 | 4 | `$6000` | **UI / automap icons** (arrows, towers, buildings) | ✔ |
| `$13:E084` | `0x9E084` | 4096 | 4 | scene | UI frames / box tiles | — |
| `$15:FBE3` | `0xAFBE3` | 1024 | 4 | `$6600` | **UI arrows** (up/right/down/left) | ✔ |
| `$08:CB00` | `0x44B00` | 13566 | 8? | `$0000` | boot/intro full-screen art (format/palette open) | — |
| `$1C:8000` | `0xE0000` | 32768 | 8? | `$0000` | boot/intro full-screen art (format/palette open) | — |
| `$1F:A5D7` | `0xFA5D7` | 11010 | n/a | `$4000` | striped in 4bpp **and** 8bpp → **tilemap / packed**, not CHR | — |
| `$10:FF76` | `0x87F76` | 16 | 4 | `$6600` | 16-byte VRAM patch (not a sheet) | — |

**Wall/dungeon tiles: FOUND** — `$0D:BFE0` (stone/brick) and `$0D:D160`
(cracked masonry) are the first-person wall sets, selected by scene type `$A2`.

#### Scene compositor (exploration first-person view)

Routine `$00E9DC`–`$00ED1E` (bank `$00`) builds the exploration scene:

- **BGMODE = `$02`** (`STA $2105` @ `$00EBBB`) — **Mode 2** (two 4bpp BGs,
  offset-per-tile). **NOT Mode 7** (corrects the earlier "$07 Mode 7 trial" guess).
- Scene selector **`$A2`** (`$00EA5B`, values 0/1/2/5) chooses the wall CHR set.
- Layers: sky `$0D:8000`→`$5920`, walls `$0D:BFE0`/`$0D:D160`→`$4010`(+`$48D0`
  far LOD), overlays `$12:EA3C`/`$12:F420`, font `$0D:90C0`.
- Tilemaps built in WRAM by **`$00E4F1`** then DMA'd to VRAM via `$00E650`
  (the `$7E:4000/$4C80/$5900`→VRAM transfers are **tilemaps, not CHR**).
- Palette staged by **`JSR $00E289`** from **`$19:8060`** (file `0xC8060`,
  `X=$90 Y=$20`) → `$7E:2000` → CGRAM. That table holds grays + stone browns
  + sky blue + foliage green — the environment palette.

#### Palette pairing (honest gap)

`$06:BA5C` is pixel-confirmed with palette `$06:9FBC` sub 2. For the environment
sets, `$19:8060` gives plausible natural colors, but **per-tile sub-palette
selection (Mode-2 tilemap attributes) and exact CGRAM state cannot be recovered
statically** — grayscale renders are the coherence proof; color renders in
`catalog_color/` are best-effort. Emulator CGRAM/VRAM capture is still the way to
lock exact colors.

#### Monsters / combat portraits — **FOUND (2026-07-07)**

Combat sprites and party portraits are **not** in the static DMA table. They load
through the **monster metadata table** and WRAM staging path:

| Piece | Address | Role |
|-------|---------|------|
| Index table | `$14:8060` | 3 bytes/entry → metadata record (stride `index*3` @ `$00D80C`) |
| Loader | `$00D803` | monster id → metadata; palette via `$00E289`; portrait via `$00F23C` |
| CHR staging | `$00F62E` | animation script → WRAM `$7E:4000+`; DMA to VRAM `$00F8D0` |
| Party template | `$0C:FE3D` | 136-byte record copied to `$7F:BE00` / `$7F:E000` combat slots |

**Metadata record layout** (confirmed on index 10 / `$16:8000`):

| Offset | Field |
|--------|-------|
| `+0` | bank byte (shared by palette + CHR + anim pointers) |
| `+2` | flags (`bit7` ⇒ 8bpp per `$00D82F`) |
| `+4` | palette addr (16-bit LE, same bank) → BGR555, 16 words to CGRAM sub `$A0` |
| `+6` | CHR tile blob addr (16-bit LE, same bank) |
| `+8` | animation script addr (16-bit LE, same bank) |

**Scene figures (first-person distant sprites):** `$12:F420` — small humanoid
silhouettes composited into the exploration view (palette `$19:8060`).

**Boot OAM DMA:** NMI path `@ $008C05` uploads 544 B from `$03:0000` (OAM table).

Catalog: `EXTRACTED/snes/analysis/monsters.json` (128 entries, 78 rendered) and
`EXTRACTED/snes/gfx/monsters/*.png`. Examples: index 4 (green knight + orange
monster faces), index 10 (mechanical creature parts).

**Repro:**
python tools\extract_snes_dma.py "EXTRACTED\SNES\Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\dma_table.json
python tools\snes_dma_summary.py EXTRACTED\snes\analysis\dma_table.json
python tools\snes_chr_catalog.py "EXTRACTED\SNES\Might and Magic II (Europe).sfc" --table EXTRACTED\snes\analysis\dma_table.json --outdir EXTRACTED\snes\gfx\catalog --cols 16 --scale 2
python tools\snes_chr_catalog.py "EXTRACTED\SNES\Might and Magic II (Europe).sfc" --table EXTRACTED\snes\analysis\dma_table.json --outdir EXTRACTED\snes\gfx\catalog8 --cols 16 --scale 2 --bpp 8
python tools\trace_snes_monsters.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\monsters.json --render-dir EXTRACTED\snes\gfx\monsters
# confirmed title render:
python tools\snes_decompress.py render "EXTRACTED\SNES\Might and Magic II (Europe).sfc" `
  --chr-offset 0x33A5C --size 0x7000 --palette-offset 0x31FBC --sub 2 --cols 16 --scale 3 `
  -o EXTRACTED\snes\gfx\title_credits_chr_06BA5C.png
```

---

## Emulator validation (2026-07-07)

User-provided **ZSNES 1.36** @ `…/zsnes1-40/zsnesw.exe`.

| Check | Result |
|-------|--------|
| Executable | `zsnesw.exe` (512 KiB) in `zsnes1-40/`; duplicate `ZSNESW.EXE` in parent folder |
| MM2 load | **OK** — `rominfo.txt`: LoROM, PAL, checksum OK, CRC32 `4062244B` |
| CLI launch | `zsnesw.exe -m -a -f 2 -s -u "<rom>"` starts emulation without GUI |
| CLI VRAM dump | **None** — no `-dump` / memory-export flag |
| Debugger (`-d`) | **SPC/DSP RAM only** (key `D`); no VRAM/CGRAM export per `readmew.txt` §14 |
| Save state (F2) | Contains 64 KiB VRAM @ file offset **`0x20C13`** (V0.6 layout); parser in `tools/snes_zst_extract.py` |
| F2 automation | **Failed** — `keybd_event`, `PostMessage`, `SendKeys` did not create `mm2.zst` on Win11 |
| Mesen / bsnes | **Not on PATH** — direct VRAM export still recommended |

**ZST parser self-test (not MM2 CHR evidence):** `ChronoTrigger.zst` from the same ROM collection extracted cleanly to `EXTRACTED/snes/emulator/selftest/`; log in `zst_layout_selftest.txt`.

**Manual ZSNES workflow:**

```powershell
powershell tools\snes_zsnes_dump.ps1 -Seconds 90 -Pal
# Press F2 at title or field view while the window is focused
python tools\snes_zst_extract.py EXTRACTED\snes\emulator\mm2.zst -o EXTRACTED\snes\emulator\dumps --compare-staging
python tools\snes_vram_dump_stub.py --dump EXTRACTED\snes\emulator\mm2.zst --compare-staging --json EXTRACTED\snes\emulator\staging_compare.json
```

**Mesen workflow (preferred when installed):** load ROM → field view → Tools → Memory Tools → Video RAM → Save (64 KiB) → pass `--dump` to `snes_vram_dump_stub.py`.

Session record: `EXTRACTED/snes/emulator/validation.json`.

---

## Tools

```powershell
python tools\analyze_console_rom.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc"
python tools\scan_console_gfx.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\gfx_scan.json
python tools\trace_console_gfx.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\gfx_trace.json
python tools\trace_snes_gfx_paths.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\gfx_paths.json
python tools\extract_snes_dma.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\dma_table.json
python tools\disasm_snes_65816.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" --offset 0x31DDD --bytes 0x180
python tools\snes_vram_dump_stub.py   # VRAM / .zst compare helper
python tools\snes_zst_extract.py EXTRACTED\snes\emulator\mm2.zst -o EXTRACTED\snes\emulator\dumps --compare-staging
powershell tools\snes_zsnes_dump.ps1 -Pal -Seconds 90   # launch ZSNES; manual F2
python tools\preview_snes_tiles.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" --offset 0x50000 --tiles 128 -o EXTRACTED\snes\gfx\preview.png
```

| Tool | Output | Use |
|------|--------|-----|
| `analyze_console_rom.py` | stdout / JSON | Header, strings, PPU pattern counts |
| `scan_console_gfx.py` | `gfx_scan.json` | Per-bank **entropy only** (tile-likeness removed) |
| `trace_console_gfx.py` | `gfx_trace.json` | PPU I/O hit map + VRAM site windows |
| `trace_snes_gfx_paths.py` | `gfx_paths.json` | Decompress call sites + exploration/palette routines |
| `extract_snes_dma.py` | `dma_table.json` | DMA setups; **corrected** source = `$4304:$4303:$4302` (bank fix) |
| `snes_dma_summary.py` | stdout | Pretty ROM/CGRAM/WRAM→VRAM table from `dma_table.json` |
| `snes_chr_catalog.py` | `catalog/`, `catalog8/` + `catalog_index.json` | Renders every ROM CHR source 4bpp/8bpp (grayscale or BGR555 palette) |
| `disasm_snes_65816.py` | stdout / `.asm` | Minimal 65816 listing for gfx routines |
| `snes_vram_dump_stub.py` | stdout / JSON | VRAM or `.zst` compare + WRAM staging table |
| `snes_zst_extract.py` | `.vram.bin`, `.cgram.bin`, `.wram7e.bin` | ZSNES save-state memory extract |
| `snes_zsnes_dump.ps1` | launches ZSNES | Manual F2 save-state capture helper |
| `snes_decompress.py` | `.png` / stdout | **Validated** raw-CHR + BGR555 palette renderer; re-implements `$00E289` copy / `$00E354` fade (`selftest`) |
| `trace_snes_monsters.py` | `monsters.json`, `gfx/monsters/` | Monster metadata table `$14:8060` + CHR/palette render |

---

## Recommended next steps

1. **Map monster index → `monsters.dat` id** and stitch animation frames via
   `$00F62E` script semantics (current renders dump the raw CHR blob only).
2. **Resolve `$08:CB00` / `$1C:8000`** boot art: 8bpp format + palette.
3. **Decode `$1F:A5D7`** as a tilemap (via `$00E4F1` header semantics).
4. **Lock per-scene sub-palettes** via emulator CGRAM capture.

*(Done 2026-07-07: fixed DMA source-bank bug; mapped channel-0 CHR; exploration
compositor BG Mode 2; walls/font/backdrops/UI; **monster table `$14:8060` +
78 combat sprite sheets**; scene figures `$12:F420`.)*

---

## Regenerate artifacts

```powershell
python tools\scan_console_gfx.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\gfx_scan.json
python tools\extract_snes_dma.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\dma_table.json
python tools\trace_snes_gfx_paths.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\gfx_paths.json
```
