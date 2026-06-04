#include "mm2/gfx/PlayScreenChrome.h"

#include "mm2/CppStdCompat.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"

namespace mm2::gfx {

namespace {

using namespace play_layout;

void drawRedFrame(ScreenCompositor &c, int x, int y, int w, int h)
{
    c.fillRect(x, y, w, kBorderPx, kBorderR, kBorderG, kBorderB);
    c.fillRect(x, y + h - kBorderPx, w, kBorderPx, kBorderR, kBorderG, kBorderB);
    c.fillRect(x, y, kBorderPx, h, kBorderR, kBorderG, kBorderB);
    c.fillRect(x + w - kBorderPx, y, kBorderPx, h, kBorderR, kBorderG, kBorderB);
}

void drawRedHLine(ScreenCompositor &c, int x0, int x1, int y)
{
    if (x1 < x0) {
        const int t = x0;
        x0 = x1;
        x1 = t;
    }
    c.fillRect(x0, y, x1 - x0 + 1, 1, kBorderR, kBorderG, kBorderB);
}

void drawRedVLine(ScreenCompositor &c, int x, int y0, int y1)
{
    if (y1 < y0) {
        const int t = y0;
        y0 = y1;
        y1 = t;
    }
    c.fillRect(x, y0, 1, y1 - y0 + 1, kBorderR, kBorderG, kBorderB);
}

}  // namespace

void drawPlayScreenChrome(ScreenCompositor &c)
{
    // 3D viewport frame (draw_viewport_red_lines @ 0x60B6 is invoked from 0x561C).
    drawRedFrame(c, kViewportFrameX, kViewportFrameY, kViewportFrameW, kViewportFrameH);

    // Protection column (809E @ 0x55D8 + fill_rect @ 0x807A in play_screen_chrome_init @ 0x54F2).
    drawRedFrame(c, kProtectFrameX, kProtectFrameY, kProtectFrameW, kProtectFrameH);
    c.fillRect(kProtectFrameX + kBorderPx, kProtectFrameY + 10, kProtectFrameW - 2 * kBorderPx, 36, 120, 130, 150);
    c.drawTextShadow(kProtectFrameX + 8, kProtectFrameY + 2, "Protection", 255, 255, 255);
    drawRedHLine(c, kProtectFrameX + kBorderPx, kProtectFrameX + kProtectFrameW - kBorderPx - 1,
                 kProtectFrameY + 48);

    // Status bar outer frame + column dividers (8080 @ 0x60B6: y row 0x12, x 0x10/0x15/0x1F cells).
    drawRedFrame(c, kViewportFrameX, kStatusBarY, kViewportFrameW + kProtectFrameW + 2, kStatusBarH);
    drawRedVLine(c, kStatusColDiv1X, kStatusBarY, kStatusBarY + kStatusBarH - 1);
    drawRedVLine(c, kStatusColDiv2X, kStatusBarY, kStatusBarY + kStatusBarH - 1);
    drawRedVLine(c, kStatusColDiv3X, kStatusBarY, kStatusBarY + kStatusBarH - 1);

    // Party panel (show_command_reference_panel @ 0x5D54 draws into this band).
    drawRedFrame(c, kViewportFrameX, kPartyPanelY, kViewportFrameW + kProtectFrameW + 2, kPartyPanelH);
}

void drawPlayStatusBar(ScreenCompositor &c, int day, int year, char facing_key)
{
    char opt[32];
    char day_buf[24];
    char year_buf[24];
    char face_buf[16];
    std::snprintf(opt, sizeof(opt), "'O' Options");
    std::snprintf(day_buf, sizeof(day_buf), "Day=%3d", day);
    std::snprintf(year_buf, sizeof(year_buf), "Year=%4d", year);
    std::snprintf(face_buf, sizeof(face_buf), "Face=%c", facing_key);

    c.drawTextShadow(12, kStatusBarY + 1, opt, 255, 255, 255);
    c.drawTextShadow(104, kStatusBarY + 1, day_buf, 255, 255, 255);
    c.drawTextShadow(176, kStatusBarY + 1, year_buf, 255, 255, 255);
    c.drawTextShadow(256, kStatusBarY + 1, face_buf, 255, 255, 255);
}

void drawPlayPartyList(ScreenCompositor &c, int party_count, const char (*names)[16], const int *hp_current,
                       const int *hp_max)
{
    const int col1_x = 12;
    const int col2_x = 168;
    for (int i = 0; i < party_count && i < 8; ++i) {
        const int col = i / 4;
        const int row = i % 4;
        const int x = (col == 0) ? col1_x : col2_x;
        const int y = kPartyPanelY + 4 + row * 12;
        char line[40];
        if (hp_max && hp_max[i] > 0) {
            std::snprintf(line, sizeof(line), "%d) %s /%d", i + 1, names[i], hp_current[i]);
        } else {
            std::snprintf(line, sizeof(line), "%d) %s", i + 1, names[i]);
        }
        c.drawTextShadow(x, y, line, 255, 255, 255);
    }
}

}  // namespace mm2::gfx
