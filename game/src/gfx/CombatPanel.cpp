#include "mm2/gfx/CombatPanel.h"

#include "mm2/Config.h"
#include "mm2/CppStdCompat.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PlayScreenChrome.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#endif

namespace mm2::gfx {

using namespace play_layout;

namespace {

#if MM2_HOST_AMIGA
constexpr uint8_t kPenYellow = MM2_UI_PEN_WARN; /* A4-$7A50 combat/sign border pen */
constexpr uint8_t kPenWhite = MM2_UI_PEN_WHITE;
#endif

/* console_box @ 0x7F68 (809E): font glyphs 0x0E..0x15; combat sets pen A4-$7A50. */
constexpr uint8_t kCombatBoxR = 255;
constexpr uint8_t kCombatBoxG = 204;
constexpr uint8_t kCombatBoxB = 0;

void textAt(ScreenCompositor &c, int col, int row, const char *text, uint8_t r = 255, uint8_t g = 255,
            uint8_t b = 255)
{
#if MM2_HOST_AMIGA
    if (r >= 200 && g >= 200 && b <= 80) {
        c.drawTextPen(col * 8, row * 8, text, kPenYellow);
    } else if (r >= 210 && g >= 210 && b >= 210) {
        c.drawTextPen(col * 8, row * 8, text, kPenWhite);
    } else {
        c.drawText(col * 8, row * 8, text, r, g, b);
    }
#else
    c.drawText(col * 8, row * 8, text, r, g, b);
#endif
}

}  // namespace

void drawCombatViewport(ScreenCompositor &c)
{
    /* Viewport interior: cols 1..25 rows 1..15 (inside the red frame). */
    fillCellRect(c, 1, 1, 25, 15);
}

void drawCombatMonsterList(ScreenCompositor &c, const CombatPanelView &view)
{
    /* Right column monster roster — same band as the Protection panel (cols 0x1B..0x26). */
    fillCellRect(c, 0x1B, 1, 12, 14);

    constexpr int kBoxCol0 = 0x1C;
    constexpr int kBoxRow0 = 2;
    constexpr int kBoxW = 0x0A;   /* cols 0x1C..0x25 inclusive (protection band width) */
    constexpr int kBoxH = 0x0D;   /* rows 2..0x0E inclusive */
    constexpr int kBoxRow1 = kBoxRow0 + kBoxH - 1;
    c.drawConsoleBox(kBoxRow0, kBoxCol0, kBoxW, kBoxH, kCombatBoxR, kCombatBoxG, kCombatBoxB);

    int row = kBoxRow0 + 1;
    for (int i = 0; i < view.monster_line_count && row < kBoxRow1; ++i) {
        const CombatMonsterLine &line = view.monster_lines[i];
        char buf[40];
        if (line.show_checkmark && line.letter != 0) {
            std::snprintf(buf, sizeof(buf), "%c%c %s", static_cast<char>(0x7E), line.letter, line.name);
        } else if (line.letter != 0) {
            std::snprintf(buf, sizeof(buf), "%c %s", line.letter, line.name);
        } else {
            std::snprintf(buf, sizeof(buf), "%s", line.name);
        }
        if (line.status_suffix[0] != '\0') {
            char tmp[48];
            std::snprintf(tmp, sizeof(tmp), "%s %s", buf, line.status_suffix);
            std::snprintf(buf, sizeof(buf), "%s", tmp);
        }
        textAt(c, kBoxCol0 + 1, row++, buf);
    }

    if (view.overflow_more > 0 && row < kBoxRow1) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "+%d more", view.overflow_more);
        textAt(c, kBoxCol0 + 1, row++, buf);
        if (view.overflow_name[0] != '\0' && row < kBoxRow1) {
            textAt(c, kBoxCol0 + 1, row++, view.overflow_name);
        }
    }
}

void drawCombatOptionsBar(ScreenCompositor &c, const CombatPanelView &view)
{
    fillCellRect(c, 1, 0x11, 38, 1);

    if (view.show_party_options) {
        textAt(c, 1, 0x11, "Options: A-Attack B-Bribe H-Hide R-Run");
        return;
    }

    if (view.show_command_options && view.options_for[0] != '\0') {
        char head[48];
        std::snprintf(head, sizeof(head), "Options for: %s", view.options_for);
        textAt(c, 1, 0x11, head);
        textAt(c, 1, 0x12, "A-Attack B-Block R-Run");
        return;
    }

    if (view.message[0] != '\0') {
        textAt(c, 1, 0x11, view.message, 255, 255, 128);
    }
}

}  // namespace mm2::gfx
