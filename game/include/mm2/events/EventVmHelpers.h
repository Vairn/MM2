#pragma once

#include "mm2/world/MapWorld.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/events/EventVmRegs.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"
#include "mm2_items_codec.h"
#include "mm2_gamestate.h"
#include "mm2_event_codec.h"

#include <cstddef>
#include <cstdint>

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

/** Skip-table length for opcodes 0x00..0x32 from ROM opcode_len_tbl @ A4-$6CC8
 *  (data hunk 0x1336). Used only by OP_10/OP_11/OP_2B (thunk 0x157FC). Distinct
 *  from execute length: op 0x00 skip is 0 not 1; op 0x25 executes 3 bytes but
 *  the skip entry is 2 (ROM under-counts by 1). Returns 1 outside 0x00..0x32. */
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

/** roster_count_living_chars @ 0x47A2: count party slots with (condition & 0xE0)==0.
 *  OP_31 abort gate `-$7F14`→`0x47EC` returns nonzero (→ SCRIPT_ABORT) when this is 0. */
int eventVmCountLivingPartyMembers(const uint8_t *a4, const Mm2RosterFile *roster,
                                   const Mm2PartyLaunch *launch);

/* SoA item runs on a 0x82-byte record (not Mm2RosterRecord named slots). */
constexpr int kRosterEquipId = 0x28;
constexpr int kRosterEquipCharges = 0x2E;
constexpr int kRosterEquipFlags = 0x34;
constexpr int kRosterBackpackId = 0x3A;
constexpr int kRosterBackpackCharges = 0x40;
constexpr int kRosterBackpackFlags = 0x46;
constexpr int kRosterItemSlots = 6;

inline const uint8_t *rosterRecordBytes(const Mm2RosterRecord &rec)
{
    return reinterpret_cast<const uint8_t *>(&rec);
}
inline uint8_t *rosterRecordBytes(Mm2RosterRecord &rec)
{
    return reinterpret_cast<uint8_t *>(&rec);
}
inline uint8_t rosterEquipId(const uint8_t *rec, int slot)
{
    return rec[kRosterEquipId + slot];
}
inline uint8_t rosterEquipCharges(const uint8_t *rec, int slot)
{
    return rec[kRosterEquipCharges + slot];
}
inline uint8_t rosterEquipFlags(const uint8_t *rec, int slot)
{
    return rec[kRosterEquipFlags + slot];
}
inline uint8_t rosterBackpackId(const uint8_t *rec, int slot)
{
    return rec[kRosterBackpackId + slot];
}
inline uint8_t rosterBackpackCharges(const uint8_t *rec, int slot)
{
    return rec[kRosterBackpackCharges + slot];
}
inline uint8_t rosterBackpackFlags(const uint8_t *rec, int slot)
{
    return rec[kRosterBackpackFlags + slot];
}
inline void rosterSetEquipId(uint8_t *rec, int slot, uint8_t id)
{
    rec[kRosterEquipId + slot] = id;
}
inline void rosterSetEquipCharges(uint8_t *rec, int slot, uint8_t charges)
{
    rec[kRosterEquipCharges + slot] = charges;
}
inline void rosterSetEquipFlags(uint8_t *rec, int slot, uint8_t flags)
{
    rec[kRosterEquipFlags + slot] = flags;
}
inline void rosterSetBackpackId(uint8_t *rec, int slot, uint8_t id)
{
    rec[kRosterBackpackId + slot] = id;
}
inline void rosterSetBackpackCharges(uint8_t *rec, int slot, uint8_t charges)
{
    rec[kRosterBackpackCharges + slot] = charges;
}
inline void rosterSetBackpackFlags(uint8_t *rec, int slot, uint8_t flags)
{
    rec[kRosterBackpackFlags + slot] = flags;
}
inline int rosterFirstEmptySlot(const uint8_t *rec, int id_base)
{
    for (int m = 0; m < kRosterItemSlots; ++m) {
        if (rec[id_base + m] == 0) {
            return m;
        }
    }
    return -1;
}
inline int rosterFirstEmptyBackpack(const uint8_t *rec)
{
    return rosterFirstEmptySlot(rec, kRosterBackpackId);
}
inline int rosterFirstEmptyEquip(const uint8_t *rec)
{
    return rosterFirstEmptySlot(rec, kRosterEquipId);
}
inline void rosterWriteEquip(uint8_t *rec, int slot, uint8_t id, uint8_t charges, uint8_t flags)
{
    rosterSetEquipId(rec, slot, id);
    rosterSetEquipCharges(rec, slot, charges);
    rosterSetEquipFlags(rec, slot, flags);
}
inline void rosterWriteBackpack(uint8_t *rec, int slot, uint8_t id, uint8_t charges, uint8_t flags)
{
    rosterSetBackpackId(rec, slot, id);
    rosterSetBackpackCharges(rec, slot, charges);
    rosterSetBackpackFlags(rec, slot, flags);
}
inline bool rosterPlaceInFirstEmptyBackpack(uint8_t *rec, uint8_t id, uint8_t charges, uint8_t flags)
{
    const int m = rosterFirstEmptyBackpack(rec);
    if (m < 0) {
        return false;
    }
    rosterWriteBackpack(rec, m, id, charges, flags);
    return true;
}

