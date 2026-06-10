# Event Text Rendering — Pixel-Exact Draw Paths (OP_01..OP_0B, prompts)

Full ASM trace of how `event.dat` text opcodes reach the screen: every draw
thunk resolved to its routine, the text-window kernel, exact destination
rects/cursor origins/line metrics, prompt loops, and restore behaviour.
Source: `EXTRACTED/mm2.capstone.annotated.asm` + data hunk
`EXTRACTED/hunks/mm2_data_00.bin` (A4 = data_base + `$7FFE`; thunk stubs are
6-byte `JMP $xxxxxxxx` at data offset `$7FFE + disp`).

Companions: [07-event-script-opcodes.md](07-event-script-opcodes.md) (opcode
semantics), [30-event-to-string-path.md](30-event-to-string-path.md) (string
sources), [15-3d-view-and-game-screen.md](15-3d-view-and-game-screen.md)
(viewport composition).

---

## 1. Coordinate model (verified)

- **Engine pixel space is 320×192** (`A4-$6180` = 320, `A4-$617E` = 192,
  read by the cell clamp @ `0x21524`). Every low-level primitive adds **+8 to
  y** right before calling graphics.library — verified in:
  - `Move` for text: `0x221DC`/`0x2387A` move to `y*8 + $E` (= `y*8 + 8` cell
    top + baseline 6),
  - h-line fill (mode 4 @ `0x22D68`): `addq #8` to y,
  - scroll (mode `$0E` @ `0x232D8`): `addq #8` to both y args before
    `ScrollRaster`,
  - blit (mode `$13` @ `0x2367C`): `addq.w #8, y`,
  - save-rect (mode `$0B` @ `0x2303A`): `addq #8` to both y args.

  So the engine's `(x, y)` maps to **physical raster `(x, y+8)`** on the
  320×200 playfield; physical rows 0–7 are outside every drawing path.
  **All coordinates below are engine-space** — the same space as the doc-15
  viewport/blit tables (3D viewport (8,8)–(215,127) reappears verbatim in the
  event cleanup restore blit, §7).

- **Text cell grid: 40×24 cells of 8×8 px.** Cell `(col,row)` = engine pixels
  `(col*8, row*8)`..`(col*8+7, row*8+7)`; cols clamped 0..39, rows 0..23
  (`0x21524`). Glyph baseline = cell top + 6.

- **Double buffer:** rastports `A4-$480` / `A4-$3EC`; active rastport
  `A4-$47C` selected by primitive mode 1 (`-$7C3E(n)` @ `0x21EAE`).
  graphics.library base at `A4-$48C`.

- **Root pixel window** `A4-$7A3E` = `(0,0,319,191)` created at startup
  `0x25F72` via `-$7C7A` → `0x2145E`; held in `A4-$619C`; pixel coords are
  clip-translated against it by `0x21728` (adds origin, clamps to x2/y2).

- **Screen text window** `A4-$7A46` = cells `(0,0)-(39,23)` — created at
  startup `0x25F5C` via `-$7C74(0,0,$27,$17)` and opened with `-$7C56` →
  becomes the current text window `A4-$6198`. **All event text draws into
  this full-screen 40×24 window unless the op opens a popup window.**

## 2. Text-window kernel (window records + primitives)

### 2.1 Window record (20 bytes, pool of 3 @ `A4-$392`, in-use flags @ `A4-$387 + i*$14`)

