#include "mm2/Game.h"
#include "mm2/Config.h"
#include "mm2/CppStdCompat.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/Mm2Dbg.h"
#include "mm2/platform/Platform.h"
#include "mm2_party_launch.h"

namespace mm2 {
bool Game::init(const char *data_dir, ui::CharacterUiKind ui_kind)
{
    data_dir_ = data_dir;
    if (!title_.init(data_dir, ui_kind)) {
        MM2_DBG("MM2 DBG: Game title init FAILED\n");
        title_.shutdown();
        return false;
    }
    MM2_DBG("MM2 DBG: Game title init ok\n");
    constexpr int kScale = 2;
    platform::setPresentScale(kScale);
    platform::setWindowSize(gfx::ScreenCompositor::kWidth * kScale, gfx::ScreenCompositor::kHeight * kScale);

#if !MM2_HOST_AMIGA
    char window_title[256];
    std::snprintf(window_title, sizeof(window_title), "MM2 (%s, ui=%s) — title screen", platform::hostName(),
                  ui::characterUiKindName(ui_kind));
    platform::setWindowTitle(window_title);
#endif
    return true;
}

void Game::shutdown()
{
    session_.shutdown();
    title_.shutdown();
}

void Game::tick()
{
    if (phase_ == Phase::Title) {
        title_.runPendingCharacterUiBootstrap();
        if (pending_in_town_start_) {
            if (session_.start(data_dir_, title_.roster(), pending_launch_)) {
                MM2_DBG("MM2 DBG: Game phase Title -> InTown (area %u)\n", (unsigned)pending_launch_.area_id);
                phase_ = Phase::InTown;
#if !MM2_HOST_AMIGA
                char window_title[256];
                std::snprintf(window_title, sizeof(window_title), "MM2 (%s) — %s", platform::hostName(),
                              GameSession::areaName(pending_launch_.area_id));
                platform::setWindowTitle(window_title);
#endif
            }
            pending_in_town_start_ = false;
        }
    }
    const auto keys = platform::pollInput();
    if (phase_ == Phase::Title) {
        title_.tick(keys);
        if (title_.shouldQuit()) {
            quit_ = true;
            return;
        }

        Mm2PartyLaunch launch{};
        if (title_.takePartyLaunch(&launch)) {
            pending_launch_ = launch;
            pending_in_town_start_ = true;
        }

        title_.render();
        platform::presentFrame(title_.compositor().pixels(), title_.compositor().width(),
                             title_.compositor().height());
    } else {
        session_.tick(keys);
        if (session_.shouldQuit()) {
            quit_ = true;
            return;
        }
        if (session_.requestTitle()) {
            session_.shutdown();
            session_ = GameSession{};
            MM2_DBG("MM2 DBG: Game phase InTown -> Title (menu)\n");
            phase_ = Phase::Title;
            title_.returnToMenu();
#if !MM2_HOST_AMIGA
            char window_title[256];
            std::snprintf(window_title, sizeof(window_title), "MM2 (%s) — title menu", platform::hostName());
            platform::setWindowTitle(window_title);
#endif
            session_.clearTitleRequest();
        }

        session_.render();
        platform::presentFrame(session_.compositor().pixels(), session_.compositor().width(),
                             session_.compositor().height());
    }

#if !MM2_HOST_AMIGA
    platform::delayMs(16);
#endif
}

}  // namespace mm2
