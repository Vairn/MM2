# First-Person 3D View and Game-Screen Composition

Reverse-engineered from `EXTRACTED/mm2.capstone.asm` (code hunk 0). All
`d(A4)` offsets are global-state fields; `A4 = $7FFE` (see
`02-runtime-memory-map.md`). The single blit primitive used everywhere is the
thunk `JSR -$7C20(A4)` — `blit(sheet_ptr, frame, y, x)` (4 longword/word args,
`LEA $A(A7),A7` after each call).

## 1. Tileset roles per environment

The area-load dispatcher (`0x1786..0x187E`, jump table at `0x1880`, 7 env
types) resolves filename pointers (filename-pointer table at DATA `0x7B0`,
4 bytes/entry) into per-environment graphics handles:

| Field (A4)        | Role            | town env        | cavern          | castle           | outdoor          |
|-------------------|-----------------|-----------------|-----------------|------------------|------------------|
| `-$7A06` ($85FA)  | **walls**       | town.32 (13)    | cave.32 (17)    | castle.32 (21)   | — (uses layers)  |
| `-$7A22` ($85DE)  | **floor**       | townf.32 (10)   | cavef.32 (14)   | castlef.32 (18)  | outf.32 (26)     |
| `-$7A1E` ($85E2)  | **ceiling**     | townt.32 (11)   | cavet.32 (15)   | castlet.32 (19)  | —                |
| `-$7A1A` ($85E6)  | **auto-map**    | townb.32 (12)   | townb.32 (16)   | townb.32 (20)    | outb.32 (25)     |
| `-$7A16` ($85EA)  | outdoor sheet **0** | —               | —               | —                | outdoor1.32 (22) |
| `-$7A12` ($85EE)  | outdoor sheet **1** | —               | —               | —                | outdoor2.32 (23) |
| `-$7A0E` ($85F2)  | outdoor sheet **2** | —               | —               | —                | outdoor3.32 (24) |

(`townb.32`/`outb.32` are the small 36-frame 14×11 auto-map tiles documented in
the cartography notes; `town.32`/`cave.32`/`castle.32` are the big ~57 KB wall
sets whose frames are stored at decreasing sizes for depth scaling, e.g.
`town.32` frame dims 160×92, 96×55, 48×27, 16×10.)

`-$7A02` ($85FE) is a separate sky/backdrop sheet used by the ceiling fill.

## 2. Render pipeline (interior dungeon view)

```
session refresh (LAB_5436 / LAB_545E, 0x5436/0x545E)
  └─ viewport refresh (0x5382)
        ├─ if -$79E2 (view mode) == 0 → JSR 0x2ECE   (3D first-person)
        └─ else                        → JSR -$7D34   (alternate: text/auto-map)
```

### 3D master — `0x2ECE`

1. `-$79E0 = 0xFFFF`, clears input latch (`-$7C3E`).
2. **Floor backdrop**: `blit(floor -$7A22, frame 0, …)` (`0x2EE2`).
3. **Ceiling/sky backdrop**: `blit(sky -$7A02, frame, …)` (`0x2F00`); the frame
   index comes from `0x2BCC` (a solid test) so it picks `sky.32` frame 0 vs 1 =
   closed-ceiling vs open-sky.
