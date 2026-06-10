# MM2 Reverse-Engineering Wiki

Central index for **Might and Magic II (Amiga)** documentation in this repo.
Source of truth: `EXTRACTED/docs/`, `EXTRACTED/mm2-ANALYSIS.md`, `tools/`, `editor/`,
`game/`.

**Also published:** [GitHub Wiki](https://github.com/Vairn/MM2/wiki) via
`python wiki/scripts/export-github-wiki.py` (see [`wiki/README.md`](../../wiki/README.md)).

> **Endianness:** `.dat` multibyte fields are **little-endian on disk** unless a
> specific doc notes an exception (see root [`CLAUDE.md`](../../CLAUDE.md)).

---

## Quick start

| Goal | Start here |
|------|------------|
| Project overview | [`00-overview.md`](00-overview.md) → [`mm2-ANALYSIS.md`](../mm2-ANALYSIS.md) |
| All `.dat` layouts | [`07-dat-files-and-formats.md`](07-dat-files-and-formats.md) |
| Edit data files | [`editor/README.md`](../../editor/README.md) |
| Run the remake | [`game/README.md`](../../game/README.md) |
| RE tooling | [`tools/RE-TOOLS.md`](../../tools/RE-TOOLS.md) |
| Open unknowns | [`05-open-questions.md`](05-open-questions.md) |

---

## Runtime & engine

| Doc | Description |
|-----|-------------|
| [`00-overview.md`](00-overview.md) | Split-doc map and address rules |
| [`01-startup-init.md`](01-startup-init.md) | Boot path, hunk entry, DOS/Exec, MANX heap |
| [`02-runtime-memory-map.md`](02-runtime-memory-map.md) | `A4` workspace, thunk tables, wrappers |
| [`03-main-loop-and-map.md`](03-main-loop-and-map.md) | Main scheduler `LAB_1280`, overland map |
| [`43-exploration-input-and-ingame-options.md`](43-exploration-input-and-ingame-options.md) | **Exploration key dispatch `$1482`** — full key table, Options/Protect panels, Controls screen, char-sheet/cast entry |
| [`04-party-and-session.md`](04-party-and-session.md) | New game, party copy, session restart |
| [`05-open-questions.md`](05-open-questions.md) | Unknowns and next trace targets |
| [`08-event-runtime.md`](08-event-runtime.md) | Event VM dispatch and collision triggers |
| [`13-time-era-calendar.md`](13-time-era-calendar.md) | In-game calendar, day/night, era |
| [`14-game-state-struct.md`](14-game-state-struct.md) | `A4` field layout and party state |
| [`29-embedded-exe-strings.md`](29-embedded-exe-strings.md) | Code-hunk ASCII tables and UI text |

---

## Data formats (`.dat` and assets)

| Doc | Description |
|-----|-------------|
| [`07-dat-files-and-formats.md`](07-dat-files-and-formats.md) | Inventory hub — all MM2 data files |
| [`06-gfx-loading.md`](06-gfx-loading.md) | `.32` filename table and loader call path |
| [`07-anm-tv-format.md`](07-anm-tv-format.md) | `.anm` TV prelude + planar image chunk |
| [`06-roster-format.md`](06-roster-format.md) | `roster.dat` — 64 × 130-byte party records |
| [`12-attrib-dat-format.md`](12-attrib-dat-format.md) | `attrib.dat` — per-screen env and neighbours |
| [`18-items-dat-format.md`](18-items-dat-format.md) | `items.dat` — 256 × 20-byte items |
| [`19-spells-and-item-use.md`](19-spells-and-item-use.md) | `spells.dat` + item use-byte spell index |
| [`21-map-dat-format.md`](21-map-dat-format.md) | `map.dat` — 60 screens, visual + collision pages |
| [`06-event-dat-format.md`](06-event-dat-format.md) | `event.dat` — 71-location container format |
| [`07-event-script-opcodes.md`](07-event-script-opcodes.md) | Script VM opcodes `0x00..0x32` |
| [`45-event-graphics-opcodes.md`](45-event-graphics-opcodes.md) | **Event VM sprite/sign ops** — `OP_0B`, Corak/Pegasus paths |
| [`16-monster-ability-format.md`](16-monster-ability-format.md) | `monsters.dat` — 256 × 26-byte records |
| [`20-copy-protection-table.md`](20-copy-protection-table.md) | `globe.32` / `disk.32` XOR string tables |

**Regeneration commands**

```powershell
python tools\decode_event.py event.dat <loc>          # decode one location
python tools\build_event_location_docs.py             # events/loc_*.md (71 files)
python tools\build_doc37.py                           # 37-mount-farview-class-quest-event.md
python tools\decode_spells.py spells.dat              # spells round-trip check
python tools\mm2_codec.py items-decode items.dat …    # items / str codecs
```

**Events per map (71 locations):** [`events/README.md`](events/README.md) · [`40-events-by-location.md`](40-events-by-location.md)

---

## Graphics & 3D view

| Doc | Description |
|-----|-------------|
| [`15-3d-view-and-game-screen.md`](15-3d-view-and-game-screen.md) | First-person hood, wall bits, auto-map |
| [`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md) | Title boot, `intro.32` / `introclips.32` / `nwcp.32` |
| [`39-title-screen-animation.md`](39-title-screen-animation.md) | Pegasus overlays, peekers, menu — **authoritative coords** |
| [`45-event-graphics-opcodes.md`](45-event-graphics-opcodes.md) | In-game `OP_0B` signboards vs title-screen pegasus |
| [`46-scripted-scene-graphics.md`](46-scripted-scene-graphics.md) | **Corak ghost + Pegasus illustration** — castle engine (`0x6FB8`/`0x64F8`/`0x316E`); **§10 full scene catalog** (loc 60–70, blobs 63/65/68, `play_song_scripted` ids 2–6) |

---

## UI & title screen

| Doc | Description |
|-----|-------------|
| [`39-title-screen-animation.md`](39-title-screen-animation.md) | Attract animation, peeker slots, timing, remake |
| [`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md) | ASM blit traces, logo `0x26BC4`, asset load strings |
| [`39-character-ui-view-create.md`](39-character-ui-view-create.md) | **P** / **C** keys, character sheet @ `$39B4` |

Remake: `game/src/TitleScreen.cpp` — **320×200**, `Mm2Font8x8.inc`, menu on **black**
(no `intro.32` under boxes).

---

## Combat

| Doc | Description |
|-----|-------------|
| [`26-combat-overview.md`](26-combat-overview.md) | Round loop summary and battle slots |
| [`17-combat-system.md`](17-combat-system.md) | Full ASM trace — player bar, AI, rewards |
| [`26-spell-cast-asm.md`](26-spell-cast-asm.md) | Spell cast dispatch in combat |
| [`35-encounter-tables.md`](35-encounter-tables.md) | Random step, arena, OP_12 fixed fights |

---

## Events & scripts

| Doc | Description |
|-----|-------------|
| **[`events/README.md`](events/README.md)** | **Index of all 71 locations** — triggers, scripts, strings per map |
| [`40-events-by-location.md`](40-events-by-location.md) | Hub link into `events/` + format cross-refs |
| [`06-event-dat-format.md`](06-event-dat-format.md) | Location header, triplets, string banks |
| [`07-event-script-opcodes.md`](07-event-script-opcodes.md) | Opcode argc, names, pseudo-code |
| [`08-event-runtime.md`](08-event-runtime.md) | Collision `0x80` → interpreter @ `0x172CA` |
| [`30-event-to-string-path.md`](30-event-to-string-path.md) | When scripts use `str.dat` vs exe text |
| [`44-event-text-rendering.md`](44-event-text-rendering.md) | **Pixel-exact event text** — draw thunks, window kernel, per-op dest rects, prompt loops |
| [`37-mount-farview-class-quest-event.md`](37-mount-farview-class-quest-event.md) | Class quests + Juror turn-in (loc 34) |
| [`36-class-quest-hp-bug.md`](36-class-quest-hp-bug.md) | Class-quest reward HP bug @ `0x9D76` |

---

## Gameplay formulas (FAQ-sourced)

| Doc | Description |
|-----|-------------|
| [`32-character-mechanics.md`](32-character-mechanics.md) | Race mods, HP/SP/AC/XP, thievery |
| [`33-skills-and-hirelings.md`](33-skills-and-hirelings.md) | 15 skill sellers, 24 hirelings A–X |
| [`34-commerce-formulas.md`](34-commerce-formulas.md) | Training, healing, portals, bar, shop cycle |
| [`34-commerce-and-world-services.md`](34-commerce-and-world-services.md) | Extended commerce / world services notes |
| [`28-town-services.md`](28-town-services.md) | Inn, temple, guild, training ASM handlers |
| [`31-spell-sources.md`](31-spell-sources.md) | Where each spell is obtained in-game |

---

## Audio

| Doc | Description |
|-----|-------------|
| [`25-audio-sounds-music.md`](25-audio-sounds-music.md) | Paula tones, Controls menu, walk beep |
| [`25-mm2-music-format.md`](25-mm2-music-format.md) | In-game music blob format |
| [`26-audio-callpaths-title-death-shared.md`](26-audio-callpaths-title-death-shared.md) | Title music, death tone, shared backend |
| [`27-mm2-known-songs-catalog.md`](27-mm2-known-songs-catalog.md) | Song id catalog |

---

## MM1 cross-walk

| Doc | Description |
|-----|-------------|
| [`22-mm1-mazedata-format.md`](22-mm1-mazedata-format.md) | DOS `MAZEDATA.DTA` layout |
| [`23-mm1-to-mm2-outdoor.md`](23-mm1-to-mm2-outdoor.md) | MM1 outdoor → MM2 sector mapping |
| [`24-mm1-outdoor-wallpix-by-sector.md`](24-mm1-outdoor-wallpix-by-sector.md) | MM1 wall sprite ids by sector |

---

## Tools, editor, decomp

| Doc | Description |
|-----|-------------|
| [`tools/RE-TOOLS.md`](../../tools/RE-TOOLS.md) | Disasm, Ghidra import, symbol harvest |
| [`editor/README.md`](../../editor/README.md) | MM2ED ImGui data editor |
| [`EXTRACTED/decomp/README.md`](../decomp/README.md) | C lift scaffold (`mm2_lift.c`, codecs) |
| [`CLAUDE.md`](../../CLAUDE.md) | Workspace quick-reference for agents |
| [`mm2-ANALYSIS.md`](../mm2-ANALYSIS.md) | Full narrative analysis (parts 1–13) |

**Symbol pipeline**

```powershell
python tools\harvest_symbols.py --merge
python tools\apply_symbols.py
python tools\extract_asm_parts.py
```

Outputs: `EXTRACTED/mm2.annotated.asm`, `EXTRACTED/asm/` splits.

---

## Game remake (`game/`)

| Doc | Description |
|-----|-------------|
| [`game/README.md`](../../game/README.md) | Build targets (SDL2 / Amiga ACE), phases |
| [`41-aga-port-plan.md`](41-aga-port-plan.md) | **AGA 6bp port** — palette zones, ACE planar, multi-combat sprites, future UI, extension dungeons/art |
| [`39-title-screen-animation.md`](39-title-screen-animation.md) | Title screen fidelity reference |
| [`39-character-ui-view-create.md`](39-character-ui-view-create.md) | Character UI backends |

---

## External wiki export

```powershell
python wiki/scripts/export-github-wiki.py          # full gallery if assets present
python wiki/scripts/export-github-wiki.py --skip-gallery
python wiki/scripts/publish-github-wiki.py       # push to MM2.wiki.git
```

Interactive map walker: [vairn.github.io/MM2/maze-walker/](https://vairn.github.io/MM2/maze-walker/)
