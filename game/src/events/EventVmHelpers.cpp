#include "mm2/events/EventVmHelpers.h"

#include "mm2/CppStdCompat.h"
#include "mm2/events/EventFieldMap.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2_found_items.h"
#include "mm2_map_codec.h"

#include <cstdio>

namespace mm2::events {

namespace {

constexpr int kInputBufSize = 16;

int partyRosterIndex(const uint8_t *a4, int party_slot)
{
    if (!a4 || party_slot < 0 || party_slot >= MM2_PARTY_LAUNCH_SLOTS) {
        return -1;
    }
    const uint16_t idx = mm2_gs_u16(a4, MM2_GS_ROSTER_INDEX_TBL + party_slot * 2);
    if (idx == 0xFFFFu || idx >= MM2_ROSTER_RECORD_COUNT) {
        return -1;
    }
    return static_cast<int>(idx);
}

int partyActiveCount(const uint8_t *a4, const Mm2PartyLaunch *launch)
{
    if (launch) {
        return launch->party_count < MM2_PARTY_LAUNCH_SLOTS ? launch->party_count
                                                           : MM2_PARTY_LAUNCH_SLOTS;
    }
    if (!a4) {
        return 0;
    }
    const int n = static_cast<int>(mm2_gs_u16(a4, MM2_GS_PARTY_COUNT));
    return n < MM2_PARTY_LAUNCH_SLOTS ? n : MM2_PARTY_LAUNCH_SLOTS;
}

/** 0x6ACE / 0x6B9A eligibility: party-table word A4-$796A[i] < 0x18. */
bool partySlotEligible(const uint8_t *a4, const Mm2PartyLaunch *launch, int party_slot)
{
    if (party_slot < 0 || party_slot >= MM2_PARTY_LAUNCH_SLOTS) {
        return false;
    }
    if (launch) {
        const int idx = launch->roster_slots[party_slot];
        return idx >= 0 && idx < 0x18;
    }
    if (!a4) {
        return false;
    }
    const uint16_t idx = mm2_gs_u16(a4, MM2_GS_ROSTER_INDEX_TBL + party_slot * 2);
    return idx < 0x18u;
}

const Mm2RosterRecord *rosterRecord(const Mm2RosterFile *roster, int roster_idx)
{
    if (!roster || roster_idx < 0 || roster_idx >= MM2_ROSTER_RECORD_COUNT) {
        return nullptr;
    }
    if (mm2_roster_slot_is_empty(&roster->records[roster_idx])) {
        return nullptr;
    }
    return &roster->records[roster_idx];
}

Mm2RosterRecord *rosterRecordMut(Mm2RosterFile *roster, int roster_idx)
{
    if (!roster || roster_idx < 0 || roster_idx >= MM2_ROSTER_RECORD_COUNT) {
        return nullptr;
    }
    if (mm2_roster_slot_is_empty(&roster->records[roster_idx])) {
        return nullptr;
    }
    return &roster->records[roster_idx];
}

/* Item storage is Structure-of-Arrays (Mm2RosterRecord: equipped_id[6]/
 * equipped_charges[6]/equipped_flags[6], then backpack_id/charges/flags) — the
 * SoA layout confirmed by OP_16 @ 0x16520 and OP_19 @ 0x165D8. */
bool itemInRecord(const Mm2RosterRecord &rec, uint8_t item_id)
{
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.equipped_id[i] == item_id || rec.backpack_id[i] == item_id) {
            return true;
        }
    }
    return false;
}

bool consumeItemInRecord(Mm2RosterRecord &rec, uint8_t item_id)
{
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.equipped_id[i] == item_id) {
            rec.equipped_id[i] = 0;
            rec.equipped_charges[i] = 0;
            rec.equipped_flags[i] = 0;
            return true;
        }
    }
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.backpack_id[i] == item_id) {
            rec.backpack_id[i] = 0;
            rec.backpack_charges[i] = 0;
            rec.backpack_flags[i] = 0;
            return true;
        }
    }
    return false;
}

int tileIndex(int y, int x)
{
    if (y < 0 || y >= MM2_MAP_GRID_DIM || x < 0 || x >= MM2_MAP_GRID_DIM) {
        return -1;
    }
    return y * MM2_MAP_GRID_DIM + x;
}

}  // namespace

bool eventVmIsTownServiceSelector(uint8_t sel)
{
    switch (sel) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
        return true;
    default:
        break;
    }
    if (sel >= 0x09 && sel <= 0x10) {
        return true;
    }
    if (sel >= 0x11 && sel <= 0x37) {
        return true;
    }
    if (sel >= 0x38 && sel <= 0x4B) {
        return true;
    }
    if (sel >= 0x4C && sel <= 0x54) {
        return true;
    }
    if (sel >= 0x56 && sel <= 0x5B) {
        return true;
    }
    if (sel >= 0x5C && sel <= 0x5E) {
        return true;
    }
    if (sel >= 0x65 && sel <= 0x69) {
        return true;
    }
    if (sel >= 0x6A && sel <= 0x7C) {
        return true;
    }
    if (sel >= 0x97 && sel <= 0x98) {
        return true;
    }
    if (sel >= 0xE3 && sel <= 0xF3) {
        return true;
    }
    if (sel >= 0xF4 && sel <= 0xFB) {
        return true;
    }
    return false;
}

/* ROM opcode_len_tbl @ A4-$6CC8 (data hunk file offset 0x1336), read verbatim
 * via tools/dump_event_token_table.py. Differs from naive "1 + argc" at op
 * 0x00 (0, not 1) and op 0x25 (2, not 3) -- see header doc comment. */
constexpr uint8_t kOpTokenDelta[0x33] = {
    0,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  3,  3,  2,  2,  1,  2,  2,  13, 11, 1,  4,  3,  3,  5,  5,  3,
    2,  2,  2,  2,  7,  7,  4,  3,  3,  3,  2,  1,  1,  3,  1,  15, 2,  2,  3,  3,  1,  11, 4,  2,
};

uint8_t eventVmTokenDelta(uint8_t op)
{
    if (op < sizeof(kOpTokenDelta)) {
        return kOpTokenDelta[op];
    }
    return 1;
}

Mm2ExecSelectorBin eventVmBinExecSelector(uint8_t sel)
{
    struct Range {
        uint8_t lo;
        uint8_t hi;
        uint8_t category;
        uint8_t subtract;
    };
    static const Range kBins[] = {
        {0x09, 0x10, 0x3C, 0x08}, {0x11, 0x37, 0x3D, 0x10}, {0x38, 0x4B, 0x3E, 0x37},
        {0x4C, 0x54, 0x3F, 0x4B}, {0x56, 0x5B, 0x40, 0x55}, {0x5C, 0x5E, 0x41, 0x5B},
        {0x65, 0x69, 0x42, 0x64}, {0x6A, 0x7C, 0x43, 0x69}, {0x97, 0x98, 0x44, 0x96},
        {0xE3, 0xF3, 0x45, 0xE2}, {0xF4, 0xFB, 0x46, 0xF3},
    };
    Mm2ExecSelectorBin out;
    for (const Range &r : kBins) {
        if (sel >= r.lo && sel <= r.hi) {
            out.category = r.category;
            out.index = static_cast<uint8_t>(sel - r.subtract);
            out.matched = true;
            break;
        }
    }
    return out;
}

