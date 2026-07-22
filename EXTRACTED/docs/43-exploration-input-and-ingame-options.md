# Exploration Input, In-Game Options/Protect Panels & Character-Sheet Entry (ASM trace)

Traced from `EXTRACTED/mm2.capstone.annotated.asm` (code-hunk PC offsets) and the data
hunk `EXTRACTED/hunks/mm2_data_00.bin` (A4 = data_base + `0x7FFE`; thunk targets resolved
via `EXTRACTED/tmp_mm2_thunk_map.txt` and direct `4EF9` stub decode).

Scope: **non-combat play mode** (town / dungeon / outdoor). The combat command bar
(`A/F/S/C/U/B/R/E/V`) is a *separate* dispatcher at `0x11866`/`0x1175C` — see doc 17.
This doc covers the exploration dispatcher at **`$1482`** and everything it reaches.

A4 thunk notation: `JSR -$xxxx(A4)` uses the capstone signed form; the raw encoded word
is `0x10000 - 0xxxxx` (e.g. `-$7E00(A4)` = raw `8200`, the form IRA/doc 39 print).

---

## 1. Input pipeline (play session loop `LAB_F1C` @ `$0F1C`)

`game_world_boot` (`LAB_F1C`) is the whole interactive play session; the scheduler
"hub" `$1280` is fall-through code inside it. Per iteration (loop top `$0FAC`):

| Step | Address | Notes |
|------|---------|-------|
| Forced search | `$1274`–`$1286` | if `-$794C(A4) == $FE` → `JSR $4800` (auto **Search** — post-combat treasure pickup), counter incremented |
| Shortcut re-dispatch | `$1062`–`$1090` | if previous key was `C/M/O/P/Q` → skip encounter step, jump `$1288` |
| Random encounter step | `$1094`–`$1256` | tile-class lookup `-$55BA(A4)[y*16+x]` → `-$7DF4` (`0x9524`); rng `-$7BB4` (`0x24C74`); fills battle slot array `A4-$11DE` |
| Pending event scan | `$1258`–`$1272` | if tile byte `-$55D6 >= $80` **and** latch `-$7952` set → clear latch, `JSR -$7D3A` (`event_tile_scanner 0x175E2`) |
| Key read (indoor) | `$12A2`–`$12B8` | `-$79E2(A4)==0`: pump `-$7C3E(0)` + `JSR -$7D0A` → **`0x1E9CE`** (indoor read; internals untraced — OPEN GAP) |
| Key read (outdoor) | `$12BA` | `-$79E2(A4)!=0`: `JSR -$7BD2` → **`0x22586`** raw keyboard read |
| Translate | `$12C8`–`$12D2` | `JSR -$7B78` → **`0x2408E`** = `toupper()` (`a-z` → `A-Z`; `$F0..$F3` pass through) |
| Exit-flag event | `$12D6` | if `-$7950(A4)` set → `JSR -$7D40` (`0x171AC`) |
| Dispatch | `$12EA`–`$12F0` → `$1482` | subtract-cascade on key code |
| Loop tail | `$14F2`–`$1508` | `JSR -$7F1A` (`0x47A2`, gate predicate — untraced); if `-$4F4E(A4)` set (position/context changed) → set event latch `-$7952 = 1` + `JSR $5382` |
| Session exit | `$150A`–`$1514` | `-$6(A5)` set (Ctrl-Q confirmed) → `UNLK`/`RTS` |

### Raw key → `$F0..$F3` movement codes (`0x22586` read, cascade @ `$22698`)

Amiga rawkey scancodes are mapped at `$2264E`–`$2266C`:

| Rawkey | Key | Code | Meaning |
|--------|-----|------|---------|
| `$4F` cursor ←, `$2D` KP4 | Left | **`$F0`** | Turn Left |
| `$4E` cursor →, `$2F` KP6 | Right | **`$F1`** | Turn Right |
| `$4C` cursor ↑, `$3E` KP8 | Up | **`$F2`** | Forward |
| `$4D` cursor ↓, `$1E` KP2 | Down | **`$F3`** | Move Back |

Rawkeys `< $4B` go through ASCII table `A4-$613D`; F1–F10 (`$50..$59`) are ignored.

---

## 2. Exploration key table (dispatcher `$1482`)

The cascade compares the translated key (long in D0). Confirmed mapping:

