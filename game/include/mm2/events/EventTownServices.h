#pragma once

#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventTextView.h"
#include "mm2/GameState.h"
#include "mm2/world/MapWorld.h"

#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::events {

/** OP_0E town/building service handlers (doc 28). Mutates wait_ on rt when input needed. */
void eventExecTownSelector(EventRuntime &rt, GameStateView &gs, world::MapWorld &world,
                             uint8_t sel, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             const Mm2ItemsFile *items, EventTextView &text, EventVmWait &wait,
                             int location_id, const char *service_title);

}  // namespace mm2::events
