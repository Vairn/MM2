# MM2ED — Might & Magic II (Amiga) Data Editor

Dear ImGui + GLFW + OpenGL3 editor for reverse-engineered MM2 `.dat` files.
Shell layout follows the GRACE editor: **Project Tree → Workspace tabs → Properties**,
with codecs in `core/` and UI in `sections/`.

## Build

```bash
cmake -S editor -B editor/build -G Ninja
cmake --build editor/build
./editor/build/MM2ED        # MM2ED.exe on Windows
```

Dependencies (Dear ImGui *docking* branch, GLFW 3.4, portable-file-dialogs,
ImGuiColorTextEdit) are fetched by CMake `FetchContent`.

## Usage

1. **File → Open Data Folder…** (folder with `items.dat` / `map.dat` / …), or pass the
   folder on the CLI: `MM2ED path/to/data`.
2. Open documents from the **Project Tree** (Game data / World / Graphics).
3. Edit in **Workspace** tabs; field inspectors live in **Properties**.
4. **Ctrl+S** saves the active document; **File → Save All** writes every dirty file.
5. Recent folders are remembered in `mm2ed.ini` beside the exe.

Marks: `*` unsaved, `o` missing, `·` open in a tab.

**Events:** edit `.mm2evt` text → **Compile** (writes location into memory) → **Save**
(writes `event.dat` to disk). Status bar shows script vs file dirty. Switching
location / overlay with an unsaved buffer prompts to confirm.

**Run:** set env `MM2ED_RUN` to a host/exe; **Run → Launch** or **F5** starts it with
the current data folder as argument / working directory.

## App layout

| Dock | Role |
|------|------|
| Project Tree | Grouped browse; opens Workspace tabs |
| Workspace | Tabbed open documents + kind-specific tools |
| Properties | Selection-driven inspector |
| Status bar | Path, dirty counts, last action |

## Project layout

```
editor/
  CMakeLists.txt
  src/
    main.cpp              GLFW/OpenGL3 init + main loop
    app/
      App.{h,cpp}         Dock shell, menus, tabs, prefs
      DocKind.h           Document kinds
      EditorSelection.h   Cross-panel selection
      EditorPrefs.*       Recent folders (mm2ed.ini)
      Section.h           Section base (workspace + properties)
      AppState.h          Shared dirs + status
    core/                 Data codecs (no ImGui)
    sections/             Per-document UI
    widgets/              HexView, Mm2EvtEditor, UiLayout, UiTheme
    eventlang/            .mm2evt AST / decompile / DSL / encode
  tools/mm2evt_dump.cpp   CLI dump / roundtrip
```

## Adding a document kind

1. Add `core/XxxFile.{h,cpp}` with load/save/decode/encode.
2. Add `sections/XxxSection` implementing `docKind`, `drawWorkspace`, `drawProperties`.
3. Register in `App::registerSections()` and extend `DocKind` / `DocKindGroup`.

## Decode status

| File | Status | Editor |
|------|--------|--------|
| items.dat | fully decoded | list + Properties fields |
| spells.dat | fully decoded | list + Properties fields |
| roster.dat | layout confirmed | list + character tabs |
| str.dat | encoding decoded | line table + Properties |
| monsters.dat | fields partial | list + Properties + sprite preview |
| map.dat | confirmed | Tiles / Window / 3D + Properties |
| attrib.dat | partial | screen editor + Properties summary |
| event.dat | VM + DSL | Outline / Script / Problems + inspector |
| .32 / .anm / PC gfx | view-only | preview (read-only) |
