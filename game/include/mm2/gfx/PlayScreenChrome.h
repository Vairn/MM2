#pragma once
#include "mm2/gfx/ScreenCompositor.h"

// Play-screen HUD chrome, traced from the session refresh chain
// (0x52A2: -$7EA8 -> 0x60F4 chrome, -$7EA2 -> 0x6150 party panel,
//  -$7EBA -> 0x560A dividers+status, then 0x5E28 / 0x5D54 right column).

namespace mm2::gfx {

struct PlayPartySlot {
    bool present = false;
    bool bad_condition = false;  /* roster byte +$26 != 0 (0x6204) */
    int hp = 0;                  /* roster word +$5E (0x624C) */
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

void drawPlayScreenChrome(ScreenCompositor &c);
void drawPlayStatusBar(ScreenCompositor &c, int day, int year, char facing_key, bool new_game);
void drawPlayPartyPanel(ScreenCompositor &c, const PlayPartySlot slots[8]);

enum class PlayRightPanel : uint8_t { Options = 0, Protect = 1 };

void drawPlayRightColumn(ScreenCompositor &c, PlayRightPanel panel, const PlayProtectValues *protect = nullptr);

}  // namespace mm2::gfx
