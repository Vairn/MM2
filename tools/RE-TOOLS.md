# Reverse-engineering MM2 on Windows

## Why IRA and Ghidra are painful here

### IRA

- **IRA is an Amiga program.** It is meant to run on real Amiga hardware or in **WinUAE / FS-UAE**, not as a native Windows .exe.
- The listing **`EXTRACTED/mm2.asm`** in this repo was already produced by IRA V2.11. You can search and read it in any editor without running IRA yourself.
- To run IRA yourself: copy `mm2` onto an Amiga hardfile, run IRA from Amiga, point it at the executable, export `.asm`. Expect setup friction (Kickstart, tools, paths).

### Ghidra

- Ghidra has a **68000 CPU** module, but **no built-in Amiga hunk loader**.
- Importing `mm2` directly usually fails or shows garbage, because the file is a **hunk container** (headers, relocations, multiple segments), not a flat ROM image.
- Fixes that work on Windows:
  1. **Import flat segments** extracted by this repo (recommended below).
  2. Install a third-party **Amiga hunk loader** extension (search GitHub for `ghidra amiga hunk loader` — names change; pick one that mentions LoadSeg/hunk).

---

## What works out of the box (Python)

From the `MM2` folder:

```powershell
pip install -r tools\requirements.txt

# Full linear disassembly (~50k insns) — Capstone + hunk parser
python tools\disasm_m68k.py mm2

# Segment binaries + addresses for Ghidra
python tools\extract_segments.py mm2
```

Outputs:

| Tool | Output | Use |
|------|--------|-----|
| `disasm_m68k.py` | `EXTRACTED/mm2.capstone.asm` | Quick grep, opcode search |
| `extract_segments.py` | `EXTRACTED/ghidra/mm2_code_*.bin`, `mm2_segments.json` | Ghidra / IDA raw import |
| (existing) | `EXTRACTED/mm2.asm` | Best static listing (IRA labels, `DC` for data) |
| `harvest_symbols.py` | `EXTRACTED/mm2_symbols.yaml` | Harvest names/addresses from `EXTRACTED/docs/` |
| `apply_symbols.py` | `EXTRACTED/mm2.annotated.asm`, `mm2.capstone.annotated.asm` | Inject symbol comments into disassembly |
| `extract_asm_parts.py` | `EXTRACTED/asm/*.asm` | Split listing into commented subsystem files |
| `scan_a4_jsr.py` | stdout | List `JSR d(A4)` thunks with names from yaml |

### PC DOS port (8086 — **not** 68k)

GOG **`MM2.EXE`**, **`CGA.DRV`**, **`EGA.DRV`**:

```powershell
python tools\disasm_pc_x86.py "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2\MM2.EXE" `
  "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2\CGA.DRV" `
  "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2\EGA.DRV"
```

| Tool | Output | Use |
|------|--------|-----|
| `disasm_pc_x86.py` | `EXTRACTED/pc/mm2.capstone.asm`, `cga.capstone.asm`, `ega.capstone.asm` | PC x86 RE (Capstone `CS_MODE_16`) |
| `decode_pc_gfx.py` | `EXTRACTED/pc_gfx/` | Extract `.4` / `.16` — wall sheets + monster atlas |
| `export_monster_variants.py` | `EXTRACTED/monster_variants/` | Per-`monsters.dat` Amiga/CGA/EGA combat GIFs |
| `gen_gfx_compare_html.py` | `EXTRACTED/pc_gfx/compare/` | Numbered slot index: monsters 01–74 + wall sheets |
| `pc_gfx_export.py` | `wiki/public/gallery/pc/` | Wiki gallery for PC CGA/EGA assets |
| `mm2_lzw.py` | (library) | Canonical `lzw_decompress` (MM2.EXE `0x2A42`), shared by `decode_pc_gfx.py` + `pc_dat_lzw.py` |
| `pc_dat_lzw.py` | `EXTRACTED/pc_dat/` (`--auto`) | Decompress GOG `.DAT` files (`ATTRIB`/`MONSTERS`/`STR`/`MAP`/`EVENTSI`+`EVENTSO`/`ITEMS`) to Amiga-plain bytes |

See **`EXTRACTED/docs/54-pc-dos-graphics-formats.md`** and **`EXTRACTED/pc/README.md`** for
format specs (`0x2A42` LZW, `MONSTERS.*` u32 header, `0x6824` loader, prelude compositing, …);
see **`EXTRACTED/docs/07-dat-files-and-formats.md`** for the `.DAT`-file compression writeup.