| Key | Handler | Target (via thunk) | Action | A4 fields touched |
|-----|---------|--------------------|--------|-------------------|
| `$11` (Ctrl-Q) | `$12F4` | inline | "**Quit without game save (y/n)?**" (string `$1524` on status line via `-$7EC0`; key read `-$7F98('N','y')` + toupper, ESC ⇒ `N`). `Y` → sets `-$6(A5)` → session loop exits | `-$79E5` cleared |
| **B** | `$1394` | `-$7DC4` → **`0x9B48`** | **Bash Door** (§8.1) | `-$55BA/-$55D6/-$55D7/-$55D8`, party HP |
| **C** | `$139C` | `-$7D5E` → **`0x13CCE`** | **Controls** screen (Sounds/Walk Beep/Disposition/Delay) (§5) | `-$79B0/-$79AF/-$79AE/-$79AD` |
| **D** | `$13A8` | `-$7D52` → **`0x141F4`** | **Dismiss** hireling (§8.2) | `-$796A[]`, `-$7950` |
| **E** | `$13B0` | `-$7CC8` → **`0x20F58`** | **Exchange** party order (§8.3) | `-$796A[]`, `-$7959` |
| **M** | `$13BC` | `-$7FC8` → **`0x223A`** | **Auto-map** (overland/area map view; doc 03 §`LAB_24C4`) | `-$86B0` ESC flag |
| **O** | `$13C8` | `JSR $5D54` (PC-rel) | **Options panel** — only when `-$79B2 == 1` (Protect panel currently shown) (§4) | `-$79B2 := 0` |
| **P** | `$13EA` | `-$7EAE` → **`0x5E28`** | **Protect panel** — only when `-$79B2 == 0` (Options shown) (§4) | `-$79B2 := 1` |
| **Q** | `$140A` | `-$7E00` → **`0x907A`** (key=`'Q'`) | **Quick Ref** party table (§7.4) | — |
| **R** | `$141C` | `-$7D2E` → **`0x19E20`** | **Rest** (§8.4); then `$54B6` frame redraw | `-$7950`, `-$7952:=1`, `-$79E5:=1` |
| **S** | `$1434` | `JSR $4800` (PC-rel) | **Search** current tile (§8.5) | `-$3F1C[]/-$3F10/-$3F12`, `-$79E5` |
| **U** | `$143C` | `-$7CCE` → **`0x20CA2`** | **Unlock** door ahead (§8.6) | collision tables, `-$7950` |
| `$F0`/`$F1` | `$1364` | `JSR $5838` | **Turn left/right** (§3); sets event latch `-$7952 := 1` (facing-conditional events re-scan) | `-$79B1` facing, `-$55D7/-$55D8`, `-$79E4` |
| `$F2`/`$F3` | `$137C` | `JSR $5816` then `-$7E72(1)` → **`0x69DC`** | **Step forward/back** + 1-minute time tick (§3) | `-$79F1/-$79F0` coords, `-$79B4` time, `-$79AB` light |
| **1**..**8** | `$1444` (default) | `-$7E00` → **`0x907A`** (key=digit) | **View Character** N (digit ≤ `'0' + -$795A` party count) (§6) | see §6 |
| other | `$1444` | — | ignored | — |

Notes:

- **There is no `V` key in exploration** — characters are viewed with digits `1..N`
  (Options panel lists it as `# View Char`). `V` exists only on the combat bar, where
  it is converted to the active character's digit and funneled into the *same* thunk
  (`$11B6E`: `key = '1' + -$4F5(A4)`, then `JSR -$7E00` @ `$11B88`).
- **There is no top-level Cast key in exploration.** Casting is reached from inside the
  character sheet (key `C` there, §7.2) — entry `0x6E08` (the `0x6E30` anchor is the
  window-handle store inside it).
- This dispatcher also serves the title/continue book menu (`$1062` filters `C/M/O/P/Q`).
  `'P'` (`tst.b -$79B2 / bne skip`) and `'O'` (`cmpi.b #1`) are gated purely by the panel
  state `-$79B2`, not by a title-vs-game flag.

### Options panel text (authoritative key list)

Pointer table **`A4-$741A`** (data-hunk file offset `0xBE4`), 15 strings printed at
column `$1C`, rows 1–15 by `$5D54`:

```
  OPTIONS        ($5CA0)
  \x05 x 11      (separator glyph row, $5CAC)
  ↑ Forward      ($5CB8, char $18)
  ↓ Move Back    ($5CC4, char $19)
  ← Turn Left    ($5CD0, char $1A)
  → Turn Rght    ($5CDC, char $1B)
  B Bash Door    ($5CE8)
  C Controls     ($5CF4)
  D Dismiss      ($5D00)
  E Exchange     ($5D0C)
  Q Quick Ref    ($5D18)
  R Rest         ($5D24)
  S Search       ($5D30)
  U Unlock       ($5D3C)
  # View Char    ($5D48)
```

