# Title Screen and Intro Assets

> **Pegasus overlay animation (coords, timing, remake):** see
> [`39-title-screen-animation.md`](39-title-screen-animation.md). The
> **`A4-$632E` / `A4-$6344`** tables @ `0x27096` are **music-note staff positions**,
> not per-cel pegasus anchors. Remake display is **320×200**.

## Introclips — verified blit trace (ASM-only, June 2026 re-audit)

> This section supersedes earlier prose where it disagrees. Every coordinate and
> frame index below is quoted from a specific instruction in
> `EXTRACTED/mm2.capstone.annotated.asm` or dumped from
> `EXTRACTED/ghidra/mm2_data_00.bin` (the DATA hunk, `A4 = data_base + 0x7FFE`).

### 1. `introclips.32` frame table (decoded from the retail file)

Header: `u16 frame_count = 11`, `u16 depth_or_mode = 3`, then per-frame
`{u16 w, u16 h, u16 flags}` (standard `.32` image-chunk codec). Verified by
parsing the real `introclips.32` (6023 bytes):

| Frame | W × H  | Frame | W × H  |
|-------|--------|-------|--------|
| 0     | 17×12  | 6     | 26×25  |
| 1     | 17×12  | 7     | 19×17  |
| 2     | 40×48  | 8     | 27×24  |
| 3     | 10×8   | 9     | 27×24  |
| 4     | 48×36  | 10    | 45×22  |
| 5     | 48×36  |       |        |

### 2. Blit routine `JSR -$7C20(A4)` — argument order (CONFIRMED)

Args are C-style, pushed right-to-left on the stack. Reading the lowest stack
slot first, the signature is:

```
blit(long srcPtr, word frame, word y, word x)
        p1           p2        p3      p4
```

`p3 = y`, `p4 = x` — confirmed three independent ways:

- **`intro.32` @ `0x260B8`** pushes `#3`(p4), `#0`(p3), `frame=#0`(p2), `ptr`(p1)
  → drawn at **x=3, y=0** (matches the known title-art offset).

```
0260b8  move.w  #$3, -(a7)         ; p4 = x = 3
0260bc  clr.w   -(a7)              ; p3 = y = 0
0260be  clr.w   -(a7)              ; p2 = frame = 0
0260c0  move.l  -$77ae(a4), -(a7)  ; p1 = intro.32 ptr
0260c4  jsr     -$7c20(a4)
```

- The attract loop (§4) pushes `Xtable[i]+8` into **p4** and `Ytable[i]-1` into
  **p3**, i.e. the X table feeds p4 and the Y table feeds p3.

### 3. Corner-clip loop `0x26A1E` (handle `A4-$7A2E`, frames 0–4)

This is the loop that draws the two clip sprites. It is reached **only** from
`0x26BC4` (logo / copy-protection hold) via `0x26E64 → 0x26B0C → 0x26B14 →
0x26A10/0x26A1E`. It is **never** reached from the interactive menu (§5).

```
026a1e  cmpi.w  #$3, -$6478(a4)    ; gate counter
026a24  blt.w   $26aaa             ;  < 3 -> just bump gate (no blit)
026a28  clr.w   -$6478(a4)         ;  >=3 -> reset gate, then blit:
; --- TOP clip ---
026a36  move.w  #$8, -(a7)         ; p4 = x = 8
026a3a  move.w  #$8, -(a7)         ; p3 = y = 8
026a3e  move.w  -$647a(a4), -(a7)  ; p2 = frame index
026a42  move.l  -$7a2e(a4), -(a7)  ; p1 = introclips ptr (handle #1)
026a46  jsr     -$7c20(a4)         ; blit TOP @ (x=8, y=8)
; --- BOTTOM clip (same frame) ---
026a4e  move.w  #$8, -(a7)         ; p4 = x = 8
026a52  move.w  #$d8, -(a7)        ; p3 = y = 0xD8 = 216
026a56  move.w  -$647a(a4), d0     ; read frame (still same value)
026a5a  addq.w  #$1, -$647a(a4)    ; frame++  (advances once per iteration)
026a5e  move.w  d0, -(a7)          ; p2 = frame
026a60  move.l  -$7a2e(a4), -(a7)  ; p1 = introclips ptr
026a64  jsr     -$7c20(a4)         ; blit BOTTOM @ (x=8, y=216)
...
026a94  cmpi.w  #$5, -$647a(a4)    ; frame wrap
026a9a  bne.b   $26aa0
026a9c  clr.w   -$647a(a4)         ;  -> frames cycle 0,1,2,3,4
...
026aaa  addq.w  #$1, -$6478(a4)    ; gate++ (the <3 path)
```

