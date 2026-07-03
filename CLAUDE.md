# MM2 Workspace Notes

## Always base on ASM code first

**The 68k disassembly is the source of truth.** Start every decode, trace, or port
change in ASM — not docs, not FAQ writeups, not third-party editors.

- Primary: `EXTRACTED/mm2.capstone.asm`, `EXTRACTED/mm2.asm`, `EXTRACTED/asm/`
- Annotated: `EXTRACTED/mm2.capstone.annotated.asm` (regenerate via symbol tools below)
- Symbols: `EXTRACTED/mm2_symbols.yaml` → `EXTRACTED/decomp/mm2_gamestate.h`

**Docs can be wrong.** Treat `EXTRACTED/docs/` as working notes confirmed (or not) against
ASM. When ASM and anything else disagree, **ASM wins**.

**Do not use the Blitz3D recreation** (`Amiga/Smite-and-Magic/Blitz3d`) for RE — it is
not MM2's original engine.

### Required workflow

1. Trace reads/writes in ASM first.
2. Cross-check against real `.dat` bytes and round-trip codecs.
3. Capture confirmed layout in `EXTRACTED/docs/` + `tools/` (+ C struct/codec when decoding).
4. If ASM is unclear, document the gap and stub — **do not guess**.

---

## Where everything else lives

**Full doc index:** [`EXTRACTED/docs/README.md`](EXTRACTED/docs/README.md)

| Area | Path |
|------|------|
| Game data (root) | `event.dat`, `map.dat`, `roster.dat`, `str.dat`, `items.dat`, `monsters.dat`, `spells.dat`, `attrib.dat` |
| RE assets | `EXTRACTED/mm2.bin`, `EXTRACTED/mm2-ANALYSIS.md`, `EXTRACTED/decomp/` |
| Tools | `tools/RE-TOOLS.md` |
| Data editor | `editor/` — build: `cmake -S editor -B editor/build -G Ninja && cmake --build editor/build` |
| C++ remake | `game/` — **100% ASM fidelity**; see `game/README.md` |

**Endianness:** `.dat` multibyte fields are **little-endian on disk** unless a specific
doc notes an exception (e.g. `event.dat` location headers). Runtime may byte-swap after
load — trace in ASM.

**Symbol regeneration:**

```powershell
python tools\harvest_symbols.py --merge
python tools\apply_symbols.py
python tools\extract_asm_parts.py
python tools\scan_a4_jsr.py
```
