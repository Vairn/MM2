#include "mm2/events/EventRuntime.h"

#include "mm2/DataPath.h"
#include "mm2/events/EventCombatEncounter.h"
#include "mm2/events/EventPartyEffects.h"
#include "mm2/events/EventTownServices.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/ServiceSignResolver.h"
#include "mm2/runtime/PathScratch.h"

#include "mm2_attrib_codec.h"
#include "mm2_found_items.h"
#include "mm2_party_launch.h"

namespace mm2::events {

namespace {

/* Token length deltas for opcodes 0x00..0x32 (A4-$6CC8), from event_token_len_table.json. */
constexpr uint8_t kOpTokenDelta[0x33] = {
    1,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  3,  3,  2,  2,  1,  2,  2,  13, 11, 1,  4,  3,  3,  5,  5,  3,
    2,  2,  2,  2,  7,  7,  4,  3,  3,  3,  3,  1,  1,  3,  1,  15, 2,  2,  3,  3,  1,  11, 4,  2,
};

void initContextMaskTable(uint8_t *a4)
{
    /* Facing index 0/2/4/6 (W/S/E/N) → context_mask_tbl @ A4-$6BE6 (scanner @ 0x175FE).
     * Verified from EXTRACTED/ghidra/mm2_data_00.bin @ A4-$6BE6 (file off 0x1418). */
    static const uint8_t kMasks[8] = {0x10, 0, 0x20, 0, 0x40, 0, 0x80, 0};
    for (int i = 0; i < 8; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_CONTEXT_MASK_TBL + i, kMasks[i]);
    }
}

/* Tile scanner cond test @ 0x17684: (triplet_cond & context) != 0.
 * context_mask_tbl @ A4-$6BE6 maps facing index 0/2/4/6 (W/S/E/N) to 0x10/0x20/0x40/0x80.
 * Triplet cond uses the same bit set — 0x10 is west-facing, not "all facings". */
bool eventCondMatches(uint8_t cond, uint8_t ctx)
{
    return (cond & ctx) != 0;
}

int tokenDelta(uint8_t tok)
{
    if (tok < sizeof(kOpTokenDelta)) {
        return kOpTokenDelta[tok];
    }
    return 1;
}

void captureServiceTitle(const char *text, char *out, size_t cap)
{
    if (!out || cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!text) {
        return;
    }
    size_t j = 0;
    for (size_t i = 0; text[i] != '\0' && j + 1 < cap; ++i) {
        if (text[i] == '\n') {
            if (j == 0) {
                continue;
            }
            break;
        }
        out[j++] = text[i];
    }
    out[j] = '\0';
}

void applyPartyProgressOp(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          uint8_t count, uint8_t op, uint8_t val, bool masked, uint8_t and_m,
                          uint8_t or_m)
{
    bool cond = false;
    eventVmApplyPartyByteOp(gs.a4(), roster, launch, count, op, val, masked, and_m, or_m, !masked,
                            &cond);
    if (!masked) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, cond ? 1 : 0);
    }
}

}  // namespace

void EventRuntime::bindParty(Mm2RosterFile *roster, const Mm2PartyLaunch *launch)
{
    roster_ = roster;
    launch_ = launch;
}

bool EventRuntime::load(const char *data_dir)
{
    unload();
    data_dir_ = data_dir;
    char *path = mm2_path_scratch_a();
    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, "event.dat")) {
        return false;
    }
    if (mm2_event_load_file(path, &file_) != MM2_EVENT_OK) {
        return false;
    }
    loaded_ = true;
    return true;
}

void EventRuntime::unload()
{
    if (loaded_) {
        mm2_event_free(&file_);
        loaded_ = false;
    }
    roster_ = nullptr;
    launch_ = nullptr;
    data_dir_ = nullptr;
    location_id_ = -1;
    loc_ = nullptr;
    script_active_ = false;
    wait_ = EventVmWait::None;
    screen_changed_ = false;
    service_title_[0] = '\0';
    text_.reset();
    pending_portal_active_ = false;
    ::memset(work_buf_, 0, sizeof(work_buf_));
}

bool EventRuntime::enterLocation(int location_id, GameStateView &gs, const world::MapWorld &world)
{
    if (!loaded_ || !gs.valid() || location_id < 0 || location_id >= MM2_EVENT_LOCATION_COUNT) {
        return false;
    }

    location_id_ = location_id;
    loc_ = &file_.locations[location_id];
    ServiceSignResolver::syncSignEnvId(gs.a4(), static_cast<int>(gs.screenId()), &world.attrib());

    const size_t copy_len =
        loc_->raw_len < MM2_GS_EVENT_WORK_SIZE ? loc_->raw_len : MM2_GS_EVENT_WORK_SIZE;
    ::memcpy(work_buf_, loc_->raw, copy_len);
    if (copy_len < MM2_GS_EVENT_WORK_SIZE) {
        ::memset(work_buf_ + copy_len, 0, MM2_GS_EVENT_WORK_SIZE - copy_len);
    }
    ::memcpy(gs.a4() + MM2_GS_EVENT_WORK_BUF, work_buf_, MM2_GS_EVENT_WORK_SIZE);

    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR, 0xFFFF);
    initContextMaskTable(gs.a4());
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERA_LOW, static_cast<uint8_t>(gs.era() & 0xFF));
    initParsed(gs);

    script_active_ = false;
    wait_ = EventVmWait::None;
    service_title_[0] = '\0';
    text_.reset();
    return true;
}