(`M` Map, `O`/`P` panel toggles and Ctrl-Q are accepted but not listed.)

---

## 3. Movement handlers

| Func | Address | Behavior |
|------|---------|----------|
| Turn dispatch | `$5838` | `$F1`: facing `-$79B1` N→E→S→W (CW); `$F0`: N→W→S→E (CCW). Then `$5636` (facing → collision masks), `-$7E42(0)` (`0x6FB8` **walk beep** / `play_sound_seq`), `-$7FD4` (`0x1D0A`), `-$79E4 := 1` |
| Facing masks | `$5636` | `-$79B1` letter → `-$55D8` (direction bit mask: N=`$C0`, E=`$30`, S=`$0C`, W=`$03`) + `-$55D7` (shift: N=6, E=4, S=2, W=0) |
| Step dispatch | `$5816` | `$F2` → `$56FC` forward; `$F3` → `$5762` back |
| Step forward | `$56FC` | `$9424` passability test (−1 = pass, else obstruction index) → blocked: `$5918(idx)` prints from msg table; clear: facing delta via `$5692` (E:+x, W:−x, N:+y, S:−y), `-$79F1/-$79F0` updated, beep, `-$79E4:=1`, `-$4F4E:=1` (event latch set at loop tail), `$63EE`, `-$7E42(0)` |
| Step back | `$5762` | flips facing (N↔S, E↔W) temporarily, same `$9424` test, restores facing; on pass applies delta |
| Facing → delta | `$5692` | writes `(dx,dy)` bytes through pointer args |
| Time tick | `-$7E72(n)` → `0x69DC` | if `n==1` ∧ tile dark (bit 5 of `-$55D6`) ∧ light `-$79AB > 0` → `light--` + redraw Protect panel (`-$7EAE`). `-$79B4 += n`; ≥ `$100` → day rollover: day counter `-$79DE[]`, year `-$79CA[]` (cap 999), period flags `-$798C/-$798D` cleared at days `$3C/$78/$B4`, weekly hook `$6988`, clock redraw `$62C8` |
| Obstruction msgs | table `A4-$7436` (file `0xBC8`) | 0 `Barrier!` 1 `Solid!` 2 `Locked!` 3 `Not Locked!` 4 `Success!` 5 `Impassable!` 6 `Can't swim!` — printer `$5918` (= thunk `-$7EB4`), prints via status line `$54F2`, sets `-$79E5` |
| Status line | `$54F2` (= thunk `-$7EC0`) | clears row `$12`, draws separators + red border row `$11`, prints message at `(0,$11)` |

Movement event triggering: stepping sets `-$4F4E`, which the loop tail converts into
the latch `-$7952`; turning sets `-$7952` directly (handler `$1364`). Next loop pass
runs `event_tile_scanner` if the tile's `$80` collision event bit is set.

---

## 4. The right-hand panel & what in-game 'O' actually opens

**Answer: in-game `O` redraws the 15-line OPTIONS command list (`$5D54`,
`show_command_reference_panel`). It is *not* the `0x13DEC/0x13F6C` toggle cluster —
those toggles are the *Controls* screen, reached with `C` (§5).**

`A4-$79B2` is the **right-panel mode**, a 3-state toggle:

| Value | Meaning | Status-row hint (row `$11`, col 1) |
|-------|---------|------------------------------------|
| `0` | OPTIONS list shown | `'P' Protect` (`$5E1C`) |
| `1` | PROTECT panel shown | `'O' Options` (`$60AA`) |
| `2` | combat command bar owns the panel (set at `combat_round_loop` `$12A2A`, saved/restored `$12CB8`/`$131B8`; Rest saves/restores too `$1A200`/`$1A24C`) |

- **`O` handler `$13C8`**: requires `-$79B2 == 1` → `JSR $5D54`: red border `-$7F62($1C,1,$26,$F)`,
  prints the 15 strings from `A4-$741A` at col `$1C`, glyph `$0B` markers at `($1B,9)`/`($27,9)`,
  then (because flag==1) bottom chrome `$560A`, prints `'P' Protect` at `(1,$11)`, **clears
  `-$79B2` to 0**.