/** OP_19 backpack place: first empty +$3A slot, else found-item overflow. Returns true
 *  when placed on a member (cond=1 path). */
bool eventVmPartyGiveItem(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          uint8_t item_id, uint8_t charges, uint8_t flags);

/** Scan equipped+backpack. `consume` removes from first match (either bank). */
bool eventVmPartyHasItem(const uint8_t *a4, const Mm2RosterFile *roster,
                         const Mm2PartyLaunch *launch, uint8_t item_id, bool consume);

/** OP_28 @ 0x16C86: backpack-only (+$3A) scan; always consumes on hit → cond. */
bool eventVmPartyConsumeBackpackItem(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                     uint8_t item_id);

/** OP_14 @ 0x16398: andi #$7F on collision-page copy -$54BA[(y<<4)|x] and the
 *  current-cell latch -$55D6. Collision bit7 gates the ambient random-encounter
 *  roll, not scripted triplets — callers that need "don't re-fire this tile
 *  event until map reload" must also EventRuntime::markTileEventResolved. */
void eventVmClearTileEventFlag(uint8_t *a4, world::MapWorld &world, int y, int x);

/** event_tile_scanner post-fight @ 0x1773A/0x17756: clear runtime + map collision
 *  event bits so the ambient path does not re-arm until map reload. */
void eventVmConsumeTileEncounterFlag(uint8_t *a4, world::MapWorld &world, int y, int x);

void eventVmPatchMapTile(world::MapWorld &world, int y, int x, uint8_t visual,
                         uint8_t collision);

/** OP_15/18 @ 0x16426. `member_spec` is the first script byte (0=all, 1..8=one,
 *  9=selected). `selector` maps via EventFieldMap.h. Test mode (OP_15) ORs
 *  field/(field&val) into COND_FLAG; masked mode (OP_18) writes (f&and)|or.
 *  Returns the final COND_FLAG byte (test) or 0 (masked). */
uint8_t eventVmApplyPartyByteOp(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                uint8_t member_spec, uint8_t selector, uint8_t val, bool masked,
                                uint8_t and_m, uint8_t or_m);

/** OP_31 → 0x4952 with out-flags=0 (ASM call pattern from 0x170BC): subtract
 *  `damage` from roster +$5E (working HP word). Skips if +$26 >= 0x80. On
 *  lethal: bset bit6 on +$26; if bit6 already set → +$26=0x81; clear +$5E. */
void eventVmApplyOp31Damage(Mm2RosterRecord *rec, uint16_t damage);

/** OP_31 member-spec resolution + per-target 0x4952 (out-flags zeroed). */
void eventVmOp31IterateDamage(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                              uint8_t member_spec, uint16_t damage);