void EventRuntime::initParsed(GameStateView &gs)
{
    int pos = 0;
    while (pos + 2 < MM2_GS_EVENT_WORK_SIZE) {
        const uint8_t a = work_buf_[pos];
        const uint8_t b = work_buf_[pos + 1];
        const uint8_t c = work_buf_[pos + 2];
        pos += 3;
        if (a == 0 && b == 0 && c == 0) {
            break;
        }
    }

    uint16_t anchor = static_cast<uint16_t>(pos);
    if (pos + 1 < MM2_GS_EVENT_WORK_SIZE) {
        const uint16_t str_rel = static_cast<uint16_t>(work_buf_[pos] | (work_buf_[pos + 1] << 8));
        anchor = static_cast<uint16_t>(pos + str_rel);
    }

    const uint16_t script_start = static_cast<uint16_t>(pos + 2);
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR, anchor);
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_START, script_start);
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, script_start);
}

uint8_t EventRuntime::contextMask(const GameStateView &gs) const
{
    const uint8_t fi = gs.facingIndex();
    if (fi < 8) {
        return mm2_gs_u8(gs.a4(), MM2_GS_CONTEXT_MASK_TBL + fi);
    }
    return 0xF0;
}

bool EventRuntime::eraGateOpen(const GameStateView &gs, const world::MapWorld &world) const
{
    const uint8_t era_low = mm2_gs_u8(gs.a4(), MM2_GS_ERA_LOW);
    const uint8_t gate = mm2_attrib_era_gate(&world.attrib());
    return era_low == gate;
}

int EventRuntime::poolSeek(uint8_t event_id) const
{
    if (!loc_ || loc_->script_offset < 0 || loc_->string_table_offset <= loc_->script_offset) {
        return -1;
    }

    const int start = loc_->script_offset;
    const int end = loc_->string_table_offset;
    int record = 0;
    int seg_start = start;
    for (int i = start; i < end; ++i) {
        if (work_buf_[i] == 0xFF) {
            if (record == event_id) {
                return seg_start;
            }
            ++record;
            seg_start = i + 1;
        }
    }
    if (record == event_id) {
        return seg_start;
    }
    return -1;
}

const char *EventRuntime::resolveString(int idx, char *buf, size_t buf_cap) const
{
    if (!loc_ || buf_cap == 0) {
        if (buf_cap > 0) {
            buf[0] = '\0';
        }
        return buf;
    }

    buf[0] = '\0';
    if (idx < 0 || loc_->string_table_offset < 0 ||
        static_cast<size_t>(loc_->string_table_offset) >= loc_->raw_len) {
        return buf;
    }

    int cur = 0;
    size_t pos = static_cast<size_t>(loc_->string_table_offset);
    while (pos < loc_->raw_len) {
        size_t end = pos;
        while (end < loc_->raw_len && loc_->raw[end] != 0xFF) {
            ++end;
        }
        if (cur == idx) {
            size_t j = 0;
            for (size_t i = pos; i < end && j + 1 < buf_cap; ++i) {
                buf[j++] = static_cast<char>(loc_->raw[i]);
            }
            buf[j] = '\0';
            normalizeAtToNewline(buf);
            return buf;
        }
        ++cur;
        pos = end + 1;
    }

    std::snprintf(buf, buf_cap, "<str[%d]>", idx);
    return buf;
}

void EventRuntime::normalizeAtToNewline(char *s) const
{
    for (char *p = s; *p; ++p) {
        if (*p == '@') {
            *p = '\n';
        }
    }
}

uint8_t EventRuntime::readU8(GameStateView &gs)
{
    const int pos = mm2_gs_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS);
    if (pos < 0 || pos >= MM2_GS_EVENT_WORK_SIZE) {
        return 0xFF;
    }
    const uint8_t b = work_buf_[pos];
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, static_cast<uint16_t>(pos + 1));
    return b;
}

void EventRuntime::skipTokens(GameStateView &gs, int count)
{
    for (int i = 0; i < count; ++i) {
        const int pos = mm2_gs_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS);
        if (pos < 0 || pos >= MM2_GS_EVENT_WORK_SIZE) {
            break;
        }
        const uint8_t tok = work_buf_[pos];
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, static_cast<uint16_t>(pos + tokenDelta(tok)));
    }
}