- **`P` handler `$13EA`**: requires `-$79B2 == 0` → `JSR -$7EAE` → **`0x5E28`** Protect panel:
  - `Light     )` (`$6068`) + value `-$79AB` at row `$A`
  - `Magic     %` (`$6074`) + value `-$79AA` at row `$B`
  - `Forces    %` (`$6080`) + value `-$79A9` at row `$C`
  - conditional rows: `Levitate` (`$608C`, if `-$79A8`), `Walk/Water` (`$6095`, if `-$79A7`),
    `Guard Dog` (`$60A0`, if `-$79A6`)
  - then sets **`-$79B2 := 1`** and prints `'O' Options` at `(1,$11)`.
- **`session_start_refresh $545E`** redraws whichever panel is current
  (`$548A`: flag==1 → `0x5E28`, else `$5D54`).
- The char-sheet dispatcher `0x907A` force-redraws the Protect panel on exit if any
  protection value is non-zero (`$91CA`–`$9212`), since casting inside the sheet can
  change them.
- Doc 15's "status bar shows 'O' Options at x=12" hint corresponds to these
  row-`$11` prompts plus the `$54F2` chrome.

---

## 5. In-game 'C' — Controls screen (`0x13CCE`)

The toggle cluster previously known only from doc 25 (`0x13F6C`) is the body of this
screen. The Options panel labels the key `C Controls`. (The same thunk `-$7D5E` / raw
`82A2` is what doc 39 §1 previously labelled "Create character" — that mapping is wrong;
doc 39 already flags it as a gap. In-game and title `C` both open Controls.)

Layout (window `-$7C74(9,3,$1E,$14)` = x 9, y 3, w 30, h 20; border colors `-$7A4C/-$7A50`):

| Row (y) | Col (x) | Text |
|---------|---------|------|
| 1 | 7 | `Controls` (`$13FF2`) |
| 3 | 1 | `1) Sounds       /` (`$13FFB`) — `ON` @ x `$F` / `OFF` @ x `$12` (`$1404C`/`$1404F`), highlight from `-$79B0` via `-$7C08` |
| 4 | 1 | `2) Walk Beep     /` (`$1400D`) — `ON`/`OFF` (`$14053`/`$14056`) from `-$79AF` |
| 6 | 1 | `3) Disposition:` (`$1401F`) |
| 7–10 | 6 | ` Inconspicuous / Average / Aggressive / Thrill Seeker ` (ptr table `A4-$6D70`, file `0x128E`), current `-$79AE` highlighted |
| `$C` | 1 | `4) Delay` (`$1402F`) — digits `0..9` at x `$A`, current `-$79AD` highlighted |
| `$E` | 1 | `Press 1-4 to toggle` (`$14038`) |
| `$10` | 2 | `('ESC' to go back)` (dynamic ptr `A4-$7874`) |

Keys (read `-$7F68('1','4')`, jump table `$13FB0`):

| Key | Address | Effect |
|-----|---------|--------|
| `1` | `$13F6C` | `BCHG #0, -$79B0` Sounds |
| `2` | `$13F78` | `BCHG #0, -$79AF` Walk Beep |
| `3` | `$13F84` | `-$79AE = (-$79AE + 1) % 4` Disposition |
| `4` | `$13F9A` | `-$79AD = (-$79AD + 1) % 10` Delay |
| ESC | `$13FD0` | close window `-$7C50`, exit |

Only changed rows are redrawn (per-row dirty flags `-$6..-$C(A5)`).

---

## 6. View Character — exploration entry (`0x907A`, thunk `-$7E00` / raw `8200`)

Called with the pressed key as word arg. Same thunk used by combat `V` (`$11B88`).

```
0x907A(key):
  save screen_mode -$79F2, coords -$79F1/-$79F0
  loop:
    key == 'Q'              -> JSR $595C  (Quick Ref, §7.4)
    key in '1'..'0'+count   -> key = $8C8A(key-'1')   ; per-char sheet session,
                                                       ; returns NEXT key (digit chains
                                                       ; directly to another character)
    else                    -> read key via -$7F68('1','q')
  until key == ESC ($1B)
  exit sync:
    screen_mode changed  -> -$79F2=$FF, reload map ctx -$7FDA(mode,x,y), -$4F4E=1
    coords changed       -> walk beep, -$79E4=1, -$4F4E=1   (Teleport/Town Portal etc.)
    if -$79B2 != 2 and any protect var set -> -$79B2=0, JSR -$7EAE (redraw Protect panel)
    if -$79E4 and tile event bit -> -$7952=1 (event latch)
```

