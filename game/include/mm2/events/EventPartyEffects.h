#pragma once

#include "mm2/GameState.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::events {

/** OP_1F / OP_20 party-effect dispatcher @ 0x167B0 (partial port). */
void eventApplyPartyEffect(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                           uint8_t sel, const uint8_t args[5], bool mode_b);

}  // namespace mm2::events
