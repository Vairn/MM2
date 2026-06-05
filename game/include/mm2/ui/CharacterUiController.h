#pragma once

#include "mm2/CppStdCompat.h"

#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/CharacterUiKind.h"
#include "mm2/ui/ICharacterUi.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::ui {

enum class CharacterUiMode {
    None,
    ViewParty,
    CreateCharacter,
    ChooseParty,
};

class CharacterUiController {
public:
    bool attachAndLoad(const char *data_dir, CharacterUiKind kind, uint8_t *items_dat = nullptr,
                       std::size_t items_size = 0);
    bool initBackend(CharacterUiKind kind);
    bool loadDataFiles();
    bool init(const char *data_dir, CharacterUiKind kind);
    void shutdown();

    void startViewParty(Mm2RosterFile &roster);
    void startCreateCharacter(Mm2RosterFile &roster, int slot = -1);
    void prepareCreateCharacterAssets();
    void startChooseParty(Mm2RosterFile &roster);

    CharacterUiMode mode() const { return mode_; }
    bool active() const { return mode_ != CharacterUiMode::None; }

    // Set when the choose-party screen requested 'ESC' (exit game). Cleared on
    // the next start*().
    bool quitRequested() const { return quit_requested_; }

    void tick(const platform::KeyState &keys);
    void render(gfx::ScreenCompositor &compositor);

    bool takePartyLaunch(Mm2PartyLaunch *out);

private:
#if MM2_HOST_AMIGA
    ICharacterUi *ui_ = nullptr;
#else
    std::unique_ptr<ICharacterUi> ui_;
#endif
    const char *data_dir_ = nullptr;
    CharacterUiMode mode_ = CharacterUiMode::None;
    Mm2RosterFile *roster_ = nullptr;
    int create_slot_ = -1;
    bool quit_requested_ = false;
    bool party_launch_ready_ = false;
    Mm2PartyLaunch party_launch_{};
};

}  // namespace mm2::ui
