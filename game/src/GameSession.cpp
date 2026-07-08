#include "mm2/GameSession.h"

#include "mm2/Mm2Dbg.h"
#include "mm2/CppStdCompat.h"

#include "mm2/DataPath.h"
#include "mm2/combat/EncounterPicker.h"
#include "mm2/events/EventTownServices.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/gameplay/Movement.h"
#include "mm2/gameplay/PlaySessionInput.h"
#include "mm2/runtime/Alloc.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/AutomapView.h"
#include "mm2/gfx/GfxBackend.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/CombatPanel.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/Platform.h"
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#endif

namespace mm2 {

namespace {

const char *kTownNames[] = {"?", "Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};

void drawSpellEyeOverlayIfActive(gfx::ScreenCompositor &c, const gfx::EnvAssets &env,
                                 const world::MapWorld &world, const GameStateView &gs, bool assets_ok)
{
    if (assets_ok && gs.valid() &&
        events::eventVmSpellEyeTimer(gs.a4(), world.isOutdoor()) > 0) {
        gfx::renderSpellEyeOverlay(c, env, world, gs);
    }
}

void blitImageFrame(gfx::ScreenCompositor &c, const mm2_gfx_sheet &sheet, int frame, int dst_x, int dst_y,
                    int opaque = 0)
{
    const mm2_image32_file &img = sheet.img;
    if (frame < 0 || frame >= img.frame_count) {
        return;
    }

    const mm2_image32_frame &f = img.frames[frame];
#if MM2_HOST_AMIGA
    (void)c;
    if (!f.bitmap) {
        return;
    }
    platform::blitImage32(&img, frame, dst_x, dst_y, opaque);
#else
    if (!f.rgba) {
        return;
    }
    c.blitRgba(f.rgba, f.width, f.height, dst_x, dst_y);
#endif
}

}  // namespace

bool GameSession::needsRedraw() const
{
#if MM2_HOST_AMIGA
    return view3d_dirty_ || chrome_dirty_ || text_dirty_ || overlay_anim_dirty_;
#else
    return frame_dirty_;
#endif
}

void GameSession::ackRedraw()
{
#if MM2_HOST_AMIGA
    view3d_dirty_ = false;
    chrome_dirty_ = false;
    text_dirty_ = false;
    overlay_anim_dirty_ = false;
#else
    frame_dirty_ = false;
#endif
}

void GameSession::markDirty()
{
#if MM2_HOST_AMIGA
    view3d_dirty_ = true;
    chrome_dirty_ = true;
    text_dirty_ = true;
    overlay_anim_dirty_ = false;
    play_buffer_valid_ = false;
    mm2_amiga_viewport_cache_invalidate();
#else
    frame_dirty_ = true;
#endif
}

#if MM2_HOST_AMIGA
void GameSession::markView3DDirty()
{
    view3d_dirty_ = true;
    mm2_amiga_viewport_cache_invalidate();
}

void GameSession::markTextDirty()
{
    text_dirty_ = true;
}

void GameSession::markOverlayAnimDirty()
{
    overlay_anim_dirty_ = true;
}
#endif

const char *GameSession::areaName(uint8_t area_id)
{
    static const char *kAreas[] = {"Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};
    if (area_id < 5) {
        return kAreas[area_id];
    }
    return "?";
}

namespace {

const char *areaNameRaw(int area)
{
    switch (area) {
        case 0:
            return "Middlegate";
        case 1:
            return "Atlantium";
        case 2:
            return "Tundara";
        case 3:
            return "Vulcania";
        case 4:
            return "Sandsobar";
        case 5:
            return "A1";
        case 6:
            return "B1";
        case 7:
            return "C1";
        case 8:
            return "D1";
        case 9:
            return "A2";
        case 10:
            return "B2";
        case 11:
            return "C2";
        case 12:
            return "A3";
        case 13:
            return "B3";
        case 14:
            return "C3";
        case 15:
            return "A4";
        case 16:
            return "B4";
        case 17:
            return "Middlegate Cavern";
        case 18:
            return "Atlantium Cavern";
        case 19:
            return "Tundara Cavern";
        case 20:
            return "Vulcania Cavern";
        case 21:
            return "Sandsobar Cavern";
        case 33:
            return "E1";
        case 34:
            return "D2";
        case 35:
            return "E2";
        case 36:
            return "D3";
        case 37:
            return "E3";
        case 38:
            return "C4";
        case 39:
            return "D4";
        case 40:
            return "E4";
        case 55:
            return "Hillstone";
        case 56:
            return "Woodhaven";
        case 57:
            return "Pinehurst";
        case 58:
            return "Luxus Palace";
        case 59:
            return "Castle Xabran";
        default:
            return nullptr;
    }
}

}  // namespace

const char *GameSession::currentScreenLabel() const
{
    static char kFallback[24];
    if (!world_.loaded()) {
        return "?";
    }
    const int screen = world_.currentScreen();
    const Mm2AttribRecord &rec = world_.attrib();
    const int area = static_cast<int>(mm2_attrib_area_id(&rec));
    const char *name = areaNameRaw(area);
    if (name != nullptr) {
        return name;
    }
    if (mm2_attrib_is_outdoor(&rec)) {
        std::snprintf(kFallback, sizeof(kFallback), "Screen %d", screen);
        return kFallback;
    }
    std::snprintf(kFallback, sizeof(kFallback), "Area %d", area);
    return kFallback;
}

const char *GameSession::townName(uint8_t town_filter)
{
    return (town_filter >= 1 && town_filter <= 5) ? kTownNames[town_filter] : "?";
}

bool GameSession::start(const char *data_dir, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                        uint32_t start_flags)
{
    MM2_DBG(
        "MM2 GOTO: GameSession::start town=%u area=%u members=%u flags=%u\n",
        (unsigned)launch.town_filter,
        (unsigned)launch.area_id,
        (unsigned)launch.party_count,
        (unsigned)start_flags
    );
    data_dir_ = data_dir;
    start_flags_ = start_flags;
    roster_ = roster;
    launch_ = launch;
    quit_ = false;
    back_to_title_ = false;
    back_to_goto_town_ = false;
    assets_ok_ = false;
    overlay_ = PlayOverlay::None;
    town_service_ui_.close();
    automap_.clearAll();
    sheet_session_ = {};
    sheet_session_.party_slot = 0;
    status_message_[0] = '\0';
    has_items_ = false;
    has_monsters_ = false;

    right_panel_ = gfx::PlayRightPanel::Protect;

    if (!gs_image_) {
        gs_image_ = static_cast<uint8_t *>(mm2::runtime::allocate(kGsImageBytes));
        if (!gs_image_) {
            return false;
        }
    }
    ::memset(gs_image_, 0, kGsImageBytes);
    gs_ = GameStateView(mm2_gs_base_from_image(gs_image_));

    gs_.initCalendarDefaults();
    gs_.initControlsDefaults();
    gs_.initProtectDefaults();
    mm2_party_launch_apply(gs_.a4(), &launch_);

    mm2_image32_set_preview_opaque(0);

#if MM2_HOST_AMIGA
    mm2_amiga_viewport_cache_create();
    mm2_amiga_play_chrome_cache_create();
    bootstrapping_ = true;
    bootstrap_step_ = 0;
    markDirty();
    MM2_DBG("MM2 GOTO: GameSession::start -> bootstrapping step 0\n");
    return true;
#else
    ingame_sheet_.loadAssets(data_dir_);

    char *path = mm2_path_scratch_a();
    if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "items.dat")) {
        has_items_ = mm2_items_load_file(path, &items_) == MM2_ITEMS_OK;
    }
    if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "monsters.dat")) {
        has_monsters_ = mm2_monsters_load_file(path, &monsters_) == MM2_MONSTERS_OK;
    }
    combat_.bindParty(&roster_, &launch_);
    combat_.bindMonsters(has_monsters_ ? &monsters_ : nullptr);
    combat_.bindRng(&rng_);

    const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
    bool has_env = false;
    bool has_sky = false;
    if (has_world) {
        has_sky = env_.loadGlobal(data_dir_);
        has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
        combat::encounterSyncScreenContext(gs_, world_);
    }
    assets_ok_ = has_world && has_env && has_sky;
    events_loaded_ = events_.load(data_dir_);
    if (events_loaded_) {
        events_.bindParty(&roster_, &launch_);
        events_.bindItems(has_items_ ? &items_ : nullptr);
        events_.bindTownServiceUi(&town_service_ui_);
        events_.bindRng(&rng_);
        events_.bindCombat(&combat_);
        refreshEventsForScreen();
    }

    scripted_loaded_ = scripted_scene_.load(data_dir_);
    if (scripted_loaded_) {
        maybeQueueScriptedScenes(true);
    }

    automap_.markPartyTileIfCartographer(gs_.screenId(), gs_.coordX(), gs_.coordY(), roster_, launch_);

    markDirty();

