# Time, Era and Calendar System

MM2 ("Gates to Another World") tracks an in-game calendar and supports time
travel between **parallel timelines / eras**. This subsystem is pure runtime
state in the `A4` workspace — it is **not** stored in `attrib.dat`. Content that
"only happens in certain years" is gated by `event.dat` scripts reading this
state.

## Calendar Globals (`A4`-relative)

All confirmed from `EXTRACTED/mm2.capstone.asm`.

| Global | Type | Meaning |
|--------|------|---------|
| `A4-$79B4` | word | Sub-day time accumulator. **256 units = 1 day** (rolls over at `0x100`). |
| `A4-$79B6` | word | **Current era / timeline index, range 0..9.** Time travel switches this. |
| `A4-$79DE[era]` | word[10] | **Day-of-year, 1..180** (one slot per era). |
| `A4-$79CA[era]` | word[10] | **Year counter, capped at 999** (one slot per era). Year 1000 = doomsday. |
| `A4-$798C`, `A4-$798D` | byte | Periodic reset flags, cleared at day 60 / 120 / 180. |

> **Correction (ASM-verified, see `14-game-state-struct.md`):** earlier revisions
> of this table listed `A4-$79B1` as a "month/sign byte" and
> `A4-$79F2/-$79F1/-$79F0` as "month/season display fields". The disassembly
> shows those are **session bytes**, not calendar fields: `-$79B1` (`$864F`) is
> the movement key (`cmpi 'N'/'S'/'E'/'W'`), `-$79F2` (`$860E`) is the
> screen/mode id, and `-$79F0` (`$8610`) is a map coordinate. The month/season
> for display are derived from the day-of-year via the word tables below.

The day array `-$79DE` is exactly `0x14` (20) bytes = **10 words**, ending right
where the year array `-$79CA` begins — i.e. 10 parallel calendars, one per era.

## Time Advance Routine (`0x69FC`)

```text
006a0a  add.w   d0, -$79b4(a4)        ; advance sub-day time
006a0e  cmpi.w  #$100, -$79b4(a4)     ; 256 units == 1 day?
006a14  blt     $6aca                 ;   not yet -> done
006a18  move.w  -$79b6(a4), d0        ; era index
006a20  lea     -$79de(a4), a0        ; day-of-year array
006a24  addq.w  #$1, (a0,d0.l)        ; ++day[era]
...
006a42  cmpi.w  #$3c, -$2(a5)         ; day == 60 ?
006a4a  cmpi.w  #$78, -$2(a5)         ; day == 120 ?
006a52  cmpi.w  #$b4, -$2(a5)         ; day == 180 ?  -> clear -$798c/-$798d
...
006a6e  cmpi.w  #$b4, (a0,d0.l)       ; day[era] > 180 ?
006a76  ...      move.w #$1, (a0,d0.l) ;   new year: day = 1
006a90  lea     -$79ca(a4), a0        ; year array
006a94  cmpi.w  #$3e7, (a0,d0.l)      ; year == 999 ?
006a9c  ...      addq.w #$1, (a0,d0.l) ;   else ++year[era]
```

So: every 256 time units a day passes; every 180 days a year passes; the year
saturates at 999 (the "World Will End In The Year 1000" condition).

## Date Display (`0x631E`)

Formats the current date for the selected era, with inline labels `"Day="`
(string at `0x63BA`) and `"Year="` (string at `0x63BF`):

```text
00631e  move.w  -$79b6(a4), d0        ; era
006326  lea     -$79de(a4), a0
00632a  move.w  (a0,d0.l), -(a7)      ; push day[era]
...     print "Year="
00635e  move.w  -$79b6(a4), d0
006366  lea     -$79ca(a4), a0
00636a  move.w  (a0,d0.l), -(a7)      ; push year[era]
```

## Era Index Clamping

`A4-$79B6` is bounded to `0..9` on the Nature's Gate path (`0x0B112`:
`cmpi.w #$9`; era ≠ 9 counts as a spell fail). Setting era to **8** at
`0x0B15E` is a different branch: when day-of-year ≥ `$96` (150) the spell
forces the 9th century. That is **not** the Wayback return.

Walking / `advanceTimeTick` (`0x6A06`) never writes `-$79B6`.

## Returning from the past (Rest snap-back)

Retail does **not** count days in the past and then auto-return. The only
snap-back is on **world Rest** (`0x19B28`), after the clock advance
(`0x19CEC`, `+$55` sub-day ticks):