### 6.1 Per-character sheet session `$8C8A(slot)`

- roster ptr via `-$7F20(slot)` (`0x477E`); display char = `'1'+slot`; roster index word
  from `A4-$796A[slot*2]`.
- Sheet redraw when `-$79E8` set: **`JSR -$7F9E(dispChar, rosterIdx)` → `0x398C`**, which
  computes `A4-$2A3E + idx*$82` and falls into **`character_sheet_draw $39B4`**
  (book.32 chrome, field tables `A4-$8AF8/-$8AE0` — doc 39 §4).
- Condition byte = roster `+$26` (`-$D(A5)`); condition bit 0 with key ≠ `C` sets a
  local block flag (`$8D72`–`$8D86`).

**Non-combat menu** (3 rows at col 1, rows `$14/$15/$16`, printed once):

```
'Q' Quick Ref  'C' Cast    'D' Drop      ($8FF0)
'E' Equip      'G' Gather  'R' Remove    ($9017)
'S' Share      'T' Trade   'U' Use       ($903E)
```

Key switch (`$8ECC`, jump table `$8EA6`, index = key − `'C'`, range `$13`):

| Key | Handler | Target | Notes |
|-----|---------|--------|-------|
| `C` | `$8DE0` | `JSR $6E08(charPtr)` | **Cast** — only if roster `+$23` non-zero (caster), condition `< $80` and `(cond & $70)==0`; after return, if `-$79EA` set the sheet exits (key forced to ESC) |
| `D` | `$8E22` | `JSR $F004(charPtr)` | Drop item; redraw flag `-$79E8:=1` |
| `E` | `$8E36` | `JSR $ED9C(charPtr)` | **Equip**; redraw |
| `G` | `$8E4A` | `JSR $8050(slot)` | Gather (party gold pooling) |
| `R` | `$8E58` | `JSR $EACC(charPtr)` | **Remove** (unequip); redraw |
| `S` | `$8E6A` | `JSR $7DCC(slot)` | Share (gold/gems/food split; `0x6ACE` family divides pooled gold by party count) |
| `T` | `$8E76` | `JSR $E61C(slot)` | **Trade** (item/gold to another character); redraw |
| `U` | `$8E88` | `JSR $E94A(charPtr)` | Use item; redraw; `-$79EA` forces exit (e.g. item teleports) |

Keys `C/D/E/G/R/S/T/U` first clear the message area (`-$7ED8(1)` → `0x5312`).
Exit: ESC or `Q` returns to `0x907A` (`Q` then opens Quick Ref); a digit `1..N`
returns that digit so `0x907A` re-enters `$8C8A` for the other character.

**Combat variant** (`-$79B2 == 2`, `$8F34`): no equip menu; prompt at `($A,$15)` =
`'V' View spell book` (`$9065`). Keys: ESC/`Q` exit, digits switch character, `V` →
border `-$7F62($A,$15,$1D,$15)` + `JSR $6732(charPtr)` (spell-book grid viewer).

### 6.2 Cast flow (`$6E08`, anchor `0x6E30`)

```
$6E08(charPtr):
  save screen_mode/coords
  window -$7C74($A,7,$1D,$13) -> handle stored at -$7A42 (the 0x6E30 anchor)
  JSR $65FA(charPtr)      ; spell-book grid: header 'Spell Book' ($6714),
                          ; 'Lvl 1 2 3 4 5 6 7' ($671F), rows = level 1..9,
                          ; known-spell dots (char $17) via per-level tables
                          ;   A4-$73DE (start index) / A4-$73D5 (count: 7,7,6,6,5,5,4,4,4)
                          ; (the LAB_6622 'picker' anchor is this drawer's header code)
  JSR -$7E12(charPtr) -> 0x79C6  ; spell level+number prompt; returns spell id, -1 = ESC
  if id != -1:
     -$79E9:=1, caster ptr -> -$47CC
     JSR $CD90(id)        ; spell-effect executor (doc 26)
     if -$7958: JSR $6DC6 ; deduct SP (-$3F08) from +$5C and gems (-$3F0A) from +$58,
                          ; set sheet redraw -$79E8
  JSR $CBD2               ; teardown / window close
  exit sync identical to 0x907A (screen-mode / coord change -> map reload, beep, -$4F4E)
```

`$6DA6` (= thunk `-$7E4E`) prints `('ESC' to go back)` at `($B,$17)` — used by the
Ctrl-Q prompt and Dismiss too.

