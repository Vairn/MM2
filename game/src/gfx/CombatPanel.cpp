#include "mm2/gfx/CombatPanel.h"

#include "mm2/Config.h"
#include "mm2/CppStdCompat.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/mm2_font8x8.h"

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

constexpr uint8_t kCombatBoxR = 255;
constexpr uint8_t kCombatBoxG = 204;
constexpr uint8_t kCombatBoxB = 0;

/* Message / options band cleared by 0x1119E: cols 1..0x26, rows 0x0F..0x11. */
constexpr int kMessageRow = 0x0F;

/* Check glyph before roster letters / party digits (0x12742 / 0x12898). */
constexpr char kCheckGlyph = 0x17;

/* Monster name field: dots pad to 14 before '/'+status (0x127C6..0x127E8). */
constexpr int kMonsterNameField = 0x0E;

void textAt(ScreenCompositor &c, int col, int row, const char *text, uint8_t r = 255, uint8_t g = 255,
            uint8_t b = 255)
{
    if (!text) {
        return;
    }
#if MM2_HOST_AMIGA
    /* drawTextPen skips codepoints < 32; combat check glyph 0x17 needs drawGlyphPen. */
    const uint8_t pen = (r >= 200 && g >= 200 && b <= 80) ? kPenYellow : kPenWhite;
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

/* "+N more" name pluralisation @ 0x12706/0x12E9C: append 's' unless the
 * name already ends in 's' or exactly one extra monster. */
void formatOverflowName(char *out, size_t cap, const char *name, int extra)
{
    const size_t len = std::strlen(name);
    if (extra != 1 && len > 0 && name[len - 1] != 's') {
        std::snprintf(out, cap, "%ss", name);
    } else {
        std::snprintf(out, cap, "%s", name);
    }
}

}  // namespace

void drawCombatViewport(ScreenCompositor &c)
{
    fillCellRect(c, kCombatViewportCol, 1, kCombatViewportWidthCells, kCombatViewportHeightCells);
}

void drawCombatViewportFrame(ScreenCompositor &c)
{
    /* console_box col0=1 row0=1 col1=0x0E rows=0x0D @ 0x136AA — yellow hood frame. */
    c.drawConsoleBox(1, kCombatViewportCol, kCombatViewportWidthCells, kCombatViewportBoxHeightCells, kCombatBoxR,
                     kCombatBoxG, kCombatBoxB);
}

void drawCombatRightColumn(ScreenCompositor &c, const CombatPanelView &view)
{
    if (!view.label_monster_slots) {
        /* Pre-combat encounter list @ 0x12D80..0x12E7E: yellow console_box at
         * cols 0x16..0x26 row 1, height = names + 2 (+3 when > 10), monster
         * names at window col 1 (screen 0x17) rows 2.., overflow "+N more" at
         * window (4, 0xC) and the pluralised name at window (1, 0xD). */
        const bool overflow = view.overflow_more > 0;
        const int box_rows = view.monster_line_count + 2 + (overflow ? 3 : 0);
        c.drawConsoleBox(1, kCombatEncounterBoxCol, kCombatEncounterBoxWidthCells, box_rows, kCombatBoxR,
                         kCombatBoxG, kCombatBoxB);
        fillCellRect(c, kCombatEncounterBoxCol + 1, 2, kCombatEncounterBoxWidthCells - 2, box_rows - 2);

        for (int i = 0; i < view.monster_line_count; ++i) {
            textAt(c, kCombatEncounterBoxCol + 1, 2 + i, view.monster_lines[i].name);
        }

        if (overflow) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "+%d more", view.overflow_more);
            textAt(c, kCombatEncounterBoxCol + 4, 0x0D, buf);
            char plural[24];
            formatOverflowName(plural, sizeof(plural), view.overflow_name, view.overflow_more);
            textAt(c, kCombatEncounterBoxCol + 1, 0x0E, plural);
        }
        return;
    }

    /* Round roster @ 0x129CC / 0x1265E: header (0x10,1), slot i at row i+3,
     * overflow line at row 0x0D. */
    fillCellRect(c, kCombatRightCol, kCombatMonsterRow0, kCombatRightWidthCells,
                 kCombatMonsterOverflowRow - kCombatMonsterRow0 + 1);

    textAt(c, kCombatRightCol, 1, "D-Delay P-Prot Q-Quick");

    for (int i = 0; i < view.monster_line_count && i < 10; ++i) {
        const CombatMonsterLine &line = view.monster_lines[i];
        const int row = kCombatMonsterRow0 + i;

        char buf[48];
        char field[24];
        if (line.status_suffix[0] != '\0') {
            /* name padded with '.' to 14, then '/' + 4-char abbrev (0x127B4). */
            char dots[kMonsterNameField + 1];
            const size_t nlen = std::strlen(line.name);
            size_t pos = 0;
            for (; pos < nlen && pos < kMonsterNameField; ++pos) {
                dots[pos] = line.name[pos];
            }
            for (; pos < kMonsterNameField; ++pos) {
                dots[pos] = '.';
            }
            dots[kMonsterNameField] = '\0';
            std::snprintf(field, sizeof(field), "%s/%s", dots, line.status_suffix);
        } else {
            /* healthy: name + 5 trailing spaces (0x1279C / string @ 0x12842). */
            std::snprintf(field, sizeof(field), "%s     ", line.name);
        }

        std::snprintf(buf, sizeof(buf), "%c%c) %s", line.show_checkmark ? kCheckGlyph : ' ',
                      line.letter ? line.letter : static_cast<char>('A' + i), field);
        textAt(c, kCombatRightCol, row, buf);
    }

    if (view.overflow_more > 0) {
        /* Single line "+96 Giant Lizards" @ 0x12692..0x12734 (row 0x0D). */
        char plural[24];
        formatOverflowName(plural, sizeof(plural), view.overflow_name, view.overflow_more);
        char buf[40];
        std::snprintf(buf, sizeof(buf), "+%d %s", view.overflow_more, plural);
        textAt(c, kCombatRightCol, kCombatMonsterOverflowRow, buf);
    }
}

