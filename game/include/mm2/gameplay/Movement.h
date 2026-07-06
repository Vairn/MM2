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

/* Step forward/back (0x5816 → 0x56FC / 0x5762). Applies 0x69DC when forward/back
 * succeeds. Screen edge uses attrib neighbours @ 0x1D0A. Call
 * latchExploreEventsAfterMove after the step encounter check (doc 43 loop order). */
MoveResult step(world::MapWorld &world, GameStateView &gs, bool forward);

/* After movement: set -$4F4E, clear -$77BD, latch tile events (0x5748 tail). */
void latchExploreEventsAfterMove(GameStateView &gs);

/* time_tick @ 0x69DC(n): n==1 on successful step drains light on dark tiles. */
void applyStepTimeTick(GameStateView &gs, uint8_t collision_cell_at_dest);

/* time_tick @ 0x69DC(n): advance the clock by n sub-day units and roll the
 * calendar over at 0x100. No light drain / no UI redraw (those are n==1 only).
 * Used by Rest (n=0x55 @ 0x19CEC). */
void advanceTimeTick(GameStateView &gs, uint16_t n);

}  // namespace mm2::gameplay