**Verified corner coordinates:** TOP **(x=8, y=8)**, BOTTOM **(x=8, y=216)**.
Both corners show the **same** frame each pass; the frame index `-$647A`
advances by 1 per pass and **wraps at 5** (frames **0–4** only). Blit fires only
when gate `-$6478` ≥ 3 (i.e. every 4th poll tick), then resets.

These (8,8)/(8,216) values are **not invented** — they are the literal
`move.w #$8` / `move.w #$d8` immediates at `0x26a36`–`0x26a52`. What *was* wrong
in the remake was the **state placement** (see §6).

### 4. Attract loop `0x27096` (handle `A4-$7A32`, frames 0–10, table-driven)

Separate routine. Callers are copy-protection / alt-boot branches only:
`0x27F3A`, `0x2808C`, `0x28124`. **Not** the interactive menu.

```
0270a8  ; loop body, i = -$2(a5), runs i = 0..10  (blt #$b @ 0x27144)
0270d4  lea.l   -$632e(a4), a0     ; X table
0270d8  move.w  (a0,d0.l), d1      ; d1 = Xtable[i]
0270dc  addq.w  #$8, d1            ; X = Xtable[i] + 8
0270de  move.w  d1, -(a7)          ; p4 = x
0270e8  lea.l   -$6344(a4), a0     ; Y table
0270ec  move.w  (a0,d0.l), d1      ; d1 = Ytable[i]
0270f0  subq.w  #$1, d1            ; Y = Ytable[i] - 1
0270f2  move.w  d1, -(a7)          ; p3 = y
0270f4  move.w  -$2(a5), -(a7)     ; p2 = frame = i
0270f8  move.l  -$7a32(a4), -(a7)  ; p1 = introclips ptr (handle #2)
0270fc  jsr     -$7c20(a4)         ; blit frame i @ (x, y)
027144  cmpi.w  #$b, -$2(a5)       ; i < 11 ?
```

**Dumped position tables** (DATA hunk, big-endian words, 11 entries each):

| `A4-$632e` (X) file off `0x1cd0` | `A4-$6344` (Y) file off `0x1cba` |
|----------------------------------|----------------------------------|
| all zeros → **x = 0 + 8 = 8**    | `8 43 61 148 195 221 248 264 264 241 229` |

The loop blits `introclips` frame **i** at **(x = 8, y = Ytable[i] − 1)** for
i = 0..10. Raw Y words: `8, 43, 61, 148, 195, 221, 248, 264, 264, 241, 229`.
Several **Y > 200** on a **320×200** field. **Do not treat this as pegasus part
placement** — use the overlay coordinate table in
[`39-title-screen-animation.md`](39-title-screen-animation.md). Likely **title-music
note / `draw_cell` staff** positions (see `0x2712E` when i = 0).

### 5. Interactive title menu `0x1062` / `0xF1C` — NO clip animation

`game_world_boot` (`0xF1C`) and `main_loop_menu_hotkeys` (`0x1062`) draw menu
text/boxes via `-$7b9c`, `-$7bfc`, `-$7c62` etc. **Neither pushes `A4-$7A2E`
nor `A4-$7A32`**, and neither calls `0x26A1E`/`0x27096`. By the time `0x1062`
runs, both introclips handles are freed (`0x26534`, `0x26338C`). The clip pixels
left on the `intro.32` playfield by the last `0x26A1E` blit simply **stay
frozen**; the menu does not cycle them.

### 6. Remake overlay model (June 2026)

See **[`39-title-screen-animation.md`](39-title-screen-animation.md)** for full detail.

- Display **320×200**; pegasus: **`intro.32` @ (3,0)** + **one** `introclips` cel at a
  time, phases **0–4** at per-part screen coords (not `A4-$632E`/`-$6344`).
