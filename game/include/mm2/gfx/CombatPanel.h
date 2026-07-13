#pragma once

#include "mm2/gfx/ScreenCompositor.h"

namespace mm2::gfx {

/** One row in the combat monster list (slots A–J). */
struct CombatMonsterLine {
    char letter = 0;       /* 'A'..'J' when label_monster_slots */
    char name[16]{};
    /* Glyph 0x17 (check) when slot < A4-$524 melee-range count — combat_monster_list_line
     * @ 0x12742. NOT an acted flag: -$524 is rolled per fight by 0x11D0C. */
    bool show_checkmark = false;
    char status_suffix[16]{};    /* 4-char abbrev from A4-$6FBC via -$519[slot] (0x127FE) */
    bool highlighted = false;    /* 0x1061E / 0x1265E acting-slot pen flash */
};

/** AGA multi-monster viewport slot (presentation upgrade — see 41-aga-port-plan §8). */
struct CombatSpriteSlot {
    int disk_index = -1;   /* monsters.dat picture & 0x7F → NN.anm */
    int stack_count = 0;   /* alive slots sharing this picture id */
    int battle_slot = -1;  /* first alive battle slot with this picture */
};

/** Cap distinct on-screen combat BOBs (Chip RAM); raise after hardware measure. */
constexpr int kAgaCombatSpriteCap = 4;

/** Snapshot for drawing the retail combat HUD (panel mode A4-$79B2 == 2). */
struct CombatPanelView {
    char message[160]{};       /* status / surprise text, message band row 0x0F */
    char options_for[16]{};    /* active character name when awaiting a command */
    bool show_command_options = false; /* draw per-character command grid (0x11866) */
    bool show_party_options = false;   /* draw party-level A/B/H/R bar (0x13222) */
    bool show_bribe_kind = false;      /* 0x12FB8 "Bribe with:  1-Food  2-Gold  3-Gems" */
    bool show_bribe_amount = false;    /* 0x1326D "How much?" + digit entry */
    bool show_cast_level = false;      /* 0x79EE " Spell Level: " (combat cast, no grid) */
    bool show_cast_number = false;     /* 0x79EE "Number: " after level digit */
    bool show_cast_target = false;     /* 0xD43C "Which monster?" + A–J */
    bool show_party_pick = false;      /* 0xD2EA "On whom (1-N)?" */
    bool show_item_pick = false;       /* 0xB56E "A-F" / Use 1-6/A-F */
    int cast_level = 0;                /* echoed after level accepted */
    bool label_monster_slots = false;  /* A–J prefixes (round loop only) */
    /* 0x1061E → 0x1265E: pen=1 redraw of the acting monster roster line. */
    int highlight_slot = -1;           /* battle slot 0..9, or -1 */
    /* Command capability flags for the active character (0x11866):
     *   melee  A4-$5E36: slot < front-rank cutoff -$5E4D
     *   shoot  A4-$5E35: (class == 2 or back rank) and bow byte +$4E
     *   cast   A4-$5E34: not silenced (+$26 bit1), caster (+$72), SP (+$58) */
    bool opt_melee = false;
    bool opt_shoot = false;
    bool opt_cast = false;
    int monster_line_count = 0;
    CombatMonsterLine monster_lines[10]{};
    int overflow_more = 0; /* live count - 10 when > 10 (0x12692 / 0x12E22) */
    char overflow_name[16]{};
    /** First alive picture (retail single-sprite path / legacy callers). */
    int sprite_disk_index = -1;
    /** Distinct picture ids for AGA multi-BOB gallery (back-to-front = low→high). */
    int sprite_slot_count = 0;
    CombatSpriteSlot sprite_slots[kAgaCombatSpriteCap]{};
};

/** Black viewport interior (replaces exploration 3D hood during combat). */
void drawCombatViewport(ScreenCompositor &c);

/** Yellow console_box around the narrow combat hood (combat_display_refresh @ 0x136AA). */
void drawCombatViewportFrame(ScreenCompositor &c);

/** Message / options band rows 0x0F..0x11 (cleared+printed by 0x1119E / 0x11866). */
void drawCombatOptionsBar(ScreenCompositor &c, const CombatPanelView &view);

/** Right column during combat: round roster (0x129CC) or pre-combat name box (0x12DA2). */
void drawCombatRightColumn(ScreenCompositor &c, const CombatPanelView &view);

}  // namespace mm2::gfx
