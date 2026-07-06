#pragma once
// Random encounter picker — faithful port of 0x11E58 (xp budget init),
// 0x1213E/0x12072/0x11F0A (picker loop), and 0x12CE0 (live-count recompute).
// See EXTRACTED/docs/35-encounter-tables.md for the full ASM trace.

#include "mm2/GameState.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/world/MapWorld.h"

#include "mm2_attrib_codec.h"
#include "mm2_map_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::combat {

/** Stands in for the unpack thunk -$7EF6, which fetches a picked monster
 *  type's "friend count" field (monsters.dat Oabil low nibble, see
 *  MM2_MON_ADD_FRIENDS_COUNT). Pass nullptr to use a faithful default of 1. */
using FriendCountLookup = uint8_t (*)(uint8_t monster_type, const void *ctx);

/** 0x11E58: party_xp_budget (A4-$6FCA) = total party current HP / 8, scaled
 *  by disposition (A4-$79AE): /4 more if 0, /2 more if 1, unchanged if 2,
 *  x2 if 3. Also sets A4-$6FC2 = half the party's highest level. */
void encounterInitXpBudget(GameStateView &gs, const Mm2RosterFile &roster,
                            const Mm2PartyLaunch &launch);

/** 0x12CE0: live_count = count of non-zero monster_slot[0..9]; if
 *  overflow_type != 0, add the seeded live_count on top. Call once right
 *  after the event/arena/rest trigger seeds MM2_GS_MONSTER_SLOTS. */
uint8_t encounterRecomputeLiveCount(GameStateView &gs);

/** 0x1213E: loop of encounterPickerBudgetCheck + encounterAddsFriends until
 *  the budget/group-size gate is exhausted. Only runs when
 *  MM2_GS_ENCOUNTER_MODE lacks the 0x80 fixed-fight bit (mirrors the ASM
 *  gate at 0x12CFA — callers should check mode themselves, matching the
 *  ROM's `cmp.w #$80,d0 / bcc` at 0x12CFE). `friend_count_lookup` is
 *  forwarded to encounterAddsFriends. */
void encounterRunRandomPicker(GameStateView &gs, const Mm2AttribRecord &attrib,
                               gameplay::Rng &rng, FriendCountLookup friend_count_lookup = nullptr,
                               const void *ctx = nullptr);

/** 0x12072: recompute the picker "done" flag (MM2_GS_PICKER_DONE) from the
 *  current battle-slot tier cost vs MM2_GS_PARTY_XP_BUDGET and the
 *  attrib.dat group-size gate (byte 0x0A). Exposed for tests. */
void encounterPickerBudgetCheck(GameStateView &gs, const Mm2AttribRecord &attrib);

/** 0x11F0A: monster_adds_friends. Rolls a monster type (tier*16+rng(1,16)-1),
 *  a "friend count" via `friend_count_lookup`, and fills consecutive
 *  MM2_GS_MONSTER_SLOTS starting at the live count. */
void encounterAddsFriends(GameStateView &gs, const Mm2AttribRecord &attrib, gameplay::Rng &rng,
                          FriendCountLookup friend_count_lookup, const void *ctx);

/** Mirror screen enter @ 0x923E/0x16EA: seed A4-$861E (view) and A4-$89A0 (runtime env)
 *  from attrib — not on every step roll (retail reads live GS @ 0x10D6/0x10DE). */
void encounterSyncScreenContext(GameStateView &gs, const world::MapWorld &world);

/** 0x10A2 random step encounter @ departure tile: rate roll, -$4F4E/-$77BD suppress,
 *  path-1 (wilderness env $0A + class 4 → mode $02 preseed, else mode $0). Path-2
 *  @ 0x11AA (failed rate) is not random combat. Call before forward/back step. */
bool encounterTryStepRandom(GameStateView &gs, const world::MapWorld &world, gameplay::Rng &rng);

}  // namespace mm2::combat