- Peekers: frames **6, 8, 9, 10** only (frame **7** = letter-hole cel, not used);
  **one** random slot; inactive hollows cover-filled before active cel (see doc 39).
- **TitleMenu:** **black** + `book.32` / menu text — **no** `intro.32` under boxes
  (Amiga @ `0x1062` keeps frozen VRAM; remake clears black explicitly).
- Logo: **black + `nwcp.32`**, X = 10, Y centered on **200** px height.
- Font: `Mm2Font8x8.inc` via `ScreenCompositor`; menu strings plain ASCII.

---

Reverse-engineering notes for the pre-game title / main-menu presentation and its

dedicated `.32` tilesets. Verified against `EXTRACTED/mm2.capstone.annotated.asm`

(June 2026 ASM-only pass). The desktop remake in `game/` uses a **320×200**

compositor (2× integer scale → **640×400** window).



## Asset files



| File | Frames | Frame size | Role |

|------|--------|------------|------|

| `intro.32` | 1 | **320×200** | Title background (pegasus / Might and Magic art) |

| `introclips.32` | 11 | **17×12** (frame 0–1); larger in frames 2–4 | Title-menu corner animation sprites |

| `nwcp.32` | 1 | **300×90** | “New World Computing Presents” splash |



Embedded load strings (CODE hunk, not the DATA-hunk filename table):



| String | PC offset | Used @ |

|--------|-----------|--------|

| `intro.32` | `0x26985` | `0x25FCE`, `0x26072`, reload @ `0x260DE` |

| `introclips.32` | `0x2697A` | `0x26058` → **`A4-$7A2E`** |

| `introclips.32` | `0x2698E` | `0x260DE` → **`A4-$7A32`** (second handle) |



`nwcp.32` is also in the main resource table (index 8) for the in-game HUD panel

(`15-3d-view-and-game-screen.md`). The logo splash blits through temp **`A4-$F4C`**

sheet pointers inside **`0x26BC4`**, not the Y=166 HUD placement.



### introclips frame sizes (decoded from retail `introclips.32`)



| Frame | Size | Notes |

|-------|------|-------|

| 0–1 | 17×12 | Small corner sparkles |

| 2 | 40×48 | Pegasus leg fragment (matches `intro.32` art) |

| 3 | 10×8 | Tiny glyph |

| 4 | 48×36 | Larger art fragment |

| 5 | 48×36 | Mouth alt — not in mod-5 loop |
| 6–10 | varies | Peekers (6,8,9,10) + letter-hole frame 7; **`0x27096`** blits all 11 |



Frames 0–4 are the mod-5 loop at **`0x26A1E`**; they overlay **`intro.32`** at

fixed corners **(8, 8)** and **(8, 216)**.



## Boot call graph (verified order)



Main title boot path (function containing **`0x25FCE`** … **`0x26840`**):



```

0x25FCE   load intro.32        → A4-$77AE

0x26058   load introclips.32   → (temp, then A4-$7A2E @ 0x2606E)

0x26072   reload intro.32      → A4-$77AE

0x260C4   blit(intro, frame=0, x=3, y=0)     ; JSR -$7C20(A4)

0x260D0   free intro sheet handle            ; pixels remain in playfield

0x260DE   load introclips.32   → A4-$7A32

          … copy-protection / data init …

0x263332  tst A4-$4A4

0x263336  bne → 0x26376          ; alt path skips 0x263338 blits when flag set

0x263376  JSR 0x26BC4            ; nwcp fade + 0x26A1E clip sessions

0x26337E  free intro.32 handle (A4-$77AE)

0x26338C  free introclips handle (A4-$7A32)

          … roster/resource setup …

0x26534   free introclips.32 (A4-$7A2E)      ; handle freed; last blit pixels stay

0x26550   JSR 0x2593C (overlay_segment_init)

          … audio/resource loads …

0x26800   clr.b A4-$79E2         ; not outdoor view mode

0x26804   spin JSR -$7FEC until ready → RTS @ 0x26840

          … main scheduler @ 0x1280 …

0xF1C     game_world_boot → 0x1062 menu hotkeys (when flags allow)

```



**`game_world_boot` @ `0xF1C`** is reached from the main scheduler (`0x372` and

related paths), not from the boot fn @ `0x26840`. It assumes **`intro.32`** pixels

