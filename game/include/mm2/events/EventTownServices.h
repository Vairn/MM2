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

/** OP_0E 0x0D guild enroll after y/n: 20 gp + per-town membership in record+0x79
 *  (same field as Sandsobar/Vulcania apply_party_masked selector 0x74) and the
 *  ASM-confirmed home-town write @ 0x1A1CE (record+0x0B). Returns false when
 *  gold is insufficient (sets text + Space wait). */
bool eventApplyGuildEnroll(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                           EventTextView &text, EventVmWait &wait, int map_id);

/** OP_0E default-range selector 0x10 @ Middlegate (event 16): brain detox after
 * member pick. Clears roster+0x50 skill pack (ASM selector 0x6D) and deducts
 * 100 gp from the selected member (FAQ §8-3; loc-60 overlay script not decoded). */
bool eventApplyBrainDetox(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          EventTextView &text, EventVmWait &wait);

}  // namespace mm2::events
