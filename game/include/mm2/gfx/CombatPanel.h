#pragma once

#include "mm2/gfx/ScreenCompositor.h"

namespace mm2::gfx {

/** One row in the combat monster list (slots A–J). */
struct CombatMonsterLine {
    char letter = 0;       /* 'A'..'J' when label_monster_slots */
    char name[16]{};
    bool show_checkmark = false; /* glyph 0x7E — combat_monster_line @ 0x1374A */
    char status_suffix[16]{};    /* e.g. "Hurt" from A4-$7026 table (0x1374A) */
};

/** Snapshot for drawing the retail combat HUD (panel mode A4-$79B2 == 2). */
struct CombatPanelView {
    char message[160]{};       /* status / surprise text on row 0x11 */
    char options_for[16]{};    /* active character name when awaiting a command */
    bool show_command_options = false; /* draw per-character A/B/R bar */
    bool show_party_options = false;   /* draw party-level A/B/H/R bar (0x13222) */
    bool label_monster_slots = false;  /* A–J prefixes (round loop only) */
    int monster_line_count = 0;
    CombatMonsterLine monster_lines[10]{};
    int overflow_more = 0; /* "+N more" tail when live count > 10 */
    char overflow_name[16]{};
    int sprite_disk_index = -1; /* monsters.dat picture & 0x7F → NN.anm */
};

/** Black viewport interior (replaces exploration 3D hood during combat). */
void drawCombatViewport(ScreenCompositor &c);

/** Monster name list in the right column (yellow-bordered box). */
void drawCombatMonsterList(ScreenCompositor &c, const CombatPanelView &view);

/** Options / message strip on row 0x11. */
void drawCombatOptionsBar(ScreenCompositor &c, const CombatPanelView &view);

}  // namespace mm2::gfx
