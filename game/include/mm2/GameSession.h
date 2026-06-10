#pragma once

#include "mm2/GameState.h"

#include "mm2/gameplay/InGameCharacterSheet.h"
#include "mm2/gameplay/InGameControlsScreen.h"
#include "mm2/gameplay/PlaySessionInput.h"

#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/View3D.h"
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

    bool start(const char *data_dir, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch);
    void shutdown();

    void tick(const platform::KeyState &keys);
    void render();

    bool shouldQuit() const { return quit_; }
    bool requestTitle() const { return back_to_title_; }
    void clearTitleRequest() { back_to_title_ = false; }

    const gfx::ScreenCompositor &compositor() const { return compositor_; }

private:
    enum class PlayOverlay : uint8_t { None, QuickRef, CharacterSheet, QuitConfirm, Controls, StatusMessage };

    static const char *townName(uint8_t town_filter);

    void renderView3D();
    void renderPartyPanel();
    void renderOverlays();
    void refreshWorldAfterMove(const gameplay::MoveResult &move);
    void tickOverlayInput(const platform::KeyState &keys);
    void tickPlayInput(const platform::KeyState &keys);
    void handleExploreCommand(gameplay::PlaySessionAction action);
    bool overlayBlocksInput() const;
    void showStatusMessage(const char *msg);
    gfx::PlayProtectValues protectValues() const;

    const char *data_dir_ = nullptr;
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
};

}  // namespace mm2
