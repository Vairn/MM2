#include "mm2/states/AppHost.h"

#include "mm2/Config.h"
#include "mm2/Mm2Dbg.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/platform/Platform.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#if defined(ACE_DEBUG)
#include <ace/managers/memory.h>
#endif
#endif

#if !MM2_HOST_AMIGA
#include <cstdio>
#endif

alignas(mm2::AppHost) unsigned char mm2_app_host_storage[sizeof(mm2::AppHost)];

namespace mm2 {

AppHost *AppHost::s_instance_ = nullptr;

AppHost &AppHost::get()
{
    return *s_instance_;
}

void AppHost::bindInstance(AppHost *host)
{
    s_instance_ = host;
}

AppHost &AppHost::createInStorage(unsigned char *storage)
{
    s_instance_ = new (storage) AppHost();
    return *s_instance_;
}

bool AppHost::start(const char *data_dir, ui::CharacterUiKind ui_kind)
{
    data_dir_ = data_dir;
    ui_kind_ = ui_kind;
    quit_ = false;
    character_ui_ready_ = false;
    char_create_prepare_done_ = false;
    pending_in_town_ = false;
    boot_done_ = false;
    title_.bootPrepare(data_dir, ui_kind);

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

void AppHost::shutdown()
{
    if (character_ui_ready_) {
        character_ui_.shutdown();
        character_ui_ready_ = false;
    }
    session_.shutdown();
    title_.shutdown();
}

void AppHost::pollInput() { keys_ = platform::pollInput(); }

void AppHost::framePresent()
{
    /* In-town frames live in the session compositor; everything else draws
     * into the title compositor. */
    const gfx::ScreenCompositor &c = in_town_active_ ? session_.compositor() : title_.compositor();
#if MM2_HOST_AMIGA
    if (in_town_active_ && !in_town_redrew_) {
        /* Idle play screen: pace to vblank without swapping DB buffers (avoids
         * showing stale back-buffer and skips redundant full-screen blits). */
        platform::waitVblank();
        return;
    }
#endif
    platform::presentFrame(c.pixels(), c.width(), c.height());
#if !MM2_HOST_AMIGA
    platform::delayMs(16);
#endif
}

void AppHost::bootLogoDraw() { title_.bootLogoDraw(); }

void AppHost::bootBeginLogoFade()
{
    boot_fade_active_ = true;
    title_.bootBeginLogoFade();
}

TitleScreen::BootOutcome AppHost::bootStep()
{
    if (boot_done_) {
        return boot_outcome_;
    }

    const TitleScreen::BootOutcome outcome = title_.bootAdvance();
    if (outcome == TitleScreen::BootOutcome::Working || outcome == TitleScreen::BootOutcome::Failed) {
        return outcome;
    }

#if MM2_HOST_AMIGA
    if (!character_ui_ready_ && !attachCharacterUi()) {
        boot_done_ = true;
        boot_outcome_ = TitleScreen::BootOutcome::Failed;
        return boot_outcome_;
    }
#endif

    boot_done_ = true;
    boot_outcome_ = outcome;
    return boot_outcome_;
}

bool AppHost::attachCharacterUi()
{
    if (character_ui_ready_) {
        return true;
    }
#if MM2_HOST_AMIGA
    uint8_t *items = nullptr;
    std::size_t size = 0;
    if (!title_.consumeItemsDat(&items, &size)) {
        return false;
    }
    if (!character_ui_.attachAndLoad(data_dir_, ui_kind_, items, size)) {
        return false;
    }
#else
    if (!character_ui_.attachAndLoad(data_dir_, ui_kind_)) {
        return false;
    }
#endif
    character_ui_ready_ = true;
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    MM2_DBG("MM2 DBG: Character UI backend ready, chip free %lu\n", (unsigned long)memGetFreeChipSize());
#endif
    return true;
}

void AppHost::logoEnter() { title_.logoEnter(); }

TitleScreen::LogoAdvance AppHost::logoStep()
{
    if (keys_.quit) {
        quit_ = true;
    }
    return title_.logoTick(keys_);
}

void AppHost::logoDraw() { title_.logoDraw(); }

void AppHost::attractEnter() { title_.attractEnter(); }

TitleScreen::AttractAdvance AppHost::attractStep()
{
    if (keys_.quit) {
        quit_ = true;
    }
    return title_.attractTick(keys_);
}

void AppHost::attractDraw() { title_.attractDraw(); }

void AppHost::menuEnter() { title_.menuEnter(); }

void AppHost::menuResume() { title_.menuResume(); }

void AppHost::menuSuspend() { title_.menuSuspend(); }

TitleScreen::MenuAdvance AppHost::menuStep()
{
    if (keys_.quit) {
        quit_ = true;
    }
    if (title_.shouldQuit()) {
        quit_ = true;
    }
    return title_.menuTick(keys_);
}

void AppHost::menuDraw() { title_.menuDraw(); }

void AppHost::controlsEnter() {}

bool AppHost::controlsStep()
{
    if (keys_.quit) {
        quit_ = true;
        return false;
    }
    if (title_.shouldQuit()) {
        quit_ = true;
    }
    return title_.controlsTick(keys_);
}

void AppHost::controlsDraw() { title_.controlsDraw(); }

void AppHost::optionsEnter() {}

bool AppHost::optionsStep()
{
    if (keys_.quit) {
        quit_ = true;
        return false;
    }
    if (title_.shouldQuit()) {
        quit_ = true;
    }
    return title_.optionsTick(keys_);
}

void AppHost::optionsDraw() { title_.optionsDraw(); }

void AppHost::charViewEnter()
{
#if MM2_HOST_AMIGA
    platform::clearScreen();
    platform::applyUiPalette();
#endif
    if (!character_ui_ready_ && !attachCharacterUi()) {
        return;
    }
    char_create_prepare_done_ = true;
    character_ui_.startViewParty(title_.roster());
}

void AppHost::charCreateEnter()
{
#if MM2_HOST_AMIGA
    platform::clearScreen();
    platform::applyUiPalette();
#endif
    char_create_prepare_done_ = false;
}

void AppHost::charChooseEnter()
{
#if MM2_HOST_AMIGA
    platform::clearScreen();
    mm2_amiga_blit_sync();
    mm2_amiga_ui_cache_invalidate();
    platform::applyUiPalette();
#endif
    if (!character_ui_ready_ && !attachCharacterUi()) {
        return;
    }
    char_create_prepare_done_ = true;
    character_ui_.startChooseParty(title_.roster());
}

void AppHost::charDestroy()
{
#if MM2_HOST_AMIGA
    character_ui_.leaveMode();
#endif
    char_create_prepare_done_ = false;
    pending_in_town_ = false;
}

void AppHost::charPopCleanup()
{
    charDestroy();
    title_.returnToMenu();
}

void AppHost::charStep()
{
#if !MM2_HOST_AMIGA
    if (!character_ui_ready_ && !attachCharacterUi()) {
        return;
    }
#endif
    if (!character_ui_ready_) {
        return;
    }

    if (!char_create_prepare_done_) {
        char_create_prepare_done_ = true;
        character_ui_.prepareCreateCharacterAssets();
        character_ui_.startCreateCharacter(title_.roster());
        return;
    }

    character_ui_.tick(keys_);
    if (!character_ui_.active()) {
        if (character_ui_.quitRequested()) {
            quit_ = true;
            return;
        }
        Mm2PartyLaunch launch{};
        if (character_ui_.takePartyLaunch(&launch)) {
            pending_launch_ = launch;
            pending_in_town_ = true;
        }
    }
}

void AppHost::charDraw()
{
#if MM2_HOST_AMIGA
    /* Drain blits from menuSuspend/clearScreen — Release can outrun the ACE blitter. */
    mm2_amiga_blit_sync();
    if (character_ui_.needsRedraw()) {
        platform::applyUiPalette();
        mm2_amiga_ui_cache_begin();
        if (character_ui_ready_) {
            character_ui_.render(title_.compositor());
        }
        mm2_amiga_ui_cache_end();
        character_ui_.ackRedraw();
    }
    /* Always composite cached UI to pBack before buffer swap (title menu does the same). */
    mm2_amiga_blit_sync();
    platform::applyUiPalette();
    mm2_amiga_ui_cache_present();
#else
    if (character_ui_ready_ && character_ui_.needsRedraw()) {
        character_ui_.render(title_.compositor());
        character_ui_.ackRedraw();
    }
#endif
}

bool AppHost::takePendingInTown()
{
    if (!pending_in_town_) {
        return false;
    }
    pending_in_town_ = false;
    return true;
}

bool AppHost::startInTownFromPending()
{
#if MM2_HOST_AMIGA
    mm2_amiga_blit_sync();
    title_.releaseChipForPlayMode();
#endif
    if (!session_.start(data_dir_, title_.roster(), pending_launch_)) {
        return false;
    }
    in_town_active_ = true;
#if !MM2_HOST_AMIGA
    char window_title[256];
    std::snprintf(window_title, sizeof(window_title), "MM2 (%s) — %s", platform::hostName(),
                  GameSession::areaName(pending_launch_.area_id));
    platform::setWindowTitle(window_title);
#endif
    return true;
}

void AppHost::inTownStep()
{
#if MM2_HOST_AMIGA
    if (session_.isBootstrapping()) {
        session_.tickBootstrap();
        return;
    }
#endif
    session_.tick(keys_);
    if (session_.shouldQuit()) {
        quit_ = true;
    }
}

void AppHost::inTownDraw()
{
#if MM2_HOST_AMIGA
    mm2_amiga_blit_sync();
    in_town_redrew_ = false;
    if (session_.isBootstrapping()) {
        if (session_.needsRedraw()) {
            platform::clearScreen();
            session_.ackRedraw();
            in_town_redrew_ = true;
        }
        return;
    }
    if (session_.needsRedraw()) {
        session_.render();
        session_.ackRedraw();
        in_town_redrew_ = true;
    }
    /* ACE keyUse is edge-triggered; re-poll after the play-screen draw so a short
     * SPACE tap during a slow Corak frame is not lost between loop polls. */
    if (session_.awaitingContinuePrompt()) {
        const platform::KeyState after = platform::pollInput();
        if (after.space || after.enter || after.any_key) {
            session_.tick(after);
            if (session_.shouldQuit()) {
                quit_ = true;
            }
            if (session_.needsRedraw()) {
                session_.render();
                session_.ackRedraw();
                in_town_redrew_ = true;
            }
        }
    }
#else
    if (session_.needsRedraw()) {
        session_.render();
        session_.ackRedraw();
    }
#endif
}

void AppHost::inTownEnd()
{
    in_town_active_ = false;
    /* shutdown() fully resets the session; re-assigning a temporary here
     * would shallow-copy ScreenCompositor's pixel buffer (double free). */
    session_.shutdown();
    title_.returnToMenu();
#if !MM2_HOST_AMIGA
    char window_title[256];
    std::snprintf(window_title, sizeof(window_title), "MM2 (%s) — title menu", platform::hostName());
    platform::setWindowTitle(window_title);
#endif
}

}  // namespace mm2
