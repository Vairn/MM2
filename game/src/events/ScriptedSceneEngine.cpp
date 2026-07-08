#include "mm2/events/ScriptedSceneEngine.h"

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/View3D.h"

#include "mm2/CppStdCompat.h"

namespace mm2::events {

namespace {

using namespace gfx;
using namespace gfx::play_layout;

/* monsters.dat #131 Pegasus picture &0x7F = 21 → 21.anm on disk. */
constexpr int kPegasusMonsterAnmDisk = 21;
constexpr int kGhostAnmDisk = 51;

}  // namespace

const ScriptedSceneEngine::SceneSpec *ScriptedSceneEngine::specFor(ScriptedSceneId id)
{
    static const SceneSpec kSpecs[] = {
        /* Ghost: monsters.dat #170 Ghost picture=51 -> 51.anm (combat sprite reused). */
        {ScriptedSceneId::CorakIntro, ScriptedSceneType::ViewportSpriteOverlay, 60, 8, true, 0, 0},
        /* Loc 11 evt 04 @ (4,7): OP_0B str[14] + OP_03 str[5]; sprite = monster #131 -> 21.anm. */
        {ScriptedSceneId::PegasusC2, ScriptedSceneType::ViewportSpriteOverlay, 11, 5, true, 1, 0},
    };
    for (const SceneSpec &s : kSpecs) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

bool ScriptedSceneEngine::load(const char *data_dir)
{
    unload();
    data_dir_ = data_dir;

    char *path = mm2_path_scratch_a();
    if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, "event.dat")) {
        if (mm2_event_load_file(path, &event_file_) == MM2_EVENT_OK) {
            event_loaded_ = true;
        }
    }

    tryLoadGhostAnm(data_dir);
    tryLoadPegasusAnm(data_dir);
    return has_pegasus_sprite_ || event_loaded_ || has_ghost_sprite_;
}

void ScriptedSceneEngine::unload()
{
    if (event_loaded_) {
        mm2_event_free(&event_file_);
        event_loaded_ = false;
    }
    pegasus_sprite_.unload();
    ghost_sprite_.unload();
    has_pegasus_sprite_ = false;
    has_ghost_sprite_ = false;
    pegasus_sign_.unload();
    has_pegasus_sign_ = false;

    data_dir_ = nullptr;
    scene_id_ = ScriptedSceneId::None;
    gfx_type_ = ScriptedSceneType::None;
    waiting_space_ = false;
    demo_armed_ = false;
    viewport_restore_ = false;
    text_.reset();
}

bool ScriptedSceneEngine::tryLoadGhostAnm(const char *data_dir)
{
    /* Defer pens 3-17 until beginScene — Pegasus preload must not clobber ghost palette. */
    has_ghost_sprite_ = ghost_sprite_.loadFromDiskIndex(data_dir, kGhostAnmDisk, gfx::AnmLoopMode::Loop, false);
    return has_ghost_sprite_;
}

bool ScriptedSceneEngine::tryLoadPegasusAnm(const char *data_dir)
{
    has_pegasus_sprite_ =
        pegasus_sprite_.loadFromDiskIndex(data_dir, kPegasusMonsterAnmDisk, gfx::AnmLoopMode::Loop, false);
    return has_pegasus_sprite_;
}

const char *ScriptedSceneEngine::resolveEventString(int loc_id, int str_idx, char *buf, size_t cap) const
{
    if (!buf || cap == 0) {
        return buf;
    }
    buf[0] = '\0';
    if (!event_loaded_ || loc_id < 0 || loc_id >= MM2_EVENT_LOCATION_COUNT) {
        return buf;
    }

    const Mm2EventLocation &loc = event_file_.locations[loc_id];
    if (loc.string_table_offset < 0 || static_cast<size_t>(loc.string_table_offset) >= loc.raw_len) {
        return buf;
    }

    int cur = 0;
    size_t pos = static_cast<size_t>(loc.string_table_offset);
    while (pos < loc.raw_len) {
        size_t end = pos;
        while (end < loc.raw_len && loc.raw[end] != 0xFF) {
            ++end;
        }
        if (cur == str_idx) {
            size_t j = 0;
            for (size_t i = pos; i < end && j + 1 < cap; ++i) {
                const char ch = static_cast<char>(loc.raw[i]);
                buf[j++] = (ch == '@') ? '\n' : ch;
            }
            buf[j] = '\0';
            return buf;
        }
        ++cur;
        pos = end + 1;
    }

    std::snprintf(buf, cap, "<str[%d]>", str_idx);
    return buf;
}

void ScriptedSceneEngine::beginScene(const SceneSpec *spec)
{
    if (!spec) {
        return;
    }
    scene_id_ = spec->id;
    gfx_type_ = spec->gfx;
    panel_mode_ = spec->panel_mode;
    waiting_space_ = false;
    demo_armed_ = false;
    viewport_restore_ = false;
    pegasus_sign_.unload();
    has_pegasus_sign_ = false;
    if (spec->sign_anm_id > 0 && data_dir_) {
        has_pegasus_sign_ = pegasus_sign_.loadFromTableId(data_dir_, spec->sign_anm_id, gfx::AnmLoopMode::Loop,
                                                          false);
    }
    text_.reset();
    showSceneText(spec);
#if MM2_HOST_AMIGA
    if (gfx_type_ == ScriptedSceneType::ViewportSpriteOverlay) {
        if (spec->id == ScriptedSceneId::CorakIntro && ghost_sprite_.loaded()) {
            ghost_sprite_.applyHardwarePalette();
        } else if (spec->id == ScriptedSceneId::PegasusC2 && pegasus_sprite_.loaded()) {
            pegasus_sprite_.applyHardwarePalette();
        }
        if (has_pegasus_sign_ && pegasus_sign_.loaded()) {
            pegasus_sign_.applyHardwarePalette();
        }
    }
#endif
    text_.showSpacePrompt();
    waiting_space_ = true;
}

