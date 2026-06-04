#pragma once
// Amiga play-screen layout traced from EXTRACTED/mm2.capstone.annotated.asm.
// See EXTRACTED/docs/15-3d-view-and-game-screen.md §4.
//
// Border / chrome ASM (A4-relative thunks):
//   console_box          JSR -$809E(A4)  @ 0x7F68  — red 8×8 glyph frame (glyphs 0x0E..0x15)
//   pixel_line_draw      JSR -$8080(A4)  @ 0x7F80  — 1px red line segments
//   fill_rect            JSR -$807A(A4)  @ 0x7F86  — solid fill (Protection panel backdrop)
//
// Key call sites:
//   play_screen_chrome_init   0x54F2  — first-time HUD chrome (807A fills + 60B6 lines)
//   draw_viewport_red_lines   0x60B6  — three 8080 segments (status-bar column dividers)
//   play_screen_panel_box     0x55D8  — 809E box row=0x11 col=0x26 (Protection header rule)
//   show_command_reference    0x5D54  — 809E + party/command text rows
//   draw_party_status_panel   0x42DC  — nwcp.32 portrait/stats blits (called from 0x5312)
//   play_frame_draw           0x5444  — 5312 party lines → 53A0 viewport (2ECE 3D)
//
// 3D backdrop anchors (view_3d_master @ 0x2ECE): sky @ (8,8), floor @ (8,68), 208×60 each.

namespace mm2::gfx::play_layout {

constexpr int kScreenW = 320;
constexpr int kScreenH = 200;

constexpr int kBorderPx = 2;
constexpr uint8_t kBorderR = 255;
constexpr uint8_t kBorderG = 0;
constexpr uint8_t kBorderB = 0;

// Interior first-person lattice (View3D / 0x2ECE).
constexpr int kViewOriginX = 8;
constexpr int kViewOriginY = 8;
constexpr int kViewW = 208;
constexpr int kViewH = 120;

// Red frame around the 3D window (outer chrome from 0x54F2 / 0x60F4 path).
constexpr int kViewportFrameX = 6;
constexpr int kViewportFrameY = 6;
constexpr int kViewportFrameW = 212;
constexpr int kViewportFrameH = 126;

// Protection / stats column (809E @ 0x55D8 uses row 0x11 ≈ this band).
constexpr int kProtectFrameX = 220;
constexpr int kProtectFrameY = 6;
constexpr int kProtectFrameW = 94;
constexpr int kProtectFrameH = 126;

// Status strip ('O' Options, Day, Year, Face) — 8080 @ 0x60B6 row 0x12 cell dividers.
constexpr int kStatusBarY = 134;
constexpr int kStatusBarH = 10;
constexpr int kStatusColDiv1X = 96;
constexpr int kStatusColDiv2X = 168;
constexpr int kStatusColDiv3X = 248;

// Eight-slot party list (format_party_status_line @ 0x5312, rows in nwcp band).
constexpr int kPartyPanelY = 146;
constexpr int kPartyPanelH = 52;

}  // namespace mm2::gfx::play_layout