4. Clears the 20 frustum-cell slots `-$F0D..-$F20`.
5. `JSR 0x2900` — **builds the visible-cell wall codes** from map data.
6. Paints walls **back-to-front** (painter's algorithm): for each non-zero
   frustum slot it calls one of the wall-piece primitives with a depth/column
   argument (`0x01..0x04` near→far, `+0x80` = mirrored/right column).
7. `JSR -$7D04` (flush), `addq #1, -$755E` (animation frame counter).

### How the "window" is drawn and how colour is chosen

There is **no flat-colour rectangle fill** — the viewport is composited entirely
from image sheets, each carrying its own 32-entry Amiga `0x0RGB` palette:

- The scene area is a **208-wide viewport** built from two stacked **208×60**
  backdrops: `sky.32` (the ceiling/sky) on top and the per-environment floor
  image on the bottom, then perspective walls painted over them.
- **`sky.32`** is a *global* sheet (loaded once at `0x25FA6` from filename-table
  entry 6) with **2 frames** — the engine selects frame 0/1 via the `0x2BCC`
  solid test (closed ceiling vs open sky).
- The **floor** image is the per-environment one (`-$7A22`): `townf.32`,
  `cavef.32`, `castlef.32`, or `outf.32` — each a single 208×60 frame. This is
  where the inside/outside colour difference comes from: the env dispatcher
  (`0x1786..0x187E`) loads a different floor/wall/ceiling sheet set per
  environment, so the "colour" is just whichever sheet's palette is active.
- `nwcp.32` (300×90) is the bottom HUD panel drawn beneath the viewport.

So **inside vs outside = different sheet sets** (town/cave/castle vs
outdoor1-3 + `outf`), not a colour flag. The editor's *Maps → Window (3D
backdrop)* tab reproduces this sky+floor composition per area.

### Cell builder — `0x2900` (helpers `0x2BCC`)

