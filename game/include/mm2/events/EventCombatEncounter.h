#pragma once

#include "mm2/combat/CombatSession.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventTextView.h"
#include "mm2/GameState.h"
#include "mm2/world/MapWorld.h"

namespace mm2::events {

/**
 * OP_12 / OP_13 encounter setup — faithful port of the event-VM handler @ 0x16300.
 *
 * Seeds the encounter-setup block (A4-$11DE / -$11D4 / -$796B / -$77BE / -$4F4E)
 * exactly as the ROM does, then sets the script-abort flag (A4-$79EA, written at
 * 0x16362) so the event interpreter yields to combat.
 *
 *   - OP_12 (variant_b=false): mode=0x80 (fixed fight). `monster_block` holds 12
 *     bytes: 10 monster-type ids + overflow_type + live_count.
 *   - OP_13 (variant_b=true):  mode=0    (seeded-random). `monster_block` holds 10
 *     monster-type ids; overflow_type and live_count are cleared.
 *
 * When `combat` and `world` are bound (GameSession::start), this also calls
 * combat->enter() right after seeding — the port equivalent of the ROM's
 * synchronous `jsr -$7EDE(a4)` @ 0x1635E/0x1638C. Post-combat pending-event
 * latch logic (-$7F1A → A4-$7952) is driven by the caller (GameSession) once
 * combat finishes, not by this handler.
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