bool eventVmLocationStringRaw(const Mm2EventLocation *loc, int idx, const uint8_t **out,
                              size_t *out_len)
{
    if (!loc || !out || !out_len || idx < 0 || !loc->raw) {
        return false;
    }
    if (loc->string_table_offset < 0 ||
        static_cast<size_t>(loc->string_table_offset) >= loc->raw_len) {
        return false;
    }

    size_t pos = static_cast<size_t>(loc->string_table_offset);
    int cur = 0;
    while (pos < loc->raw_len) {
        const size_t start = pos;
        while (pos < loc->raw_len && loc->raw[pos] != 0xFF) {
            ++pos;
        }
        if (cur == idx) {
            *out = loc->raw + start;
            *out_len = pos - start;
            return true;
        }
        ++cur;
        ++pos;
    }
    return false;
}

bool eventVmStringLooksLikeBytecode(const uint8_t *bytes, size_t len)
{
    if (!bytes || len == 0) {
        return false;
    }
    const uint8_t op = bytes[0];
    if (op == 0 || op >= 0x33) {
        return false;
    }
    if (op == 0x12) {
        return len >= 12;
    }
    if (op == 0x13) {
        return len >= 10;
    }
    if (len >= 2 && bytes[1] < 0x20) {
        return true;
    }
    return false;
}

int32_t eventVmResolveVarOffset(uint8_t group, uint8_t index)
{
    /* event_op_var_resolve @ 0x15620 keys on a single id byte; `index` is unused
     * (OP_17 reads+discards a 2nd byte). ids 0x00..0x17 index the flag bank. */
    (void)index;
    if (group <= 0x17) {
        return MM2_GS_EVENT_VAR_BANK + group;
    }
    switch (group) {
    case 0x23:
        return MM2_GS_LEVITATE_FLAG;
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
        return MM2_GS_TALISMAN_BASE + (group - 0x27);
    case 0x2B:
        return MM2_GS_CLASS_QUEST_CNT;
    case 0x2C:
        return MM2_GS_QUEST_COUNTER_B;
    case 0x32:
        return MM2_GS_GUARDIAN_CAVE;
    case 0x33:
        return MM2_GS_TUNDRA_LEVER;
    case 0x3B:
        return MM2_GS_XABRAN_GATE;
    case 0x3C:
        return MM2_GS_DAWN_GATE;
    case 0x3D:
        return MM2_GS_PERIOD_FLAG_B;
    case 0x3E:
        return MM2_GS_PERIOD_FLAG_A;
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
        return MM2_GS_GATE_BANK_B + (group - 0x80);
    case 0x84:
        return MM2_GS_ERA_LOW;
    default:
        return 0;
    }
}

uint8_t eventVmLoadVar(const uint8_t *a4, uint8_t group, uint8_t index)
{
    const int32_t off = eventVmResolveVarOffset(group, index);
    if (!a4 || off == 0) {
        return 0;
    }
    return mm2_gs_u8(a4, off);
}

void eventVmStoreVar(uint8_t *a4, uint8_t group, uint8_t index, uint8_t val)
{
    const int32_t off = eventVmResolveVarOffset(group, index);
    if (!a4 || off == 0) {
        return;
    }
    mm2_gs_set_u8(a4, off, val);
}

uint8_t eventVmSpellEyeTimer(const uint8_t *a4, bool outdoor_view)
{
    if (!a4) {
        return 0;
    }
    const int32_t off = outdoor_view ? MM2_GS_CLASS_QUEST_CNT : MM2_GS_QUEST_COUNTER_B;
    return mm2_gs_u8(a4, off);
}

void eventVmTickSpellEyeOnStep(uint8_t *a4, bool outdoor_view)
{
    /* spell_eye_step_tick @ 0x4672: pick Eagle (-$79A0) when -$79E2 != 0, else
     * Wizard (-$799F); subq while nonzero. Protect-panel vars are OR'd into a
     * refresh gate but the timer decrement is unconditional when active. */
    if (!a4) {
        return;
    }
    const int32_t off = outdoor_view ? MM2_GS_CLASS_QUEST_CNT : MM2_GS_QUEST_COUNTER_B;
    uint8_t timer = mm2_gs_u8(a4, off);
    if (timer == 0) {
        return;
    }
    mm2_gs_set_u8(a4, off, static_cast<uint8_t>(timer - 1));
}

uint32_t eventVmPartyGoldTotal(const uint8_t *a4, const Mm2RosterFile *roster,
                               const Mm2PartyLaunch *launch)
{
    uint32_t total = 0;
    if (roster && launch) {
        for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            const int idx = launch->roster_slots[i];
            const Mm2RosterRecord *rec = rosterRecord(roster, idx);
            if (rec) {
                total += rec->gold;
            }
        }
        return total;
    }
    if (!a4) {
        return 0;
    }
    for (int i = 0; i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = partyRosterIndex(a4, i);
        if (idx < 0) {
            continue;
        }
        const int off = MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x66;
        total += mm2_gs_u32(a4, off);
    }
    return total;
}

int eventVmCountPartyNibbleMatches(const uint8_t *a4, const Mm2RosterFile *roster,
                                   const Mm2PartyLaunch *launch, uint8_t id)
{
    /* event_op32 @ 0x17190 -> thunk -$7F2C -> 0x04614 (per A4 thunk map):
     * sum, over LIVING party members (condition byte @ record+0x26 < 0x81), the
     * number of nibbles of record+0x50 equal to `id` (helper 0x45C4 tests both
     * the low and high nibble, returning 0/1/2). The result is the raw count.
     * record+0x50 is a packed pair of alignment/profession-title nibbles (ASM
     * annotates it "class nibble"): the script gates it against titles, e.g.
     * id 0x04 = Crusader (Hillstone "...only bestow a quest unto a Crusader"),
     * 0x08 = Heroic (Murray's Cave "You are not heroic!"), 0x09 = druid/pagan
     * (C3 Oak Grove). It is NOT the roster `spells[]` bitfield the struct label
     * suggests (MM2 gates spells by spell-level, not per-spell flags). */
    int count = 0;
    if (roster && launch) {
        for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            const Mm2RosterRecord *rec = rosterRecord(roster, launch->roster_slots[i]);
            if (!rec || rec->condition >= 0x81) {
                continue;
            }
            const uint8_t packed = reinterpret_cast<const uint8_t *>(rec)[0x50];
            if ((packed & 0x0F) == id) {
                ++count;
            }
            if (((packed >> 4) & 0x0F) == id) {
                ++count;
            }
        }
        return count;
    }
    if (!a4) {
        return 0;
    }
    for (int i = 0; i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = partyRosterIndex(a4, i);
        if (idx < 0) {
            continue;
        }
        const int base = MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE;
        if (mm2_gs_u8(a4, base + 0x26) >= 0x81) {
            continue;
        }
        const uint8_t packed = mm2_gs_u8(a4, base + 0x50);
        if ((packed & 0x0F) == id) {
            ++count;
        }
        if (((packed >> 4) & 0x0F) == id) {
            ++count;
        }
    }
    return count;
}