#if !MM2_NO_STL
    if (!assets_ok_) {
        const char *dir_label = (data_dir_ && data_dir_[0]) ? data_dir_ : ".";
        const gfx::GfxBackend gfx = gfx::gfxSettings().resolved;
        std::fprintf(stderr, "mm2: play view missing assets in %s (world=%d env=%d sky=%d gfx=%s)\n",
                     dir_label, has_world, has_env, has_sky, gfx::gfxBackendLabel(gfx));
        if ((gfx == gfx::GfxBackend::Cga || gfx == gfx::GfxBackend::Ega) && (!has_env || !has_sky)) {
            std::fprintf(stderr,
                         "mm2: --gfx=%s needs SKY/TOWN .4 or .16 in the data dir "
                         "(try: game\\build\\pc_gfx_test_data)\n",
                         gfx::gfxBackendLabel(gfx));
        }
    }
    if (!has_monsters_) {
        std::fprintf(stderr,
                     "mm2: WARNING monsters.dat not loaded from %s — tile/step fights will not run\n",
                     data_dir_ ? data_dir_ : "(cwd)");
    }
#endif

    return true;
#endif
}

#if MM2_HOST_AMIGA
void GameSession::tickBootstrap()
{
    if (!bootstrapping_) {
        return;
    }

    MM2_DBG("MM2 GOTO: tickBootstrap step=%u\n", (unsigned)bootstrap_step_);

    switch (bootstrap_step_) {
    case 0:
        MM2_DBG("MM2 GOTO: bootstrap load ingame_sheet + items\n");
        ingame_sheet_.loadAssets(data_dir_);
        {
            char *path = mm2_path_scratch_a();
            if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "items.dat")) {
                has_items_ = mm2_items_load_file(path, &items_) == MM2_ITEMS_OK;
            }
            if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "monsters.dat")) {
                has_monsters_ = mm2_monsters_load_file(path, &monsters_) == MM2_MONSTERS_OK;
            }
        }
        combat_.bindParty(&roster_, &launch_);
        combat_.bindMonsters(has_monsters_ ? &monsters_ : nullptr);
        combat_.bindRng(&rng_);
        bootstrap_step_ = 1;
        markDirty();
        return;
    case 1: {
        MM2_DBG("MM2 GOTO: bootstrap world load screen=%d\n", gs_.screenId());
        const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
        if (has_world) {
            combat::encounterSyncScreenContext(gs_, world_);
        }
        assets_ok_ = has_world;
        bootstrap_step_ = 2;
        markDirty();
        return;
    }
    case 2: {
        MM2_DBG("MM2 GOTO: bootstrap env load\n");
        bool has_env = false;
        bool has_sky = false;
        if (world_.loaded()) {
            has_sky = env_.loadGlobal(data_dir_);
            has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
        }
        assets_ok_ = world_.loaded() && has_env && has_sky;
        bootstrap_step_ = 3;
        markDirty();
        return;
    }
    case 3:
        MM2_DBG("MM2 GOTO: bootstrap events load\n");
        events_loaded_ = events_.load(data_dir_);
        if (events_loaded_) {
            events_.bindParty(&roster_, &launch_);
            events_.bindItems(has_items_ ? &items_ : nullptr);
            events_.bindTownServiceUi(&town_service_ui_);
            events_.bindRng(&rng_);
            events_.bindCombat(&combat_);
            refreshEventsForScreen();
        }
        bootstrap_step_ = 4;
        markDirty();
        return;
    default:
        MM2_DBG("MM2 GOTO: bootstrap scripted + finish\n");
        scripted_loaded_ = scripted_scene_.load(data_dir_);
        if (scripted_loaded_) {
            maybeQueueScriptedScenes(true);
        }
        automap_.markPartyTileIfCartographer(gs_.screenId(), gs_.coordX(), gs_.coordY(), roster_,
                                               launch_);
        bootstrapping_ = false;
        MM2_DBG("MM2 GOTO: bootstrap DONE assets_ok=%d\n", assets_ok_ ? 1 : 0);
        markDirty();
        return;
    }
}
#endif

void GameSession::shutdown()
{
    env_.unloadAll();
    events_.unload();
    events_loaded_ = false;
    scripted_scene_.unload();
    scripted_loaded_ = false;
    if (gs_image_) {
        mm2::runtime::deallocate(gs_image_, kGsImageBytes);
        gs_image_ = nullptr;
    }
    gs_ = GameStateView();

    data_dir_ = nullptr;
    assets_ok_ = false;
    overlay_ = PlayOverlay::None;
    has_items_ = false;
#if MM2_HOST_AMIGA
    view3d_dirty_ = false;
    chrome_dirty_ = false;
    text_dirty_ = false;
    overlay_anim_dirty_ = false;
    play_buffer_valid_ = false;
    mm2_amiga_viewport_cache_invalidate();
    mm2_amiga_play_chrome_cache_destroy();
    bootstrapping_ = false;
    bootstrap_step_ = 0;
#else
    frame_dirty_ = false;
#endif
}

void GameSession::refreshWorldAfterMove(const gameplay::MoveResult &move)
{
    if (!move.acted) {
        return;
    }

#if MM2_HOST_AMIGA
    markView3DDirty();
    if (move.turned) {
        markTextDirty();
    }
#else
    markDirty();
#endif

    if (move.acted && data_dir_) {
        if (move.screen_changed) {
            world_.enterScreen(gs_.screenId());
            const bool has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
            assets_ok_ = world_.loaded() && env_.loadGlobal(data_dir_) && has_env;
            combat::encounterSyncScreenContext(gs_, world_);
            if (events_loaded_) {
                refreshEventsForScreen();
            }
        }
        if (scripted_loaded_) {
            maybeQueueScriptedScenes(false);
        }
        automap_.markPartyTileIfCartographer(gs_.screenId(), gs_.coordX(), gs_.coordY(), roster_,
                                               launch_);
        if (move.moved) {
            gameplay::latchExploreEventsAfterMove(gs_);
        }
    }
}

void GameSession::maybeTriggerStepEncounter()
{
    /* Random step encounter @ 0x10A2 on the departure tile (before coords update). */
    if (!has_monsters_ || combat_.active()) {
        return;
    }
    const bool triggered = combat::encounterTryStepRandom(gs_, world_, rng_);
    if (!triggered) {
        return;
    }
    if (!combat_.enter(gs_, world_)) {
        showStatusMessage("Encounter! (monsters.dat missing — fight skipped)");
        return;
    }
    markDirty();
}

void GameSession::maybeQueueScriptedScenes(bool on_start)
{
    if ((start_flags_ & kStartSkipScriptedIntros) != 0) {
        return;
    }
    if (!scripted_loaded_ || !gs_.valid() || scripted_scene_.active()) {
        return;
    }

    /* Area enter @ 0x6E84: first_time_flag until Corak prologue at (7,4) (FAQ x,y).
     * corak_intro_seen_ is a port-side one-shot (the prologue is scene scheduling,
     * not a per-character quest bit). */
    if (on_start && gs_.screenId() == 0 && !corak_intro_seen_) {
        gs_.setFirstTimeFlag(true);
        return;
    }

    if (!on_start && gs_.screenId() == 0 && gs_.coordX() == 7 && gs_.coordY() == 4 &&
        gs_.firstTimeFlag() && !corak_intro_seen_) {
        scripted_scene_.queueScene(events::ScriptedSceneId::CorakIntro);
        corak_intro_seen_ = true;
        gs_.setFirstTimeFlag(false);
        markDirty();
    }
}

void GameSession::refreshEventsForScreen()
{
    if (!events_loaded_ || !gs_.valid()) {
        return;
    }
    events_.enterLocation(static_cast<int>(gs_.screenId()), gs_, world_);
}

void GameSession::refreshWorldAfterEventTransition()
{
    if (!events_loaded_ || !events_.screenChanged() || !data_dir_) {
        return;
    }
    events_.clearScreenChanged();

    world_.enterScreen(gs_.screenId());
    const bool has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
    assets_ok_ = world_.loaded() && env_.loadGlobal(data_dir_) && has_env;
    combat::encounterSyncScreenContext(gs_, world_);
    if (events_loaded_) {
        /* OP_0C map_transition: drop OP_0B portraits / popups from the prior screen
         * (enterLocation → text_.reset). Without this, Middlegate shop .anm overlays
         * the overland viewport after exiting town. */
        refreshEventsForScreen();
    }
    markDirty();
}