| Off | Field | Notes |
|-----|-------|-------|
| +0/+1 | `x1`,`y1` (cells) | inclusive top-left |
| +2/+3 | `x2`,`y2` (cells) | inclusive bottom-right (normalized/clamped @ `0x215B0`/`0x21524`) |
| +4/+5 | cursor col/row | **window-relative**, 0-based |
| +6 | text pen (A-pen) | default **1**; set by `-$7C14` |
| +7 | background pen (B-pen) | `$FF` = **transparent (JAM1)**; else SetBPen + JAM2; set by `-$7C0E` |
| +8 | open flags | `&$7F == 1` → save-under on open / restore on close; `>= $80` → **skip auto-clear** on open |
| +$A | bits 0-1 justify (0 left / 2 **center**, `-$7C02`); bit 3 inverse video (SetDrMd 5, `0x220BE`) |
| +$B | in-use / depth | set on open (`1 + #open windows`) |
| +$10 | save-under buffer ptr | from mode `$0B` save |
| +$12 | (root window) default A-pen cache |
| +$13 | depth count | |

### 2.2 Thunk → routine map (text path)

| Thunk (A4) | Routine | Name (this doc) | Behaviour |
|------------|---------|------------------|-----------|
| `-$7C74` | `0x21624` | `win_define(x1,y1,x2,y2)` | alloc record from 3-slot pool, cells clamped 0..39/0..23; returns ptr (0 = pool full) |
| `-$7C56` | `0x21B9A` | `win_open(ptr)` | `-$6198 = ptr`; save-under iff `flags&$7F==1` (`0x21A24` → mode `$0B`, ptr → `+$10`; frees gfx mem via `-$7FE6` if < `(w*h*5)>>10 + 10` KB free); auto-clear interior unless `flags>=$80` (putchar `$100`) |
| `-$7C50` | `0x21C1E` | `win_close(ptr)` | restore save-under iff `flags&$7F==1` (`0x21AFC` → mode `$0C`); free slot; current ← deepest remaining open window |
| `-$7C62` | `0x218EA` | `win_putchar(ch)` | see §2.3 |
| `-$7BE4` | `0x22376` | `win_print(str)` | per line (to `0x0A`/NUL): if justify==2 → cursor col = `((x2-x1) - (len-1))/2`; SetAPen(win+6) on active rp; `Move(rp, (x1+col)*8, (y1+row)*8 + $E)`; **`Text(rp,str,len)`** (graphics.library); cursor col += len, col wrap → col 0 row+1; `0x0A` → putchar(`0x0A`) |
| `-$7BFC` | `0x22108` | `win_set_cursor(col,row)` | window-relative; clamped to 39/23 |
| `-$7F62` | `0x42DC` | `win_clear_cells(x1,y1,x2,y2)` | cell rect **relative to current window**, filled with the window's B-pen via h-line loop `0x217BA` (pixel rect `x1*8..x2*8+7`, `y1*8..y2*8+7`) |
| `-$7C14` | `0x22076` | `win_set_text_pen(p)` | win+6 |
| `-$7C0E` | `0x22088` | `win_set_bg_pen(p)` | win+7 + SetBPen on both rastports; `$FF` → JAM1 transparent at draw time |
| `-$7C02` | `0x220E2` | `win_set_justify(m)` | win+$A bits 0-1 (only m ≤ 2) |
| `-$7C1A` | `0x2206E` | no-op | empty function |
| `-$7EC0` | `0x54F2` | `msg_show_centered(str)` | OP_01 composite, §3.1 |
| `-$7F86` | `0x4032` | `chrome_divider(x1,x2,y)` | glyph 6 at `(x1,y)`, glyph 5 × `(x2-x1-1)`, glyph 7 at `(x2,y)` |
| `-$7F5C` | `0x43A8` | `chrome_msg_top()` | glyph `$0B` at (0,18) and (39,18); glyph 5 at (12,16),(21,16),(31,16); pen 2 then back to 1 |
| `-$7ED8` | `0x5312` | `clear_rect_preset(i)` | preset cell rects, §2.4 |
| `-$7E54` | `0x6D86` | `prompt_space()` | print `"('Space' to continue)"` (exe string @ `0x1FC`, ptr `A4-$7870`) at **(9,23)** |
| (sibling) | `0x6DA6` | `prompt_esc()` | `"('ESC' to go back)"` (@ `0x1E8`, ptr `A4-$7874`) at **(11,23)** |
| `-$7F4A` | `0x44E0` | `row23_separator()` | pen 2; cursor (2,23); glyph 5 × **36** (cols 2..37); pen 1 — restores the row-23 rule after prompts |
| `-$7FBC` | `0x3266` | `sign_sprite_place(a,b,c)` | mode `$17` (`0x23C8C`); `(-1,0,0)` clears handle `A4-$79FE` |
| `-$7FC2` | `0x316E` | `sign_sprite_load(id)` | OP_0B service signboard, §3.8 |
| `-$7BD2` | `0x22586` | `key_read_console()` | IDCMP GetMsg/ReplyMsg RAWKEY translate (outdoor path) |
| `-$7D0A` | `0x1E9CE` | `key_read_3d()` | animates viewport torches (`-$7C20` blits from `-$7A1E`, tables `-$4F62/-$4F76/-$4F8A`, phase `-$667A` 0..2) then engine key call #8 (`-$7E84(8)` → `0x6798`) |
| `-$7DDC` | `0x97A2` | `key_read_scripted()` | replays input buffer `A4-$119A` (idx `-$1110`, repeat `-$71D6`, delays `-$71DA/-$71D8`); `-$71DC == $FD` → wrap at `$FF` |
| `-$7BAE` | `0x22C24` | gfx primitive dispatcher | §2.5 |
| `-$7C3E` | `0x21EAE` | `buf_select(n)` | active rastport ← buffer n (0/1), `-$618E = n` |
| `-$7C38` | `0x21ED8` | `buf_copy_rect(dst,src,x1,y1,x2,y2)` | clipped buffer-to-buffer blit |
| `-$7C20` | `0x2203C` | `blit(sheet,frame,x,y)` | clip via `0x21728` then mode `$13` (doc 15's blit) |
| `-$7B84` | `0x24022` | `strlen(p)` | |
| `-$7B78` | `0x2408E` | `toupper(c)` | |

Global pens (data hunk): **`A4-$7A50` = 8** (sign border), **`A4-$7A51` = 2**
(chrome/dividers), **`A4-$7A52` = 1** (normal text).
Amiga wrappers (gfxbase `A4-$48C`): `-$7AC4` SetAPen, `0x257F8` SetBPen,
`-$7ABE` SetDrMd, `-$7ACA` Move, `-$7AB2` Text, `0x257CC` ScrollRaster,
`0x25818` SetFont.

### 2.3 `win_putchar` (`0x218EA`) — the glyph engine

- `ch == $100`: **clear window** — SetAPen(win bg pen +7), fill cell rect
  `(x1,y1)-(x2,y2)` (mode-4 h-lines), cursor ← (0,0), restore A-pen from root
  win `+$12`.
- `ch == $0D`: cursor col ← 0. `ch == $0A`: col ← 0, row +1.
- else: draw glyph via mode `$15` (`0x2387A`): SetDrMd (5 if inverse bit else
  1), SetBPen cache `-$356` ← win+7 (skip pen write when `$FF`, instead
  SetDrMd(active rp, **JAM1**)), SetAPen win+6 if ≠ root default, `Move(rp,
  (x1+col)*8, (y1+row)*8+$E)`, `Text(rp, &ch, 1)`. If auto-advance flag
  `A4-$6186` (default 1): col +1; **col > x2-x1 → col 0, row +1**; **row >
  y2-y1 → scroll window up 8 px** (`0x21822`: mode `$0E` ScrollRaster dy −8 on
  the window pixel rect + clear bottom 8 lines with bg pen) and row −1.

So: wrap at the window's right edge to **column 0** (not the text's start
column), line height fixed **8 px**, overflow scrolls the window interior.

