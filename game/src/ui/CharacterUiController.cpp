#include "mm2/ui/CharacterUiController.h"

#include "mm2/Mm2Dbg.h"
#include "mm2/ui/CharacterUiFactory.h"
#include "mm2/ui/ICharacterUi.h"

namespace mm2::ui {

bool CharacterUiController::attachAndLoad(const char *data_dir, CharacterUiKind kind, uint8_t *items_dat,
                                          std::size_t items_size)
{
    data_dir_ = data_dir;
    if (!initBackend(kind)) {
        return false;
    }
    if (!ui_->init(data_dir)) {
        shutdown();
        return false;
    }
    if (items_dat && items_size > 0) {
        if (!ui_->adoptItemNames(items_dat, items_size)) {
            shutdown();
            return false;
        }
    } else if (!loadDataFiles()) {
        shutdown();
        return false;
    }
    return true;
}

bool CharacterUiController::initBackend(CharacterUiKind kind)
{
#if MM2_HOST_AMIGA
    ui_ = acquireCharacterUi(kind);
    return ui_ != nullptr;
#else
    ui_ = createCharacterUi(kind);
    return static_cast<bool>(ui_);
#endif
}

bool CharacterUiController::loadDataFiles()
{
    return ui_ && ui_->loadDataFiles();
}

bool CharacterUiController::init(const char *data_dir, CharacterUiKind kind)
{
    return attachAndLoad(data_dir, kind);
}

void CharacterUiController::shutdown()
{
    if (ui_) {
        ui_->shutdown();
#if !MM2_HOST_AMIGA
        ui_.reset();
#endif
        ui_ = nullptr;
    }
    data_dir_ = nullptr;
    mode_ = CharacterUiMode::None;
    roster_ = nullptr;
}

void CharacterUiController::leaveMode()
{
    mode_ = CharacterUiMode::None;
    roster_ = nullptr;
    quit_requested_ = false;
    party_launch_ready_ = false;
}

void CharacterUiController::startViewParty(Mm2RosterFile &roster)
{
    roster_ = &roster;
    mode_ = CharacterUiMode::ViewParty;
    quit_requested_ = false;
    ui_->beginViewParty(roster);
}

void CharacterUiController::startChooseParty(Mm2RosterFile &roster, uint8_t town_filter,
                                             const Mm2PartyLaunch *saved_party)
{
    MM2_DBG("MM2 GOTO: CharacterUiController::startChooseParty town=%u\n", town_filter);
    roster_ = &roster;
    mode_ = CharacterUiMode::ChooseParty;
    quit_requested_ = false;
    party_launch_ready_ = false;
    ui_->beginChooseParty(roster, town_filter, saved_party);
    MM2_DBG("MM2 GOTO: startChooseParty done needsRedraw=%d\n", needsRedraw() ? 1 : 0);
}

void CharacterUiController::startCreateCharacter(Mm2RosterFile &roster, int slot)
{
    roster_ = &roster;
    create_slot_ = slot;
    mode_ = CharacterUiMode::CreateCharacter;
    quit_requested_ = false;
    ui_->beginCreateCharacter(roster, slot);
}

void CharacterUiController::prepareCreateCharacterAssets()
{
    if (ui_) {
        ui_->prepareCreateCharacterAssets();
    }
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

    if (mode_ == CharacterUiMode::ChooseParty && result != UiResult::Continue) {
        MM2_DBG("MM2 GOTO: ChooseParty tick result=%d\n", static_cast<int>(result));
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
                MM2_DBG(
                    "MM2 GOTO: party_launch_ready town=%u members=%u\n",
                    (unsigned)launch.town_filter,
                    (unsigned)launch.party_count
                );
            } else {
                MM2_DBG("MM2 GOTO: ChooseParty Done but takePartyLaunch failed\n");
            }
        }
        mode_ = CharacterUiMode::None;
    } else if (result == UiResult::Cancel) {
        mode_ = CharacterUiMode::None;
    }
}

bool CharacterUiController::needsRedraw() const
{
    return ui_ && ui_->needsRedraw();
}

void CharacterUiController::ackRedraw()
{
    if (ui_) {
        ui_->ackRedraw();
    }
}

bool CharacterUiController::needsIncrementalRedraw() const
{
    return ui_ && ui_->needsIncrementalRedraw();
}

void CharacterUiController::renderIncremental(gfx::ScreenCompositor &compositor)
{
    if (ui_) {
        ui_->renderIncremental(compositor);
    }
}

bool CharacterUiController::takeIncrementalPresentRect(int *out_x, int *out_y, int *out_w, int *out_h)
{
    return ui_ && ui_->takeIncrementalPresentRect(out_x, out_y, out_w, out_h);
}

bool CharacterUiController::takePartyLaunch(Mm2PartyLaunch *out)
{
    if (!party_launch_ready_ || !out) {
        MM2_DBG("MM2 GOTO: takePartyLaunch miss ready=%d out=%p\n", party_launch_ready_ ? 1 : 0,
                static_cast<void *>(out));
        return false;
    }
    *out = party_launch_;
    party_launch_ready_ = false;
    MM2_DBG("MM2 GOTO: takePartyLaunch ok\n");
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