/** Search key @ 0x4800 → 0x1B19C. */
enum class SearchPrepareResult : uint8_t {
    Nothing = 0,       /* "Nothing Here!" */
    Distributed,       /* short path: loot already given (sentinel was non-0) */
    NeedIdentify,      /* long path: rating ready; host '1'..'4' @ 0x1B3F2 */
};

struct SearchPrepareOut {
    uint8_t rating = 0; /* display rating after 0x1B270 re-roll */
    char container_name[24]{}; /* -$6A54[env_row][score-1] @ 0x1B37A */
    /* Gold/gems + up to 3 "Name found Item" lines for party-panel reward UI. */
    char msg[256]{};
};

/** Prepare Search: nothing / short-path distribute / long-path Identify modal.
 *  Long path does NOT clear the found buffer — call distribute after Open/Find.
 *  Optional `items` names loot on the short-path distribute message. */
SearchPrepareResult eventVmSearchPrepare(uint8_t *a4, Mm2RosterFile *roster,
                                         const Mm2PartyLaunch *launch, gameplay::Rng *rng,
                                         SearchPrepareOut *out,
                                         const Mm2ItemsFile *items = nullptr);

/** Distribute found buffer @ 0x1AC94 (after Open/Find or short path).
 *  `msg` receives the party-panel reward text (0x1ACFA): share line then
 *  per-item "Name found Item" lines (optional `items` for names). */
bool eventVmSearchDistribute(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             char *msg, size_t msg_cap, const Mm2ItemsFile *items = nullptr);

/** Legacy one-shot: prepare + auto-distribute (no Identify modal). Prefer
 *  eventVmSearchPrepare when a UI can host 0x1B3F2. */
bool eventVmSearchPayoff(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                         char *msg, size_t msg_cap);

/** Open / Find Traps thievery leaf @ 0x1AEC2 / 0x1AF6E.
 *  `party_slot` 0-based; `find_traps` selects Find path (always opens after roll).
 *  Trap spring @ 0x1AA70: trap_type = rng(1,100)%4; place frames type*2+4 / +1 / 0
 *  (host animates); HP via 0x1A8A4 → 0x4952 after the cel loop. */
struct SearchOpenResult {
    bool opened = false;     /* distribute should run */
    bool trapped = false;    /* thievery failed and trap sprung */
    bool aborted = false;    /* ESC on member pick */
    uint8_t trap_type = 0;   /* 0..3 from trap_victim_pick @ 0x1A9A6 */
    uint8_t trap_place_frame = 0; /* type*2+4 — first sign_sprite_place cel */
    uint16_t trap_damage = 0; /* 0x1A8A4 pre-resist amount (table << attrib+0x14) */
    char trap_line0[48]{};    /* -$69B4[env][type*2] @ rows 0x13 */
    char trap_line1[48]{};    /* -$69B4[env][type*2+1] @ row 0x14 */
};

SearchOpenResult eventVmSearchOpenOrFind(uint8_t *a4, Mm2RosterFile *roster,
                                         const Mm2PartyLaunch *launch, int party_slot,
                                         uint8_t rating, bool find_traps, gameplay::Rng *rng,
                                         const Mm2ItemsFile *items = nullptr);

/** 0x1AA8E: first place() cel for trap_type 0..3. */
uint8_t eventVmSearchTrapPlaceFrame(uint8_t trap_type);

/** 0x1A8A4 pre-resist HP: -$690C[env_row] doubled attrib+0x14 times. */
uint16_t eventVmSearchTrapDamageAmount(const uint8_t *a4);

/** trap_damage_apply @ 0x1A90E after the 0x1AA8E cel loop. */
void eventVmSearchApplyTrapDamage(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                  int opener_slot, uint8_t trap_type, gameplay::Rng *rng);

/** Detect Magic @ 0x1AFE8 — "Contents magical (Yes|No), has trap (Yes|No)". */
void eventVmSearchDetectMagic(uint8_t *a4, uint8_t rating, char *msg, size_t msg_cap);

/** Leave @ 0x1B45C: keep loot, set -$7950 := 7. */
void eventVmSearchLeave(uint8_t *a4);

bool eventVmCheckOp30Password(const uint8_t *input_buf, const uint8_t *expected,
                              size_t expected_len);

