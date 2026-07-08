#pragma once

#include "mm2/GameState.h"

#include "mm2/combat/CombatSession.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/gameplay/InGameCharacterSheet.h"
#include "mm2/gameplay/InGameControlsScreen.h"
#include "mm2/gameplay/PlaySessionInput.h"

#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/world/AutomapState.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/OutdoorView3D.h"
#include "mm2/gfx/View3D.h"
#include "mm2/gfx/ViewportAnmOverlay.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/events/ScriptedSceneEngine.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/PlayTownServiceUi.h"
#include "mm2/world/MapWorld.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"
#include "mm2_items_codec.h"
#include "mm2_monsters_codec.h"
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
    bool needsRedraw() const;
    void ackRedraw();

    bool shouldQuit() const { return quit_; }
    bool requestTitle() const { return back_to_title_; }
    void clearTitleRequest() { back_to_title_ = false; }
    bool requestGotoTown() const { return back_to_goto_town_; }
    void clearGotoTownRequest() { back_to_goto_town_ = false; }
    /** Char-choose town filter (1..5) saved @ 0x1A1E2 → A4-$79AC after inn registry. */
    uint8_t gotoTownFilter() const { return goto_town_filter_; }

    const Mm2RosterFile &roster() const { return roster_; }
    Mm2RosterFile &roster() { return roster_; }
    const Mm2PartyLaunch &launch() const { return launch_; }

    bool awaitingContinuePrompt() const;

    const gfx::ScreenCompositor &compositor() const { return compositor_; }

    /** Current map screen label (town name, D2, Luxus Palace, …) — not home town. */
    const char *currentScreenLabel() const;
    int currentScreenId() const { return world_.currentScreen(); }

private:
    enum class PlayOverlay : uint8_t { None, QuickRef, CharacterSheet, QuitConfirm, Controls, StatusMessage, Automap, TownService, RestConfirm };

    static const char *townName(uint8_t town_filter);

    void renderFrame(bool overlay_anim_only);
#if MM2_HOST_AMIGA
    bool canUsePartialView3DRefresh() const;
    void renderFrameView3DOnly();
    void renderFrameOverlayAnimOnly();
    void renderFrameCombatAnimOnly();
#endif
    void renderView3D();
    void renderCombatBackdrop();
    void renderIndoorView3D();
    void renderOutdoorView();
    void renderPartyPanel();
    void refreshCombatSprite();
    void renderOverlays();
    void refreshWorldAfterMove(const gameplay::MoveResult &move);
    void tickCombatCharacterSheetInput(const platform::KeyState &keys);
    void tickOverlayInput(const platform::KeyState &keys);
    bool combat_character_sheet_ = false;
    void tickPlayInput(const platform::KeyState &keys);
    void handleExploreCommand(gameplay::PlaySessionAction action);
    /* Bash door @ 0x9B48 / Unlock door @ 0x20CA2 — detect the wall field ahead
     * and run the faithful Might / thievery rolls (gameplay::ExploreActions). */
    void handleBashDoor();
    void handleUnlockDoor();
    void applyDoorTrapDamage();
    /* Rest execution @ 0x19E20 -> 0x19AD6 (pay) / 0x19D64 (encounter) / 0x19B28
     * (heal + 0x55-tick advance), run after the Y/N confirm overlay resolves. */
    void executeRest();
    /* Random step encounter @ 0x10A2, simplified gate: outdoor, non-town,
     * same-screen forward step; rate byte = attrib.dat 0x09 (doc 35). */
    void maybeTriggerStepEncounter();
    void finishCombat();
    bool overlayBlocksInput() const;
    void tickEventInput(const platform::KeyState &keys);
    void tickOverlayAnimations();
    void tickTorchAnimation();
    bool eventBlocksInput() const;
    bool scriptedBlocksInput() const;
    void runPendingEvents();
    void refreshEventsForScreen();
    void refreshWorldAfterEventTransition();
    void maybeQueueScriptedScenes(bool on_start);
    /** Open the interactive town-service overlay when an OP_0E selector requested it. */
    void maybeOpenTownServiceMenu();
    void maybeFinishInnRegistry();
    void showStatusMessage(const char *msg);
    void markDirty();
#if MM2_HOST_AMIGA
    void markView3DDirty();
    void markTextDirty();
    void markOverlayAnimDirty();
#endif
    /** Quick Ref / character sheet / etc. replace the playfield — no viewport .anm. */
    bool viewportHiddenByOverlay() const;
    gfx::PlayProtectValues protectValues() const;

    const char *data_dir_ = nullptr;
    uint32_t start_flags_ = 0;
    bool quit_ = false;
    bool back_to_title_ = false;
    bool back_to_goto_town_ = false;
    uint8_t goto_town_filter_ = 1;
    bool assets_ok_ = false;

    Mm2PartyLaunch launch_{};
    Mm2RosterFile roster_{};
    Mm2ItemsFile items_{};
    bool has_items_ = false;
    Mm2MonstersFile monsters_{};
    bool has_monsters_ = false;
    combat::CombatSession combat_;
    gfx::ViewportAnmOverlay combat_sprite_{};
    int combat_sprite_disk_ = -1;
#if MM2_HOST_AMIGA
    bool combat_backdrop_cached_ = false;
#endif

    world::MapWorld world_;
    world::AutomapState automap_;
    gfx::EnvAssets env_;
    gameplay::InGameCharacterSheet ingame_sheet_;
    gameplay::InGameControlsScreen controls_screen_;
    gameplay::SheetSession sheet_session_{};

    PlayOverlay overlay_ = PlayOverlay::None;
    char status_message_[64] = {};

    /* rng(min,max) source for Bash/Unlock/Rest rolls (0x22BC6 contract). */
    gameplay::Rng rng_;

    gfx::PlayRightPanel right_panel_ = gfx::PlayRightPanel::Protect;

    static constexpr std::size_t kGsImageBytes = static_cast<std::size_t>(MM2_A4_ANCHOR) + 0x8000u;
    uint8_t *gs_image_ = nullptr;
    GameStateView gs_;

    gfx::ScreenCompositor compositor_;
    events::EventRuntime events_;
    bool events_loaded_ = false;

    ui::PlayTownServiceUi town_service_ui_;

    events::ScriptedSceneEngine scripted_scene_;
    bool scripted_loaded_ = false;
    /* Port-side one-shot for the Corak new-game prologue scene scheduling (the
     * classic intro is not a per-character quest bit). */
    bool corak_intro_seen_ = false;

    /** Torch flicker phase (A4-$667A) — advanced each indoor viewport tick. */
    int torch_phase_ = 0;
    int torch_tick_ = 0;

#if MM2_HOST_AMIGA
    bool view3d_dirty_ = false;
    bool chrome_dirty_ = false;
    bool text_dirty_ = false;
    bool overlay_anim_dirty_ = false;
    /** After a full play-screen draw, pFront holds HUD chrome safe to copy to pBack. */
    bool play_buffer_valid_ = false;
#else
    bool frame_dirty_ = false;
#endif

#if MM2_HOST_AMIGA
    bool bootstrapping_ = false;
    uint8_t bootstrap_step_ = 0;
#endif
};

}  // namespace mm2