```powershell
python tools\decode_pc_gfx.py "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2" --batch
python tools\export_monster_variants.py
python tools\gen_gfx_compare_html.py
python wiki\scripts\export-platform-compare.py   # wiki: monsters + walls iframe page
python wiki\scripts\export-monster-variants.py   # wiki gallery page
python tools\pc_gfx_export.py
python tools\pc_dat_lzw.py --auto "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2" -o EXTRACTED\pc_dat
```

### Apple II port (6502 — WOZ2 disks)

Source images: `EXTRACTED/apple/` (woz-a-day collection). RE workspace: `EXTRACTED/apple2/`.

```powershell
pip install git+https://github.com/BrentRector/orchard.git   # nibbler (WOZ decode)

python tools\extract_apple2_woz.py EXTRACTED\apple -o EXTRACTED\apple2
python tools\disasm_6502.py EXTRACTED\apple2\asm\disk_a_t00_s00.bin --org 0x800
```

| Tool | Output | Use |
|------|--------|-----|
| `extract_apple2_woz.py` | `EXTRACTED/apple2/disks/*.dsk`, manifests | WOZ2 → 140K sector dumps |
| `disasm_6502.py` | stdout or `.asm` | Boot / loader 6502 listings |

See **`EXTRACTED/docs/apple2-graphics.md`** and **`EXTRACTED/apple2/README.md`**. Disks use
copy-protected custom loaders (non-standard DOS 3.3 catalog); gfx blobs not extractable until
loader is traced. Target display: **double hi-res (560×192)**.

### Commodore 64 port (6502 — D64 disks)

Source images: `EXTRACTED/c64/` (`MM2-1.D64` … `MM2-6.D64`). RE workspace: `EXTRACTED/c64/`.

```powershell
python tools\analyze_c64_disk.py EXTRACTED\c64 --out EXTRACTED\c64\analysis
python tools\extract_c64_d64.py EXTRACTED\c64\MM2-1.D64 -o EXTRACTED\c64\extracted\mm2-1
python tools\disasm_6502.py EXTRACTED\c64\asm\mm2_1_t18_s01.bin --org 0x800
```

| Tool | Output | Use |
|------|--------|-----|
| `analyze_c64_disk.py` | `EXTRACTED/c64/analysis/disk_analysis.json` | BAM, string/PRG scans |
| `extract_c64_d64.py` | per-disk `extracted/` tree | Standard D64 catalog extract (blocked on side A by protection) |
| `disasm_6502.py` | stdout or `.asm` | Boot / IEC loader 6502 listings |
| `c64_vice_run_loader.py` | `EXTRACTED/c64/emulator/ram/` | VICE: load T19 chain @ `$081D`, RAM dump |
| `c64_load_prepared_ram.py` | `emulator/logs/prepared_run.json` | VICE: inject prepared RAM + T18 IEC @ `$0400` |
| `c64_vice_full_boot.py` | `emulator/logs/full_boot_*.json` | Full boot: `--prepared`, `--t18`, `--stage`; breaks `$FFD5`/`$DD00` |
| `c64_analyze_ram_dump.py` | stdout / JSON | Summarize `$4546`/`$0100`/`$02A7` from VICE RAM dumps |
| `c64_vice_iec_trace.py` | `emulator/logs/iec_trace.json` | Staged IEC trace; entry `$03B7`, `--skip-decomp` → `$03D4` |
| `c64_prepare_memory.py` | `emulator/ram/mm2-1_prepared_0800.bin` | Offline init + 247-pass decomp (no VICE) |
| `c64_decomp.py` | (library) | Boot decompression sliding-window codec |
| `c64_scan_tracks.py` | `analysis/track_gfx_scan.json` | Entropy/LZW/hires heuristics on `gfx/tracks/` |
| `c64_vice_boot.ps1` / `c64_vice_dump.ps1` | GUI / warp dumps | VICE launch helpers |

See **`EXTRACTED/docs/c64-graphics.md`**, **`EXTRACTED/c64/README.md`**, **`EXTRACTED/c64/emulator/README.md`**.

### SNES port (65816 — LoROM cartridge)

Source image: `EXTRACTED/SNES/Might and Magic II (Europe).sfc`. RE workspace: `EXTRACTED/snes/`.