void ScriptedSceneEngine::showSceneText(const SceneSpec *spec)
{
    char buf[512];
    resolveEventString(spec->event_loc, spec->str_idx, buf, sizeof(buf));

    if (spec->tall_text) {
        text_.showOp03(buf);
    } else {
        text_.showOp02(buf, 19);
    }
}

void ScriptedSceneEngine::endScene()
{
    bool redraw_status = false;
    bool redraw_roster = false;
    bool redraw_divider = false;
    text_.scriptCleanup(&redraw_status, &redraw_roster, &redraw_divider);
    (void)redraw_status;
    (void)redraw_roster;
    (void)redraw_divider;

    if (gfx_type_ == ScriptedSceneType::ViewportSpriteOverlay) {
        viewport_restore_ = true; /* 0x171AC sign_sprite_place(-1) + buf_copy_rect */
    }

    scene_id_ = ScriptedSceneId::None;
    gfx_type_ = ScriptedSceneType::None;
    waiting_space_ = false;
    demo_armed_ = false;
}

void ScriptedSceneEngine::queueScene(ScriptedSceneId id)
{
    if (id == ScriptedSceneId::None || active()) {
        return;
    }
    const SceneSpec *spec = specFor(id);
    if (!spec) {
        return;
    }
    beginScene(spec);
}

void ScriptedSceneEngine::armDemo(ScriptedSceneId id)
{
    /* Demo loop reuses one engine; end prior scene so queueScene is not blocked by active(). */
    if (active()) {
        endScene();
    }
    queueScene(id);
    demo_armed_ = true;
}

void ScriptedSceneEngine::tick(const platform::KeyState &keys)
{
    if (!active() || demo_armed_) {
        return;
    }

    if (waiting_space_ && (keys.space || keys.enter || keys.any_key)) {
        endScene();
    }
}

bool ScriptedSceneEngine::tickAnimation()
{
    if (!active()) {
        return false;
    }

    bool changed = false;
    if (scene_id_ == ScriptedSceneId::PegasusC2 && has_pegasus_sprite_) {
        changed |= pegasus_sprite_.tick();
    } else if (has_ghost_sprite_) {
        changed |= ghost_sprite_.tick();
    }
    if (has_pegasus_sign_) {
        changed |= pegasus_sign_.tick();
    }
    return changed;
}

bool ScriptedSceneEngine::blocksInput() const
{
    return active() && !demo_armed_;
}

bool ScriptedSceneEngine::hidesView3D() const
{
    return false;
}

void ScriptedSceneEngine::drawGhostPlaceholder(gfx::ScreenCompositor &c) const
{
    const int cx = kView3DOriginX + kView3DViewportW / 2;
    const int cy = kView3DSkyY + 36;
    const int gw = 48;
    const int gh = 72;
    const int gx = cx - gw / 2;
    const int gy = cy - gh / 2;

    c.fillRect(gx, gy, gw, gh, 240, 240, 255, 220);
    c.fillRect(gx + 12, gy + 8, 8, 8, 40, 40, 60, 255);
    c.fillRect(gx + 28, gy + 8, 8, 8, 40, 40, 60, 255);
    c.drawTextShadow(gx - 4, gy + gh + 4, "ghost 51.anm candidate", 200, 200, 220);
}

bool ScriptedSceneEngine::hasViewportSpriteOverlays() const
{
    if (!active() || gfx_type_ != ScriptedSceneType::ViewportSpriteOverlay) {
        return false;
    }
    if (scene_id_ == ScriptedSceneId::PegasusC2) {
        return has_pegasus_sprite_ || has_pegasus_sign_;
    }
    return has_ghost_sprite_ || has_pegasus_sign_;
}

void ScriptedSceneEngine::drawViewportSpriteOverlays(gfx::ScreenCompositor &c) const
{
    if (!active() || gfx_type_ != ScriptedSceneType::ViewportSpriteOverlay) {
        return;
    }

    /* Same path as OP_0B / sign_sprite_place @ 0x3266 → mode $17 @ 0x23C8C
     * (pos, $40, $20) — blitCentered(), not viewport-centered blitAt(). */
    if (scene_id_ == ScriptedSceneId::PegasusC2) {
        if (has_pegasus_sprite_ && pegasus_sprite_.loaded()) {
            pegasus_sprite_.blitCentered(c, 0);
        } else {
            c.drawTextShadow(kView3DOriginX + 8, kView3DSkyY + 48, "(21.anm pegasus sprite missing)", 255, 200,
                             100);
        }
    } else if (has_ghost_sprite_ && ghost_sprite_.loaded()) {
        ghost_sprite_.blitCentered(c, 0);
    }

    if (has_pegasus_sign_) {
        pegasus_sign_.blitCentered(c, 0);
    }
}

void ScriptedSceneEngine::draw(gfx::ScreenCompositor &c)
{
    if (!active()) {
        return;
    }

    drawViewportSpriteOverlays(c);
    text_.draw(c);
}

}  // namespace mm2::events
