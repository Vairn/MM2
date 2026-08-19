#pragma once

#include "mm2/combat/CombatSession.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventTextView.h"
#include "mm2/GameState.h"
#include "mm2/world/MapWorld.h"

namespace mm2::events {

/**
 * OP_12 / OP_13 @ 0x16300. Seeds A4-$11DE / -$11D4 / -$796B / -$77BE / -$4F4E
 * and aborts the script (A4-$79EA @ 0x16362) so combat can run.
 *
 *   OP_12 (variant_b=false): mode=0x80. 12-byte block: 10 types + overflow + live_count.
 *   OP_13 (variant_b=true):  mode=0.    10 types; overflow and live_count cleared.
 *
 * If `combat` and `world` are bound, calls combat->enter() after seed
 * (`jsr -$7EDE(a4)` @ 0x1635E/0x1638C). Pending-event latch (-$7F1A → A4-$7952)
 * is GameSession's job after the fight ends.
 */
void eventRunFixedEncounter(GameStateView &gs, EventTextView &text, EventVmWait &wait,
                            const uint8_t *monster_block, int block_len, bool variant_b,
                            combat::CombatSession *combat = nullptr,
                            const world::MapWorld *world = nullptr);

/** Collision-page event flag with no matching event.dat triplet @ 0x176F2:
 *  clear slots, mode=0 (random picker), jsr -$7EDE. Caller clears the map
 *  collision 0x80 bit after a successful enter (0x17756). */
void eventRunTileAmbientEncounter(GameStateView &gs, combat::CombatSession *combat,
                                  const world::MapWorld *world);

/** OP_0E 0xFD / 0x1493C @ 0x14A92: fixed endgame fight — slots
 *  $FF,$E1,$C2,$C1,$E0, mode $83, clear 5..10, jsr -$7EDE. */
void eventRunOp0eFdEncounter(GameStateView &gs, combat::CombatSession *combat,
                             const world::MapWorld *world);

}  // namespace mm2::events