# MM2 Reverse-Engineering Overview

This directory splits the big `mm2-ANALYSIS.md` into focused documents that are easier to evolve while reversing and decompiling.

**Wiki hub:** [`README.md`](README.md) — full index with sections, regeneration commands, and links to all numbered docs.

## Document Map (core runtime)

- `01-startup-init.md` — Boot path, hunk entry, DOS/Exec setup, MANX heap bring-up.
- `02-runtime-memory-map.md` — `A4` workspace map, thunk tables, and key runtime wrappers.
- `03-main-loop-and-map.md` — Main scheduler (`LAB_1280`) and overland map renderer (`LAB_24C4`).
- `04-party-and-session.md` — New game / party copy flow and session restart (`LAB_545E`).
- `05-open-questions.md` — Unknowns, next trace targets, and TODOs.
- `06-gfx-loading.md` — Confirmed `.32`/`.dat` asset table and the current loader call path.
- `07-dat-files-and-formats.md` — MM2-local `.dat` inventory plus confirmed format decoding notes.

## Address Rules

- Capstone addresses are code-hunk-relative (segment 0 base 0).
- IRA listing tends to be offset by `+0x20` from Capstone in early code.
- Always verify with raw bytes in `mm2` when labels disagree.

## Source Files

- `EXTRACTED/mm2.capstone.asm`
- `EXTRACTED/mm2.asm`
- `EXTRACTED/mm2-ANALYSIS.md` (full running narrative)