from **`0x260C4`** are still in the playfield (plus any **`introclips`** pixels

from the last **`0x26A1E`** blit before **`0x26534`**).



| Step | Address | What happens |

|------|---------|--------------|

| Background once | `0x260C4` | `intro.32` frame 0 @ **(3, 0)**; sheet freed @ `0x260D0`, VRAM kept |

| Boot intro | `0x26376` → **`0x26BC4`** | nwcp palette-fade **and** corner **`0x26A1E`** clip sessions over **`intro.32`** |

| Handle free | `0x26534` | **Free `A4-$7A2E`** — no further **`-$7C20`** blits from that handle |

| Menu | **`0x1062`** | C/M/O/P/Q on retained framebuffer; **no new clip blits** (corners frozen) |



## Timeline diagram (ASM-verified)



```

[0x260C4] intro.32 blit once @ (3,0) ───────────────────────────────────► stays on screen

[0x26058] introclips load → A4-$7A2E ──────────────────► [0x26534] free handle

                              │

[0x263376] 0x26BC4 outer loop (-$55DA = 0,1,2 → exit at 3)

    each iteration:

      copy-prot display

      [0x26E64] → 0x26B0C → 0x26A1E  ◄── CORNER CLIPS ANIMATE HERE (poll loop)

      nwcp blit via A4-$F4C

      phase delay / palette fade (blocking — NO 0x26A1E during waits)

[0x27062] 0x26BC4 returns — one menu line @ y=0xC

[0x26337E/0x26338C] free intro + second introclips handles

[0x26534] free A4-$7A2E (first introclips handle)

[0x26804] boot spin until ready

[0xF1C] game_world_boot — title music, menu text

[0x1062] hotkey idle — NO 0x26A1E, NO A4-$7A2E blits

[0x1280] main_scheduler_loop — NO introclips refs

```



## Logo intro @ `0x26BC4`



Entry: **`0x26376`** `JSR 0x26BC4`.



| A4 field | Role |

|----------|------|

| **`-$55DA`** | Phase gate: `cmpi #3` @ `0x26D2E`; when **≥ 3**, exit @ **`0x27062`** |

| **`-$6476`** | Word table (51 copy-prot records; also palette entries during fade) |

| **`-$77B4` / `-$77BC`** | Nested-loop palette fade index / 8-byte table @ **`0x26ABC`** |

| **`-$F4C`** | nwcp splash bitmap pointer; blit @ **`0x26E78`**, **`0x26F4E`** via **`-$7BE4`** |

| **`-$7A28`** | Hold-phase counter copied into **`-$55DA`** @ **`0x26F02`** |

| **`-$79EC`** | Fade-out counter copied into **`-$55DA`** @ **`0x27054`** |



Phase sketch ( **`-$55DA` = 0, 1, 2** then exit):



1. **Fade in** — palette steps from **`-$6476`** / nested **`-$77BC`** table; nwcp

   blit through **`-$F4C`**; **`-$55DA < 2`** branch @ **`0x26F1A`** uses alternate

   fade blit @ **`0x26F4E`**.

2. **Hold / copy-prot** — each outer iteration calls **`0x26E64` → `0x26B0C` →

   **`0x26A1E`** (corner clip blits + palette tick @ **`0x26ABC`**) while the

   user types the protection string; on match, delay **`0x32`** (50) frames @

   **`0x26EF8`**; set **`-$55DA`** from **`-$7A28`** @ **`0x26F02`**.

3. **Fade out** — when **`-$55DA ≥ 2`**, multi-blit palette restore @

   **`0x26F5C`–`0x27048`** (blocking — **no `0x26A1E`** during this wait); wait

   **`0x28208`** @ **`0x2704C`**; set **`-$55DA`** from **`-$79EC`** @

   **`0x27054`**.



**Clip blits are not limited to nwcp fade ticks.** They run inside the

**`0x26B0C`** input loop ( **`0x26B14` → `0x26A10`/`0x26A1E`** ) at the start

of each **`-$55DA`** phase iteration @ **`0x26E64`**, with **`intro.32`** already

on screen from **`0x260C4`**. Blocking palette waits @ **`0x26EF8`** /

**`0x2704C`** do **not** call **`0x26A1E`**.



Return @ **`0x27062`**: free splash alloc (**`-$A(A5)`**), draw one menu line

