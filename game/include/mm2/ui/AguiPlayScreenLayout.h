#pragma once

// A1200 Agui play HUD layout on the classic 320×200 compositor.
// Viewport hood stays 208×120 so View3D tables remain valid.

namespace mm2::ui::agui_layout {

constexpr int kScreenW = 320;
constexpr int kScreenH = 200;

constexpr int kViewOriginX = 8;
constexpr int kViewOriginY = 8;
constexpr int kViewW = 208;
constexpr int kViewH = 120;

constexpr int kFrameX = 4;
constexpr int kFrameY = 4;
constexpr int kFrameW = 216;
constexpr int kFrameH = 128;

constexpr int kProtectGemSize = 6;
constexpr int kProtectGemInset = 2;

constexpr int kFaceGemW = 18;
constexpr int kFaceGemH = 10;
constexpr int kFaceGemX = kViewOriginX + (kViewW - kFaceGemW) / 2;
constexpr int kFaceGemY = kViewOriginY + kViewH - kFaceGemH - 2;

constexpr int kRailX = 228;
constexpr int kRailY = 4;
constexpr int kRailW = 88;
constexpr int kRailH = 128;
constexpr int kRailBtnCount = 8;
constexpr int kRailBtnH = 15;
constexpr int kIconSize = 12;

constexpr int kMsgX = 4;
constexpr int kMsgY = 134;
constexpr int kMsgW = 216;
constexpr int kMsgH = 14;

constexpr int kPartyX = 4;
constexpr int kPartyY = 150;
constexpr int kPartyW = 252;
constexpr int kPartyH = 46;
constexpr int kPartySlots = 8;
constexpr int kPartySlotW = 31;
constexpr int kFaceSize = 28;
constexpr int kBarH = 2;

constexpr int kDpadX = 268;
constexpr int kDpadY = 150;
constexpr int kDpadW = 48;
constexpr int kDpadH = 46;
constexpr int kDpadCell = 14;

}  // namespace mm2::ui::agui_layout
