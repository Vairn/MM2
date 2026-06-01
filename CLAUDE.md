# MM2 Workspace Notes

## Purpose

Quick memory aid for where core reverse-engineering assets live in this workspace.

## Key Game Data Files (root)

- `event.dat`
- `map.dat`
- `roster.dat`
- `str.dat`
- `items.dat`
- `monsters.dat`
- `spells.dat`
- `attrib.dat`

## Extracted Working Area

- `EXTRACTED/mm2.asm` - IRA disassembly (primary asm source).
- `EXTRACTED/mm2.capstone.asm` - Capstone disassembly view.
- `EXTRACTED/mm2.bin` - raw binary used for analysis.
- `EXTRACTED/mm2-ANALYSIS.md` - consolidated analysis notes.
- `EXTRACTED/docs/` - focused docs by subsystem.
- `EXTRACTED/decomp/` - C lift scaffold (`mm2_lift.c`, `mm2_types.h`).

## Tools

- `tools/decode_event.py` - **event.dat decoder** (tile events, scripts, strings).
- `tools/disasm_m68k.py` - disassembly helper.
- `tools/extract_segments.py` - segment extraction helper.
- `tools/inspect_hunk.py` - binary hunk inspection helper.
- `tools/scan_a4_jsr.py` - scan JSR `d(A4)` offsets.
- `tools/RE-TOOLS.md` - tooling notes.

## Reminder

You have assembly code available and should use it directly when decoding formats or tracing game logic.

## Endianness (Amiga)

MM2 is an original **Amiga** game (Motorola 68000). For `.dat` file work, treat
multibyte numeric fields as **little-endian on disk** by default. This matches
the on-disk bytes and the Blitz3D reference editors (`ReadShort` / `ReadInt` on
PC). Do not assume big-endian just because the game ran on Amiga — that mistake
made `items.dat` gold look 256× too large (e.g. Small Club `01 00` → **1** gp,
not 256).

The runtime may still byte-swap after load (items gold init @ asm `0x26030`); editors
and codecs read/write the file bytes directly. If a specific format doc or ASM trace
shows an exception (e.g. `event.dat` location header offset/length), note it there —
otherwise default to LE and validate with round-trip on real files.

## Decoded Formats

- **event.dat**: fully decoded — see `EXTRACTED/docs/06-event-dat-format.md`
  - 71-entry header (4B offset + 2B length, big-endian)
  - Per-location: triplet table → string offset → script bytecodes → string table
  - Loader at asm `0x92F2`, init at `0x1754A`, interpreter at `0x172CA`
- **map.dat**: 60 screens × 512 bytes (2 × 256-byte tile pages), flat read into A4-$EEF4
 - **page 0 (visual)**: four **2-bit wall fields** per cell (N/E/S/W @ bits 0-1/2-3/4-5/6-7):
 `0` open, `1` wall, `2` wall+torch, `3` door — **no event bit**. Used by 3D hood @`0x2900`.
 - **page 1 (collision)**: same bit positions but `(dark<<1)|wall` per direction; bit `0x80`
 = **event flag** (collision only). Darkness drains light factor; see `15-3d-view` doc.
 Every `event.dat` triplet aligns with collision `0x80`, not a separate visual flag.
 - Auto-map cell→frame depends on view mode `-$79E2` (helper `0x2182`): **outdoor**
 (surface, `-$79E2!=0`) uses raw `byte & 0x1F` into `outb.32` (page-0 bytes are
 terrain ids, hi bits are flags); **interior** dungeons use `kCartoTile[byte>>2]`
 (data `0x9D0`) into `townb.32`. Planes 41-44 force frame 8/4/4/5. See
 `EXTRACTED/docs/15-3d-view-and-game-screen.md` §3.
- **monsters.dat**: 256 × 26 bytes — ability/attack decode ASM-confirmed, see
  `EXTRACTED/docs/16-monster-ability-format.md`
  - Unpacker at asm `0x4C8E`; group-attack verbs (Pabil `0x11` low5) dispatched
    at `0x10002` from verb table `A4-$6E56` (idx 29 = "frenzies", e.g. Cuisinart).
  - Single attack (Sabil `0x12` low5) = victim-status table; Oabil `0x13` =
    adds-friends count / flee tier / multiplies.
  - Verified against data hunk `ghidra/mm2_data_00.bin` (A4 = data_base + 0x7FFE).
- **combat**: round/turn engine traced — see `EXTRACTED/docs/17-combat-system.md`
  - Round loop at asm `0x12A22`; player turn `0x119C2` (command bar `0x11866`,
    keys A/F/S/C/U/B/R/E/V); monster AI `0x1064C`.
  - Battle slots: parallel arrays `A4-$11DE[]` (type), `-$53A[]` (HP),
    `-$519[]` (status), `-$50E[]` (speed); victory `0x12430`, rewards `0x10B74`.
