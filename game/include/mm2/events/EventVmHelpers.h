#pragma once

#include "mm2/world/MapWorld.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"
#include "mm2_gamestate.h"
#include "mm2_event_codec.h"

namespace mm2::events {

/** True for OP_0E selectors handled as town/building services (stub in port). */
bool eventVmIsTownServiceSelector(uint8_t sel);

/** Default-path binning @ asm 0x15EDC (doc 07 §OP_0E). */
struct Mm2ExecSelectorBin {
    uint8_t category = 0; /* 0x3C..0x46 written to A4-$79F2 during -$7DFA call */
    uint8_t index = 0;    /* adjusted slot stored in A4-$5D46 */
    bool matched = false;
};

Mm2ExecSelectorBin eventVmBinExecSelector(uint8_t sel);

/** Raw bytes of overlay string-bank entry `idx` (0xFF-delimited pool). */
bool eventVmLocationStringRaw(const Mm2EventLocation *loc, int idx, const uint8_t **out,
                              size_t *out_len);

/** True when a string-bank entry is event bytecode (e.g. loc 61 str[22] = OP_12). */
bool eventVmStringLooksLikeBytecode(const uint8_t *bytes, size_t len);

/** Token-skip length delta for opcodes 0x00..0x32, byte-exact from the ROM's
 *  opcode_len_tbl @ A4-$6CC8 (data hunk file offset 0x1336; source of truth:
 *  tools/dump_event_token_table.py / EXTRACTED/event_token_len_table.json).
 *  Used ONLY by the skip-token walker (OP_10/OP_11/OP_2B via thunk 0x157FC)
 *  to advance PAST a token it does not execute. This is distinct from (and,
 *  for 2 opcodes, different than) "1 + argc":
 *    - op 0x00 (invalid): ROM skip delta is 0, not 1.
 *    - op 0x25 (OP_25 check-code16): the handler reads 2 argument bytes when
 *      EXECUTED (own length 3), but the ROM skip-table entry is only 2
 *      (opcode + 1 byte) — a genuine ROM quirk. A skip walk that passes OVER
 *      an OP_25 token (rather than executing it) under-counts by one byte in
 *      the original game, desyncing the token stream by 1. Returns 1 for
 *      opcodes outside 0x00..0x32 (should not occur; dispatcher aborts first). */
uint8_t eventVmTokenDelta(uint8_t op);

/** event_op_var_resolve @ 0x15620 — returns byte offset from A4, or 0 if unmapped. */
int32_t eventVmResolveVarOffset(uint8_t group, uint8_t index);

uint8_t eventVmLoadVar(const uint8_t *a4, uint8_t group, uint8_t index);
void eventVmStoreVar(uint8_t *a4, uint8_t group, uint8_t index, uint8_t val);

/** Eagle/Wizard Eye step timers @ 0x4672 / doc 19. `outdoor_view` mirrors
 *  A4-$79E2 (non-zero = outdoor surface → Eagle Eye `-$79A0`). */
uint8_t eventVmSpellEyeTimer(const uint8_t *a4, bool outdoor_view);
void eventVmTickSpellEyeOnStep(uint8_t *a4, bool outdoor_view);

uint32_t eventVmPartyGoldTotal(const uint8_t *a4, const Mm2RosterFile *roster,
                               const Mm2PartyLaunch *launch);

/** OP_32 @ 0x17190 -> 0x04614: count nibbles of record+0x50 equal to `id`,
 * summed over living party members (condition < 0x81). Returns the raw count. */
int eventVmCountPartyNibbleMatches(const uint8_t *a4, const Mm2RosterFile *roster,
                                   const Mm2PartyLaunch *launch, uint8_t id);

bool eventVmPartyHasItem(const uint8_t *a4, const Mm2RosterFile *roster,
                         const Mm2PartyLaunch *launch, uint8_t item_id, bool consume);

void eventVmClearTileEventFlag(uint8_t *a4, int y, int x);

/** event_tile_scanner post-fight @ 0x1773A/0x17756: clear runtime + map collision
 *  event bits so the tile does not re-arm until map reload. */
void eventVmConsumeTileEncounterFlag(uint8_t *a4, world::MapWorld &world, int y, int x);

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

/* ---------------------------------------------------------------------------
 * Arena Games ticket-combat engine (ASM 0x9D76). CORRECTED 2026-07: this
 * routine is the FIXED, sole target of explicit OP_0E selector 0x08's thunk
 * -$7DBE — byte-verified by reading the A4 vtable trampoline table directly
 * out of EXTRACTED/ghidra/mm2_data_00.bin (file offset 0x7FFE+disp):
 *   -$7DBE (file off 0x0240): `4EF9 00009D76` -> 0x09D76 (this engine)
 *   -$7DFA (file off 0x0204): `4EF9 000092F2` -> 0x092F2 (event_dat_loader)
 * A prior pass misread this and claimed -$7DFA (the OP_0E DEFAULT-range
 * dispatch thunk, called from 0x160AE after 0x15EDC bins the selector into
 * category 0x3C-0x46) was the arena's target; that made the port fabricate
 * "you must have a ticket to compete" text for every unrelated default-range
 * selector (Olympic trials, post-victory reward tiers, the game-start Corak
 * monologue, ...). The default-range thunk actually re-enters the SAME
 * event_dat_loader used to enter a normal on-map location (0x92F2) — almost
 * certainly to load one of event.dat's non-map "string bank" pseudo-records
 * (e.g. decoder location 60, "Nordon/Nordonna/Corak") keyed by the
 * category/index pair; that reinvocation is not reverse-engineered yet, so
 * EventTownServices.cpp's default case no longer guesses at it. This engine
 * is NOT shared by explicit selector 0x07 (general store, thunk -$7DB8 ->
 * 0xA62C, also byte-verified) — a distinct fixed address. Selectors that DO
 * reach this engine are only those whose event.dat script encodes OP_0E 0x08
 * directly (e.g. Middlegate's arena-entrance tile). Every default-range
 * selector (Atlantium Olympic trials 0x12-0x25, Vulcania/Sandsobar arena
 * tiers, post-victory combat tiers 0x26-0x29/0x4A-0x4F, Mount Farview reward
 * 0x97, etc.) does NOT funnel into this code — only explicit selector 0x08
 * does, unconditionally:
 *   1) scans every party member's BACKPACK ONLY (record+0x3A..0x3F, NOT
 *      equipped slots) for a ticket item 0xD0..0xD3 (asm 0x9D9C-0x9DDA);
 *   2) on miss: "Sorry, but you must have a ticket to compete in these
 *      games." (str @ code 0xA082/0xA0A7) — no combat;
 *   3) on hit: consumes the ticket (thunk -$7F26), shows "The games master
 *      accepts your ticket.  Let the battle begin!" (0xA0BF/0xA0E5), and
 *      arms a FIXED encounter via the same battle-slot array as OP_12/13
 *      (A4-$11DE), monster type = ((color*3 + area[screen]) * 16) +
 *      rng(1,16) (asm 0x9E86-0x9EC2, area table @ data hunk 0xE74);
 *   4) on victory (combat engine not ported — see EventCombatEncounter.h):
 *      the ROM grants gold from a 4(color) x 3(town tier) table (data hunk
 *      0xE7A) to the first eligible party member and prints "Winner, you
 *      receive N gold" (0xA0FC/0xA111) — plus a documented ROM bug that
 *      corrupts record+0x79 (doc 36-class-quest-hp-bug.md). Reward granting
 *      is intentionally NOT performed by this port yet (no combat-victory
 *      callback exists), matching OP_12/13's fidelity level. */

struct Mm2ArenaTicket {
    bool found = false;
    uint8_t color = 0;      /* 0 green .. 3 black (item id 0xD0..0xD3 minus 0xD0) */
    int member_slot = -1;   /* party slot 0..7 whose backpack held the ticket */
    int backpack_slot = -1; /* backpack slot 0..5 (record+0x3A+slot) */
};

/** asm 0x9D9C-0x9DDA: member-major, backpack-slot-minor scan; first match wins. */
Mm2ArenaTicket eventVmFindArenaTicket(const uint8_t *a4, const Mm2RosterFile *roster,
                                      const Mm2PartyLaunch *launch);

/** asm 0x9E40 (thunk -$7F26): remove the ticket located by eventVmFindArenaTicket. */
void eventVmConsumeArenaTicket(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                const Mm2ArenaTicket &ticket);

/** Per-screen difficulty add-in, data hunk 0xE74 (A4-$718A): Middlegate,
 *  Atlantium, Tundara, Vulcania, Sandsobar. */
extern const uint8_t kArenaAreaIndex[5];

/** asm 0x9E86-0x9EC2 fixed-encounter monster-type id. `rng_1_to_16` must be
 *  in [1,16] (asm rng(1,16) via thunk -$7BB4, doc 43 Rng::range contract). */
uint8_t eventVmArenaMonsterType(uint8_t color, int screen, int rng_1_to_16);

/** Gold reward table, data hunk 0xE7A (A4-$7184): 4 ticket colors x 3 town
 *  tiers (tier = min(screen,2): Middlegate=0, Atlantium=1, all others=2). */
extern const uint32_t kArenaGoldReward[4][3];
uint32_t eventVmArenaGoldReward(uint8_t color, int screen);

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
