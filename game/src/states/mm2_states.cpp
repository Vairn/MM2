#include "mm2/states/mm2_states.h"

#include "mm2/Mm2Dbg.h"
#include "mm2/states/AppHost.h"
#include "mm2/states/mm2_app_context.h"
#include "mm2/TitleScreen.h"
#include "mm2/platform/Platform.h"

#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
#include <ace/managers/memory.h>
#endif

namespace {

using mm2::AppHost;
using mm2::TitleScreen;

AppHost &host() { return AppHost::get(); }

#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
static unsigned long goto_chip_free()
{
    return (unsigned long)memGetFreeChipSize();
}
#else
static unsigned long goto_chip_free() { return 0; }
#endif

static int g_char_choose_frame = 0;

/* ---- Boot ---- */
static void boot_create(void) {}

static void boot_loop(void)
{
    AppHost &h = host();

    if (h.bootIsFading()) {
        const TitleScreen::LogoAdvance adv = h.logoStep();
        if (h.shouldQuit()) {
            return;
        }
        if (adv == TitleScreen::LogoAdvance::GoAttract) {
            mm2_pending_change(&g_mm2_state_attract);
        } else if (adv == TitleScreen::LogoAdvance::GoMenu) {
            mm2_pending_change(&g_mm2_state_menu);
        }
        h.logoDraw();
        h.framePresent();
        return;
    }

    const TitleScreen::BootOutcome r = h.bootStep();
    if (r == TitleScreen::BootOutcome::Failed) {
        h.requestQuit();
        return;
    }
    if (h.shouldQuit()) {
        return;
    }
    if (r == TitleScreen::BootOutcome::ReadyFade) {
        h.bootBeginLogoFade();
        h.logoDraw();
        h.framePresent();
        return;
    }
    if (r == TitleScreen::BootOutcome::ReadyAttract) {
        mm2_pending_change(&g_mm2_state_attract);
        h.framePresent();
        return;
    }

    h.bootLogoDraw();
    h.framePresent();
}

static void boot_destroy(void) {}

/* ---- Logo ---- */
static void logo_create(void) { host().logoEnter(); }

static void logo_loop(void)
{
    AppHost &h = host();
    const TitleScreen::LogoAdvance adv = h.logoStep();
    if (h.shouldQuit()) {
        return;
    }
    if (adv == TitleScreen::LogoAdvance::GoAttract) {
        mm2_pending_change(&g_mm2_state_attract);
    } else if (adv == TitleScreen::LogoAdvance::GoMenu) {
        mm2_pending_change(&g_mm2_state_menu);
    }
    h.logoDraw();
    h.framePresent();
}

static void logo_destroy(void) {}

/* ---- Attract ---- */
static void attract_create(void) { host().attractEnter(); }

static void attract_loop(void)
{
    AppHost &h = host();
    const TitleScreen::AttractAdvance adv = h.attractStep();
    if (h.shouldQuit()) {
        return;
    }
    if (adv == TitleScreen::AttractAdvance::GoMenu) {
        mm2_pending_change(&g_mm2_state_menu);
    }
    h.attractDraw();
    h.framePresent();
}

static void attract_destroy(void) {}

/* ---- Menu ---- */
static void menu_create(void) { host().menuEnter(); }

static void menu_loop(void)
{
    AppHost &h = host();
    const TitleScreen::MenuAdvance adv = h.menuStep();
    if (h.shouldQuit() || adv == TitleScreen::MenuAdvance::Quit) {
        return;
    }
    switch (adv) {
    case TitleScreen::MenuAdvance::Controls:
        mm2_pending_push(&g_mm2_state_controls);
        break;
    case TitleScreen::MenuAdvance::Options:
        mm2_pending_push(&g_mm2_state_options);
        break;
    case TitleScreen::MenuAdvance::ViewParty:
        mm2_pending_push(&g_mm2_state_char_view);
        break;
    case TitleScreen::MenuAdvance::CreateCharacter:
        mm2_pending_push(&g_mm2_state_char_create);
        break;
    case TitleScreen::MenuAdvance::ChooseParty:
        MM2_DBG("MM2 GOTO: menu_loop pushing char_choose chip=%lu\n", goto_chip_free());
        mm2_pending_push(&g_mm2_state_char_choose);
        break;
    default:
        break;
    }
    h.menuDraw();
    h.framePresent();
}

static void menu_destroy(void) {}

static void menu_resume(void) { host().menuResume(); }

static void menu_suspend(void) { host().menuSuspend(); }

/* ---- Controls overlay ---- */
static void controls_create(void) { host().controlsEnter(); }

static void controls_loop(void)
{
    AppHost &h = host();
    if (h.controlsStep()) {
        mm2_pending_pop();
        return;
    }
    h.controlsDraw();
    h.framePresent();
}

static void controls_destroy(void) {}

/* ---- Options overlay ---- */
static void options_create(void) { host().optionsEnter(); }

static void options_loop(void)
{
    AppHost &h = host();
    if (h.optionsStep()) {
        mm2_pending_pop();
        return;
    }
    h.optionsDraw();
    h.framePresent();
}

static void options_destroy(void) {}

/* ---- Character UI ---- */
static void char_view_create(void) { host().charViewEnter(); }

static void char_view_loop(void)
{
    AppHost &h = host();
    h.charStep();
    if (h.shouldQuit()) {
        return;
    }
    if (h.takePendingInTown()) {
        mm2_pending_change(&g_mm2_state_in_town);
        return;
    }
    if (!h.characterUi().active()) {
        h.charPopCleanup();
        mm2_pending_pop();
        return;
    }
    h.charDraw();
    h.framePresent();
}

static void char_view_destroy(void) { host().charDestroy(); }

static void char_create_create(void) { host().charCreateEnter(); }

static void char_create_loop(void)
{
    AppHost &h = host();
    h.charStep();
    if (h.shouldQuit()) {
        return;
    }
    if (h.takePendingInTown()) {
        mm2_pending_change(&g_mm2_state_in_town);
        return;
    }
    if (!h.characterUi().active() && !h.charCreatePendingStart()) {
        h.charPopCleanup();
        mm2_pending_pop();
        return;
    }
    h.charDraw();
    h.framePresent();
}

static void char_create_destroy(void) { host().charDestroy(); }

static void char_choose_create(void)
{
    MM2_DBG("MM2 GOTO: char_choose_create() chip=%lu\n", goto_chip_free());
    g_char_choose_frame = 0;
    host().charChooseEnter();
    MM2_DBG("MM2 GOTO: char_choose_create done chip=%lu\n", goto_chip_free());
}

static void char_choose_loop(void)
{
    AppHost &h = host();
    MM2_DBG(
        "MM2 GOTO: char_choose loop #%d active=%d mode=%d redraw=%d chip=%lu\n",
        g_char_choose_frame,
        h.characterUi().active() ? 1 : 0,
        static_cast<int>(h.characterUi().mode()),
        h.characterUi().needsRedraw() ? 1 : 0,
        goto_chip_free()
    );
    ++g_char_choose_frame;

    h.charStep();
    if (h.shouldQuit()) {
        MM2_DBG("MM2 GOTO: char_choose quit requested\n");
        return;
    }
    if (h.takePendingInTown()) {
        MM2_DBG("MM2 GOTO: char_choose -> pending in_town, change state\n");
        mm2_pending_change(&g_mm2_state_in_town);
        return;
    }
    if (!h.characterUi().active()) {
        MM2_DBG("MM2 GOTO: char_choose UI inactive -> pop menu\n");
        h.charPopCleanup();
        mm2_pending_pop();
        return;
    }
    MM2_DBG("MM2 GOTO: char_choose charDraw...\n");
    h.charDraw();
    MM2_DBG("MM2 GOTO: char_choose framePresent...\n");
    h.framePresent();
    MM2_DBG("MM2 GOTO: char_choose frame done chip=%lu\n", goto_chip_free());
}

static void char_choose_destroy(void) { host().charDestroy(); }

/* ---- In-town ---- */
static void in_town_create(void)
{
    AppHost &h = host();
    MM2_DBG("MM2 GOTO: in_town_create() chip=%lu\n", goto_chip_free());
    if (!h.startInTownFromPending()) {
        MM2_DBG("MM2 GOTO: in_town_create FAILED -> menu\n");
        mm2_pending_change(&g_mm2_state_menu);
    } else {
        MM2_DBG("MM2 GOTO: in_town_create ok chip=%lu\n", goto_chip_free());
    }
}

static void in_town_loop(void)
{
    AppHost &h = host();
    h.inTownStep();
    if (h.shouldQuit()) {
        return;
    }
    if (h.inTownRequestGotoTown()) {
        h.syncSessionRosterToTitle();
        h.inTownEndForGotoTown();
        mm2_pending_change(&g_mm2_state_char_choose);
        return;
    }
    if (h.inTownRequestTitle()) {
        h.syncSessionRosterToTitle();
        mm2_pending_change(&g_mm2_state_menu);
        return;
    }
    h.inTownDraw();
    h.framePresent();
}

static void in_town_destroy(void) { host().inTownEnd(); }

}  // namespace

Mm2State g_mm2_state_boot = {boot_create, boot_loop, boot_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_logo = {logo_create, logo_loop, logo_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_attract = {attract_create, attract_loop, attract_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_menu = {menu_create, menu_loop, menu_destroy, menu_suspend, menu_resume, NULL};
Mm2State g_mm2_state_controls = {controls_create, controls_loop, controls_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_options = {options_create, options_loop, options_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_char_view = {char_view_create, char_view_loop, char_view_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_char_create = {char_create_create, char_create_loop, char_create_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_char_choose = {char_choose_create, char_choose_loop, char_choose_destroy, NULL, NULL, NULL};
Mm2State g_mm2_state_in_town = {in_town_create, in_town_loop, in_town_destroy, NULL, NULL, NULL};