- **spells.dat**: decoded — 96 spells × 2 bytes (Sorcerer 0..47, Cleric 48..95,
  same flat order as the items.dat use-effect index) + 64 trailing/unused bytes
  (preserved on round-trip). See `EXTRACTED/docs/19-spells-and-item-use.md`.
  - byte0: `0x40` combat-only, `0x80` non-combat-only (neither = anytime),
    `0x10` special/computed gem cost (Duplication, Star Burst, Enchant Item,
    Divine Intervention, Uncurse Item), low nibble = gem cost.
  - byte1: `0x80` outdoor-only, bits 6-4 = per-level SP multiplier (X in "X/L"),
    low nibble = flat SP cost (0 ⇒ per caster level).
  - Validated vs manual Appendix B: gems 96/96, cast 96/96, outdoor 12/12, SP
    95/96 (Meteor Shower base SP is per-monster, computed in code). Manual
    cost/type/object/description for all 96 transcribed into
    `editor/src/core/Spells.cpp` `kSpells[]` and `tools/mm2_spells.py`
    `SPELL_META`. Codec: `tools/decode_spells.py`, `core/SpellsFile.{h,cpp}`.


## External Reverse-Engineering Reference

- The Blitz3D project (`C:\_20260421_\D-REC\development\Amiga\Smite-and-Magic\Blitz3d`) is **NOT useful** for
 reverse engineering MM2. It was a separate recreation project using its own 3D engine and did not
 replicate MM2's original rendering logic. **Do not consult it.** Use only the 68k ASM disassembly.

## Decode Workflow (Required)

1. Start with `EXTRACTED/mm2.capstone.asm` and/or `EXTRACTED/mm2.asm` to locate real runtime reads/writes.
2. Cross-check field layout assumptions against Blitz3d reverse code (especially loaders/savers/editors).
3. Resolve conflicts in favor of ASM behavior.
4. Capture confirmed structure in local docs/tools so future passes reuse verified mappings.

## Confirmed `items.dat` Record Layout

From `b3dmm2/Item_ED.bb` and current MM2 data:

- 256 records
- Record size: 20 bytes (`0x14`)
- Per-record layout:
  - bytes `0x00-0x0B`: item name, 12-byte ASCII (space-padded)
  - byte `0x0C`: separator/padding (typically `0x00`)
 - byte `0x0D`: **forbidden-class** bitmask — a **set** bit means that class
 *cannot* use the item (so `0x00` = usable by everyone). Predicate: class can use
 ⇔ `(mask & bit) == 0`. Bit order `K P A C S R N B` = `0x80..0x01`. Validated vs
 `items.dat`: Katana/Nunchakas `0x7D` (only Knight+Ninja clear), Holy Cudgel
 `0xAF` (Paladin+Cleric), every blade sets the Cleric bit `0x10` (clerics can't
 use blades). The legacy `Item_ED.bb` label "Useable By" and its Robber/Cleric
 bit swap are both wrong. See `EXTRACTED/docs/18-items-dat-format.md`.
 - byte `0x0E`: bonus nibble pack (`hi=bonus_type`, `lo=bonus_amount`) — `bonus_type`
   is a **category table** (equipped "special power"), `lo`=magnitude (**amount 0
   = no bonus**): 0 Might, 1 Int, 2 Per, 3 Speed, 4 Acc, 5 Luck, 6-13 resistances
   (Magic/Fire/Elec/Cold/Energy/Sleep/MaxHP*/Acid), 14 Thievery, 15 AC. Anchored by
   named elemental shields (Magic/Fire/Electric/Cold/Acid → 6/7/8/9/13), "+N Mgt"
   gear → 0, "+N AC" gear → 15. Cross-validated vs RPGClassics/Fandom item tables.
   (*type 12: RPGClassics "PHP/MaxHP" vs Fandom "Poison".) See
   `EXTRACTED/docs/18-items-dat-format.md`.
 - byte `0x0F`: **use power** — NOT a nibble pair. The whole byte is a **flat
   spell index**: `0x00` none, `0x01-0x7F` stat boost (hi nibble kind {0 MaxHP,
   1 Might, 2 Speed, 3 Acc, 5 Level, 6 SpLvl} + lo amount), `0x81-0xB0` Sorcerer
   spell `#(byte-0x80)`, `0xB1-0xE0` Cleric spell `#(byte-0xB0)`. Flat index walks
   levels with spell counts **7,7,6,6,5,5,4,4,4** (48/school). Verified byte-exact
   vs RPGClassics "Use Ability" (49 items, 0 mismatch); every item casts its themed
   spell (Web Caster→Web, Hourglass→Time Distortion, Divine Mace→Divine
   Intervention). Dispatched via `combat_use_item_handler` (`0x133EC`). Decoder:
   `tools/mm2_spells.py`, `describeItemEffect` in `editor/src/core/ItemsFile.cpp`.
   Spell tables + encoding in `EXTRACTED/docs/19-spells-and-item-use.md`. Messages:
   "No special power" `0xe1e8`, "No charges" `0xe1f9`.
  - byte `0x10`: damage/value component (byte)
  - byte `0x11`: padding/unused in editor write path
  - bytes `0x12-0x13`: gold (`uint16` little-endian on disk; game byte-swaps on load at asm `0x26030`)