/** Deduct gold from party members (front-loaded) until amount satisfied. Returns amount left. */
uint32_t eventVmDeductPartyGold(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                uint32_t amount);

/** OP_2A treasure block: u24 gold+exp, u16 gems, 3× item triplets. */
void eventVmApplyTreasure(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          const uint8_t block[14]);

/** OP_0D → play_sound_seq @ 0x6FB8 (ids 0..9). Index 0x09 also sets exit bit0. */
void eventVmExecEngineCall(uint8_t *a4, uint8_t index, world::MapWorld *world);

/** OP_24 @ 0x16B54 → -$7E6C → 0x6ACE: if party gold (sum +$66 over slots with
 *  roster index < 0x18) >= amount, deduct amount, pool the remainder, then
 *  re-share it equally among all eligible members ($7BBE; remainder to first).
 *  Returns true on success. */
bool eventVmPartyTryPayGold(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                            uint32_t amount);

/** OP_25 @ 0x16B82 → -$7E66 → 0x6B9A: same pool/deduct + re-share ($7CB0) for
 *  gems (+$5C, u16). */
bool eventVmPartyTryPayGems(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                            uint16_t amount);

/** Bribe food pay @ 0x6C66 (thunk -$7E60): pool/deduct + re-share ($7D3E) party
 *  food (+$25, u8). */
bool eventVmPartyTryPayFood(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                            uint8_t amount);

/* Arena Games (OP_0E 0x08). Thunk -$7DBE → 0x09D76 (mm2_data_00.bin
 * 0x7FFE+disp @ file 0x0240: `4EF9 00009D76`). -$7DFA is the OP_0E
 * default-range loader (0x092F2), not this; 0x07 store is -$7DB8 → 0xA62C.
 *
 * Only scripts that encode OP_0E 0x08 reach here (e.g. Middlegate arena tile):
 *   1) backpack-only scan for ticket 0xD0..0xD3 (0x9D9C-0x9DDA);
 *   2) miss → str 0xA082/0xA0A7, no combat;
 *   3) hit → consume (-$7F26), str 0xA0BF/0xA0E5, seed A4-$11DE:
 *      type = ((color*3 + area[screen])*16) + rng(1,16) (0x9E86-0x9EC2, 0xE74);
 *   4) victory gold: 4×3 table @ 0xE7A; ROM also corrupts record+0x79 (doc 36).
 *      Reward write is still a gap (no victory callback). */

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
 *  eventVmTrainingTownIndex() (map→index [1,5,2,4,2]). OP_0E training hall
 *  level-up (townSvcTrainLevelUp). Stat shrine 0x1C898 is a separate leaf.
 *  0x9B48/0x9BCA are bash-door (doc 43) — not training HP. */
uint32_t eventVmTrainingCostPerChar(int level, int town_index);

/** Per-character base HEALING cost (FAQ §3-6, doc 34 §13.2) for a LIVING
 *  character: `current_level × training_town_index × 10` gp (same town index as
 *  training). Dead (×10) / eradicated (×100) multipliers are applied in
 *  townSvcHeal (OP_0E temple → 0x1D208), not here: roster $26 only groups $80+
 *  as Dead/Stone/Eradicated (doc 06); dead-vs-eradicated is not ASM-confirmed. */
uint32_t eventVmHealingCostPerChar(int level, int town_index);

/** str.dat tip/rumor bank (0x9666 / A4-$7DE8):
 *  Bank word table at A4-$71E8; raw file at A4-$ED6 ($1E80 bytes).
 *  Decode: (byte+0x1C), then 0x1D→NUL into A4-$ED2; 0x976E walks C-strings. */
constexpr int kStrDatSize = 0x1E80;
constexpr int kStrBankSpan = 0x924;
/** DATA hunk A4-$71E8 (mm2_data_00.bin @ 0xE16, BE words): bank0..3 starts,
 *  bank3 end sentinel, then str.dat size. 0x9666 copies $924 from start. */