int eventVmCountLivingPartyMembers(const uint8_t *a4, const Mm2RosterFile *roster,
                                   const Mm2PartyLaunch *launch)
{
    /* roster_count_living_chars @ 0x47A2: status byte +$26 & $E0 == 0. */
    int count = 0;
    if (roster && launch) {
        for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            const Mm2RosterRecord *rec = rosterRecord(roster, launch->roster_slots[i]);
            if (!rec) {
                continue;
            }
            if ((rec->condition & 0xE0) == 0) {
                ++count;
            }
        }
        return count;
    }
    if (!a4) {
        return 0;
    }
    const int n = partyActiveCount(a4, nullptr);
    for (int i = 0; i < n; ++i) {
        const int idx = partyRosterIndex(a4, i);
        if (idx < 0) {
            continue;
        }
        const int base = MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE;
        if ((mm2_gs_u8(a4, base + 0x26) & 0xE0) == 0) {
            ++count;
        }
    }
    return count;
}

bool eventVmPartyGiveItem(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          uint8_t item_id, uint8_t charges, uint8_t flags)
{
    if (!item_id) {
        return false;
    }
    bool placed = false;
    if (roster && launch) {
        for (int i = 0; !placed && i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
            if (!rec) {
                continue;
            }
            auto *raw = reinterpret_cast<uint8_t *>(rec);
            for (int m = 0; m < 6; ++m) {
                if (raw[0x3A + m] == 0) {
                    raw[0x3A + m] = item_id;
                    raw[0x40 + m] = charges;
                    raw[0x46 + m] = flags;
                    placed = true;
                    break;
                }
            }
        }
    }
    if (!placed && a4) {
        mm2_found_items_overflow_append(a4, item_id, charges, flags);
    }
    return placed;
}

bool eventVmPartyHasItem(const uint8_t *a4, const Mm2RosterFile *roster,
                         const Mm2PartyLaunch *launch, uint8_t item_id, bool consume)
{
    if (item_id == 0) {
        return false;
    }
    if (roster && launch) {
        for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            const int idx = launch->roster_slots[i];
            Mm2RosterRecord *rec = rosterRecordMut(const_cast<Mm2RosterFile *>(roster), idx);
            if (!rec) {
                continue;
            }
            if (consume) {
                if (consumeItemInRecord(*rec, item_id)) {
                    return true;
                }
            } else if (itemInRecord(*rec, item_id)) {
                return true;
            }
        }
        return false;
    }
    (void)a4;
    return false;
}

bool eventVmPartyConsumeBackpackItem(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                     uint8_t item_id)
{
    /* OP_28 @ 0x16C86: backpack id run record+$3A only; always consume. */
    if (!roster || !launch || item_id == 0) {
        return false;
    }
    for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
        if (!rec) {
            continue;
        }
        for (int m = 0; m < MM2_ROSTER_ITEM_SLOTS; ++m) {
            if (rec->backpack_id[m] == item_id) {
                rec->backpack_id[m] = 0;
                rec->backpack_charges[m] = 0;
                rec->backpack_flags[m] = 0;
                return true;
            }
        }
    }
    return false;
}

void eventVmClearTileEventFlag(uint8_t *a4, int y, int x)
{
    /* OP_14 @ 0x16398: andi #$7F on collision page -$54BA[(y<<4)|x] AND on
     * the current-cell latch -$55D6 (single byte — not an indexed array). */
    const int idx = tileIndex(y, x);
    if (!a4 || idx < 0) {
        return;
    }
    mm2_gs_set_u8(a4, MM2_GS_TILE_VISITED + idx,
                  static_cast<uint8_t>(mm2_gs_u8(a4, MM2_GS_TILE_VISITED + idx) & static_cast<uint8_t>(~0x80)));
    mm2_gs_set_u8(a4, MM2_GS_TILE_RT_FLAGS,
                  static_cast<uint8_t>(mm2_gs_u8(a4, MM2_GS_TILE_RT_FLAGS) & static_cast<uint8_t>(~0x80)));
}

void eventVmConsumeTileEncounterFlag(uint8_t *a4, world::MapWorld &world, int y, int x)
{
    eventVmClearTileEventFlag(a4, y, x);
    if (!world.loaded() || y < 0 || y >= MM2_MAP_GRID_DIM || x < 0 || x >= MM2_MAP_GRID_DIM) {
        return;
    }
    Mm2MapScreen &screen = world.mapFileMut().screens[world.currentScreen()];
    const int idx = y * MM2_MAP_GRID_DIM + x;
    screen.collision[idx] =
        static_cast<uint8_t>(screen.collision[idx] & static_cast<uint8_t>(~MM2_MAP_COLL_EVENT));
}

void eventVmPatchMapTile(world::MapWorld &world, int y, int x, uint8_t visual, uint8_t collision)
{
    if (!world.loaded() || y < 0 || y >= MM2_MAP_GRID_DIM || x < 0 || x >= MM2_MAP_GRID_DIM) {
        return;
    }
    Mm2MapScreen &screen = world.mapFileMut().screens[world.currentScreen()];
    screen.visual[y * MM2_MAP_GRID_DIM + x] = visual;
    screen.collision[y * MM2_MAP_GRID_DIM + x] = collision;
}

int eventVmSelectedPartySlot(const uint8_t *a4);