void EventRuntime::applyMapTransition(GameStateView &gs, world::MapWorld &world, uint8_t dest_screen,
                                      uint8_t dest_tile)
{
    world.enterScreen(dest_screen);
    gs.setScreenId(dest_screen);
    gs.setCoordX(static_cast<uint8_t>(dest_tile & 0x0F));
    gs.setCoordY(static_cast<uint8_t>((dest_tile >> 4) & 0x0F));
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERA_LOW, static_cast<uint8_t>(gs.era() & 0xFF));
    ServiceSignResolver::syncSignEnvId(gs.a4(), static_cast<int>(dest_screen), &world.attrib());
    enterLocation(static_cast<int>(dest_screen), gs, world);
    screen_changed_ = true;
}

bool EventRuntime::finishPendingPortal(GameStateView &gs, world::MapWorld &world, bool accepted)
{
    if (!pending_portal_active_) {
        return false;
    }
    pending_portal_active_ = false;

    if (!accepted) {
        return false;
    }

    const uint32_t have = eventVmPartyGoldTotal(gs.a4(), roster_, launch_);
    if (have < pending_portal_.cost) {
        text_.showOp02("Not enough gold!", 19);
        text_.showSpacePrompt();
        wait_ = EventVmWait::Space;
        return true;
    }

    if (pending_portal_.cost > 0) {
        eventVmDeductPartyGold(gs.a4(), roster_, launch_, pending_portal_.cost);
    }
    applyMapTransition(gs, world, pending_portal_.dest_screen, pending_portal_.dest_tile);
    endScript(gs);
    return true;
}

void EventRuntime::endScript(GameStateView &gs)
{
    bool redraw_status = false;
    bool redraw_roster = false;
    bool redraw_divider = false;
    text_.scriptCleanup(&redraw_status, &redraw_roster, &redraw_divider);
    (void)redraw_status;
    (void)redraw_roster;
    (void)redraw_divider;
    mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);
    script_active_ = false;
    wait_ = EventVmWait::None;
}

