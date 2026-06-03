# Character UI — View Party & Create Character (ASM trace)

Reverse-engineering notes for title-menu **P** (View Party), **C** (Create), and the
shared **character sheet** used from the roster browser and in-game (**V**). All addresses
are code-hunk PC offsets in `EXTRACTED/mm2.asm` / `mm2.annotated.asm`.

Implementation: `game/src/ui/AmigaCharacterUi.cpp`, layout in
`game/include/mm2/ui/AmigaCharacterUiLayout.h`.

---

## 1. Title menu entry

| Step | Address | Notes |
|------|---------|-------|
| Main title loop | `$0FD4`–`$1062` | Logo / menu; key in `-2(A5)` |
| Key filter | `$1062`–`$1090` | **`C`/`M`/`O`/`P`/`Q`** → `$1288` scheduler |
| Menu hub | `$1280` | `main_scheduler_loop` |
| Command switch | `$1482` | Subtract cascade maps ASCII → handler |

### `$1482` key map (confirmed)

| Key | ASCII | Handler | Action |
|-----|-------|---------|--------|
| (special) | `0x42` | `$13BC` | Save game (`JSR -$823C`) |
| **C** | `0x43` | `$13C4` | **Create character** (`JSR -$82A2`) |
| D | `0x44` | `$13D0` | `JSR -$82AE` |
| E | `0x45` | `$13D8` | `JSR -$8338` |
| **M** | `0x4D` | `$13E4` | Controls (`JSR -$8038`) |
| **O** | `0x4F` | `$13F0` | Options if `A4-$864E==1` |
| **P** | `0x50` | `$1412` | **View Party** (roster browser) |
| **Q** | `0x51` | `$1432` | Quit confirm (`JSR -$8200`) |

### Title **P** call graph (`$1412`)

When **`A4-$864E==0`** (normal title, no continue flag):

```
$1412  TST.B -$864E(A4)
       BEQ   $142E
       CLR.W -(SP)
       JSR -$83C2          ; hide cursor
       JSR -$8152(A4)      ; → roster browser session LAB_84E @ $084E
       MOVE.W #1,-(SP)
       JSR -$83C2
$142E  BRA $151A
```

**Continue-game path** (`A4-$864E==1`, `$54BA`): same **`JSR -$8152`** opens the roster
browser; **`FUNC_ShowCommandReferencePanel`** (`$5D7C`) is the separate command-reference UI.

Combat **V** uses **`JSR -$8200`** @ `$11B88` (different thunk — in-game inspect / quit path).

---

## 2. Title View Party — roster browser (`LAB_84E` @ `$084E`)

**Not** the in-game **`$551A` / `$5984` book panel** (8 party slots, `book.32` blits).

### Session setup (`$084E`)

| Step | ASM | Notes |
|------|-----|-------|
| Clear / palette | `$0862`–`$0886` | `-83C2`, `-83EC`, `-8086`, `-85AE` |
| Header strings | `$0892`–`$0920` | `(View All)`, underline, `Characters`, party digits |
| Red border | `$009E2` | **`JSR -$809E`** — row `$15`, width `$26`, height `$15`, col `$1B` |
| Slot list | `$00AD4` | **`JSR LAB_470`** — 24 lines A–X |
| Footer | `$00AEC`–`$00B02` | `'A'-'X' to View`, Space hirelings, Esc back |
| Key loop | **`LAB_B26` @ `$00B26`** | Poll `-8098`; dispatch below |

### List draw — **`LAB_470` @ `$0470`**

Loop **0..$17** (24 slots). Two columns via **`DIVS #12`**:

| Column | Slots | Row | Col |
|--------|-------|-----|-----|
| Left | A–L | `slot/12 + 5` | `1` |
| Right | M–X | `slot/12 + 5` | `$15` (21) |

Occupied line format (`$0552`–`$0648`):

```
[letter+'A'] '-' [name] [class abbrev from A4-$86D8] '/' [level $71]
```

Example: `A- Sir Felgar K/1`. Empty slot: `G-` only (`LAB_6AE`).

**Hirelings page:** Space @ `$00C5A` adds **`$18`** to page offset in `-2(A5)` (slots 24–47).

### Key dispatch — **`LAB_B26` @ `$00B26`**

