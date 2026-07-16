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

namespace {

#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
unsigned long apphost_chip_free() { return (unsigned long)memGetFreeChipSize(); }
#else
unsigned long apphost_chip_free() { return 0; }
#endif

}  // namespace

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

void AppHost::pollInput()
{
    keys_ = platform::pollInput();
    /* Window close / OS quit is a global signal: honor it from any state
     * (boot, title, menus, in-town play, combat) so the loop always exits.
     * In-town play (GameSession::tick) does not inspect keys.quit itself. */
    if (keys_.quit) {
        quit_ = true;
    }
}

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
    if (!in_town_active_ && character_ui_ready_ && character_ui_.active() && !char_ui_redrew_) {
        /* Idle character chooser / create screens — same gate as play. */
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
        MM2_DBG("MM2 GOTO: attachCharacterUi FAIL consumeItemsDat\n");
        return false;
    }
    if (!character_ui_.attachAndLoad(data_dir_, ui_kind_, items, size)) {
        MM2_DBG("MM2 GOTO: attachCharacterUi FAIL attachAndLoad\n");
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

void AppHost::controlsEnter() { title_.controlsEnter(); }

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

void AppHost::optionsEnter() { title_.optionsEnter(); }

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
    mm2_amiga_blit_sync();
    mm2_amiga_ui_cache_invalidate();
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
    mm2_amiga_blit_sync();
    mm2_amiga_ui_cache_invalidate();
    platform::clearScreen();
    platform::applyUiPalette();
#endif
    char_create_prepare_done_ = false;
    char_create_booting_ = true;
}

void AppHost::charChooseEnter()
{
    MM2_DBG("MM2 GOTO: charChooseEnter begin chip=%lu ui_ready=%d\n", apphost_chip_free(),
            character_ui_ready_ ? 1 : 0);
#if MM2_HOST_AMIGA
    MM2_DBG("MM2 GOTO: charChooseEnter blit_sync\n");
    mm2_amiga_blit_sync();
    /* Inn→goto-town destroys the UI cache in releaseChipForPlayMode; recreate. */
    if (!mm2_amiga_ui_cache_ready()) {
        mm2_amiga_ui_cache_create();
    }
    mm2_amiga_ui_cache_invalidate();
    MM2_DBG("MM2 GOTO: charChooseEnter clearScreen\n");
    platform::clearScreen();
    platform::applyUiPalette();
#endif
    if (!character_ui_ready_ && !attachCharacterUi()) {
        MM2_DBG("MM2 GOTO: charChooseEnter ABORT attachCharacterUi failed\n");
        return;
    }
    char_create_prepare_done_ = true;
    char_ui_redrew_ = false;
    MM2_DBG("MM2 GOTO: charChooseEnter startChooseParty\n");
    if (pending_launch_.party_count == 0) {
        restoreSavedPartyFromRosterTail();
    }
    const uint8_t town_filter = pending_choose_town_filter_;
    pending_choose_town_filter_ = 1;
    const Mm2PartyLaunch *saved =
        pending_launch_.party_count > 0 ? &pending_launch_ : nullptr;
    character_ui_.startChooseParty(title_.roster(), town_filter, saved);
    MM2_DBG(
        "MM2 GOTO: charChooseEnter done mode=%d active=%d redraw=%d chip=%lu\n",
        static_cast<int>(character_ui_.mode()),
        character_ui_.active() ? 1 : 0,
        character_ui_.needsRedraw() ? 1 : 0,
        apphost_chip_free()
    );
}

void AppHost::restoreSavedPartyFromRosterTail()
{
    /* Party words in the roster.dat global tail (save_game_state @ 0x823C):
     * 8 x u16 roster indices (-$796A, 0xFFFF = empty) + u16 size (-$795A). */
    Mm2RosterFile &roster = title_.roster();
    const uint16_t saved_size = mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_PARTY_SIZE);
    if (saved_size == 0 || saved_size > MM2_PARTY_LAUNCH_SLOTS) {
        return;
    }
    int members[MM2_PARTY_LAUNCH_SLOTS];
    int count = 0;
    for (int i = 0; i < static_cast<int>(saved_size); ++i) {
        const uint16_t w =
            mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_PARTY_ROSTER_IDX + i * 2);
        if (w >= MM2_ROSTER_TAIL_FIRST_SLOT) {
            continue; /* 0xFFFF empty / out of playable range */
        }
        if (mm2_roster_slot_is_empty(&roster.records[w])) {
            continue;
        }
        members[count++] = static_cast<int>(w);
    }
    if (count == 0) {
        return;
    }
    /* Inn registry stamps every member's $0B town byte on sign-in (0x1A1CE),
     * so the first member carries the saved party's home-inn town. */
    uint8_t town = roster.records[members[0]].town_flags & 0x7F;
    if (town < 1 || town > 5) {
        town = 1;
    }
    mm2_party_launch_build(&pending_launch_, town, members, count);
    pending_choose_town_filter_ = town;
    MM2_DBG("MM2 GOTO: restored saved party from tail members=%d town=%u\n", count,
            (unsigned)town);
}