void EventRuntime::abortScript(GameStateView &gs)
{
    /* VM loop @ 0x17540: abort set → skip $171AC cleanup; OP_02 message stays visible. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);
    script_active_ = false;
    wait_ = EventVmWait::None;
}

void EventRuntime::dispatchOp(GameStateView &gs, world::MapWorld &world, uint8_t op)
{
    char text_buf[256];

    switch (op) {
    case 0x01: {
        const uint8_t idx = readU8(gs);
        text_.showOp01(resolveString(idx, text_buf, sizeof(text_buf)));
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 1));
        break;
    }
    case 0x02: {
        const uint8_t idx = readU8(gs);
        text_.showOp02(resolveString(idx, text_buf, sizeof(text_buf)), 19);
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 2));
        break;
    }
    case 0x03: {
        const uint8_t idx = readU8(gs);
        text_.showOp03(resolveString(idx, text_buf, sizeof(text_buf)));
        if (location_id_ == 11 && idx == 5 && data_dir_) {
            text_.showPegasusIllustration(data_dir_);
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 3));
        break;
    }
    case 0x04: {
        const uint8_t idx = readU8(gs);
        text_.showOp04(resolveString(idx, text_buf, sizeof(text_buf)));
        break;
    }
    case 0x05: {
        const uint8_t idx = readU8(gs);
        text_.showOp05(resolveString(idx, text_buf, sizeof(text_buf)));
        break;
    }
    case 0x06: {
        const uint8_t idx = readU8(gs);
        text_.showOp06(resolveString(idx, text_buf, sizeof(text_buf)));
        break;
    }
    case 0x07:
        text_.showSpacePrompt();
        wait_ = EventVmWait::Space;
        break;
    case 0x08:
        text_.showSpacePrompt();
        wait_ = EventVmWait::Space;
        break;
    case 0x09:
    case 0x0A:
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 0);
        wait_ = EventVmWait::YesNo;
        break;
    case 0x0F:
        endScript(gs);
        break;
    case 0x10: {
        const uint8_t n = readU8(gs);
        if (mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG)) {
            skipTokens(gs, n);
        }
        break;
    }
    case 0x11: {
        const uint8_t n = readU8(gs);
        if (!mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG)) {
            skipTokens(gs, n);
        }
        break;
    }
    case 0x22: {
        /* event_op22_era_gate @ 0x16A9E: cond = (MM2_GS_ERA_LOW in [lo..hi]). */
        const uint8_t lo = readU8(gs);
        const uint8_t hi = readU8(gs);
        const uint8_t era = mm2_gs_u8(gs.a4(), MM2_GS_ERA_LOW);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, (era >= lo && era <= hi) ? 1 : 0);
        break;
    }
    case 0x29:
        /* event_op29_force_abort @ 0x16D08: stop script before fall-through (e.g. C2 evt 22). */
        mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
        break;
    case 0x0C: {
        const uint8_t dest_screen = readU8(gs);
        const uint8_t dest_tile = readU8(gs);
        applyMapTransition(gs, world, dest_screen, dest_tile);
        endScript(gs);
        break;
    }
    case 0x0E: {
        const uint8_t sel = readU8(gs);
        eventExecTownSelector(*this, gs, world, sel, roster_, launch_, items_, text_, wait_,
                              location_id_, service_title_);
        break;
    }
    case 0x0B: {
        const uint8_t str_idx = readU8(gs);
        const uint8_t placement = readU8(gs);
        captureServiceTitle(resolveString(str_idx, text_buf, sizeof(text_buf)), service_title_,
                            sizeof(service_title_));
        /* OP_0B @ 0x15DB0 / 0x15756: str_idx is table key (not text, not .anm id).
         * env = A4-$79E3 (area_env_lookup @ 0x18AE on map load @ 0x1C44).
         * anm = table[env][str_idx-1] → sign_sprite_load @ 0x316E.
         * Hillstone evt 15: 0b 0e 00 → str[14], env 2 → 49.anm Lord Slayer portrait. */
        text_.showOp0B(nullptr, data_dir_, gs, &world.attrib(), str_idx, placement);
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 4));
        break;
    }
    case 0x15: {
        const uint8_t count = readU8(gs);
        const uint8_t op = readU8(gs);
        const uint8_t val = readU8(gs);
        applyPartyProgressOp(gs, roster_, launch_, count, op, val, false, 0, 0);
        break;
    }
    case 0x18: {
        const uint8_t count = readU8(gs);
        const uint8_t op = readU8(gs);
        const uint8_t and_m = readU8(gs);
        const uint8_t or_m = readU8(gs);
        applyPartyProgressOp(gs, roster_, launch_, count, op, 0, true, and_m, or_m);
        break;
    }
    case 0x00:
        mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
        break;
    case 0x0D: {
        /* OP_0D @ 0x15EC4 -> thunk -$7E42 -> 0x06FB8: play canned on-screen
         * sequence #idx (presentation only; no game-state side effects — see
         * eventVmExecEngineCall). The handler consumes the 1 index byte. */
        const uint8_t idx = readU8(gs);
        eventVmExecEngineCall(gs.a4(), idx, &world);
        break;
    }
    case 0x14:
        eventVmClearTileEventFlag(gs.a4(), gs.coordY(), gs.coordX());
        break;
    case 0x16: {
        /* event_op16_scan_party_items @ 0x16520: reads 2 bytes (arg1 read then
         * OVERWRITTEN by arg2 — only arg2 is used). Scans each party member's
         * record (get_party_member_ptr_by_slot @ 0x477E, bound = party count
         * A4-$795A); for m in 0..5 increments cond_flag when record[0x3A+m] OR
         * record[0x28+m] == arg2, then breaks at the first member with any match.
         * +0x28/+0x3A are the equipped/backpack item-id runs (6 ids each) — so this
         * is an "any party member carrying item arg2" check. cond reflects the
         * match count of that first member. */
        readU8(gs);
        const uint8_t want = readU8(gs);
        uint8_t cond = 0;
        if (launch_ && roster_) {
            for (int i = 0; i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                const auto *rec = reinterpret_cast<const uint8_t *>(
                    &roster_->records[launch_->roster_slots[i]]);
                for (int m = 0; m < 6; ++m) {
                    if (rec[0x3A + m] == want || rec[0x28 + m] == want) {
                        ++cond;
                    }
                }
                if (cond != 0) {
                    break;
                }
            }
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, cond);
        break;
    }
    case 0x17: {
        /* event_op17_load_cond_var @ 0x165A4: cond_flag = *resolve(id) — the RAW
         * variable byte (`move.b (a0),-$7951`), NOT a 0/1 bool. OP_1B compares
         * cond against a threshold, so booleanizing here broke threshold gates.
         * The handler reads the id byte, then reads+discards a 2nd byte. */
        const uint8_t group = readU8(gs);
        const uint8_t index = readU8(gs);
        (void)index;
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, eventVmLoadVar(gs.a4(), group, index));
        break;
    }
    case 0x19: {
        /* event_op19_give_item @ 0x165D8: give one item to the first party member
         * with a free backpack slot; set cond_flag=1 on success (0 on entry).
         * Reads 4 bytes: arg1 (member-spec), id, attr2, attr3.
         *   - If arg1 >= 0x80, the item id is taken from cond_flag (captured at
         *     entry, BEFORE cond_flag is cleared) instead of the literal id byte.
         *   - Per member, scan the backpack item-id run rec[0x3A+m] (m=0..5) for
         *     the first empty (==0) slot and write id->rec[0x3A+m],
         *     attr2->rec[0x40+m], attr3->rec[0x46+m]. These are the SoA backpack
         *     id/bonus/flag runs (see OP_16; raw-byte access matches the file
         *     bytes — the roster struct's AoS item slots mislabel them, flagged
         *     for the roster agent).
         *   - If every backpack is full the ROM drops the item into the shared
         *     found-item buffer (overflow tail @ 0x166A0): it scans buffer id[0],
         *     id[1] for the first empty slot (else slot 2), writing id->A4-$3F1C,
         *     attr3->A4-$3F19 (flags), attr2->A4-$3F16 (charges) and raising the
         *     sentinel A4-$794C=0xFF. cond_flag stays 0 in that case (the item was
         *     NOT placed on a member). The buffer's "you found..." pickup is the
         *     deferred Search payoff (0x1B19C) — we model the buffer state here. */
        const uint8_t arg1 = readU8(gs);
        uint8_t id = readU8(gs);
        const uint8_t attr2 = readU8(gs);
        const uint8_t attr3 = readU8(gs);
        if (arg1 >= 0x80) {
            id = mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG);
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 0);
        bool placed = false;
        if (launch_ && roster_) {
            for (int i = 0; !placed && i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS;
                 ++i) {
                auto *rec = reinterpret_cast<uint8_t *>(
                    &roster_->records[launch_->roster_slots[i]]);
                for (int m = 0; m < 6; ++m) {
                    if (rec[0x3A + m] == 0) {
                        rec[0x3A + m] = id;
                        rec[0x40 + m] = attr2;
                        rec[0x46 + m] = attr3;
                        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 1);
                        placed = true;
                        break;
                    }
                }
            }
        }
        if (!placed) {
            mm2_found_items_overflow_append(gs.a4(), id, attr2, attr3);
        }
        break;
    }
    case 0x1A: {
        /* event_op1a_store_var @ 0x166F8: reads ONLY 2 bytes — var id then value
         * (the pointer resolver $15620 keys on the id alone). Reading a 3rd byte
         * desynced every script that used OP_1A. */
        const uint8_t group = readU8(gs);
        const uint8_t val = readU8(gs);
        eventVmStoreVar(gs.a4(), group, 0, val);
        break;
    }
    case 0x1B: {
        const uint8_t threshold = readU8(gs);
        if (mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG) < threshold) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 0);
        }
        break;
    }
    case 0x1C:
        /* event_op1c_engine_query @ 0x16742: cond_flag = engineQuery(1, arg1),
         * where engineQuery is the A4 function-pointer slot -$7BB4 (jsr (d16,a4),
         * resolved at runtime — NOT a static thunk). The query result is engine
         * state we cannot compute here, so this stays a documented stub that
         * consumes the 1 arg byte and writes the neutral cond_flag=0. Partial. */
        readU8(gs);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 0);
        break;
    case 0x1D:
        /* event_op1d_engine_indexed @ 0x16762: calls engine pointer -$7E84 with
         * index (arg1*7 + 1). No VM-level state change (no cond_flag / GS write):
         * pure engine dispatch. Consume 1 arg byte; behaviour deferred. Partial. */
        readU8(gs);
        break;
    case 0x1E:
        /* event_op1e_timed_wait @ 0x16780: busy-wait `arg1` iterations, each
         * delay(10) via engine pointer -$7BC0 then poll input -$7BD2, breaking
         * early on a keypress. Presentation/timing only — no game-state effect.
         * Consume 1 arg byte; the delay is a no-op in the headless port. Partial. */
        readU8(gs);
        break;
    case 0x1F:
    case 0x20: {
        uint8_t args[5];
        const uint8_t sel = readU8(gs);
        for (int i = 0; i < 5; ++i) {
            args[i] = readU8(gs);
        }
        eventApplyPartyEffect(gs, roster_, launch_, sel, args, op == 0x20);
        break;
    }
    case 0x21: {
        const uint8_t pos = readU8(gs);
        const uint8_t visual = readU8(gs);
        const uint8_t collision = readU8(gs);
        eventVmPatchMapTile(world, (pos >> 4) & 0xF, pos & 0xF, visual, collision);
        break;
    }
    case 0x23: {
        /* event_op23_day_gate @ 0x16ADA: cond = day-of-year predicate.
         * The day byte is the LOW byte of the current era's day word
         * (-$79DE[era], read as `move.b $1(a0,era*2)` — big-endian RAM word, so
         * +1 is the low 8 bits; day is 1..180 so this == day & 0xFF). arg1 is
         * read first, arg2 second (both always consumed → argc 2):
         *   arg1 == 0xB5 -> cond = (day bit0 set)   — odd-day gate
         *   arg1 == 0xB6 -> cond = (day bit0 clear)  — even-day gate
         *   else         -> cond = (arg1 <= day <= arg2)  — inclusive byte range
         * (Previously only the range path existed; the 0xB5/0xB6 moon/odd-even
         * gates silently fell through to a bogus range compare.) */
        const uint8_t arg1 = readU8(gs);
        const uint8_t arg2 = readU8(gs);
        const uint8_t day = static_cast<uint8_t>(gs.day() & 0xFF);
        uint8_t cond;
        if (arg1 == 0xB5) {
            cond = (day & 1) ? 1 : 0;
        } else if (arg1 == 0xB6) {
            cond = (day & 1) ? 0 : 1;
        } else {
            cond = (day >= arg1 && day <= arg2) ? 1 : 0;
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, cond);
        break;
    }
    case 0x24: {
        const uint8_t lo = readU8(gs);
        const uint8_t hi = readU8(gs);
        const uint32_t need = static_cast<uint32_t>(lo | (hi << 8));
        const uint32_t have = eventVmPartyGoldTotal(gs.a4(), roster_, launch_);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, have >= need ? 1 : 0);
        break;
    }
    case 0x25: {
        const uint8_t hi = readU8(gs);
        const uint8_t lo = readU8(gs);
        const uint16_t code = static_cast<uint16_t>((hi << 8) | lo);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG,
                      eventVmCheckCode16(gs.a4(), roster_, launch_, code) ? 1 : 0);
        break;
    }
    case 0x26:
        mm2_gs_set_u8(gs.a4(), MM2_GS_SAVED_COND_FLAG, mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG));
        text_.showOp02("Who will learn this skill (1-8)?", 19);
        wait_ = EventVmWait::MemberSelect;
        break;
    case 0x27:
        mm2_gs_set_u8(gs.a4(), MM2_GS_SAVED_COND_FLAG, mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG));
        text_.showOp02("Select a party member (1-8):", 19);
        wait_ = EventVmWait::MemberSelect;
        break;
    case 0x28: {
        const uint8_t probe = readU8(gs);
        const uint8_t item_id = readU8(gs);
        const bool consume = probe == 0;
        const bool has = eventVmPartyHasItem(gs.a4(), roster_, launch_, item_id, consume);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, has ? 1 : 0);
        break;
    }
    case 0x2A: {
        uint8_t block[14];
        for (int i = 0; i < 14; ++i) {
            block[i] = readU8(gs);
        }
        eventVmApplyTreasure(gs.a4(), roster_, launch_, block);
        break;
    }
    case 0x2C: {
        /* event_op2c_adjust_state @ 0x16D98: WORD add of the u8 arg into the
         * counter at -$79B8 (add.w), then set exit-flag bit0 (redraw). */
        const uint8_t add = readU8(gs);
        const uint16_t cur = mm2_gs_u16(gs.a4(), MM2_GS_SCRIPT_COUNTER);
        mm2_gs_set_u16(gs.a4(), MM2_GS_SCRIPT_COUNTER, static_cast<uint16_t>(cur + add));
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 1));
        break;
    }
    case 0x2D: {
        /* event_op2d_check_member_attr @ 0x16DBA: match each party member's
         * attribute field against a value nibble; set cond=1 if the predicate
         * holds across the party.
         *   arg1 bit7 -> race (+0xE), bit6 -> sex (+0xC), neither -> class (+0xF);
         *   bit5 -> "any member matches" mode, else "all members match" mode;
         *   low nibble of arg1 = primary match value. If arg1 has no high bits
         *   (& 0xE0 == 0), arg2's low nibble is a 2nd accepted value (field may
         *   equal val1 OR val2). The loop breaks at the first member that fails
         *   the desired predicate; cond reflects the last-examined member. */
        const uint8_t arg1 = readU8(gs);
        const uint8_t arg2 = readU8(gs);
        const bool useRace = (arg1 & 0x80) != 0;
        const bool useSex = (arg1 & 0x40) != 0;
        const bool useClass = !useRace && !useSex;
        const bool anyMode = (arg1 & 0x20) != 0;
        const uint8_t val1 = static_cast<uint8_t>(arg1 & 0x0F);
        const uint8_t val2 =
            ((arg1 & 0xE0) == 0) ? static_cast<uint8_t>(arg2 & 0x0F) : val1;

        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 0);
        bool match = false;
        if (launch_ && roster_) {
            for (int i = 0; i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                const Mm2RosterRecord *rec = &roster_->records[launch_->roster_slots[i]];
                /* fields evaluated in ASM order (class, sex, race); each active
                 * field overwrites `match`, so the last active field wins. */
                match = false;
                if (useClass) {
                    match = (rec->class_id == val1);
                }
                if (useSex) {
                    match = (rec->sex == val1);
                }
                if (useRace) {
                    match = (rec->race == val1);
                }
                if (!match) {
                    if (useClass) {
                        match = (rec->class_id == val2);
                    }
                    if (useSex) {
                        match = (rec->sex == val2);
                    }
                    if (useRace) {
                        match = (rec->race == val2);
                    }
                }
                /* break at first member that violates the predicate:
                 * all-mode breaks on a non-match, any-mode breaks on a match. */
                if (anyMode ? match : !match) {
                    break;
                }
            }
        }
        if (match) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 1);
        }
        break;
    }
    case 0x2E: {
        /* event_op2e_set_attr_bit @ 0x16F50: OR arg2 into a per-member byte, but
         * only for members of two specific classes.
         *   class pair = {4, 2}; if arg1 >= 0x80 -> pair {3, 1} and arg1 &= 0x7F.
         *   target byte = member + (uint8)(arg1 - 0x6E) + 0x51; *target |= arg2.
         * (arg1 ~ 0x6E selects field +0x51, the class-quest bit region near the
         * +0x50 title nibble read by OP_32.) */
        uint8_t arg1 = readU8(gs);
        const uint8_t arg2 = readU8(gs);
        uint8_t cls_a = 4;
        uint8_t cls_b = 2;
        if (arg1 >= 0x80) {
            cls_a = 3;
            cls_b = 1;
            arg1 = static_cast<uint8_t>(arg1 & 0x7F);
        }
        const int field_off =
            static_cast<int>(static_cast<uint8_t>(arg1 - 0x6E)) + 0x51;
        if (launch_ && roster_ && field_off >= 0 &&
            field_off < static_cast<int>(MM2_ROSTER_RECORD_SIZE)) {
            for (int i = 0; i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                Mm2RosterRecord *rec = &roster_->records[launch_->roster_slots[i]];
                if (rec->class_id == cls_a || rec->class_id == cls_b) {
                    reinterpret_cast<uint8_t *>(rec)[field_off] |=
                        static_cast<uint8_t>(arg2);
                }
            }
        }
        break;
    }
    case 0x2F:
        for (int i = 0; i < 16; ++i) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_INPUT_BUF + i, 0);
        }
        break;
    case 0x30: {
        uint8_t expected[10];
        for (int i = 0; i < 10; ++i) {
            expected[i] = readU8(gs);
        }
        const bool ok =
            eventVmCheckOp30Password(gs.a4() + MM2_GS_INPUT_BUF, expected, sizeof(expected));
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, ok ? 1 : 0);
        break;
    }
    case 0x31: {
        /* event_op31_party_engine_op @ 0x170BC: sets EXIT_FLAGS bit1 (block-text
         * exit) at entry, then reads a member-spec byte + a 16-bit value (read as
         * 2 bytes here — argc=3, token delta 4). It resolves the target member set
         * (0=all party, 9=selected member, 1..8=that member; arg1>=0x80 takes the
         * value from cond_flag) and calls the per-member engine op-pointer -$7F08
         * (a combat/spell field op writing scratch outputs), then -$7F14 which can
         * set the script-abort flag. -$7F08/-$7F14 are runtime A4 pointers (engine,
         * not portable), so only the EXIT_FLAGS side effect is replicated. Partial. */
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 2));
        readU8(gs);
        readU8(gs);
        readU8(gs);
        break;
    }
    case 0x32: {
        /* event_op32 @ 0x17190: cond_flag = party class-nibble count (RAW byte,
         * `move.b d0,-$7951`). The handler calls thunk -$7F2C, which the A4 thunk
         * map resolves to 0x04614 (NOT a variable load): sum over living members
         * of the nibbles of record+0x50 equal to `id` (helper 0x45C4). The prior
         * port read a GS flag via eventVmLoadVar, which was wrong. */
        const uint8_t id = readU8(gs);
        const int count = eventVmCountPartyNibbleMatches(gs.a4(), roster_, launch_, id);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, static_cast<uint8_t>(count));
        break;
    }
    case 0x2B: {
        /* OP_2B @ 0x16D74: skip N tokens when combat-victory latch set (A4-$77BD). */
        const uint8_t n = readU8(gs);
        if (mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH)) {
            skipTokens(gs, n);
        }
        break;
    }
    case 0x12: {
        uint8_t block[12];
        for (int i = 0; i < 12; ++i) {
            block[i] = readU8(gs);
        }
        eventRunFixedEncounter(gs, text_, wait_, block, 12, false);
        break;
    }
    case 0x13: {
        uint8_t block[10];
        for (int i = 0; i < 10; ++i) {
            block[i] = readU8(gs);
        }
        eventRunFixedEncounter(gs, text_, wait_, block, 10, true);
        break;
    }
    default:
        if (op >= 0x33) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
            endScript(gs);
            break;
        }
        /* GAP: unimplemented op — advance past argc via token table. */
        {
            const int pos = mm2_gs_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS);
            const int delta = tokenDelta(op);
            if (delta > 1) {
                mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, static_cast<uint16_t>(pos + delta - 1));
            }
        }
        break;
    }
}

