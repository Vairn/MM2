#include "mm2/events/EventRuntime.h"

#include "mm2/CppStdCompat.h"
#include "mm2/events/EventOp.h"
#include "mm2/events/EventVmRegs.h"
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

uint8_t controlsDelayClamped(const GameStateView &gs)
{
    const uint8_t d = mm2_gs_u8(gs.a4(), MM2_GS_DELAY);
    return d > 9 ? 9 : d;
}

/* ASM 0x16780: arg × 0x22B4A(10). 0x22B4A clamps to 20, /20 → dos Delay(1) =
 * 1/50 s, so arg=200 ≈ 4.0 s. Host continueInput is ~60 Hz:
 * polls = (arg * 6 + 2) / 5. Retail OP_1E ignores Controls Delay (-$79AD);
 * the remake scales so Delay 5 = 1.0× (0 snappier, 9 slower). */
uint16_t op1eHostPolls(uint8_t arg, uint8_t delay)
{
    const uint32_t base = (static_cast<uint32_t>(arg) * 6u + 2u) / 5u;
    uint32_t scaled = base * (static_cast<uint32_t>(delay) + 3u) / 8u;
    if (scaled < 1u) {
        scaled = 1u;
    }
    if (scaled > 0xFFFFu) {
        scaled = 0xFFFFu;
    }
    return static_cast<uint16_t>(scaled);
}

/* PORT DEVIATION: Amiga OP_0C is paced by map load + 3D rebuild; the remake
 * hop is instant. Silent cruise tiles (OP_0D 0x09 then OP_0C, no prior wait)
 * dwell so the ocean is visible. (delay+1)*8 frames: Delay 0≈133 ms, 5≈800 ms. */
uint16_t hopDwellPolls(uint8_t delay)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(delay) + 1u) * 8u);
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
    space_wait_armed_ = true;
    space_wait_active_ = false;
    resetDelayState();
    op0d_09_this_script_ = false;
    script_had_wait_ = false;
    screen_changed_ = false;
    service_title_[0] = '\0';
    text_.reset();
    pending_town_menu_ = PendingTownMenu::None;
    pending_inn_goto_town_ = false;
    pending_death_strikes_ = false;
    pending_fd_print_chrome_ = false;
    pending_skill_buy_member_ = false;
    pending_general_store_member_ = false;
    pending_circus_attr_ = false;
    pending_time_machine_ = false;
    pending_quest_encode_stage_ = 0;
    pending_quest_drink_ = false;
    pending_skill_id_ = 0;
    pending_skill_cost_ = 0;
    ::memset(work_buf_, 0, sizeof(work_buf_));
    ::memset(tile_event_resolved_, 0, sizeof(tile_event_resolved_));
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
    clearEventAbort(gs);
    initContextMaskTable(gs.a4());
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERA_LOW, static_cast<uint8_t>(gs.era() & 0xFF));
    initParsed(gs);

    /* refreshWorldAfterEventTransition re-enters the dest location after OP_0C.
     * Keep a silent-hop dwell so the ocean tile stays on screen. */
    const bool keep_hop_dwell =
        wait_ == EventVmWait::Delay && !delay_key_skippable_ && delay_remaining_ > 0;
    const uint16_t saved_delay = delay_remaining_;

    script_active_ = false;
    wait_ = EventVmWait::None;
    space_wait_armed_ = true;
    space_wait_active_ = false;
    resetDelayState();
    op0d_09_this_script_ = false;
    script_had_wait_ = false;
    inline_script_end_ = -1;
    saved_location_id_ = -1;
    saved_loc_ = nullptr;
    service_title_[0] = '\0';
    text_.reset();
    pending_town_menu_ = PendingTownMenu::None;
    pending_inn_goto_town_ = false;
    pending_death_strikes_ = false;
    pending_fd_print_chrome_ = false;
    pending_skill_buy_member_ = false;
    pending_general_store_member_ = false;
    pending_circus_attr_ = false;
    pending_time_machine_ = false;
    pending_quest_encode_stage_ = 0;
    pending_quest_drink_ = false;
    pending_skill_id_ = 0;
    pending_skill_cost_ = 0;
    /* PORT DEVIATION: scripted-tile resolved flags last for this map visit
     * only. Collision bit7 is the ambient-encounter gate, not a triplet latch. */
    ::memset(tile_event_resolved_, 0, sizeof(tile_event_resolved_));
    if (keep_hop_dwell) {
        delay_remaining_ = saved_delay;
        delay_skip_armed_ = false;
        delay_key_skippable_ = false;
        wait_ = EventVmWait::Delay;
    }
    return true;
}

void EventRuntime::markTileEventResolved(int y, int x)
{
    if (y < 0 || x < 0 || y >= MM2_MAP_GRID_DIM || x >= MM2_MAP_GRID_DIM) {
        return;
    }
    tile_event_resolved_[((y & 0x0F) << 4) | (x & 0x0F)] = 1;
}

