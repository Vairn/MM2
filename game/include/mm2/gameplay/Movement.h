#pragma once
// Exploration movement — turn @ 0x5838, step @ 0x5816/0x56FC/0x5762,
// passability @ 0x9424, screen edge @ 0x1D0A, time tick @ 0x69DC.
// See EXTRACTED/docs/43-exploration-input-and-ingame-options.md.

#include "mm2/GameState.h"
#include "mm2/world/MapWorld.h"

namespace mm2::gameplay {

/* Raw exploration codes from the indoor/outdoor key readers (doc 43). */
enum class ExploreCode : uint8_t {
    TurnLeft = 0xF0,
    TurnRight = 0xF1,
    Forward = 0xF2,
    Back = 0xF3,
};

struct MoveResult {
    bool acted = false;
    bool turned = false;
    bool moved = false;
    bool blocked = false;
    bool screen_changed = false;
};

/* Rotate facing without moving (0x5838). Sets event latch -$7952. */
MoveResult turn(world::MapWorld &world, GameStateView &gs, bool right_cw);

/* Step forward/back (0x5816 → 0x56FC / 0x5762). Sets -$4F4E; applies 0x69DC
 * when forward/back succeeds. Screen edge uses attrib neighbours @ 0x1D0A. */
MoveResult step(world::MapWorld &world, GameStateView &gs, bool forward);

/* time_tick @ 0x69DC(n): n==1 on successful step drains light on dark tiles. */
void applyStepTimeTick(GameStateView &gs, uint8_t collision_cell_at_dest);

}  // namespace mm2::gameplay
