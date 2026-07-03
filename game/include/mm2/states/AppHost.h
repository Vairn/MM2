#pragma once



#include "mm2/CppStdCompat.h"

#include "mm2/GameSession.h"

#include "mm2/TitleScreen.h"

#include "mm2/ui/CharacterUiController.h"

#include "mm2/ui/CharacterUiKind.h"

#include "mm2_party_launch.h"



namespace mm2 {



/** Heap singleton — one flat entry point per frame for ACE state cbLoop callbacks. */

class AppHost {

public:

    static AppHost &get();
    /** Placement-new into mm2_app_host_storage (freestanding — no static ctor). */
    static AppHost &createInStorage(unsigned char *storage);
    static void bindInstance(AppHost *host);



    bool start(const char *data_dir, ui::CharacterUiKind ui_kind);

    void shutdown();



    void pollInput();

    void framePresent();

    bool shouldQuit() const { return quit_; }
    void requestQuit() { quit_ = true; }



    TitleScreen &title() { return title_; }

    const TitleScreen &title() const { return title_; }

    ui::CharacterUiController &characterUi() { return character_ui_; }

    GameSession &session() { return session_; }



    TitleScreen::BootOutcome bootStep();
    void bootLogoDraw();
    void bootBeginLogoFade();
    bool bootIsFading() const { return boot_fade_active_; }

    bool attachCharacterUi();



    void logoEnter();

    TitleScreen::LogoAdvance logoStep();

    void logoDraw();



    void attractEnter();

    TitleScreen::AttractAdvance attractStep();

    void attractDraw();



    void menuEnter();

    void menuResume();

    void menuSuspend();

    TitleScreen::MenuAdvance menuStep();

    void menuDraw();



    void controlsEnter();

    bool controlsStep();

    void controlsDraw();

    void optionsEnter();

    bool optionsStep();

    void optionsDraw();



    void charViewEnter();

    void charCreateEnter();

    void charChooseEnter();

    void charDestroy();

    void charPopCleanup();

    void charStep();

    void charDraw();

    /** Create-character: throw.32 load + beginCreateCharacter staged across frames (4K stack). */
    bool charCreatePendingStart() const { return char_create_booting_ || !char_create_prepare_done_; }



    bool takePendingInTown();

    bool startInTownFromPending();



    void inTownStep();

    void inTownDraw();

    bool inTownRequestTitle() const { return session_.requestTitle(); }
    bool inTownRequestGotoTown() const { return session_.requestGotoTown(); }

    void syncSessionRosterToTitle();
    void inTownEnd();
    void inTownEndForGotoTown();
    uint8_t pendingChooseTownFilter() const { return pending_choose_town_filter_; }



    const platform::KeyState &keys() const { return keys_; }



private:
    void refreshPlayWindowTitle();

    AppHost() = default;

    static AppHost *s_instance_;

    const char *data_dir_ = nullptr;

    ui::CharacterUiKind ui_kind_ = ui::CharacterUiKind::AmigaClassic;

    platform::KeyState keys_{};

    bool quit_ = false;

    bool character_ui_ready_ = false;

    bool char_create_prepare_done_ = false;
    bool char_create_booting_ = false;

    bool boot_done_ = false;
    bool boot_fade_active_ = false;
    bool pending_in_town_ = false;

    bool in_town_active_ = false;
    uint8_t pending_choose_town_filter_ = 1;
    /** Set when inTownDraw rebuilt the playfield this frame (Amiga DB swap gate). */
    bool in_town_redrew_ = false;
    int play_title_screen_ = -1;

    TitleScreen::BootOutcome boot_outcome_ = TitleScreen::BootOutcome::Working;



    TitleScreen title_;

    ui::CharacterUiController character_ui_;

    GameSession session_;

    Mm2PartyLaunch pending_launch_{};

};



}  // namespace mm2



#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char mm2_app_host_storage[];

int mm2_app_run(const char *data_dir, int ui_kind);



#ifdef __cplusplus

}

#endif