void AppHost::charDestroy()
{
#if MM2_HOST_AMIGA
    character_ui_.leaveMode();
#endif
    char_create_prepare_done_ = false;
    char_create_booting_ = false;
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

    if (char_create_booting_) {
        character_ui_.prepareCreateCharacterAssets();
        char_create_booting_ = false;
        return;
    }

    if (!char_create_prepare_done_) {
        char_create_prepare_done_ = true;
        character_ui_.startCreateCharacter(title_.roster());
        return;
    }

    if (false && character_ui_.mode() == ui::CharacterUiMode::ChooseParty) {
        MM2_DBG(
            "MM2 GOTO: charStep tick ChooseParty ascii=%c ctrl=%d chip=%lu\n",
            keys_.last_ascii,
            keys_.ctrl ? 1 : 0,
            apphost_chip_free()
        );
    }
    character_ui_.tick(keys_);
    if (!character_ui_.active()) {
        if (character_ui_.quitRequested()) {
            MM2_DBG("MM2 GOTO: charStep quit from character UI\n");
            quit_ = true;
            return;
        }
        Mm2PartyLaunch launch{};
        if (character_ui_.takePartyLaunch(&launch)) {
            MM2_DBG(
                "MM2 GOTO: charStep party launch town=%u area=%u chip=%lu\n",
                (unsigned)launch.town_filter,
                (unsigned)launch.area_id,
                apphost_chip_free()
            );
            pending_launch_ = launch;
            pending_in_town_ = true;
        }
    }
}

void AppHost::charDraw()
{
#if MM2_HOST_AMIGA
    char_ui_redrew_ = false;
    const bool goto_trace = false; // character_ui_.mode() == ui::CharacterUiMode::ChooseParty;
    if (goto_trace) {
        MM2_DBG("MM2 GOTO: charDraw begin redraw=%d chip=%lu\n", character_ui_.needsRedraw() ? 1 : 0,
                apphost_chip_free());
    }
    if (!character_ui_.needsRedraw()) {
        /* Static chooser: keep the previous front buffer; framePresent waits vblank. */
        return;
    }
    /* Drain blits from menuSuspend/clearScreen — Release can outrun the ACE blitter. */
    mm2_amiga_blit_sync();
    if (goto_trace) {
        MM2_DBG("MM2 GOTO: charDraw ui_cache_begin+render\n");
    }
    platform::applyUiPalette();
    if (character_ui_.needsIncrementalRedraw()) {
        /* Ctrl+letter tick: patch check/count cells; cache already holds the rest. */
        mm2_amiga_ui_cache_begin_keep();
        if (character_ui_ready_) {
            character_ui_.renderIncremental(title_.compositor());
        }
        mm2_amiga_ui_cache_end();
    } else {
        mm2_amiga_ui_cache_begin();
        if (character_ui_ready_) {
            character_ui_.render(title_.compositor());
        }
        mm2_amiga_ui_cache_end();
    }
    character_ui_.ackRedraw();
    mm2_amiga_blit_sync();
    if (goto_trace) {
        MM2_DBG("MM2 GOTO: charDraw ui_cache_present\n");
    }
    mm2_amiga_ui_cache_present();
    char_ui_redrew_ = true;
    if (goto_trace) {
        MM2_DBG("MM2 GOTO: charDraw end\n");
    }
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
    MM2_DBG("MM2 GOTO: takePendingInTown\n");
    pending_in_town_ = false;
    return true;
}

bool AppHost::startInTownFromPending()
{
    MM2_DBG("MM2 GOTO: startInTownFromPending chip=%lu\n", apphost_chip_free());
#if MM2_HOST_AMIGA
    mm2_amiga_blit_sync();
    MM2_DBG("MM2 GOTO: releaseChipForPlayMode...\n");
    title_.releaseChipForPlayMode();
    MM2_DBG("MM2 GOTO: releaseChipForPlayMode done chip=%lu\n", apphost_chip_free());
#endif
    if (!session_.start(data_dir_, title_.roster(), pending_launch_)) {
        MM2_DBG("MM2 GOTO: GameSession::start FAILED\n");
        return false;
    }
    MM2_DBG("MM2 GOTO: GameSession::start ok bootstrapping=%d\n", session_.isBootstrapping() ? 1 : 0);
    in_town_active_ = true;
    play_title_screen_ = -1;
    refreshPlayWindowTitle();
    return true;
}

void AppHost::refreshPlayWindowTitle()
{
#if !MM2_HOST_AMIGA
    const int screen = session_.currentScreenId();
    if (screen == play_title_screen_) {
        return;
    }
    play_title_screen_ = screen;
    char window_title[256];
    std::snprintf(window_title, sizeof(window_title), "MM2 (%s) — %s", platform::hostName(),
                  session_.currentScreenLabel());
    platform::setWindowTitle(window_title);
#endif
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
    refreshPlayWindowTitle();
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
     * SPACE / combat-command tap during a slow frame is not lost between loop polls. */
    if (session_.awaitingContinuePrompt()) {
        const platform::KeyState after = platform::pollInput();
        if (after.space || after.enter || after.any_key || after.last_ascii != 0) {
            keys_ = after;
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

void AppHost::syncSessionRosterToTitle()
{
    pending_choose_town_filter_ = session_.gotoTownFilter();
    pending_launch_ = session_.launch();
    title_.roster() = session_.roster();
}

void AppHost::inTownEnd()
{
    in_town_active_ = false;
    session_.clearGotoTownRequest();
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

void AppHost::inTownEndForGotoTown()
{
    in_town_active_ = false;
    session_.clearGotoTownRequest();
    session_.shutdown();
#if MM2_HOST_AMIGA
    /* Play mode freed the title UI cache — restore before char choose redraws. */
    mm2_amiga_ui_cache_create();
    platform::applyUiPalette();
#endif
#if !MM2_HOST_AMIGA
    char window_title[256];
    std::snprintf(window_title, sizeof(window_title), "MM2 (%s) — goto town", platform::hostName());
    platform::setWindowTitle(window_title);
#endif
}

}  // namespace mm2