bool EventRuntime::runVmLoop(GameStateView &gs, world::MapWorld &world)
{
    if (!loc_) {
        return false;
    }

    const int script_end = loc_->string_table_offset;
    while (wait_ == EventVmWait::None && script_active_) {
        const int pos = mm2_gs_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS);
        if (script_end >= 0 && pos >= script_end) {
            endScript(gs);
            break;
        }

        const uint8_t op = readU8(gs);
        if (op == 0xFF) {
            endScript(gs);
            break;
        }
        if (op >= 0x33) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
            endScript(gs);
            break;
        }

        dispatchOp(gs, world, op);
        if (mm2_gs_u8(gs.a4(), MM2_GS_SCRIPT_ABORT)) {
            abortScript(gs);
            break;
        }
        if (wait_ != EventVmWait::None || !script_active_) {
            break;
        }
    }

    return script_active_ || wait_ != EventVmWait::None;
}

bool EventRuntime::scanAndRun(GameStateView &gs, world::MapWorld &world)
{
    if (!loaded_ || !loc_ || !gs.valid()) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 0);
        return false;
    }

    if (mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR) == 0xFFFF) {
        initParsed(gs);
    }

    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_START));
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS, 0);

    const uint8_t party_tile =
        static_cast<uint8_t>(((gs.coordY() & 0x0F) << 4) | (gs.coordX() & 0x0F));
    const uint8_t ctx = contextMask(gs);
    bool fired = false;

    /* Every move/turn re-scan: drop ambient door/sign layers from the previous
     * facing, then re-run the first triplet whose cond matches the current ctx.
     * OP_04/0B are re-applied only when the scanner fires again (ALWAYS, ENTER,
     * or the matching DIR_* bit); stale labels from facing E must not survive N. */
    if (!script_active_ && wait_ == EventVmWait::None) {
        text_.clearPersistentOverlays();
    }

    int pos = 0;
    while (pos + 2 < MM2_GS_EVENT_WORK_SIZE) {
        uint8_t a = work_buf_[pos];
        uint8_t b = work_buf_[pos + 1];
        uint8_t c = work_buf_[pos + 2];
        if (a == 0 && b == 0 && c == 0) {
            break;
        }

        if (a == party_tile && eventCondMatches(c, ctx)) {
            const int script_off = poolSeek(b);
            /* event_handler_pool_seek @ 0x17262: seek to record N, then read its
             * first opcode. The screen era gate (attrib byte $560B == era_low,
             * @ 0x172BC) is applied ONLY when that opcode is NOT OP_22 — records
             * starting with OP_22 range-check the era themselves and must run
             * regardless of the screen gate. Applying the gate to every event
             * (the previous behaviour) wrongly suppressed era/time-travel events. */
            if (script_off >= 0 && script_off < loc_->string_table_offset) {
                const uint8_t first_op = work_buf_[script_off];
                if (first_op == 0x22 || eraGateOpen(gs, world)) {
                    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, static_cast<uint16_t>(script_off));
                    script_active_ = true;
                    wait_ = EventVmWait::None;
                    runVmLoop(gs, world);
                    fired = true;
                    /* ASM @ 0x17698 clears stack temporaries only — work_buf triplets
                     * stay intact so ALWAYS/ENTER events re-fire on next latch. */
                    break;
                }
            }
        }
        pos += 3;
    }

    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 0);
    return fired;
}

