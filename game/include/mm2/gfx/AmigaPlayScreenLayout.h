#pragma once
// Play-screen cells. Thunks: console_box -$809E @ 0x7F68 (glyphs 0x0E..0x15),
// pixel_line -$8080 @ 0x7F80, fill_rect -$807A @ 0x7F86.
// Chrome 0x54F2 / 0x60F4, party 0x6150, clear_cell_rect 0x42DC.
// 3D hood @ 0x2ECE: sky (8,8), floor (8,68), 208×60.

namespace mm2::gfx::play_layout {

constexpr int kScreenW = 320;
constexpr int kScreenH = 200;

constexpr int kBorderPx = 2;
constexpr uint8_t kBorderR = 255;
constexpr uint8_t kBorderG = 0;
constexpr uint8_t kBorderB = 0;

/* Blue modal frames: outdoor init @ 0x2680E stores A4-$7A4C = pen $16 and
 * A4-$7A50 = pen $12 (town.32 / play palette). Controls @ 0x13CCE paints the
 * window with -$7A4C then prints with -$7A50. Death Strikes / Search use
 * -$7A4C via -$7F74 console_box; Victory rules use A4-$7A4D (pen 20) glyph 5.
 * RGB matches town.32 pens $16 / $12 / $14-ish border highlight. */
constexpr uint8_t kUiBlueFillR = 51;   /* pen $16 — Controls window fill */
constexpr uint8_t kUiBlueFillG = 51;
constexpr uint8_t kUiBlueFillB = 119;
constexpr uint8_t kUiBlueBorderR = 0x55; /* brighter frame approx (pens $14/$15) */
constexpr uint8_t kUiBlueBorderG = 0x88;
constexpr uint8_t kUiBlueBorderB = 0xFF;
constexpr uint8_t kUiYellowTextR = 255; /* pen $12 — Controls / OP_06 text+border */
constexpr uint8_t kUiYellowTextG = 255;
constexpr uint8_t kUiYellowTextB = 0;

// Interior first-person lattice (View3D / 0x2ECE).
constexpr int kViewOriginX = 8;
constexpr int kViewOriginY = 8;
constexpr int kViewW = 208;
constexpr int kViewH = 120;

// Red frame around the 3D window — outer glyph border uses cells, not a 2px frame.
// Viewport interior cells (1,1)-(26,15) → px (8,8)-(215,127). Right column (28,1)-(38,15).
constexpr int kViewportFrameX = 8;
constexpr int kViewportFrameY = 8;
constexpr int kViewportFrameW = 208;
constexpr int kViewportFrameH = 120;

// Protection / Options column interior: cols 28..38, rows 1..15.
constexpr int kProtectFrameX = 0x1C * 8;
constexpr int kProtectFrameY = 8;
constexpr int kProtectFrameW = 11 * 8;
constexpr int kProtectFrameH = 15 * 8;

// Eagle/Wizard Eye 5×5 overlay (spell_eye @ 0x1E74, blit loop @ 0x1F7E).
// Dest X = col*0xE + 0xE8; dest Y = 0x3D - row*0xB (row 0 = bottom).
constexpr int kSpellEyeOriginX = 0xE8;
constexpr int kSpellEyeBottomY = 0x3D;

// overland_map_view @ 0x2334: col-0 X=0x36; row-0 Y=0xAC (south / bottom row).
constexpr int kAutomapOriginX = 0x36;
constexpr int kAutomapBottomY = 0xAC;
constexpr int kAutomapOriginY = kAutomapBottomY - 15 * 11; /* row 15 (north) → Y=7 */

// Status strip ('O' Options, Day, Year, Face) — row 0x11 (engine y = 17*8).
constexpr int kStatusBarY = 0x11 * 8;
constexpr int kStatusBarH = 8;
constexpr int kStatusColDiv1X = 0x0C * 8;
constexpr int kStatusColDiv2X = 0x15 * 8;
constexpr int kStatusColDiv3X = 0x1F * 8;

// Eight-slot party list @ 0x6150 — rows 0x13..0x16 (engine y = 19*8).
constexpr int kPartyPanelY = 0x13 * 8;
constexpr int kPartyPanelH = 4 * 8;

// Status + party band chrome (0x60F4 h-lines rows 0x10/0x12; dividers 0x60B6).
// Exploration chrome uses h_line/v_line/outerFrame only — NOT console_box (809E
// is for modal overlays). Interior black fills stay inside cols 1..38.
constexpr int kPartySlotRowBase = 0x13;
constexpr int kPartySlotColLeft = 0x01;
constexpr int kPartySlotColRight = 0x14;
constexpr int kPartySlotClearWidth = 0x13;  // empty slot clear @ 0x6178 (-$7F62)

// Combat HUD — combat_display_refresh @ 0x135BE (narrow hood + wide right band).
constexpr int kCombatDividerCol = 0x0F;          // v_line(0, 0x0E, 0x0F) @ 0x135F6
constexpr int kCombatDividerRowEnd = 0x0E;
constexpr int kCombatViewportCol = 0x01;
constexpr int kCombatViewportWidthCells = 0x0E;  // cols 1..14 (14 cells = 112 px)
constexpr int kCombatViewportHeightCells = 0x0E; // rows 1..14
constexpr int kCombatRightCol = 0x10;          // "D-Delay…" @ 0x13682
constexpr int kCombatRightWidthCells = 0x17;   // cols 0x10..0x26 (23 cells)
constexpr int kCombatHoodBottomRow = 0x0E;     // h_line(0, 0x27, 0x0E) @ 0x135E6
constexpr int kCombatHeaderRuleRow = 0x02;     // h_line(0x0F, 0x27, 0x02) @ 0x13606
constexpr int kCombatPanelLineCol1 = 0x27;     // both h_lines end col 0x27
constexpr int kCombatMonsterRow0 = 0x03;       // slot row = slot + 3 (0x1267A)
constexpr int kCombatMonsterOverflowRow = 0x0D; // "+N Name" @ 0x126A0
// Pre-combat encounter name box @ 0x12DA2: console_box cols 0x16..0x26, row 1.
constexpr int kCombatEncounterBoxCol = 0x16;
constexpr int kCombatEncounterBoxWidthCells = 0x11;
constexpr int kCombatView3DViewportW = kCombatViewportWidthCells * 8;
constexpr int kCombatView3DViewportH = kCombatViewportHeightCells * 8;
constexpr int kCombatViewportBoxHeightCells = 0x0D; /* console_box @ 0x136AA (rows 1..13) */
constexpr int kCombatView3DSkyY = 8;
constexpr int kCombatView3DFloorY = kCombatView3DSkyY + (kCombatView3DViewportH / 2);

/**
 * AGA multi-monster gallery offsets inside the narrow combat hood (presentation
 * upgrade — not retail A4-$7538). Index 0 = back-most; blit low→high.
 * Dest = (kViewOriginX + x, kViewOriginY + y); sprites are then centered in the
 * remaining hood via blitCenteredInViewport when w/h known.
 */
struct AgaCombatSpriteLayout {
    int x;
    int y;
};

constexpr int kAgaCombatSpriteLayoutCount = 4;
constexpr AgaCombatSpriteLayout kAgaCombatSpriteLayout[kAgaCombatSpriteLayoutCount] = {
    {16, 24}, /* back-left */
    {48, 16}, /* back-right */
    {8, 40},  /* front-left */
    {40, 48}, /* front-center */
};

// In-game modal overlays (Quick Ref @ 0x595C, character sheet wrapper @ 0x398C).
// Red frame is play outer frame (-$7F7A → 0x422A), NOT console_box: cols 0..39,
// rows 0..23. Sheet/QR text Locate(1,…) sits inside that frame.
constexpr int kPlayOverlayBorderRow = 0;
constexpr int kPlayOverlayBorderCol = 0;
constexpr int kPlayOverlayBorderW = 40;
constexpr int kPlayOverlayBorderH = 24;

// Quick Ref table — party_roster_list_draw @ 0x5984 (Locate/Print cols).
constexpr int kQuickRefHeaderRow1 = 0x01;
constexpr int kQuickRefHeaderRow2 = 0x0c;
constexpr int kQuickRefDataRow1Base = 0x03;   // slot + 3
constexpr int kQuickRefDataRow2Base = 0x0e;   // slot + 14
constexpr int kQuickRefColIndex = 0x01;
constexpr int kQuickRefColHpSlash = 0x14;     // '/' after live HP
constexpr int kQuickRefColSpCurrent = 0x1b;   // SP current (width-1)
constexpr int kQuickRefColSpSlash = 0x20;     // '/' then SP max
constexpr int kQuickRefColSL = 0x08;
constexpr int kQuickRefColAC = 0x0a;
constexpr int kQuickRefColAge = 0x0e;
constexpr int kQuickRefColGems = 0x12;
constexpr int kQuickRefColFood = 0x18;
constexpr int kQuickRefColCond = 0x1c;

// LAB_38EA field 2/6: '/' + max at col $11 (not $12).
constexpr int kInGameSheetSlashCol = 0x11;

}  // namespace mm2::gfx::play_layout
