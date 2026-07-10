#include "mm2/events/EventRuntime.h"

#include "mm2/CppStdCompat.h"
#include "mm2/DataPath.h"
#include "mm2/events/EventCombatEncounter.h"
#include "mm2/events/EventPartyEffects.h"
#include "mm2/events/EventSkillBuy.h"
#include "mm2/events/EventTownServices.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/ServiceSignResolver.h"
#include "mm2/events/TownServiceTransactions.h"
#include "mm2/runtime/PathScratch.h"

#include "mm2_attrib_codec.h"
#include "mm2_found_items.h"
#include "mm2_map_codec.h"
#include "mm2_party_launch.h"

namespace mm2::events {

namespace {

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
    return eventVmTokenDelta(tok);
}

void applyPartyProgressOp(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          uint8_t count, uint8_t op, uint8_t val, bool masked, uint8_t and_m,
                          uint8_t or_m)
{
    (void)eventVmApplyPartyByteOp(gs.a4(), roster, launch, count, op, val, masked, and_m, or_m);
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
    pending_town_menu_ = PendingTownMenu::None;
    pending_inn_goto_town_ = false;
    pending_skill_buy_member_ = false;
    pending_general_store_member_ = false;
    pending_circus_attr_ = false;
    pending_skill_id_ = 0;
    pending_skill_cost_ = 0;
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
    mm2_gs_set_u8(gs.a4(), MM2_GS_QUEUED_EVENT_ID, 0xFF);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);
    initContextMaskTable(gs.a4());
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERA_LOW, static_cast<uint8_t>(gs.era() & 0xFF));
    initParsed(gs);

    script_active_ = false;
    wait_ = EventVmWait::None;
    inline_script_end_ = -1;
    saved_location_id_ = -1;
    saved_loc_ = nullptr;
    service_title_[0] = '\0';
    text_.reset();
    pending_town_menu_ = PendingTownMenu::None;
    pending_inn_goto_town_ = false;
    pending_skill_buy_member_ = false;
    pending_general_store_member_ = false;
    pending_circus_attr_ = false;
    pending_skill_id_ = 0;
    pending_skill_cost_ = 0;
    return true;
}

void EventRuntime::initParsed(GameStateView &gs)
{
    /* event_system_init @ 0x1754A: walk triplets until 00 00 00.
     * Castle blobs (locs 63/65/68) have no terminator — leave anchor $FFFF
     * so the scanner skips normal init and uses queued dispatch only. */
    if (loc_ && loc_->kind == MM2_EVENT_KIND_CASTLE_BLOB) {
        string_anchor_ = 0xFFFF;
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR, 0xFFFF);
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_START, 0);
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, 0);
        return;
    }

    int pos = 0;
    bool found_term = false;
    while (pos + 2 < MM2_GS_EVENT_WORK_SIZE) {
        const uint8_t a = work_buf_[pos];
        const uint8_t b = work_buf_[pos + 1];
        const uint8_t c = work_buf_[pos + 2];
        pos += 3;
        if (a == 0 && b == 0 && c == 0) {
            found_term = true;
            break;
        }
    }

    if (!found_term) {
        string_anchor_ = 0xFFFF;
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR, 0xFFFF);
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_START, 0);
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, 0);
        return;
    }

    /* After terminator: LE u16 string-relative offset; ASM adds it to the
     * terminator index to form the string-table anchor (-$7954). */
    uint16_t anchor = static_cast<uint16_t>(pos);
    if (pos + 1 < MM2_GS_EVENT_WORK_SIZE) {
        const uint16_t str_rel = static_cast<uint16_t>(work_buf_[pos] | (work_buf_[pos + 1] << 8));
        anchor = static_cast<uint16_t>(pos + str_rel);
    }

    /* script_start = terminator_index + 5 in ASM (pos already past the three
     * zero bytes, then +2 for the LE word → pos+2 == terminator+5). */
    const uint16_t script_start = static_cast<uint16_t>(pos + 2);
    string_anchor_ = anchor;
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

