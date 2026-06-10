#include "mm2/events/EventRuntime.h"

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

#include "mm2_attrib_codec.h"

namespace mm2::events {

namespace {

/* Token length deltas for opcodes 0x00..0x32 (A4-$6CC8), from event_token_len_table.json. */
constexpr uint8_t kOpTokenDelta[0x33] = {
    1,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  3,  3,  2,  2,  1,  2,  2,  13, 11, 1,  4,  3,  3,  5,  5,  3,
    2,  2,  2,  2,  7,  7,  4,  3,  3,  3,  3,  1,  1,  3,  1,  15, 2,  2,  3,  3,  1,  11, 4,  2,
};

void initContextMaskTable(uint8_t *a4)
{
    /* Facing index 0/2/4/6 (W/S/E/N) → cond context @ A4-$6BE6 (scanner @ 0x175FE).
     * Bit 0x10 is always set; direction adds 0x20/0x40/0x80 so ALWAYS (0x10) matches
     * every facing while DIR_N?/DIR_SPECIAL/ENTER test the facing bits. */
    static const uint8_t kMasks[8] = {0x10, 0, 0x30, 0, 0x50, 0, 0x90, 0};
    for (int i = 0; i < 8; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_CONTEXT_MASK_TBL + i, kMasks[i]);
    }
}

int tokenDelta(uint8_t tok)
{
    if (tok < sizeof(kOpTokenDelta)) {
        return kOpTokenDelta[tok];
    }
    return 1;
}

}  // namespace

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
    data_dir_ = nullptr;
    location_id_ = -1;
    loc_ = nullptr;
    script_active_ = false;
    wait_ = EventVmWait::None;
    screen_changed_ = false;
    text_.reset();
    ::memset(work_buf_, 0, sizeof(work_buf_));
}

bool EventRuntime::enterLocation(int location_id, GameStateView &gs, const world::MapWorld &world)
{
    (void)world;
    if (!loaded_ || !gs.valid() || location_id < 0 || location_id >= MM2_EVENT_LOCATION_COUNT) {
        return false;
    }

    location_id_ = location_id;
    loc_ = &file_.locations[location_id];

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
    enterLocation(static_cast<int>(dest_screen), gs, world);
    screen_changed_ = true;
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
    case 0x0C: {
        const uint8_t dest_screen = readU8(gs);
        const uint8_t dest_tile = readU8(gs);
        applyMapTransition(gs, world, dest_screen, dest_tile);
        endScript(gs);
        break;
    }
    case 0x0E:
        readU8(gs);
        /* GAP: town service selector — end active script. */
        endScript(gs);
        break;
    case 0x0B: {
        const uint8_t str_idx = readU8(gs);
        const uint8_t placement = readU8(gs);
        char text_buf[256];
        text_.showOp0B(resolveString(str_idx, text_buf, sizeof(text_buf)), data_dir_, location_id_,
                       str_idx, placement);
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 4));
        break;
    }
    case 0x12:
    case 0x13:
        /* GAP: combat encounter setup — abort script. */
        mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
        endScript(gs);
        break;
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
    if (!script_active_ && wait_ == EventVmWait::None) {
        text_.reset();
    }

    const uint8_t party_tile =
        static_cast<uint8_t>(((gs.coordY() & 0x0F) << 4) | (gs.coordX() & 0x0F));
    const uint8_t ctx = contextMask(gs);
    bool fired = false;

    int pos = 0;
    while (pos + 2 < MM2_GS_EVENT_WORK_SIZE) {
        uint8_t a = work_buf_[pos];
        uint8_t b = work_buf_[pos + 1];
        uint8_t c = work_buf_[pos + 2];
        if (a == 0 && b == 0 && c == 0) {
            break;
        }

        if (a == party_tile && (c & ctx) != 0) {
            if (eraGateOpen(gs, world)) {
                const int script_off = poolSeek(b);
                if (script_off >= 0) {
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
        if (!keys.space) {
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
        wait_ = EventVmWait::None;
        if (script_active_) {
            runVmLoop(gs, world);
        }
        return script_active_ || wait_ != EventVmWait::None;
    }

    return false;
}

}  // namespace mm2::events