### 2.4 Clear-rect presets (`0x5312`, tables `-$745C/-$7453/-$744A/-$7441`)

Cell rects (x1,y1)-(x2,y2), window-relative (full screen in play):

| # | Rect | Use |
|---|------|-----|
| 0 | (1,19)-(38,22) | message block / roster body |
| 1 | (1,20)-(38,22) | |
| 2 | (1,17)-(38,22) | **OP_03 prep** (`-$7ED8(2)`) |
| 3 | (1,1)-(38,22) | |
| 4 | (0,0)-(39,23) | full screen |
| 5 | (1,11)-(38,22) | |
| 6 | (1,1)-(38,9) | |
| 7 | (15,17)-(38,22) | |
| 8 | (1,1)-(26,15) | **3D viewport** = px (8,8)-(215,127) ✔ doc 15 |

### 2.5 Primitive dispatcher `-$7BAE` (`0x22C24`) modes

| Mode | Routine | Action |
|------|---------|--------|
| 1 | `0x22CCC` | select active rastport (buffer 0/1) |
| 2 | `0x22CF0` | SetAPen both buffers + SetDrMd JAM2 |
| 3 | `0x22D40` | WritePixel (y+8) |
| 4 | `0x22D68` | horizontal line `(x1,y,x2)` (y+8, Move+Draw) |
| `$B` | `0x2303A` | save pixel rect to alloc'd buffer (y+8) |
| `$C` | `0x231F6` | restore saved rect |
| `$E` | `0x232D8` | ScrollRaster rect, dy −n (y+8) |
| `$13` | `0x2367C` | blit `.32` sheet frame (y+8) — doc 15 |
| `$15` | `0x2387A` | glyph at window cursor (§2.3) |
| `$16` | `0x23A18` | sprite/anm prep (OP_0B path) |
| `$17` | `0x23C8C` | overlay sprite place (service sign) |