int EventRuntime::poolSeekIn(const Mm2EventLocation *loc, uint8_t event_id) const
{
    if (!loc || loc->script_offset < 0 || loc->string_table_offset <= loc->script_offset) {
        return -1;
    }

    const int start = loc->script_offset;
    const int end = loc->string_table_offset;
    int record = 0;
    int seg_start = start;
    for (int i = start; i < end; ++i) {
        if (loc->raw[i] == 0xFF) {
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

int EventRuntime::poolSeek(uint8_t event_id) const
{
    return poolSeekIn(loc_, event_id);
}

int EventRuntime::poolSeekWorkBuf(int start_pos, uint8_t event_id) const
{
    /* event_handler_pool_seek @ 0x17262: from parse_pos, skip event_id
     * FF-delimited records (count 0..id-1), then return the next byte offset. */
    if (start_pos < 0 || start_pos >= MM2_GS_EVENT_WORK_SIZE) {
        return -1;
    }
    int pos = start_pos;
    uint8_t count = 0;
    while (count < event_id) {
        if (pos >= MM2_GS_EVENT_WORK_SIZE) {
            return -1;
        }
        while (pos < MM2_GS_EVENT_WORK_SIZE && work_buf_[pos] != 0xFF) {
            ++pos;
        }
        if (pos >= MM2_GS_EVENT_WORK_SIZE) {
            return -1;
        }
        ++pos; /* consume FF */
        ++count;
    }
    if (pos >= MM2_GS_EVENT_WORK_SIZE) {
        return -1;
    }
    return pos;
}

const char *EventRuntime::resolveString(int idx, char *buf, size_t buf_cap) const
{
    if (buf_cap == 0) {
        return buf;
    }

    buf[0] = '\0';
    if (idx < 0) {
        return buf;
    }

    /* ASM event_text_resolve_u8 @ 0x15884 walks from A4-$7954 (runtime
     * string anchor), not the codec's load-time string_table_offset. Queued
     * overlays rebuild that anchor from work_buf[0..1] (LE) — using the codec
     * offset here mis-indexes loc-60/61 string banks (Corak vs goblet). */
    size_t pos = string_anchor_;
    const size_t limit = MM2_GS_EVENT_WORK_SIZE;
    if (pos >= limit) {
        if (loc_ && loc_->string_table_offset >= 0 &&
            static_cast<size_t>(loc_->string_table_offset) < loc_->raw_len) {
            pos = static_cast<size_t>(loc_->string_table_offset);
        } else {
            return buf;
        }
    }

    int cur = 0;
    while (pos < limit) {
        size_t end = pos;
        while (end < limit && work_buf_[end] != 0xFF) {
            ++end;
        }
        if (cur == idx) {
            size_t j = 0;
            for (size_t i = pos; i < end && j + 1 < buf_cap; ++i) {
                buf[j++] = static_cast<char>(work_buf_[i]);
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

void EventRuntime::remapOp0cDest(uint8_t &dest_screen, uint8_t &dest_tile)
{
    /* event_op0c_map_transition @ 0x15E12:
     *   if dest bit6 set: dest = rng(1,20)+5; if dest>=0x11 then dest+=0x10; set bit7
     *   if dest >= 0x80:  dest_tile = rng(1,255)
     *   dest &= 0x3F before map load
     * rng via A4-$7BB4 (same contract as gameplay::Rng). */
    if ((dest_screen & 0x40) != 0) {
        int roll = 1;
        if (rng_) {
            roll = rng_->range(1, 20);
        }
        uint8_t d = static_cast<uint8_t>(roll + 5);
        if (d >= 0x11) {
            d = static_cast<uint8_t>(d + 0x10);
        }
        d = static_cast<uint8_t>(d | 0x80);
        dest_screen = d;
    }
    if (dest_screen >= 0x80) {
        int tile_roll = 1;
        if (rng_) {
            tile_roll = rng_->range(1, 255);
        }
        dest_tile = static_cast<uint8_t>(tile_roll);
    }
    dest_screen = static_cast<uint8_t>(dest_screen & 0x3F);
}

void EventRuntime::applyMapTransition(GameStateView &gs, world::MapWorld &world, uint8_t dest_screen,
                                      uint8_t dest_tile)
{
    remapOp0cDest(dest_screen, dest_tile);
    world.enterScreen(dest_screen);
    gs.setScreenId(dest_screen);
    gs.setCoordX(static_cast<uint8_t>(dest_tile & 0x0F));
    gs.setCoordY(static_cast<uint8_t>((dest_tile >> 4) & 0x0F));
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERA_LOW, static_cast<uint8_t>(gs.era() & 0xFF));
    ServiceSignResolver::syncSignEnvId(gs.a4(), static_cast<int>(dest_screen), &world.attrib());
    enterLocation(static_cast<int>(dest_screen), gs, world);
    /* ASM @ 0x15EB6: end script then set pending_event_latch so the new tile
     * is scanned on the next scheduler tick. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    screen_changed_ = true;
}

bool EventRuntime::finishPendingTownMenu(GameStateView &gs, bool accepted)
{
    if (pending_town_menu_ == PendingTownMenu::None) {
        return false;
    }
    const PendingTownMenu kind = pending_town_menu_;
    pending_town_menu_ = PendingTownMenu::None;

    if (!accepted) {
        return false;
    }

    if (kind == PendingTownMenu::Inn) {
        pending_inn_goto_town_ = true;
        return true;
    }

    if (kind == PendingTownMenu::SkillBuy) {
        text_.showOp02("Who will learn this skill (1-8)?", 19);
        wait_ = EventVmWait::MemberSelect;
        pending_skill_buy_member_ = true;
        return true;
    }

    if (kind == PendingTownMenu::GeneralStore) {
        text_.showOp02("Who will convert skills (1-8)?", 19);
        wait_ = EventVmWait::MemberSelect;
        pending_general_store_member_ = true;
        return true;
    }

    if (kind == PendingTownMenu::Circus) {
        /* 0xDF04: after Y, pick attribute 1..6 (then win leaf if +$7D bit1). */
        text_.showOp02(
            "1) Might  2) Int  3) Personality\n"
            "4) Speed  5) Accuracy  6) Luck",
            19);
        wait_ = EventVmWait::MemberSelect; /* reuse 1-8 digit entry; only 1-6 valid */
        pending_circus_attr_ = true;
        return true;
    }

    (void)eventTownServiceRunBoundMenu(*this, gs, roster_, launch_, items_, location_id_, kind);
    return true;
}

void EventRuntime::restoreOverlayIfIdle(GameStateView &gs)
{
    if (saved_loc_ == nullptr) {
        return;
    }
    if (wait_ != EventVmWait::None || script_active_) {
        return;
    }
    /* OP_0E default-range leaves QUEUED_EVENT_ID set for the scanner epilogue;
     * do not restore the home work_buf until that queued script has run. */
    if (mm2_gs_u8(gs.a4(), MM2_GS_QUEUED_EVENT_ID) != 0xFF) {
        return;
    }
    if (combat_ && combat_->active()) {
        return;
    }
    location_id_ = saved_location_id_;
    loc_ = saved_loc_;
    ::memcpy(work_buf_, saved_work_buf_, sizeof(work_buf_));
    ::memcpy(gs.a4() + MM2_GS_EVENT_WORK_BUF, work_buf_, MM2_GS_EVENT_WORK_SIZE);
    saved_location_id_ = -1;
    saved_loc_ = nullptr;
    initParsed(gs);
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
    inline_script_end_ = -1;
    restoreOverlayIfIdle(gs);
}

void EventRuntime::abortScript(GameStateView &gs)
{
    /* VM loop @ 0x17540: abort set → skip $171AC cleanup; OP_02 message stays visible. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);
    script_active_ = false;
    wait_ = EventVmWait::None;
    inline_script_end_ = -1;
    restoreOverlayIfIdle(gs);
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
        /* OP_04 @ 0x159F4: resolve string, then skip draw if -$79E1 != 0. */
        const uint8_t idx = readU8(gs);
        const char *s = resolveString(idx, text_buf, sizeof(text_buf));
        if (mm2_gs_u8(gs.a4(), MM2_GS_CANT_SEE_FLAG) == 0) {
            text_.showOp04(s);
        }
        break;
    }
    case 0x05: {
        /* OP_05 @ 0x15A46: same can't-see gate @ 0x15A52. */
        const uint8_t idx = readU8(gs);
        const char *s = resolveString(idx, text_buf, sizeof(text_buf));
        if (mm2_gs_u8(gs.a4(), MM2_GS_CANT_SEE_FLAG) == 0) {
            text_.showOp05(s);
        }
        break;
    }
    case 0x06: {
        /* OP_06 @ 0x15AEE: '-'→'{' rewrite then can't-see gate @ 0x15B24. */
        const uint8_t idx = readU8(gs);
        const char *s = resolveString(idx, text_buf, sizeof(text_buf));
        if (mm2_gs_u8(gs.a4(), MM2_GS_CANT_SEE_FLAG) == 0) {
            text_.showOp06(s);
        }
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
        /* OP_09 @ 0x15D3C: clears cond, polls Y/N — draws nothing (doc 44 §3.7). */
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
        /* event_op0c_map_transition @ 0x15E12 — bit6/high remaps inside
         * applyMapTransition; then OP_0F cleanup via endScript. */
        const uint8_t dest_screen = readU8(gs);
        const uint8_t dest_tile = readU8(gs);
        applyMapTransition(gs, world, dest_screen, dest_tile);
        endScript(gs);
        break;
    }
    case 0x0E: {
        /* event_op0e_selector_dispatch @ 0x160C2: SCRIPT_ABORT=1 at entry so
         * the current script ends after the selector returns; default-range
         * bins set QUEUED_EVENT_ID for the scanner epilogue @ 0x176B6. */
        mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
        const uint8_t sel = readU8(gs);
        eventExecTownSelector(*this, gs, world, sel, roster_, launch_, items_, text_, wait_,
                              location_id_, service_title_, rng_);
        break;
    }
    case 0x0B: {
        const uint8_t str_idx = readU8(gs);
        const uint8_t placement = readU8(gs);
        /* OP_0B @ 0x15DB0 / 0x15756: str_idx is a SIGN/PORTRAIT lookup key, NOT a
         * str.dat text index. Direct trace of 0x15DB0 (see EXTRACTED docs
         * 07/28 §OP_0B correction, 2026-07): the routine resolves str_idx via
         * the per-env table @ 0x15756 into an .anm id, draws the window frame
         * + portrait bitmap via thunks -$7FBC/-$7FC2, and sets exit-flag bit 2
         * (below) — there is NO jsr to any text/string routine anywhere in
         * this handler. The previous port fabricated a "service title" by
         * ALSO feeding str_idx into resolveString(); because str_idx is a
         * sign-table key that happens to collide with unrelated str.dat
         * indices at some locations, this produced byte-for-byte wrong text
         * cross-wired from a DIFFERENT string in the SAME location (e.g.
         * Middlegate idx 0x14 == str[20] "Fool, you have no farthing to
         * flick!" bleeding into the enroll-mages-guild AND goblet-quest
         * doorways just because both happen to reuse Nordon's sign id 0x14;
         * Middlegate Inn idx 0x03 == str[3] "Slaughtered Lamb"). Fixed by
         * removing the text capture entirely — service_title_ is no longer
         * written here, so callers correctly fall back to their own honest
         * placeholder (town name) instead of fabricated/cross-wired text.
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
    case 0x1C: {
        /* event_op1c_engine_query @ 0x16742: push arg, push #1, jsr -$7BB4
         * (rng_roll @ 0x22BC6). Stores the RAW roll byte into cond_flag
         * (`move.b d0,-$7951`) — not a boolean. Unbound rng → 1 (same as
         * OP_0C fallback when rng_ is null). */
        const uint8_t hi = readU8(gs);
        const int roll = rng_ ? rng_->range(1, static_cast<int>(hi)) : 1;
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, static_cast<uint8_t>(roll & 0xFF));
        break;
    }
    case 0x1D:
        /* event_op1d_engine_indexed @ 0x16762: -$7E84 → audio_wait_helper @ 0x6798
         * with index (arg1*7+1). Halves the wait count, polls -$7BD2, yields via
         * -$7B42. Presentation/audio only — no cond/GS write. Consume argc=1. */
        readU8(gs);
        break;
    case 0x1E:
        /* event_op1e_timed_wait @ 0x16780: busy-wait `arg1` iterations, each
         * delay(10) via -$7BC0→0x22B4A then poll -$7BD2→0x22586, break on key.
         * Presentation/timing only — no game-state effect. Headless no-op. */
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
        /* 0x16A34: bset #$2, EXIT_FLAGS (map redraw). */
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 4));
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
        /* event_op24_gold_check @ 0x16B54: u16 via 0x155DA, then -$7E6C → 0x6ACE
         * (pool+deduct). Cond = success. */
        const uint8_t lo = readU8(gs);
        const uint8_t hi = readU8(gs);
        const uint32_t need = static_cast<uint32_t>(lo | (hi << 8));
        const bool ok = eventVmPartyTryPayGold(gs.a4(), roster_, launch_, need);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, ok ? 1 : 0);
        break;
    }
    case 0x25: {
        /* event_op25_code_check @ 0x16B82: hi,lo → u16, then -$7E66 → 0x6B9A
         * gems pool+deduct (NOT tickets/keys — those are OP_0E 0x08 / OP_28). */
        const uint8_t hi = readU8(gs);
        const uint8_t lo = readU8(gs);
        const uint16_t need = static_cast<uint16_t>((hi << 8) | lo);
        const bool ok = eventVmPartyTryPayGems(gs.a4(), roster_, launch_, need);
        mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, ok ? 1 : 0);
        break;
    }
    case 0x26:
        /* OP_26 @ 0x16BC0 flag≠0: key wait only (thunks -$7D0A/-$7BD2). No ROM
         * prompt string here — preceding OP_01/02 already drew the question.
         * Success path @ 0x16C70 writes slot → cond / -$5D42 / -$5D3F. */
        wait_ = EventVmWait::MemberSelect;
        break;
    case 0x27:
        /* OP_27 @ 0x16BC0 flag=0: same leaf, input via -$7DDC. */
        wait_ = EventVmWait::MemberSelect;
        break;
    case 0x28: {
        /* OP_28 @ 0x16C86: discard 1st arg, item id = 2nd; backpack-only consume. */
        (void)readU8(gs);
        const uint8_t item_id = readU8(gs);
        const bool has = eventVmPartyConsumeBackpackItem(roster_, launch_, item_id);
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
        /* event_op2f @ 0x16FEA: NOT a silent clear — calls -$7F92 which reads
         * up to 10 characters into A4-$5C50, then space-pads the remainder and
         * clears the trailing NUL at -$5C46. Port: arm Answer wait; continueInput
         * fills the buffer then resumes so OP_30 can compare. */
        answer_len_ = 0;
        ::memset(answer_buf_, 0, sizeof(answer_buf_));
        for (int i = 0; i < 16; ++i) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_INPUT_BUF + i, 0);
        }
        text_.showOp02("?", 19);
        text_.setTextEntry(answer_buf_, answer_len_);
        wait_ = EventVmWait::Answer;
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
        /* event_op31_iterate_targets @ 0x170BC:
         *   EXIT_FLAGS |= bit1
         *   member-spec + u16 value (arg1>=0x80 → value from cond_flag)
         *   per resolved member: -$7F08 → 0x4952 (out-flags zeroed at call site)
         *   then -$7F14 → 0x47EC: living-party abort → SCRIPT_ABORT */
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 2));
        const uint8_t member_spec = readU8(gs);
        const uint8_t lo = readU8(gs);
        const uint8_t hi = readU8(gs);
        const uint16_t value = static_cast<uint16_t>(lo | (hi << 8));
        eventVmOp31IterateDamage(gs.a4(), roster_, launch_, member_spec, value);
        if (eventVmCountLivingPartyMembers(gs.a4(), roster_, launch_) == 0) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
        }
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
        eventRunFixedEncounter(gs, text_, wait_, block, 12, false, combat_, &world);
        break;
    }
    case 0x13: {
        uint8_t block[10];
        for (int i = 0; i < 10; ++i) {
            block[i] = readU8(gs);
        }
        eventRunFixedEncounter(gs, text_, wait_, block, 10, true, combat_, &world);
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

    const int script_end =
        inline_script_end_ >= 0 ? inline_script_end_ : loc_->string_table_offset;
    while (wait_ == EventVmWait::None && script_active_) {
        /* OP_0E sets SCRIPT_ABORT at entry; after an async wait resumes, end
         * without fetching further opcodes from the same script (ASM fetch
         * loop exits on abort). */
        if (mm2_gs_u8(gs.a4(), MM2_GS_SCRIPT_ABORT)) {
            abortScript(gs);
            break;
        }

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
        /* Waits (Y/N, Answer, SPACE) must win over SCRIPT_ABORT. OP_0E @ 0x160C2
         * sets abort at entry so the script ends after the selector returns; in
         * the remake selectors are async, so abort is deferred until the wait
         * completes and runVmLoop resumes. */
        if (wait_ != EventVmWait::None || !script_active_) {
            break;
        }
        if (mm2_gs_u8(gs.a4(), MM2_GS_SCRIPT_ABORT)) {
            abortScript(gs);
            break;
        }
    }

    /* ASM scanner epilogue @ 0x176B6: after the current script ends, run any
     * OP_0E default-range queue (Hero / skill tiles that OP_0E mid-script). */
    if (wait_ == EventVmWait::None && mm2_gs_u8(gs.a4(), MM2_GS_QUEUED_EVENT_ID) != 0xFF) {
        (void)runQueuedDispatch(gs, world);
    }

    return script_active_ || wait_ != EventVmWait::None;
}

