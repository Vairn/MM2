#pragma once
#include "mm2/gfx/ScreenCompositor.h"

// Play-screen HUD chrome, traced from the session refresh chain
// (0x52A2: -$7EA8 -> 0x60F4 chrome, -$7EA2 -> 0x6150 party panel,
//  -$7EBA -> 0x560A dividers+status, then 0x5E28 / 0x5D54 right column).

namespace mm2::gfx {

struct PlayPartySlot {
    bool present = false;
    bool bad_condition = false;   /* roster byte +$26 != 0 (0x622C) */
    bool in_combat = false;       /* use the combat strip format (0x12848) */
    bool combat_front_rank = false; /* glyph 0x17 when slot < A4-$5E4D (0x12892) */
    int hp = 0;                   /* roster word +$5E (0x6274) */
    char name[16] = {0};
};

/** Protect panel values A4 -$79AB..-$79A6 @ 0x5E28 (doc 43 §4). */
struct PlayProtectValues {
    uint8_t light = 0;
    uint8_t magic = 0;
    uint8_t forces = 0;
    uint8_t levitate = 0;
    uint8_t walk_water = 0;
    uint8_t guard_dog = 0;
};

/** Solid black fill for a text-cell rectangle (clear_cell_rect @ 0x42DC). */
void fillCellRect(ScreenCompositor &c, int col, int row, int width_cells, int height_cells);

/** Full-screen black + red console_box for in-game Quick Ref / character sheet. */
void drawPlayModalBackdrop(ScreenCompositor &c);

/** Black interior fills + red border/divider glyphs (no dynamic text). */
void drawPlayScreenChromeStatic(ScreenCompositor &c);

void drawPlayScreenChrome(ScreenCompositor &c);

/** Red v-line col 0x1B rows 0..0x10 — repaint after 3D hood (walls overwrite x=216). */
void drawPlayViewportDivider(ScreenCompositor &c);

/** Combat round chrome: clear cols 1..0x26 rows 1..0x11 + rules (0x135BE). */
void drawCombatScreenChrome(ScreenCompositor &c);

/** Combat rules/patches only (repaint after the backdrop/sprite blits). */
void drawCombatScreenLines(ScreenCompositor &c);

/** Combat hood divider col 0x0F rows 0..0x0E — repaint after monster sprite blit. */
void drawCombatViewportDivider(ScreenCompositor &c);

void drawPlayStatusBar(ScreenCompositor &c, int day, int year, char facing_key, bool new_game);
void drawPlayPartyPanel(ScreenCompositor &c, const PlayPartySlot slots[8]);

enum class PlayRightPanel : uint8_t { Options = 0, Protect = 1 };

void drawPlayRightColumn(ScreenCompositor &c, PlayRightPanel panel, const PlayProtectValues *protect = nullptr);

}  // namespace mm2::gfx
