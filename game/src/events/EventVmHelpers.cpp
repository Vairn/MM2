#include "mm2/events/EventVmHelpers.h"

#include "mm2/CppStdCompat.h"
#include "mm2/events/EventFieldMap.h"
#include "mm2_found_items.h"
#include "mm2_map_codec.h"

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
    case 0x0A:
    case 0x0D:
    case 0x11:
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

void eventVmClearTileEventFlag(uint8_t *a4, int y, int x)
{
    const int idx = tileIndex(y, x);
    if (!a4 || idx < 0) {
        return;
    }
    mm2_gs_set_u8(a4, MM2_GS_TILE_VISITED + idx,
                  static_cast<uint8_t>(mm2_gs_u8(a4, MM2_GS_TILE_VISITED + idx) & static_cast<uint8_t>(~0x80)));
    mm2_gs_set_u8(a4, MM2_GS_TILE_RT_FLAGS + idx,
                  static_cast<uint8_t>(mm2_gs_u8(a4, MM2_GS_TILE_RT_FLAGS + idx) & static_cast<uint8_t>(~0x80)));
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

void eventVmApplyPartyByteOp(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             uint8_t count, uint8_t selector, uint8_t val, bool masked,
                             uint8_t and_m, uint8_t or_m, bool test_only, bool *out_cond)
{
    if (out_cond) {
        *out_cond = false;
    }
    if (!a4) {
        return;
    }

    /* OP_15/18 (event_op15_party_state_apply @ 0x16426) ALWAYS set bit7 on the
     * selector before calling the field engine (0x163CA -> 0x17766), so these
     * are byte-granular bit operations on a character-record field. Translate
     * the script selector to its real record byte offset via the ROM jump
     * table @ 0x17FEA (EventFieldMap.h) instead of using the selector byte
     * directly as an offset (the old code's bug: selector 0x6D wrote record
     * 0x6D instead of the real 0x50, selector 0x74 invented a global). */
    int field_off = -1;
    int field_w = 1;
    if (!eventVmResolveMemberField(selector, &field_off, &field_w)) {
        /* computed getter (sel 0x00/0x01 = max HP/SP) — no writable offset. */
        return;
    }
    /* These ops are byte-granular (bit7 path forces width 1 @ 0x18100); for the
     * rare multi-byte selector the engine targets the Amiga low byte. We operate
     * on the field's base byte, which is correct for all byte-flag selectors.
     * NOTE: selector 0x74 -> record offset 0x79 (class_quest_guild_mask) now flows
     * through the normal per-member path below — the bits (e.g. "seen Pegasus"
     * 0x40, set by OP_18 @ 0x9FE0) are stored per character record, matching ASM.
     * The old global MM2_GS_PARTY_PROGRESS bridge has been removed (that A4 byte
     * -$79E8 is an unrelated engine flag the port had misappropriated). */
    (void)field_w;

    int members = count;
    int selected_only = -1;
    if (members == 0x09) {
        selected_only = eventVmSelectedPartySlot(a4);
        if (selected_only < 0) {
            if (out_cond) {
                *out_cond = false;
            }
            return;
        }
        members = 1;
    } else if (members == 0 || members > MM2_PARTY_LAUNCH_SLOTS) {
        members = MM2_PARTY_LAUNCH_SLOTS;
    }

    bool any_match = false;
    for (int i = 0; i < members; ++i) {
        int party_idx = (selected_only >= 0) ? selected_only : i;
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

        if (test_only) {
            if ((byte_val & val) != 0) {
                any_match = true;
            }
            continue;
        }

        if (masked) {
            byte_val = static_cast<uint8_t>((byte_val & and_m) | or_m);
            if (roster) {
                uint8_t *raw = reinterpret_cast<uint8_t *>(&roster->records[roster_idx]);
                if (field_off < MM2_ROSTER_RECORD_SIZE) {
                    raw[field_off] = byte_val;
                }
            } else {
                const int off = MM2_GS_ROSTER_BASE + roster_idx * MM2_GS_ROSTER_STRIDE + field_off;
                mm2_gs_set_u8(a4, off, byte_val);
            }
        }
    }

    if (test_only && out_cond) {
        *out_cond = any_match;
    }
}

bool eventVmCheckOp30Password(const uint8_t *input_buf, const uint8_t *expected,
                              size_t expected_len)
{
    if (!input_buf || !expected || expected_len == 0) {
        return false;
    }
    char decoded[16];
    size_t n = expected_len;
    if (n > sizeof(decoded) - 1) {
        n = sizeof(decoded) - 1;
    }
    for (size_t i = 0; i < n; ++i) {
        decoded[i] = static_cast<char>((0x11A - expected[i]) & 0xFF);
    }
    decoded[n] = '\0';
    while (n > 0 && decoded[n - 1] == ' ') {
        decoded[--n] = '\0';
    }

    if (input_buf[0] == '\0') {
        return false;
    }
    return std::strcmp(reinterpret_cast<const char *>(input_buf),
                       decoded) == 0;
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
    /* event_op2a_set_reward_block @ 0x16D16. The ROM handler does NOT distribute
     * the reward to the party here — it ONLY fills the shared found-item /
     * treasure buffer (gold/exp -> A4-$3F10, gems -> A4-$3F12, 3x(id,charges,
     * flags) -> A4-$3F1C/-$3F16/-$3F19) and raises the sentinel A4-$794C=0xFF.
     * The actual "you found..." pickup/distribution is the Search payoff
     * (Search key @ 0x4800 -> -$7D1C -> 0x1B19C), which is engine/presentation
     * territory not yet ported — so we faithfully model the buffer state and
     * defer the display/distribution. (The previous port fabricated an immediate
     * deposit into party member 0; that is removed.) */
    (void)roster;
    (void)launch;
    if (!a4 || !block) {
        return;
    }
    mm2_found_items_op2a_fill(a4, block);
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

bool eventVmCheckCode16(uint8_t *a4, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                        uint16_t code)
{
    if (code == 0) {
        return true;
    }
    /* Arena ticket colors: 0=green 208, 1=yellow 209, 2=red 210, 3=black 211. */
    if (code <= 3) {
        const uint8_t ticket_id = static_cast<uint8_t>(208u + code);
        return eventVmPartyHasItem(a4, roster, launch, ticket_id, false);
    }
    /* Bishop keys and similar keyed checks reuse small integer codes in scripts. */
    if (code >= 0x10 && code <= 0x13) {
        const uint8_t key_id = static_cast<uint8_t>(0x70 + code);
        return eventVmPartyHasItem(a4, roster, launch, key_id, false);
    }
    return false;
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

}  // namespace mm2::events