when you decode something please make a c struct for it, and a loader/saver/encoder/decoder in c and python

## GUI Editor (`editor/`)

ImGui + GLFW + OpenGL3 data editor for the `.dat` files. Structured like the
GRACE (`C:\Development\GRACE\editor`) and LorED
(`C:\_20260421_\D-REC\development\Amiga\LandsOfLore\LorED\src`) editors:
**each data type has its own self-contained section**, with a clean split
between the data layer and the UI layer.

### Build (Ninja preferred)

```bash
cd editor
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build      # -> editor/build/MM2ED.exe
```

Dependencies (Dear ImGui *docking* branch, GLFW 3.4, portable-file-dialogs)
are fetched via CMake `FetchContent` into `editor/build/_deps` on first
configure. Toolchain in this workspace: MSYS2 UCRT64 g++ 14.1, CMake 3.24,
Ninja 1.11. `CMAKE_CXX_STANDARD` is 17.

### Layout

- `src/main.cpp` — GLFW/OpenGL3 init + main loop.
- `src/app/` — `App` (dockspace, menu, section registry, Open/Save, cross-refs),
  `AppState` (shared state), `Section` (abstract base: `title/fileName/load/save/draw`).
- `src/core/` — data layer, **no ImGui**. One model per file with
  `decode/encode/load/save`: `ItemsFile`, `MonstersFile`, `RosterFile`,
  `MapFile`, `AttribFile`, `StrFile`, plus `RawFile` (spells/event) and
  `ByteIO` (endian + file helpers). Default `.dat` multibyte = **little-endian**
  (see **Endianness** above); per-format exceptions live in `EXTRACTED/docs/`.
- `src/sections/` — UI layer, one `Section` subclass per type.
- `src/widgets/HexView` — shared hex+ASCII dump used by most sections.

### Adding a section

1. Add `core/XxxFile.{h,cpp}` (decode/encode/load/save).
2. Add `sections/XxxSection.{h,cpp}` subclassing `Section`.
3. Register it in `App::registerSections()`.
4. Reconfigure — the CMake `GLOB_RECURSE` picks up new files.

### Section status

items (full), roster (stats/equipment/spells tabs), str (text), monsters
(name + named/raw fields + sprite preview: `picture & 0x7F` -> `NN.anm`,
0x80 bit is a placement/size flag), map (visual+collision 16x16 grids), attrib
(env + roof bits + hex). event.dat is a structured **imnodes node-graph
editor** (see below). spells (full: per-spell cast/SP/gem/outdoor editing from
`core/SpellsFile`, plus the manual reference cost/type/object/description from
`core/Spells.cpp`).

**Events (`event.dat`):** node-graph editor built on `imnodes`
(FetchContent, compiled into `libimnodes.a`; `ImNodes` context created in
`main.cpp`). `core/EventFile.{h,cpp}` decodes the 71-location container into
triggers/segments/ops/strings (ported from `tools/decode_event.py`, keeps the
original `raw` bytes + absolute offsets for in-place editing).
`core/EventOps.h` holds the `0x00..0x32` opcode table (argc + names +
`describeOp` pseudo-code). `sections/EventSection.{h,cpp}` renders, per
selected location, **trigger nodes -> event-script nodes -> string nodes**;
opcode argument bytes and trigger pos/cond flags are editable hex fields that
patch `raw` length-preservingly and persist on Save. Re-encoding that changes
record/segment sizes is intentionally not supported yet.

**Graphics:** `.32` tilesets and `.anm` animations are decoded and previewed
(`core/Gfx.{h,cpp}` + `sections/GfxSection` + `widgets/Texture`). Both use one
planar image-chunk codec: u16 `frame_count`, u16 `depth_or_mode`, per-frame
`{u16 w, u16 h, u16 flags}`, 32-word Amiga `0x0RGB` palette, then a nibble-RLE
stream of 5 concatenated bitplanes per frame (`rassize = h*(((w+15)>>3)&0xFFFE)`).
`.32` starts the chunk at offset 0 (depth may be 0); `.anm` finds it after the
"TV" prelude via an `FF 00` marker. `depth` is a mode value — plane count is
always 5 (32 colors). `globe.32` is the same chunk (1 frame, 64×74, depth 0)
but XOR-obfuscated on disk; the decoder unwraps it automatically. Decoder
validated against real files with `editor/tests/gfx_dump.cpp` (renders correct
castle/cave wall tiles). The 60 map/area names live in `core/AreaNames.h`
(transcribed from `b3dmm2/mm2ed.bb`).
