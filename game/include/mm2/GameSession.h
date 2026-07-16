#pragma once

#include "mm2/GameState.h"

#include "mm2/combat/CombatSession.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/gameplay/InGameCharacterSheet.h"
#include "mm2/gameplay/InGameControlsScreen.h"
#include "mm2/gameplay/PlaySessionInput.h"

#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/CombatPanel.h"
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

    /** Read-only game-state view (offline golden dumps / debugging). */
    const GameStateView &gameState() const { return gs_; }
    const combat::CombatSession &combatSession() const { return combat_; }

    /** Current map screen label (town name, D2, Luxus Palace, …) — not home town. */
    const char *currentScreenLabel() const;
    int currentScreenId() const { return world_.currentScreen(); }

private:
    enum class PlayOverlay : uint8_t {
        None,
        QuickRef,
        CharacterSheet,
        QuitConfirm,
        Controls,
        StatusMessage,
        Automap,
        TownService,
        RestConfirm,
        SearchIdentify, /* 0x1B3F2 '1'..'4' after long-path rating */
        ExchangeOrder,  /* 0x20F58: Exchange (1-N) / with (1-N) */
        DismissHireling, /* 0x141F4: Dismiss whom (1-N)?; hireling only */
        UnlockWho,      /* 0x1AE2E: who tries unlock (1-N) */
        GameOver,       /* party wipe — no living members; return to title */
        DeathStrikes,   /* 0x14106 OP_0E FD abort==3 panel; ENTER → Goto Town */
        FdPrintChrome,  /* 0x1493C PTR0 / post-fight pages / WAFE name entry */
    };

    static const char *townName(uint8_t town_filter);

    void renderFrame(bool overlay_anim_only);
#if MM2_HOST_AMIGA
    bool canUsePartialView3DRefresh() const;
    void renderFrameView3DOnly();
    void renderFrameOverlayAnimOnly();
    void renderFrameCombatAnimOnly();
    /** ASM combat HUD only: 0x129CC roster + 0x12848 party + 0x1119E message band.
     *  Does not wipe combat chrome / rebuild the hood (0x135BE is once per fight). */
    void renderFrameCombatHudOnly();
    void renderFrameTextOnly();
#endif
    void renderView3D();
    void renderCombatBackdrop();
    void renderIndoorView3D();
    /** Floor/sky/walls only — torch cels are blitted after viewport-cache save. */
    void renderIndoorView3DBase();
    void blitIndoorTorches();
    void refreshHoodTorchCache();
    void renderOutdoorView();
    void renderPartyPanel();
    void refreshCombatSprites();
    void blitCombatSprites();
    void unloadCombatSprites();
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
    /** 0x20F58 explore Exchange: begin two-digit party-order prompt. */
    void beginExchangeOrder();
    /** Swap launch_ + GS -$796A[a/b] (0x20F94..0x20FD0). */
    void exchangeExplorePartySlots(int slot_a, int slot_b);
    /** 0x141F4: begin Dismiss whom prompt. */
    void beginDismissHireling();
    /** 0x362C: remove roster index from -$796A, compact, dec -$795A. */
    bool removeHirelingFromParty(int16_t roster_index);
    /** 0x171AC / -$7D40 GS: clear -$7950 after explore modal epilogue. */
    void applyExitFlagCleanup();
    /** 0x20D26+: finish unlock after 0x1AE2E party-slot pick. */
    void finishUnlockWithPartySlot(int party_slot);
    /* Rest execution @ 0x19E20 -> 0x19AD6 (pay) / 0x19D64 (encounter) / 0x19B28
     * (heal + 0x55-tick advance), run after the Y/N confirm overlay resolves. */
    void executeRest();
    /* Random step encounter @ 0x10A2, simplified gate: outdoor, non-town,
     * same-screen forward step; rate byte = attrib.dat 0x09 (doc 35). */
    void maybeTriggerStepEncounter();
    void finishCombat();
    /** save_game_state @ 0x823C mirror: sync live GS words into the roster.dat
     *  global tail (party -$796A/-$795A, event bank -$798B = hireling A..X
     *  unlocks) and write the file. */
    void saveRosterWithGlobalTail();
    /** True when every party slot has (condition & 0xE0) != 0 — roster_count_living @ 0x47A2. */
    bool partyAllDead() const;
    /** Total wipe: funeral prompt, do not persist corpses, then title on dismiss. */
    void beginPartyWipeGameOver();
    /** If the party is fully dead and not already on GameOver, start the funeral. */
    void maybeBeginPartyWipeGameOver();
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
    void maybeOpenDeathStrikes();
    void maybeOpenFdPrintChrome();
    void loadFdPrintChromePage();
    void startOp0eFdEncounter();
    void resumeOp0eFdAfterCombat();
    bool op0eFdPasswordOk(const char *typed) const;
    void ensureEndgameArtLoaded();
    void armSearchContainerArt();
    void clearSearchContainerArt();
    void showStatusMessage(const char *msg);
    void markDirty();