bool EventRuntime::continueInput(GameStateView &gs, world::MapWorld &world, const platform::KeyState &keys)
{
    if (!script_active_ && wait_ == EventVmWait::None) {
        return false;
    }

    if (wait_ == EventVmWait::Space) {
        if (!keys.space && !keys.enter && !keys.any_key) {
            return true;
        }
        text_.clearSpacePrompt();
        wait_ = EventVmWait::None;
        if (script_active_) {
            runVmLoop(gs, world);
        }
        return script_active_ || wait_ != EventVmWait::None;
    }

    if (wait_ == EventVmWait::YesNo) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch != 'Y' && ch != 'N') {
            return true;
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, ch == 'Y' ? 1 : 0);
        if (hasPendingPortal()) {
            if (finishPendingPortal(gs, world, ch == 'Y')) {
                return script_active_ || wait_ != EventVmWait::None;
            }
        }
        wait_ = EventVmWait::None;
        if (script_active_) {
            runVmLoop(gs, world);
        }
        return script_active_ || wait_ != EventVmWait::None;
    }

    if (wait_ == EventVmWait::MemberSelect) {
        const char ch = keys.last_ascii;
        if (ch == 27) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
            abortScript(gs);
            return false;
        }
        if (ch >= '1' && ch <= '8') {
            eventVmSetSelectedPartySlot(gs.a4(), ch - '0');
            mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 1);
            wait_ = EventVmWait::None;
            if (script_active_) {
                runVmLoop(gs, world);
            }
            return script_active_ || wait_ != EventVmWait::None;
        }
        return true;
    }

    return false;
}

}  // namespace mm2::events
