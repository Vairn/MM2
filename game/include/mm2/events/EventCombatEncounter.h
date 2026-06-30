#pragma once

#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventTextView.h"
#include "mm2/GameState.h"

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
 * The actual combat run (-$7EDE @ 0x051C2) and its post-combat pending-event
 * latch logic (-$7F1A → A4-$7952) are part of the combat engine, which is NOT
 * ported yet. Combat victory (A4-$77BD) is set by the combat engine
 * (combat_victory_rewards @ 0x12438), NOT by this handler.
 */
void eventRunFixedEncounter(GameStateView &gs, EventTextView &text, EventVmWait &wait,
                            const uint8_t *monster_block, int block_len, bool variant_b);

}  // namespace mm2::events