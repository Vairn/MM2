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
