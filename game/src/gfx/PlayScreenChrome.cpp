#include "mm2/gfx/PlayScreenChrome.h"

#include "mm2/CppStdCompat.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"

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

namespace {

using namespace play_layout;

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

void glyphAt(ScreenCompositor &c, int col, int row, uint8_t glyph)
{
    c.drawGlyph(col * 8, row * 8, glyph, kBorderR, kBorderG, kBorderB);
}

void textAt(ScreenCompositor &c, int col, int row, const char *text, uint8_t r = 255, uint8_t g = 255,
            uint8_t b = 255)
{
    c.drawText(col * 8, row * 8, text, r, g, b);
}

/* The original prints control glyphs (arrows 0x18..0x1B, rule 0x05) through
 * the same putchar path -$7C62; drawText skips codes < 0x20, so render the
 * command-reference strings glyph by glyph. */
void glyphTextAt(ScreenCompositor &c, int col, int row, const char *text)
{
    for (int i = 0; text[i]; ++i) {
        c.drawGlyph((col + i) * 8, row * 8, static_cast<uint8_t>(text[i]), 255, 255, 255);
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

/* Print a number the way -$7BDE + the trailing-space pads at 0x6266..0x629A
 * do for the party HP: ">999" prints "+++" (0x625A), otherwise the value
 * left-aligned padded with trailing spaces to 3 cells. */
void formatHp(char *out, size_t cap, int hp)
{
    if (hp > 999) {
        std::snprintf(out, cap, "+++");
    } else {
        std::snprintf(out, cap, "%d", hp);
    }
}

}  // namespace

void drawPlayScreenChrome(ScreenCompositor &c)
{
    /* play_screen_chrome_init @ 0x60F4 */
    outerFrame(c);                /* -$7F7A */
    hLine(c, 0, 0x27, 0x10);      /* status strip top    (row 16) */
    hLine(c, 0, 0x27, 0x12);      /* status strip bottom (row 18) */
    vLine(c, 0, 0x10, 0x1B);      /* viewport / right column divider (col 27) */

    /* draw_viewport_red_lines @ 0x60B6: status column dividers. */
    vLine(c, 0x10, 0x12, 0x0C);
    vLine(c, 0x10, 0x12, 0x15);
    vLine(c, 0x10, 0x12, 0x1F);
}

void drawPlayStatusBar(ScreenCompositor &c, int day, int year, char facing_key, bool new_game)
{
    /* draw_status_bar @ 0x62C8 — row 17 (0x11). */
    const int row = 0x11;

    /* col 1: "'O' Options" while new_game flag == 1 (0x63A2), else
     * "'P' Protect" (0x63AE). */
    textAt(c, 1, row, new_game ? "'O' Options" : "'P' Protect");

    char buf[16];
    /* col 13: "Day=" + day[era] (-$79DE), width 3 (0x630C..0x6332). */
    textAt(c, 0x0D, row, "Day=");
    std::snprintf(buf, sizeof(buf), "%d", day);
    textAt(c, 0x0D + 4, row, buf);

    /* col 22: "Year=" + year[era] (-$79CA), width 4 (0x634C..0x6372). */
    textAt(c, 0x16, row, "Year=");
    std::snprintf(buf, sizeof(buf), "%d", year);
    textAt(c, 0x16 + 5, row, buf);

    /* col 32: "Face=" + movement key char -$79B1 (0x6382..0x6398). */
    textAt(c, 0x20, row, "Face=");
    buf[0] = facing_key;
    buf[1] = '\0';
    textAt(c, 0x20 + 5, row, buf);
}

void drawPlayPartyPanel(ScreenCompositor &c, const PlayPartySlot slots[8])
{
    /* draw_party_status_panel @ 0x6150: slot i at row 0x13 + i/2,
     * col alternating 1 / 0x14 (slots 1..8 read across, two per row). */
    for (int i = 0; i < 8; ++i) {
        const int row = 0x13 + i / 2;
        const int col = (i & 1) ? 0x14 : 0x01;
        const PlayPartySlot &s = slots[i];

        if (!s.present) {
            /* Empty slot: -$7F62 clear of cells (col, row)-(col+0x12, row)
             * @ 0x617C..0x61A6 — nothing drawn. */
            continue;
        }

        char head[8];
        std::snprintf(head, sizeof(head), " %c) ", '1' + i);
        textAt(c, col, row, head);

        /* Name printed with text attribute 1 (-$7C08 @ 0x6212) when the
         * condition byte +$26 is non-zero. GAP: exact palette effect of
         * attribute 1 untraced (0x220BE); rendered here as red text. */
        if (s.bad_condition) {
            textAt(c, col + 4, row, s.name, 255, 80, 80);
        } else {
            textAt(c, col + 4, row, s.name);
        }

        /* " /" + HP word +$5E (0x623E..0x629A). */
        char hp[8];
        formatHp(hp, sizeof(hp), s.hp);
        char tail[16];
        std::snprintf(tail, sizeof(tail), " /%s", hp);
        textAt(c, col + 4 + static_cast<int>(std::strlen(s.name)), row, tail);
    }
}

void drawPlayRightColumn(ScreenCompositor &c, PlayRightPanel panel, const PlayProtectValues *protect)
{
    if (panel == PlayRightPanel::Protect) {
        /* Protection panel @ 0x5E28 (via -$7EAE): clear (28,1)-(38,15),
         * h-line row 9 cols 27..39, labels col 28 rows 10..12. */
        hLine(c, 0x1B, 0x27, 0x09);
        textAt(c, 0x1C, 0x0A, "Light     )");
        textAt(c, 0x1C, 0x0B, "Magic     %");
        textAt(c, 0x1C, 0x0C, "Forces    %");

        if (protect) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%u", protect->light);
            textAt(c, 0x25, 0x0A, buf);
            std::snprintf(buf, sizeof(buf), "%u", protect->magic);
            textAt(c, 0x25, 0x0B, buf);
            std::snprintf(buf, sizeof(buf), "%u", protect->forces);
            textAt(c, 0x25, 0x0C, buf);

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

    /* Command reference @ 0x5D54: strings from the pointer table at
     * A4-$741A printed at col 0x1C rows 1..15 (0x5DBA..0x5DF0). Control
     * chars 0x18/0x19/0x1A/0x1B are the font arrow glyphs and 0x05 the
     * horizontal-rule glyph. */
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
        glyphTextAt(c, 0x1C, 1 + i, kCommandRef[i]);
    }
}

}  // namespace mm2::gfx
