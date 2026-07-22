# Title Screen Animation (pegasus + peekers)

Authoritative notes for how **`intro.32`** and **`introclips.32`** compose on the
pre-game title attract scene. Asset/boot tracing lives in
[`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md).

**Remake:** `game/src/TitleScreen.cpp`  
**Display:** **320×200** NTSC (`gfx::ScreenCompositor::kWidth` × `kHeight`; 2× scale → 640×400 window).  
**Font:** `Mm2Font8x8.inc` — menu strings are plain ASCII via `drawText` / `drawTextShadow`.

---

## Summary

| Layer | File | Behavior |
|-------|------|----------|
| Background | `intro.32` | Single frame @ **(3, 0)** — 320×200 pegasus scene |
| Pegasus patches | `introclips.32` frames **0–4** | **One cel at a time**, cycling mod 5 (`kOverlayStepTicks` = 20) |
| Peekers | `introclips.32` frames **6, 8, 9, 10** only | **One** random slot visible; frame **7 is not used** (letter-hole cel) |
| Logo | `nwcp.32` | Black field + alpha fade; X = 10, Y = `(200 − height) / 2` |
| Menu | `book.32` + text | **Black background only** — no `intro.32` under the red boxes |

**Do not** use **`A4-$632E` / `A4-$6344`** @ `0x27096` as pegasus sprite anchors. Those
tables drive a **one-shot** copy-protection / music-staff blit loop (many raw Y > 200).

---

## `introclips.32` frame inventory

Decoded from retail file (11 frames, standard `.32` image chunk):

| Frame | Size | Role |
|------:|------|------|
| 0 | 17×12 | Tail patch (cel A) |
| 1 | 17×12 | Tail patch (cel B) — subtle swap vs frame 0 |
| 2 | 40×48 | Leg / body fragment |
| 3 | 10×8 | Small accent near tail |
| 4 | 48×36 | Mouth / head patch |
| 5 | 48×36 | Mouth alt — **not** in ASM mod-5 loop @ `0x26A1E` |
| 6 | 26×25 | Peeker — tree hollow (right, upper) |
| 7 | 19×17 | **Letter-hole cel — not a peeker** (was wrongly placed on tree coords) |
| 8 | 27×24 | Peeker — tree hollow (right, mid) |
| 9 | 27×24 | Peeker — tree hollow (right, lower) |
| 10 | 45×22 | Peeker — gnome / wide peek (left, behind pegasus) |

Palette index **0** = transparent (`mm2_image32_set_preview_opaque(0)` on all title `.32` loads).

---

## Pegasus overlay coordinates (320×200)

One cel at a time during **TitleAttract** (`kOverlayAnim[]`):

| Phase | Frame | X | Y | Part |
|------:|------:|--:|--:|------|
| 0 | 0 | 169 | 164 | Tail |
| 1 | 1 | 169 | 164 | Tail (alt cel) |
| 2 | 2 | 35 | 124 | Leg |
| 3 | 3 | 170 | 155 | Accent |
| 4 | 4 | 265 | 73 | Mouth |

Background: **`intro.32` frame 0 @ (3, 0)**.

ASM @ `0x26A1E` cycles the same frame index (`A4-$647A`, wrap @ 5) but blits to
corners **(8, 8)** and **(8, 216)** on the logo/copy-protection path — not these
per-part anchors. The remake uses the table above for attract animation.

---

## Peeker slots (TitleAttract only)

**Correction (2026-07-17):** `intro.32` does **not** bake face-like pixels into
the hollows — the art behind every hole is already correct scenery (bark,
grass, wing). Only **one** peeker cel should be visible at a time, and no
cover-fill is needed or used:

1. Re-blit **`intro.32` @ (3, 0)** (restores plain scenery everywhere).
2. Leave inactive peeker slots **undrawn** — the re-blitted background already
   shows through correctly. (An earlier note called for a per-slot RGB
   "cover-fill" to mask a baked face; that would incorrectly paint over the
   real bark/grass/wing art and has never matched the remake.)
3. Blit **one** active `introclips` cel on top.

Remake: `game/src/TitleScreen.cpp` `kPeekerSlots` (see its comment for the
byte-level justification).

| Slot | Frame | X | Y | Location |
|------|------:|--:|--:|----------|
| 0 | 6 | 250 | 106 | Tree hollow (right, upper) |
| 1 | 8 | 246 | 128 | Tree hollow (right, mid) |
| 2 | 9 | 246 | 130 | Tree hollow (right, lower) |
| 3 | 10 | 95 | 96 | Gnome / left peek |

**Frame 7 is excluded** — it is the letter-hole cel, not a tree peeker.

### Timing (~60 Hz via `delayMs(16)`)

| Constant | Ticks | ~Seconds | Effect |
|----------|------:|---------:|--------|
| `kPeekerInitialGapTicks` | 300 | ~5 s | Delay before first peeker |
| `kPeekerDelayMinTicks` | 360 | ~6 s | Min visible duration |
| `kPeekerDelayMaxTicks` | 600 | ~10 s | Max visible duration |
| `kPeekerGapTicks` | 120 | ~2 s | Off gap between appearances |
| `kOverlayStepTicks` | 20 | ~0.33 s | Pegasus phase step |

Random slot pick uses LCG `peeker_rng_`; never repeats the same slot twice in a row.

Peekers run only in **TitleAttract**. **TitleMenu** does not draw peekers or `intro.32`.

---

## Remake state machine (`TitleScreen.cpp`)

| State | Layers |
|-------|--------|
| **LogoFadeIn / Hold / FadeOut** | Black + **`nwcp.32`** (alpha fade, Y centered on 200 px) |
| **TitleAttract** | `intro.32` + animated pegasus (one phase 0–4) + one peeker |
| **TitleMenu** | **Black** + red-bordered boxes + **`book.32`** flanking title + menu text |
| **CharacterUi** | Character sheet backend (`AmigaCharacterUi`) |

### TitleMenu layout

- Top box (304×98 @ 8,3): open-book art + centered **MIGHT / and / MAGIC / Book Two** + tagline.
- Bottom box (304×93 @ 8,104): **C / V / G / M / O / Q** menu lines (greyed when `roster.dat` missing).
- `book.32` page-turn: one frame step every **`kBookStepTicks` (8)** ticks.
- Any key from attract opens the menu (edge-triggered `keys.any_key`). **Escape
  is not a dedicated "back to attract" key in `menuTick()`** — the menu stays up
  until **C/V/G/M/O** is pressed or **Q** quits; only the Controls/Options
  sub-panels pop back to the menu on Escape. (Remake: `TitleScreen.cpp::menuTick`.)

---

## Amiga ASM reference (two introclips blit sites)

### `JSR -$7C20(A4)` — `blit(sheet, frame, y, x)`

Push order confirmed @ `0x260B8`: first pushed = **x**, second = **y**.

### A) `0x26A1E` — mod-5 overlay cycle (`A4-$7A2E`)

Logo / copy-protection hold only. Gate `A4-$6478` ≥ 3; frame `A4-$647A` wraps @ 5.
Blits same frame to **(8, 8)** and **(8, 216)**. **`0x1062` menu** does not call this.

### B) `0x27096` — table loop (`A4-$7A32`)

Copy-protection / create-prompt one-shot; frames 0..10 @ x = 8, y = `Ytable[i] − 1`.
**Not** pegasus part placement — see doc 38.

### C) `0x1062` / `0xF1C` — menu hotkeys

On original hardware: menu text drawn on **retained** `intro.32` framebuffer (last
`0x26A1E` corner pixels frozen). Remake uses explicit black fill instead.

---

## Validation

- **Codec round-trip:** `tools/test_image32_golden.py` — `introclips.32` vs `mm2_image32_codec.c`.
- **Overlay fit:** RGB diff search vs `intro.32` @ 320×200; refs under `game/build/`.
- **Run:** `game/build/mm2.exe <datadir>` — needs `intro.32`, `introclips.32`, `nwcp.32`, `book.32`.

---

## Related

- [`38-title-screen-and-intro-assets.md`](38-title-screen-and-intro-assets.md) — boot graph, logo `0x26BC4`, asset strings
- [`39-character-ui-view-create.md`](39-character-ui-view-create.md) — **P** / **C** / character sheet
- [`58-amiga-audio-in-exe.md`](58-amiga-audio-in-exe.md) — title theme is DATA `0x1D79` / overlay `0x283FC`, cooperative pump in the remake ([`26-audio-callpaths-title-death-shared.md`](26-audio-callpaths-title-death-shared.md)'s "title music `0x12D`" claim is **superseded/wrong** — that address is a graphics blit, not audio)
- [`06-gfx-loading.md`](06-gfx-loading.md) — `.32` codec
- [`game/README.md`](../../game/README.md) — build / run