---

## 3. Per-opcode destination rects and draw paths

All ops first resolve the `u8` string index (`0x15884`) and copy to
`A4-$5D3C` with `@`→`0x0A` (`0x158C8`). The current window is the full-screen
40×24 window, so window-relative cells = absolute cells.

### 3.1 `OP_01` `0x15924` — single centered message line (row 17)

Sets exit bit 0 → `-$7EC0(&buf)` = `0x54F2`:

1. pen 2; `chrome_divider(0, $27, $12)` → row 18 = glyph 6, 5×38, 7 (cols 0..39).
2. if `-$79E5 == 0` (not modal): glyph 5 "ticks" at cells (12,16) (21,16)
   (31,16) (12,18) (21,18) (31,18).
3. pen 1; `win_clear_cells(1,17,38,17)` → clear **row 17 cols 1..38** to black.
4. justify center; cursor (0,17); `win_print` → text centered on **row 17**
   (cols `(39-(len-1))/2` ..), px y **136..143**; justify back to 0.

One line; an over-long string (>40 visible) wraps to col 0 row 18 by the
putchar rule (no clipping to the cleared band).

### 3.2 `OP_02` `0x15942` — message block (rows 19..22)

Dispatch stub pushes base row **`$13`** (19). Sets exit bit 1.

1. `win_clear_cells(1, base, 38, 22)` → clears **cols 1..38, rows base..22**.
2. For each line i (until NUL/`$FF`): cursor **(1, base+i)**; putchar each
   char up to `0x0A`/NUL (auto-advance, wrap col>39 → col 0 next row).

With base 19: **4 rows, 19..22** (px y 152..183), text starts at **col 1**,
left-justified, line height 8 px. Lines beyond row 23 pile on row 23 (cursor
clamp) — long texts are normally paged with `OP_07` between `OP_02`s.

### 3.3 `OP_03` `0x159CE` — taller message block (rows 17..22)

Sets exit bits 0+1; `chrome_msg_top()` (`-$7F5C`): glyph `$0B` junctions at
(0,18) and (39,18), glyph-5 ticks row 16; `clear_rect_preset(2)` → clears
(1,17)-(38,22); then `OP_02` body with base **`$11`** (17) → up to **6 rows
17..22**, text at col 1.

### 3.4 `OP_04` `0x159F4` — door label (row 3, viewport-centered)