uint8_t eventVmApplyPartyByteOp(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                uint8_t member_spec, uint8_t selector, uint8_t val, bool masked,
                                uint8_t and_m, uint8_t or_m)
{
    /* event_op15_party_state_apply @ 0x16426 (OP_18 = same with masked flag). */
    if (!a4) {
        return 0;
    }

    int field_off = -1;
    int field_w = 1;
    if (!eventVmResolveMemberField(selector, &field_off, &field_w)) {
        return 0;
    }
    (void)field_w;

    /* 0x16430: snapshot incoming cond → -$5D3F; clear -$5D41; clear cond. */
    const uint8_t incoming_cond = mm2_gs_u8(a4, MM2_GS_COND_FLAG);
    mm2_gs_set_u8(a4, MM2_GS_SAVED_COND_FLAG, incoming_cond);
    mm2_gs_set_u8(a4, MM2_GS_COND_FLAG, 0);

    uint8_t effective_or = or_m;
    uint8_t spec = member_spec;
    /* 0x1646A: bit7 → or_mask from saved cond, then strip. */
    if (spec >= 0x80) {
        effective_or = incoming_cond;
        spec = static_cast<uint8_t>(spec & 0x7F);
    }

    const int party_count = partyActiveCount(a4, launch);
    const uint8_t party_n =
        static_cast<uint8_t>(party_count > 0 ? (party_count > 8 ? 8 : party_count) : 0);

    /* Resolve which 1-based members to visit (into slots[], count). */
    uint8_t slots[8];
    int nslots = 0;
    if (spec == 0) {
        /* All party: ASM walks N, N-1, …, 1 (0x164B8). */
        for (int m = party_n; m >= 1; --m) {
            slots[nslots++] = static_cast<uint8_t>(m);
        }
    } else if (spec == 9) {
        /* 0x163CA: -$5D42, else -$5D3F; writeback to -$5D42. */
        uint8_t sel = mm2_gs_u8(a4, MM2_GS_SELECTED_MEMBER);
        if (sel == 0) {
            sel = mm2_gs_u8(a4, MM2_GS_SAVED_COND_FLAG);
            mm2_gs_set_u8(a4, MM2_GS_SELECTED_MEMBER, sel);
        }
        if (sel > party_n) {
            sel = 1;
            mm2_gs_set_u8(a4, MM2_GS_SAVED_COND_FLAG, 1);
        }
        if (sel >= 1 && sel <= party_n) {
            slots[nslots++] = sel;
        }
    } else {
        uint8_t one = spec;
        if (one > party_n) {
            one = 1;
        }
        if (one >= 1 && one <= party_n) {
            slots[nslots++] = one;
        }
    }

    uint8_t cond = 0;
    for (int si = 0; si < nslots; ++si) {
        const int party_idx = static_cast<int>(slots[si]) - 1; /* 0-based */
        int roster_idx = -1;
        if (launch && party_idx < launch->party_count) {
            roster_idx = launch->roster_slots[party_idx];
        } else {
            roster_idx = partyRosterIndex(a4, party_idx);
        }
        if (roster_idx < 0) {
            continue;
        }

        uint8_t byte_val = 0;
        if (roster) {
            const uint8_t *raw = reinterpret_cast<const uint8_t *>(&roster->records[roster_idx]);
            if (field_off < MM2_ROSTER_RECORD_SIZE) {
                byte_val = raw[field_off];
            }
        } else {
            const int off = MM2_GS_ROSTER_BASE + roster_idx * MM2_GS_ROSTER_STRIDE + field_off;
            byte_val = mm2_gs_u8(a4, off);
        }

        if (masked) {
            /* 0x164E6: (field & and) | or */
            byte_val = static_cast<uint8_t>((byte_val & and_m) | effective_or);
            if (roster) {
                uint8_t *raw = reinterpret_cast<uint8_t *>(&roster->records[roster_idx]);
                if (field_off < MM2_ROSTER_RECORD_SIZE) {
                    raw[field_off] = byte_val;
                }
            } else {
                const int off = MM2_GS_ROSTER_BASE + roster_idx * MM2_GS_ROSTER_STRIDE + field_off;
                mm2_gs_set_u8(a4, off, byte_val);
            }
        } else {
            /* 0x16500: if val!=0 then field&=val; then cond |= field */
            uint8_t piece = byte_val;
            if (val != 0) {
                piece = static_cast<uint8_t>(piece & val);
            }
            cond = static_cast<uint8_t>(cond | piece);
        }
    }

    if (!masked) {
        mm2_gs_set_u8(a4, MM2_GS_COND_FLAG, cond);
    }
    return masked ? 0 : cond;
}

bool eventVmCheckOp30Password(const uint8_t *input_buf, const uint8_t *expected,
                              size_t expected_len)
{
    /* event_op30_answer_check @ 0x17034: for i in 0..9 compare
     * toupper(input[i]) vs (0x11A - expected[i]); cond=1 iff all 10 match.
     * Trailing expected bytes are typically 0xFA → space. */
    if (!input_buf || !expected || expected_len < 10) {
        return false;
    }
    for (size_t i = 0; i < 10; ++i) {
        const uint8_t got =
            static_cast<uint8_t>(std::toupper(static_cast<unsigned char>(input_buf[i])));
        const uint8_t want = static_cast<uint8_t>((0x11A - expected[i]) & 0xFF);
        if (got != want) {
            return false;
        }
    }
    return true;
}

uint32_t eventVmDeductPartyGold(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                uint32_t amount)
{
    if (amount == 0) {
        return 0;
    }
    uint32_t left = amount;
    if (roster && launch) {
        for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS && left > 0; ++i) {
            Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
            if (!rec) {
                continue;
            }
            if (rec->gold >= left) {
                rec->gold -= left;
                left = 0;
            } else {
                left -= rec->gold;
                rec->gold = 0;
            }
        }
        return left;
    }
    if (!a4) {
        return left;
    }
    for (int i = 0; i < MM2_PARTY_LAUNCH_SLOTS && left > 0; ++i) {
        const int idx = partyRosterIndex(a4, i);
        if (idx < 0) {
            continue;
        }
        const int off = MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x66;
        uint32_t gold = mm2_gs_u32(a4, off);
        if (gold >= left) {
            mm2_gs_set_u32(a4, off, gold - left);
            left = 0;
        } else {
            left -= gold;
            mm2_gs_set_u32(a4, off, 0);
        }
    }
    return left;
}

void eventVmApplyTreasure(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          const uint8_t block[14])
{
    /* event_op2a_set_reward_block @ 0x16D16 — fills found-item buffer only.
     * Distribution is Search @ 0x4800 → 0x1B19C (eventVmSearchPayoff). */
    (void)roster;
    (void)launch;
    if (!a4 || !block) {
        return;
    }
    mm2_found_items_op2a_fill(a4, block);
}

void eventVmApplyOp31Damage(Mm2RosterRecord *rec, uint16_t damage)
{
    /* 0x4952 with out-flags=0 (OP_31 call site @ 0x1714E peals three zeros):
     * skip if +$26 >= 0x80; else subtract from +$5E (working HP / hp_max word).
     * Lethal: bset bit6; if bit6 already set → +$26=0x81; clear +$5E. */
    if (!rec || damage == 0) {
        return;
    }
    auto *raw = reinterpret_cast<uint8_t *>(rec);
    if (raw[0x26] >= 0x80) {
        return;
    }
    raw[0x26] = static_cast<uint8_t>(raw[0x26] & 0xEF); /* andi #$EF @ 0x4AA0 */
    const uint16_t hp = rec->hp_max;
    uint8_t lethal = 0;
    if (raw[0x26] & 0x40) {
        /* Already unconscious bit6 → force dead 0x81 @ 0x4AB2. */
        raw[0x26] = 0x81;
        lethal = 1;
    } else if (damage >= hp) {
        raw[0x26] = static_cast<uint8_t>(raw[0x26] | 0x40);
        lethal = 1;
    } else {
        rec->hp_max = static_cast<uint16_t>(hp - damage);
    }
    if (lethal) {
        rec->hp_max = 0;
    }
}

void eventVmOp31IterateDamage(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                              uint8_t member_spec, uint16_t damage)
{
    /* Member resolution mirrors 0x170FC..0x17142 (simplified for out-flags=0). */
    if (!roster || !launch) {
        return;
    }
    const int party_n = partyActiveCount(a4, launch);
    if (party_n <= 0) {
        return;
    }

    uint8_t spec = member_spec;
    uint16_t value = damage;
    if (a4 && spec >= 0x80) {
        spec = static_cast<uint8_t>(spec & 0x7F);
        value = mm2_gs_u8(a4, MM2_GS_COND_FLAG);
    }

    uint8_t slots[8];
    int nslots = 0;
    if (spec == 0) {
        for (int m = 1; m <= party_n && m <= 8; ++m) {
            slots[nslots++] = static_cast<uint8_t>(m);
        }
    } else {
        /* 0x1710A: subq #1 then cmpi #8 → only original spec==9 is "selected"
         * (-$5D42 / else cond). Spec 1..8 are 1-based member indices (8 = slot 8). */
        uint8_t one = spec;
        if (one == 9) {
            one = a4 ? mm2_gs_u8(a4, MM2_GS_SELECTED_MEMBER) : 0;
            if (one == 0 && a4) {
                one = mm2_gs_u8(a4, MM2_GS_COND_FLAG);
            }
            if (one == 0) {
                one = 1;
            }
        }
        if (one >= 1 && one <= static_cast<uint8_t>(party_n)) {
            slots[nslots++] = one;
        }
    }

    for (int i = 0; i < nslots; ++i) {
        const int party_idx = slots[i] - 1;
        if (party_idx < 0 || party_idx >= launch->party_count) {
            continue;
        }
        Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[party_idx]);
        if (rec) {
            eventVmApplyOp31Damage(rec, value);
        }
    }
}