(font **0x11**, **y=0xC** @ **`0x27076`**) — full C/M/O/P/Q strings come from

**`game_world_boot`/`0x1062`**, not here.



Fade mechanism is **palette remapping**, not per-pixel alpha. The C port approximates

nwcp visibility with global alpha on a black field.



## Clip loop @ `0x26A1E` (inside `0x26BC4` only)



**Only caller chain (verified — no other `A4-$6478` / `A4-$647A` refs in binary):**

**`0x26BC4`** @ **`0x26E64`** → **`0x26B0C`** → **`0x26B14`** → **`0x26A10`/`0x26A1E`**.

Inner loop @ **`0x26AF0`** until **`-$7BD2`** reports a key @ **`0x26AF4`**.



| A4 offset | Role |

|-----------|------|

| **`-$6478`** | Gate — `addq #1` @ **`0x26AAA`**; blit when **≥ 3** |

| **`-$647A`** | Frame index; wrap @ **5** (`0x26A94`–`0x26A9C`) |

| **`-$7A2E`** | `introclips.32` pointer (loaded @ **`0x2606E`**, freed @ **`0x26534`**) |

| **`-$77B4`/`-$77BC`** | Palette cycle table stepped @ **`0x26ABC`** each inner-loop tick |



Blit args ( **`JSR -$7C20(A4)`** @ **`0x26A46`** top, **`0x26A64`** bottom):



- Top: **x=8, y=8**, frame = **`-$647A`**

- Bottom: **x=8, y=0xD8 (216)**, same frame index (increment after top blit @ **`0x26A5A`**)



Song **`0x137`** on channel **`0x4F`** @ **`0x26A6C`–`0x26A7A`**. **`draw_cell`**

@ **`-$7C38`** @ **`0x26A8C`**: x=8, y=8, frame=0, mode=1.



**After `0x26BC4` returns and `0x26534` frees the handle, `0x1062` / `0x1280` never

call `0x26A1E` or blit from `A4-$7A2E`.** Corner pixels from the last blit remain

on the **`intro.32`** playfield until overwritten (frozen overlay, not cycling).



## Table-driven loop @ `0x27096` (music / copy-protection — NOT pegasus anchors)

One-shot loop during copy-protection / create-prompt paths. Uses **`A4-$7A32`**
and the word tables at **`A4-$632E` / `A4-$6344`**. **Not** the idle pegasus wag on
`intro.32` — see [`39-title-screen-animation.md`](39-title-screen-animation.md).

Callers:

| Address | Context |
|---------|---------|
| `0x27F3A` | Copy-protection input routine ("Class=/Race=/Alignment=") |
| `0x2808C` | Copy-protection input — space key |
| `0x28124` | "(C)reate New Char…" prompt path (after `0x28716`/proto setup) |

Uses **`A4-$7A32`** (second introclips load @ `0x260DE`/`0x260F6`, freed @
`0x2638C`) with position tables **`A4-$632E`** (X, `+8` @ `0x270DC`) and
**`A4-$6344`** (Y, `−1` @ `0x270F0`).

### Blit signature (proven, `JSR -$7C20(A4)`)

From the `0x26A1E` corner pair: top blit pushes `8,8`; bottom blit pushes
`8,0xd8` (`0x26A4E`–`0x26A52`). "Bottom" = larger Y, and only the **second**
pushed word changes (8 → 0xd8). So push order is **X, Y, frame, sheet** and the
callee reads `blit(sheet, frame, Y, X)`. In `0x27096`:

```
0270d4  lea.l   -$632e(a4), a0     ; X table base
0270d8  move.w  (a0, d0.l), d1     ; d0 = i*2
0270dc  addq.w  #$8, d1            ; X = X_table[i] + 8
0270de  move.w  d1, -(a7)          ; push X  (first → arg4)
0270e8  lea.l   -$6344(a4), a0     ; Y table base
0270ec  move.w  (a0, d0.l), d1
0270f0  subq.w  #$1, d1            ; Y = Y_table[i] - 1
0270f2  move.w  d1, -(a7)          ; push Y  (second → arg3)
0270f4  move.w  -$2(a5), -(a7)     ; push frame = i
0270f8  move.l  -$7a32(a4), -(a7)  ; push sheet (introclips handle)
0270fc  jsr     -$7c20(a4)         ; blit(sheet, frame=i, Y, X)
...
027140  addq.w  #$1, -$2(a5)       ; i++
027144  cmpi.w  #$b, -$2(a5)       ; i < 11
02714a  blt.w   $270a8             ; loop frames 0..10
```

