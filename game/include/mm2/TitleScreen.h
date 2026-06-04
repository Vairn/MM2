#pragma once

#include "mm2/CppStdCompat.h"

#include "mm2/gfx/ScreenCompositor.h"

#include "mm2/platform/Platform.h"

#include "mm2/ui/CharacterUiController.h"

#include "mm2/ui/CharacterUiKind.h"

#include "mm2_party_launch.h"
#include "mm2_image32_codec.h"



namespace mm2 {



enum class TitleState {

    LogoFadeIn,

    LogoHold,

    LogoFadeOut,

    TitleAttract,  // intro.32 + pegasus overlay anim; any key -> menu

    TitleMenu,     // frozen intro composite + book menu (ASM 0x1062)

    Controls,

    Options,

    CharacterUi,

};



class TitleScreen {

public:

    bool init(const char *data_dir, ui::CharacterUiKind ui_kind = ui::CharacterUiKind::AmigaClassic);

    void shutdown();



    void tick(const platform::KeyState &keys);

    void render();



    bool shouldQuit() const { return quit_; }
    const gfx::ScreenCompositor &compositor() const { return compositor_; }
    const Mm2RosterFile &roster() const { return roster_; }

    bool takePartyLaunch(Mm2PartyLaunch *out);
    void returnToMenu();

private:

    bool loadImage(const char *name, mm2_image32_file *out);

    void drawLogoSplash();

    void drawIntroPegasus(bool animate_overlays);

    void drawAttract();

    void blitIntroClipFrame(int frame_index, int x, int y);

    void tickPegasusAnimation();

    void pickRandomPeekerSlot();

    void releaseIntroClips();

    void drawTitleMenu();
    void drawControls();
    void drawOptions();

    void tickBookAnimation();

    bool isLogoState() const;

    void skipLogoToTitle();



    const char *data_dir_ = nullptr;

    bool quit_ = false;

    TitleState state_ = TitleState::LogoFadeIn;



    gfx::ScreenCompositor compositor_;

    ui::CharacterUiController character_ui_;



    mm2_image32_file intro_{};

    mm2_image32_file introclips_{};

    mm2_image32_file nwcp_{};

    mm2_image32_file book_{};

    Mm2RosterFile roster_{};



    bool has_intro_ = false;

    bool has_introclips_ = false;

    bool has_nwcp_ = false;

    bool has_book_ = false;

    bool has_roster_ = false;



    int overlay_gate_ = 0;

    int overlay_phase_ = 0;  // 0..4 pegasus overlays (ASM A4-$647A mod 5 @ 0x26A1E)

    int peeker_gate_ = 0;

    int peeker_gap_ticks_ = 0;

    int peeker_delay_ticks_ = 0;

    int peeker_slot_ = 0;

    bool peeker_visible_ = false;

    uint32_t peeker_rng_ = 1;



    int book_frame_ = 0;

    int book_gate_ = 0;



    int state_ticks_ = 0;

    uint8_t logo_alpha_ = 0;

    int logo_splash_x_ = 10;

};



}  // namespace mm2

