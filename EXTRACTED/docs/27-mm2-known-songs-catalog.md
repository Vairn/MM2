# MM2 known songs and SFX catalog (SUPERSEDED)

> **SUPERSEDED (2026-07-10).** Title “song id `0x12D`” was a blit Y coordinate;
> `audio_init@0x823C` / `master.32` claims were wrong. **Current catalog:**
> [`58-amiga-audio-in-exe.md`](58-amiga-audio-in-exe.md) (10 DATA-hunk sequences +
> title theme @ DATA `0x1D79`). Kept below for archaeology only.

## Scope (historical)

This is a factual catalog of what is currently identified from:

- `EXTRACTED/docs/25-mm2-music-format.md`
- `EXTRACTED/docs/26-audio-callpaths-title-death-shared.md`
- `EXTRACTED/mm2_mod_audio_intermediate.json`

Confidence labels:

- **proven**: direct instruction-level evidence with aligned target behavior.
- **likely**: strong evidence, but naming/target semantics still partly unresolved.
- **uncertain**: tracked placeholder where behavior is known but identity/semantics are not yet confirmed.

## Songs / score-track families

### Song: `0x12D` (title score)

- **Identifier/name:** song id `0x12D` (main title score; formal title unknown).
- **Where called (addresses/callsites):**
  - Start/blocking call: `0x001028` (`jsr -$7B9C(A4)`), params pushed from `0x001010..0x001026`.
  - Step loop call: `0x002004` (`jsr -$7BA8(A4)`), loop control at `0x00200E` / `0x002014`.
  - Also referenced in title loop setup at `0x001FF6`.
- **Thunk/backend path summary:**
  - `-$7B9C(A4) -> 0x22EAA` (song start path).
  - `-$7BA8(A4) -> 0x22D68` (per-step sequencer path).
  - Uses song-bank/runtime audio tables initialized by `audio_init`.
- **Known parameters:**
  - Start call uses channel `0x47`, song `0x12D`, arg2 `0x08`, level/tempo-like `0xE0`, gate `0x01`, and zeroed args.
  - Step loop uses song `0x12D`, tempo `0xE8`, step index from `-$24(A5)`, loop bound `0x48` (72 iterations).
- **Confidence:** `proven`.
- **Unresolved questions:**
  - Canonical in-game name for `0x12D`.
  - Exact semantic naming for all start-call args (`arg2`, `arg4`, `arg6`).

### Family: scripted score ids via `play_song_scripted` (IDs not cataloged yet)

- **Identifier/name:** unknown scripted score id set (placeholder family).
- **Where called (addresses/callsites):**
  - Scripted score helper at `0x64F8` (uses pointer table `A4-$73C4`).
  - Referenced by scripted flow (example call path noted near `0x78E6` in existing docs).
- **Thunk/backend path summary:**
  - Script helper routes to the same backend song machinery as title score paths.
  - Table-driven indirection indicates score ids are not a simple direct map to 60 song-bank rows.
- **Known parameters:**
  - Uses table-driven pointer/id values from runtime state (`A4-$73C4` family).
  - No concrete additional numeric song ids are proven in current extraction output.
- **Confidence:** `uncertain`.
- **Unresolved questions:**
  - Enumerate actual scripted score ids beyond `0x12D`.
  - Determine stable mapping from scripted ids to song-bank patterns.

### Family: 60-song bank (`A4-$4F4C`) patterns

- **Identifier/name:** song-bank family (60 rows x 16 steps, no per-row song names yet).
- **Where called (addresses/callsites):**
  - Bank loaded during `audio_init` (`0x823C`) from master blob offset `0x780`.
  - Step playback logic in `0x2188` region consumes indexed step words.
- **Thunk/backend path summary:**
  - Runtime step-play path uses indexed song-step words and downstream tone dispatch.
  - Title stepping (`-$7BA8`) is proven to consume this sequencer infrastructure.
- **Known parameters:**
  - Layout: 60 songs x 16 steps x 2 bytes (`1920` bytes total).
  - Step addressing follows `song_index * 0x20 + step * 2`.
- **Confidence:** `proven` (structure), `uncertain` (human song-name mapping).
- **Unresolved questions:**
  - Which rows correspond to named gameplay songs (if any beyond title family).
  - How scripted ids map onto this bank in all contexts.

## Short SFX families

### SFX family: walk beep (`note 0x2D`)

- **Identifier/name:** walk-beep candidate family (`play_note` with immediate `0x2D`).
- **Where called (addresses/callsites):**
  - 7 deterministic callsites: `0x00053A`, `0x000A78`, `0x00A87A`, `0x01BA20`, `0x01BDA8`, `0x01DA0C`, `0x01DB60`.