```powershell
python tools\analyze_console_rom.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc"
python tools\trace_console_gfx.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\gfx_trace.json
python tools\trace_snes_gfx_paths.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\gfx_paths.json
python tools\extract_snes_dma.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\dma_table.json
python tools\disasm_snes_65816.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" --offset 0x6289 --bytes 0x160   # flag-tracked 65816
python tools\snes_decompress.py selftest EXTRACTED\SNES\"Might and Magic II (Europe).sfc"
python tools\snes_decompress.py render EXTRACTED\SNES\"Might and Magic II (Europe).sfc" --chr-offset 0x33A5C --size 0x7000 --palette-offset 0x31FBC --sub 2 --cols 16 --scale 3 -o EXTRACTED\snes\gfx\title_credits_chr_06BA5C.png
python tools\trace_snes_monsters.py EXTRACTED\SNES\"Might and Magic II (Europe).sfc" -o EXTRACTED\snes\analysis\monsters.json --render-dir EXTRACTED\snes\gfx\monsters
python tools\snes_vram_dump_stub.py --print-paths
powershell tools\snes_zsnes_dump.ps1 -Pal -Seconds 90
python tools\snes_zst_extract.py EXTRACTED\snes\emulator\mm2.zst -o EXTRACTED\snes\emulator\dumps --compare-staging
```

| Tool | Output | Use |
|------|--------|-----|
| `analyze_console_rom.py` | stdout / JSON | LoROM header, MM2 strings, PPU register hit counts |
| `scan_console_gfx.py` | `analysis/gfx_scan.json` | Per-bank entropy (tile-likeness heuristic removed) |
| `trace_console_gfx.py` | `gfx_trace.json` | PPU I/O map, VRAM upload sites |
| `trace_snes_gfx_paths.py` | `gfx_paths.json` | Palette + loader routine map (E289 is a palette copy, not a codec) |
| `extract_snes_dma.py` | `dma_table.json` | DMA channel 0 with WRAM mirror resolution |
| `disasm_snes_65816.py` | stdout / `.asm` | **Full 256-opcode 65816 disasm with M/X width tracking** (REP/SEP aware; `--m/--x`) |
| `snes_decompress.py` | `.png` / stdout | **Raw-CHR + BGR555 palette renderer**; re-implements `$00E289` copy / `$00E354` fade (`selftest` verifies) |
| `snes_vram_dump_stub.py` | stdout / JSON | VRAM or ZSNES `.zst` compare + WRAM staging |
| `snes_zst_extract.py` | `.vram.bin`, etc. | Extract VRAM/CGRAM/WRAM from ZSNES save state |
| `snes_zsnes_dump.ps1` | — | Launch ZSNES for manual F2 save (Win11 automation gap) |
| `trace_snes_monsters.py` | `monsters.json`, `gfx/monsters/` | Monster metadata `$14:8060` + CHR/palette PNG export |
| `preview_snes_tiles.py` | `.png` | Quick 4bpp preview (superseded by `snes_decompress.py render`) |

See **`EXTRACTED/docs/snes-graphics.md`**. **Finding (2026-07-07): MM2 SNES has no gfx decompressor** — `$00E289` is a palette word copy; **CHR is RAW + DMA**. Validated: title/credits `$06:BA5C`, field walls, font, UI, **78 monster/portrait sheets** via `$14:8060` table (`EXTRACTED/snes/gfx/monsters/`).

### Genesis / Mega Drive port (68000 — flat ROM)

Source image: `EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen` (**MM2**). RE workspace: `EXTRACTED/genesis/`.

```powershell
python tools\analyze_genesis_gfx.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" -o EXTRACTED\genesis\analysis\gfx_static.json
python tools\scan_console_gfx.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" -o EXTRACTED\genesis\analysis\gfx_scan.json
python tools\trace_console_gfx.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" -o EXTRACTED\genesis\analysis\gfx_trace.json
python tools\disasm_genesis_m68k.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" --offset 0x706 --length 0x300 -o EXTRACTED\genesis\asm\vdp_init_0706.asm
python tools\genesis_lz_decompress.py --self-test
python tools\genesis_lz_decompress.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" --offset 0xC52 -o out.bin
python tools\render_genesis_lz_tiles.py
python tools\scan_genesis_lz_tables.py EXTRACTED\Genesis\"Might and Magic - Gates to Another World (USA, Europe).gen" --start 0x30000 --end 0xC0000 -o EXTRACTED\genesis\analysis\lz_scan_30000.json
python tools\genesis_vram_dump_stub.py --print-paths
powershell tools\blastem_launch.ps1
python tools\genesis_vram_dump_stub.py --manual
```