bool EventRuntime::runDefaultRangeOverlay(GameStateView &gs, world::MapWorld &world,
                                          uint8_t category, uint8_t index)
{
    /* OP_0E default-range @ 0x15EDC / 0x160A2:
     *   save screen_mode_id; screen_mode_id = category;
     *   -$7DFA event_dat_loader (overlay → work_buf);
     *   restore screen_mode_id; queued_event_id = index; rts
     * The scanner epilogue @ 0x176B6 then pool_seeks and runs the queued id.
     * Do NOT run the overlay VM here — defer to runQueuedDispatch.
     * Do NOT pre-validate via codec string slots: ASM always loads the overlay
     * and queues the index; pool_seek from parse_pos=2 decides what runs. */
    (void)world;
    if (!loaded_ || !gs.valid() || category >= MM2_EVENT_LOCATION_COUNT) {
        return false;
    }

    const Mm2EventLocation *overlay = &file_.locations[category];
    if (!overlay->raw || overlay->raw_len == 0) {
        return false;
    }

    if (saved_loc_ == nullptr) {
        saved_location_id_ = location_id_;
        saved_loc_ = loc_;
        ::memcpy(saved_work_buf_, work_buf_, sizeof(saved_work_buf_));
    }

    /* 0x160A2: temporarily write category into -$79F2 while loader runs. */
    const uint8_t saved_screen = gs.screenId();
    gs.setScreenId(category);

    const size_t copy_len =
        overlay->raw_len < MM2_GS_EVENT_WORK_SIZE ? overlay->raw_len : MM2_GS_EVENT_WORK_SIZE;
    ::memcpy(work_buf_, overlay->raw, copy_len);
    if (copy_len < MM2_GS_EVENT_WORK_SIZE) {
        ::memset(work_buf_ + copy_len, 0, MM2_GS_EVENT_WORK_SIZE - copy_len);
    }
    ::memcpy(gs.a4() + MM2_GS_EVENT_WORK_BUF, work_buf_, MM2_GS_EVENT_WORK_SIZE);

    gs.setScreenId(saved_screen);
    location_id_ = category;
    loc_ = overlay;
    mm2_gs_set_u8(gs.a4(), MM2_GS_QUEUED_EVENT_ID, index);
    return true;
}

