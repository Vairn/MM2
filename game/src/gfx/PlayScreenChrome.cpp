#include "mm2/gfx/PlayScreenChrome.h"

#include "mm2/CppStdCompat.h"
#include "mm2/Config.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PartyStatusFormat.h"
#include "mm2/gfx/mm2_font8x8.h"

#include "mm2_roster_codec.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#endif

// All draws happen on the 40x24 cell grid of the 8px font: x = col*8,
// y = row*8 (text cursor -$7BFC -> 0x22108, glyph put -$7C62 -> 0x218EA).
//
// Glyph lattice primitives (targets of the A4 jump thunks, data hunk):
//   -$7F86 -> 0x4032  h_line(col0, col1, row): glyph 6, 5..., 7
//   -$7F80 -> 0x4088  v_line(row0, row1, col): glyph 9, 0x0B..., 8
//   -$7F7A -> 0x422A  outer frame: corner glyphs 1/2/3/4 + 5 + 0x0B (via 0x410A)
//   -$7F62 -> 0x42DC  clear cell rect (pixels (col*8, row*8)-(col*8+7, row*8+7))
//
// Chrome init 0x60F4: outer frame, h-lines rows 0x10/0x12 cols 0..0x27,
// v-line col 0x1B rows 0..0x10, then 0x60B6 dividers (v-lines rows
// 0x10..0x12 at cols 0xC/0x15/0x1F).

