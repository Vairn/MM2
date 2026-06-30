#include "mm2/gameplay/InGameControlsScreen.h"

#include "mm2/ui/AmigaCharacterUiLayout.h"

namespace mm2::gameplay {

namespace {

using namespace mm2::ui::amiga_layout;

void drawText(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255, uint8_t g = 255,
              uint8_t b = 255)
{
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

void drawOnOff(gfx::ScreenCompositor &c, int row, int col, bool on)
{
    drawText(c, row, col, on ? "ON " : "OFF", on ? 255 : 180, on ? 255 : 180, on ? 128 : 180);
}

static const char *kDisposition[] = {"Inconspicuous", "Average", "Aggressive", "Thrill Seeker"};

}  // namespace

void InGameControlsScreen::render(gfx::ScreenCompositor &c, const GameStateView &gs) const
{
    /* Window -$7C74(9,3,$1E,$14) per doc 43 §5. */
    c.drawConsoleBox(3, 9, 30, 20, 255, 0, 0);

    drawText(c, 4, 16, "Controls", 255, 255, 128);

    drawText(c, 6, 10, "1) Sounds       /");
    drawOnOff(c, 6, 25, gs.soundsEnabled());

    drawText(c, 7, 10, "2) Walk Beep     /");
    drawOnOff(c, 7, 25, gs.walkBeepEnabled());

    drawText(c, 9, 10, "3) Disposition:");
    const int disp = gs.disposition() < 4 ? gs.disposition() : 0;
    drawText(c, 10, 15, kDisposition[disp], 255, 255, 128);

    drawText(c, 12, 10, "4) Delay");
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%u", gs.delaySetting());
    drawText(c, 12, 20, buf, 255, 255, 128);

    drawText(c, 14, 10, "Press 1-4 to toggle", 180, 180, 180);
    drawText(c, 16, 11, "('ESC' to go back)", 180, 180, 180);
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