constexpr uint16_t kStrBankOffs[] = {0x0000, 0x063C, 0x0F5C, 0x1286, 0x1844, 0x1E80};
constexpr int kStrBankCount = 4;
constexpr int kStrBankTableWords = 6;

/** Seed A4-$71E8 bank offsets (call once after loading str.dat into -$ED6). */
void eventVmInitStrBankOffsets(uint8_t *a4);

/** 0x9666: decode bank `bank_index` (0..3) into A4-$ED2; clr -$71EA. */
void eventVmDecodeStrBank(uint8_t *a4, int bank_index, const uint8_t *str_dat, size_t str_len);

/** 0x976E: next C-string in decode buf; advances -$71EA. Returns nullptr at end. */
const char *eventVmNextStrBankCString(uint8_t *a4);

/** 0x1493C GS: decode bank 3, fill OP_0E FD ptr tables via 0x976E, set -$71DC=$FD,
 *  clr -$11DE[0..10]. Ptr slots store A4-relative int32 (Amiga stores abs addrs). */
void eventVmFillOp0eFdStrTables(uint8_t *a4, const uint8_t *str_dat, size_t str_len);

/** 0x1D208 GS: decode bank 1, fill tavern rumor/tip/food ptr tables, set -$71DC=$FD. */
void eventVmFillTavernStrTables(uint8_t *a4, const uint8_t *str_dat, size_t str_len);

/** 0x18204 GS: decode bank 0, fill A4-$5C42 with 22×4 joke line ptrs via 0x976E. */
void eventVmFillJokeStrTables(uint8_t *a4, const uint8_t *str_dat, size_t str_len);

/** 0x18254: joke index = day[era] % 22 (divs.w #$16; swap remainder). */
int eventVmJokeIndex(uint16_t day);

/** Join the 4 C-strings for `joke_index` (0..21) from A4-$5C42. */
int eventVmFormatJoke(const uint8_t *a4, int joke_index, char *out, size_t out_cap);

/** Resolve A4-relative C-string ptr stored by fill helpers. */
const char *eventVmGsRelCString(const uint8_t *a4, uint32_t rel_u32);

/** Join `count` C-strings from an OP_0E FD ptr table (A4-relative longs) into
 *  `out`, separated by newlines. Empty slots become blank lines (ASM print loop).
 *  Used by 0x14A58 (PTR0×4), 0x14F98 (PTR1×4), 0x15142 (PTR2×14 + PTR3×4),
 *  0x1531E (PTR4×11 + PTR5×10). */
int eventVmFormatOp0eFdPtrTable(const uint8_t *a4, int32_t table_base, int count, char *out,
                                size_t out_cap);

/** 0x14106 Death Strikes panel lines (CODE @$1405A..$140F4 via A4-$6D60). */
void eventVmDeathStrikesLines(char *out, size_t out_cap);

/** 0x1B0B6: env A4-$79E3 → sign_sprite_load id ($46..$4A → NN.anm). */
int eventVmSearchContainerAnmId(const uint8_t *a4);

/** Pending -$7FBC sign_sprite_place from bit7 choreography @ 0x9888. */
struct ScriptedKeyPlace {
    bool active = false;
    bool clear = false; /* place(-1) → drop handle */
    uint8_t placement = 0;
    uint16_t dst_x = 0x40; /* arg2 (-$71DA) */
    uint16_t dst_y = 0x28; /* arg3 (-$71D8) + 8 @ 0x23E24 */
};

/** -$7DDC @ 0x97A2 host: pop next plain ASCII from -$119A, or -1 for real input.
 *  Honors -$71DB delay (0x993e) and bit7 → 0x9888 place stream (-$7FBC).
 *  Optional `place` receives one sprite place per poll when choreography fires. */
int eventVmScriptedKeyPoll(uint8_t *a4, ScriptedKeyPlace *place = nullptr);
void eventVmScriptedKeyReset(uint8_t *a4);
/** Write up to 255 bytes into -$119A, append $FF, reset cursors. */
void eventVmScriptedKeyQueue(uint8_t *a4, const uint8_t *bytes, int len);

}  // namespace mm2::events