void GameSession::finishCombat()
{
    combat_sprite_.unload();
    combat_sprite_disk_ = -1;
    combat_character_sheet_ = false;
    if (overlay_ == PlayOverlay::CharacterSheet) {
        overlay_ = PlayOverlay::None;
    }
#if MM2_HOST_AMIGA
    combat_backdrop_cached_ = false;
    mm2_amiga_viewport_cache_invalidate();
#endif
    /* Combat's own -$7EDE call already ran synchronously inside the OP_12/13/
     * arena handler that armed it, so the event VM already aborted the script
     * that same tick (0x16362/abortScript). Re-scan for tile events now that
     * the fight is over so OP_2B (gated on MM2_GS_COMBAT_VICTORY_LATCH) and
     * any post-combat script can fire, mirroring the ROM's -$7F1A pending
     * latch (0x16368-0x1637C). */
    mm2_gs_set_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT, 0);
    mm2_gs_set_u8(gs_.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    if (data_dir_) {
        char *const path = mm2_path_scratch_a();
        if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "roster.dat")) {
            (void)mm2_roster_save_file(path, &roster_); /* persist combat XP/gold/HP */
        }
    }
    runPendingEvents();
    refreshWorldAfterEventTransition();
    markDirty();
}

void GameSession::runPendingEvents()
{
    if (!events_loaded_ || !gs_.valid()) {
        return;
    }
    if (mm2_gs_u8(gs_.a4(), MM2_GS_PENDING_EVENT_LATCH)) {
        const bool blocking_before = events_.blocksMovement();
        const bool had_script_abort = mm2_gs_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT) != 0;
        events_.scanAndRun(gs_, world_);
        if (combat_.active()) {
            markDirty();
        } else if (!had_script_abort && mm2_gs_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT) != 0 &&
                   !has_monsters_) {
            /* OP_12/13 aborted the script but CombatSession::enter could not run. */
            showStatusMessage("Encounter! (monsters.dat missing — fight skipped)");
        }
        if (events_.blocksMovement() != blocking_before || events_.blocksMovement()) {
            markDirty();
        }
    }
}

bool GameSession::eventBlocksInput() const
{
    return events_loaded_ && events_.blocksMovement();
}

bool GameSession::scriptedBlocksInput() const
{
    return scripted_loaded_ && scripted_scene_.blocksInput();
}

bool GameSession::overlayBlocksInput() const
{
    return overlay_ != PlayOverlay::None || eventBlocksInput();
}

void GameSession::tickEventInput(const platform::KeyState &keys)
{
    if (!events_loaded_ || !events_.blocksMovement()) {
        return;
    }
    const bool blocking_before = events_.blocksMovement();
    const int layers_before = events_.textView().layerCount();
    events_.continueInput(gs_, world_, keys);
    refreshWorldAfterEventTransition();
    maybeFinishInnRegistry();
    maybeOpenTownServiceMenu();
    if (events_.blocksMovement() != blocking_before || events_.textView().layerCount() != layers_before) {
        markDirty();
    }
}

void GameSession::showStatusMessage(const char *msg)
{
    if (!msg) {
        status_message_[0] = '\0';
        overlay_ = PlayOverlay::None;
        markDirty();
        return;
    }
    std::snprintf(status_message_, sizeof(status_message_), "%s", msg);
    overlay_ = PlayOverlay::StatusMessage;
    markDirty();
}

gfx::PlayProtectValues GameSession::protectValues() const
{
    gfx::PlayProtectValues v{};
    if (gs_.valid()) {
        v.light = gs_.lightFactor();
        v.magic = gs_.magicProtect();
        v.forces = gs_.forcesProtect();
        v.levitate = gs_.levitateFlag();
        v.walk_water = gs_.walkWaterFlag();
        v.guard_dog = gs_.guardDogFlag();
    }
    return v;
}

void GameSession::handleExploreCommand(gameplay::PlaySessionAction action)
{
    switch (action) {
    case gameplay::PlaySessionAction::BashDoor:
        handleBashDoor();
        break;
    case gameplay::PlaySessionAction::Controls:
        overlay_ = PlayOverlay::Controls;
        markDirty();
        break;
    case gameplay::PlaySessionAction::DismissHireling:
        showStatusMessage("Dismiss hireling (GAP 0x141F4).");
        break;
    case gameplay::PlaySessionAction::ExchangeOrder:
        showStatusMessage("Exchange order (GAP 0x20F58).");
        break;
    case gameplay::PlaySessionAction::Automap:
        if (!assets_ok_ || !env_.automapReady()) {
            showStatusMessage("Automap tiles missing.");
        } else {
            overlay_ = PlayOverlay::Automap;
        }
        markDirty();
        break;
    case gameplay::PlaySessionAction::Search:
        showStatusMessage("Search (GAP $4800 / 0x1B19C).");
        break;
    case gameplay::PlaySessionAction::Unlock:
        handleUnlockDoor();
        break;
    case gameplay::PlaySessionAction::Rest:
        /* Rest @ 0x19E20: set the modal flag, then put up the Y/N confirm overlay.
         * Execution (pay / encounter / heal / day-advance) runs on confirm in
         * executeRest(). The original's "Too dangerous!" pre-check (0x19E32
         * btst #3,-$55D6) reads the re-bundled per-tile flag table that the port
         * does not maintain yet, so it is DEFERRED (no false blocking). */
        mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 1); /* 0x19E24: -$7950 := 1 */
        overlay_ = PlayOverlay::RestConfirm;
        markDirty();
        break;
    default:
        break;
    }
}

namespace {

/* Wall field directly ahead, read from the centre cell of the active screen's
 * VISUAL page (the original's -$55BA source, MapWorld.h). 2-bit field per the
 * port codec: 0 open, 1 wall, 2 door, 3 wall+torch (MM2_MAP_WALL_*). This is
 * the same extraction View3D uses for the forward wall slot (cell & mask, then
 * >> shift). */
int forwardWallField(const mm2::world::MapWorld &world, int x, int y, char facing)
{
    const uint8_t cell = world.visualPage()[static_cast<size_t>((y << 4) | (x & 0x0F))];
    return (cell >> mm2_map_facing_shift(facing)) & 3;
}

bool forwardDoorBlocked(const mm2::world::MapWorld &world, int x, int y, char facing)
{
    return mm2_map_door_locked_at(&world.collisionScreen(), x, y, facing) != 0;
}

void clearForwardDoorLock(mm2::world::MapWorld &world, int x, int y, char facing)
{
    mm2_map_clear_door_lock(&world.mapFileMut().screens[world.currentScreen()], x, y, facing);
}

/* Unlock @ 0x20D44 reads roster +$1E; creation thievery lives at +$16 when +$1E
 * is still zero (common on non-Robber slots in roster.dat). */
int unlockThieveryFor(const Mm2RosterRecord &rec)
{
    const int at_1e = rec.unknown_1a_20[4];
    if (at_1e != 0) {
        return at_1e;
    }
    return rec.thievery_percent;
}

int bestPartyUnlockThievery(const Mm2PartyLaunch &launch, const Mm2RosterFile &roster)
{
    int best = 0;
    for (int i = 0; i < launch.party_count; ++i) {
        const int idx = launch.roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        const int skill = unlockThieveryFor(roster.records[idx]);
        if (skill > best) {
            best = skill;
        }
    }
    return best;
}

}  // namespace

void GameSession::applyDoorTrapDamage()
{
    /* Trap victim pick @ 0x1A9A6 + damage @ 0x1A90E (rng 1..100). Full ASM path
     * prints party status and sets -$7950 := 3; here we apply HP loss only. */
    if (launch_.party_count <= 0) {
        return;
    }

    const int slot = rng_.range(0, launch_.party_count - 1);
    const int idx = launch_.roster_slots[slot];
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
        return;
    }

    const int damage = rng_.range(1, 100);
    Mm2RosterRecord &rec = roster_.records[idx];
    if (rec.hp_current > static_cast<uint16_t>(damage)) {
        rec.hp_current = static_cast<uint16_t>(rec.hp_current - static_cast<uint16_t>(damage));
    } else {
        rec.hp_current = 0;
    }
    markDirty();
}