### 6.3 Quick Ref (`$595C`)

Full party table; headers `#     Name    Hit Points  Spell Points` (`$5C52`) and
`# Lvl SL AC Age Gems  Food Condition` (`$5C79`). Per slot (max 8): row `slot+3`
prints `N) name  HP/maxHP …` from roster fields `+$5E/+$74/+$58/+$5A…`; a second block
at rows `slot+$E` prints the level/AC/age/gems/food/condition line.

---

## 7. Other command handlers

### 7.1 `B` Bash Door — `0x9B48` (thunk `-$7DC4`)

> NOTE: the bootstrap symbol names `training_mode_select` (0x9B48) / `training_hp_restore`
> (0x9BCA) are **wrong** — this is the bash-door handler (the `open_inn_lodging` label at
> `0x19EC0` is likewise misplaced inside the Rest strings). Doc/symbol cleanup pending.

- Reads collision byte `-$55BA[y*16+x]`, shifts by facing (`-$55D7`), `& 3` = wall field
  ahead.
- Field 0, or outdoor, or no door/event bits (`-$55D6 & -$55D8 & $55`): falls through to a
  normal forward step (`JSR $5816($F2)` + `-$7E72(1)`).
- Field ≠ 2: `-$7EB4(1)` → `Solid!`.
- Field == 2 (door): `$9BCA` — bash strength = char1 `+$6B` Might (+ char2 if party > 1),
  roll `-$7BB4(10,$6D)` /10, compare vs door strength byte `-$5608(A4)`; success path
  `$9C4C` rolls d100 vs `$33` (trap chance) → `-$7F02` (`0x4B06`) opens, failure prints
  `Locked!`/damage path. (Fine-grained branch semantics partially traced — see gaps.)

### 7.2 `D` Dismiss — `0x141F4` (thunk `-$7D52`)

Prompt template `Dismiss whom (1- )?` (ptr `A4-$6D38`, max digit patched in at `+$10`),
printed via status line `$54F2`. Key `'1'..'0'+count` (`-$7F98`); **only hireling slots**
are dismissable — party slot word `A4-$796A[slot*2] >= $18` (roster indices ≥ 24 are
hirelings). Dismissal via `-$7FAA(slotWord)` → `0x362C`; farewell string `Come back real
soon.` (`$142A6`). ESC exits; `-$7950` modal flag 1→3; exit via `-$7F4A` + `-$7D40`.

### 7.3 `E` Exchange — `0x20F58` (thunk `-$7CC8`)

Two-slot prompt dialog `0x20DE0`: window `-$7C74(9,8,$1E,$D)`, strings
`Exchange (1 - x)` (`A4-$6482`, `$20DC0`) and `with (1 -  )` (`A4-$647E`, `$20DD2`),
max digit patched at `+$E`/`+$A` (`'0' + -$7959`, the low byte of party-count word
`-$795A`). Reads two party slots (`-$7F98('1',max)`), ESC aborts, then swaps the
party-order entries.

### 7.4 `R` Rest — `0x19E20` (thunk `-$7D2E`)

- Tile bit 3 of `-$55D6` set → `Too dangerous!` (`$19EBC`), abort.
- Prompt `Rest here? (Y/N)` (`$19ECB`) — same indoor/outdoor key-read split as the main
  loop; `N` → repaint (`-$7EBA` → `0x560A`), clear `-$7950`.
- `Y` → `$19AD6` (hireling daily-pay check; fail → `Not enough gold - Dismiss hirelings`
  (`$19EDC`)), then `$19D64` and `$19B28` (rest execution / day advance). Dispatcher
  epilogue (`$1420`) runs `$54B6` frame redraw, sets `-$7952` (event latch) and
  `-$79E5`.
- **Traced + ported (2026-07-17):** `GameSession::executeRest()` implements the
  full body — hireling pay/dismiss gate, `-$55D6` bit3 "Too dangerous!" ambush
  check, HP heal-to-max, food consumption, the SP recompute at `0x19C30`
  (`gameplay::syncRosterWorkingLevelFields` + `recomputeRestSpellPoints` — see
  [`06-roster-format.md`](06-roster-format.md) and
  [`57-user-help-trace-residuals.md`](57-user-help-trace-residuals.md) §4), and
  the `0x55`-unit clock advance. `$19AD6`/`$19D64`/`$19B28` are no longer
  "internals untraced" — see `game/src/GameSession.cpp::executeRest` (~line 1274).

