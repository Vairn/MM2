# Might & Magic II (Amiga)

Reverse-engineering workspace and faithful C++ remake of *Might & Magic II* for the Amiga.

The **68k disassembly is the source of truth.** Docs, codecs, and the remake are derived from ASM traces and real `.dat` / asset bytes — not from third-party recreations. See [`CLAUDE.md`](CLAUDE.md) for workflow rules.

## What’s here

| Path | Purpose |
|------|---------|
| [`game/`](game/) | Cross-platform remake (SDL2 desktop + Amiga/ACE) — [build & design](game/README.md) |
| [`editor/`](editor/) | Dear ImGui data editor (`MM2ED`) — [build & usage](editor/README.md) |
| [`EXTRACTED/`](EXTRACTED/) | Disassembly, analysis, decomp codecs, and docs |
| [`EXTRACTED/docs/`](EXTRACTED/docs/) | RE wiki / doc index — [start here](EXTRACTED/docs/README.md) |
| [`tools/`](tools/) | Disasm, codecs, symbol harvest, event DSL — [RE-TOOLS.md](tools/RE-TOOLS.md) |
| [`wiki/`](wiki/) | GitHub Wiki export scripts |
| Root `*.dat` | Game data (`map.dat`, `event.dat`, `roster.dat`, …) used by remake and editor |

Published wiki: [github.com/Vairn/MM2/wiki](https://github.com/Vairn/MM2/wiki)

## Quick start — remake (desktop)

Requires CMake ≥ 3.16, C++17, Ninja (optional), network on first configure (SDL2 via FetchContent).

```bash
cd game
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mm2 ../          # data dir = repo root (map.dat, town.32, …)
```

Amiga (ACE) and UI options: [`game/README.md`](game/README.md).

## Quick start — data editor

```bash
cmake -S editor -B editor/build -G Ninja
cmake --build editor/build
./editor/build/MM2ED .   # open this folder as the data root
```

## Documentation map

| Goal | Start here |
|------|------------|
| Project overview | [`EXTRACTED/docs/00-overview.md`](EXTRACTED/docs/00-overview.md) |
| All `.dat` layouts | [`EXTRACTED/docs/07-dat-files-and-formats.md`](EXTRACTED/docs/07-dat-files-and-formats.md) |
| Full doc index | [`EXTRACTED/docs/README.md`](EXTRACTED/docs/README.md) |
| Agent / RE workflow | [`CLAUDE.md`](CLAUDE.md) |
| Open unknowns | [`EXTRACTED/docs/05-open-questions.md`](EXTRACTED/docs/05-open-questions.md) |

**Endianness:** `.dat` multibyte fields are **little-endian on disk** unless a specific doc notes an exception. Runtime may byte-swap after load — confirm in ASM.

## Primary ASM / symbols

- Capstone listing: `EXTRACTED/mm2.capstone.asm` (annotated: `mm2.capstone.annotated.asm`)
- IRA listing: `EXTRACTED/mm2.asm`
- Symbols: `EXTRACTED/mm2_symbols.yaml` → `EXTRACTED/decomp/mm2_gamestate.h`
- Narrative analysis: `EXTRACTED/mm2-ANALYSIS.md`

Regenerate annotated ASM / symbols (from repo root):

```powershell
python tools\harvest_symbols.py --merge
python tools\apply_symbols.py
python tools\extract_asm_parts.py
python tools\scan_a4_jsr.py
```