```text
019cf6  cmpi.w  #$9, -$79b6(a4)      ; already in 10th century (era 9)?
019cfc  beq     $19d34               ;   yes -> skip
019cfe  move.w  #$3c, -(a7)          ; rng(1, $3C)  inclusive 1..60
019d02  move.w  #$1,  -(a7)
019d06  jsr     -$7bb4(a4)
019d0c  cmp.w   #$a, d0              ; roll < 10 ?
019d10  bge     $19d34               ;   10..60 -> stay in the past
019d12  move.w  #$9, -$79b6(a4)      ; era = 9 (year 900s / "the day you left")
019d18  move.w  #$ff, -(a7)          ; y = $FF
019d1c  move.w  #$ff, -(a7)          ; x = $FF  → attrib 0x0E entry_coord
019d20  clr.w   -(a7)                ; screen = 0 (Middlegate)
019d22  jsr     -$7fda(a4)           ; map load @ 0x1B2A
019d28  move.b  #$1, -$7952(a4)      ; pending event latch
019d2e  move.b  #$1, -$79e4(a4)      ; host map-reload latch
```

So: **15% per Rest** (rolls 1–9 of 1–60) while `era != 9`. On hit, era is
forced back to **9** and the party is placed at Middlegate's spawn square
(the `$FF,$FF` args make `0x1B2A` unpack attrib byte `0x0E`). There is no
"pulled back through time" string — the Amiga disk-swap prompt for town
data is what made it obvious. FAQ: *"To get out of the elemental planes, or
any time zone, just rest a bunch… zapped to Middlegate on the day you left
the tenth century."*

Port: `GameSession::executeRest` after `advanceTimeTick(…, 0x55)`.

The month mapping at `0x0B0EA` walks a 13-entry word table at `A4-$711C`
(`cmpi.w #$d`) and tables at `A4-$7102`/`A4-$70F5` to derive the month/season
display bytes from the day-of-year.

## How "events only in certain years" works

There is no per-era copy of map/attrib data. Instead:

1. The current era index (`-$79B6`) selects which timeline's day/year counters
   are live.
2. `event.dat` scripts gate on the calendar directly:
   - `OP_22` (`0x16A9E`) sets the condition flag if the **current era**
     (`-$79B5`) is in a `[lo,hi]` range.
   - `OP_23` (`0x16ADA`) range-checks the **day-of-year** (`-$79DE[era]`).
   - generic variable loads `OP_17` (`0x165A4`) / `OP_32` (`0x17190`) and the
     16-bit predicate `OP_25` (`0x16B82`) cover the rest.
3. Conditional-skip opcodes `OP_10`/`OP_11` (`0x162B8`/`0x162DC`) then run or
   skip a block based on that flag.

This is the same predicate→skip pattern documented for gold/ticket gates in
`07-event-script-opcodes.md`, applied to the calendar. The in-world switch is
the **Wayback machine** in Castle Pinehurst (loc 57 evt 19 @ `(2,5)` facing
north): `OP_0E` selector `0xCF` → `0x1480A`. After Lord Peabody's Sherman
quest sets record+`$80` bit 1, the prompt is `"What era do you desire (1-8)?"`
(code string at `0x14914`); choices 5–8 write `-$79B6`, then all eight land
on the dest tables at A4-`$6CE4` (X) / `$6CDC` (Y) / `$6CD4` (screen). Without
the quest bit the tile shows `"If you wish to use the wayback machine"` /
`"see Lord Peabody."` (`0x143DE` index `$10`). The tavern rumor `"Time travel
at Pinehurst"` (`str.dat`) is only a hint, not the machine itself.

## Direct attrib ↔ era link (`attrib.dat` byte `0x0F`)

There is also a per-screen era gate baked into `attrib.dat`. Each screen's
record carries an `era_gate` byte at offset `0x0F`. In the event interpreter:

```text
0172b6  move.b  -$79b5(a4), -$2(a5)   ; current era (low byte of -$79b6)
0172bc  move.b  -$560b(a4), d0        ; attrib byte 0x0F of current screen
0172c0  cmp.b   -$2(a5), d0           ; era_gate == current era ?
0172c4  beq     $172ca                ;   yes -> process the event record
0172c6  rts                           ;   no  -> skip
```

So whether a screen's event record fires can depend on which era/timeline the
party is currently in. Combined with the `0x16`/`0x18` transition fields in the
same record, this is the mechanism behind "this level is a different era" and
"this event only happens in certain years". (`-$79B5` is the low byte of the
era word `-$79B6`; era values 0..9 fit in that byte.)

## Supporting strings

From `EXTRACTED/docs/11-str-decoded.txt` and `09-event-decoded-events.txt`:

- `Time / Will / End / In The / Year / 1000`
- `Time travel at Pinehurst`
- `There are only 180 days per year.`
- `... a year has passed.`
- `"... I will only see you once a year."`
