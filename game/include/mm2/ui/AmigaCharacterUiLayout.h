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
constexpr int kRosterViewAllRow = 0x02;    // $00892
constexpr int kRosterTitleRow = 0x03;      // $008b8 "Characters"
constexpr int kRosterUnderlineRow = 0x04;  // dashes on their own row beneath

constexpr int kRosterListRowBase = 6;      // roster_slot_list_draw LAB_470 @ $0470
constexpr int kRosterListColLeft = 4;      // indented off the left border
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

constexpr int kSheetHeaderRow = 0x01;
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
constexpr int kSheetFooterRow3 = 0x16;     // 22
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
constexpr int kPartyListColRightCheck = 0x14; // checkmark cell (right column)
constexpr int kPartyListColRight = 0x15;      // 21 (right column)
constexpr int kPartySlotsPerColumn = 12;
constexpr int kPartySlotCount = 24;

constexpr int kPartyFooterViewRow = 0x14;     // 20 "'A' - 'X' to View"
constexpr int kPartyFooterAddRow = 0x15;      // 21 "(Ctrl) 'A' - 'X' to Add/Remove"
constexpr int kPartyFooterHireRow = 0x16;     // 22 "'Space' for Hirelings" / "'Z' to exit"
constexpr int kPartyFooterHireCol = 0x03;
constexpr int kPartyFooterExitCol = 0x1a;     // "'Z' to exit"
constexpr int kPartyFooterEscRow = 0x17;      // 23 "( 'ESC' to exit game )"

// Highlight (inverse-video) colors for "*** Party is Full ***".
constexpr uint8_t kPartyHiliteR = 220;
constexpr uint8_t kPartyHiliteG = 220;
constexpr uint8_t kPartyHiliteB = 220;

}  // namespace mm2::ui::amiga_layout