Skipped entirely when `-$79E1` (can't-see/darkness) ≠ 0.

1. `win_set_bg_pen($FF)` → **JAM1 transparent** over the 3D view.
2. `cursor((28 - strlen)/2, 3)` — centered on **col 14** = the 208-px
   viewport center (cols 1..26).
3. `win_print(&buf)` (left mode; manual centering above) → glyphs on
   **row 3**, px y 24..31, over the 3D scene.
4. `win_set_bg_pen(0)`.

No clear, no restore — wiped by the next viewport redraw.

### 3.5 `OP_05` `0x15A46` — Popup A (plain overlay, window (4,3)-(24,13))

Skipped when `-$79E1` ≠ 0.

1. `win_define(4,3,24,13)` → 21×11 cells, px (32,24)-(199,111); **flags ←
   `$80`** → open performs **no save-under and no clear**.
2. `win_open`; bg pen `$FF` (transparent); justify **center**.
3. Line estimate `0x1582E` on the buffer: a "line" ends at `0x0A` **or every
   `$14` = 20 chars** (≈ the 21-col window). If `lines/2 < 5`: cursor
   **(0, 4 − lines/2)** → first text row = abs row `7 − lines/2` (1 line →
   row 7, 8 px-centered-ish in the window); otherwise cursor stays (0,0) =
   abs (4,3).
4. `win_print` — each line centered in 21 cols, wrap col>20 → col 0,
   overflow > row 13 scrolls the window interior.
5. `win_close` — flags&$7F ≠ 1 → **no restore**; text persists until the next
   3D redraw.

### 3.6 `OP_06` `0x15AEE` — Popup B (framed sign, window (8,8)-(18,9))

Preprocess: every `-` (`$2D`) in the buffer → `{` (`$7B`, full-width bar).
Skipped when `-$79E1` ≠ 0.

1. `win_define(8,8,18,9)` → interior **11×2 cells**, px (64,64)-(151,79).
2. On the **screen window**, text pen ← `A4-$7A50` (**8**):
   - row 7: cursor (7,7): glyph `$10`, `$0E` ×11, `$11` (cols 7..19)
   - row 8: cursor (7,8): `$14`, `win_print("           ")` (11 spaces, exe
     bytes @ `0x15AE2`, ptr `A4-$6BEA`; JAM2 bg 0 = black fill), `$15`
   - row 9: same as row 8
   - row 10: cursor (7,10): `$12`, `$0F` ×11, `$13`
3. bg pen `$FF` (transparent): **signpost post** at cols 12..14 — rows
   11,12,13 each `$15 $7E $14`, row 14 `$13 $7E $12`; bg pen back to 0.
4. text pen ← `A4-$7A52` (1); `win_open(sign)` → auto-clear interior
   (8,8)-(18,9) to black; justify center; `win_print` → up to **2 visible
   rows 8..9** (cols 8..18), centered in 11 cols; extra lines scroll the
   11×2 interior.
5. `win_close` — no restore (no save-under); full frame spans cells
   (7,7)-(19,14), px (56,56)-(159,119), centered over the viewport.

### 3.7 Prompt ops

**`OP_07` `0x15CE6(0)` — SPACE wait** (stub pushes 0):

1. `prompt_space()` → **"('Space' to continue)"** printed at **(9,23)**,
   cols 9..29, px y 184..191 (current pen; no clear under it — it overprints
   the row-23 glyph-5 rule).
2. Key loop — source per state: arg≠0 → `key_read_scripted` (`0x97A2`,
   replay buffer `-$119A`); else `-$79E2 == 0` (3D view) →
   `key_read_3d` (`0x1E9CE`, **animates wall torches every poll**); else
   `key_read_console` (`0x22586`, IDCMP). Loop until code == `$20`.
3. `row23_separator()` → pen 2, glyph 5 at cols 2..37 of row 23, pen 1 —
   erases the prompt.

**`OP_08` `0x15D26`**: `-$71DC ← $FD` then `0x15CE6(1)` → waits SPACE from
the **scripted-input replay buffer** (`$FF` in buffer wraps to start when
`-$71DC == $FD`).

**`OP_09` `0x15D3C(0)` — Y/N**: clears cond flag `-$7951`; same 3-way key
source; `toupper`; loops until `Y` (`$59`) or `N` (`$4E`); `Y` → cond ← 1.
**Draws nothing and restores nothing** — the question text and "(y/n)?" come
from the preceding text op (usually `OP_01`), and the answer is not echoed.

**`OP_0A` `0x15D9A`**: `-$71DC ← $FD` then `0x15D3C(1)` (scripted source).

### 3.8 `OP_0B` `0x15DB0` — service title window + signboard sprite

Reads `u8 str_idx`, `u8 pos`; `0x15756(str_idx)` → service title id →
`-$55C8`; `sign_sprite_place(-1,0,0)` (clear old handle);
`sign_sprite_load(id)` (`0x316E`): if can't-see → `buf_select(1)`,
`clear_rect_preset(8)` (viewport), `buf_select(0)`; consults placement table
`A4-$7538` (24 entries); loads the signboard graphic via `0x9A30(id-1)` →
handle `-$79FE`, prepares with mode `$16`; then `sign_sprite_place(pos, $40,
$20)` → mode `$17` draws the board over the viewport. Sets exit bit 2.

**OPEN GAP `0x23C8C`:** mode `$17`'s pixel placement math (how `pos`/`$40`/
`$20` map to screen x/y) is untraced — needed before a remake can place the
service sign pixel-identically. (`0x9A30` sprite resolution also untraced.)