void GameSession::handleBashDoor()
{
    /* Bash door @ 0x9B48. */
    const int x = gs_.coordX();
    const int y = gs_.coordY();
    const char facing = gs_.facingKey();
    const int field = forwardWallField(world_, x, y, facing);

    /* 0x9B7A/0x9B82: outdoor, or no wall ahead -> a plain forward step + tick. */
    if (world_.isOutdoor() || field == MM2_MAP_WALL_OPEN) {
        maybeTriggerStepEncounter();
        gameplay::MoveResult move = gameplay::step(world_, gs_, true);
        refreshWorldAfterMove(move);
        return;
    }

    /* 0x9B88: door already unlocked (no wall bit) -> step through. */
    if (!forwardDoorBlocked(world_, x, y, facing)) {
        maybeTriggerStepEncounter();
        gameplay::MoveResult move = gameplay::step(world_, gs_, true);
        refreshWorldAfterMove(move);
        return;
    }

    /* 0x9BB4: a non-door wall (1 solid / 3 torch) -> "Solid!". */
    if (field != MM2_MAP_WALL_DOOR) {
        showStatusMessage(gameplay::obstructionMessage(gameplay::ObstructionMsg::Solid));
        return;
    }

    /* 0x9BCA door: bash strength = char0 +$6B (might base) [+ char1 if party>1]. */
    int might_sum = 0;
    for (int i = 0; i < launch_.party_count && i < 2; ++i) {
        const int idx = launch_.roster_slots[i];
        if (idx >= 0 && idx < MM2_ROSTER_RECORD_COUNT) {
            might_sum += roster_.records[idx].might_base; /* roster +$6B */
        }
    }

    const int door_strength = mm2_attrib_door_strength(&world_.attrib());
    const int roll_10_109 = rng_.range(10, 0x6D); /* 0x9BFE */
    const int trap_d100 = rng_.range(1, 100);     /* 0x9C54 */

    const gameplay::BashDecision d =
        gameplay::bashDoorRoll(might_sum, door_strength, roll_10_109, trap_d100);

    if (d.outcome == gameplay::BashOutcome::Locked) {
        showStatusMessage(gameplay::obstructionMessage(d.msg));
        return;
    }

    if (d.outcome == gameplay::BashOutcome::TrapSprung) {
        applyDoorTrapDamage();
        return;
    }

    if (d.clears_lock) {
        clearForwardDoorLock(world_, x, y, facing); /* -$7F02 @ 0x4B06 */
    }

    /* 0x9C66: step forward + time tick. */
    maybeTriggerStepEncounter();
    gameplay::MoveResult move = gameplay::step(world_, gs_, true);
    refreshWorldAfterMove(move);
}

void GameSession::handleUnlockDoor()
{
    /* Unlock door @ 0x20CA2. */
    if (world_.isOutdoor()) {
        return; /* 0x20CAE: outdoor -> silent exit. */
    }

    const int x = gs_.coordX();
    const int y = gs_.coordY();
    const char facing = gs_.facingKey();
    const int field = forwardWallField(world_, x, y, facing);

    /* 0x20CE4: must be a door ahead, else exit silently. */
    if (field != MM2_MAP_WALL_DOOR) {
        return;
    }

    /* 0x20CF0: lock bit set in collision page for this facing. */
    if (!forwardDoorBlocked(world_, x, y, facing)) {
        showStatusMessage(gameplay::obstructionMessage(gameplay::ObstructionMsg::NotLocked));
        return;
    }

    /* GAP: the "who tries to unlock" character picker (0x1AE2E) is its own modal
     * UI (not yet ported); use the party member with the highest +$1E / +$16 skill. */
    const int thievery = bestPartyUnlockThievery(launch_, roster_);
    if (thievery <= 0) {
        showStatusMessage(gameplay::obstructionMessage(gameplay::ObstructionMsg::Locked));
        return;
    }

    const int lock_d100 = rng_.range(1, 100);  /* 0x20D2E */
    const int trap_d100 = rng_.range(1, 100);  /* 0x20D64 (only used on a failed pick) */
    const int trap_byte = mm2_attrib_door_trap_byte(&world_.attrib());

    const gameplay::UnlockDecision d =
        gameplay::unlockDoorRoll(thievery, lock_d100, trap_byte, trap_d100);

    if (d.outcome == gameplay::UnlockOutcome::TrapSprung) {
        applyDoorTrapDamage();
        return;
    }

    if (d.clears_lock) {
        clearForwardDoorLock(world_, x, y, facing); /* -$7F02 @ 0x4B06 */
    }

    showStatusMessage(gameplay::obstructionMessage(d.msg));
}

void GameSession::executeRest()
{
    uint8_t *a4 = gs_.a4();

    /* --- 0x19AD6 hireling daily-pay check --------------------------------- */
    uint32_t hireling_pay = 0;
    for (int i = 0; i < launch_.party_count; ++i) {
        const int idx = launch_.roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        if (idx >= 0x18) { /* roster index >= 24 -> hireling (doc 33 / 0x19AE4) */
            hireling_pay += roster_.records[idx].gold; /* +$66 daily fee */
        }
    }
    if (hireling_pay > 0) {
        /* The pooled-gold spend routine -$7E6C (0x19B1E) is not traced; pool the
         * party's gold to decide affordability. DEFER the exact distribution. */
        uint32_t pool = 0;
        for (int i = 0; i < launch_.party_count; ++i) {
            const int idx = launch_.roster_slots[i];
            if (idx >= 0 && idx < MM2_ROSTER_RECORD_COUNT) {
                pool += roster_.records[idx].gold;
            }
        }
        if (pool < hireling_pay) {
            /* 0x19EA0 inline string. */
            mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 0);
            showStatusMessage("Not enough gold - Dismiss hirelings");
            return;
        }
        uint32_t remaining = hireling_pay;
        for (int i = 0; i < launch_.party_count && remaining > 0; ++i) {
            const int idx = launch_.roster_slots[i];
            if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
                continue;
            }
            const uint32_t take = roster_.records[idx].gold < remaining ? roster_.records[idx].gold
                                                                        : remaining;
            roster_.records[idx].gold -= take;
            remaining -= take;
        }
    }

    /* --- 0x19D64 rest-interruption (encounter) check ---------------------- */
    /* Conditions that suppress the encounter (all -> no roll): event tile, an
     * active guard dog, or a non-zero move counter -$796C. */
    const uint8_t move_counter = mm2_gs_u8(a4, -0x796C);
    mm2_gs_set_u8(a4, -0x796C, 0); /* 0x19D76: clr -$796C */
    const bool on_event_tile = (world_.collisionAt(gs_.coordX(), gs_.coordY()) & MM2_MAP_COLL_EVENT) != 0;
    const bool guard_dog = mm2_gs_u8(a4, MM2_GS_GUARD_DOG_FLAG) != 0;

    bool encounter = false;
    if (!on_event_tile && !guard_dog && move_counter == 0) {
        encounter = (rng_.range(1, 50) == 2); /* 0x19D98: rng(1,50)==2 */
    }
    if (encounter) {
        /* 0x19DAC: wakes the party (sets condition bit 4) and starts a fixed
         * fight (-$796B := 3, -$7EDE combat setup @ 0x051C2). Faithfully skip
         * the heal + day advance (rest interrupted). */
        mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 0);
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 3);
        for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
            mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
        }
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 0);
        combat_.enter(gs_, world_);
        markDirty();
        return;
    }

    /* --- 0x19B28 rest execution: clear buffs, heal, advance the clock ------ */
    /* 0x19B2C..0x19B5C: clear the 13 contiguous temporary-buff bytes
     * (-$79AB light .. -$799F wizard-eye), exactly the set the ASM zeroes. */
    for (int32_t off = -0x79AB; off <= -0x799F; ++off) {
        mm2_gs_set_u8(a4, off, 0);
    }

    for (int i = 0; i < launch_.party_count; ++i) {
        const int idx = launch_.roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        Mm2RosterRecord &rec = roster_.records[idx];

        if (rec.condition >= 0x80) {
            continue; /* 0x19B80: dead/stone/eradicated -> not healed. */
        }

        rec.condition &= 0x0D; /* 0x19B8C: keep bits 0,2,3; clear the rest. */

        /* 0x19B96: age >= 0x50 -> 50%-ish chance to fall ill (condition = 0x81). */
        if (rec.age >= 0x50 && rng_.range(1, 100) < 0x32) {
            rec.condition = 0x81;
        }

        if (rec.hp_max == 0) {
            rec.hp_max = 1; /* 0x19BC4 */
        }

        /* 0x19BD0: poisoned (condition bit 3) heals HP_current to half; otherwise
         * restore HP_current to the permanent max in +$60 (hp_aux). */
        if (rec.condition & 0x08) {
            rec.hp_current = static_cast<uint16_t>(rec.hp_current / 2);
        } else {
            rec.hp_current = rec.hp_aux;
        }

        /* 0x19C06: with no food, HP_current is restored but the rest of the
         * refresh (HP_max sync, food spend, SP) is skipped for this member. */
        if (rec.food == 0) {
            continue;
        }
        rec.food = static_cast<uint8_t>(rec.food - 1); /* 0x19C12 */

        if (!(rec.condition & 0x04)) {
            rec.hp_max = rec.hp_current; /* 0x19C2A */
        }

        /* 0x19C30 SP recompute: sp_current = (stat_bonus+3) * spell_level for
         * casters (+$23), then sp_max = sp_current. The bonus lookup -$7F56 and
         * the exact field semantics are not ported; restore SP to the stored max
         * (the rested end state for an undrained caster). DEFER exact recompute. */
        rec.sp_current = rec.sp_max;
    }

    /* 0x19CEC: advance the clock by 0x55 sub-day units (one rest = ~85 ticks),
     * rolling the calendar over so the day/night cycle progresses. */
    gameplay::advanceTimeTick(gs_, 0x55);

    /* DEFER 0x19CF6: era==9 endgame timeline-shuffle (rng(1,60) -> map reload). */

    mm2_gs_set_u8(a4, MM2_GS_PENDING_EVENT_LATCH, 1); /* dispatcher epilogue $1420 */
    mm2_gs_set_u8(a4, MM2_GS_BUSY_STATUS, 1);
    mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 0);

    showStatusMessage("Rest complete, no encounters."); /* 0x19D46 inline string */
}