void drawCombatOptionsBar(ScreenCompositor &c, const CombatPanelView &view)
{
    /* 0x1119E: clear cols 1..0x26 rows 0x0F..0x11, cursor (1, 0x0F). */
    fillCellRect(c, 1, kMessageRow, 0x26, 3);

    if (view.show_party_options) {
        /* string @ 0x13222, printed via -$7EC0 (0x12F74). */
        textAt(c, 1, kMessageRow, "Options: A-Attack B-Bribe H-Hide R-Run");
        return;
    }

    /* Combat cast @ 0x11A90 → 0x79EE: prompts on message band only (no LAB_6622 grid).
       Combat mode uses prompt row $0F (0x7A04); exploration uses $15. */
    if (view.show_cast_level) {
        textAt(c, 2, kMessageRow, " Spell Level: ");
        return;
    }
    if (view.show_cast_number) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), " Spell Level: %d", view.cast_level);
        textAt(c, 2, kMessageRow, buf);
        textAt(c, 0x0C, kMessageRow, "Number: ");
        return;
    }

    if (view.show_command_options && view.options_for[0] != '\0') {
        /* combat_command_bar_build @ 0x11866: " Options for:" at (1,0x0F),
         * name at (2,0x10), commands column-major in the 3x3 grid at
         * cols {0x0F,0x18,0x20} rows {0x0F,0x10,0x11} (tables -$6F54/-$6F4B). */
        textAt(c, 1, kMessageRow, " Options for:");
        textAt(c, 2, kMessageRow + 1, view.options_for);

        const char *cmds[9];
        int n = 0;
        if (view.opt_melee) {
            cmds[n++] = "A-Attack";
            cmds[n++] = "F-Fight";
        }
        if (view.opt_shoot) {
            cmds[n++] = "S-Shoot";
        }
        if (view.opt_cast) {
            cmds[n++] = "C-Cast";
        }
        cmds[n++] = "U-Use";
        cmds[n++] = "B-Block";
        cmds[n++] = "R-Run";
        cmds[n++] = "E-Exch";
        cmds[n++] = "V-View";

        static const int kGridCols[3] = {0x0F, 0x18, 0x20};
        for (int i = 0; i < n && i < 9; ++i) {
            textAt(c, kGridCols[i / 3], kMessageRow + (i % 3), cmds[i]);
        }
        return;
    }

    if (view.message[0] != '\0') {
        textAt(c, 1, kMessageRow, view.message);
    }
}

}  // namespace mm2::gfx
