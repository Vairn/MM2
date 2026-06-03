# Title Screen Animation (pegasus + introclips)

Authoritative notes for how **`intro.32`** and **`introclips.32`** compose on the
pre-game title scene. Complements asset/boot tracing in
[`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md).

**Remake implementation:** `game/src/TitleScreen.cpp`  
**Display:** **320×200** NTSC (same as `intro.32` frame 0 and `gfx::ScreenCompositor`).

---

## Summary

| Layer | File | Behavior |
|-------|------|----------|
| Background | `intro.32` | Single frame blitted once @ **(3, 0)** — 320×200 pegasus scene |
| Pegasus patches | `introclips.32` frames **0–4** | **One cel at a time**, cycling mod 5 (ASM `0x26A1E`, `A4-$647A`) at **per-part screen coords** |
| Grey peekers | `introclips.32` frames **6–10** | **One** random peeker visible; slot changes every 2–4 s |
| Logo | `nwcp.32` | Black field + centered fade (Y = `(200 − height) / 2`, X = 10) |
| Menu | book UI + text | Drawn over a **frozen** composite (all static pegasus patches) |

**Do not** use **`A4-$632E` / `A4-$6344`** word tables @ `0x27096` as sprite (x, y)
anchors. Those words are **title-music / `draw_cell` staff positions** (many raw Y
values exceed 200). The `0x27096` loop is a **one-shot** copy-protection / prompt
sequence, not the idle pegasus wag.

---

## `introclips.32` frame inventory

Decoded from retail file (11 frames, standard `.32` image chunk):

| Frame | Size | Role on pegasus scene |
|------:|------|------------------------|
| 0 | 17×12 | Tail patch (cel A) |
| 1 | 17×12 | Tail patch (cel B) — **~34 RGB pixels differ from frame 0**; swap alone is subtle |
| 2 | 40×48 | Leg / body fragment |
| 3 | 10×8 | Small accent near tail |
| 4 | 48×36 | Mouth / head patch (cel A) |
| 5 | 48×36 | Mouth patch (cel B) — large diff vs frame 4; not in ASM mod-5 loop |
| 6 | 26×25 | Grey peeker slot A |
| 7 | 19×17 | Grey peeker slot B |
| 8 | 27×24 | Grey peeker slot C |
| 9 | 27×24 | Grey peeker slot D |
| 10 | 45×22 | Grey peeker slot E (tree / wide peek) |

Palette index **0** = transparent for all title `.32` loads
(`mm2_image32_set_preview_opaque(0)`).

---

## Pegasus overlay screen coordinates (320×200)

Each cel is placed so opaque pixels align with the matching hole in **`intro.32`**
(verified by overlay fit on retail assets; see `game/build/intro_aligned.png`).

| Frame | X | Y | Notes |
|------:|--:|--:|-------|
| 0 | 169 | 164 | Tail |
| 1 | 169 | 164 | Tail (alternate cel) |
| 2 | 35 | 124 | Leg |
| 3 | 170 | 155 | Accent |
| 4 | 265 | 73 | Mouth |
| 5 | 265 | 73 | Mouth alt (remake frozen/menu uses frame 4 only) |
| 6 | 250 | 106 | Tree peeker (right) |
| 8 | 246 | 128 | Tree peeker (right, lower) |
| 9 | 246 | 130 | Tree peeker alt cel |
| 10 | 95 | 96 | Tree peeker (left) |

Coords tuned ±3 px per cel by minimizing RGB diff vs `intro.32` @ (3, 0)
(retail assets in repo root). Reference composite: `game/build/intro_aligned_v2.png`.

Background blit: **`intro.32` frame 0 @ (3, 0)**.

---

## Amiga ASM paths (two introclips blit sites)

### `JSR -$7C20(A4)` — `blit(sheet, frame, y, x)`

Push order (first pushed = **x**, second = **y**): confirmed @ `0x260B8` (intro @ 3, 0).

### A) `0x26A1E` — pegasus overlay cycle (`A4-$7A2E`)

- **Caller:** `0x26BC4` logo / copy-protection hold only (`0x26E64 → 0x26B0C → 0x26A1E`).
- **Gate `A4-$6478`:** increment each poll; blit when **≥ 3**, then reset gate.
- **Frame `A4-$647A`:** **0 → 4**, wrap at 5 (mod 5).
- **Blit:** same frame index to **(8, 8)** and **(8, 216)** (`0xD8`) — corner/stack
  presentation on the logo path, **not** the per-part coords in the table above.
- On a **200-line** display, **y = 216** is below the visible framebuffer.
- **`0x1062` menu** does not call this; last blit pixels stay frozen on VRAM.

This mod-5 index is what the remake uses for **which** `introclips` cel is shown,
but at the **pegasus anchor table** (one cel at a time), not duplicated corners.

### B) `0x27096` — table-driven loop (`A4-$7A32`)

- **Callers:** `0x27F3A`, `0x2808C`, `0x28124` (copy-protection / create-prompt).
- Steps **i = 0..10**: `blit(introclips, frame=i, y = Y[i]−1, x = X[i]+8)`.
- **`A4-$632E`** (file `0x1CD0`): all **0** → x = **8**.
- **`A4-$6344`** (file `0x1CBA`):  
  `8, 43, 61, 148, 195, 221, 248, 264, 264, 241, 229` (raw words; Y blit = raw−1).

Interpretation: **music-note / staff vertical positions** for `draw_cell` @ `0x2712E`
when **i = 0**, not anatomical pegasus part placement. Several Y values **> 200**.
Do **not** use this table for `game/TitleScreen.cpp` overlay coords.

### C) `0x1062` / `0xF1C` — menu

No `introclips` blits; menu text/boxes on retained framebuffer. Handles freed @
`0x26534` / `0x26338C` before hotkey loop.

---

## Remake state machine (`game/TitleScreen.cpp`)

| State | Layers |
|-------|--------|
| **LogoFadeIn / Hold / FadeOut** | Black + **`nwcp.32`** (alpha fade, vertically centered on 200px) |
| **TitleAttract** | `intro.32` + **animated** pegasus (one overlay phase 0–4) + **one** random peeker |
| **TitleMenu** | `intro.32` + **frozen** overlays (frames 0, 2, 3, 4) + book/menu UI |
| **CharacterUi** | Character sheet backend |

### Pegasus animation (attract)

- **`overlay_phase_`:** 0..4, advances every **`kOverlayStepTicks` (20)** render ticks
  (~60 Hz via `Game::delayMs(16)` → ~0.33 s per step).
- Each phase blits **one** entry of `kOverlayAnim[]` (maps to table above for frames 0–4).
- Phases 0 and 1 share **(168, 164)** (tail); the visible cycle still advances through
  leg (2), accent (3), and mouth (4) on other anchors.

### Grey peekers (attract only)

- Tree hollows **6/8/9/10** only: exactly **one** active slot (`peeker_slot_`,
  `peeker_visible_`). Inactive holes are **filled** with per-slot cover RGB (intro.32
  otherwise shows a face in every hollow at once).
- Visible **360–600** ticks (~6–10 s), off **120** ticks (~2 s), **300** ticks (~5 s)
  before the first spawn; never the same slot twice in a row (LCG `peeker_rng_`).

### Frozen composite (menu)

```text
Frame 0 @ (168, 164)  — tail
Frame 2 @ (36, 124)   — leg
Frame 3 @ (172, 156)  — accent
Frame 4 @ (264, 72)   — mouth
+ one peeker cel (last random slot)
```

---

## Timing constants (remake)

| Constant | Value | Effect @ ~60 Hz |
|----------|------:|-----------------|
| `kOverlayStepTicks` | 30 | ~0.5 s per pegasus phase |
| `kPeekerDelayMinTicks` | 120 | ~2 s min between peeker swaps |
| `kPeekerDelayMaxTicks` | 240 | ~4 s max between peeker swaps |
| `kFadeTicks` | 60 | Logo fade ~1 s |
| `kLogoHoldTicks` | 312 | Logo hold ~5.2 s |

ASM gate @ `0x26A1E`: blit every **4th** poll when gate reaches **3** (faster than
remake’s 30-tick phase step — remake pacing is intentionally slower for desktop).

---

## Validation

- **C/Python codec:** `tools/test_image32_golden.py` — `introclips.32` frames match
  between `mm2_image32_codec.c` and `tools/render_view_refs.py`.
- **Overlay fit:** coarse search minimizing RGB diff vs `intro.32` @ 320×200;
  composite reference: `game/build/intro_aligned.png`.
- **Run:** `game/build/mm2.exe <datadir>` — needs `intro.32`, `introclips.32`; warns
  if `introclips.32` missing.

---

## Related

- [`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md) — boot
  graph, logo `0x26BC4`, asset load strings, blit traces
- [`26-audio-callpaths-title-death-shared.md`](26-audio-callpaths-title-death-shared.md) —
  title music `0x12D`, `-$7C62` chirp
- [`06-gfx-loading.md`](06-gfx-loading.md) — `.32` codec
- `game/README.md` — build / run