void GameSession::tickOverlayInput(const platform::KeyState &keys)
{
    if (overlay_ == PlayOverlay::TownService) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        town_service_ui_.handleKey(ch, keys.escape);
        if (!town_service_ui_.active()) {
            overlay_ = PlayOverlay::None;
        }
        markDirty();
        return;
    }

    if (overlay_ == PlayOverlay::StatusMessage) {
        if (keys.escape || keys.any_key) {
            overlay_ = PlayOverlay::None;
            status_message_[0] = '\0';
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::Automap) {
        if (keys.escape) {
            overlay_ = PlayOverlay::None;
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::Controls) {
        if (keys.escape) {
            overlay_ = PlayOverlay::None;
            markDirty();
            return;
        }
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch >= '1' && ch <= '4') {
            controls_screen_.handleKey(ch, gs_);
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::RestConfirm) {
        /* 0x19E50 prompt loop: read a key, toupper, accept only Y / N (the ASM
         * loops on anything else). ESC is treated as N here for parity with the
         * other confirm overlays. */
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 'Y') {
            overlay_ = PlayOverlay::None;
            executeRest();
            markDirty();
            return;
        }
        if (keys.escape || ch == 'N') {
            /* 0x19E8E: repaint chrome, clear -$7950, abort. */
            mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 0);
            overlay_ = PlayOverlay::None;
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::QuitConfirm) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 'Y') {
            quit_ = true;
            overlay_ = PlayOverlay::None;
            return;
        }
        if (keys.escape || ch == 'N') {
            overlay_ = PlayOverlay::None;
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::QuickRef) {
        if (keys.escape) {
            overlay_ = PlayOverlay::None;
            markDirty();
            return;
        }
        if (keys.key_q && !keys.ctrl) {
            return;
        }

        int party_slot = -1;
        gameplay::PlaySessionAction action = gameplay::PlaySessionAction::None;
        if (gameplay::pollPlaySessionAction(keys, launch_.party_count, &action, &party_slot)) {
            if (action == gameplay::PlaySessionAction::ViewCharacter) {
                sheet_session_.party_slot = party_slot;
                sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
                sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
                sheet_session_.status_line[0] = '\0';
                combat_character_sheet_ = false;
                overlay_ = PlayOverlay::CharacterSheet;
                markDirty();
            }
        }
        return;
    }

    if (overlay_ == PlayOverlay::CharacterSheet) {
        if (keys.escape) {
            if (gameplay::sheetSubModeBlocksCharacterSwitch(sheet_session_.sub_mode)) {
                sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
                sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
                sheet_session_.status_line[0] = '\0';
                markDirty();
                return;
            }
            overlay_ = PlayOverlay::None;
            sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
            sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
            sheet_session_.status_line[0] = '\0';
            combat_character_sheet_ = false;
            markDirty();
            return;
        }
        if (keys.key_q && !keys.ctrl && !combat_character_sheet_) {
            overlay_ = PlayOverlay::QuickRef;
            markDirty();
            return;
        }

        const bool pending = gameplay::sheetSubModeBlocksCharacterSwitch(sheet_session_.sub_mode);
        if (!pending) {
            int party_slot = -1;
            gameplay::PlaySessionAction action = gameplay::PlaySessionAction::None;
            if (gameplay::pollPlaySessionAction(keys, launch_.party_count, &action, &party_slot)) {
                if (action == gameplay::PlaySessionAction::ViewCharacter) {
                    /* 0x907A digit chain: switch character without closing sheet. */
                    sheet_session_.party_slot = party_slot;
                    sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
                    sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
                    sheet_session_.status_line[0] = '\0';
                    markDirty();
                    return;
                }
            }
        }

        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 0) {
            return;
        }

        const Mm2ItemsFile *items_ptr = has_items_ ? &items_ : nullptr;
        const gameplay::SheetKeyOutcome outcome =
            ingame_sheet_.handleKey(ch, sheet_session_, roster_, launch_, items_ptr, combat_character_sheet_);
        if (outcome == gameplay::SheetKeyOutcome::Close) {
            overlay_ = PlayOverlay::None;
            combat_character_sheet_ = false;
        }
        markDirty();
        return;
    }
}

void GameSession::tickCombatCharacterSheetInput(const platform::KeyState &keys)
{
    if (keys.escape) {
        if (gameplay::sheetSubModeBlocksCharacterSwitch(sheet_session_.sub_mode)) {
            sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
            sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
            sheet_session_.status_line[0] = '\0';
            return;
        }
        overlay_ = PlayOverlay::None;
        combat_character_sheet_ = false;
        sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
        sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
        sheet_session_.status_line[0] = '\0';
        return;
    }

    const bool pending = gameplay::sheetSubModeBlocksCharacterSwitch(sheet_session_.sub_mode);
    if (!pending) {
        int party_slot = -1;
        gameplay::PlaySessionAction action = gameplay::PlaySessionAction::None;
        if (gameplay::pollPlaySessionAction(keys, launch_.party_count, &action, &party_slot)) {
            if (action == gameplay::PlaySessionAction::ViewCharacter) {
                sheet_session_.party_slot = party_slot;
                sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
                sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
                sheet_session_.status_line[0] = '\0';
                return;
            }
        }
    }

    const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
    if (ch == 0) {
        return;
    }

    const Mm2ItemsFile *items_ptr = has_items_ ? &items_ : nullptr;
    ingame_sheet_.handleKey(ch, sheet_session_, roster_, launch_, items_ptr, true);
}

void GameSession::tickPlayInput(const platform::KeyState &keys)
{
    if (keys.ctrl && keys.key_q) {
        overlay_ = PlayOverlay::QuitConfirm;
        markDirty();
        return;
    }

    int party_slot = -1;
    gameplay::PlaySessionAction action = gameplay::PlaySessionAction::None;
    if (!gameplay::pollPlaySessionAction(keys, launch_.party_count, &action, &party_slot)) {
        return;
    }

    switch (action) {
    case gameplay::PlaySessionAction::CtrlQuit:
        overlay_ = PlayOverlay::QuitConfirm;
        markDirty();
        break;
    case gameplay::PlaySessionAction::QuickRef:
        overlay_ = PlayOverlay::QuickRef;
        markDirty();
        break;
    case gameplay::PlaySessionAction::ViewCharacter:
        sheet_session_.party_slot = party_slot;
        sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
        sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
        sheet_session_.status_line[0] = '\0';
        combat_character_sheet_ = false;
        overlay_ = PlayOverlay::CharacterSheet;
        markDirty();
        break;
    case gameplay::PlaySessionAction::PanelOptions:
        if (right_panel_ == gfx::PlayRightPanel::Protect) {
            right_panel_ = gfx::PlayRightPanel::Options;
            gs_.setRightPanelMode(0);
            markDirty();
        }
        break;
    case gameplay::PlaySessionAction::PanelProtect:
        if (right_panel_ == gfx::PlayRightPanel::Options) {
            right_panel_ = gfx::PlayRightPanel::Protect;
            gs_.setRightPanelMode(1);
            markDirty();
        }
        break;
    case gameplay::PlaySessionAction::BashDoor:
    case gameplay::PlaySessionAction::Controls:
    case gameplay::PlaySessionAction::DismissHireling:
    case gameplay::PlaySessionAction::ExchangeOrder:
    case gameplay::PlaySessionAction::Automap:
    case gameplay::PlaySessionAction::Rest:
    case gameplay::PlaySessionAction::Search:
    case gameplay::PlaySessionAction::Unlock:
        handleExploreCommand(action);
        break;
    default:
        break;
    }
}

