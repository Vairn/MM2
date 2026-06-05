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
    /** Loads character UI backend on first C/V/G (not at title boot). */
    bool ensureCharacterUi();

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
    void resetAttractTiming();
    void pickRandomPeekerSlot();
    void releaseIntroClips();
    void releaseLogoAsset();
    void drawTitleMenu();
    void buildTitleMenuCache();
    void presentTitleMenu();
    void blitTitleMenuBooks();
    void drawControls();
    void drawOptions();
    void tickBookAnimation();
    bool isLogoState() const;
    void skipLogoToTitle();
    void setState(TitleState next, const char *reason);
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    void dbgLogDisplay() const;
#endif

    const char *data_dir_ = nullptr;
    ui::CharacterUiKind ui_kind_ = ui::CharacterUiKind::AmigaClassic;
    bool character_ui_ready_ = false;
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

    // Animation timing is scheduled against platform::nowTicks() (VBlank frame clock),
    // not per-loop-iteration counters, so speed tracks the 50/60 Hz display like LoL.
    uint32_t overlay_until_ = 0;  // next pegasus-overlay phase deadline (frame ticks)
    int overlay_phase_ = 0;       // 0..4 pegasus overlays (ASM A4-$647A mod 5 @ 0x26A1E)
    uint32_t peeker_until_ = 0;   // next peeker show/hide deadline (frame ticks)
    int peeker_delay_ticks_ = 0;  // visible duration for the current peeker
    int peeker_slot_ = 0;
    bool peeker_visible_ = false;
    uint32_t peeker_rng_ = 1;

    int book_frame_ = 0;
    uint32_t book_until_ = 0;  // next book page-turn deadline (frame ticks)

    uint32_t logo_hold_until_ = 0;  // Amiga logo-splash hold deadline (frame ticks)
    int state_ticks_ = 0;
    uint8_t logo_alpha_ = 0;
    int logo_splash_x_ = 10;

#if MM2_HOST_AMIGA
    /* Last state painted into ACE pBack (retail retains FB between overlay steps). */
    int pegasus_painted_overlay_phase_ = -1;
    int pegasus_painted_peeker_slot_ = -1;
    bool pegasus_painted_peeker_visible_ = false;
    void invalidatePegasusPaint();

    bool title_menu_painted_ = false;
    int title_menu_painted_book_frame_ = -1;
    void invalidateTitleMenuPaint();
#endif

};

}  // namespace mm2