namespace {

/* -$6A54 container names: 5 env rows × 8 score ranks (0x1B112 / 0x1B37A). */
static const char *const kSearchContainerNames[5][8] = {
    {"Wooden Crate", "Tin Lockbox", "Steel Safe", "Copper Safe", "Bronze Safe", "Steel Safe",
     "Gold Safe", "Stasis Safe"},
    {"Hidden Cache", "Wicker Chest", "Rusty Trunk", "Copper Box", "Bronze Box", "Steel Box",
     "Gold Box", "Doomsday Box"},
    {"Rotting Box", "Rusty Chest", "Stone Chest", "Copper Chest", "Bronze Chest", "Steel Chest",
     "Gold Chest", "Statis Box"},
    {"Wooden Chest", "Rusty Chest", "Copper Chest", "Bronze Chest", "Silver Chest", "Gold Chest",
     "Platinum Box", "Doomsday Box"},
    {"Ceramic Case", "Lacquer Box", "Jewelled Box", "Copper Trunk", "Bronze Trunk", "Silver Trunk",
     "Gold Trunk", "Statis Box"},
};

int searchContainerEnvRow(const uint8_t *a4)
{
    /* 0x1B112: map A4-$79E3 → row 0..4. */
    const uint8_t env = a4 ? mm2_gs_u8(a4, MM2_GS_SIGN_ENV_ID) : 0;
    if (env == 0) {
        return 0;
    }
    if (env == 3) {
        return 1;
    }
    if (env == 1) {
        return 2;
    }
    if (env == 6 || env == 4) {
        return 3;
    }
    return 4;
}

void searchFillContainerName(const uint8_t *a4, uint8_t score, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    uint8_t idx = score > 0 ? static_cast<uint8_t>(score - 1) : 0;
    if (idx > 7) {
        idx = 7;
    }
    const int row = searchContainerEnvRow(a4);
    std::snprintf(out, out_cap, "%s", kSearchContainerNames[row][idx]);
}

uint8_t searchComputeRating(const Mm2FoundItems &peek)
{
    /* 0x1B1BC..0x1B262 */
    uint8_t score = 0;
    for (int i = 0; i < 3; ++i) {
        if (((peek.gold_exp >> (i * 8)) & 0xFFu) != 0) {
            ++score;
        }
    }
    if ((peek.gems & 0xFFu) != 0) {
        ++score;
    }
    if (((peek.gems >> 8) & 0xFFu) != 0) {
        ++score;
    }
    if (score == 0) {
        score = 1;
    }
    uint8_t max_flag = 0;
    for (int i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        const uint8_t f = static_cast<uint8_t>(peek.flags[i] & 0x3F);
        if (f > max_flag) {
            max_flag = f;
        }
    }
    if (max_flag > 1) {
        ++score;
    }
    score = static_cast<uint8_t>(score + (max_flag >> 2));
    if (score > 8) {
        score = 8;
    }
    return score;
}

uint8_t searchApplyRatingRng(uint8_t score, gameplay::Rng *rng)
{
    /* 0x1B268: if score < 4 → rng(1,100); if roll>=$1E → display=roll else 0.
     * Else display stays at computed score. 0x1B296 subq is on the score counter
     * used for container art, not the displayed rating. */
    if (score < 4) {
        const int roll = rng ? rng->range(1, 100) : 1;
        return (roll >= 0x1E) ? static_cast<uint8_t>(roll) : 0;
    }
    return score;
}

int searchThievery(const Mm2RosterRecord &rec)
{
    /* +$1E preferred; else +$16 thievery_percent (same as Unlock). */
    const int at_1e = rec.unknown_1a_20[4];
    return at_1e != 0 ? at_1e : static_cast<int>(rec.thievery_percent);
}

}  // namespace

bool eventVmSearchDistribute(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             char *msg, size_t msg_cap)
{
    /* 0x1AC94 distribute + 0x1B4D4 epilogue. */
    if (!a4) {
        return false;
    }

    /* 0x1B4A4–0x1B4BE: non-$FF sentinel folds into id[0]; always clr -$794C. */
    const uint8_t sent = mm2_gs_u8(a4, MM2_GS_FOUND_SENTINEL);
    if (sent != 0 && sent != MM2_FOUND_SENTINEL_FILLED) {
        mm2_gs_set_u8(a4, MM2_GS_FOUND_ITEM_ID, sent);
        mm2_gs_set_u8(a4, MM2_GS_FOUND_ITEM_CHARGES, 0);
        mm2_gs_set_u8(a4, MM2_GS_FOUND_ITEM_FLAGS, 0);
    }
    mm2_gs_set_u8(a4, MM2_GS_FOUND_SENTINEL, MM2_FOUND_SENTINEL_EMPTY);

    if (!roster || !launch || launch->party_count <= 0) {
        Mm2FoundItems empty{};
        empty.sentinel = MM2_FOUND_SENTINEL_EMPTY;
        mm2_found_items_write(a4, &empty);
        if (msg && msg_cap > 0) {
            std::snprintf(msg, msg_cap, "Each share = 0 Gold");
        }
        mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 3);
        return true;
    }

    const int party_n = launch->party_count < MM2_PARTY_LAUNCH_SLOTS ? launch->party_count
                                                                     : MM2_PARTY_LAUNCH_SLOTS;

    int gold_div = 0;
    for (int i = 0; i < party_n; ++i) {
        if (launch->roster_slots[i] < 0x18) {
            ++gold_div;
        }
    }
    if (gold_div <= 0) {
        gold_div = 1;
    }

    Mm2FoundItems loot{};
    mm2_found_items_read(a4, &loot);
    const uint32_t gold_each = loot.gold_exp / static_cast<uint32_t>(gold_div);
    const uint16_t gems_each =
        party_n > 0 ? static_cast<uint16_t>(loot.gems / static_cast<uint16_t>(party_n)) : 0;

    loot.gold_exp = 0;
    loot.gems = 0;
    loot.sentinel = MM2_FOUND_SENTINEL_EMPTY;
    mm2_found_items_write(a4, &loot);
    mm2_gs_set_u8(a4, -0x5AD2, 0);

    for (int i = 0; i < party_n; ++i) {
        Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
        if (!rec) {
            continue;
        }
        rec->gems = static_cast<uint16_t>(rec->gems + gems_each);
        if (launch->roster_slots[i] < 0x18) {
            rec->gold += gold_each;
        }
    }

    for (int s = 0; s < MM2_FOUND_ITEM_SLOTS; ++s) {
        if (mm2_gs_u8(a4, -0x5AD2) != 0) {
            break;
        }
        const uint8_t id = mm2_gs_u8(a4, MM2_GS_FOUND_ITEM_ID + s);
        if (id == 0) {
            continue;
        }
        const uint8_t charges = mm2_gs_u8(a4, MM2_GS_FOUND_ITEM_CHARGES + s);
        const uint8_t flags = mm2_gs_u8(a4, MM2_GS_FOUND_ITEM_FLAGS + s);
        bool placed = false;
        for (int i = 0; !placed && i < party_n; ++i) {
            Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
            if (!rec) {
                continue;
            }
            auto *raw = reinterpret_cast<uint8_t *>(rec);
            for (int m = 0; m < 6; ++m) {
                if (raw[0x3A + m] == 0) {
                    raw[0x3A + m] = id;
                    raw[0x40 + m] = charges;
                    raw[0x46 + m] = flags;
                    mm2_gs_set_u8(a4, MM2_GS_FOUND_ITEM_ID + s, 0);
                    mm2_gs_set_u8(a4, MM2_GS_FOUND_ITEM_CHARGES + s, 0);
                    mm2_gs_set_u8(a4, MM2_GS_FOUND_ITEM_FLAGS + s, 0);
                    placed = true;
                    break;
                }
            }
        }
        if (!placed) {
            const uint8_t ov = mm2_gs_u8(a4, -0x5AD2);
            mm2_gs_set_u8(a4, -0x5AD2, static_cast<uint8_t>(ov + 1));
            mm2_gs_set_u8(a4, MM2_GS_FOUND_SENTINEL, MM2_FOUND_SENTINEL_FILLED);
            break;
        }
    }

    mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 3);
    if (msg && msg_cap > 0) {
        /* 0x1ACFA..0x1AD5E: "Each share = N Gold" [+ " + M Gems"]. */
        if (gems_each != 0) {
            std::snprintf(msg, msg_cap, "Each share = %u Gold\n+ %u Gems",
                          static_cast<unsigned>(gold_each), static_cast<unsigned>(gems_each));
        } else {
            std::snprintf(msg, msg_cap, "Each share = %u Gold",
                          static_cast<unsigned>(gold_each));
        }
    }
    return true;
}

