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

constexpr int kRosterListRowBase = 6;      // roster_slot_list_draw LAB_470 @ $0470
constexpr int kRosterListColLeft = 2;      // indented off the left border
constexpr int kRosterListColRight = 20;    // 20
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
constexpr int kSheetStatRowBase = 0x03;
constexpr int kSheetStatColLeft = 0x02;
constexpr int kSheetStatColMid = 0x0e;
constexpr int kSheetStatColRight = 0x1a;

// Equipped/backpack block + footer compressed to fit the NTSC 25-row grid
// (stats end at row 11; footer must stay inside the row-24 border bottom).
constexpr int kSheetDividerRow = 0x0c;     // 12
constexpr int kSheetEquipRowBase = 0x0d;   // 13 -> rows 13..18 (6 item slots)
constexpr int kSheetEquipCol = 0x02;
constexpr int kSheetBackpackCol = 0x14;    // 20
constexpr int kSheetBackpackHeaderCol = kSheetBackpackCol;

constexpr int kSheetFooterRow1 = 0x14;     // 20
constexpr int kSheetFooterRow2 = 0x15;     // 21
constexpr int kSheetFooterRow3 = 0x18;     // 24 — bottom border row for ESC prompt
constexpr int kSheetFooterCol = 0x02;

// In-game party panel (book.32) — NOT title P; kept for create confirm / combat paths.
constexpr int kPartyPanelBlitX = 0x27 * kCellW;
constexpr int kPartyPanelBlitY = 0x12 * kCellH;

// ---- Choose-party screen ( "( N-Town )" inn roster ) ----------------------
// Same full-screen red frame (NTSC 40x25) as the roster list. Layout traced
// from the WinUAE screenshot of the original party-assembly screen.
constexpr int kPartyTextCols = 40;            // NTSC text columns used for centering
constexpr int kPartyTitleRow = 0x02;          // "( 1-Middlegate )" centered
constexpr int kPartyOtherTownsRow = 0x03;     // "Other Towns" (left)
constexpr int kPartyOtherTownsCol = 0x03;
constexpr int kPartyTownKeysRow = 0x04;       // "'1' - '5'" (left)
constexpr int kPartyTownKeysCol = 0x05;
constexpr int kPartyHeaderRow = 0x04;         // "Characters"/"Hirelings" (center)
constexpr int kPartyUnderlineRow = 0x05;      // underline under header
constexpr int kPartyPartyLabelRow = 0x03;     // "PARTY" (right)
constexpr int kPartyCountRow = 0x04;          // "C=x / H=y" (right)
constexpr int kPartyRightCol = 0x1c;          // 28
constexpr int kPartyFullRow = 0x06;           // "*** Party is Full ***" (center)

constexpr int kPartyListRowBase = 0x07;       // rows 7..18 (12 per column)
constexpr int kPartyListColLeftCheck = 0x01;  // checkmark cell (left column)
constexpr int kPartyListColLeft = 0x02;       // "A- Name Cls" (left column)
constexpr int kPartyListColRightCheck = 0x13; // 19 checkmark cell (right column)
constexpr int kPartyListColRight = 0x14;      // 20 (right column)
constexpr int kPartySlotsPerColumn = 12;
constexpr int kPartySlotCount = 24;

constexpr int kPartyFooterViewRow = 0x14;     // 20 "'A' - 'X' to View"
constexpr int kPartyFooterAddRow = 0x15;      // 21 "(Ctrl) 'A' - 'X' to Add/Remove"
constexpr int kPartyFooterHireRow = 0x16;     // 22 "'Space' for Hirelings" / "'Z' to exit"
constexpr int kPartyFooterHireCol = 0x03;
constexpr int kPartyFooterExitCol = 0x1a;     // "'Z' to exit"
constexpr int kPartyFooterEscRow = 0x18;      // 24 — bottom border row for ESC prompt

// Highlight (inverse-video) colors for "*** Party is Full ***".
constexpr uint8_t kPartyHiliteR = 220;
constexpr uint8_t kPartyHiliteG = 220;
constexpr uint8_t kPartyHiliteB = 220;