### Dumped tables (data hunk `EXTRACTED/ghidra/mm2_data_00.bin`, `file_off = 0x7FFE − disp`, big-endian words)

`A4-$632E` X table @ file `0x1CD0` — **all `0x0000` for i=0..10** ⇒ X = `0 + 8` = **8** (constant).

`A4-$6344` Y table @ file `0x1CBA` (the two tables are contiguous: 11 Y-words then 11 X-words):

| frame i | raw word | Y = raw − 1 | X = 0 + 8 |
|--------:|---------:|------------:|----------:|
| 0 | `0x0008` | **7**   | 8 |
| 1 | `0x002B` | **42**  | 8 |
| 2 | `0x003D` | **60**  | 8 |
| 3 | `0x0094` | **147** | 8 |
| 4 | `0x00C3` | **194** | 8 |
| 5 | `0x00DD` | **220** | 8 |
| 6 | `0x00F8` | **247** | 8 |
| 7 | `0x0108` | **263** | 8 |
| 8 | `0x0108` | **263** | 8 |
| 9 | `0x00F1` | **240** | 8 |
| 10 | `0x00E5` | **228** | 8 |

Per-step delay @ `0x27136`: `push #$4b → JSR -$7BC0` → `wait(1)` per index.

**Two introclips blit sites** (whole binary): `0x26A1E` (`A4-$7A2E`, corners @
logo hold) and `0x270FC` (`A4-$7A32`, table loop above). Idle menu **`0x1062`**
calls neither.



## Title menu @ `0x1062`



**`main_loop_menu_hotkeys`** — reached from **`game_world_boot` @ `0xF1C`** when

**`A4-$79E4`** / **`A4-$7950`** allow (`0xFBC` / **`0xFC4`** → **`0x1062`**).



| Key | ASCII | Dispatch |

|-----|-------|----------|

| **C** | `$43` | Create character → **`0x1288`** |

| **M** | `$4D` | Controls |

| **O** | `$4F` | Options |

| **P** | `$50` | View party |

| **Q** | `$51` | Quit |



**`A4-$79F2`** set to **`$FF`** @ **`0xF7A`**. Idle menu: retained **`intro.32`**

framebuffer (with frozen corner clip pixels from last **`0x26A1E`** blit) + menu

text — **no** ongoing **`0x26A1E`** loop, **no** nwcp compositor.



Idle wait: **`0x1230`** polls **`-$7BD2`**; falls through to **`0x1258`** →

**`0x1280`** scheduler — neither path references **`A4-$7A2E`**, **`-$6478`**, or

**`-$647A`**.



## Remake state machine (`game/TitleScreen.cpp`)

Documented in **[`39-title-screen-animation.md`](39-title-screen-animation.md)**.

Run: `game/build/mm2.exe <datadir>`.



## Open questions



- ~~Exact contents of **`A4-$632E` / `-$6344`** word tables for **`0x27096`** (11 entries).~~
  **Resolved** — dumped above (X all 0 → constant 8; Y = `{7,42,60,147,194,220,247,263,263,240,228}`).

- Initial values loaded into **`A4-$7A28`** (hold) and **`A4-$79EC`** (fade-out) before

  the **`-$55DA`** phase loop.

- Precise menu string positions on the static screen (partial trace @ **`0x27076`**;

  **`0x103C`** draws one line @ y=**0x26** in another boot branch).

- Whether copy-protection **`0x26BC4`** typing UI must precede the visible nwcp fade

  on original hardware (same routine).



## Related docs

- [`39-title-screen-animation.md`](39-title-screen-animation.md) — pegasus overlay coords, timing, peekers
- `06-gfx-loading.md` — general `.32` loader path
- `15-3d-view-and-game-screen.md` — in-game `nwcp.32` HUD
- `20-copy-protection-table.md` — **`A4-$6476`** word table shared with logo fade
- `26-audio-callpaths-title-death-shared.md` — title music / ambience