SearchPrepareResult eventVmSearchPrepare(uint8_t *a4, Mm2RosterFile *roster,
                                         const Mm2PartyLaunch *launch, gameplay::Rng *rng,
                                         SearchPrepareOut *out)
{
    if (out) {
        out->rating = 0;
        out->container_name[0] = '\0';
        out->msg[0] = '\0';
    }
    if (!a4) {
        return SearchPrepareResult::Nothing;
    }
    mm2_gs_set_u8(a4, -0x79E4, 0); /* 0x1B1B0 */

    if (!mm2_found_items_has_loot(a4)) {
        if (out) {
            std::snprintf(out->msg, sizeof(out->msg), "Nothing Here!");
        }
        return SearchPrepareResult::Nothing;
    }

    const uint8_t sent0 = mm2_gs_u8(a4, MM2_GS_FOUND_SENTINEL);
    if (sent0 == 0) {
        /* Long path — rate, arm Identify modal; do NOT distribute yet. */
        Mm2FoundItems peek{};
        mm2_found_items_read(a4, &peek);
        const uint8_t score = searchComputeRating(peek);
        const uint8_t rating = searchApplyRatingRng(score, rng);
        mm2_gs_set_u8(a4, -0x79E4, 1); /* 0x1B30C during Identify window */
        mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 7); /* 0x1B48E */
        if (out) {
            out->rating = rating;
            searchFillContainerName(a4, score, out->container_name, sizeof(out->container_name));
            /* 0x1B320..0x1B3E0: Search… / The Party Has found a: / Treasure! /
             * container name / 1) Open It … 4) Leave it */
            std::snprintf(out->msg, sizeof(out->msg),
                          "Search...\nThe Party Has found a:\nTreasure!\n%s\n"
                          "1) Open It\n2) Find Trap\n3) Detect Magic\n4) Leave it",
                          out->container_name[0] ? out->container_name : "Treasure!");
        }
        return SearchPrepareResult::NeedIdentify;
    }

    /* Short path @ 0x1B49A → distribute immediately. */
    char tmp[96];
    eventVmSearchDistribute(a4, roster, launch, tmp, sizeof(tmp));
    if (out) {
        std::snprintf(out->msg, sizeof(out->msg), "%s", tmp);
    }
    return SearchPrepareResult::Distributed;
}

bool eventVmSearchPayoff(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                         char *msg, size_t msg_cap)
{
    /* One-shot: prepare; if Identify needed, auto-Open (no modal). */
    SearchPrepareOut prep{};
    const SearchPrepareResult r = eventVmSearchPrepare(a4, roster, launch, nullptr, &prep);
    if (r == SearchPrepareResult::Nothing) {
        if (msg && msg_cap > 0) {
            std::snprintf(msg, msg_cap, "%s", prep.msg);
        }
        return false;
    }
    if (r == SearchPrepareResult::NeedIdentify) {
        eventVmSearchDistribute(a4, roster, launch, msg, msg_cap);
        return true;
    }
    if (msg && msg_cap > 0) {
        std::snprintf(msg, msg_cap, "%s", prep.msg);
    }
    return true;
}

SearchOpenResult eventVmSearchOpenOrFind(uint8_t *a4, Mm2RosterFile *roster,
                                         const Mm2PartyLaunch *launch, int party_slot,
                                         uint8_t rating, bool find_traps, gameplay::Rng *rng)
{
    /* 0x1AEC2 (Open) / 0x1AF6E (Find): member pick already done by host.
     * Thievery +$1E vs rng(1,100): fail if roll<=$60 AND roll>thievery.
     * Trap spring @ 0x1AA70: damage = rating*2+4 via 0x1A90E (OP_31-style). */
    SearchOpenResult r;
    (void)a4;
    if (!roster || !launch || party_slot < 0 || party_slot >= launch->party_count) {
        r.aborted = true;
        return r;
    }
    Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[party_slot]);
    if (!rec) {
        r.aborted = true;
        return r;
    }
    /* Afflicted high-nibble rejects (0x1AE9A andi #$F0). */
    if ((rec->condition & 0xF0) != 0) {
        r.aborted = true;
        return r;
    }

    const int thievery = searchThievery(*rec);
    const int roll = rng ? rng->range(1, 100) : 1;
    const bool fail = (roll <= 0x60) && (roll > thievery);
    if (fail && rating != 0) {
        /* Trap: damage = rating*2+4 (0x1AA7C..0x1AA86). */
        r.trapped = true;
        r.trap_damage = static_cast<uint16_t>(static_cast<uint16_t>(rating) * 2u + 4u);
        eventVmApplyOp31Damage(rec, r.trap_damage);
    }
    /* Find Traps always opens after the roll (0x1AFDA → Open with rating $FF skip).
     * Open opens unless we treat fail as still opening after trap — ASM Open
     * continues to distribute after trap (0x1AF40 → 0x1AC94). */
    (void)find_traps;
    r.opened = true;
    return r;
}