| Key | Range | Action |
|-----|-------|--------|
| **A–X** | `$41`–`$58` | Validate slot → **`JSR -$8062`** @ `$00BC6` → sheet modal |
| **Space** | `$20` | Toggle hirelings page (`+$18` / wrap) |
| **Esc** | `$1B` | Exit session → title |
| **1–5** | `$31`–`$35` | Party-slot picker (clears `A4-$8696`, `$00C72`) |

Sheet open path @ `$00C38`: redraw border → **`JSR LAB_675A`** → **`LAB_6622`**.

### SDL port (`AmigaCharacterUi`)

| ASM | Port | Status |
|-----|------|--------|
| `LAB_84E` + `LAB_470` | `renderRosterList()` | **Implemented** — black + red `-809E` frame, A–X columns |
| `LAB_B26` keys | `tickViewParty()` | **A–X**, Space page flip, Esc |
| Class abbrev | `K P A C S R N B` | First letter via `A4-$86D8` table |
| Hirelings page | `roster_page_offset_ += 0x18` | **Implemented** (footer string swap) |

---

## 3. In-game party panel — `$5940` / `$551A` / `$5984` (separate)

Used after create confirm and in-game party view — **8 slots** from **`A4-$8696`**, not title **P**.

| Step | Address | Asset |
|------|---------|-------|
| `$5940` | `view_party_session` | Modal loop |
| `$551A` | `party_roster_panel_draw` | `book.32` frames 0 + 11 @ (312,144) |
| `$5984` | `party_roster_list_draw` | HP/SP columns, 8 rows |

SDL: constants kept in `AmigaCharacterUiLayout.h` for future in-game wiring; **not** used for title **P**.

---

## 4. Character sheet — title path vs `$39B4`

### Title / roster sheet — **`LAB_675A` → `LAB_6622`**

Opened from roster **A–X** (`-$8062` @ `$00BC6`). Black screen + red border @ **`$00C1A`**
(`JSR -$809E`: row `$15`, width `$1D`, height `$15`, col `$0A`).

Three-column stat block, equipped/backpack divider, Ctrl-N / Ctrl-D footer (`LAB_F1C` region).
**No `book.32` backdrop** on this path.

SDL: `renderCharacterSheet()` — text layout matched to WinUAE screenshot; item names from
`items.dat`.

### In-game / combat sheet — **`$39B4` (`character_sheet_draw`)**

Same roster fields via **`LAB_38EA`**, but **`JSR LAB_4252`** composites **`book.32`** frames
11,11,5,5,4,3,2,1 @ (312,144). Used from combat **V** / in-game inspect (`-$8200` family).

Field index → row/col tables: **`A4-$8AF8` / `A4-$8AE0`** (`$38EA`).

---

## 5. Create Character (partial)

| Address | Role |
|---------|------|
| `$13C4` | Title **C** → `JSR -$82A2` |
| `$944C` | Slot picker (portraits `A4-$AA2C`, N/E/S/W) |
| `$5724` | Confirm → `$944C` then **`$5940`** (8-slot book panel) |
| `$6520+` | Race/class/sex/alignment (str.dat indices **pending**) |

SDL: slot picker stub only; **no roster writes**.

---

## 6. Red border draw — **`JSR -$809E`**

Rectangle outline in **text cells** (palette index `$17` = red on Amiga). Not `book.32`,
not `throw.32`. Related line helper **`-$8080`** via **`LAB_60DE`**.

| Screen | ASM call | Cells (row, col, w, h) |
|--------|----------|-------------------------|
| Roster list | `$009E2` | (21, 27, 38, 21) |
| Roster sheet | `$00C1A` | (21, 10, 29, 21) |
| Command ref | `$5D80` | similar family |

SDL: `drawRedBorder()` — 2 px `fillRect` outline, RGB (255,0,0).

---

## 7. Implementation status

| Feature | Status |
|---------|--------|
| Title **P** → **`LAB_84E`** A–X roster list | **Done** |
| Red `-809E` border (not book panel) | **Done** |
| **`$551A` / `$5984`** 8-party book panel | **Not title P** — deferred for in-game |
| Title sheet (`LAB_675A` layout) | **Done** (skills str.dat **pending**) |
| `$39B4` book composite sheet | **Not wired** (combat **V** path) |
| Ctrl-N / Ctrl-D rename/delete | **Stub** (keys detected) |
| Create flow / str.dat | **Stub** |

Regenerate symbols: `python tools/harvest_symbols.py --merge && python tools/apply_symbols.py`