- **Thunk/backend path summary:**
  - `-$7C62(A4) -> 0x218EA` (`play_note` helper).
- **Known parameters:**
  - Note index immediate `0x2D` (45) at each listed callsite.
  - Family is part of broader `-$7C62(A4)` inventory (`232` short-note callsites total).
- **Confidence:** `likely`.
- **Unresolved questions:**
  - Final filtering from "candidate beep" to canonical walk-only contexts.

### SFX family: party death / combat defeat tones

- **Identifier/name:** death-tone dispatch family (`types 0x31/0x32/0x33`).
- **Where called (addresses/callsites):**
  - Entry calls: `0x009F22` (party wipe), `0x011660` (combat defeat), both `jsr -$7E96(A4)`.
  - Dispatch core: `0x007E2A` (type picker), per-type tones at `0x007ECC`, `0x007F0E`, `0x007F54`.
- **Thunk/backend path summary:**
  - `-$7E96(A4)` entry routes to observed death backend (`0x007DCC`) but static map disagrees (`0x063EE`).
  - `-$7F68(A4) -> 0x042AA` type selection.
  - `-$7BDE(A4) -> 0x22480` proven in type `0x32`/`0x33` branches.
  - `-$7BD8(A4) -> 0x2249C` likely related tone path for type `0x31`.
- **Known parameters:**
  - Type picker min/max: `0x31..0x33`.
  - Type `0x31` path uses long source `$66(A0)` with channel/mode `0x20` and gate `0x01`.
  - Type `0x32` path uses word source `$5C(A0)` with channel/mode `0x20` and gate `0x01`.
  - Type `0x33` path uses byte source `$25(A0)` (via `D0`) with channel/mode `0x20` and gate `0x01`.
- **Confidence:** `likely` for family entry mapping; branch-level tones mostly `proven`.
- **Unresolved questions:**
  - Normalize contradictory static vs observed target identity for `-$7E96(A4)`.
  - Fully classify `-$7BD8(A4)` semantics relative to `-$7BDE(A4)`.

### SFX family: combat victory short-note loop

- **Identifier/name:** combat-victory jingle loop (short-note repetition).
- **Where called (addresses/callsites):**
  - Victory handler rooted at `0x12430`; repeated note path around `0x12508..0x12544`.
- **Thunk/backend path summary:**
  - Uses `-$7C62(A4)` short-note path in repeated loops.
- **Known parameters:**
  - Note index `0x05` repeated in two loop blocks of `0x17` iterations each (existing trace docs).
- **Confidence:** `proven`.
- **Unresolved questions:**
  - Confirm final timing/spacing constants as tracker-row equivalents.

### SFX family: combat-enter cue (short-note + optional tone)

- **Identifier/name:** combat-enter cue family.
- **Where called (addresses/callsites):**
  - Encounter setup range `0x12C6E..0x12E58`, including note call near `0x12E42`.
- **Thunk/backend path summary:**
  - `-$7C62(A4)` for the note cue.
  - Optional `play_tone` path when monster count exceeds threshold.
- **Known parameters:**
  - Immediate note `0x2B` (43) for entry cue.
  - Conditional duration-like argument derived from `monster_count - 10`.
  - Music channel allocation path includes channel `0x16`.
- **Confidence:** `likely` (core call proven, some parameter semantics still inferred).
- **Unresolved questions:**
  - Final function/argument labeling for the optional tone branch.

### SFX family: title UI chirp

- **Identifier/name:** title-interface chirp.
- **Where called (addresses/callsites):**
  - Title path callsite `0x001052`.
- **Thunk/backend path summary:**
  - `-$7C62(A4) -> 0x218EA` short-note helper.
- **Known parameters:**
  - Observed note value path includes note `0x11` in title setup flow.
- **Confidence:** `proven` (callsite), `likely` (exact UI state coverage).
- **Unresolved questions:**
  - Full set of title states that trigger this chirp versus other `-$7C62` uses.

## Current certainty snapshot

- **Proven families/entries:** title `0x12D`, song-bank structure, combat victory loop core, several death-branch subcalls.
- **Likely families/entries:** walk beep family, combat-enter cue family, death-family thunk identity at entry.
- **Uncertain families/entries:** scripted score-id family (ids beyond `0x12D` not yet cataloged).

## Related artifacts

- `EXTRACTED/docs/25-mm2-music-format.md`
- `EXTRACTED/docs/26-audio-callpaths-title-death-shared.md`
- `EXTRACTED/mm2_mod_audio_intermediate.json`