bool GameSession::awaitingContinuePrompt() const
{
    if (scripted_loaded_ && scripted_scene_.active()) {
        return true;
    }
    if (events_loaded_ && events_.blocksMovement()) {
        return true;
    }
    return false;
}

void GameSession::tickOverlayAnimations()
{
    if (viewportHiddenByOverlay()) {
#if MM2_HOST_AMIGA
        overlay_anim_dirty_ = false;
#endif
        return;
    }

    bool changed = false;
    if (combat_.active()) {
        refreshCombatSprite();
        changed |= combat_sprite_.tick();
    }
    if (scripted_loaded_ && scripted_scene_.active()) {
        changed |= scripted_scene_.tickAnimation();
    }
    if (events_loaded_ &&
        (events_.textView().hasServicePortrait() || events_.textView().hasPegasusIllustration())) {
        changed |= events_.textView().tickAnimation();
    }
    if (changed) {
#if MM2_HOST_AMIGA
        markOverlayAnimDirty();
#else
        markDirty();
#endif
    }
}

void GameSession::tickTorchAnimation()
{
    if (!assets_ok_ || world_.isOutdoor() || viewportHiddenByOverlay()) {
        return;
    }
    if (overlay_ == PlayOverlay::Automap) {
        return;
    }

    /* key_read_3d @0x1E9CE advances -$667A once per indoor input poll (~vblank). */
    constexpr int kTicksPerPhase = 2;
    ++torch_tick_;
    if (torch_tick_ < kTicksPerPhase) {
        return;
    }
    torch_tick_ = 0;
    torch_phase_ = (torch_phase_ + 1) % gfx::kView3DTorchPhaseCount;
#if MM2_HOST_AMIGA
    markView3DDirty();
#else
    markDirty();
#endif
}

void GameSession::tick(const platform::KeyState &keys)
{
    if (!gs_.valid()) {
        return;
    }

    tickOverlayAnimations();
    tickTorchAnimation();

    if (combat_.active()) {
        if (overlay_ == PlayOverlay::CharacterSheet) {
            tickCombatCharacterSheetInput(keys);
            markDirty();
            return;
        }
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 'V') {
            const int slot = combat_.activePartySlot();
            sheet_session_.party_slot = (slot >= 0 && slot < launch_.party_count) ? slot : 0;
            sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
            sheet_session_.trade_kind = gameplay::SheetTradeKind::None;
            sheet_session_.status_line[0] = '\0';
            combat_character_sheet_ = true;
            overlay_ = PlayOverlay::CharacterSheet;
            markDirty();
            return;
        }
        const bool ended = combat_.tick(gs_, world_, keys);
        markDirty();
        if (ended) {
            finishCombat();
        }
        return;
    }

    if (scripted_loaded_ && scripted_scene_.active()) {
        const bool was_active = scripted_scene_.active();
        scripted_scene_.tick(keys);
        if (was_active && !scripted_scene_.active()) {
            markDirty();
        }
        if (scripted_scene_.needsViewportRestore()) {
            scripted_scene_.clearViewportRestore();
#if MM2_HOST_AMIGA
            markView3DDirty();
#else
            markDirty();
#endif
        }
        return;
    }

    if (events_loaded_ && events_.blocksMovement()) {
        tickEventInput(keys);
        return;
    }

    /* 0x1290: clear -$4F4E once per play-loop iteration before input dispatch. */
    mm2_gs_set_u16(gs_.a4(), MM2_GS_ENCOUNTER_REDRAW, 0);

    maybeFinishInnRegistry();

    if (overlay_ != PlayOverlay::None) {
        tickOverlayInput(keys);
    } else {
        tickPlayInput(keys);
        if (overlay_ == PlayOverlay::None) {
            gameplay::ExploreCode code{};
            if (gameplay::pollExploreCode(keys, &code)) {
                gameplay::MoveResult move{};
                switch (code) {
                case gameplay::ExploreCode::TurnLeft:
                    move = gameplay::turn(world_, gs_, false);
                    break;
                case gameplay::ExploreCode::TurnRight:
                    move = gameplay::turn(world_, gs_, true);
                    break;
                case gameplay::ExploreCode::Forward:
                    maybeTriggerStepEncounter();
                    move = gameplay::step(world_, gs_, true);
                    break;
                case gameplay::ExploreCode::Back:
                    maybeTriggerStepEncounter();
                    move = gameplay::step(world_, gs_, false);
                    break;
                default:
                    break;
                }
                refreshWorldAfterMove(move);
            }
        }
    }

    /* Debug triggers: Ctrl+G = Corak scene, Ctrl+P = Pegasus scene (doc 46 demos). */
    if (overlay_ == PlayOverlay::None && scripted_loaded_ &&
        keys.ctrl && (keys.last_ascii == 'G' || keys.last_ascii == 'g')) {
        scripted_scene_.queueScene(events::ScriptedSceneId::CorakIntro);
        markDirty();
        return;
    }
    if (overlay_ == PlayOverlay::None && scripted_loaded_ &&
        keys.ctrl && (keys.last_ascii == 'P' || keys.last_ascii == 'p')) {
        scripted_scene_.queueScene(events::ScriptedSceneId::PegasusC2);
        markDirty();
        return;
    }

    runPendingEvents();
    if (combat_.active()) {
        return;
    }
    refreshWorldAfterEventTransition();
    maybeOpenTownServiceMenu();
}

void GameSession::maybeOpenTownServiceMenu()
{
    if (!town_service_ui_.pending() || overlay_ != PlayOverlay::None) {
        return;
    }
    /* ASM -$7ED8(5): clear (1,11)-(38,22) before shop menu; drop the intro
     * OP_02 layer so it does not paint over menu rows 0x11..0x16. */
    events_.textView().clearConsoleMessageLayers();
    town_service_ui_.begin();
    overlay_ = PlayOverlay::TownService;
    markDirty();
}

void GameSession::maybeFinishInnRegistry()
{
    if (!events_.takePendingInnGotoTown()) {
        return;
    }
    /* ASM @ 0x1A1CE: roster+$0B <- A4-$79F2+1 (town map index 0..4, not event.dat id). */
    const int map_id = static_cast<int>(gs_.screenId());
    events::eventInnApplyRegistry(&roster_, &launch_, map_id);
    if (map_id >= 0 && map_id < 5) {
        goto_town_filter_ = static_cast<uint8_t>(map_id + 1);
    }
    if (data_dir_) {
        char *const path = mm2_path_scratch_a();
        if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "roster.dat")) {
            (void)mm2_roster_save_file(path, &roster_);
        }
    }
    back_to_goto_town_ = true;
}

void GameSession::renderView3D()
{
    if (!assets_ok_) {
        compositor_.drawTextShadow(gfx::play_layout::kViewOriginX, gfx::play_layout::kViewOriginY,
                                   "(missing gfx / map.dat)", 200, 120, 120);
        return;
    }

    if (world_.isOutdoor()) {
        renderOutdoorView();
    } else {
        renderIndoorView3D();
    }
}

void GameSession::renderIndoorView3D()
{
    using namespace gfx;
    using namespace gfx::play_layout;

    View3DCamera camera{};
    camera.x = gs_.coordX();
    camera.y = gs_.coordY();
    camera.facing = gs_.facing03();

    const View3DMapBuffers bufs = world_.buildView3DMapBuffers();
    const View3DScene scene = buildView3DScene(bufs, camera);

    const int sky_frame = world_.roofBitAt(camera.x, camera.y) ? 1 : 0;
    blitImageFrame(compositor_, env_.floor(), 0, kView3DOriginX, kView3DFloorY, 1);
    blitImageFrame(compositor_, env_.sky(), sky_frame, kView3DOriginX, kView3DSkyY, 1);

    for (int i = 0; i < scene.num_blits; ++i) {
        const View3DBlit &b = scene.blits[static_cast<size_t>(i)];
        blitImageFrame(compositor_, env_.walls(), b.frame, b.x, b.y, 0);
    }
    for (int i = 0; i < scene.num_torch_blits; ++i) {
        const View3DBlit &b = scene.torch_blits[static_cast<size_t>(i)];
        View3DTorchBlit tb{};
        if (view3dTorchBlitFor(b, torch_phase_, &tb) && env_.torches().img.frame_count > 0) {
            blitImageFrame(compositor_, env_.torches(), tb.frame, tb.x, tb.y, 0);
        }
    }
}