Reads the materialized neighbourhood bytes around the party (`A4-$55C9..-$55D4`,
filled by `map_row_sampler @0x190C` from **map.dat page 0** (`A4-$55BA` centre
page and adjacent `$51BA/$50BA/$53BA/$52BA` buffers) and ANDs them with
direction-bit masks (`d1/d4/d5`, plus shift counts `-$75C7/-$75C8/-$55D7`) to
extract each cell's 2-bit field code into the frustum slots `A4-$F0D..-$F20`.

Each page-0 direction field uses the same `(dark<<1)|wall` bit layout as page 1,
but **3D rendering treats all non-zero field codes as visible surfaces** (ASM
`cell & mask != 0`). Code `1` = plain wall, `2` = door-variant wall (`+0x10`
frames in `wall_piece_*`), `3` = door (+ separate overlay blit). Code `2`'s
"dark" semantics apply to **page-1 movement/light only**, not to the 3D hood.
Values `3` adjacent to an open cell are normalised to `1` (`0x2B6A..0x2BBC`).

Page 1 (`A4-$54BA`) is loaded separately for movement, events, and darkness
(`A4-$55D6` current-cell copy after hood refresh) — **not** the hood source for
3D wall sampling.

#### Page-0 visual layout (2 bits per side — walls only)

Each cell is one byte, four **2-bit** fields. **No event bit** on page 0.

| bits | field | values |
|------|-------|--------|
| 0-1  | North | `0` open, `1` wall, `2` wall+torch, `3` door |
| 2-3  | East  | same |
| 4-5  | South | same |
| 6-7  | West  | same |

Example: `0x08` → E=2 (torch); `0x80` → W=2 (torch); `0xB3` → N=3 and S=3 (doors).
The 3D hood (`0x2900`) reads **only** these bytes from `A4-$55BA`.

#### Page-1 collision layout (2 bits per side + darkness + event)

Per cell, each direction is `(dark << 1) | wall`: low bit = wall, high bit =
darkness. **West's high bit (`0x80`) is the event flag** (not a fourth wall type).

| bits | mask        | field            |
|------|-------------|------------------|
| 0-1  | `0x01/0x02` | North wall / **dark** |
| 2-3  | `0x04/0x08` | East wall / **dark**  |
| 4-5  | `0x10/0x20` | South wall / **dark** |
| 6    | `0x40`      | West wall            |
| 7    | `0x80`      | **EVENT flag** (collision only) |

**Darkness (collision high bit per direction).** Entering a dark square drains one party *light factor*.
The "can't see" overlay flag `-$79E1` is (re)computed each frame in
`session_interaction_gate` (`0x53A0`): it is forced to 0 while the light flag
`-$79AB` is non-zero (active light suppresses darkness), otherwise it is set
from the materialized current-cell bits (`-$5600`/`-$55D6`). This is why the
Cleric has two light spells: **Light** grants +1 light factor ("light a darkened
square") and **Lasting Light** grants +20 ("dispelling darkness") — each factor
covers one dark square. The per-direction darkness bits are sparse (N=206,
E=950, S=125 cells).

**Event flag (`0x80` on page 1 only).** Marks "this tile has an `event.dat` entry"
on the **collision** page (`A4-$54BA`; copied to `A4-$55D6` after hood refresh).
Verified: every tile-event triplet lands on a collision cell with `0x80` set.
`event_tile_scanner` @`0x175E2` uses tile id `(y<<4)|x`. **Do not** treat page-0
visual `0x80` as an event — that byte is W=2 (torch) in the visual encoding.

### Wall-piece primitives (each blits from the wall set `-$7A06`)

| Addr     | Piece            | Depth tables (per slot, A4)                          |
|----------|------------------|-------------------------------------------------------|
| `0x2C1A` | **front** wall   | pos `-$75AE`/`-$75B6`, frame from slot value          |
| `0x2C9A` | **left** side    | pos `-$75A2`/`-$759A` & `-$7592`/`-$758A`, frame `-$75A6`, side toggle `+0x10` |
| `0x2DB4` | **right** side   | pos `-$757E`/`-$7576` & `-$756E`/`-$7566`, frame `-$7582`, `+0x08`/side toggle |

Each takes `(depth_index, frame/flags)` on the stack, decrements the depth
(`subq #1, $b(a5)`), indexes the depth tables to get screen x/y and the wall
frame, then `blit(-$7A06, frame, y, x)`. The `+0x80`/`+0x10`/`+0x08` adjustments
select mirrored geometry and near/far frame variants. Distance scaling is
inherent: deeper slots pick smaller frames from the multi-resolution wall set.

Auxiliary `0x2BCC` returns whether a given cell/edge is solid (mask test
against `-$75BE`), used to decide which side pieces to draw.

### Extracted geometry tables (DATA hunk; 4 depth bands, near→far)

Resolved via `data_offset = $7FFE - disp` and dumped from `MM2BOOT/mm2`. Each
table has 4 entries (depth 0 = nearest). The blit anchor is top-left and the
call is `blit(sheet, frame, y, x)` (verified against the auto-map call site).

```
front wall:  y (-$75AE @0xA50) = [22, 40, 54, 62]  (d=0 → screen 23)
             x (-$75B6 @0xA48) = [32, 64, 88, 104]
left side:   x1(-$75A2 @0xA5C) = [8, 32, 64, 88]    y1(-$759A @0xA64) = [8, 22, 40, 54]
             x2(-$7592 @0xA6C) = [8, 8, 40, 88]     y2(-$758A @0xA74) = [22, 40, 54, 62]
right side:  x1(-$757E @0xA80) = [192,160,136,120]  y1(-$7576 @0xA88) = [8, 22, 40, 54]
             x2(-$756E @0xA90) = [192,160,136,120]  y2(-$7566 @0xA98) = [22, 40, 54, 62]
left frame:  (-$75A6 @0xA58) = [12, 14, 2, 3]
right frame: (-$7582 @0xA7C) = [13, 15, 2, 3]
dir mask:    (-$75BE @0xA40) = [1, 2, 4, 8, 16, 32, 64, 128]
```

Far corners at depth 2–3 **reuse the same sheet frames as the front wall** at
that depth (`2` / `3`); only the blit position comes from the far tables
`-$7592`/`-$758A` (left) or `-$756E`/`-$7566` (right). Dedicated far sprites
exist only for d0–d1 (frames `12`–`15`). See `ref-layout-town-frames.png` rows 4–5.

**Lateral × depth lattice** (rows −3=far..0=near): the engine paints **five**
lateral slots only — **x=−2** far left, **x=−1** near left, **x=0** front,
**x=+1** near right, **x=+2** far right. Deepest row: `(-2,-3)` / `(2,-3)` far
corners (reuse front frame 3 at far-table coords); `(-1,-3)` / `(1,-3)` near.
Reference: `ref-layout-town-coords.png` (5×4, x = −2..+2).

Front-wall frame = depth band (`0..3`) into the wall set; **`+0x10` for field
code 2** (`wall_piece_front @0x2C4E`). Code 3 normalises to 1 for the wall
frame and triggers a separate door-overlay blit (`jsr -$7cfe`). The nearest
front wall (depth 0) is `town.32` frame 0 (160×92) placed at **(32, 23)**; frames
16–31 are the code-2 (+0x10) variants of the same geometry.

### Blit placement — where `(x, y)` come from

All 3D sprites are pasted with the same primitive:

```text
JSR -$7C20(A4)     ; blit(sheet, frame, y, x)
```

Stack layout is `[sheet.l][frame.w][coord0.w][coord1.w]`. Backdrops and side
pieces push **y then x**; `wall_piece_front @0x2C6E` alone pushes **x then y**.
The blit always reads **screen x from the second-to-last word, screen y from
the last** (verified: sides at table (8,8) render correctly; front joins side
inner edges at x=32 / x=192 with depth-0 y=23).

Coordinates are **absolute top-left positions inside the 208×120 viewport**
(sky 60 px + floor 60 px). They are **not** relative to a second origin — the
table values are used verbatim by `wall_piece_*` (`0x2C6E..0x2C7E` loads
`-$75AE`/`-$75B6` and pushes them straight to the blit).

#### Backdrops (hardcoded in `view_3d_master @0x2ECE`)

| Pass | Sheet (A4) | Frame | x | y | ASM |
|------|------------|-------|---|---|-----|
| Floor | `-$7A22` | 0 | **8** | **68** (`0x44`) | `0x2EE2` |
| Sky/ceiling | `-$7A02` | `0x2BCC` | **8** | **8** | `0x2F00` |

Only the backdrops use the `(8, 8)` / `(8, 68)` pair. The **8** is the left
margin that centres the 208-wide `.32` sheet inside the playfield; it is **not**
added again when painting walls.

#### Indoor walls (DATA tables indexed by depth 0..3)

Loaded from `A4-$75xx` by `wall_piece_front/left/right` after `subq #1` on the
paint-depth argument (callers pass 1..4; table index = depth−1).

| Piece | A4 table (disk) | values d=0 | **screen (x, y)** |
|-------|-----------------|------------|-------------------|
| Front | `-$75AE` / `-$75B6` | 22 / 32 | **(32, 23)** — d=0 y +1 vs table |
| Left near | `-$75A2` / `-$759A` | 8 / 8 | (8, 8) |
| Left far | `-$7592` / `-$758A` | 8 / 22 | (8, 22) |
| Right near | `-$757E` / `-$7576` | 192 / 8 | (192, 8) |
| Right far | `-$756E` / `-$7566` | 192 / 22 | (192, 22) |

**Front-wall coordinates.** `wall_piece_front @0x2C6E` pushes `-$75AE` (y) then
`-$75B6` (x) — same y-then-x order as side pieces. Screen **x ← 75B6**,
**y ← 75AE**. Reference composites need **y + 1 at depth 0** (table 22 →
screen 23) to align the nearest front wall with the side-piece joins; deeper
bands use the table values verbatim.

Paint order (far → near) is fixed in `view_3d_master @0x2F7E`; see
`editor/src/core/View3D.cpp` `collectBlits()`.

Dump verified with `tools/dump_tables.ps1` against `ghidra/mm2_data_00.bin`.

#### Outdoor terrain (separate tables — do **not** reuse indoor wall coords)

Outdoor first-person rendering is **`outdoor_3d_master @0x18870`**, not
`0x2ECE`. Paint order:

1. `outf.32` floor @ **(8, 68)** — same backdrop anchor as indoor
2. `sky.32` @ **(8, 8)** (or alternate sky sheet when weather flag set)
3. **`@0x182D8` floor decor** — sheet **`-$7A0A`** (biome: desert/ocean/tundra/swamp
   loaded per screen type 41–44), **not** `outdoor1/2/3.32`
4. **`@0x1877A` horizon blits** — three passes (`@0x1844C`, `@0x184B8`, `@0x18620`).

**Map → terrain (sheet index).** `@0x9544` samples three map page-0 hood rows into
`-$55C6` / `-$55C2` / `-$55BE`. Each byte is converted by `@0x9524`
(`terrain = kLookup[byte & $1F]`) and stored in `-$5ADA` / `-$5AE2` / `-$5ADE`
(at `@0x1877A`, value−1). That index selects **`-$7A16[index]`** →
`outdoor1/2/3.32`. **Any sheet can appear in any lane** — the map byte decides
which art library, not the filename.

**Lane → screen coords.** The three passes only differ in **position/frame
tables** (which screen band + depth column gets that row's terrain):

| Pass | ASM | terrain from | y table | x table (col 0..3) | frame table |
|------|-----|--------------|---------|---------------------|-------------|
| L1 | `@0x1844C` | `-$5ADA` | `-$6BC2` | `-$6BCA` [40,40,64,88] | `-$6BCE` |
| L2 | `@0x184B8` | `-$5AE2` | `-$6BAA` | `-$6BB2` [8,16,32,88] | `-$6BBA` |
| L3 | `@0x18620` | `-$5ADE` | `-$6B96` | `-$6B9E` [176,152,136,120] | `-$6BA2` |

L2/L3 y can be overridden from `-$6BC2` when adjacent columns match.

Reference grids (`ref-layout-outdoor{1,2,3}-frames.png`) show **the same
lane×column slots for every sheet** — only the art differs. Runtime placement
is **map row → lane pass → coords**, **map terrain id → sheet**.

**Floor decor (separate from horizon walls)** — `@0x182D8` on sheet **`-$7A0A`**
uses **y = `$80 − 6BD6[col]`** → **[108, 93, 78, 68]** (bottom of the 120px
viewport — foreground grass/terrain detail over the floor band). Three decor
branches (push y, x, frame, sheet):

| Branch | Condition | frame | x | y |
|--------|-----------|-------|---|---|
| A | map flag `-$55C6` | `col` (f0-3) | **8** | `$80−6BD6` |
| B | flag `-$55C2` | `col+4` (f4-7) alt, **`col+12` (f12-15)** main | **8** | `$80−6BD6` |
| C | flag `-$55BE` | **`col+8` (f8-11)** alt, **`col+16` (f16-19)** main | **112** / **6BDE−8** | `$80−6BD6` |

**Frame art (`desert.32`, 20 frames):** **f0-3** centre path; **f4-7** left wedge
sprites (B alt, x=8); **f8-11** separate right wedge sprites (C alt, x=`$70`) —
not mirrored copies of f4-7, distinct triangles/trapezoids in the sheet. **f12-15**
/ **f16-19** narrower stepped strips when `-$55C6` is also set (main `@18344`:
B+12 left, C+16 at `6BDE−8`).

`6BDE` on disk = **[184, 160, 136, 112]**. `ref-outdoor-floors.png` **decor C** =
**f8-11**; **C main** = f16-19.
Do **not** place `outdoor1.32` frames at these coords — decor art comes from the
biome sheet in `-$7A0A` (e.g. `desert.32`). Reference grid
`ref-layout-outdoor1-frames.png` (and outdoor2/3) show **all three lane tables**
(L1/L2/L3 rows) for that sheet's art. `ref-layout-outdoor-combined-frames.png`
stacks L1+L2+L3 per column. `ref-outdoor-floors.png` is the separate `-$7A0A`
decor pass.

## 3. Outdoor view

Uses **`outdoor_3d_master @0x18870`**: `outf.32` floor + optional `sky.32`, then
the three `outdoor1/2/3.32` layer passes above. Terrain frame/sheet selection
comes from the `-$5ADA` / `-$5AE2` / `-$5ADE` byte tables filled by
`0x1877A` from map page-0 terrain bytes — **not** from the indoor frustum
builder `0x2900`.

### 2D map / auto-map tile mapping (cell → frame)

The 16×16 `overland_map_view` (`0x223A`) and the cell helper it calls (`0x2182`)
turn each map-cell byte into a frame of the auto-map tileset `-$7A1A`
(`outb.32` outdoors, `townb.32` interior). The helper **branches on the
view-mode flag `-$79E2`** (set on screen entry: `1` = outdoor surface):

- **`-$79E2 != 0` (outdoor/surface, `@0x222A`):** `frame = byte & 0x1F`. The
  outdoor map bytes are **raw terrain ids**; the high bits (`0x20/0x40/0x80`)
  are passability/event flags. The low 5 bits index `outb.32` directly. (Verified
  on `map.dat`: surface screens use a small set of dominant terrain values, e.g.
  screen 11 = `0x64/0x21/0x8A` → frames `4/1/10`.)
- **`-$79E2 == 0` (interior dungeon, `@0x21D4`):** `frame =
  kCartoTile[byte >> 2]`, a 64-entry wall-bit-decode table at data `0x9D0`.

Elemental planes 41..44 override every cell to a fixed frame (`8/4/4/5`,
`@0x21EA`); their `map.dat` page-0 bytes are uniform (`0x28/0x25/0x27/0x26`).

> **Editor note:** the `Cartography.h`/`MapSection` auto-map originally applied
> the `kCartoTile` wall-decode to *all* screens, which scrambled outdoor terrain
> (treating terrain ids as wall-bit patterns). Outdoor screens now use the raw
> `byte & 0x1F` path.

## 4. Game-screen composition

The full play screen is assembled by the **session refresh** family:

- `LAB_545E` (`0x545E`) — `session_start_refresh`: the "resume interactive play"
  gate; sets mode via `-$7C32`/`-$7C3E`, routes through `0x53A0` then `-$7EAE`
  (LAB_5D54) and finishes by re-enabling input.
- `LAB_5436` (`0x5436`) — body invoked by `0x54BA` (which brackets it with
  `-$7438` paging counter); draws the play frame and calls the viewport refresh.
- `0x5382` — viewport area refresh (the `-$79E2` switch above); also handles the
  darkness/"can't see" overlay when `-$79E1` is set.

### 4.1 Play-screen chrome is a font-glyph lattice (NOT `nwcp.32`)

**Corrected trace (2026-06).** The earlier note that the persistent in-game
chrome is the `nwcp.32` panel sheet is **wrong** — `nwcp.32` is only used on
the title/menu screens. The in-game chrome is drawn entirely with text-engine
primitives on the 40×24 grid of 8×8 font cells (`x = col*8`, `y = row*8`;
cursor thunk `-$7BFC → 0x22108`, glyph put `-$7C62 → 0x218EA`, string put
`-$7BE4 → 0x22376`):

- `-$7F86 → 0x4032` `h_line(col0,col1,row)` — glyph `6`, `5`…, `7`.
- `-$7F80 → 0x4088` `v_line(row0,row1,col)` — glyph `9`, `0x0B`…, `8`.
- `-$7F7A → 0x422A` outer frame — corner glyphs `1/2/3/4` via `0x410A`.
- `-$7F62 → 0x42DC` — **cell-rect clear** (the old `draw_party_status_panel`
  annotation was a mislabel; `0x42DC` clears `(col*8,row*8)` rectangles).
- Line glyphs `1..9,0x0B` are the red box-drawing set in the game font;
  `0x18..0x1B` are the arrow glyphs used by the command reference list.

Chrome assembly per refresh (`0x52A2` body):

- `0x60F4` chrome init: outer frame; h-lines rows `0x10`/`0x12` cols `0..0x27`;
  v-line col `0x1B` rows `0..0x10` (viewport / right-column divider).
- `0x60B6` status dividers: v-lines rows `0x10..0x12` at cols `0xC/0x15/0x1F`.
- `0x62C8` status row `0x11`: col 1 `'O' Options` (new-game flag `=1`) else
  `'P' Protect`; col `0xD` `Day=` + `-$79DE[era]` (3 cells); col `0x16`
  `Year=` + `-$79CA[era]` (4 cells); col `0x20` `Face=` + key char `-$79B1`.
- `0x6150` party panel rows `0x13..0x16`, slots two-per-row at cols `1`/`0x14`:
  `" n) "` + name (text attribute 1 via `-$7C08` when roster condition byte
  `+$26 != 0`) + `" /"` + HP word `+$5E` (roster **HP max** field; values
  `1..999` print via `-$7BDE` with trailing-space pad to 3 cells @
  `0x6266..0x629A`; `>999` prints literal `"+++"` @ `0x62C4` via `pea` @
  `0x625A`). Combat reuses the same rule on the bottom party strip @
  `0x12910` (`cmpi #$3E8` / `bcc` → `"+++"` @ `0x129C8`). **Quick Ref**
  (`0x595C`) and the character sheet (`0x39B4`) print full numeric HP/SP with
  no `+++` cap. SP has no `+++` rule anywhere traced. Empty slots are
  cell-cleared via `-$7F62`.
- Right column cols `0x1C..0x26` rows `1..15`: new game → `0x5E28`
  protection/spell-status panel (h-line row 9, labels `Light`/`Magic`/`Forces`
  rows `0xA..0xC`); otherwise `0x5D54` command reference (string table
  `A4-$741A`).

**Adjudication note on `0x42DC` / `A4-$85C2`:** a parallel trace read `0x42DC`
as an nwcp.32 portrait blit because it retargets `-$619C` to `A4-$85C2`
(allocated @ `0x25F7E`). The alloc dims are `$BF/$13F` = **191/319**, i.e. the
**320×192 play-area back buffer**, not the 300×90 `nwcp.32` sheet; `-$619C` is
the text engine's *current render-target bitmap*. `0x42DC` therefore reads:
save target + its attribute byte `$12`, retarget to the play buffer, set
mode-2 attribute (`-$7BAE`), **clear the cell rect** (`-$7C68` takes only a
pixel rect `((base+arg)<<3, +7)` — no source frame), restore. The runtime
publish `0xCBF6/0xCBFE` (`-$7ED8` frame 0) belongs to the title/menu panel
path (callers `0x6EAC/0xA820/0xA906/0xB572`), not the exploration refresh
chain `0x545E/0x5436/0x52A2`.

**Remaining gaps:** `0x410A` frame-corner internals; exact palette effect of
text attribute 1 (`0x220BE`); `0x5EB8..` value printout after the
Light/Magic/Forces labels (strings `0x6068/0x6074/0x6080`); copy-protection
challenge content rows 1..8 of the new-game right column (out of scope);
`-$7BAE` mode-2 attribute semantics (`0x22C24` dispatcher) and the per-slot
byte tables `A4-$8BBF/-$8BB6/-$8BAD/-$8BA4` used by
`format_party_status_line @ 0x531A` (need a data-hunk dump).
The character/party stats *text formatting* is the `0x5312`/`0x62C8`/`0x6150`
family described above.

`0x316E` builds the on-screen **monster/encounter sprites** layered over the
viewport (24-entry placement table `-$7538`).

## 5. Notes toward an editor preview

To render either view from the data files alone:

- **Walls/floor/ceiling**: decode the env wall set (`town/cave/castle.32`) +
  `*f.32`/`*t.32` with the existing `Gfx` codec; each `.32` already exposes its
  multi-size frames.
- **Geometry**: replicate `0x2900` — from a chosen cell + facing, gather the
  ~5-deep frustum of cells, read map.dat **page 0 visual** 2-bit direction codes
  (non-zero field = visible; code 2 = door-variant wall), and emit the front/left/right
  slot list. Use `editor/tests/view3d_trace.cpp` for offline verification.
- **Composite**: floor + ceiling fills, then paint slots far→near using the
  depth position/frame tables (`-$75AE`, `-$75A2`, `-$7592`, `-$757E`, `-$756E`,
  `-$7566`, frame tables `-$75A6`/`-$7582`). Those tables live in the DATA hunk
  (resolve each `A4-$76xx` to data offset `$7FFE-disp` and dump like the
  cartography tables) and would need to be extracted for pixel-accurate output.
- The auto-map ("cartography") view is already implemented in the Maps section.