| Tool | Output | Use |
|------|--------|-----|
| `analyze_genesis_gfx.py` | `gfx_static.json` | A5 jump vectors, resource table headers, `$2DBA` call sites |
| `genesis_lz_decompress.py` | `.bin` | Okumura LZSS decode/encode; `--self-test` round-trip; **byte-exact vs `0x29954`** (match len `(b1&0xF)+3`) |
| `render_genesis_lz_tiles.py` | `genesis/gfx/*.png` | Render decoded blobs as 4bpp tiles + CRAM (9-bit BGR) palette |
| `scan_genesis_lz_tables.py` | `analysis/lz_scan_*.json` | Scan ROM for `+0`(comp)/`+4`(out) LZ headers (consumed==`+0` + entropy drop) — 169 tables found `0x37C00`–`0x94F40` |
| `genesis_vram_dump_stub.py` | stdout | Fusion / VRAM dump compare helper |
| `analyze_console_rom.py` | stdout / JSON | Genesis header, MM2 strings, entropy |
| `scan_console_gfx.py` | `analysis/gfx_scan.json` | VDP port refs, per-64K entropy |
| `trace_console_gfx.py` | `gfx_trace.json` | VDP setup patterns |
| `disasm_genesis_m68k.py` | `.asm` | Capstone 68000 (boot, `$706` VDP init, `$2DBA` upload) |
| `preview_genesis_tiles.py` | `.png` | Trial Genesis 4 bpp tile preview (**unvalidated**) |

See **`EXTRACTED/docs/genesis-graphics.md`**. Native path: **`A5 = 0x312`** jump table → `$0C(A5)` LZ load (`0x29954`) → `$06(A5)` VDP DMA (`0x29F7E`) or `$2DBA` CPU upload.

### Data codec helpers (`items.dat`, `str.dat`)

Python:

```powershell
python tools\mm2_codec.py items-decode items.dat items.csv
python tools\mm2_codec.py items-encode items.csv items.dat
python tools\mm2_codec.py str-decode str.dat str.txt
python tools\mm2_codec.py str-encode str.txt str.dat
```

C (compile + run):

```powershell
gcc tools\mm2_codec.c -O2 -o tools\mm2_codec.exe
tools\mm2_codec.exe items-decode items.dat items.csv
tools\mm2_codec.exe items-encode items.csv items.dat
tools\mm2_codec.exe str-decode str.dat str.txt
tools\mm2_codec.exe str-encode str.txt str.dat
```

`play` is only an AmigaDOS **script** (assigns fonts/libs, runs `mm2`). Disassemble **`mm2`**, not `play`.

---

## Ghidra: import extracted code (no plugin)

1. Run `python tools\extract_segments.py mm2`.
2. **File → Import File** → `EXTRACTED/ghidra/mm2_code_00.bin`.
3. Format: **Raw binary**.
4. Language: **Motorola 68000:BE:32:default** (big-endian 68000).
5. Base address: **0** (see `mm2_segments.json` → first code segment `load_addr`).
6. After import: **Window → Memory Map** — confirm base `00000000`.
7. **Navigate → Go To** → `00000018` (entry after stub; see manifest `entry_points`).
8. **Symbol → Create Label** at that address (e.g. `start`).
9. Import `mm2_code_01.bin` at base **`0x27adc`** (see manifest — data/BSS sit between the two code hunks in the real load map).

   **Alternative:** if you only care about code and want addresses to match `mm2.capstone.asm`, place the overlay at **`0x2593c`** (immediately after the first code hunk, skipping data).

Limitations: relocations are **not** applied. Jump targets and A4-based Amiga OS calls may look wrong until you fix them manually or use a hunk loader plugin.

---

## MM2 layout (for navigation)

| Item | Offset (seg 0) | Notes |
|------|----------------|-------|
| Hunk header | file `0x0` | Not in extracted `.bin` |
| Code start | file `0x20` / load `0x0` | First byte of `mm2_code_00.bin` |
| `jmp` main | load `0x18` | `4ef9 0002 48ae` → game init ~`0x248ae` |
| Data | load `0x2593c` | `mm2_data_00.bin` |
| Overlay code | load `0x27adc` | `mm2_code_01.bin` (or `0x2593c` in code-only views) |

Main game init is around **`0x248ae`** in the first code segment (search `mm2.asm` for `LAB_248AE` or capstone listing).

---

## If you still want IRA-quality labels

Use **`EXTRACTED/mm2.asm`** as your primary reference. Re-run IRA only if you need a fresh export after patching the binary.

For interactive decompilation on Windows without Amiga: **Ghidra + extracted segments**, or consider **Binary Ninja** / **IDA** with the same flat binaries and manifest bases.