// ---- Create-character stat roll (WinUAE reference @ ASM $01BC8A / $01C494) ----
// Full-screen red frame; header breaks top border; throw.32 tableau (304×72) centered
// under header; stats + class list below the graphic.
constexpr int kCreateBorderRow = 1;
constexpr int kCreateBorderCol = 1;
constexpr int kCreateBorderW = 38;
constexpr int kCreateBorderH = 23;

constexpr int kCreateHeaderRow = 0x01;       // "( Create New Characters )"
constexpr int kCreateTableauY = 0x10;        // px y=16 (row 2), below header
constexpr int kCreateTableauW = 304;         // throw.32 frame 0 width

constexpr int kCreateStatRowBase = 0x0c;     // row 12 — below 72px tableau (y=16..88)
constexpr int kCreateStatColLetter = 0x02;   // "A -"
constexpr int kCreateStatColName = 0x06;     // stat name
// Left panel only — must stay left of kCreateClassDigitCol (26). ASM $01BC8A prints
// stat value via -$7BDE at col $22, but the right class column starts at $1A on the
// same rows; WinUAE keeps stat '=' / value in cols 20–22 so the two panels don't collide.
constexpr int kCreateStatColEquals = 0x14;   // col 20
constexpr int kCreateStatColValue = 0x16;    // col 22

// Right panel — stat-roll class list (digit / " - " / name); ASM digit path $01BD66
// uses col $1F for eligibility marker on stat rows, full class names at $01BD70+.
constexpr int kCreateClassDigitCol = 0x1a;   // col 26
constexpr int kCreateClassSepCol = 0x1b;     // col 27 — " - "
constexpr int kCreateClassNameCol = 0x1e;    // col 30

// Progress + name entry (post class pick): same right panel cols as class list.
// Labels are 6 chars right-padded so '=' / ':' align (WinUAE name screen).
constexpr int kCreateProgressLabelCol = 0x1a; // col 26 — "Class=", " Race=", …
constexpr int kCreateProgressValueCol = 0x21; // col 33 — value after "= "
// Name field scrolls: roster allows 11 chars but only 5 fit before the cursor at col 38.
constexpr int kCreateNameCursorMaxCol = kCreateBorderCol + kCreateBorderW - 1; // col 38
constexpr int kCreateNameVisibleChars = kCreateNameCursorMaxCol - kCreateProgressValueCol; // 5

constexpr int kCreateTextCols = 40;          // NTSC 320/8 — for centered prompts

constexpr int kCreatePromptRow1 = 0x14;      // step prompt line 1
constexpr int kCreatePromptRow2 = 0x15;      // step prompt line 2 (stat roll only)
constexpr int kCreatePromptRow3 = 0x16;      // step prompt line 3 (stat roll only)
constexpr int kCreateEscRow = 0x18;          // bottom border row for ESC prompt
constexpr int kCreatePromptCol = 0x02;

// throw.32: frame 0 = 304×72 rest tableau (bar + fist); frames 1–10 = right-anchored throw sprites.
// Create-char draw @ ASM $0054F2: one blit of A4-$7A51 at y=$12, x=$27 (col 39).
// During reroll anim: LAB_60B6 orange bar fills @ y=$10 only (no frame-0 underlay); A4-$7A52
// indexes die text, not a second sprite layer.
constexpr int kCreateThrowBlitCol = 0x27;
constexpr int kCreateThrowBlitY = 0x12;       // px y=18 — sole sprite blit row ($54F2)
constexpr int kCreateThrowBarY = 0x10;        // px y=16 — LAB_60B6 fill rects during anim

constexpr int kCreateThrowAnimStepTicks = 4;  // ~15 Hz hand advance @ 60 Hz tick
constexpr int kCreateThrowAnimSteps = 15;     // A4-$7A52 runs 0..$0F during reroll ($01C02)

// Name-entry cursor @ read_key_with_spinner / A4-$77BC table (4-char spin).
constexpr int kCreateNameCursorStepTicks = 4; // ~15 Hz @ 60 Hz tick
constexpr char kCreateNameCursorChars[] = {'-', '\\', '|', '/'};
constexpr int kCreateNameCursorFrameCount = 4;

}  // namespace mm2::ui::amiga_layout