namespace mm2::gfx {

using namespace play_layout;

void fillCellRect(ScreenCompositor &c, int col, int row, int width_cells, int height_cells)
{
    if (width_cells <= 0 || height_cells <= 0) {
        return;
    }
#if MM2_HOST_AMIGA
    /* Pen 0 = black bitplane clear (0x42DC). Avoid RGB→pen lookup on every cell wipe. */
    c.fillRectPen(col * 8, row * 8, width_cells * 8, height_cells * 8, 0);
#else
    c.fillRect(col * 8, row * 8, width_cells * 8, height_cells * 8, 0, 0, 0);
#endif
}

namespace {

constexpr uint8_t kGlyphCornerTL = 0x01;
constexpr uint8_t kGlyphCornerTR = 0x02;
constexpr uint8_t kGlyphCornerBL = 0x03;
constexpr uint8_t kGlyphCornerBR = 0x04;
constexpr uint8_t kGlyphHSeg = 0x05;
constexpr uint8_t kGlyphHCapL = 0x06;
constexpr uint8_t kGlyphHCapR = 0x07;
constexpr uint8_t kGlyphVCapBottom = 0x08;
constexpr uint8_t kGlyphVCapTop = 0x09;
constexpr uint8_t kGlyphVSeg = 0x0B;

#if MM2_HOST_AMIGA
constexpr uint8_t kPenBorder = MM2_UI_PEN_RED;
constexpr uint8_t kPenWhite = MM2_UI_PEN_WHITE;
constexpr uint8_t kPenWarn = MM2_UI_PEN_WARN;
#endif

void glyphAt(ScreenCompositor &c, int col, int row, uint8_t glyph)
{
#if MM2_HOST_AMIGA
    c.drawGlyphPen(col * 8, row * 8, glyph, kPenBorder);
#else
    c.drawGlyph(col * 8, row * 8, glyph, kBorderR, kBorderG, kBorderB);
#endif
}

void textAt(ScreenCompositor &c, int col, int row, const char *text, uint8_t r = 255, uint8_t g = 255,
            uint8_t b = 255)
{
    if (!text) {
        return;
    }
#if MM2_HOST_AMIGA
    /* drawTextPen skips codepoints < 32; combat check glyph 0x17 needs drawGlyphPen. */
    uint8_t pen = kPenWhite;
    if (r >= 200 && g <= 96 && b <= 96) {
        pen = kPenWarn;
    } else if (!(r >= 210 && g >= 210 && b >= 210)) {
        /* Non-white RGB path still uses white UI pen on Amiga (palette banks). */
        pen = kPenWhite;
    }
    int x = col * 8;
    const int y = row * 8;
    for (const char *p = text; *p; ++p) {
        const unsigned uch = static_cast<unsigned char>(*p);
        if (uch >= MM2_FONT8X8_GLYPHS) {
            continue;
        }
        c.drawGlyphPen(x, y, static_cast<uint8_t>(uch), pen);
        x += 8;
    }
#else
    int x = col * 8;
    const int y = row * 8;
    for (const char *p = text; *p; ++p) {
        const unsigned uch = static_cast<unsigned char>(*p);
        if (uch >= MM2_FONT8X8_GLYPHS) {
            continue;
        }
        c.drawGlyph(x, y, static_cast<uint8_t>(uch), r, g, b);
        x += 8;
    }
#endif
}

/** -$7C08(1) → SetDrMd 5 (JAM2+INVERSVID): A-pen fills the cell, glyph bits in black. */
void textAtInverse(ScreenCompositor &c, int col, int row, const char *text)
{
    if (!text) {
        return;
    }
    int x = col * 8;
    const int y = row * 8;
    for (const char *p = text; *p; ++p) {
        const unsigned uch = static_cast<unsigned char>(*p);
        c.fillRect(x, y, 8, 8, 255, 255, 255, 255);
        if (uch < MM2_FONT8X8_GLYPHS) {
            c.drawGlyph(x, y, static_cast<uint8_t>(uch), 0, 0, 0);
        }
        x += 8;
    }
}

/* h_line @ 0x4032: cap 6 at col0, glyph 5 cols col0+1..col1-1, cap 7 at col1. */
void hLine(ScreenCompositor &c, int col0, int col1, int row)
{
    glyphAt(c, col0, row, kGlyphHCapL);
    for (int col = col0 + 1; col < col1; ++col) {
        glyphAt(c, col, row, kGlyphHSeg);
    }
    glyphAt(c, col1, row, kGlyphHCapR);
}

/* v_line @ 0x4088: cap 9 at row0, glyph 0x0B rows row0+1..row1-1, cap 8 at row1. */
void vLine(ScreenCompositor &c, int row0, int row1, int col)
{
    glyphAt(c, col, row0, kGlyphVCapTop);
    for (int row = row0 + 1; row < row1; ++row) {
        glyphAt(c, col, row, kGlyphVSeg);
    }
    glyphAt(c, col, row1, kGlyphVCapBottom);
}

/* Outer frame @ 0x422A (glyph set 1/2/3/4 + 5 + 0x0B via 0x410A).
 * GAP: 0x410A internals not fully traced; the full 40x24 grid border is
 * assumed (matches the visible outer red frame on the play screen). */
void outerFrame(ScreenCompositor &c)
{
    const int col1 = 39;
    const int row1 = 23;
    glyphAt(c, 0, 0, kGlyphCornerTL);
    glyphAt(c, col1, 0, kGlyphCornerTR);
    glyphAt(c, 0, row1, kGlyphCornerBL);
    glyphAt(c, col1, row1, kGlyphCornerBR);
    for (int col = 1; col < col1; ++col) {
        glyphAt(c, col, 0, kGlyphHSeg);
        glyphAt(c, col, row1, kGlyphHSeg);
    }
    for (int row = 1; row < row1; ++row) {
        glyphAt(c, 0, row, kGlyphVSeg);
        glyphAt(c, col1, row, kGlyphVSeg);
    }
}

/* Black fills for play-screen interiors (clear_cell_rect @ 0x42DC / play_frame_draw
 * @ 0x54F2). All rects stay INSIDE the outer frame (cols 1..38, not 0..39) so
 * border glyph cells are never overwritten. */
void playScreenInteriorFills(ScreenCompositor &c)
{
    /* Viewport interior: cols 1..26, rows 1..15 (left of v-line col 0x1B). */
    fillCellRect(c, 1, 1, 26, 15);
    /* Right column interior: cols 28..37, rows 1..15. */
    fillCellRect(c, 28, 1, 10, 15);
    /* Status text row + party rows 17..22 (between h-lines at 16/18, above bottom border). */
    fillCellRect(c, 1, 0x11, 38, 6);
}

}  // namespace

void drawPlayModalBackdrop(ScreenCompositor &c)
{
    /* Quick Ref $595C / sheet $398C: full black, then -$7F7A outer frame @ 0x422A
     * (cols 0..39, rows 0..23). Not console_box (-$7F62 / glyphs $0E..$15). */
    c.fillRect(0, 0, kScreenW, kScreenH, 0, 0, 0);
    outerFrame(c);
    /* clear_rect_preset(3) @ $5312: interior (1,1)-(38,22) — keep frame cells. */
    fillCellRect(c, kPlayOverlayBorderCol + 1, kPlayOverlayBorderRow + 1, kPlayOverlayBorderW - 2,
                 kPlayOverlayBorderH - 2);
}

void drawPlayScreenChromeStatic(ScreenCompositor &c)
{
    /* play_screen_chrome_init @ 0x60F4 — black fills first, red glyphs on top. */
    playScreenInteriorFills(c);

    outerFrame(c);                /* -$7F7A */
    hLine(c, 0, 0x27, 0x10);      /* status strip top    (row 16) */
    hLine(c, 0, 0x27, 0x12);      /* status strip bottom (row 18) */
    vLine(c, 0, 0x10, 0x1B);      /* viewport / right column divider (col 27) */

    /* draw_viewport_red_lines @ 0x60B6: status column dividers. */
    vLine(c, 0x10, 0x12, 0x0C);
    vLine(c, 0x10, 0x12, 0x15);
    vLine(c, 0x10, 0x12, 0x1F);
}

void drawPlayScreenChrome(ScreenCompositor &c)
{
    drawPlayScreenChromeStatic(c);
}

void drawPlayOuterFrame(ScreenCompositor &c)
{
    outerFrame(c);
}

void drawPlayViewportDivider(ScreenCompositor &c)
{
    vLine(c, 0, 0x10, 0x1B);
}

void drawCombatScreenLines(ScreenCompositor &c)
{
    /* combat_display_refresh @ 0x135BE rules:
     *   h_line(0, 0x27, 0x0E), v_line(0, 0x0E, 0x0F), h_line(0x0F, 0x27, 0x02),
     *   border patches: glyph 0x0B at (0,0x10)/(0x27,0x10), glyph 0x05 at (0x1B,0). */
    hLine(c, 0, kCombatPanelLineCol1, kCombatHoodBottomRow);
    vLine(c, 0, kCombatDividerRowEnd, kCombatDividerCol);
    hLine(c, kCombatDividerCol, kCombatPanelLineCol1, kCombatHeaderRuleRow);
    glyphAt(c, 0x00, 0x10, kGlyphVSeg);
    glyphAt(c, 0x27, 0x10, kGlyphVSeg);
    glyphAt(c, 0x1B, 0x00, kGlyphHSeg);
}

void drawCombatScreenChrome(ScreenCompositor &c)
{
    /* combat_display_refresh @ 0x135BE: clear cols 1..0x26 rows 1..0x11 (wipes
     * the exploration h-line 0x10 + v-line 0x1B), then the combat rules. */
    fillCellRect(c, 1, 1, 0x26, 0x11);
    drawCombatScreenLines(c);
}

void drawCombatViewportDivider(ScreenCompositor &c)
{
    vLine(c, 0, kCombatDividerRowEnd, kCombatDividerCol);
}

void drawPlayStatusBar(ScreenCompositor &c, int day, int year, char facing_key, bool new_game)
{
    /* draw_status_bar @ 0x62C8 — row 17 (0x11). */
    const int row = 0x11;

    /* col 1: "'O' Options" while new_game flag == 1 (0x63A2), else
     * "'P' Protect" (0x63AE). */
    textAt(c, 1, row, new_game ? "'O' Options" : "'P' Protect");

    char buf[16];
    /* col 13: "Day=" + print_number(day, width 3, pad ' ') + putchar ' '
     * (0x630C..0x633C). -$7BDE left-pads; the extra space is after the field. */
    textAt(c, 0x0D, row, "Day=");
    formatPrintNumber(static_cast<uint32_t>(day < 0 ? 0 : day), buf, sizeof(buf), 3, ' ');
    textAt(c, 0x0D + 4, row, buf);
    textAt(c, 0x0D + 4 + 3, row, " ");

    /* col 22: "Year=" + print_number(year, width 4, pad ' ') (0x634C..0x6372). */
    textAt(c, 0x16, row, "Year=");
    formatPrintNumber(static_cast<uint32_t>(year < 0 ? 0 : year), buf, sizeof(buf), 4, ' ');
    textAt(c, 0x16 + 5, row, buf);

    /* col 32: "Face=" + movement key char -$79B1 (0x6382..0x6398). */
    textAt(c, 0x20, row, "Face=");
    buf[0] = facing_key;
    buf[1] = '\0';
    textAt(c, 0x20 + 5, row, buf);
}

void drawPlayPartyPanel(ScreenCompositor &c, const PlayPartySlot slots[8])
{
    /* draw_party_status_panel @ 0x6178: slot i at row 0x13 + i/2,
     * col alternating 1 / 0x14 (slots 1..8 read across, two per row). */
    for (int i = 0; i < 8; ++i) {
        const int row = kPartySlotRowBase + i / 2;
        const int col = (i & 1) ? kPartySlotColRight : kPartySlotColLeft;
        const PlayPartySlot &s = slots[i];

        /* Empty slots only: -$7F62 clear @ 0x6178 when party word is $FFFF.
         * Occupied slots overwrite in place; clearing width 0x13 from col 1
         * would erase the left slot's last HP cell (shared with col 0x14). */
        if (!s.present) {
            fillCellRect(c, col, row, kPartySlotClearWidth, 1);
            continue;
        }

        PartyStatusPrefix prefix_style = PartyStatusPrefix::Exploration;
        if (s.in_combat) {
            prefix_style =
                s.combat_front_rank ? PartyStatusPrefix::CombatFrontRank : PartyStatusPrefix::CombatBackRank;
        }
        char line[48];
        formatPartyStatusLine(line, sizeof(line), i, s.name, static_cast<uint16_t>(s.hp), prefix_style);

        /* 0x6150 / 0x12848: putchar prefix (` n)` or check+digit+`)`), then
         * -$7C08(1) when +$26 != 0 → SetDrMd 5 (JAM2+INVERSVID @ 0x221EC).
         * Inverse covers the spaces around the name; '/' is after attr clear. */
        constexpr int kPrefixLen = 3; /* " n)" / "\x17n)" — space after ')' is inverted */
        const char *after_prefix = line + kPrefixLen;
        const char *slash = std::strstr(after_prefix, " /");
        char prefix[kPrefixLen + 1];
        char name_field[MM2_ROSTER_NAME_SIZE + 4];
        char tail[16];
        std::memcpy(prefix, line, kPrefixLen);
        prefix[kPrefixLen] = '\0';
        if (slash) {
            const size_t name_len = static_cast<size_t>(slash - after_prefix);
            const size_t copy_n =
                name_len < sizeof(name_field) - 2 ? name_len : sizeof(name_field) - 2;
            std::memcpy(name_field, after_prefix, copy_n);
            name_field[copy_n] = ' ';
            name_field[copy_n + 1] = '\0';
            std::snprintf(tail, sizeof(tail), "%s", slash + 1); /* skip the space; inverse ate it */
        } else {
            name_field[0] = '\0';
            std::snprintf(tail, sizeof(tail), "%s", after_prefix);
        }

        textAt(c, col, row, prefix);

        const int name_col = col + kPrefixLen;
        if (s.bad_condition) {
            textAtInverse(c, name_col, row, name_field);
        } else {
            textAt(c, name_col, row, name_field);
        }

        textAt(c, name_col + static_cast<int>(std::strlen(name_field)), row, tail);
    }
}

void drawPlayRightColumn(ScreenCompositor &c, PlayRightPanel panel, const PlayProtectValues *protect)
{
    if (panel == PlayRightPanel::Protect) {
        /* Protection panel @ 0x5E28 (via -$7EAE): clear (28,1)-(38,15),
         * h-line row 9 cols 27..39, labels col 28 rows 10..12. */
        fillCellRect(c, 0x1C, 1, 11, 15);
        hLine(c, 0x1B, 0x27, 0x09);
        textAt(c, 0x1C, 0x0A, "Light     )");
        textAt(c, 0x1C, 0x0B, "Magic     %");
        textAt(c, 0x1C, 0x0C, "Forces    %");

        if (protect) {
            /* 0x5EB8..0x5F86: start col $24 (Light) / $25 (Magic/Forces), then
             * col-- if value>=10 and again if >=100. Light putchar '(' first
             * (0x5EE4), then -$7BDE(value, width 1, pad space). */
            const auto valueCol = [](int base, unsigned v) {
                int col = base;
                if (v >= 10u) {
                    --col;
                }
                if (v >= 100u) {
                    --col;
                }
                return col;
            };
            char buf[8];
            const int light_col = valueCol(0x24, protect->light);
            textAt(c, light_col, 0x0A, "(");
            formatPrintNumber(protect->light, buf, sizeof(buf), 1, ' ');
            textAt(c, light_col + 1, 0x0A, buf);

            formatPrintNumber(protect->magic, buf, sizeof(buf), 1, ' ');
            textAt(c, valueCol(0x25, protect->magic), 0x0B, buf);
            formatPrintNumber(protect->forces, buf, sizeof(buf), 1, ' ');
            textAt(c, valueCol(0x25, protect->forces), 0x0C, buf);

            int row = 0x0D;
            if (protect->levitate) {
                textAt(c, 0x1C, row++, "Levitate");
            }
            if (protect->walk_water) {
                textAt(c, 0x1C, row++, "Walk/Water");
            }
            if (protect->guard_dog) {
                textAt(c, 0x1C, row++, "Guard Dog");
            }
        }

        /* GAP: copy-protection challenge rows 1..8 @ 0x5EB8+ untraced (globe.32). */
        return;
    }

    /* Command reference @ 0x5D54: clear right column then print A4-$741A strings. */
    fillCellRect(c, 0x1C, 1, 11, 15);
    static const char *kCommandRef[15] = {
        "  OPTIONS  ",
        "\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05",
        "\x18 Forward  ",
        "\x19 Move Back",
        "\x1A Turn Left",
        "\x1B Turn Rght",
        "B Bash Door",
        "C Controls ",
        "D Dismiss  ",
        "E Exchange ",
        "Q Quick Ref",
        "R Rest     ",
        "S Search   ",
        "U Unlock   ",
        "# View Char",
    };
    for (int i = 0; i < 15; ++i) {
        textAt(c, 0x1C, 1 + i, kCommandRef[i]);
    }
}

}  // namespace mm2::gfx