---

## 4. The "message window" on the play screen

There is **no dedicated message bitmap** — event text is drawn into the
full-screen 40×24 text window, over two distinct zones:

```
cells (engine px = cell*8; physical y = +8)
row 0          top border / compass row (chrome)
rows 1..15     3D viewport zone, cols 1..26 (= px (8,8)-(215,127))
                 OP_04 row 3 ┊ OP_05 (4,3)-(24,13) ┊ OP_06 frame (7,7)-(19,14)
row 16         chrome ticks (glyph 5 @ cols 12/21/31)
row 17         single-line message strip (OP_01/OP_03), cols 1..38
row 18         divider row (glyph 6 + 5×38 + 7; $0B junctions)
rows 19..22    message block / party roster lines (OP_02/OP_03)
row 23         bottom rule (glyph 5, cols 2..37) / prompt line
```

- **Over the viewport** (OP_04/05/06): drawn transparently (JAM1) or in small
  cleared windows; never saved/restored — the next 3D refresh repaints.
- **Bottom panel** (OP_01/02/03): overwrites the party-roster text area; the
  cleanup path repaints the roster (§7). The `nwcp.32` HUD chrome around it
  is only repainted on a full redraw.
- Doc-15 reconciliation: viewport preset rect #8 (cells (1,1)-(26,15)) ==
  px (8,8)-(215,127) == doc 15's 208×120 viewport at (8,8) — and the OP_0B
  cleanup restore blit uses exactly `(8,8)-($D7,$7F)` (§7), confirming both
  documents share one coordinate space.

---

## 5. Chrome glyphs used by the text paths

Font 8×8 (`EXTRACTED/fonts/mm2/8`): `$05` horizontal rule segment, `$06`/`$07`
left/right rule caps, `$0B` rule junction, `$0E`/`$0F` top/bottom box bars,
`$10`-`$13` box corners, `$14`/`$15` box sides, `$7E` solid block (post core),
`$7B` full-width bar (OP_06 `-` rewrite). Pens: border 8, chrome 2, text 1.

---

## 6. Worked example — Middlegate (location 0)

### 6.1 Event 01 — door label

Trigger (5,7)/DIR_N?, bytecode `04 01` → `OP_04 str[1] "Middlegate Inn"`.

