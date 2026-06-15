#pragma once
#include "mm2/CppStdCompat.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/CharacterUiKind.h"
#include "mm2_party_launch.h"
#include "mm2_image32_codec.h"
#include "mm2_roster_codec.h"

namespace mm2 {

/** Per-screen advance results for the ACE state machine (no nested TitleState). */
class TitleScreen {
public:
    enum class BootOutcome {
        Working,
        Failed,
        ReadyFade,
        ReadyAttract,
    };

    enum class LogoAdvance {
        Continue,
        GoAttract,
        GoMenu,
    };

    enum class AttractAdvance {
        Continue,
        GoMenu,
    };

    enum class MenuAdvance {
        None,
        Quit,
        Attract,
        Controls,
        Options,
        ViewParty,
        CreateCharacter,
        ChooseParty,
    };

    enum class LogoPhase {
        Static,
        FadeIn,
        Hold,
        FadeOut,
        Done,
    };

    bool bootPrepare(const char *data_dir, ui::CharacterUiKind ui_kind);
    BootOutcome bootAdvance();
    /** Full-opacity nwcp while boot loads remaining assets (before fade sequence). */
    void bootLogoDraw();
    void bootBeginLogoFade();
    void shutdown();

    void logoEnter();
    LogoAdvance logoTick(const platform::KeyState &keys);
    void logoDraw();

    void attractEnter();
    AttractAdvance attractTick(const platform::KeyState &keys);
    void attractDraw();

    void menuEnter();
    void menuResume();
    void menuSuspend();
    MenuAdvance menuTick(const platform::KeyState &keys);
    void menuDraw();

    bool controlsTick(const platform::KeyState &keys);
    void controlsDraw();
    void controlsEnter();
    bool optionsTick(const platform::KeyState &keys);
    void optionsDraw();
    void optionsEnter();

    void returnToMenu();

#if MM2_HOST_AMIGA
    /** Drop title/attract CHIP bitmaps before play-mode load (4K-stack / CHIP budget). */
    void releaseChipForPlayMode();
#endif

    bool shouldQuit() const { return quit_; }
    const gfx::ScreenCompositor &compositor() const { return compositor_; }
    gfx::ScreenCompositor &compositor() { return compositor_; }
    Mm2RosterFile &roster() { return roster_; }
    const Mm2RosterFile &roster() const { return roster_; }
    bool hasRoster() const { return has_roster_; }
    bool hasLogo() const { return has_nwcp_; }

#if MM2_HOST_AMIGA
    bool consumeItemsDat(uint8_t **out, std::size_t *out_size);
#endif

private:
    enum class BootStep {
        UiCache,
        Nwcp,
        Intro,
        IntroClips,
        Book,
        Roster,
        Items,
        MenuCache,
        Done,
    };

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
    /** Drop intro.32 / introclips.32 / nwcp.32 once the interactive menu is shown. */
    void releaseAttractAssets();
    void ensureAttractAssetsLoaded();
    void drawTitleMenu();
    void buildTitleMenuCache();
    void presentTitleMenu();
    void blitTitleMenuBooks();
    void drawControls();
    void drawOptions();
    void buildControlsCache();
    void buildOptionsCache();
    void tickBookAnimation();
    void skipLogoToAttract();
    void skipLogoToMenu();

    bool loadItemsDat();
    void releaseItemsDat();

#if MM2_HOST_AMIGA
    void invalidatePegasusPaint();
    void invalidateTitleMenuPaint();
#endif

    const char *data_dir_ = nullptr;
    ui::CharacterUiKind ui_kind_ = ui::CharacterUiKind::AmigaClassic;
    BootStep boot_step_ = BootStep::UiCache;
    bool quit_ = false;
    LogoPhase logo_phase_ = LogoPhase::FadeIn;

    gfx::ScreenCompositor compositor_;

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

#if MM2_HOST_AMIGA
    uint8_t *items_dat_ = nullptr;
    std::size_t items_dat_size_ = 0;
    bool has_items_dat_ = false;
#endif

    uint32_t overlay_until_ = 0;
    int overlay_phase_ = 0;
    uint32_t peeker_until_ = 0;
    int peeker_delay_ticks_ = 0;
    int peeker_slot_ = 0;
    bool peeker_visible_ = false;
    uint32_t peeker_rng_ = 1;

    int book_frame_ = 0;
    uint32_t book_until_ = 0;

    uint32_t logo_hold_until_ = 0;
    int state_ticks_ = 0;
    uint8_t logo_alpha_ = 0;
    int logo_splash_x_ = 10;
    bool logo_fade_armed_ = false;
    bool logo_fade_out_armed_ = false;

#if MM2_HOST_AMIGA
    int pegasus_painted_overlay_phase_ = -1;
    int pegasus_painted_peeker_slot_ = -1;
    bool pegasus_painted_peeker_visible_ = false;
    bool title_menu_painted_ = false;
    int title_menu_painted_book_frame_ = -1;
    bool controls_subscreen_painted_ = false;
    bool options_subscreen_painted_ = false;
#endif
};

}  // namespace mm2