void EventRuntime::initParsed(GameStateView &gs)
{
    /* event_system_init @ 0x1754A: walk triplets until 00 00 00.
     * Castle blobs (locs 63/65/68) and overlay banks (60..70) are not
     * map-entered via this path — leave anchor $FFFF. Overlay string
     * resolve uses LE@0 from runQueuedDispatch @ 0x176B6 instead. */
    if (loc_ && (loc_->kind == MM2_EVENT_KIND_CASTLE_BLOB ||
                 loc_->kind == MM2_EVENT_KIND_OVERLAY_BANK)) {
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
        if (loc->raw[i] == static_cast<uint8_t>(EventOp::EndRecord)) {
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
        while (pos < MM2_GS_EVENT_WORK_SIZE &&
               work_buf_[pos] != static_cast<uint8_t>(EventOp::EndRecord)) {
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

const char *EventRuntime::readScriptString(GameStateView &gs, char *buf, size_t cap)
{
    return resolveString(readU8(gs), buf, cap);
}

bool EventRuntime::cantSee(const GameStateView &gs) const
{
    return mm2_gs_u8(gs.a4(), MM2_GS_CANT_SEE_FLAG) != 0;
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
    if ((dest_screen & kOp0cRandomScreen) != 0) {
        int roll = 1;
        if (rng_) {
            roll = rng_->range(1, 20);
        }
        uint8_t d = static_cast<uint8_t>(roll + 5);
        if (d >= 0x11) {
            d = static_cast<uint8_t>(d + 0x10);
        }
        d = static_cast<uint8_t>(d | kOp0cRandomTile);
        dest_screen = d;
    }
    if (dest_screen >= kOp0cRandomTile) {
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
        /* Inn/smith/tavern/training No @ 0x1A1F0 / 0x1C7D6 / 0x1D684 / 0x20C2A:
         * jsr -$7D40 ($171AC). */
        bool redraw_status = false;
        bool redraw_roster = false;
        bool redraw_divider = false;
        text_.applyScriptExitCleanup(&redraw_status, &redraw_roster, &redraw_divider);
        (void)redraw_status;
        (void)redraw_roster;
        (void)redraw_divider;
        setEventExit(gs, 0);
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
    /* Do not wait for combat to finish. ROM OP_12 is a synchronous jsr -$7EDE
     * inside the overlay VM; scanner epilogue @ 0x176EA then reloads the home
     * location via -$7DFA before the next tile scan. The port's combat is async,
     * so a combat-active guard left loc_ on the overlay bank: the next scan
     * skipped the map triplet table and every event-flagged tile fell through
     * to ambient -$7EDE ("all events turn into combat"). */
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
    setEventExit(gs, 0);
    clearEventAbort(gs);
    script_active_ = false;
    wait_ = EventVmWait::None;
    space_wait_armed_ = true;
    space_wait_active_ = false;
    resetDelayState();
    op0d_09_this_script_ = false;
    script_had_wait_ = false;
    inline_script_end_ = -1;
    restoreOverlayIfIdle(gs);
}

void EventRuntime::abortScript(GameStateView &gs)
{
    /* VM loop @ 0x17540: abort set → skip $171AC cleanup; OP_02 message stays visible. */
    clearEventAbort(gs);
    script_active_ = false;
    wait_ = EventVmWait::None;
    space_wait_armed_ = true;
    space_wait_active_ = false;
    resetDelayState();
    op0d_09_this_script_ = false;
    script_had_wait_ = false;
    inline_script_end_ = -1;
    restoreOverlayIfIdle(gs);
}

void EventRuntime::resetDelayState()
{
    delay_remaining_ = 0;
    delay_skip_armed_ = false;
    delay_key_skippable_ = false;
}

void EventRuntime::haltUiForHostOverlay(GameStateView &gs)
{
    abortScript(gs);
    text_.clearConsoleMessageLayers();
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 0);
}

void EventRuntime::dispatchOp(GameStateView &gs, world::MapWorld &world, uint8_t op)
{
    switch (static_cast<EventOp>(op)) {
    case EventOp::Invalid:
        opInvalid(gs);
        break;
    case EventOp::Text:
        opText(gs);
        break;
    case EventOp::TextBlock:
        opTextBlock(gs);
        break;
    case EventOp::Text3:
        opText3(gs);
        break;
    case EventOp::TextDoor:
        opTextDoor(gs);
        break;
    case EventOp::TextPopupA:
        opTextPopupA(gs);
        break;
    case EventOp::TextPopupB:
        opTextPopupB(gs);
        break;
    case EventOp::WaitSpace:
        opWaitSpace(gs);
        break;
    case EventOp::WaitSpaceScripted:
        opWaitSpaceScripted(gs);
        break;
    case EventOp::PromptYn:
        opPromptYn(gs);
        break;
    case EventOp::PromptYnB:
        opPromptYnB(gs);
        break;
    case EventOp::ShowServiceWindow:
        opShowServiceWindow(gs, world);
        break;
    case EventOp::MapTransition:
        opMapTransition(gs, world);
        break;
    case EventOp::PlaySoundSeq:
        opPlaySoundSeq(gs, world);
        break;
    case EventOp::ExecSelector:
        opExecSelector(gs, world);
        break;
    case EventOp::EndScript:
        opEndScript(gs);
        break;
    case EventOp::IfTrueSkiptok:
        opIfTrueSkiptok(gs);
        break;
    case EventOp::IfFalseSkiptok:
        opIfFalseSkiptok(gs);
        break;
    case EventOp::EncounterSetup:
        opEncounterSetup(gs, world);
        break;
    case EventOp::EncounterSetupB:
        opEncounterSetupB(gs, world);
        break;
    case EventOp::ClearTileEvent:
        opClearTileEvent(gs, world);
        break;
    case EventOp::ApplyParty:
        opApplyParty(gs);
        break;
    case EventOp::ScanPartyItems:
        opScanPartyItems(gs);
        break;
    case EventOp::LoadVarRawToCond:
        opLoadVarRawToCond(gs);
        break;
    case EventOp::ApplyPartyMasked:
        opApplyPartyMasked(gs);
        break;
    case EventOp::GiveItem:
        opGiveItem(gs);
        break;
    case EventOp::StoreVar8:
        opStoreVar8(gs);
        break;
    case EventOp::CondThreshold:
        opCondThreshold(gs);
        break;
    case EventOp::RngRollToCond:
        opRngRollToCond(gs);
        break;
    case EventOp::AudioWait:
        opAudioWait(gs);
        break;
    case EventOp::Delay:
        opDelay(gs);
        break;
    case EventOp::PartyEffect:
        opPartyEffect(gs, false);
        break;
    case EventOp::PartyEffectB:
        opPartyEffect(gs, true);
        break;
    case EventOp::SetTile:
        opSetTile(gs, world);
        break;
    case EventOp::CheckEraRange:
        opCheckEraRange(gs);
        break;
    case EventOp::CheckDayRange:
        opCheckDayRange(gs);
        break;
    case EventOp::PayGoldToCond:
        opPayGoldToCond(gs);
        break;
    case EventOp::PayGemsToCond:
        opPayGemsToCond(gs);
        break;
    case EventOp::SelectMember:
        opSelectMember(gs);
        break;
    case EventOp::SelectMemberB:
        opSelectMemberB(gs);
        break;
    case EventOp::ConsumeItemToCond:
        opConsumeItemToCond(gs);
        break;
    case EventOp::SetAbort:
        opSetAbort(gs);
        break;
    case EventOp::SetTreasure:
        opSetTreasure(gs);
        break;
    case EventOp::SkiptokIfVictory:
        opSkiptokIfVictory(gs);
        break;
    case EventOp::AddWordCounter:
        opAddWordCounter(gs);
        break;
    case EventOp::CheckMemberAttr:
        opCheckMemberAttr(gs);
        break;
    case EventOp::OrMemberField:
        opOrMemberField(gs);
        break;
    case EventOp::ReadAnswer:
        opReadAnswer(gs);
        break;
    case EventOp::CheckAnswer:
        opCheckAnswer(gs);
        break;
    case EventOp::PartyIterateDamage:
        opPartyIterateDamage(gs);
        break;
    case EventOp::CountTitleNibble:
        opCountTitleNibble(gs);
        break;
    case EventOp::EndRecord:
    default:
        opUnknown(gs, op);
        break;
    }
}

void EventRuntime::opText(GameStateView &gs)
{
    char text_buf[256];
    text_.showOp01(readScriptString(gs, text_buf, sizeof(text_buf)));
    orEventExit(gs, 1);
}

void EventRuntime::opTextBlock(GameStateView &gs)
{
    char text_buf[256];
    text_.showOp02(readScriptString(gs, text_buf, sizeof(text_buf)), 19);
    orEventExit(gs, 2);
}

void EventRuntime::opText3(GameStateView &gs)
{
    char text_buf[256];
    const uint8_t idx = readU8(gs);
    text_.showOp03(resolveString(idx, text_buf, sizeof(text_buf)));
    if (location_id_ == 11 && idx == 5 && data_dir_) {
        text_.showPegasusIllustration(data_dir_);
    }
    orEventExit(gs, 3);
}

void EventRuntime::opTextDoor(GameStateView &gs)
{
    /* OP_04 @ 0x159F4: skip draw if -$79E1 != 0. */
    char text_buf[256];
    const char *s = readScriptString(gs, text_buf, sizeof(text_buf));
    if (!cantSee(gs)) {
        text_.showOp04(s);
    }
}

void EventRuntime::opTextPopupA(GameStateView &gs)
{
    /* OP_05 @ 0x15A46: can't-see gate @ 0x15A52. */
    char text_buf[256];
    const char *s = readScriptString(gs, text_buf, sizeof(text_buf));
    if (!cantSee(gs)) {
        text_.showOp05(s);
    }
}

void EventRuntime::opTextPopupB(GameStateView &gs)
{
    /* OP_06 @ 0x15AEE: '-'→'{' rewrite then can't-see gate @ 0x15B24. */
    char text_buf[256];
    const char *s = readScriptString(gs, text_buf, sizeof(text_buf));
    if (!cantSee(gs)) {
        text_.showOp06(s);
    }
}

void EventRuntime::opWaitSpace(GameStateView &gs)
{
    (void)gs;
    text_.showSpacePrompt();
    wait_ = EventVmWait::Space;
}

void EventRuntime::opWaitSpaceScripted(GameStateView &gs)
{
    /* OP_08 @ 0x15D26: -$71DC←$FD then SPACE via -$7DDC scripted buffer. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPTED_KEY_MODE, 0xFD);
    text_.showSpacePrompt();
    wait_ = EventVmWait::Space;
}

void EventRuntime::opPromptYn(GameStateView &gs)
{
    /* OP_09 @ 0x15D3C: clears cond, polls Y/N — draws nothing (doc 44 §3.7). */
    setEventCond(gs, 0);
    wait_ = EventVmWait::YesNo;
}

void EventRuntime::opPromptYnB(GameStateView &gs)
{
    /* OP_0A: -$71DC←$FD then same as OP_09 with scripted source. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPTED_KEY_MODE, 0xFD);
    setEventCond(gs, 0);
    wait_ = EventVmWait::YesNo;
}

void EventRuntime::opEndScript(GameStateView &gs)
{
    endScript(gs);
}

void EventRuntime::opIfTrueSkiptok(GameStateView &gs)
{
    const uint8_t n = readU8(gs);
    if (eventCond(gs)) {
        skipTokens(gs, n);
    }
}

void EventRuntime::opIfFalseSkiptok(GameStateView &gs)
{
    const uint8_t n = readU8(gs);
    if (!eventCond(gs)) {
        skipTokens(gs, n);
    }
}

void EventRuntime::opCheckEraRange(GameStateView &gs)
{
    /* event_op22_era_gate @ 0x16A9E: cond = (MM2_GS_ERA_LOW in [lo..hi]). */
    const uint8_t lo = readU8(gs);
    const uint8_t hi = readU8(gs);
    const uint8_t era = mm2_gs_u8(gs.a4(), MM2_GS_ERA_LOW);
    setEventCond(gs, (era >= lo && era <= hi) ? 1 : 0);
}

void EventRuntime::opSetAbort(GameStateView &gs)
{
    /* event_op29_force_abort @ 0x16D08: stop script before fall-through (e.g. C2 evt 22). */
    setEventAbort(gs);
}

void EventRuntime::opMapTransition(GameStateView &gs, world::MapWorld &world)
{
    /* event_op0c_map_transition @ 0x15E12 — bit6/high remaps inside
     * applyMapTransition; then OP_0F cleanup via endScript. */
    const uint8_t dest_screen = readU8(gs);
    const uint8_t dest_tile = readU8(gs);
    /* Silent cruise hops: OP_0D 0x09 then OP_0C with no prior wait. Save
     * before applyMapTransition → enterLocation clears the script flags. */
    const bool chain_dwell = op0d_09_this_script_ && !script_had_wait_;
    applyMapTransition(gs, world, dest_screen, dest_tile);
    endScript(gs);
    if (chain_dwell && mm2_gs_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH) != 0) {
        delay_remaining_ = hopDwellPolls(controlsDelayClamped(gs));
        delay_skip_armed_ = false;
        delay_key_skippable_ = false;
        wait_ = EventVmWait::Delay;
    }
}

void EventRuntime::opExecSelector(GameStateView &gs, world::MapWorld &world)
{
    /* event_op0e_selector_dispatch @ 0x160C2: SCRIPT_ABORT=1 at entry so
     * the current script ends after the selector returns; default-range
     * bins set QUEUED_EVENT_ID for the scanner epilogue @ 0x176B6.
     * 0x160D4: -$7946 = 2 (inn/0xFD overwrite to 1 before their jsr). */
    setEventAbort(gs);
    mm2_gs_set_u16(gs.a4(), MM2_GS_OP0E_SUBMODE, 2);
    const uint8_t sel = readU8(gs);
    eventExecTownSelector(*this, gs, world, sel, roster_, launch_, items_, text_, wait_,
                          location_id_, service_title_, rng_);
}

void EventRuntime::opShowServiceWindow(GameStateView &gs, world::MapWorld &world)
{
    const uint8_t str_idx = readU8(gs);
    const uint8_t placement = readU8(gs);
    /* OP_0B @ 0x15DB0 / 0x15756: str_idx is a sign/portrait table key, not str.dat.
     * table[A4-$79E3][str_idx-1] → .anm; draw via -$7FBC/-$7FC2; exit bit 2. No text.
     * Hillstone evt 15: 0b 0e 00 → env 2 → 49.anm. */
    text_.showOp0B(nullptr, data_dir_, gs, &world.attrib(), str_idx, placement);
    orEventExit(gs, 4);
}

void EventRuntime::opApplyParty(GameStateView &gs)
{
    const uint8_t count = readU8(gs);
    const uint8_t op = readU8(gs);
    const uint8_t val = readU8(gs);
    applyPartyProgressOp(gs, roster_, launch_, count, op, val, false, 0, 0);
}

void EventRuntime::opApplyPartyMasked(GameStateView &gs)
{
    const uint8_t count = readU8(gs);
    const uint8_t op = readU8(gs);
    const uint8_t and_m = readU8(gs);
    const uint8_t or_m = readU8(gs);
    applyPartyProgressOp(gs, roster_, launch_, count, op, 0, true, and_m, or_m);
}

void EventRuntime::opInvalid(GameStateView &gs)
{
    setEventAbort(gs);
}

void EventRuntime::opPlaySoundSeq(GameStateView &gs, world::MapWorld &world)
{
    /* OP_0D @ 0x15EC4 -> thunk -$7E42 -> play_sound_seq @ 0x6FB8 (ids 0..9). */
    const uint8_t idx = readU8(gs);
    if (idx == 0x09) {
        op0d_09_this_script_ = true;
    }
    eventVmExecEngineCall(gs.a4(), idx, &world);
}

void EventRuntime::opClearTileEvent(GameStateView &gs, world::MapWorld &world)
{
    eventVmClearTileEventFlag(gs.a4(), world, gs.coordY(), gs.coordX());
    /* PORT DEVIATION (ASM unclear): OP_14's collision-page andi #$7F does
     * not stop the triplet walk (cavern (1,2) is already 0x41). A separate
     * per-visit flag, checked by scanAndRun, is what actually suppresses
     * re-trigger until enterLocation. Typical dungeon fights are
     * OP_2B / OP_12 / OP_14 — post-combat re-scan skips OP_12 via OP_2B
     * and lands here. */
    markTileEventResolved(static_cast<int>(gs.coordY()), static_cast<int>(gs.coordX()));
}

void EventRuntime::opScanPartyItems(GameStateView &gs)
{
    /* event_op16_scan_party_items @ 0x16520: 1st byte discarded; scan +$3A/+ $28
     * for arg2. cond = match count of the first member with any hit. */
    readU8(gs);
    const uint8_t want = readU8(gs);
    uint8_t cond = 0;
    if (launch_ && roster_) {
        for (int i = 0; i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            const uint8_t *rec = rosterRecordBytes(roster_->records[launch_->roster_slots[i]]);
            for (int m = 0; m < kRosterItemSlots; ++m) {
                if (rosterBackpackId(rec, m) == want || rosterEquipId(rec, m) == want) {
                    ++cond;
                }
            }
            if (cond != 0) {
                break;
            }
        }
    }
    setEventCond(gs, cond);
}

void EventRuntime::opLoadVarRawToCond(GameStateView &gs)
{
    /* event_op17_load_cond_var @ 0x165A4: cond = *resolve(group) raw byte
     * (`move.b (a0),-$7951`), not 0/1 — OP_1B compares magnitude. 2nd byte discarded. */
    const uint8_t group = readU8(gs);
    const uint8_t index = readU8(gs);
    (void)index;
    setEventCond(gs, eventVmLoadVar(gs.a4(), group, index));
}

void EventRuntime::opGiveItem(GameStateView &gs)
{
    /* event_op19_give_item @ 0x165D8: 4 bytes (member-spec, id, attr2, attr3).
     * arg1 >= 0x80 → id from cond (captured before cond is cleared). First empty
     * backpack +$3A/+$40/+$46; else found-item overflow @ 0x166A0 (cond stays 0). */
    const uint8_t arg1 = readU8(gs);
    uint8_t id = readU8(gs);
    const uint8_t attr2 = readU8(gs);
    const uint8_t attr3 = readU8(gs);
    if (arg1 >= kOp19ItemFromCond) {
        id = eventCond(gs);
    }
    setEventCond(gs, 0);
    bool placed = false;
    if (launch_ && roster_) {
        for (int i = 0; !placed && i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            uint8_t *rec = rosterRecordBytes(roster_->records[launch_->roster_slots[i]]);
            if (rosterPlaceInFirstEmptyBackpack(rec, id, attr2, attr3)) {
                setEventCond(gs, 1);
                placed = true;
            }
        }
    }
    if (!placed) {
        mm2_found_items_overflow_append(gs.a4(), id, attr2, attr3);
    }
}

void EventRuntime::opStoreVar8(GameStateView &gs)
{
    /* event_op1a_store_var @ 0x166F8: 2 bytes — var id then value (resolver $15620
     * keys on id alone). No 3rd byte. */
    const uint8_t group = readU8(gs);
    const uint8_t val = readU8(gs);
    eventVmStoreVar(gs.a4(), group, 0, val);
}

void EventRuntime::opCondThreshold(GameStateView &gs)
{
    const uint8_t threshold = readU8(gs);
    if (eventCond(gs) < threshold) {
        setEventCond(gs, 0);
    }
}

void EventRuntime::opRngRollToCond(GameStateView &gs)
{
    /* event_op1c_engine_query @ 0x16742: push arg, push #1, jsr -$7BB4
     * (rng_roll @ 0x22BC6). Stores the RAW roll byte into cond_flag
     * (`move.b d0,-$7951`) — not a boolean. Unbound rng → 1 (same as
     * OP_0C fallback when rng_ is null). */
    const uint8_t hi = readU8(gs);
    const int roll = rng_ ? rng_->range(1, static_cast<int>(hi)) : 1;
    setEventCond(gs, static_cast<uint8_t>(roll & 0xFF));
}

void EventRuntime::opAudioWait(GameStateView &gs)
{
    /* event_op1d_engine_indexed @ 0x16762: -$7E84 → audio_wait_helper @ 0x6798
     * with index (arg1*7+1). Halves the wait count, polls -$7BD2, yields via
     * -$7B42. Presentation/audio only — no cond/GS write. Consume argc=1. */
    readU8(gs);
}

void EventRuntime::opDelay(GameStateView &gs)
{
    /* event_op1e_timed_wait @ 0x16780: `arg` iterations of delay(10) via
     * -$7BC0→0x22B4A then poll -$7BD2→0x22586, break on key. Host maps that
     * onto ~60 Hz frames (arg=200 ≈ 4 s at Delay 5), skippable after the
     * Yes/Enter key is released so boarding does not need a second press.
     * arg==0 is a no-op. */
    const uint8_t ticks = readU8(gs);
    if (ticks != 0) {
        delay_remaining_ = op1eHostPolls(ticks, controlsDelayClamped(gs));
        delay_skip_armed_ = false;
        delay_key_skippable_ = true;
        wait_ = EventVmWait::Delay;
    }
}

void EventRuntime::opPartyEffect(GameStateView &gs, bool mode_b)
{
    uint8_t args[5];
    const uint8_t sel = readU8(gs);
    for (int i = 0; i < 5; ++i) {
        args[i] = readU8(gs);
    }
    eventApplyPartyEffect(gs, roster_, launch_, sel, args, mode_b);
}

void EventRuntime::opSetTile(GameStateView &gs, world::MapWorld &world)
{
    const uint8_t pos = readU8(gs);
    const uint8_t visual = readU8(gs);
    const uint8_t collision = readU8(gs);
    eventVmPatchMapTile(world, (pos >> 4) & 0xF, pos & 0xF, visual, collision);
    /* 0x16A34: bset #$2, EXIT_FLAGS (map redraw). */
    orEventExit(gs, 4);
}

void EventRuntime::opCheckDayRange(GameStateView &gs)
{
    /* event_op23_day_gate @ 0x16ADA: cond = day-of-year predicate.
     * The day byte is the LOW byte of the current era's day word
     * (-$79DE[era], read as `move.b $1(a0,era*2)` — big-endian RAM word, so
     * +1 is the low 8 bits; day is 1..180 so this == day & 0xFF). arg1 is
     * read first, arg2 second (both always consumed → argc 2):
     *   arg1 == 0xB5 -> cond = (day bit0 set)   — odd-day gate
     *   arg1 == 0xB6 -> cond = (day bit0 clear)  — even-day gate
     *   else         -> cond = (arg1 <= day <= arg2)  — inclusive byte range */
    const uint8_t arg1 = readU8(gs);
    const uint8_t arg2 = readU8(gs);
    const uint8_t day = static_cast<uint8_t>(gs.day() & 0xFF);
    uint8_t cond;
    if (arg1 == kOp23OddDay) {
        cond = (day & 1) ? 1 : 0;
    } else if (arg1 == kOp23EvenDay) {
        cond = (day & 1) ? 0 : 1;
    } else {
        cond = (day >= arg1 && day <= arg2) ? 1 : 0;
    }
    setEventCond(gs, cond);
}

void EventRuntime::opPayGoldToCond(GameStateView &gs)
{
    /* event_op24_gold_check @ 0x16B54: u16 via 0x155DA, then -$7E6C → 0x6ACE
     * (pool+deduct). Cond = success. */
    const uint8_t lo = readU8(gs);
    const uint8_t hi = readU8(gs);
    const uint32_t need = static_cast<uint32_t>(lo | (hi << 8));
    const bool ok = eventVmPartyTryPayGold(gs.a4(), roster_, launch_, need);
    setEventCond(gs, ok ? 1 : 0);
}

void EventRuntime::opPayGemsToCond(GameStateView &gs)
{
    /* event_op25_code_check @ 0x16B82: hi,lo → u16, then -$7E66 → 0x6B9A
     * gems pool+deduct (NOT tickets/keys — those are OP_0E 0x08 / OP_28). */
    const uint8_t hi = readU8(gs);
    const uint8_t lo = readU8(gs);
    const uint16_t need = static_cast<uint16_t>((hi << 8) | lo);
    const bool ok = eventVmPartyTryPayGems(gs.a4(), roster_, launch_, need);
    setEventCond(gs, ok ? 1 : 0);
}

void EventRuntime::opSelectMember(GameStateView &gs)
{
    (void)gs;
    /* OP_26 @ 0x16BC0 flag≠0: key wait only (thunks -$7D0A/-$7BD2). No ROM
     * prompt string here — preceding OP_01/02 already drew the question.
     * Success path @ 0x16C70 writes slot → cond / -$5D42 / -$5D3F. */
    wait_ = EventVmWait::MemberSelect;
}

void EventRuntime::opSelectMemberB(GameStateView &gs)
{
    (void)gs;
    /* OP_27 @ 0x16BC0 flag=0: same leaf, input via -$7DDC. */
    wait_ = EventVmWait::MemberSelect;
}

void EventRuntime::opConsumeItemToCond(GameStateView &gs)
{
    /* OP_28 @ 0x16C86: discard 1st arg, item id = 2nd; backpack-only consume. */
    (void)readU8(gs);
    const uint8_t item_id = readU8(gs);
    const bool has = eventVmPartyConsumeBackpackItem(roster_, launch_, item_id);
    setEventCond(gs, has ? 1 : 0);
}

void EventRuntime::opSetTreasure(GameStateView &gs)
{
    uint8_t block[14];
    for (int i = 0; i < 14; ++i) {
        block[i] = readU8(gs);
    }
    eventVmApplyTreasure(gs.a4(), roster_, launch_, block);
}

void EventRuntime::opAddWordCounter(GameStateView &gs)
{
    /* event_op2c_adjust_state @ 0x16D98: WORD add of the u8 arg into the
     * counter at -$79B8 (add.w), then set exit-flag bit0 (redraw). */
    const uint8_t add = readU8(gs);
    const uint16_t cur = mm2_gs_u16(gs.a4(), MM2_GS_SCRIPT_COUNTER);
    mm2_gs_set_u16(gs.a4(), MM2_GS_SCRIPT_COUNTER, static_cast<uint16_t>(cur + add));
    orEventExit(gs, 1);
}

void EventRuntime::opCheckMemberAttr(GameStateView &gs)
{
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
    const uint8_t val2 = ((arg1 & 0xE0) == 0) ? static_cast<uint8_t>(arg2 & 0x0F) : val1;

    setEventCond(gs, 0);
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
        setEventCond(gs, 1);
    }
}

void EventRuntime::opOrMemberField(GameStateView &gs)
{
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
    if (arg1 >= kOp2eClericPair) {
        cls_a = 3;
        cls_b = 1;
        arg1 = static_cast<uint8_t>(arg1 & 0x7F);
    }
    const int field_off = static_cast<int>(static_cast<uint8_t>(arg1 - 0x6E)) + 0x51;
    if (launch_ && roster_ && field_off >= 0 &&
        field_off < static_cast<int>(MM2_ROSTER_RECORD_SIZE)) {
        for (int i = 0; i < launch_->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            Mm2RosterRecord *rec = &roster_->records[launch_->roster_slots[i]];
            if (rec->class_id == cls_a || rec->class_id == cls_b) {
                rosterRecordBytes(*rec)[field_off] |= static_cast<uint8_t>(arg2);
            }
        }
    }
}

void EventRuntime::opReadAnswer(GameStateView &gs)
{
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
}

void EventRuntime::opCheckAnswer(GameStateView &gs)
{
    uint8_t expected[10];
    for (int i = 0; i < 10; ++i) {
        expected[i] = readU8(gs);
    }
    const bool ok =
        eventVmCheckOp30Password(gs.a4() + MM2_GS_INPUT_BUF, expected, sizeof(expected));
    setEventCond(gs, ok ? 1 : 0);
}

void EventRuntime::opPartyIterateDamage(GameStateView &gs)
{
    /* event_op31_iterate_targets @ 0x170BC:
     *   EXIT_FLAGS |= bit1
     *   member-spec + u16 value (arg1>=0x80 → value from cond_flag)
     *   per resolved member: -$7F08 → 0x4952 (out-flags zeroed at call site)
     *   then -$7F14 → 0x47EC: living-party abort → SCRIPT_ABORT */
    orEventExit(gs, 2);
    const uint8_t member_spec = readU8(gs);
    const uint8_t lo = readU8(gs);
    const uint8_t hi = readU8(gs);
    const uint16_t value = static_cast<uint16_t>(lo | (hi << 8));
    eventVmOp31IterateDamage(gs.a4(), roster_, launch_, member_spec, value);
    if (eventVmCountLivingPartyMembers(gs.a4(), roster_, launch_) == 0) {
        setEventAbort(gs);
    }
}

void EventRuntime::opCountTitleNibble(GameStateView &gs)
{
    /* event_op32 @ 0x17190: cond = party class-nibble count (raw byte,
     * `move.b d0,-$7951`). Thunk -$7F2C → 0x04614: sum over living members of
     * nibbles of record+0x50 equal to `id` (helper 0x45C4). */
    const uint8_t id = readU8(gs);
    const int count = eventVmCountPartyNibbleMatches(gs.a4(), roster_, launch_, id);
    setEventCond(gs, static_cast<uint8_t>(count));
}

void EventRuntime::opSkiptokIfVictory(GameStateView &gs)
{
    /* OP_2B @ 0x16D74: skip N tokens when combat-victory latch set (A4-$77BD). */
    const uint8_t n = readU8(gs);
    if (mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH)) {
        skipTokens(gs, n);
    }
}

void EventRuntime::opEncounterSetup(GameStateView &gs, world::MapWorld &world)
{
    uint8_t block[12];
    for (int i = 0; i < 12; ++i) {
        block[i] = readU8(gs);
    }
    eventRunFixedEncounter(gs, text_, wait_, block, 12, false, combat_, &world);
}

void EventRuntime::opEncounterSetupB(GameStateView &gs, world::MapWorld &world)
{
    uint8_t block[10];
    for (int i = 0; i < 10; ++i) {
        block[i] = readU8(gs);
    }
    eventRunFixedEncounter(gs, text_, wait_, block, 10, true, combat_, &world);
}

void EventRuntime::opUnknown(GameStateView &gs, uint8_t op)
{
    if (op >= kEventOpFirstInvalid) {
        setEventAbort(gs);
        endScript(gs);
        return;
    }
    /* GAP: unimplemented op — advance past argc via token table. */
    const int pos = mm2_gs_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS);
    const int delta = tokenDelta(op);
    if (delta > 1) {
        mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, static_cast<uint16_t>(pos + delta - 1));
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
        if (eventAbort(gs)) {
            abortScript(gs);
            break;
        }

        const int pos = mm2_gs_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS);
        if (script_end >= 0 && pos >= script_end) {
            endScript(gs);
            break;
        }

        const uint8_t op = readU8(gs);
        if (op == static_cast<uint8_t>(EventOp::EndRecord)) {
            endScript(gs);
            break;
        }
        if (op >= kEventOpFirstInvalid) {
            setEventAbort(gs);
            endScript(gs);
            break;
        }

        dispatchOp(gs, world, op);
        /* Waits (Y/N, Answer, SPACE) must win over SCRIPT_ABORT. OP_0E @ 0x160C2
         * sets abort at entry so the script ends after the selector returns; in
         * the remake selectors are async, so abort is deferred until the wait
         * completes and runVmLoop resumes. */
        if (wait_ != EventVmWait::None) {
            script_had_wait_ = true;
            break;
        }
        if (!script_active_) {
            break;
        }
        if (eventAbort(gs)) {
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
     * Loc 60 starts FF 00 … so LE anchor = 0x00FF (Corak text bank). */
    const uint8_t qid = mm2_gs_u8(gs.a4(), MM2_GS_QUEUED_EVENT_ID);
    if (qid == 0xFF) {
        return false;
    }

    const uint16_t le_anchor =
        static_cast<uint16_t>((static_cast<uint16_t>(work_buf_[1]) << 8) | work_buf_[0]);
    string_anchor_ = le_anchor;
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR, le_anchor);
    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, 2);
    clearEventAbort(gs);

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

    /* Bound the inline segment at the next EndRecord (ASM stops on 0xFF fetch). */
    int seg_end = script_off;
    while (seg_end < MM2_GS_EVENT_WORK_SIZE &&
           work_buf_[seg_end] != static_cast<uint8_t>(EventOp::EndRecord)) {
        ++seg_end;
    }
    inline_script_end_ = seg_end;

    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, static_cast<uint16_t>(script_off));
    script_active_ = true;
    wait_ = EventVmWait::None;
    op0d_09_this_script_ = false;
    script_had_wait_ = false;
    runVmLoop(gs, world);
    /* Keep inline_script_end_ while waiting so continueInput stays in the same
     * EndRecord-delimited overlay segment (loc-60 banks embed 00 00 00). */
    if (wait_ == EventVmWait::None) {
        inline_script_end_ = -1;
    }

    /* ASM @ 0x176EA: reload home location via event_dat_loader + init.
     * Overlay swap is restored when the overlay script goes idle. */
    if (saved_loc_ == nullptr && loc_ &&
        loc_->kind != MM2_EVENT_KIND_CASTLE_BLOB &&
        loc_->kind != MM2_EVENT_KIND_OVERLAY_BANK) {
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

    /* Mid-script waits (OP_1E cruise delay, Y/N, SPACE) keep the VM paused.
     * Do not consume a latch OP_0C armed for the destination tile. */
    if (script_active_ || wait_ != EventVmWait::None) {
        return true;
    }

    /* ASM @ 0x176EA: after overlay dispatch the scanner reloads the home
     * location before the next triplet walk. Restore here when idle so a
     * post-combat re-scan cannot walk an overlay bank (no triplets → ambient
     * combat on every event-flagged tile). Mid-overlay waits keep saved_loc_. */
    if (!script_active_ && wait_ == EventVmWait::None) {
        restoreOverlayIfIdle(gs);
    }

    /* ASM @ 0x175EE–0x175F2: clear saved_cond and reset queued id to $FF at
     * scanner entry. Queued id set during a prior OP_0E default-range path must
     * survive until *after* that script's scanner epilogue — so we only clear
     * here when not mid-overlay. runDefaultRangeOverlay sets the queue and
     * runs the VM itself; the post-scan path below handles deferred queues. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_SAVED_COND_FLAG, 0);

    if (mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR) == 0xFFFF) {
        if (loc_->kind == MM2_EVENT_KIND_CASTLE_BLOB ||
            loc_->kind == MM2_EVENT_KIND_OVERLAY_BANK) {
            /* No map triplet table — fall through to queued path only. */
        } else {
            initParsed(gs);
        }
    }

    mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS, mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_START));
    clearEventAbort(gs);
    setEventExit(gs, 0);

    /* ASM scheduler @ 0x1280 clears -$7952 *before* the scanner. OP_0C @
     * 0x15EBA then sets it so the destination tile is scanned on the next
     * tick — cruise hops (Murray's boat) chain this way. Clearing at the
     * end of scanAndRun wiped that latch and stalled the tour. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 0);

    const uint8_t party_tile =
        static_cast<uint8_t>(((gs.coordY() & 0x0F) << 4) | (gs.coordX() & 0x0F));
    const uint8_t ctx = contextMask(gs);
    bool fired = false;
    bool matched_tile = false;

    if (!script_active_ && wait_ == EventVmWait::None) {
        /* Viewport OP_04/05/06/0B drop on the next tile scan. Console OP_01/02/03
         * are the plaque/message band: ASM $12D6 → 0x171AC roster/status redraw
         * wipes them on the key that leaves the tile. Drop them here too so a
         * scan that missed that key cannot keep stale event text. Mid-wait
         * prompts stay until continueInput finishes. */
        text_.clearPersistentOverlays();
        text_.clearConsoleMessageLayers();
    }

    /* Castle/overlay banks have no map triplet table — skip the walk. */
    if (loc_->kind != MM2_EVENT_KIND_CASTLE_BLOB &&
        loc_->kind != MM2_EVENT_KIND_OVERLAY_BANK &&
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
                if (tile_event_resolved_[a]) {
                    /* PORT DEVIATION: resolved this visit (OP_14). Keep
                     * matched_tile so 0x176F2 ambient combat does not arm. */
                } else if (eventCondMatches(c, ctx)) {
                    const int script_off = poolSeek(b);
                    if (script_off >= 0 && script_off < loc_->string_table_offset) {
                        const uint8_t first_op = work_buf_[script_off];
                        if (static_cast<EventOp>(first_op) == EventOp::CheckEraRange ||
                            eraGateOpen(gs, world)) {
                            mm2_gs_set_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS,
                                           static_cast<uint16_t>(script_off));
                            script_active_ = true;
                            wait_ = EventVmWait::None;
                            op0d_09_this_script_ = false;
                            script_had_wait_ = false;
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
                clearEventAbort(gs);
            }
        }
    }

    return fired;
}

