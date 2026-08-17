#include "mm2/gfx/CombatPanel.h"

#include "mm2/Config.h"
#include "mm2/CppStdCompat.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/mm2_font8x8.h"

#include <cstring>

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
    /* drawTextPen skips codepoints < 32; combat check glyph 0x17 needs drawGlyphPen.
     * Roster lines are usually 0x17 + ASCII — stamp the check, then batch the rest. */
    const uint8_t pen = (r >= 200 && g >= 200 && b <= 80) ? kPenYellow : kPenWhite;
    int x = col * 8;
    const int y = row * 8;
    const char *p = text;
    if (static_cast<unsigned char>(*p) == static_cast<unsigned char>(kCheckGlyph)) {
        c.drawGlyphPen(x, y, static_cast<uint8_t>(kCheckGlyph), pen);
        x += 8;
        ++p;
    }
    bool has_ctrl = false;
    for (const char *q = p; *q; ++q) {
        if (static_cast<unsigned char>(*q) < 32u) {
            has_ctrl = true;
            break;
        }
    }
    if (!has_ctrl) {
        if (*p) {
            c.drawTextPen(x, y, p, pen);
        }
        return;
    }
    for (; *p; ++p) {
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

/* Long lines wrap onto the band's following rows instead of running off the
 * right edge (PC + Amiga alike): the round band is cleared rows 0x0F..0x11 by
 * 0x1119E, so up to 3 wrapped rows fit; pre-round keeps one status row. */
void textAtWrapped(ScreenCompositor &c, int col, int row, const char *text, int max_cols,
                   int max_rows, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255)
{
    if (!text || max_cols <= 0 || max_rows <= 0) {
        return;
    }
    /* Identify Monster @ 0xB760 (and any other positioned band dump) is one
     * newline-separated block; keep interior spaces so columns stay put. */
    if (std::strchr(text, '\n') != nullptr) {
        int rows_used = 0;
        const char *p = text;
        while (*p != '\0' && rows_used < max_rows) {
            const char *nl = std::strchr(p, '\n');
            int n = nl ? static_cast<int>(nl - p) : static_cast<int>(std::strlen(p));
            if (n > max_cols) {
                n = max_cols;
            }
            char line[80];
            if (n > 0) {
                std::memcpy(line, p, static_cast<size_t>(n));
            }
            line[n] = '\0';
            textAt(c, col, row + rows_used, line, r, g, b);
            ++rows_used;
            if (!nl) {
                break;
            }
            p = nl + 1;
        }
        return;
    }
    char line[80];
    int len = 0;
    int rows_used = 0;
    const auto flush = [&]() {
        if (len <= 0) {
            return;
        }
        line[len] = '\0';
        textAt(c, col, row + rows_used, line, r, g, b);
        ++rows_used;
        len = 0;
    };
    const char *p = text;
    while (*p != '\0' && rows_used < max_rows) {
        while (*p == ' ') {
            ++p;
        }
        const char *w = p;
        while (*w != '\0' && *w != ' ') {
            ++w;
        }
        const int wlen = static_cast<int>(w - p);
        if (wlen == 0) {
            break;
        }
        if (len > 0 && len + 1 + wlen <= max_cols) {
            line[len++] = ' ';
            for (int i = 0; i < wlen; ++i) {
                line[len++] = p[i];
            }
        } else {
            flush();
            const char *q = p;
            int remain = wlen;
            while (remain > 0 && rows_used < max_rows) {
                const int take = remain < max_cols ? remain : max_cols;
                for (int i = 0; i < take; ++i) {
                    line[len++] = q[i];
                }
                q += take;
                remain -= take;
                if (remain > 0) {
                    flush();
                }
            }
        }
        p = w;
    }
    flush();
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
    /* Victory panel @ 0x124D2 clears (0x10,3)-(0x26,0x0D); row 1 must not keep
     * the round-loop "D-Delay P-Prot Q-Quick" header over the Victory! chrome. */
    if (view.show_victory) {
        fillCellRect(c, kCombatRightCol, 1, kCombatRightWidthCells,
                     kCombatMonsterOverflowRow - 1 + 1);
        return;
    }

    if (!view.label_monster_slots) {
        /* Pre-combat encounter list @ 0x12D80..0x12E7E: yellow console_box at
         * cols 0x16..0x26 row 1, height = names + 2 (+3 when > 10), monster
         * names at window col 1 (screen 0x17) rows 2.., overflow "+N more" at
         * window (4, 0xC) and the pluralised name at window (1, 0xD).
         * win_open @ 0x21B9A blacks the full window first — border glyphs are
         * sparse, so without that wipe the 3D hatch shows through the frame. */
        const bool overflow = view.overflow_more > 0;
        const int box_rows = view.monster_line_count + 2 + (overflow ? 3 : 0);
        fillCellRect(c, kCombatEncounterBoxCol, 1, kCombatEncounterBoxWidthCells, box_rows);
        c.drawConsoleBox(1, kCombatEncounterBoxCol, kCombatEncounterBoxWidthCells, box_rows, kCombatBoxR,
                         kCombatBoxG, kCombatBoxB);

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
     * overflow line at row 0x0D.
     * Clear from row 1 (not only row 3): renderCombatBackdrop paints the indoor
     * ceiling hatch past the narrow hood into cols 0x10+, and
     * maskCombatBackdropBleed only blacks the divider column — without this
     * fill, "D-Delay P-" sits on hatch and reads as a truncated "Prot Q-Quick". */
    fillCellRect(c, kCombatRightCol, 1, kCombatRightWidthCells,
                 kCombatMonsterOverflowRow - 1 + 1);

    textAt(c, kCombatRightCol, 1, "D-Delay P-Prot Q-Quick");

    for (int i = 0; i < view.monster_line_count && i < 10; ++i) {
        const CombatMonsterLine &line = view.monster_lines[i];
        const int row = kCombatMonsterRow0 + i;
        if (!line.occupied) {
            /* 0x129CC: empty/dead battle slot → clear row (fill above already did).
             * After 0x10CCE compact these are only trailing rows (G–J empty), never
             * holes above a later letter. */
            continue;
        }

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
        /* 0x1061E: pen #$1 while 0x1265E redraws the acting slot, then pen 0. */
        if (line.highlighted) {
            c.fillRect(kCombatRightCol * 8, row * 8, kCombatRightWidthCells * 8, 8, 255, 255, 0, 255);
            textAt(c, kCombatRightCol, row, buf, 0, 0, 0);
        } else {
            textAt(c, kCombatRightCol, row, buf);
        }
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
    /* Pre-combat (encounter entry @ 0x12C6E) still uses the exploration hood;
     * Options/Bribe/surprise strings print via -$7EC0 → 0x54F2 at row $11.
     * Round-loop messages use 0x1119E at row $0F (hood bottom is then $0E). */
    if (view.show_victory) {
        /* 0x124D2 panel owns the chrome; leave the message band blank. */
        fillCellRect(c, 1, kMessageRow, 0x26, 3);
        return;
    }
    const bool round_layout = view.label_monster_slots;
    const int msg_row = round_layout ? kMessageRow : 0x11;

    if (round_layout) {
        /* 0x1119E: clear cols 1..0x26 rows 0x0F..0x11, cursor (1, 0x0F). */
        fillCellRect(c, 1, kMessageRow, 0x26, 3);
    } else {
        /* 0x54F2: clear the exploration status/message line (row $11). */
        fillCellRect(c, 1, 0x11, 0x26, 1);
    }

    if (view.show_party_options) {
        /* string @ 0x13222, printed via -$7EC0 (0x12F74) → row $11. */
        textAt(c, 1, msg_row, "Options: A-Attack B-Bribe H-Hide R-Run");
        return;
    }

    if (view.show_bribe_kind) {
        /* string @ 0x13249 (0x12FBC) via -$7EC0. */
        textAt(c, 1, msg_row, "Bribe with:  1-Food  2-Gold  3-Gems");
        return;
    }
    if (view.show_bribe_amount) {
        /* string @ 0x1326D (0x12FF6); digits via -$7F8C → 0x3EE0. */
        textAt(c, 1, msg_row, "How much?");
        return;
    }

    /* Combat cast @ 0x11A90 → 0x79C6/0x79EE: message band only (no LAB_6622).
     * Combat row $0F (0x79DC); exploration row $15.
     * Level line @ (2,$0F) string 0x7BA2 "Cast Spell Level: "; digit echoed;
     * Number @ (0x0C, row+1) string 0x7BB5 — must not share the level row. */
    if (view.show_cast_level) {
        textAt(c, 2, msg_row, "Cast Spell Level: ");
        return;
    }
    if (view.show_cast_number) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Cast Spell Level: %d", view.cast_level);
        textAt(c, 2, msg_row, buf);
        textAt(c, 0x0C, msg_row + 1, "Number: ");
        return;
    }
    if (view.show_cast_target) {
        /* Cast: 0xD52E family. Fight/Shoot: status already "Fight/Shoot which (A-x)?". */
        if (view.message[0] != '\0') {
            textAtWrapped(c, 1, msg_row, view.message, 0x26, round_layout ? 3 : 1);
        } else {
            textAt(c, 1, msg_row, "Which monster?");
        }
        return;
    }
    if (view.show_party_pick) {
        /* 0xD2EA "On whom (1-N)?"; solo 0xCCE8 / 0xD390 "'Return' to cast" at col $16. */
        if (std::strcmp(view.message, "'Return' to cast") == 0) {
            textAt(c, 0x16, msg_row, view.message);
        } else if (view.message[0] != '\0') {
            textAtWrapped(c, 1, msg_row, view.message, 0x26, round_layout ? 3 : 1);
        }
        return;
    }
    if (view.show_item_pick) {
        /* 0xB56E / Use pick — prompt already in view.message. */
        if (view.message[0] != '\0') {
            textAtWrapped(c, 1, msg_row, view.message, 0x26, round_layout ? 3 : 1);
        }
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
        /* Surprise strings @ 0x12F30/0x12F44 also go through -$7EC0 → row $11. */
        textAtWrapped(c, 1, msg_row, view.message, 0x26, round_layout ? 3 : 1);
    }
}

void drawCombatVictoryPanel(ScreenCompositor &c, const CombatPanelView &view)
{
    /* combat_victory_rewards presentation @ 0x124D2..0x125F8:
     *   win_clear_cells(0x10,3,0x26,0x0D) — screen cells (16,3)-(38,13)
     *   A-pen ← -$7A4D (20, light blue)
     *   glyph 5 × $17 at rows 4 and 0x0C starting col 0x10 (blue H-rules)
     *   A-pen ← -$7A52 (1, white); strings @ 0x12610.. + XP share from -$6FC6 */
    if (!view.show_victory) {
        return;
    }

    constexpr int kX1 = 0x10;
    constexpr int kY1 = 3;
    constexpr int kX2 = 0x26;
    constexpr int kY2 = 0x0D;
    constexpr int kRuleLen = 0x17; /* 0x12516 / 0x1253E loop bound */
    constexpr uint8_t kGlyphHSeg = 0x05;
    /* Play-palette pen 20 ≈ light blue (A4-$7A4D / OutdoorView3D star_color). */
    using namespace play_layout;

    fillCellRect(c, kX1, kY1, kX2 - kX1 + 1, kY2 - kY1 + 1);

    for (int i = 0; i < kRuleLen; ++i) {
        c.drawGlyph((kX1 + i) * 8, 4 * 8, kGlyphHSeg, kUiBlueBorderR, kUiBlueBorderG, kUiBlueBorderB);
        c.drawGlyph((kX1 + i) * 8, 0x0C * 8, kGlyphHSeg, kUiBlueBorderR, kUiBlueBorderG, kUiBlueBorderB);
    }

    textAt(c, 0x18, 5, "Victory!");
    textAt(c, kX1, 7, "Your party has won its");
    /* 0x12584 cursor (0x16,8) then 0x1215A: number + st/nd/rd/th + " battle." */
    {
        const unsigned n = view.victory_battles_won;
        const unsigned rem = n % 10u;
        const char *suf = "th";
        if (rem == 1u && n != 11u) {
            suf = "st";
        } else if (rem == 2u && n != 12u) {
            suf = "nd";
        } else if (rem == 3u && n != 13u) {
            suf = "rd";
        }
        char battle_line[40];
        std::snprintf(battle_line, sizeof(battle_line), "%u%s battle.", n, suf);
        textAt(c, 0x16, 8, battle_line);
    }
    textAt(c, kX1, 0x0A, "Each survivor receives");

    char xp_line[48];
    /* 0x125BC..0x125F6: number + " experience p" + ("ts" if share>=100000 else "oints"). */
    if (view.victory_xp_share >= 100000u) {
        std::snprintf(xp_line, sizeof(xp_line), "%lu experience pts",
                      static_cast<unsigned long>(view.victory_xp_share));
    } else {
        std::snprintf(xp_line, sizeof(xp_line), "%lu experience points",
                      static_cast<unsigned long>(view.victory_xp_share));
    }
    textAt(c, kX1, 0x0B, xp_line);
}

}  // namespace mm2::gfx
