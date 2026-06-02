# MM2 audio call paths: title + death + shared backend

## Scope and evidence policy

This document merges the currently useful paths for MOD-targeted extraction:

- title music loop path
- party death / combat-defeat tone path
- shared backend audio calls used by those paths

Main sections contain **proven edges only** from direct instruction evidence in:

- `EXTRACTED/mm2.asm` (IRA)
- `EXTRACTED/mm2.capstone.asm` (Capstone)

Anything inferred or unresolved is isolated in the appendix.

## Confidence + dual-view target model

The MOD intermediate export now carries both target views per family/event:

- `static_target` = thunk-map/static resolution
- `observed_behavior_target` = observed runtime-entry behavior
- `confidence` = `proven` / `likely` / `uncertain`

Current corrected deterministic inventory (`EXTRACTED/mm2_mod_audio_intermediate.json`):

- families: `4`
- events: `18`
- short-note helper callsites (`-$7C62(A4)`): `232` (corrected from prior overcount)
- walk-beep candidates (immediate note `0x2D`): `7`

Contradictory thunk interpretation is explicitly scored:

- **proven:** static and observed targets align.
- **likely:** call edge is proven but target identity differs by view (notably `-$7E96(A4)` static `0x063EE` vs observed `0x007DCC`).
- **uncertain:** reserved for unresolved target/semantic cases.

---

## Proven: title path

### A) Title gating and blocking song call

Capstone evidence:

- `0x001000`: `bne.b $1034` (gate after sounds flag test)
- `0x001014`: `move.w #$12d, -(a7)`
- `0x001028`: `jsr -$7b9c(a4)`

IRA evidence (same path):

- `;01022`: `CMPI.B #$01,-31154(A4)`
- `;01028`: `BNE.S LAB_105C`
- `;0103c`: `MOVE.W #$012d,-(A7)`
- `;01050`: `JSR -31644(A4)` (same thunk call as Capstone `-$7b9c(A4)`)

Proven edge:

- title setup pushes song id `0x12D` and calls `-$7B9C(A4)` from `0x001028`.

### B) Title loop stepping

Capstone evidence:

- `0x001ff6`: `move.w #$12d, -(a7)`
- `0x002004`: `jsr -$7ba8(a4)`
- `0x00200e`: `cmpi.w #$48, -$24(a5)`
- `0x002014`: `blt.b $1ff6`

IRA evidence:

- `;0201e`: `MOVE.W #$012d,-(A7)`
- `;0202c`: `JSR -31656(A4)` (same thunk call as Capstone `-$7ba8(A4)`)
- `;02036`: `CMPI.W #$0048,-36(A5)`
- `;0203c`: `BLT.S LAB_201E`

Proven edge:

- title loop calls `-$7BA8(A4)` with `song=0x12D`, tempo `0xE8`, for `0x48` (72) looped steps.

### C) Title short chirp (note path)

Capstone evidence:

- `0x00104a`: `move.b -$79b1(a4), d0`
- `0x001052`: `jsr -$7c62(a4)`

IRA evidence:

- `;01072`: `MOVE.B -31153(A4),D0`
- `;0107a`: `JSR -31842(A4)` (same thunk call as Capstone `-$7c62(A4)`)

Proven edge:

- title path also invokes the short note helper through `-$7C62(A4)`.

---

## Proven: death / defeat path

### A) Two upstream entrypoints into shared death-tone backend

Capstone evidence:

- `0x009f22`: `jsr -$7e96(a4)` (party-death branch)
- `0x011660`: `jsr -$7e96(a4)` (combat-defeat path)

Proven edge:

- both party-death and combat-defeat route into the same `-$7E96(A4)` backend entry.

### B) Death backend initializes tone sequence

Capstone evidence:

- `0x007dcc`: `link.w a5, #$fff8`
- `0x007e0a`: `move.w #$15, -(a7)`
- `0x007e0e`: `move.w #$11, -(a7)`
- `0x007e12`: `jsr -$7bfc(a4)`

IRA evidence:

- `;07df4`: `LINK.W A5,#-8`
- `;07e32`: `MOVE.W #$0015,-(A7)`
- `;07e36`: `MOVE.W #$0011,-(A7)`
- `;07e3a`: `JSR -31740(A4)` (same thunk call as Capstone `-$7bfc(A4)`)

Proven edge:

- death backend emits fixed setup tone/voice calls before per-type tone dispatch.

### C) Death backend per-type tone dispatch (`0x31/0x32/0x33`)

Capstone evidence:

- `0x007e22`: `move.w #$33, -(a7)` and `move.w #$31, -(a7)` then `jsr -$7f68(a4)` (type picker)
- `0x007e90`: `cmpi.w #$31, -$2(a5)`
- `0x007ed2`: `cmpi.w #$32, -$2(a5)`
- `0x007f14`: `cmpi.w #$33, -$2(a5)`
- `0x007f0e`: `jsr -$7bde(a4)` (type `0x32` branch)
- `0x007f54`: `jsr -$7bde(a4)` (type `0x33` branch)

IRA evidence:

- `;07e4a`: `MOVE.W #$0033,-(A7)` / `MOVE.W #$0031,-(A7)` then `JSR -32616(A4)`
- `;07e70`: `CMPI.W #$0031,-2(A5)`
- `;07e86`: `CMPI.W #$0032,-2(A5)`
- `;07e9c`: `CMPI.W #$0033,-2(A5)`

Proven edge:

- death backend dispatches type-driven tones and reaches `-$7BDE(A4)` in `0x32` and `0x33` branches.

---

## Proven shared backend calls used by both domains

- `-$7BFC(A4)` appears in title and death paths as a shared audio setup call.
- `-$7C62(A4)` is the shared short-note helper used by title chirp.
- `-$7BA8(A4)` is proven for iterative title song stepping.
- `-$7B9C(A4)` is proven for blocking title song invocation.
- `-$7BDE(A4)` is proven in death per-type tone branches.

---

## MOD generation pipeline (practical milestones)

### 1) Derive pattern/note events for tracks

Unblocked now:

- export deterministic title-loop step events from proven loop callsite:
  - loop callsite `0x002004` (`-$7BA8(A4)`)
  - loop bound from `0x00200E`/`0x002014` (`0x48` iterations)
  - song id from stack push `0x001ff6` (`0x12D`)
  - tempo push `0x001ffe` (`0xE8`)

Milestone:

1. Emit intermediate JSON records (`song_step` events with callsite + params + loop metadata).
2. Build converter that maps those records into tracker pattern rows.
3. Resolve note-period mapping for full melodic reconstruction (requires deeper table decode, see blocked list).

### 2) Derive/export short SFX samples (walk beep etc.)

Unblocked now:

- export proven death-tone events from shared backend:
  - `play_tone` calls at `0x007f0e` and `0x007f54`
  - each annotated with parameter sources (`$5c(a0)` or `$25(a0)`, channel/flags pushes)

Partially blocked:

- walk beep can be located as short-note path usage (`-$7C62(A4)`), but full extraction of all walk contexts and stable per-context labeling is not fully merged into this doc yet.

Milestone:

1. Export death-tone calls as candidate SFX sources.
2. Add dedicated walk-beep call graph extraction.
3. Render each SFX event into short PCM (or synthetic source descriptors) for MOD sample slots.

### 3) Blocked vs unblocked summary

Unblocked:

- deterministic extraction of title start/step/poll events
- deterministic extraction of death tone dispatch (`0x31/0x32/0x33`) with confidence tagging
- deterministic walk-beep candidate callsites (immediate note `0x2D` into `-$7C62(A4)`) = `7`
- short-note SFX metadata buckets from full `-$7C62(A4)` caller inventory = `232` callsites
- machine-readable event annotations with exact source addresses and stack-parameter provenance
- dual-view `static_target` vs `observed_behavior_target` fields for contradictory thunk evidence

Blocked:

- complete note/period table decode into tracker-note space for full track rebuild
- globally validated semantics for every audio thunk variant (`-$7BD8`, `-$7BFC`, etc.)
- full walk/beep event coverage and final sample-shaping policy for MOD-friendly output

---

## Appendix A: uncertain or inferred edges (not used as main proven graph)

1. **`-$7E96(A4)` exact target naming mismatch**
   - Existing thunk maps in prior tooling may label this slot as a different absolute target than the `0x007dcc` routine shown above.
   - Routing into `-$7E96(A4)` is proven; exact symbol naming/target mapping remains to be normalized.

2. **`-$7BD8(A4)` call semantics in death type `0x31` path**
   - Capstone at `0x007ecc` calls `-$7bd8(a4)` with a long value (`$66(a0)`).
   - It is likely tone-related but not yet formally classified in current docs as the same operation family as `-$7BDE(A4)`.

3. **Walk beep canonical path set**
   - `-$7C62(A4)` usage is proven broadly, but the final canonical set for "walk beep" specifically is still being narrowed to avoid false positives.