### 7.5 `S` Search — `$4800` (PC-relative; also auto-run when `-$794C == $FE`)

Prints `Search...` (`$48A1`) on the status row. Scans pending-loot state
`-$3F1C[0..2]` plus `-$3F10` (long) / `-$3F12` (word); all empty → `Nothing Here!`
(`$48AB`) + `-$79E5 := 1`; otherwise `JSR -$7D1C` → **`0x1B19C`** (treasure/encounter
payoff — untraced).

### 7.6 `U` Unlock — `0x20CA2` (thunk `-$7CCE`)

- Outdoor → exit silently. Wall field ahead must be 2 (door), else exit.
- Lock bit test vs `-$55D6`; not locked → `-$7EB4(3)` `Not Locked!`.
- Char picker `$1AE2E` ("who tries", ESC aborts) → roster `+$1E` (thievery); roll
  d100 (`-$7BB4(1,100)`); roll < `$60` and thievery ≥ roll → `-$7F02` (`0x4B06`,
  clears lock) + `Success!`. Otherwise second d100 vs `-$5607` (trap byte): ≤ →
  `Locked!`; > → trap springs: `-$7D22` (`0x1A9A6`) picks victim, `-$7D28(0,victim)`
  (`0x1A90E`) applies damage, `-$7950 := 3`, `-$7D40` event hook.

### 7.7 `M` Map — `0x223A` (thunk `-$7FC8`)

Existing trace in doc 03 (`LAB_24C4` movement loop, ESC via `-$86B0`).

---

## 8. Thunk quick reference (this doc)

| Thunk (signed) | Raw | Target | Role |
|----------------|-----|--------|------|
| `-$7E00` | `8200` | `0x907A` | view-character dispatcher (digits/Q; combat V) |
| `-$7F9E` | `8062` | `0x398C` | char-sheet draw wrapper → `$39B4` |
| `-$7D5E` | `82A2` | `0x13CCE` | Controls screen |
| `-$7DC4` | `823C` | `0x9B48` | Bash door |
| `-$7D52` | `82AE` | `0x141F4` | Dismiss |
| `-$7CC8` | `8338` | `0x20F58` | Exchange |
| `-$7CCE` | `8332` | `0x20CA2` | Unlock |
| `-$7D2E` | `82D2` | `0x19E20` | Rest |
| `-$7FC8` | `8038` | `0x223A` | Auto-map |
| `-$7EAE` | `8152` | `0x5E28` | Protect panel draw |
| `-$7E72` | `818E` | `0x69DC` | time tick / light drain |
| `-$7E42` | `81BE` | `0x6FB8` | `play_sound_seq` (id 0 = walk beep after move) |
| `-$7E12` | `81EE` | `0x79C6` | spell level+number prompt |
| `-$7E4E` | `81B2` | `0x6DA6` | "('ESC' to go back)" printer |
| `-$7EB4` | `814C` | `0x5918` | status message by index (`A4-$7436`) |
| `-$7EC0` | `8140` | `0x54F2` | status-line printer (row `$11` chrome) |
| `-$7EBA` | `8146` | `0x560A` | bottom chrome redraw |
| `-$7ED8` | `8126` | `0x5312` | party panel line format/clear |
| `-$7EA2` | `815E` | `0x6150` | party status panel redraw |
| `-$7F68` | `8098` | `0x42AA` | read key within range (+ESC) |
| `-$7F98` | `8068` | `0x3E02` | prompt key read (min,max) |
| `-$7F62` | `809E` | `0x42DC` | red border (cells x,y,w,h) |
| `-$7F20` | `80E0` | `0x477E` | party member ptr by slot |
| `-$7FAA` | `8056` | `0x362C` | remove hireling from party |
| `-$7F02` | `80FE` | `0x4B06` | clear lock (unlock/bash success) |
| `-$7D1C` | `82E4` | `0x1B19C` | search payoff (loot/encounter) |
| `-$7D22` | `82DE` | `0x1A9A6` | trap victim pick |
| `-$7D28` | `82D8` | `0x1A90E` | trap damage apply |
| `-$7D0A` | `82F6` | `0x1E9CE` | indoor input read |
| `-$7BD2` | `842E` | `0x22586` | raw keyboard read (+`$F0..$F3` mapping) |
| `-$7B78` | `8488` | `0x2408E` | toupper key translate |
| `-$7FD4` | `802C` | `0x1D0A` | walk beep |
| `-$7C74` | `838C` | `0x21624` | open window (x,y,w,h) |
| `-$7C50` | `83B0` | `0x21C1E` | close window |
| `-$7C62` | `839E` | `0x218EA` | print char (NOT combat fanfare — yaml `-0x7C62` note is for the raw-keyed entry) |
| `-$7BFC` | `8404` | `0x22108` | locate cursor (x,y) |
| `-$7BE4` | `841C` | `0x22376` | print string |
| `-$7BDE` | `8422` | `0x22480` | print number (value,width,pad) |
| `-$7C08` | `83F8` | `0x220BE` | highlight on/off for next print |

