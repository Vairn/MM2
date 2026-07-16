#pragma once

// Text grid: 8×8 px cells (Amiga console @ JSR -$8404 / -$841C family).
namespace mm2::ui::amiga_layout {

constexpr int kCellW = 8;
constexpr int kCellH = 8;

inline int cellX(int col) { return col * kCellW; }
inline int cellY(int row) { return row * kCellH; }

// Red frame via JSR -$809E: font-8 box glyphs 0x0E..0x15 (palette $17 = red on Amiga).
constexpr uint8_t kBorderR = 255;
constexpr uint8_t kBorderG = 0;
constexpr uint8_t kBorderB = 0;

// Title roster session — red frame around the whole screen (matches WinUAE
// screenshot: ~1 cell margin on an NTSC 40x25 text grid = 320x200). The console
// box routine (-$7F62) draws a near-full-screen outline; prior constants placed
// it at (col 27, row 21) which framed only the bottom-right corner.
constexpr int kRosterBorderRow = 1;
constexpr int kRosterBorderCol = 1;
constexpr int kRosterBorderW = 38;   // cols 1..39 -> x 8..312 (8px right margin)
constexpr int kRosterBorderH = 23;   // rows 1..24 -> y 8..192 (8px bottom margin, NTSC 320x200)

// Header sits ABOVE the slot list (centered on the 40-col NTSC grid), matching
// the WinUAE screenshot: "(View All)" / "Characters" + underline at the top.
constexpr int kRosterTextCols = 40;        // NTSC text columns (320/8) for centering
constexpr int kRosterViewAllRow = 0x01;    // "(View All)" embedded in top border row
constexpr int kRosterTitleRow = 0x03;      // $008b8 "Characters"
constexpr int kRosterUnderlineRow = 0x04;  // dashes on their own row beneath

/* LAB_470 @ $0448/$0470: row = (slot % 12) + 5; locate col = (slot>11)*20 + 1
 * then putchar tick/space — letter starts one cell right (2 / 22). */
constexpr int kRosterListRowBase = 5;
constexpr int kRosterListColLeft = 2;
constexpr int kRosterListColRight = 0x16;  // 22
constexpr int kRosterSlotsPerColumn = 12;
constexpr int kRosterSlotCount = 24;
constexpr int kRosterHirelingPageOffset = 0x18;

constexpr int kRosterFooterRow = 0x15;     // $00aec
constexpr int kRosterFooterCol = 0x02;

// Title sheet modal — same full-screen red frame as the roster list (NTSC 40x25).
constexpr int kSheetBorderRow = 1;
constexpr int kSheetBorderCol = 1;
constexpr int kSheetBorderW = 38;
constexpr int kSheetBorderH = 23;   // rows 1..24 -> y 8..192 (NTSC 320x200)

constexpr int kSheetHeaderRow = 0x01;      // character name embedded in top border row
constexpr int kSheetHeaderCol = 0x02;

// Three-column stat block (title sheet path; matches WinUAE screenshot).
constexpr int kSheetStatRowBase = 0x04;    // one row down from border (8px)
constexpr int kSheetStatColLeft = 0x02;
constexpr int kSheetStatColMid = 0x09;     // HP=/SP=/AC=/Thievery/skills
constexpr int kSheetStatColSlash = 0x12;   // /max, SL=
constexpr int kSheetStatColCost = 0x18;    // Cost/Gold, Gems, Food
constexpr int kSheetStatColRight = 0x1a;   // Age, Exp, Cond=

// Equipped/backpack block + footer compressed to fit the NTSC 25-row grid
// (stats end at row 11; footer must stay inside the row-24 border bottom).
constexpr int kSheetDividerRow = 0x0c;     // 12
constexpr int kSheetEquipRowBase = 0x0d;   // 13 -> rows 13..18 (6 item slots)
constexpr int kSheetEquipCol = 0x02;
constexpr int kSheetBackpackCol = 0x14;    // 20
constexpr int kSheetBackpackHeaderCol = kSheetBackpackCol;

constexpr int kSheetFooterRow1 = 0x14;     // 20 — command row 1 ($8FF0)
constexpr int kSheetFooterRow2 = 0x15;     // 21 — command row 2 ($9017)
constexpr int kSheetFooterCmdRow3 = 0x16;  // 22 — command row 3 ($903E)
constexpr int kSheetFooterRow3 = 0x18;     // 24 — bottom border row for ESC prompt
constexpr int kSheetFooterCol = 0x02;

// In-game party panel (book.32) — NOT title P; kept for create confirm / combat paths.
constexpr int kPartyPanelBlitX = 0x27 * kCellW;
constexpr int kPartyPanelBlitY = 0x12 * kCellH;

// ---- Choose-party screen — char_create_party_assemble @ $0826 -------------
// String/locate coords from $086A..$0AD0; slot list is LAB_470 ($0448) with
// town filter in +$A(A5). Tick glyph is font $17 (putchar -$7C62), not DIY pixels.
constexpr int kPartyTextCols = 40;
constexpr int kPartyOtherTownsRow = 0x01;     // $089A col $02 "Other Towns"
constexpr int kPartyOtherTownsCol = 0x02;
constexpr int kPartyTownKeysRow = 0x02;       // $08B2 col $02 " '1' - '5'"
constexpr int kPartyTownKeysCol = 0x02;
constexpr int kPartyPartyLabelRow = 0x01;     // $08CA col $1F "PARTY"
constexpr int kPartyPartyLabelCol = 0x1F;
constexpr int kPartyCountRow = 0x02;          // $08E2 col $1D "C=  / H="
constexpr int kPartyCountCol = 0x1D;
constexpr int kPartyFullRow = 0x04;           // $09D0 col $0A "*** Party is Full ***"
constexpr int kPartyFullCol = 0x0A;

constexpr int kPartyListRowBase = 0x05;       // LAB_470: rows 5..16
constexpr int kPartyListColLeftCheck = 0x01;  // locate col 1 / 21, then glyph $17 or $20
constexpr int kPartyListColLeft = 0x02;       // letter starts here
constexpr int kPartyListColRightCheck = 0x15; // 21
constexpr int kPartyListColRight = 0x16;      // 22
constexpr int kPartySlotsPerColumn = 12;
constexpr int kPartySlotCount = 24;
constexpr uint8_t kPartyCheckGlyph = 0x17;    // same mark as combat front-rank

constexpr int kPartyFooterViewRow = 0x12;     // $086A col $0C "'A' - 'X' to View"
constexpr int kPartyFooterViewCol = 0x0C;
constexpr int kPartyFooterAddRow = 0x13;      // $0882 col $05 "(Ctrl) 'A' - 'X'…"
constexpr int kPartyFooterAddCol = 0x05;
constexpr int kPartyFooterHireRow = 0x15;     // $0AC4 col $02 "'Space' for "
constexpr int kPartyFooterHireCol = 0x02;
constexpr int kPartyFooterExitCol = 0x1B;     // $09A0 "'Z' to exit"
constexpr int kPartyFooterEscRow = 0x18;

// Highlight (inverse-video) colors for "*** Party is Full ***".
constexpr uint8_t kPartyHiliteR = 220;
constexpr uint8_t kPartyHiliteG = 220;
constexpr uint8_t kPartyHiliteB = 220;

// ---- Create-character stat roll — overlay ASM $280BA ----
// Window 4 ($5312 tables @ A4-$745C…) = cells (0,0)-(39,23): frame on the
// outermost cells, "(Create New Characters)" printed at (col 10, row 0) over
// the top border. The throw.32 tableau (px 8..311 × 8..79 = cells 1..38 ×
// rows 1..9) fits exactly inside the frame.
constexpr int kCreateBorderRow = 0;
constexpr int kCreateBorderCol = 0;
constexpr int kCreateBorderW = 40;
constexpr int kCreateBorderH = 24;

constexpr int kCreateHeaderRow = 0;          // $2810A locate(0xA, 0)
constexpr int kCreateHeaderCol = 0x0a;       // "(Create New Characters)"

constexpr int kCreateStatRowBase = 0x0c;     // row 12 — below 72px tableau (y=16..88)
constexpr int kCreateStatColLetter = 0x02;   // "A -"
constexpr int kCreateStatColName = 0x06;     // stat name
// Left panel only — must stay left of kCreateClassDigitCol (26). ASM $01BC8A prints
// stat value via -$7BDE at col $22, but the right class column starts at $1A on the
// same rows; WinUAE keeps stat '=' / value in cols 20–22 so the two panels don't collide.
constexpr int kCreateStatColEquals = 0x14;       // col 20
constexpr int kCreateStatColValueSpace = 0x15;   // col 21 — ' ' after '='
constexpr int kCreateStatColValue = 0x16;        // col 22

// Right panel — stat-roll class list (digit / " - " / name); ASM digit path $01BD66
// uses col $1F for eligibility marker on stat rows, full class names at $01BD70+.
constexpr int kCreateClassDigitCol = 0x1a;   // col 26
constexpr int kCreateClassSepCol = 0x1b;     // col 27 — " - "
constexpr int kCreateClassNameCol = 0x1e;    // col 30

// Progress + name entry (post class pick): same right panel cols as class list.
// Labels are 6 chars right-padded so '=' / ':' align (WinUAE name screen).
constexpr int kCreateProgressLabelCol = 0x1a; // col 26 — "Class=", " Race=", …
constexpr int kCreateProgressValueCol = 0x21; // col 33 — value after "= "
constexpr int kCreateRightPanelCol = kCreateProgressLabelCol;
constexpr int kCreateRightPanelWidth = 16;
// Name field scrolls: roster allows 11 chars but only 5 fit before the cursor at col 38.
constexpr int kCreateNameMaxLen = 10;            // create flow name entry (roster holds 11)
constexpr int kCreateNameCursorMaxCol = kCreateBorderCol + kCreateBorderW - 1; // col 38
constexpr int kCreateNameVisibleChars = kCreateNameCursorMaxCol - kCreateProgressValueCol; // 5

constexpr int kCreateTextCols = 40;          // NTSC 320/8 — for centered prompts

constexpr int kCreatePromptRow1 = 0x14;      // step prompt line 1
constexpr int kCreatePromptRow2 = 0x15;      // step prompt line 2 (stat roll only)
constexpr int kCreatePromptRow3 = 0x16;      // step prompt line 3 (stat roll only)
constexpr int kCreateEscRow = 0x18;          // bottom border row for ESC prompt
constexpr int kCreatePromptCol = 0x02;

// throw.32 dice-throw animation — overlay ASM $27096 (traced against the real
// binary; the old LAB_551A/LAB_5632/LAB_60DE notes described in-game chrome, not
// this screen). Per loop iteration i = 0..10:
//   i==0: SetAPen(0) ($21704) + FillRect (8,8)-(311,79) ($217BA)   [hidden buffer]
//   BlitBob(throw.32, i, x = xtab[i]-1, y = ytab[i]+8) ($2203C — opaque, minterm $C0)
//   i==0: LoadRGB4 palette refresh ($23ED2)
//   raster-wait line 79 ($22E3A), copy rect (8,8)-(311,79) hidden→front ($21ED8)
//   Delay(75 ms → 3 ticks = 60 ms) ($22B4A)
// The region is cleared once; frames accumulate on top of each other and the
// final pile-up stays on screen after the loop (there is no rest-frame redraw,
// and nothing else — no highlights, no die-value text — is drawn in the region).
// xtab @ A4-$6344 = {8,43,61,148,195,221,248,264,264,241,229}; ytab @ A4-$632E = 0s.
constexpr int kCreateThrowClearX = 8;
constexpr int kCreateThrowClearY = 8;
constexpr int kCreateThrowClearW = 304;  // x 8..311
constexpr int kCreateThrowClearH = 72;   // y 8..79
constexpr int kCreateThrowFrameCount = 11;
constexpr int kCreateThrowFrameX[kCreateThrowFrameCount] = {
    7, 42, 60, 147, 194, 220, 247, 263, 263, 240, 228};  // xtab[i] - 1
constexpr int kCreateThrowFrameY = 8;                     // ytab[i] + 8
constexpr int kCreateThrowAnimStepTicks = 4;  // Delay(3/50 s) = 60 ms/frame @ 60 Hz tick

// Name-entry cursor @ read_key_with_spinner / A4-$77BC table (4-char spin).
constexpr int kCreateNameCursorStepTicks = 4; // ~15 Hz @ 60 Hz tick
constexpr char kCreateNameCursorChars[] = {'-', '\\', '|', '/'};
constexpr int kCreateNameCursorFrameCount = 4;

}  // namespace mm2::ui::amiga_layout
