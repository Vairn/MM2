#pragma once
// overland_map_view @0x223A — 16×16 cartography blit into the play viewport.
// Tile size 14×11 (townb.32 / outb.32). Unexplored cells are skipped (helper @0x2182 → 0xFF).

#include "mm2/GameState.h"
#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/world/AutomapState.h"
#include "mm2/world/MapWorld.h"

namespace mm2::gfx {

struct AutomapRenderParams {
    int origin_x = 8;
    int origin_y = 8;
    bool draw_edge_overlay = true;
    bool draw_party_marker = true;
};

void renderAutomap(ScreenCompositor &c, const EnvAssets &env, const world::MapWorld &world,
                   const world::AutomapState &vis, const GameStateView &gs,
                   const AutomapRenderParams &params = {});

int automapPartyArrowFrame(char facing_key);

}  // namespace mm2::gfx
