#pragma once

#include "mm2/GameState.h"

#include "mm2/gameplay/InGameCharacterSheet.h"
#include "mm2/gameplay/InGameControlsScreen.h"
#include "mm2/gameplay/PlaySessionInput.h"

#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/OutdoorView3D.h"
#include "mm2/gfx/View3D.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/events/ScriptedSceneEngine.h"
#include "mm2/platform/Platform.h"
#include "mm2/world/MapWorld.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"
#include "mm2_items_codec.h"
#include "mm2_image32_codec.h"
#include "mm2_gamestate.h"

namespace mm2 {

namespace gameplay {
struct MoveResult;
}

class GameSession {
public:
    static const char *areaName(uint8_t area_id);

    /** kStartSkipScriptedIntros: offline golden dumps — no Corak/Pegasus overlay on HUD. */
    static constexpr uint32_t kStartSkipScriptedIntros = 1u;

    bool start(const char *data_dir, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
               uint32_t start_flags = 0);
    void shutdown();

#if MM2_HOST_AMIGA
    /** Staged play-mode asset load — one heavy step per frame (Bartman 4K stack). */
    bool isBootstrapping() const { return bootstrapping_; }
    void tickBootstrap();
#endif

    void tick(const platform::KeyState &keys);
    void render();

    /** Amiga: skip full play-screen rebuild when false (still pace vblank). */
    bool needsRedraw() const { return frame_dirty_; }
    void ackRedraw() { frame_dirty_ = false; }

    bool shouldQuit() const { return quit_; }
    bool requestTitle() const { return back_to_title_; }
    void clearTitleRequest() { back_to_title_ = false; }

    bool awaitingContinuePrompt() const;

    const gfx::ScreenCompositor &compositor() const { return compositor_; }

private:
    enum class PlayOverlay : uint8_t { None, QuickRef, CharacterSheet, QuitConfirm, Controls, StatusMessage };

    static const char *townName(uint8_t town_filter);

    void renderView3D();
    void renderIndoorView3D();
    void renderOutdoorView();
    void renderPartyPanel();
    void renderOverlays();
    void refreshWorldAfterMove(const gameplay::MoveResult &move);
    void tickOverlayInput(const platform::KeyState &keys);
    void tickPlayInput(const platform::KeyState &keys);
    void handleExploreCommand(gameplay::PlaySessionAction action);
    bool overlayBlocksInput() const;
    void tickEventInput(const platform::KeyState &keys);
    void tickOverlayAnimations();
    bool eventBlocksInput() const;
    bool scriptedBlocksInput() const;
    void runPendingEvents();
    void refreshEventsForScreen();
    void refreshWorldAfterEventTransition();
    void maybeQueueScriptedScenes(bool on_start);
    void showStatusMessage(const char *msg);
    void markDirty() { frame_dirty_ = true; }
    gfx::PlayProtectValues protectValues() const;

    const char *data_dir_ = nullptr;
    uint32_t start_flags_ = 0;
    bool quit_ = false;
    bool back_to_title_ = false;
    bool assets_ok_ = false;

    Mm2PartyLaunch launch_{};
    Mm2RosterFile roster_{};
    Mm2ItemsFile items_{};
    bool has_items_ = false;

    world::MapWorld world_;
    gfx::EnvAssets env_;
    gameplay::InGameCharacterSheet ingame_sheet_;
    gameplay::InGameControlsScreen controls_screen_;
    gameplay::SheetSession sheet_session_{};

    PlayOverlay overlay_ = PlayOverlay::None;
    char status_message_[64] = {};

    gfx::PlayRightPanel right_panel_ = gfx::PlayRightPanel::Protect;

    static constexpr std::size_t kGsImageBytes = static_cast<std::size_t>(MM2_A4_ANCHOR) + 0x8000u;
    uint8_t *gs_image_ = nullptr;
    GameStateView gs_;

    gfx::ScreenCompositor compositor_;
    events::EventRuntime events_;
    bool events_loaded_ = false;

    events::ScriptedSceneEngine scripted_scene_;
    bool scripted_loaded_ = false;

    bool frame_dirty_ = false;

#if MM2_HOST_AMIGA
    bool bootstrapping_ = false;
    uint8_t bootstrap_step_ = 0;
#endif
};

}  // namespace mm2
