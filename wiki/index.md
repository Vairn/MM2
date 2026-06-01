---
layout: home
title: MM2 Reverse Engineering
hero:
  name: Might & Magic II
  text: Amiga reverse-engineering wiki
  tagline: Data formats, 68k runtime traces, combat, events, graphics codecs, and the MM2ED editor — assembled from EXTRACTED/docs and tooling in this repo.
  actions:
    - theme: brand
      text: Start Reading
      link: /docs/reverse-engineering/00-overview
    - theme: alt
      text: Data Formats
      link: /docs/reverse-engineering/07-dat-files-and-formats
    - theme: alt
      text: MM2ED Editor
      link: /docs/editor/mm2ed
features:
  - icon: 📦
    title: Decoded .dat formats
    details: items, roster, map, monsters, spells, event scripts, and attrib — with ASM-confirmed field layouts and round-trip codecs in Python and C.
  - icon: ⚔️
    title: Game systems
    details: Combat round engine, monster abilities, 3D view / collision pages, copy protection, and the event-script opcode reference.
  - icon: 🛠️
    title: Tools & editor
    details: Python disassemblers and decoders, C lift scaffold, and MM2ED — an ImGui editor for every major data file.
  - icon: 🗺️
    title: Living documentation
    details: Pages sync from EXTRACTED/docs on each dev/build run — edit markdown in the repo, refresh the wiki.
---

## Quick links

| Topic | Page |
|-------|------|
| Boot & startup | [01-startup-init](/docs/reverse-engineering/01-startup-init) |
| `A4` workspace map | [02-runtime-memory-map](/docs/reverse-engineering/02-runtime-memory-map) |
| `items.dat` layout | [18-items-dat-format](/docs/reverse-engineering/18-items-dat-format) |
| Combat engine | [17-combat-system](/docs/reverse-engineering/17-combat-system) |
| Event opcodes | [07-event-script-opcodes](/docs/reverse-engineering/07-event-script-opcodes) |
| RE tooling | [re-tools](/docs/tools/re-tools) |

## Endianness reminder

MM2 `.dat` multibyte fields are **little-endian on disk** (matching Blitz3D editors and our codecs). The 68000 runtime may byte-swap after load — always trace ASM before assuming otherwise.