void eventVmSearchDetectMagic(uint8_t *a4, uint8_t rating, char *msg, size_t msg_cap)
{
    /* 0x1AFE8: count non-zero flags/charges @ -$3F19/-$3F16; Yes/No @ 0x1A89C. */
    if (!msg || msg_cap == 0) {
        return;
    }
    int n = 0;
    if (a4) {
        for (int i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
            if (mm2_gs_u8(a4, MM2_GS_FOUND_ITEM_FLAGS + i) != 0 ||
                mm2_gs_u8(a4, MM2_GS_FOUND_ITEM_CHARGES + i) != 0) {
                ++n;
            }
        }
    }
    const char *mag = n != 0 ? "Yes" : "No";
    const char *trap = rating != 0 ? "Yes" : "No";
    std::snprintf(msg, msg_cap, "Contents magical (%s), has trap (%s)", mag, trap);
}

void eventVmSearchLeave(uint8_t *a4)
{
    /* 0x1B45C / 0x1B48E: keep loot, -$7950 := 7. */
    if (a4) {
        mm2_gs_set_u8(a4, -0x79E4, 0);
        mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 7);
    }
}

void eventVmExecEngineCall(uint8_t *a4, uint8_t index, world::MapWorld *world)
{
    (void)world;
    if (!a4) {
        return;
    }
    /* OP_0D (event_op0d @ 0x15EC4) calls engine thunk -$7E42 -> 0x06FB8 with this
     * index (valid 0..9; index 0 gated by enable flag -$79AF, 1..9 by -$79B0).
     * 0x06FB8 is a CANNED ON-SCREEN SEQUENCE PLAYER: it loads a draw-command list
     * from ROM table -$7232[index], rendering (row, glyph/value) byte-pairs via
     * 0x77AA until a 0xFF terminator (abortable via input poll -$7BD2). It touches
     * ONLY local/stack state + the renderer — no game-state writes — so for game
     * LOGIC fidelity this is safely a no-op; only the visual sequence is missing
     * (presentation layer, like the Eagle/Wizard Eye overhead render).
     * The index 0x09 redraw-exit-flag below is a pragmatic pre-transition refresh
     * approximation, not an ASM-required side effect. */
    switch (index) {
    case 0x09:
        mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(a4, MM2_GS_EXIT_FLAGS) | 1));
        break;
    default:
        break;
    }
}

namespace {

/** 0x6ACE / 0x6B9A — helpers live at file top of anonymous namespace. */

}  // namespace

bool eventVmPartyTryPayGold(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                            uint32_t amount)
{
    /* party_gold_pool_pay @ 0x6ACE (thunk -$7E6C). */
    const int count = partyActiveCount(a4, launch);
    uint32_t total = 0;
    for (int i = 0; i < count; ++i) {
        if (!partySlotEligible(a4, launch, i)) {
            continue;
        }
        if (roster && launch) {
            const Mm2RosterRecord *rec = rosterRecord(roster, launch->roster_slots[i]);
            if (rec) {
                total += rec->gold;
            }
        } else if (a4) {
            const int idx = partyRosterIndex(a4, i);
            if (idx >= 0) {
                total += mm2_gs_u32(a4, MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x66);
            }
        }
    }
    if (total < amount) {
        return false;
    }
    uint32_t remain = total - amount;
    bool pooled = false;
    for (int i = 0; i < count; ++i) {
        if (!partySlotEligible(a4, launch, i)) {
            continue;
        }
        if (roster && launch) {
            Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
            if (!rec) {
                continue;
            }
            rec->gold = pooled ? 0u : remain;
            pooled = true;
            remain = 0;
        } else if (a4) {
            const int idx = partyRosterIndex(a4, i);
            if (idx < 0) {
                continue;
            }
            const int off = MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x66;
            mm2_gs_set_u32(a4, off, pooled ? 0u : remain);
            pooled = true;
            remain = 0;
        }
    }
    return pooled || amount == 0;
}

bool eventVmPartyTryPayGems(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                            uint16_t amount)
{
    /* party_gems_pool_pay @ 0x6B9A (thunk -$7E66). */
    const int count = partyActiveCount(a4, launch);
    uint32_t total = 0;
    for (int i = 0; i < count; ++i) {
        if (!partySlotEligible(a4, launch, i)) {
            continue;
        }
        if (roster && launch) {
            const Mm2RosterRecord *rec = rosterRecord(roster, launch->roster_slots[i]);
            if (rec) {
                total += rec->gems;
            }
        } else if (a4) {
            const int idx = partyRosterIndex(a4, i);
            if (idx >= 0) {
                total += mm2_gs_u16(a4, MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x5C);
            }
        }
    }
    if (total < amount) {
        return false;
    }
    uint16_t remain = static_cast<uint16_t>(total - amount);
    bool pooled = false;
    for (int i = 0; i < count; ++i) {
        if (!partySlotEligible(a4, launch, i)) {
            continue;
        }
        if (roster && launch) {
            Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
            if (!rec) {
                continue;
            }
            rec->gems = pooled ? 0u : remain;
            pooled = true;
            remain = 0;
        } else if (a4) {
            const int idx = partyRosterIndex(a4, i);
            if (idx < 0) {
                continue;
            }
            const int off = MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x5C;
            mm2_gs_set_u16(a4, off, pooled ? 0u : remain);
            pooled = true;
            remain = 0;
        }
    }
    return pooled || amount == 0;
}

bool eventVmPartyTryPayFood(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                            uint8_t amount)
{
    /* party_food_pool_pay @ 0x6C66 (thunk -$7E60). */
    const int count = partyActiveCount(a4, launch);
    uint32_t total = 0;
    for (int i = 0; i < count; ++i) {
        if (!partySlotEligible(a4, launch, i)) {
            continue;
        }
        if (roster && launch) {
            const Mm2RosterRecord *rec = rosterRecord(roster, launch->roster_slots[i]);
            if (rec) {
                total += rec->food;
            }
        } else if (a4) {
            const int idx = partyRosterIndex(a4, i);
            if (idx >= 0) {
                total += mm2_gs_u8(a4, MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x25);
            }
        }
    }
    if (total < amount) {
        return false;
    }
    uint8_t remain = static_cast<uint8_t>(total - amount);
    bool pooled = false;
    for (int i = 0; i < count; ++i) {
        if (!partySlotEligible(a4, launch, i)) {
            continue;
        }
        if (roster && launch) {
            Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[i]);
            if (!rec) {
                continue;
            }
            rec->food = pooled ? 0u : remain;
            pooled = true;
            remain = 0;
        } else if (a4) {
            const int idx = partyRosterIndex(a4, i);
            if (idx < 0) {
                continue;
            }
            const int off = MM2_GS_ROSTER_BASE + idx * MM2_GS_ROSTER_STRIDE + 0x25;
            mm2_gs_set_u8(a4, off, pooled ? 0u : remain);
            pooled = true;
            remain = 0;
        }
    }
    return pooled || amount == 0;
}

const uint8_t kArenaAreaIndex[5] = {0, 2, 0, 0, 1};

const uint32_t kArenaGoldReward[4][3] = {
    {200, 1500, 500},
    {2000, 5000, 3000},
    {7000, 15000, 10000},
    {20000, 50000, 30000},
};

