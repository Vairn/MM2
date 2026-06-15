#pragma once
// Castle / scripted-scene graphics @ 0x6FB8 / 0x76AC, overlay @ 0x316E / mode $17,
// text+wait @ 0x64F8. See EXTRACTED/docs/46-scripted-scene-graphics.md.

#include "mm2/Config.h"
#include "mm2/CppStdCompat.h"
#include "mm2/GameState.h"
#include "mm2/events/EventTextView.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/ViewportAnmOverlay.h"
#include "mm2/platform/Platform.h"

#include "mm2_event_codec.h"
#include "mm2_gamestate.h"

namespace mm2::events {

enum class ScriptedSceneId : uint8_t {
    None = 0,
    CorakIntro,   /* Middlegate first visit: ghost overlay + loc 60 str[8] */
    PegasusC2,    /* Overland C2 (map 11) evt 04 @ (4,7): 21.anm sprite + loc 11 str[5] */
};

enum class ScriptedSceneType : uint8_t {
    None = 0,
    ViewportSpriteOverlay,      /* 0x316E / 0x23C8C mode $17 — monster .anm over 3D hood */
    TextAndWait,                /* 0x64F8 + OP_07 family wait */
};

/** Scripted illustration / overlay engine (separate from event VM text ops). */
class ScriptedSceneEngine {
public:
    bool load(const char *data_dir);
    void unload();

    void queueScene(ScriptedSceneId id);
    /** Offline demo: show graphics + text + SPACE prompt without blocking tick loop. */
    void armDemo(ScriptedSceneId id);

    void tick(const platform::KeyState &keys);
    /** Advance viewport .anm overlays one game tick; true when any cel changed. */
    bool tickAnimation();
    void draw(gfx::ScreenCompositor &c);

    /** OP_0B / 0x316E viewport sprites only — partial redraw after 3D hood restore. */
    void drawViewportSpriteOverlays(gfx::ScreenCompositor &c) const;
    bool hasViewportSpriteOverlays() const;

    bool blocksInput() const;
    bool active() const { return scene_id_ != ScriptedSceneId::None; }
    /** When true, skip 3D hood draw (unused — Pegasus uses sprite overlay, not full-sheet blit). */
    bool hidesView3D() const;
    /** After scene end @ 0x171AC cleanup — caller should redraw 3D next frame. */
    bool needsViewportRestore() const { return viewport_restore_; }
    void clearViewportRestore() { viewport_restore_ = false; }
    /** A4-$79B2 while scene active: 0=OPTIONS (Corak), 1=PROTECT (Pegasus). */
    uint8_t panelMode() const { return panel_mode_; }

    EventTextView &textView() { return text_; }
    const EventTextView &textView() const { return text_; }

private:
    struct SceneSpec {
        ScriptedSceneId id;
        ScriptedSceneType gfx;
        int event_loc;
        int str_idx;
        bool tall_text;       /* OP_03 rows 17-22 vs OP_02 */
        uint8_t panel_mode;   /* A4-$79B2: 0=OPTIONS, 1=PROTECT */
        int sign_anm_id;      /* Optional pre-illustration OP_0B-style sign; 0 = none */
    };

    static const SceneSpec *specFor(ScriptedSceneId id);

    const char *resolveEventString(int loc_id, int str_idx, char *buf, size_t cap) const;
    void beginScene(const SceneSpec *spec);
    void endScene();
    void showSceneText(const SceneSpec *spec);

    void drawGhostPlaceholder(gfx::ScreenCompositor &c) const;

    bool tryLoadGhostAnm(const char *data_dir);
    bool tryLoadPegasusAnm(const char *data_dir);

    Mm2EventFile event_file_{};
    bool event_loaded_ = false;
    const char *data_dir_ = nullptr;

    gfx::ViewportAnmOverlay pegasus_sprite_{};
    gfx::ViewportAnmOverlay ghost_sprite_{};
    bool has_pegasus_sprite_ = false;
    bool has_ghost_sprite_ = false;

    gfx::ViewportAnmOverlay pegasus_sign_{};
    bool has_pegasus_sign_ = false;

    ScriptedSceneId scene_id_ = ScriptedSceneId::None;
    ScriptedSceneType gfx_type_ = ScriptedSceneType::None;
    uint8_t panel_mode_ = 1;
    bool waiting_space_ = false;
    bool demo_armed_ = false;
    bool viewport_restore_ = false;

    EventTextView text_{};
};

}  // namespace mm2::events