## 9. Symbol corrections found

- `0x9B48` "training_mode_select" / `0x9BCA` "training_hp_restore" → actually **bash door**
  (mode select / strength roll). Real training docs live at doc 34 addresses.
- `0x19EC0` "open_inn_lodging" annotation lands inside the Rest handler's string data.
- yaml `a4_thunks` keys mix raw words (`-0x83C2` = `-$7C3E`) and signed values
  (`-0x7C62` note "combat_fanfare" does not match the print-char thunk at signed
  `-$7C62`/raw `839E`). Doc 39 §1 "Create character (`JSR -$82A2`)" is the Controls
  screen thunk (raw `82A2` = `-$7D5E` → `0x13CCE`).

## 10. OPEN GAPS

| Gap | Address to trace |
|-----|------------------|
| Indoor input reader internals (mouse/joystick? returns key codes incl. `$F0..$F3`) | `0x1E9CE` |
| Passability test internals (which obstruction index per tile/flags; swim/barrier rules) | `$9424` |
| Forced-search sentinel writer (`-$794C := $FE`, presumed combat victory) | writer not located |
| Search payoff handler (loot vs fixed encounter) | `0x1B19C` |
| ~~Rest execution: pay check / heal / day advance~~ | **Closed 2026-07-17** — traced + ported, see §7.4 |
| Bash success/trap fine branches; door strength `-$5608`, trap byte `-$5607` semantics | `$9BCA`–`$9CAA`, `0x20D5C` |
| Trap victim/damage helpers | `0x1A9A6`, `0x1A90E` |
| Sheet sub-handlers: Drop/Equip/Remove/Trade/Use/Gather/Share internals | `$F004`, `$ED9C`, `$EACC`, `$E61C`, `$E94A`, `$8050`, `$7DCC` |
| Loop-gate predicate (returns 0 ⇒ skip input handling) | `0x47A2` (thunk `-$7F1A`) |
| Spell number prompt layout/keys | `0x79C6` |
| `-$79E9`/`-$79EA`/`-$7958` cast-state flags exact lifecycle (set sites in `$CD90` family) | doc 26 cross-check |
| Title-menu Create entry (in-game & title `C` both = Controls; create session `$1C684` reached elsewhere) | doc 39 §5 gap stands |

## 11. `game/` Milestone 2 port (movement only)

Implemented in `game/src/gameplay/Movement.cpp` + `PlaySessionInput.cpp`:

- Turn/step dispatch matches `$5838` / `$5816` / `$5692` (facing bundle + coord delta).
- Passability: **first gate only** @ `$9424` (`-$55D6` AND `-$55D8` AND `$55`; bundle-hi
  masks are **not** the same labels as page-1 N/E/S/W bit names — see `mm2_map_codec.h`).
- Screen edge: `0x1D0A` neighbour bytes `attrib` `0x05..0x08`, coord wrap `& $F`, reload via
  `MapWorld::enterScreen` (full `0x1B2A` / `$2546` chain partially stubbed).
- Time tick @ `0x69DC(n=1)`: subday increment + dark-tile light drain (`-$79AB`); day
  rollover @ `0x6A06` is **implemented** (`applyDayRollover` in `Movement.cpp` ~line
  136, called from both turn and step paths). Protect panel redraw @ `0x5E28`
  is still deferred.
- Walk beep is **wired**: `audio::playSoundSeq(0, …)` fires on both turn and step
  (`Movement.cpp` ~lines 320, 384) — this is thunk `-$7E42(0)` → `0x6FB8`, **not**
  `-$7FD4` (`-$7FD4`/`0x1D0A` is the **screen-edge** handler, a separate concern;
  an earlier note conflated the two addresses). Obstruction messages @ `$5918`
  are wired via `obstructionMessage` in `GameSession`'s bash/unlock/step paths.
- SDL maps cursor + keypad only (doc §1 table); WASD not mapped.
