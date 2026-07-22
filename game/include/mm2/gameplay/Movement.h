#pragma once
// Exploration movement — turn @ 0x5838, step @ 0x5816/0x56FC/0x5762,
// passability @ 0x9424, screen edge @ 0x1D0A, time tick @ 0x69DC.
// See EXTRACTED/docs/43-exploration-input-and-ingame-options.md.

#include "mm2/GameState.h"
#include "mm2/world/MapWorld.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

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
 * latchExploreEventsAfterMove after the step encounter check (doc 43 loop order).
 * Optional roster/launch feed day-rollover aging @ 0x6988. */
MoveResult step(world::MapWorld &world, GameStateView &gs, bool forward,
                Mm2RosterFile *roster = nullptr, const Mm2PartyLaunch *launch = nullptr);

/* After movement: set -$4F4E, clear -$77BD, latch tile events (0x5748 tail). */
void latchExploreEventsAfterMove(GameStateView &gs);

/* time_tick @ 0x69DC(n): n==1 on successful step drains light on dark tiles.
 * Optional roster/launch feed day-rollover aging @ 0x6988. */
void applyStepTimeTick(GameStateView &gs, uint8_t collision_cell_at_dest,
                       Mm2RosterFile *roster = nullptr, const Mm2PartyLaunch *launch = nullptr);

/* time_tick @ 0x69DC(n): advance the clock by n sub-day units and roll the
 * calendar over at 0x100. No light drain / no UI redraw (those are n==1 only).
 * Used by Rest (n=0x55 @ 0x19CEC). */
void advanceTimeTick(GameStateView &gs, uint16_t n, Mm2RosterFile *roster = nullptr,
                     const Mm2PartyLaunch *launch = nullptr);

/* Screen-enter materialize @ 0x923E: copy current attrib.raw[64] → A4-$561A
 * so defeat/rest/doors can read -$560C / -$5600 / door bytes from GS. */
void materializeScreenAttrib(GameStateView &gs, const world::MapWorld &world);

/* Hood-refresh current-cell latch @ 0x1B1C: collision[(y<<4)|x] → -$55D6. */
void syncCurrentCellFlags(GameStateView &gs, const world::MapWorld &world);

/* session_interaction_gate darkness leaf @ 0x53C0..0x53E8:
 * clr -$79E1; if light==0 and (-$5600>=$80 or bit5 of -$55D6) → set -$79E1. */
void sessionInteractionGate(GameStateView &gs);

/* Rest SP bonus lookup @ 0x4442 (table A4-$7486): returns the unsigned tier
 * byte used at 0x19C74 before addq #3. Attr is INT or PER per class @ 0x19C48. */
uint8_t restSpellBonusFactor(uint8_t attr);

/* Align Amiga working level (+$20) / spell-level (+$23) with remake-canonical
 * +$71/+72. Stock roster starters ship +$20=1 while +$71=4; Rest @ 0x19C9A
 * multiplies by +$20, so a stale working byte leaves casters at creation SP. */
void syncRosterWorkingLevelFields(Mm2RosterRecord &rec);

/* Rest SP recompute @ 0x19C30: if +$23!=0, SP = (bonus+3)*+$20 → +$5A/+ $58.
 * Call syncRosterWorkingLevelFields first when canonical fields may have drifted. */
void recomputeRestSpellPoints(Mm2RosterRecord &rec);

}  // namespace mm2::gameplay