bool EventRuntime::continueInput(GameStateView &gs, world::MapWorld &world, const platform::KeyState &keys)
{
    if (!script_active_ && wait_ == EventVmWait::None) {
        return false;
    }

    if (wait_ == EventVmWait::Delay) {
        /* OP_1E @ 0x16780: wall-clock wait, break on a *fresh* key (poll -$7BD2).
         * Hop-dwell after silent OP_0C is not skippable — leftover Yes/arrows
         * must not fast-forward the whole cruise. Timer expiry needs no key. */
        const bool key_down = keys.any_key || keys.space || keys.enter || keys.escape ||
                              keys.last_ascii != 0;
        if (delay_key_skippable_) {
            if (!delay_skip_armed_) {
                if (!key_down) {
                    delay_skip_armed_ = true;
                }
            } else if (key_down) {
                delay_remaining_ = 0;
            }
        }
        if (delay_remaining_ > 0) {
            --delay_remaining_;
        }
        if (delay_remaining_ > 0) {
            return true;
        }
        wait_ = EventVmWait::None;
        resetDelayState();
        if (script_active_) {
            runVmLoop(gs, world);
        }
        return script_active_ || wait_ != EventVmWait::None;
    }

    if (wait_ == EventVmWait::Space) {
        if (!space_wait_active_) {
            /* First poll of this SPACE wait. Combat victory (any key) and SDL/Amiga
             * SPACE are level-sampled; 0x15CE6 waits for a new $20, not a held key. */
            space_wait_armed_ = false;
            space_wait_active_ = true;
        }
    } else {
        space_wait_active_ = false;
    }

    if (wait_ == EventVmWait::Space) {
        /* OP_07 real keys; OP_08 / FD mode → -$7DDC; ASM loops until $20. */
        events::ScriptedKeyPlace place{};
        const int sk = eventVmScriptedKeyPoll(gs.a4(), &place);
        if (place.active) {
            if (place.clear) {
                text_.clearServiceSignSprite();
            } else if (text_.hasServicePortrait()) {
                text_.applyScriptedSignPlace(place.placement, place.dst_x, place.dst_y);
            }
        }
        const bool scripted_space = (sk == ' ' || sk == '\r' || sk == '\n');
        const char ascii = keys.last_ascii;
        const bool edge_continue = ascii == ' ' || ascii == '\r' || ascii == '\n';
        const bool level_held = keys.space || keys.enter || keys.any_key;
        if (!level_held && ascii == 0) {
            space_wait_armed_ = true;
        }
        if (!scripted_space && !edge_continue && !(space_wait_armed_ && level_held)) {
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
                    Mm2RosterRecord &rec = roster_->records[ridx];
                    rec.alignment_base = static_cast<uint8_t>(rec.alignment_base / 2);
                    rec.might_base = static_cast<uint8_t>(rec.might_base / 2);
                    rec.intelligence_base = static_cast<uint8_t>(rec.intelligence_base / 2);
                    rec.personality_base = static_cast<uint8_t>(rec.personality_base / 2);
                    rec.speed_base = static_cast<uint8_t>(rec.speed_base / 2);
                    rec.accuracy_base = static_cast<uint8_t>(rec.accuracy_base / 2);
                    rec.luck_base = static_cast<uint8_t>(rec.luck_base / 2);
                    rec.level = static_cast<uint8_t>(rec.level / 2);
                    rec.spell_level = static_cast<uint8_t>(rec.spell_level / 2);
                    rec.endurance_base = static_cast<uint8_t>(rec.endurance_base / 2);
                    rec.sp_max = static_cast<uint16_t>(rec.sp_max / 2);
                }
            }
        }
        if (script_active_) {
            runVmLoop(gs, world);
        }
        if (wait_ == EventVmWait::Space) {
            space_wait_armed_ = false;
            space_wait_active_ = true;
        } else {
            space_wait_active_ = false;
        }
        return script_active_ || wait_ != EventVmWait::None;
    }

    if (wait_ == EventVmWait::YesNo) {
        events::ScriptedKeyPlace place{};
        const int sk = eventVmScriptedKeyPoll(gs.a4(), &place);
        if (place.active) {
            if (place.clear) {
                text_.clearServiceSignSprite();
            } else if (text_.hasServicePortrait()) {
                text_.applyScriptedSignPlace(place.placement, place.dst_x, place.dst_y);
            }
        }
        char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (sk > 0) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(sk)));
        }
        if (ch != 'Y' && ch != 'N') {
            return true;
        }
        setEventCond(gs, ch == 'Y' ? 1 : 0);
        if (pending_quest_encode_stage_ == 1) {
            if (ch == 'Y') {
                pending_quest_encode_stage_ = 2;
                /* 0x198A0..0x19912: two-column caption + A–D (exe 0x18B5A..). */
                text_.showQuestDifficultyMenu(pending_quest_drink_ ? "Slayer" : "Hoardall");
                wait_ = EventVmWait::LetterSelect;
            } else {
                pending_quest_encode_stage_ = 0;
                wait_ = EventVmWait::None;
            }
        } else if (hasPendingTownMenu()) {
            finishPendingTownMenu(gs, ch == 'Y');
        } else {
            wait_ = EventVmWait::None;
        }
        /* finishPendingTownMenu may arm MemberSelect or Space; only drop
         * the consumed YesNo wait. */
        if (wait_ == EventVmWait::YesNo) {
            wait_ = EventVmWait::None;
        }
        /* OP_0E sets SCRIPT_ABORT at entry. Bare abort skips $171AC (0x17540), but
         * inn/smith/tavern No leaves call -$7D40 themselves — endScript matches that
         * so console OP_02 + OP_0B do not stick. Yes keeps layers until the shop
         * menu clears the console (sign stays). */
        if (ch == 'N' && wait_ == EventVmWait::None &&
            eventAbort(gs)) {
            endScript(gs);
            return script_active_ || wait_ != EventVmWait::None;
        }
        if (script_active_) {
            runVmLoop(gs, world);
        }
        return script_active_ || wait_ != EventVmWait::None;
    }

    if (wait_ == EventVmWait::MemberSelect) {
        const char ch = keys.last_ascii;
        if (ch == 27) {
            pending_time_machine_ = false;
            setEventAbort(gs);
            abortScript(gs);
            return false;
        }
        if (ch >= '1' && ch <= '8') {
            const int slot = ch - '0';
            if (pending_time_machine_) {
                /* 0x1480A: ESC already handled; digit − $30; if >=5 write era;
                 * then −1 indexes A4-$6CE4 (X) / -$6CDC (Y) / -$6CD4 (screen)
                 * into -$7FDA map load @ 0x1B2A. */
                pending_time_machine_ = false;
                wait_ = EventVmWait::None;
                if (slot >= 5) {
                    mm2_gs_set_u16(gs.a4(), MM2_GS_ERA, static_cast<uint16_t>(slot));
                }
                static const uint8_t kWaybackX[8] = {0x00, 0x00, 0x0F, 0x0F, 0x07, 0x05, 0x08, 0x0E};
                static const uint8_t kWaybackY[8] = {0x00, 0x0F, 0x0F, 0x00, 0x06, 0x05, 0x03, 0x04};
                static const uint8_t kWaybackScr[8] = {0x0F, 0x05, 0x21, 0x28, 0x0B, 0x25, 0x06, 0x26};
                const int idx = slot - 1;
                const uint8_t dest_tile =
                    static_cast<uint8_t>((kWaybackY[idx] << 4) | kWaybackX[idx]);
                applyMapTransition(gs, world, kWaybackScr[idx], dest_tile);
                return script_active_ || wait_ != EventVmWait::None;
            }
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
            setEventCond(gs, static_cast<uint8_t>(slot));
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
                        uint8_t *raw = rosterRecordBytes(roster_->records[ridx]);
                        if (raw[MM2_ROSTER_OFF_CIRCUS] & MM2_ROSTER_CIRCUS_BIT) {
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

    if (wait_ == EventVmWait::LetterSelect) {
        /* 0x19962: ESC / A–D; A–C → 0x19030 encode; D → 0x191A6 lord arm. */
        if (keys.escape || keys.last_ascii == 27) {
            pending_quest_encode_stage_ = 0;
            wait_ = EventVmWait::None;
            text_.showOp02("Then begone, knave!", 19);
            text_.showSpacePrompt();
            wait_ = EventVmWait::Space;
            return true;
        }
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch < 'A' || ch > 'D') {
            return true;
        }
        const int choice = ch - 'A';
        pending_quest_encode_stage_ = 0;
        wait_ = EventVmWait::None;
        if (choice <= 2) {
            if (roster_ && launch_) {
                if (pending_quest_drink_) {
                    (void)townSvcDrinkEncodePurchase(roster_, launch_, choice, rng_);
                } else {
                    (void)townSvcFoodEncodePurchase(roster_, launch_, choice, rng_);
                }
            }
            /* 0x1980A A–C: the pick wrote the item id (Hoardall/0xC9) or
             * monster type (Slayer/0xCA) into +$78, then the briefing prints
             * intro + resolved item/monster name + outro (exe strings beside
             * the 0x189DE/0x18A4C Lord's briefings). Falls back to the
             * previous generic wording only when the backing data is absent. */
            char quest_target[32];
            const bool has_target =
                townSvcQuestTargetName(roster_, launch_, pending_quest_drink_, items_,
                                       combat_ ? combat_->monsters() : nullptr, quest_target,
                                       sizeof(quest_target));
            if (has_target) {
                std::snprintf(quest_msg_, sizeof(quest_msg_),
                              "The quest I have decided upon for your\n"
                              "party, is to seek the %s%s",
                              quest_target, pending_quest_drink_
                                  ? " and destroy it.  I wish you luck,\n"
                                    "fair travelers!"
                                  : " and return it to me.  I wish you\n"
                                    "luck, fair travelers!");
            } else {
                std::snprintf(quest_msg_, sizeof(quest_msg_),
                              pending_quest_drink_
                                  ? "The quest I have decided upon for your\n"
                                    "party, is to seek the foe."
                                  : "The quest I have decided upon for your\n"
                                    "party, is to seek the item.");
            }
            text_.showOp02(quest_msg_, 19);
        } else {
            const int armed = townSvcQuestLordArm(roster_, launch_, pending_quest_drink_);
            if (armed < 0) {
                /* ASM returns -1 → re-prompt A–D. */
                pending_quest_encode_stage_ = 2;
                wait_ = EventVmWait::LetterSelect;
                return true;
            }
            /* 0x199ca → 0x191a6: Lord's Quest arm. The D pick then prints the
             * Lord's briefing table $6B6E (drink selector 0xCA = Slayer "trophies";
             * food 0xC9 = Hoardall "items"), not the generic "seek the target":
             *   0x189DE "Your party must bring me the three hidden swords of
             *   chivalry:  Valor,  Honor, and Nobility.  Good luck!"   (<- ret.)
             *   0x18A4C "Your party must defeat the three royal envoys of evil
             *   that wreak havoc to the north.  One flies, one slithers, and one
             *   crawls.  Good luck!"                                 (<- destroy) */
            text_.showOp02(pending_quest_drink_
                               ? "Your party must defeat the three royal\n"
                                 "envoys of evil that wreak havoc to the\n"
                                 "north.  One flies, one slithers, and one\n"
                                 "crawls.  Good luck!"
                               : "Your party must bring me the three\n"
                                 "hidden swords of chivalry:  Valor,\n"
                                 "Honor, and Nobility.  Good luck!",
                           19);
        }
        text_.showSpacePrompt();
        wait_ = EventVmWait::Space;
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

void EventRuntime::armWaybackMachineUi()
{
    /* 0x1480A: pea "What era do you desire (1-8)?" → -$7BE4; then -$7F68 key. */
    pending_time_machine_ = true;
    text_.showOp02("What era do you desire (1-8)?", 19);
    wait_ = EventVmWait::MemberSelect;
}

void EventRuntime::armQuestEncodeUi(bool drink)
{
    /* 0x1980A: intro from A4-$6B8E then Y/N @ 0x197D4; A–D @ 0x19962. */
    pending_quest_encode_stage_ = 1;
    pending_quest_drink_ = drink;
    if (drink) {
        text_.showOp02("Heads of monstrous beasts adorn the\n"
                       "walls in this room.  Will you gather\n"
                       "more trophies for Lord Slayer (y/n)?",
                       19);
    } else {
        text_.showOp02("The huge chamber is overstocked with\n"
                       "many unusual items.  Lord Hoardall\n"
                       "begs your party for a favor.  Will\n"
                       "you gather more items for him (y/n)?",
                       19);
    }
    wait_ = EventVmWait::YesNo;
}

}  // namespace mm2::events
