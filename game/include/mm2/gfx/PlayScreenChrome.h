#pragma once
#include "mm2/gfx/ScreenCompositor.h"

namespace mm2::gfx {
/** Red play-screen borders and HUD chrome (ASM 0x54F2 / 0x60B6 / 0x55D8 family). */
void drawPlayScreenChrome(ScreenCompositor &c);
/** Status strip text: 'O' Options, Day=, Year=, Face= (formatter @ 0x5312). */
void drawPlayStatusBar(ScreenCompositor &c, int day, int year, char facing_key);
/** Eight party slots in two columns (draw_party_status_panel @ 0x42DC). */
void drawPlayPartyList(ScreenCompositor &c, int party_count, const char (*names)[16],
                       const int *hp_current, const int *hp_max);
}  // namespace mm2::gfx
