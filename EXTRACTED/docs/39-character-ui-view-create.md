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

## 5. Create Character (ASM trace)

### Entry paths

| Path | Address | Notes |
|------|---------|-------|
| Title menu **C** | `$1482` → `$1394` | Save-game branch in annotated listing; title **create** session enters at **`$01C684`** after slot setup (`-$5A38`, `-$5A3E`). |
| Inn / party assembly | **`$0826`** `char_create_party_assemble` | Era/town filter, roster **A–X** picker, good/evil mix check — **not** the dice-roll stat screen. |
| Step driver | **`$01C25A`** | Main input loop; step mode in **`A4-$5AD0`** (values 1–6 via menu @ `$01C714`). |

**Gap:** title **C** thunk in older notes (`JSR -$82A2` @ `$13C4`) does **not** match the current annotated dispatch (`$13C4` → `$14F2`). Treat **`$01C684`** as the verified stat-create session entry.

### Stat roll — **`$01C00E`**

| Item | ASM | Notes |
|------|-----|-------|
| Roll loop | `$01C0E2`–`$01C13C` | Seven stats (`cmpi #6` → indices 0..6). |
| RNG | `$01C10C` `moveq #$14,d1` + **`JSR -$7B54`** | **1d20** per stat (confirmed); manual ratings **3..21** = roll + 2, cap 21. |
| Source tables | **`A4-$68EE` / `-$68D0`** (+ jump table @ `$01C09E`) | Per-step table pair; race re-roll path @ **`$01B646`**. |
| Eligibility byte | `$01C244` `move.b $d(a1)` → **`A4-$5A12[]`** | Stored from rolled-stat block +0x0D. |
| Class mask AND | **`$01BC8A`** | **`A4-$67F9[class]`** (`data_hunk` @ file **`0x1805`**: `80 40 20 10 08 04 02 01 …`) AND **`A4-$5A12[stat]`**; ineligible → `'-'`. |
| Display | **`$01BC8A`** | Stat names/values + class digit column. |

SDL logic: `mm2_create_roll_stats()` + `mm2_create_class_eligible()` in `EXTRACTED/decomp/mm2_create_character.c` (minimum-stat rules FAQ-cross-checked vs retail `roster.dat`).

### Main input loop — **`$01C25A`**

| Key | ASCII | Handler |
|-----|-------|---------|
| **A–F** | `$41`–`$46` | Exchange-stat family (`$01BE44` / `$01BC26` / `$01B6E0` by step). |
| **G** | `$47` | Proceed / pay (`JSR -$7E0C`, then **`$01BDD6`** gold @ roster **`+$66`**). |
| **1–8** | `$31`–`$38` | Class pick → **`$01C3DE`** (sets **`A4-$5A3E`**, **`A4-$5A3A`**). |
| **Esc** | `$1B` | Exit loop. |
| Reroll | (step **`A4-$5AD0==2`**) | **`$01B646`** re-applies race-adjusted rolls. |

Post-class steps ( **`A4-$5AD0`**: 5=race **`$01BC26`**, 6=sex/name **`$01B6E0`**, 4=alignment copy @ **`$01C1C0`**) then name entry @ **`$01BBEE`** → **`$01C3DE`** + save.

### Dice hand / border

| Item | ASM | Cells (row, col, w, h) |
|------|-----|-------------------------|
| Red frame | **`JSR -$809E`** @ **`$01BE06`** | **(19, 14, 19, 7)** |
| Asset | **`throw.32`** | Loaded via resource table; SDL blit @ `AmigaCharacterUiLayout.h` **`kCreateDice*`** |

### Obfuscated name tables

| Table | A4 offset | Use |
|-------|-----------|-----|
| Class abbrev | **`-$86D8`** | Roster list (`$0552`); create screen uses longer strings via **`JSR -$7BE4`** / **`-$5ACE`**. |
| Race | **`-$86F8`** | Runtime decode (**exact XOR/not pinned**). |
| Alignment | **`-$870C`** | Same. |

SDL port uses plain ASCII enum names until decode is traced.

### Record write + save

| Step | Address | Fields |
|------|---------|--------|
| Build roster ptr | **`$01C3DE`** | **`JSR -$7F20`** → **`A4-$5A3E`**. |
| Finalize stats | **`$01B646`**, **`$01BE44`** | Copies into record bytes **`$10`–`$15`**, **`$27`**, **`$6B`–`$73`**. |
| Defaults | retail `roster.dat` | Age **18**, food **10**, gold **200**, town **1**, level **1**, HP/SP from class + END / INT\|PER tables. |
| Persist | **`JSR -$823C`** family | SDL: `mm2_roster_save_file(data_dir/roster.dat, …)`. |

Implementation: `game/src/ui/AmigaCharacterUi.cpp` + `mm2_create_character.c`.

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
| Create flow (stat roll → race → align → sex → name) | **Done** (plain names; str.dat decode pending) |

Regenerate symbols: `python tools/harvest_symbols.py --merge && python tools/apply_symbols.py`