#if MM2_HOST_AMIGA
    void markView3DDirty();
    void markTextDirty();
    void markOverlayAnimDirty();
    /** Combat message/roster/party refresh — retail is surgical, not full-frame. */
    void markCombatHudDirty();
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
    gfx::ViewportAnmOverlay combat_sprites_[gfx::kAgaCombatSpriteCap]{};
    int combat_sprite_disks_[gfx::kAgaCombatSpriteCap] = {-1, -1, -1, -1};
    int combat_sprite_stacks_[gfx::kAgaCombatSpriteCap]{};
    int combat_sprite_count_ = 0;
#if MM2_HOST_AMIGA
    bool combat_backdrop_cached_ = false;
    bool combat_backdrop_round_layout_ = false; /* layout the cache was rendered with */
    /** Last HUD-only draw signatures — skip roster/party when only message band changed. */
    uint32_t combat_hud_roster_sig_ = 0;
    uint32_t combat_hud_party_sig_ = 0;
    bool combat_hud_sigs_valid_ = false;
#endif

    world::MapWorld world_;
    world::AutomapState automap_;
    gfx::EnvAssets env_;
    gameplay::InGameCharacterSheet ingame_sheet_;
    gameplay::InGameControlsScreen controls_screen_;
    gameplay::SheetSession sheet_session_{};

    PlayOverlay overlay_ = PlayOverlay::None;
    char status_message_[320] = {};
    /* 0x1493C stages: 0=PTR0 1=await combat 2=vacuum+PTR1 3=WAFE entry
     * 4=thank-you 5=PTR2+3 6=PTR4+5 → Goto Town. */
    int fd_print_stage_ = 0;
    bool fd_await_combat_ = false;
    char fd_name_buf_[11] = {};
    int fd_name_len_ = 0;
    /** Long-path Search Identify @ 0x1B3F2: rating from 0x1B270; phase for member pick. */
    uint8_t search_identify_rating_ = 0;
    char search_identify_container_[24]{};
    bool search_identify_pick_member_ = false;
    bool search_identify_find_traps_ = false;
    /** 0x1B2CE -$7FC2: container sign sprite (70..74.anm) over viewport. */
    gfx::ViewportAnmOverlay search_container_{};
    /** 0x14EC2 endgame.32 blit @ (x=$20,y=$40) during post-fight FD pages. */
    mm2_image32_file endgame_{};
    bool has_endgame_ = false;
    /** 0x20F58: -1 = pick first slot; else 0-based first awaiting "with". */
    int exchange_first_slot_ = -1;

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

    /** Torch flicker phase (A4-$667A) — advanced each indoor viewport tick. */
    int torch_phase_ = 0;
    int torch_tick_ = 0;
    /** Cached from last move/turn: skip torch redraw when the hood has no torch walls. */
    bool hood_has_torches_ = false;
    bool hood_torch_cache_valid_ = false;

    /** Last platform::nowTicks() sample for .anm overlay pacing (VBlank clock). */
    uint32_t overlay_anim_clock_ = 0;
    bool overlay_anim_clock_valid_ = false;

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
