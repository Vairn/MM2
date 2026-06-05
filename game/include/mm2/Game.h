#pragma once

#include "mm2/GameSession.h"
#include "mm2/TitleScreen.h"
#include "mm2/ui/CharacterUiKind.h"

namespace mm2 {

class Game {
public:
    bool init(const char *data_dir, ui::CharacterUiKind ui_kind = ui::CharacterUiKind::AmigaClassic);
    void shutdown();
    void tick();
    bool shouldQuit() const { return quit_; }

private:
    enum class Phase { Title, InTown };

    const char *data_dir_ = nullptr;
    bool quit_ = false;
    Phase phase_ = Phase::Title;
    bool pending_in_town_start_ = false;
    Mm2PartyLaunch pending_launch_{};

    TitleScreen title_;
    GameSession session_;
};

}  // namespace mm2