void GameSession::renderOutdoorView()
{
    using namespace gfx;
    using namespace gfx::play_layout;

    View3DCamera camera{};
    camera.x = gs_.coordX();
    camera.y = gs_.coordY();
    camera.facing = gs_.facing03();

    /* Paint order @ outdoor_3d_master 0x18870: floor, sky, decor, horizon layers. */
    blitImageFrame(compositor_, env_.floor(), 0, kView3DOriginX, kView3DFloorY, 1);

    /* Day vs night sky @0x18898: cmpi.w #$80,-$79b4 — subday < 0x80 draws the
     * sky.32 backdrop; >= 0x80 (night) fills the band black + plots stars
     * (night routine @0x0687C). */
    const uint16_t subday = mm2_gs_u16(gs_.a4(), MM2_GS_TIME_SUBDAY);
    if (subday < static_cast<uint16_t>(kOutdoorNightSubdayThreshold)) {
        blitImageFrame(compositor_, env_.sky(), 0, kView3DOriginX, kView3DSkyY, 1);
    } else {
        const mm2_image32_file &sky = env_.sky().img;
        auto penRgb = [&sky](uint8_t pen, uint8_t &r, uint8_t &g, uint8_t &b) {
            const uint8_t *c = sky.palette_rgba[pen & (MM2_IMAGE32_PALETTE_COLORS - 1)];
            r = c[0];
            g = c[1];
            b = c[2];
        };
        uint8_t fr = 0, fg = 0, fb = 0;
        penRgb(kOutdoorSkyFillPen, fr, fg, fb);
        compositor_.fillRect(kOutdoorSkyFillX, kOutdoorSkyFillY, kOutdoorSkyFillW, kOutdoorSkyFillH,
                             fr, fg, fb);

        std::array<OutdoorStarBlit, kOutdoorStarCount> stars{};
        const int nstars = buildOutdoorStars(camera.facing, stars);
        for (int i = 0; i < nstars; ++i) {
            uint8_t r = 0, g = 0, b = 0;
            penRgb(stars[static_cast<size_t>(i)].pen, r, g, b);
            compositor_.fillRect(stars[static_cast<size_t>(i)].x, stars[static_cast<size_t>(i)].y, 1, 1,
                                 r, g, b);
        }
    }

    const OutdoorScene scene = buildOutdoorScene(world_, camera);

    for (int i = 0; i < scene.num_decor; ++i) {
        const OutdoorSpriteBlit &b = scene.decor[static_cast<size_t>(i)];
        blitImageFrame(compositor_, env_.biomeSheet(b.biome), b.frame, b.x, b.y, 0);
    }
    for (int i = 0; i < scene.num_horizon; ++i) {
        const OutdoorSpriteBlit &b = scene.horizon[static_cast<size_t>(i)];
        blitImageFrame(compositor_, env_.horizonSheet(b.horizon), b.frame, b.x, b.y, 0);
    }
}

void GameSession::renderCombatBackdrop()
{
    using namespace gfx;
    using namespace gfx::play_layout;

    if (!assets_ok_) {
        gfx::drawCombatViewport(compositor_);
        return;
    }

    if (world_.isOutdoor()) {
        View3DCamera camera{};
        camera.x = gs_.coordX();
        camera.y = gs_.coordY();
        camera.facing = gs_.facing03();

        blitImageFrame(compositor_, env_.floor(), 0, kView3DOriginX, kView3DFloorY, 1);

        const uint16_t subday = mm2_gs_u16(gs_.a4(), MM2_GS_TIME_SUBDAY);
        if (subday < static_cast<uint16_t>(kOutdoorNightSubdayThreshold)) {
            blitImageFrame(compositor_, env_.sky(), 0, kView3DOriginX, kView3DSkyY, 1);
        } else {
            const mm2_image32_file &sky = env_.sky().img;
            auto penRgb = [&sky](uint8_t pen, uint8_t &r, uint8_t &g, uint8_t &b) {
                const uint8_t *c = sky.palette_rgba[pen & (MM2_IMAGE32_PALETTE_COLORS - 1)];
                r = c[0];
                g = c[1];
                b = c[2];
            };
            uint8_t fr = 0, fg = 0, fb = 0;
            penRgb(kOutdoorSkyFillPen, fr, fg, fb);
            compositor_.fillRect(kOutdoorSkyFillX, kOutdoorSkyFillY, kOutdoorSkyFillW, kOutdoorSkyFillH, fr, fg,
                                 fb);

            std::array<OutdoorStarBlit, kOutdoorStarCount> stars{};
            const int nstars = buildOutdoorStars(camera.facing, stars);
            for (int i = 0; i < nstars; ++i) {
                uint8_t r = 0, g = 0, b = 0;
                penRgb(stars[static_cast<size_t>(i)].pen, r, g, b);
                compositor_.fillRect(stars[static_cast<size_t>(i)].x, stars[static_cast<size_t>(i)].y, 1, 1, r,
                                     g, b);
            }
        }

        const OutdoorScene scene = buildOutdoorScene(world_, camera);
        for (int i = 0; i < scene.num_decor; ++i) {
            const OutdoorSpriteBlit &b = scene.decor[static_cast<size_t>(i)];
            blitImageFrame(compositor_, env_.biomeSheet(b.biome), b.frame, b.x, b.y, 0);
        }
        for (int i = 0; i < scene.num_horizon; ++i) {
            const OutdoorSpriteBlit &b = scene.horizon[static_cast<size_t>(i)];
            blitImageFrame(compositor_, env_.horizonSheet(b.horizon), b.frame, b.x, b.y, 0);
        }
        return;
    }

    View3DCamera camera{};
    camera.x = gs_.coordX();
    camera.y = gs_.coordY();
    camera.facing = gs_.facing03();
    const int sky_frame = world_.roofBitAt(camera.x, camera.y) ? 1 : 0;
    blitImageFrame(compositor_, env_.floor(), 0, kView3DOriginX, kView3DFloorY, 1);
    blitImageFrame(compositor_, env_.sky(), sky_frame, kView3DOriginX, kView3DSkyY, 1);
}

void GameSession::renderPartyPanel()
{
    gfx::PlayPartySlot slots[8]{};
    for (int i = 0; i < launch_.party_count && i < 8; ++i) {
        const int slot = launch_.roster_slots[i];
        if (slot < 0 || slot >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }

        const Mm2RosterRecord &rec = roster_.records[slot];
        slots[i].present = true;
        mm2_roster_name_to_cstr(&rec, slots[i].name, sizeof(slots[i].name));
        slots[i].hp = rec.hp_current;
        slots[i].bad_condition = rec.condition != 0;
    }

    gfx::drawPlayPartyPanel(compositor_, slots);
}

void GameSession::refreshCombatSprite()
{
    if (!combat_.active() || !data_dir_ || !has_monsters_) {
        return;
    }
    const gfx::CombatPanelView view = combat_.panelView();
    if (view.sprite_disk_index < 0) {
        return;
    }
    if (view.sprite_disk_index == combat_sprite_disk_ && combat_sprite_.loaded()) {
        return;
    }
    combat_sprite_.unload();
    combat_sprite_disk_ = view.sprite_disk_index;
    if (combat_sprite_.loadFromDiskIndex(data_dir_, combat_sprite_disk_, gfx::AnmLoopMode::Loop, false)) {
        /* playback starts at frame 0 from loadFromDiskIndex */
    } else {
        combat_sprite_disk_ = -1;
    }
}

void GameSession::renderOverlays()
{
    switch (overlay_) {
    case PlayOverlay::QuickRef:
        ingame_sheet_.renderQuickRef(compositor_, roster_, launch_);
        break;
    case PlayOverlay::CharacterSheet:
        ingame_sheet_.renderSheet(compositor_, roster_, launch_, sheet_session_.party_slot,
                                  has_items_ ? &items_ : nullptr, &sheet_session_, combat_character_sheet_);
        break;
    case PlayOverlay::Controls:
        controls_screen_.render(compositor_, gs_);
        break;
    case PlayOverlay::StatusMessage:
        /* Status row 0x11 — same clear as EventTextView Op01 / y/n bar (doc 44). */
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, status_message_, 255, 255, 128, 255);
        compositor_.drawText(8, 18 * 8, "(ESC to dismiss)", 180, 180, 180, 255);
        break;
    case PlayOverlay::Automap:
        gfx::renderAutomap(compositor_, env_, world_, automap_, gs_);
        break;
    case PlayOverlay::QuitConfirm:
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, "Quit without game save (y/n)?", 255, 80, 80, 255);
        break;
    case PlayOverlay::RestConfirm:
        /* 0x19ECB inline string, drawn on the status row like the other prompts. */
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, "Rest here? (Y/N)", 255, 255, 128, 255);
        break;
    case PlayOverlay::TownService:
        town_service_ui_.render(compositor_);
        break;
    default:
        break;
    }
}

