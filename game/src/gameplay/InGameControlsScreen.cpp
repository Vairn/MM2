#include "mm2/gameplay/InGameControlsScreen.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"

#include <cstdio>

namespace mm2::gameplay {

namespace {

using namespace mm2::ui::amiga_layout;
using namespace mm2::gfx::play_layout;

/* 0x13CCE: text pen ← A4-$7A50 (pen $12 = yellow on play palette). */
void drawText(gfx::ScreenCompositor &c, int row, int col, const char *text,
              uint8_t r = kUiYellowTextR, uint8_t g = kUiYellowTextG, uint8_t b = kUiYellowTextB)
{
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

void drawOnOff(gfx::ScreenCompositor &c, int row, int col, bool on)
{
    /* Both ON/OFF print under -$7C08 highlight of the active state; remake shows
     * the active token only, still in the yellow text pen. */
    drawText(c, row, col, on ? "ON " : "OFF");
}

static const char *kDisposition[] = {"Inconspicuous", "Average", "Aggressive", "Thrill Seeker"};

}  // namespace

void InGameControlsScreen::render(gfx::ScreenCompositor &c, const GameStateView &gs) const
{
    /* Window -$7C74(9,3,$1E,$14) is x1,y1,x2,y2 (same as Death Strikes / Search),
     * not width/height — cells (9,3)-(30,20) → 22×18.
     * ASM @ 0x13D1A: B-pen ← A4-$7A4C (pen $16 blue fill);
     * @ 0x13D34: A-pen ← A4-$7A50 (pen $12 yellow) for text + frame (doc 43). */
    constexpr int kWinX1 = 9;
    constexpr int kWinY1 = 3;
    constexpr int kWinX2 = 0x1E;
    constexpr int kWinY2 = 0x14;
    constexpr int kWinW = kWinX2 - kWinX1 + 1; /* 22 */
    constexpr int kWinH = kWinY2 - kWinY1 + 1; /* 18 */

    c.fillRect(kWinX1 * 8, kWinY1 * 8, kWinW * 8, kWinH * 8, kUiBlueFillR, kUiBlueFillG,
               kUiBlueFillB, 255);
    c.drawConsoleBox(kWinY1, kWinX1, kWinW, kWinH, kUiYellowTextR, kUiYellowTextG, kUiYellowTextB);

    /* Window-relative layout (doc 43 §5) → absolute cells. */
    auto absCol = [](int rel) { return kWinX1 + rel; };
    auto absRow = [](int rel) { return kWinY1 + rel; };

    drawText(c, absRow(1), absCol(7), "Controls");

    drawText(c, absRow(3), absCol(1), "1) Sounds       /");
    drawOnOff(c, absRow(3), absCol(0x0F), gs.soundsEnabled());

    drawText(c, absRow(4), absCol(1), "2) Walk Beep     /");
    drawOnOff(c, absRow(4), absCol(0x0F), gs.walkBeepEnabled());

    drawText(c, absRow(6), absCol(1), "3) Disposition:");
    const int disp = gs.disposition() < 4 ? gs.disposition() : 0;
    drawText(c, absRow(7), absCol(6), kDisposition[disp]);

    drawText(c, absRow(0x0C), absCol(1), "4) Delay");
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%u", gs.delaySetting());
    drawText(c, absRow(0x0C), absCol(0x0A), buf);

    drawText(c, absRow(0x0E), absCol(1), "Press 1-4 to toggle");
    drawText(c, absRow(0x10), absCol(2), "('ESC' to go back)");
}

void InGameControlsScreen::handleKey(char key, GameStateView &gs)
{
    switch (key) {
    case '1':
        gs.toggleSounds();
        break;
    case '2':
        gs.toggleWalkBeep();
        break;
    case '3':
        gs.cycleDisposition();
        break;
    case '4':
        gs.cycleDelay();
        break;
    default:
        break;
    }
}

}  // namespace mm2::gameplay