bool EventRuntime::runQueuedDispatch(GameStateView &gs, world::MapWorld &world)
{
    /* event_queued_dispatch @ 0x176B6 (after triplet scan):
     *   if queued_event_id == $FF → skip
     *   rebuild string anchor from work_buf[0..1] as LE u16
     *     (ASM: d0 = work_buf[1]<<8 | work_buf[0] @ 0x176C2–0x176D2)
     *   parse_pos = 2
     *   pool_seek(queued_id) → interpreter
     *   then -$7DFA (event_dat_loader) + re-init
     *
     * Loc 60 starts FF 00 … so LE anchor = 0x00FF, which points at the Corak
     * text bank. A prior BE read (0xFF00) + codec-string bytecode fallback made
     * selector 0x09 (Corak ghost) run the Nordon goblet quest script instead. */
    const uint8_t qid = mm2_gs_u8(gs.a4(), MM2_GS_QUEUED_EVENT_ID);
    if (qid == 0xFF) {
        return false;
    }

    const uint16_t le_anchor =
        static_cast<uint16_t>((static_cast<uint16_t>(work_buf_[1]) << 8) | work_buf_[0]);
    string_anchor_ = le_anchor;
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR, le_anchor);
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, 2);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);

    const int script_off = poolSeekWorkBuf(2, qid);
    mm2_gs_set_u8(gs.a4(), MM2_GS_QUEUED_EVENT_ID, 0xFF);

    if (script_off < 0) {
        inline_script_end_ = -1;
        if (saved_loc_ != nullptr) {
            location_id_ = saved_location_id_;
            loc_ = saved_loc_;
            ::memcpy(work_buf_, saved_work_buf_, sizeof(work_buf_));
            ::memcpy(gs.a4() + MM2_GS_EVENT_WORK_BUF, work_buf_, MM2_GS_EVENT_WORK_SIZE);
            saved_location_id_ = -1;
            saved_loc_ = nullptr;
            initParsed(gs);
        }
        return false;
    }

    /* Bound the inline segment at the next FF (ASM stops on 0xFF fetch). */
    int seg_end = script_off;
    while (seg_end < MM2_GS_EVENT_WORK_SIZE && work_buf_[seg_end] != 0xFF) {
        ++seg_end;
    }
    inline_script_end_ = seg_end;

    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, static_cast<uint16_t>(script_off));
    script_active_ = true;
    wait_ = EventVmWait::None;
    runVmLoop(gs, world);
    /* Keep inline_script_end_ while waiting (Y/N / SPACE) so continueInput
     * resumes inside the same FF-delimited overlay segment. Clearing it early
     * made resume use loc_->string_table_offset (wrong for loc-60 string banks
     * whose codec offset is poisoned by embedded 00 00 00 in pool bytecode). */
    if (wait_ == EventVmWait::None) {
        inline_script_end_ = -1;
    }

    /* ASM @ 0x176EA: reload home location via event_dat_loader + init.
     * Overlay swap is restored when the overlay script goes idle. */
    if (saved_loc_ == nullptr && loc_ && loc_->kind != MM2_EVENT_KIND_CASTLE_BLOB) {
        initParsed(gs);
    } else {
        restoreOverlayIfIdle(gs);
    }
    return true;
}

