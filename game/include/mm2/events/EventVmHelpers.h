#pragma once

#include "mm2/world/MapWorld.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"
#include "mm2_gamestate.h"

namespace mm2::events {

/** True for OP_0E selectors handled as town/building services (stub in port). */
bool eventVmIsTownServiceSelector(uint8_t sel);

/** event_op_var_resolve @ 0x15620 — returns byte offset from A4, or 0 if unmapped. */
int32_t eventVmResolveVarOffset(uint8_t group, uint8_t index);

uint8_t eventVmLoadVar(const uint8_t *a4, uint8_t group, uint8_t index);
void eventVmStoreVar(uint8_t *a4, uint8_t group, uint8_t index, uint8_t val);

uint32_t eventVmPartyGoldTotal(const uint8_t *a4, const Mm2RosterFile *roster,
                               const Mm2PartyLaunch *launch);

/** OP_32 @ 0x17190 -> 0x04614: count nibbles of record+0x50 equal to `id`,
 * summed over living party members (condition < 0x81). Returns the raw count. */
int eventVmCountPartyNibbleMatches(const uint8_t *a4, const Mm2RosterFile *roster,
                                   const Mm2PartyLaunch *launch, uint8_t id);

bool eventVmPartyHasItem(const uint8_t *a4, const Mm2RosterFile *roster,
                         const Mm2PartyLaunch *launch, uint8_t item_id, bool consume);

void eventVmClearTileEventFlag(uint8_t *a4, int y, int x);

void eventVmPatchMapTile(world::MapWorld &world, int y, int x, uint8_t visual,
                         uint8_t collision);

/** OP_15/18 party field op. `selector` is the raw script field-selector byte
 * (0x00..0x7F); it is translated to the real record byte offset via the ROM
 * field map (EventFieldMap.h), NOT used directly as an offset. */
void eventVmApplyPartyByteOp(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             uint8_t count, uint8_t selector, uint8_t val, bool masked,
                             uint8_t and_m, uint8_t or_m, bool test_only, bool *out_cond);

bool eventVmCheckOp30Password(const uint8_t *input_buf, const uint8_t *expected,
                              size_t expected_len);

/** Deduct gold from party members (front-loaded) until amount satisfied. Returns amount left. */
uint32_t eventVmDeductPartyGold(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                uint32_t amount);

/** OP_2A treasure block: u24 gold+exp, u16 gems, 3× item triplets. */
void eventVmApplyTreasure(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          const uint8_t block[14]);

/** OP_0D engine_call index → side effects (0x09 = refresh hooks). */
void eventVmExecEngineCall(uint8_t *a4, uint8_t index, world::MapWorld *world);

/** OP_25 non-gold code check (tickets 208–211, keys 112–114, flags). */
bool eventVmCheckCode16(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                        uint16_t code);

/** Party slot 0..7 for OP_26/27 selection (stored 1..8 in GS). */
int eventVmSelectedPartySlot(const uint8_t *a4);
void eventVmSetSelectedPartySlot(uint8_t *a4, int slot_1_to_8);

/** Map screen 0..4 → training/healing town index (FAQ §3-6). */
int eventVmTrainingTownIndex(int map_screen);

/** Per-character TRAINING cost (FAQ §3-6, doc 34 §13.2):
 *  `current_level × training_town_index × 50` gp. `town_index` comes from
 *  eventVmTrainingTownIndex() (map→index [1,5,2,4,2]). This is the gold the
 *  deferred training engine (OP_0E 0x04 → A4 thunk -$7D16 → menu 0x8050; HP
 *  path 0x9BCA, stat path 0x1C898) charges per trained character. Exposed for
 *  the future engine port + unit tests. Status: FAQ-sourced; the exact
 *  multiply/index sequence in 0x9B48/0x9BCA is not yet ASM-confirmed. */
uint32_t eventVmTrainingCostPerChar(int level, int town_index);

/** Per-character base HEALING cost (FAQ §3-6, doc 34 §13.2) for a LIVING
 *  character: `current_level × training_town_index × 10` gp (same town index as
 *  training). The FAQ dead (×10) / eradicated (×100) multipliers are applied
 *  inside the deferred temple engine (OP_0E 0x03 → 0x1D208) and are NOT folded
 *  in here: roster condition byte $26 only groups $80+ as Dead/Stone/Eradicated
 *  (doc 06), and the dead-vs-eradicated threshold is not yet ASM-confirmed.
 *  Exposed for the future engine port + unit tests. */
uint32_t eventVmHealingCostPerChar(int level, int town_index);

}  // namespace mm2::events