void GameSession::renderFrame(bool overlay_anim_only)
{
    const bool scripted_active = scripted_loaded_ && scripted_scene_.active();
    const bool combat_active = combat_.active();

    /* Chrome (red border + black fills) is cached in chip RAM and blitted each
     * frame so double-buffer swaps do not drop HUD glyphs outside the 3D cache. */
#if MM2_HOST_AMIGA
    if (!overlay_anim_only) {
        if (chrome_dirty_ || !mm2_amiga_play_chrome_cache_ready()) {
            if (!mm2_amiga_play_chrome_cache_ready()) {
                mm2_amiga_play_chrome_cache_create();
            }
            mm2_amiga_play_chrome_cache_begin();
            gfx::drawPlayScreenChromeStatic(compositor_);
            mm2_amiga_play_chrome_cache_end();
        }
        mm2_amiga_play_chrome_cache_present();
    }
#else
    (void)overlay_anim_only;
    compositor_.clear(0, 0, 0, 255);
    gfx::drawPlayScreenChrome(compositor_);
#endif

    if (combat_active) {
#if MM2_HOST_AMIGA
        if (!combat_backdrop_cached_) {
            mm2_amiga_restore_play_world_palette();
            renderCombatBackdrop();
            mm2_amiga_viewport_cache_save();
            combat_backdrop_cached_ = true;
        } else {
            mm2_amiga_viewport_cache_restore();
        }
#else
        renderCombatBackdrop();
#endif
        refreshCombatSprite();
        if (combat_sprite_.loaded()) {
            combat_sprite_.blitCentered(compositor_, 0);
        }
        gfx::drawPlayViewportDivider(compositor_);
    } else if (!scripted_active || !scripted_scene_.hidesView3D()) {
        if (overlay_ != PlayOverlay::Automap && !viewportHiddenByOverlay()) {
#if MM2_HOST_AMIGA
            if (overlay_anim_only) {
                /* Retail buf_copy_rect @ 0x171AC: restore the saved 3D viewport
                 * instead of rebuilding floor+sky+wall slices every ghost cel. */
                mm2_amiga_viewport_cache_restore();
            } else {
                renderView3D();
                drawSpellEyeOverlayIfActive(compositor_, env_, world_, gs_, assets_ok_);
                mm2_amiga_viewport_cache_save();
            }
#else
            renderView3D();
            drawSpellEyeOverlayIfActive(compositor_, env_, world_, gs_, assets_ok_);
#endif
            /* Walls blit past x=216 and erase the viewport/right-column divider. */
            gfx::drawPlayViewportDivider(compositor_);
        }
    }

    if (!combat_active) {
        gfx::PlayRightPanel panel = right_panel_;
        if (scripted_active) {
            panel = scripted_scene_.panelMode() == 0 ? gfx::PlayRightPanel::Options : gfx::PlayRightPanel::Protect;
        }

        const bool protect_panel = panel == gfx::PlayRightPanel::Protect;
        gfx::drawPlayStatusBar(compositor_, gs_.valid() ? gs_.day() : 0, gs_.valid() ? gs_.year() : 0,
                               gs_.valid() ? gs_.facingKey() : 'N', protect_panel);
    }

    renderPartyPanel();

    if (combat_active) {
        const gfx::CombatPanelView combat_view = combat_.panelView();
        gfx::drawCombatMonsterList(compositor_, combat_view);
        gfx::drawCombatOptionsBar(compositor_, combat_view);
    } else {
        gfx::PlayRightPanel panel = right_panel_;
        if (scripted_active) {
            panel = scripted_scene_.panelMode() == 0 ? gfx::PlayRightPanel::Options : gfx::PlayRightPanel::Protect;
        }

        if (overlay_ == PlayOverlay::None || overlay_ == PlayOverlay::StatusMessage ||
            overlay_ == PlayOverlay::TownService) {
            /* Town services render in the lower console band only (faithful, not a
             * fullscreen modal) — keep the 3D view + right column on screen. */
            const bool protect_panel = panel == gfx::PlayRightPanel::Protect;
            const gfx::PlayProtectValues prot = protectValues();
            gfx::drawPlayRightColumn(compositor_, panel, protect_panel ? &prot : nullptr);
        }
    }

    if (scripted_active) {
        scripted_scene_.draw(compositor_);
    }

    renderOverlays();

    /* OP_04 door labels / OP_05/06/0B viewport overlays must not paint over modal
     * sheets (character sheet, quick ref, etc.) — retail hides the 3D hood entirely. */
    /* OP_0B service sign stays visible during shop menus; drop only OP_02 intro text. */
    if (!combat_active && events_loaded_ && !scripted_active && !viewportHiddenByOverlay()) {
        if (overlay_ == PlayOverlay::TownService) {
            events_.textView().drawPersistentViewportOverlays(compositor_);
            events_.textView().drawServiceSignOverlay(compositor_);
            events_.textView().drawPegasusIllustration(compositor_);
        } else {
            events_.textView().draw(compositor_);
        }
    }
}

bool GameSession::viewportHiddenByOverlay() const
{
    switch (overlay_) {
    case PlayOverlay::QuickRef:
    case PlayOverlay::CharacterSheet:
    case PlayOverlay::Controls:
    case PlayOverlay::QuitConfirm:
    case PlayOverlay::Automap:
        return true;
    /* TownService is NOT here: its menu draws only in the lower console band, so
     * the 3D viewport stays visible (faithful non-fullscreen presentation). */
    default:
        return false;
    }
}

#if MM2_HOST_AMIGA
bool GameSession::canUsePartialView3DRefresh() const
{
    if (!play_buffer_valid_ || !view3d_dirty_ || chrome_dirty_) {
        return false;
    }
    if (!assets_ok_ || overlay_ != PlayOverlay::None) {
        return false;
    }
    if (scripted_loaded_ && scripted_scene_.active()) {
        return false;
    }
    return true;
}

void GameSession::renderFrameOverlayAnimOnly()
{
    if (!mm2_amiga_copy_front_to_back()) {
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    if (overlay_ != PlayOverlay::Automap) {
        mm2_amiga_viewport_cache_restore();
        gfx::drawPlayViewportDivider(compositor_);
    }

    if (scripted_loaded_ && scripted_scene_.active()) {
        scripted_scene_.drawViewportSpriteOverlays(compositor_);
    } else if (events_loaded_) {
        events_.textView().drawPersistentViewportOverlays(compositor_);
        events_.textView().drawServiceSignOverlay(compositor_);
        events_.textView().drawPegasusIllustration(compositor_);
    }
}

void GameSession::renderFrameCombatAnimOnly()
{
    if (!mm2_amiga_copy_front_to_back()) {
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    mm2_amiga_viewport_cache_restore();
    refreshCombatSprite();
    if (combat_sprite_.loaded()) {
        combat_sprite_.blitCentered(compositor_, 0);
    }
    gfx::drawPlayViewportDivider(compositor_);
}

void GameSession::renderFrameView3DOnly()
{
    if (!mm2_amiga_copy_front_to_back()) {
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    if (overlay_ != PlayOverlay::Automap) {
        renderView3D();
        drawSpellEyeOverlayIfActive(compositor_, env_, world_, gs_, assets_ok_);
        mm2_amiga_viewport_cache_save();
        gfx::drawPlayViewportDivider(compositor_);
    }

    if (text_dirty_) {
        const bool protect_panel = right_panel_ == gfx::PlayRightPanel::Protect;
        gfx::drawPlayStatusBar(compositor_, gs_.valid() ? gs_.day() : 0, gs_.valid() ? gs_.year() : 0,
                               gs_.valid() ? gs_.facingKey() : 'N', protect_panel);
    }

    if (events_loaded_) {
        events_.textView().drawPersistentViewportOverlays(compositor_);
        events_.textView().drawServiceSignOverlay(compositor_);
        events_.textView().drawPegasusIllustration(compositor_);
    }
}
#endif

void GameSession::render()
{
#if MM2_HOST_AMIGA
    /* Sign / portrait / combat monster cel tick: copy HUD from front, restore
     * cached viewport hood, repaint animated overlay only. */
    if (overlay_anim_dirty_ && !view3d_dirty_ && !chrome_dirty_ && !text_dirty_ &&
        !viewportHiddenByOverlay()) {
        if (combat_.active() && combat_backdrop_cached_) {
            renderFrameCombatAnimOnly();
            play_buffer_valid_ = true;
            return;
        }
        if (mm2_amiga_viewport_cache_valid()) {
            renderFrameOverlayAnimOnly();
            play_buffer_valid_ = true;
            return;
        }
    }

    if (canUsePartialView3DRefresh()) {
        renderFrameView3DOnly();
        play_buffer_valid_ = true;
        return;
    }
#endif
    renderFrame(false);
#if MM2_HOST_AMIGA
    play_buffer_valid_ = true;
#endif
}

}  // namespace mm2