Mm2ArenaTicket eventVmFindArenaTicket(const uint8_t *a4, const Mm2RosterFile *roster,
                                      const Mm2PartyLaunch *launch)
{
    (void)a4;
    Mm2ArenaTicket result;
    if (!roster || !launch) {
        return result;
    }
    for (int m = 0; m < launch->party_count && m < MM2_PARTY_LAUNCH_SLOTS; ++m) {
        const Mm2RosterRecord *rec = rosterRecord(roster, launch->roster_slots[m]);
        if (!rec) {
            continue;
        }
        for (int slot = 0; slot < MM2_ROSTER_ITEM_SLOTS; ++slot) {
            const uint8_t id = rec->backpack_id[slot];
            if (id >= 0xD0u && id <= 0xD3u) {
                result.found = true;
                result.color = static_cast<uint8_t>(id - 0xD0u);
                result.member_slot = m;
                result.backpack_slot = slot;
                return result;
            }
        }
    }
    return result;
}

void eventVmConsumeArenaTicket(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                const Mm2ArenaTicket &ticket)
{
    if (!roster || !launch || !ticket.found) {
        return;
    }
    if (ticket.member_slot < 0 || ticket.member_slot >= launch->party_count) {
        return;
    }
    Mm2RosterRecord *rec = rosterRecordMut(roster, launch->roster_slots[ticket.member_slot]);
    if (!rec || ticket.backpack_slot < 0 || ticket.backpack_slot >= MM2_ROSTER_ITEM_SLOTS) {
        return;
    }
    rec->backpack_id[ticket.backpack_slot] = 0;
    rec->backpack_charges[ticket.backpack_slot] = 0;
    rec->backpack_flags[ticket.backpack_slot] = 0;
}

uint8_t eventVmArenaMonsterType(uint8_t color, int screen, int rng_1_to_16)
{
    if (color > 3) {
        color = 3;
    }
    const uint8_t area = (screen >= 0 && screen < 5) ? kArenaAreaIndex[screen] : kArenaAreaIndex[0];
    int roll = rng_1_to_16;
    if (roll < 1) {
        roll = 1;
    }
    if (roll > 16) {
        roll = 16;
    }
    const int type = (static_cast<int>(color) * 3 + area) * 16 + roll;
    return static_cast<uint8_t>(type & 0xFF);
}

uint32_t eventVmArenaGoldReward(uint8_t color, int screen)
{
    if (color > 3) {
        color = 3;
    }
    int tier = screen;
    if (tier < 0) {
        tier = 0;
    }
    if (tier > 2) {
        tier = 2;
    }
    return kArenaGoldReward[color][tier];
}

int eventVmSelectedPartySlot(const uint8_t *a4)
{
    if (!a4) {
        return -1;
    }
    const uint8_t sel = mm2_gs_u8(a4, MM2_GS_SELECTED_MEMBER);
    if (sel < 1 || sel > MM2_GS_PARTY_SIZE) {
        return -1;
    }
    return static_cast<int>(sel - 1);
}

void eventVmSetSelectedPartySlot(uint8_t *a4, int slot_1_to_8)
{
    if (!a4) {
        return;
    }
    if (slot_1_to_8 < 1 || slot_1_to_8 > MM2_GS_PARTY_SIZE) {
        mm2_gs_set_u8(a4, MM2_GS_SELECTED_MEMBER, 0);
        return;
    }
    mm2_gs_set_u8(a4, MM2_GS_SELECTED_MEMBER, static_cast<uint8_t>(slot_1_to_8));
}

int eventVmTrainingTownIndex(int map_screen)
{
    static const uint8_t kMapToTown[5] = {1, 5, 2, 4, 2};
    if (map_screen < 0 || map_screen >= 5) {
        return 1;
    }
    return static_cast<int>(kMapToTown[map_screen]);
}

uint32_t eventVmTrainingCostPerChar(int level, int town_index)
{
    if (level < 0 || town_index < 0) {
        return 0;
    }
    return static_cast<uint32_t>(level) * static_cast<uint32_t>(town_index) * 50u;
}

uint32_t eventVmHealingCostPerChar(int level, int town_index)
{
    if (level < 0 || town_index < 0) {
        return 0;
    }
    return static_cast<uint32_t>(level) * static_cast<uint32_t>(town_index) * 10u;
}

void eventVmInitStrBankOffsets(uint8_t *a4)
{
    /* 0x9666 reads word[bank] from A4-$71E8; producer not in CODE — seed ASM-clear spans. */
    if (!a4) {
        return;
    }
    for (int i = 0; i < kStrBankCount; ++i) {
        mm2_gs_set_u16(a4, MM2_GS_STR_BANK_OFFS + i * 2, kStrBankOffs[i]);
    }
    /* Sentinel end-of-file for bank-length math (next-start − start). */
    mm2_gs_set_u16(a4, MM2_GS_STR_BANK_OFFS + kStrBankCount * 2, kStrBankOffs[kStrBankCount]);
    mm2_gs_set_u16(a4, MM2_GS_STR_BANK_CURSOR, 0);
}

void eventVmDecodeStrBank(uint8_t *a4, int bank_index, const uint8_t *str_dat, size_t str_len)
{
    /* 0x9666 with -$ED6 set: copy [off, off+$924) through +$1C / 0x1D→0 into -$ED2. */
    if (!a4 || !str_dat || bank_index < 0 || bank_index >= kStrBankCount) {
        return;
    }
    const uint16_t off = mm2_gs_u16(a4, MM2_GS_STR_BANK_OFFS + bank_index * 2);
    for (uint16_t i = 0; i < kStrBankSpan; ++i) {
        const size_t src = static_cast<size_t>(off) + i;
        uint8_t c = 0;
        if (src < str_len) {
            c = static_cast<uint8_t>((str_dat[src] + 0x1C) & 0xFF);
            if (c == 0x1D) {
                c = 0;
            }
        }
        mm2_gs_set_u8(a4, -0x0ED2 + static_cast<int32_t>(i), c);
    }
    mm2_gs_set_u16(a4, MM2_GS_STR_BANK_CURSOR, 0);
}

const char *eventVmNextStrBankCString(uint8_t *a4)
{
    /* 0x976E: return ptr to current C-string in -$ED2; advance cursor past NUL. */
    if (!a4) {
        return nullptr;
    }
    uint16_t cur = mm2_gs_u16(a4, MM2_GS_STR_BANK_CURSOR);
    while (cur < kStrBankSpan && mm2_gs_u8(a4, -0x0ED2 + static_cast<int32_t>(cur)) == 0) {
        ++cur;
    }
    if (cur >= kStrBankSpan) {
        mm2_gs_set_u16(a4, MM2_GS_STR_BANK_CURSOR, cur);
        return nullptr;
    }
    const char *out = reinterpret_cast<const char *>(a4 + (-0x0ED2) + static_cast<int32_t>(cur));
    uint16_t p = cur;
    while (p < kStrBankSpan && mm2_gs_u8(a4, -0x0ED2 + static_cast<int32_t>(p)) != 0) {
        ++p;
    }
    if (p < kStrBankSpan) {
        ++p;
    }
    mm2_gs_set_u16(a4, MM2_GS_STR_BANK_CURSOR, p);
    return out;
}

}  // namespace mm2::events
