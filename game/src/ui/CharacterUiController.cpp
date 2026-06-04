#include "mm2/ui/CharacterUiController.h"

#include "mm2/ui/CharacterUiFactory.h"

namespace mm2::ui {

bool CharacterUiController::init(const char *data_dir, CharacterUiKind kind)
{
    ui_ = createCharacterUi(kind);
    if (!ui_) {
        return false;
    }
    return ui_->init(data_dir);
}

void CharacterUiController::shutdown()
{
    if (ui_) {
        ui_->shutdown();
        ui_.reset();
    }
    mode_ = CharacterUiMode::None;
    roster_ = nullptr;
}

void CharacterUiController::startViewParty(Mm2RosterFile &roster)
{
    roster_ = &roster;
    mode_ = CharacterUiMode::ViewParty;
    quit_requested_ = false;
    ui_->beginViewParty(roster);
}

void CharacterUiController::startChooseParty(Mm2RosterFile &roster)
{
    roster_ = &roster;
    mode_ = CharacterUiMode::ChooseParty;
    quit_requested_ = false;
    party_launch_ready_ = false;
    ui_->beginChooseParty(roster);
}

void CharacterUiController::startCreateCharacter(Mm2RosterFile &roster, int slot)
{
    roster_ = &roster;
    create_slot_ = slot;
    mode_ = CharacterUiMode::CreateCharacter;
    quit_requested_ = false;
    ui_->beginCreateCharacter(roster, slot);
}

void CharacterUiController::tick(const platform::KeyState &keys)
{
    if (!ui_ || mode_ == CharacterUiMode::None) {
        return;
    }

    UiResult result = UiResult::Continue;
    if (mode_ == CharacterUiMode::ViewParty) {
        result = ui_->tickViewParty(keys);
    } else if (mode_ == CharacterUiMode::CreateCharacter) {
        result = ui_->tickCreateCharacter(keys);
    } else if (mode_ == CharacterUiMode::ChooseParty) {
        result = ui_->tickChooseParty(keys);
    }

    if (result == UiResult::Quit) {
        quit_requested_ = true;
        mode_ = CharacterUiMode::None;
    } else if (result == UiResult::Done) {
        if (mode_ == CharacterUiMode::ChooseParty) {
            Mm2PartyLaunch launch{};
            if (ui_->takePartyLaunch(&launch)) {
                party_launch_ = launch;
                party_launch_ready_ = true;
            }
        }
        mode_ = CharacterUiMode::None;
    } else if (result == UiResult::Cancel) {
        mode_ = CharacterUiMode::None;
    }
}

bool CharacterUiController::takePartyLaunch(Mm2PartyLaunch *out)
{
    if (!party_launch_ready_ || !out) {
        return false;
    }
    *out = party_launch_;
    party_launch_ready_ = false;
    return true;
}

void CharacterUiController::render(gfx::ScreenCompositor &compositor)
{
    if (!ui_ || mode_ == CharacterUiMode::None) {
        return;
    }
    if (mode_ == CharacterUiMode::ViewParty) {
        ui_->renderViewParty(compositor);
    } else if (mode_ == CharacterUiMode::CreateCharacter) {
        ui_->renderCreateCharacter(compositor);
    } else if (mode_ == CharacterUiMode::ChooseParty) {
        ui_->renderChooseParty(compositor);
    }
}

}  // namespace mm2::ui