- `0x15884`/`0x158C8` copy "Middlegate Inn" (14 chars) to `-$5D3C`.
- can't-see check passes (lit town tile).
- bg pen `$FF`; cursor((28−14)/2, 3) = **(7,3)**; `Text()` paints
  "Middlegate Inn" at cells (7..20, 3) = **engine px x 56..167, y 24..31**
  (physical y 32..39), pen 1, transparent over the 3D street scene; bg pen 0.
- No exit bits → nothing repainted; label vanishes on the next step's
  viewport redraw.

### 6.2 Event 20 — city gates exit prompt

Trigger (15,5)/ENTER, bytecode `01 18 09 10 01 0f 0c 0b 37`:

1. `OP_01 str[24]` = "You are at the city gates. Exit (y/n)?" (38 chars):
   divider row 18 + ticks rows 16/18; row 17 cols 1..38 cleared to black;
   centered col = (39 − 37)/2 = **1** → text at cells **(1..38, 17)**, engine
   px x 8..311, y 136..143 (physical 144..151), pen 1. Exit bit 0 set.
2. `OP_09`: waits for Y/N (3D key source — torches keep animating); no echo.
   `Y` → cond 1.
3. `OP_10 skip 1`: if cond, skips the `OP_0F end_script` token →
   `OP_0C map_transition($0B, $37)` — party exits to overland C-3.
   If `N`: `OP_0F` → cleanup `0x171AC` → exit bit 0 → status-bar text redraw
   (`0x560A`), erasing the question from row 17.

---

## 7. Restore path after a script ends

Scheduler `0x12D6`: after the event, if exit flags `-$7950` ≠ 0 → JSR
`-$7D40` = **`event_script_cleanup` `0x171AC`**:

1. `sign_sprite_place(-1,0,0)` — drop service-sign handle; `-$55C8 ← $FF`;
   `-$7EE4(1)`.
2. If bits 0 and 1 **both** set: redraw row-18 divider (`chrome_divider(0,
   $27, $12)`, pen 2 → 1).
3. Bit 0 → `-$7EBA` = `0x560A`: status-bar text repaint (chrome `0x60B6` pen
   2, values `0x62C8` pen 1).
4. Bit 1 → `-$7EA2` = `0x6150`: party roster lines repaint (slots from
   `-$796A`, empty slots cleared on rows ~19..22).
5. Bit 2 (OP_0B) → `-$7ECC` = `0x53A0` session gate, `-$7BA2($7F)`, then
   **`buf_copy_rect(1, 0, 8, 8, $D7, $7F)`** — blits the 3D viewport pixels
   (8,8)-(215,127) back from the other buffer; anim counter `-$755E` cleared.
6. `-$7950 ← 0`.

Viewport popups (OP_04/05/06) rely on the normal per-step viewport refresh
(`0x5382` → `0x2ECE`/`0x18870`) rather than this path.

---

## 8. OPEN GAPS

| Item | Address | Needed for |
|------|---------|-----------|
| Mode `$17` overlay-sprite placement math (OP_0B sign position) | `0x23C8C` | pixel-exact service signboard |
| Service-sign resource resolve (id → .anm) | `0x9A30` | same |
| `0x15756` service-title id semantics (vs plain string resolve `0x15884`) | `0x15756` | OP_0B title text |
| `-$71DC = $FD` scripted-input buffer: when is `-$119A` populated in normal play (demo/attract only?) | `0x97A2` | OP_08/OP_0A behaviour |
| `-$7EE4(1)` (`0x5080`) role in cleanup | `0x5080` | restore completeness |
| Status-bar repaint internals (`0x60B6`/`0x62C8` exact fields/cells) | `0x60B6` | bottom-panel fidelity |
| Physical top 8 raster lines (0..7): confirmed unreachable by these paths; what the display actually shows there (copper/OpenScreen setup) | startup | full-screen fidelity |

All other items above are ASM-confirmed (no guesses).
