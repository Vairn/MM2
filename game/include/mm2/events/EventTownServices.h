#pragma once

#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventTextView.h"
#include "mm2/GameState.h"
#include "mm2/world/MapWorld.h"

#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::events {

/** OP_0E town/building service handlers (doc 28). Mutates wait_ on rt when input needed.
 *  `rng` feeds the default-range Arena Games ticket engine's rng(1,16) roll
 *  (asm 0x9D76 @ thunk -$7BB4); may be null (falls back to a fixed roll). */
void eventExecTownSelector(EventRuntime &rt, GameStateView &gs, world::MapWorld &world,
                             uint8_t sel, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             const Mm2ItemsFile *items, EventTextView &text, EventVmWait &wait,
                             int location_id, const char *service_title, gameplay::Rng *rng);

/** Run the bound town-service menu after a pub/temple intro Y/N (see
 *  EventRuntime::PendingTownMenu). Returns true when a menu backend opened. */
bool eventTownServiceRunBoundMenu(EventRuntime &rt, GameStateView &gs,
                                  Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                  const Mm2ItemsFile *items, int location_id,
                                  EventRuntime::PendingTownMenu kind);

/** Inn registry @ 0x1A1B2: replace roster+$0B with map+1 for each active party slot. */
void eventInnApplyRegistry(Mm2RosterFile *roster, const Mm2PartyLaunch *launch, int map_id);

/* Guild enroll (0x0D) / locksmith (0x10) / Poorman's Portal (0x11) are
 * default-range overlay scripts — no FAQ stubs; see runDefaultRangeOverlay. */

}  // namespace mm2::events