bool EventRuntime::scanAndRun(GameStateView &gs, world::MapWorld &world)
{
    if (!loaded_ || !loc_ || !gs.valid()) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 0);
        return false;
    }

    /* ASM @ 0x175EE–0x175F2: clear saved_cond and reset queued id to $FF at
     * scanner entry. Queued id set during a prior OP_0E default-range path must
     * survive until *after* that script's scanner epilogue — so we only clear
     * here when not mid-overlay. runDefaultRangeOverlay sets the queue and
     * runs the VM itself; the post-scan path below handles deferred queues. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_SAVED_COND_FLAG, 0);

    if (mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR) == 0xFFFF) {
        if (loc_->kind == MM2_EVENT_KIND_CASTLE_BLOB) {
            /* No triplet table — fall through to queued path only. */
        } else {
            initParsed(gs);
        }
    }

    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_START));
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS, 0);

    const uint8_t party_tile =
        static_cast<uint8_t>(((gs.coordY() & 0x0F) << 4) | (gs.coordX() & 0x0F));
    const uint8_t ctx = contextMask(gs);
    bool fired = false;
    bool matched_tile = false;

    if (!script_active_ && wait_ == EventVmWait::None) {
        text_.clearPersistentOverlays();
    }

    /* Castle blobs have no 00 00 00 terminator — skip the triplet walk. */
    if (loc_->kind != MM2_EVENT_KIND_CASTLE_BLOB &&
        mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR) != 0xFFFF) {
        int pos = 0;
        while (pos + 2 < MM2_GS_EVENT_WORK_SIZE) {
            uint8_t a = work_buf_[pos];
            uint8_t b = work_buf_[pos + 1];
            uint8_t c = work_buf_[pos + 2];
            if (a == 0 && b == 0 && c == 0) {
                break;
            }

            if (a == party_tile) {
                matched_tile = true;
                if (eventCondMatches(c, ctx)) {
                    const int script_off = poolSeek(b);
                    if (script_off >= 0 && script_off < loc_->string_table_offset) {
                        const uint8_t first_op = work_buf_[script_off];
                        if (first_op == 0x22 || eraGateOpen(gs, world)) {
                            mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS,
                                           static_cast<uint16_t>(script_off));
                            script_active_ = true;
                            wait_ = EventVmWait::None;
                            runVmLoop(gs, world);
                            fired = true;
                            break;
                        }
                    }
                }
            }
            pos += 3;
        }
    }

    /* Queued dispatch @ 0x176B6 — after triplet loop, before ambient combat.
     * Only when the triplet script is fully done (no wait); queue is set during
     * the same scan's OP_0E default-range path and consumed here (ASM clears
     * queue at the *next* scanner entry). */
    if (wait_ == EventVmWait::None && runQueuedDispatch(gs, world)) {
        fired = true;
    }

    /* event_tile_scanner @ 0x176F2: no triplet matched on a collision-flagged
     * tile → random-picker combat (-$7EDE), then clear the map event bit. */
    if (!fired && !matched_tile) {
        const int x = static_cast<int>(gs.coordX());
        const int y = static_cast<int>(gs.coordY());
        if (x >= 0 && y >= 0 && x < MM2_MAP_GRID_DIM && y < MM2_MAP_GRID_DIM &&
            mm2_map_collision_has_event(world.collisionAt(x, y))) {
            eventRunTileAmbientEncounter(gs, combat_, &world);
            if (combat_ && combat_->active()) {
                eventVmConsumeTileEncounterFlag(gs.a4(), world, y, x);
                fired = true;
            } else {
                mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 0);
            }
        }
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
        if (pending_slide_trap_halve_) {
            pending_slide_trap_halve_ = false;
            /* 0xD75C..0xD87C: halve base stats / sp_max / level / spell_level. */
            if (roster_ && launch_) {
                for (int i = 0; i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                    const int ridx = launch_->roster_slots[i];
                    if (ridx < 0 || ridx >= MM2_ROSTER_RECORD_COUNT) {
                        continue;
                    }
                    auto *raw = reinterpret_cast<uint8_t *>(&roster_->records[ridx]);
                    raw[0x6A] = static_cast<uint8_t>(raw[0x6A] / 2);
                    raw[0x6B] = static_cast<uint8_t>(raw[0x6B] / 2);
                    raw[0x6C] = static_cast<uint8_t>(raw[0x6C] / 2);
                    raw[0x6D] = static_cast<uint8_t>(raw[0x6D] / 2);
                    raw[0x6E] = static_cast<uint8_t>(raw[0x6E] / 2);
                    raw[0x6F] = static_cast<uint8_t>(raw[0x6F] / 2);
                    raw[0x70] = static_cast<uint8_t>(raw[0x70] / 2);
                    raw[0x71] = static_cast<uint8_t>(raw[0x71] / 2);
                    raw[0x72] = static_cast<uint8_t>(raw[0x72] / 2);
                    raw[0x73] = static_cast<uint8_t>(raw[0x73] / 2);
                    const uint16_t sp = static_cast<uint16_t>(raw[0x58] | (raw[0x59] << 8));
                    const uint16_t sp2 = static_cast<uint16_t>(sp / 2);
                    raw[0x58] = static_cast<uint8_t>(sp2 & 0xFF);
                    raw[0x59] = static_cast<uint8_t>((sp2 >> 8) & 0xFF);
                }
            }
        }
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
        if (hasPendingTownMenu()) {
            finishPendingTownMenu(gs, ch == 'Y');
        } else {
            wait_ = EventVmWait::None;
        }
        /* finishPendingTownMenu may arm MemberSelect or Space; only drop
         * the consumed YesNo wait. */
        if (wait_ == EventVmWait::YesNo) {
            wait_ = EventVmWait::None;
        }
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
            const int slot = ch - '0';
            const int party_n = launch_ ? launch_->party_count : 8;
            if (slot < 1 || slot > party_n) {
                return true; /* re-prompt */
            }
            if (roster_ && launch_ && slot <= launch_->party_count) {
                const int ridx = launch_->roster_slots[slot - 1];
                if (ridx >= 0 && ridx < MM2_ROSTER_RECORD_COUNT &&
                    roster_->records[ridx].condition >= 0x81) {
                    return true; /* dead/stoned — re-prompt (0x16C42) */
                }
            }
            /* 0x16C70: cond = slot; -$5D42 = slot; -$5D3F = slot. */
            eventVmSetSelectedPartySlot(gs.a4(), slot);
            mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, static_cast<uint8_t>(slot));
            mm2_gs_set_u8(gs.a4(), MM2_GS_SAVED_COND_FLAG, static_cast<uint8_t>(slot));
            if (pending_skill_buy_member_) {
                pending_skill_buy_member_ = false;
                (void)eventApplySkillBuy(gs, roster_, launch_, text_, wait_, pending_skill_id_,
                                         pending_skill_cost_);
                if (wait_ == EventVmWait::MemberSelect) {
                    wait_ = EventVmWait::None;
                }
                return wait_ != EventVmWait::None;
            }
            if (pending_general_store_member_) {
                pending_general_store_member_ = false;
                wait_ = EventVmWait::None;
                if (roster_ && launch_ && slot >= 1 && slot <= launch_->party_count) {
                    const int ridx = launch_->roster_slots[slot - 1];
                    if (ridx >= 0 && ridx < MM2_ROSTER_RECORD_COUNT) {
                        const TownSvcGeneralStoreResult r =
                            townSvcGeneralStoreConvert(roster_->records[ridx]);
                        text_.showOp02(r.message ? r.message : "Done.", 19);
                        text_.showSpacePrompt();
                        wait_ = EventVmWait::Space;
                        return true;
                    }
                }
                return script_active_ || wait_ != EventVmWait::None;
            }
            if (pending_circus_attr_) {
                pending_circus_attr_ = false;
                wait_ = EventVmWait::None;
                if (slot < 1 || slot > 6) {
                    return true; /* re-prompt — only 1..6 */
                }
                const int attr = slot - 1;
                bool any_cupie = false;
                if (roster_ && launch_) {
                    for (int i = 0; i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                        const int ridx = launch_->roster_slots[i];
                        if (ridx < 0 || ridx >= MM2_ROSTER_RECORD_COUNT) {
                            continue;
                        }
                        auto *raw = reinterpret_cast<uint8_t *>(&roster_->records[ridx]);
                        if (raw[0x7D] & 0x02) {
                            any_cupie = true;
                            townSvcCircusWinBoost(roster_->records[ridx], attr);
                        }
                    }
                }
                if (any_cupie) {
                    text_.showOp02("You win a prize!", 19);
                } else {
                    const bool doll =
                        townSvcCircusGiveCupieDoll(roster_, launch_, rng_);
                    text_.showOp02(doll ? "You receive a Cupie Doll!" : "Sorry, you lose.", 19);
                }
                text_.showSpacePrompt();
                wait_ = EventVmWait::Space;
                return true;
            }
            wait_ = EventVmWait::None;
            if (script_active_) {
                runVmLoop(gs, world);
            }
            return script_active_ || wait_ != EventVmWait::None;
        }
        return true;
    }

    if (wait_ == EventVmWait::Answer) {
        /* OP_2F @ 0x16FEA → -$7F92: collect up to 10 chars, Enter commits,
         * space-pad remainder, clear trailing byte at buf+10 (-$5C46). */
        if (keys.escape) {
            answer_len_ = 0;
            ::memset(answer_buf_, 0, sizeof(answer_buf_));
            for (int i = 0; i < 11; ++i) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_INPUT_BUF + i, i < 10 ? ' ' : 0);
            }
            text_.clearTextEntry();
            wait_ = EventVmWait::None;
            if (script_active_) {
                runVmLoop(gs, world);
            }
            return script_active_ || wait_ != EventVmWait::None;
        }
        if (keys.backspace) {
            if (answer_len_ > 0) {
                answer_buf_[--answer_len_] = '\0';
                text_.setTextEntry(answer_buf_, answer_len_);
            }
            return true;
        }
        if (keys.enter || keys.space) {
            for (int i = 0; i < 10; ++i) {
                const char c = (i < answer_len_) ? answer_buf_[i] : ' ';
                mm2_gs_set_u8(gs.a4(), MM2_GS_INPUT_BUF + i,
                              static_cast<uint8_t>(std::toupper(static_cast<unsigned char>(c))));
            }
            mm2_gs_set_u8(gs.a4(), MM2_GS_INPUT_BUF + 10, 0);
            text_.clearTextEntry();
            wait_ = EventVmWait::None;
            if (script_active_) {
                runVmLoop(gs, world);
            }
            return script_active_ || wait_ != EventVmWait::None;
        }
        const char ch = keys.last_ascii;
        if (ch >= 32 && ch < 127 && answer_len_ < 10) {
            answer_buf_[answer_len_++] = ch;
            answer_buf_[answer_len_] = '\0';
            text_.setTextEntry(answer_buf_, answer_len_);
        }
        return true;
    }

    if (wait_ == EventVmWait::HexDigit) {
        /* OP_0E 0x7E @ 0xD5D0 / 0xD5FA: -$7F8C digit; reject > $F. */
        int digit = -1;
        const char ch = keys.last_ascii;
        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            digit = 10 + (ch - 'a');
        } else if (ch >= 'A' && ch <= 'F') {
            digit = 10 + (ch - 'A');
        }
        if (digit < 0 || digit > 0xF) {
            return true;
        }
        if (pending_free_teleport_stage_ == 1) {
            pending_free_teleport_x_ = static_cast<uint8_t>(digit);
            pending_free_teleport_stage_ = 2;
            text_.showOp02("What is the magical location:\n       Y ( 0-15 ) ?", 19);
            return true;
        }
        if (pending_free_teleport_stage_ == 2) {
            pending_free_teleport_stage_ = 0;
            gs.setCoordX(pending_free_teleport_x_);
            gs.setCoordY(static_cast<uint8_t>(digit));
            mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
            screen_changed_ = true;
            wait_ = EventVmWait::None;
            if (script_active_) {
                runVmLoop(gs, world);
            }
            return script_active_ || wait_ != EventVmWait::None;
        }
        wait_ = EventVmWait::None;
        if (script_active_) {
            runVmLoop(gs, world);
        }
        return script_active_ || wait_ != EventVmWait::None;
    }

    return false;
}

void EventRuntime::armFreeTeleportUi()
{
    /* 0xD576: four prompt lines; remake hosts X then Y. */
    pending_free_teleport_stage_ = 1;
    pending_free_teleport_x_ = 0;
    text_.showOp02("What is the magical location:\n       X ( 0-15 ) ?", 19);
    wait_ = EventVmWait::HexDigit;
}

}  // namespace mm2::events
