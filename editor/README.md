# MM2ED — Might & Magic II (Amiga) Data Editor

A Dear ImGui + GLFW + OpenGL3 editor for the reverse-engineered MM2 `.dat`
data files. Structured so that **each data type has its own self-contained
section** (modeled on the GRACE and LorED editors), with a clean split between
the data layer (`core/`) and the UI layer (`sections/`).

## Build

Dependencies (Dear ImGui *docking* branch, GLFW 3.4, portable-file-dialogs)
are fetched automatically by CMake `FetchContent` — no vendoring required.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MM2ED        # MM2ED.exe on Windows
```

Requires a C++17 compiler, CMake >= 3.16, and internet access on first
configure (to clone deps into `build/_deps`).

## Usage

1. **File > Open Data Folder...** and pick the directory containing the MM2
   `.dat` files (e.g. the workspace root, or `MM2BOOT/`).
2. Pick a data type in the **Data Files** panel on the left.
3. Edit in the **Editor** panel. `*` = unsaved changes, `x` = file not found.
4. **Ctrl+S** saves the active file; **File > Save All** writes every loaded file.

## Project layout

```
editor/
  CMakeLists.txt          FetchContent deps + glob of src/
  src/
    main.cpp              GLFW/OpenGL3 init + main loop
    app/
      App.{h,cpp}         Dockspace, menu, section registry, save/load, cross-refs
      AppState.h          Shared cross-panel state (data dir, status line)
      Section.h           Abstract base: title/fileName/load/save/draw
    core/                 Data layer (no ImGui) — one model per .dat file
      ByteIO.{h,cpp}      Endian read/write helpers + file IO
      ItemsFile.{h,cpp}   items.dat   (256 * 20)  — fully decoded
      MonstersFile.{h,cpp}monsters.dat(256 * 26)  — geometry + partial fields
      RosterFile.{h,cpp}  roster.dat  (64 * 130)  — confirmed layout (LE)
      MapFile.{h,cpp}     map.dat     (60 * 512)  — visual + collision pages
      AttribFile.{h,cpp}  attrib.dat  (60 * 64)   — partial (env + roof bits)
      StrFile.{h,cpp}     str.dat     (text pool) — (byte+0x1C)&0xFF transform
      RawFile.h           generic byte container (spells/event)
      AreaNames.h         60 map/area names + env-type decode (from b3dmm2)
      Gfx.{h,cpp}         planar image-chunk decoder for .32 and .anm
    sections/             UI layer — one editor per data type
      ItemsSection, MonstersSection, RosterSection, MapSection,
      AttribSection, StrSection, RawSection (spells.dat, event.dat),
      GfxSection (.32 tilesets and .anm animations)
    widgets/
      HexView.{h,cpp}     shared scrollable hex+ASCII dump
      Texture.{h,cpp}     RGBA -> OpenGL texture helper for previews
  tests/
    gfx_dump.cpp          offline gfx-decoder validator (not in CMake build)
```

## Graphics (.32 / .anm)

Both formats share a planar image-chunk codec (5 Amiga bitplanes, 32-color
`0x0RGB` palette, nibble-RLE pixel stream). `.32` tilesets begin the chunk at
offset 0; `.anm` animations carry a "TV" prelude and the chunk is located via
an `FF 00` marker. The **Graphics (.32)** and **Animations (.anm)** sections
scan the data folder, decode the selected file, and preview frames (single +
contact sheet), the palette, and per-frame metadata. Decoding is verified
against real wall/UI `.32` sheets via `tests/gfx_dump.cpp`. Note: `globe.32` and
`disk.32` use the `.32` extension in the resource table but are XOR data blobs
(copy-protection strings / other), not planar image chunks. The `.anm` viewer also includes an **experimental** "Use prelude placement" toggle
that draws each selected frame at its prelude `(x,y)` on a shared canvas for
layout sanity checks without forcing a hard format interpretation.

## Adding a new section

1. Add a `core/XxxFile.{h,cpp}` model with `decode`/`encode`/`load`/`save`.
2. Add a `sections/XxxSection.{h,cpp}` subclass of `Section`.
3. `#include` it and `sections_.push_back(std::make_unique<XxxSection>())` in
   `App::registerSections()`.
4. Reconfigure (the CMake glob picks up new files automatically).

## Decode status (see EXTRACTED/docs and b3dmm2)

| File         | Status                          | Editor |
|--------------|---------------------------------|--------|
| items.dat    | fully decoded                   | full field editor |
| roster.dat   | layout confirmed (some unknown) | stats/equipment/spells tabs |
| str.dat      | encoding decoded                | text editor |
| monsters.dat | 26-byte records; fields partial | name + named/raw fields |
| map.dat      | 512 B/screen confirmed          | tile + collision grids |
| attrib.dat   | partial (env + roof)            | known fields + roof grid + hex |
| spells.dat   | fully decoded                   | cast/SP/gem/outdoor + reference text |
| event.dat    | container + VM decoded          | imnodes graph editor (triggers/scripts/strings) |
