#include "mm2/GameSession.h"

#include "mm2/Mm2Dbg.h"
#include "mm2/CppStdCompat.h"

#include "mm2/DataPath.h"
#include "mm2/combat/EncounterPicker.h"
#include "mm2/events/EventTownServices.h"
#include "mm2/events/EventCombatEncounter.h"
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

#include "mm2/platform/Audio.h"
#include "mm2/platform/Platform.h"
#if MM2_HOST_AMIGA
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

void maskCombatBackdropBleed(gfx::ScreenCompositor &c)
{
    using namespace gfx::play_layout;
    /* Narrow hood ends at x=origin+viewportW (col 0x0F). Roster starts at
     * col 0x10. Only black the divider column — NEVER (screenW - mask_x), which
     * wiped D-Delay/monster list every combat frame and stalled the blitter. */
    const int mask_x = gfx::kView3DOriginX + kCombatView3DViewportW;
    const int mask_w = kCombatRightCol * 8 - mask_x;
    if (mask_w > 0) {
        c.fillRect(mask_x, kCombatView3DSkyY, mask_w, kCombatView3DViewportH, 0, 0, 0);
    }
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
    combat_backdrop_cached_ = false;
    combat_hud_sigs_valid_ = false;
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

void GameSession::markCombatHudDirty()
{
    /* Retail combat waits (0x6798 / 0x119C2) and round patches only redraw
     * message band + monster column + party strip — never full chrome / 3D. */
    text_dirty_ = true;
}
#endif

namespace {

#if MM2_HOST_AMIGA
uint32_t combatRosterHudSig(const gfx::CombatPanelView &view, int highlight)
{
    uint32_t h = 2166136261u;
    h ^= static_cast<uint32_t>(highlight + 1);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(view.monster_line_count);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(view.overflow_more);
    h *= 16777619u;
    for (int i = 0; i < view.monster_line_count && i < 10; ++i) {
        const gfx::CombatMonsterLine &line = view.monster_lines[i];
        h ^= line.occupied ? 1u : 0u;
        h *= 16777619u;
        h ^= line.show_checkmark ? 1u : 0u;
        h *= 16777619u;
        h ^= line.highlighted ? 1u : 0u;
        h *= 16777619u;
        for (const char *p = line.name; *p; ++p) {
            h ^= static_cast<uint8_t>(*p);
            h *= 16777619u;
        }
        for (const char *p = line.status_suffix; *p; ++p) {
            h ^= static_cast<uint8_t>(*p);
            h *= 16777619u;
        }
    }
    return h;
}

uint32_t combatPartyHudSig(const Mm2RosterFile &roster, const Mm2PartyLaunch &launch)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < launch.party_count && i < MM2_GS_PARTY_SIZE; ++i) {
        const int idx = launch.roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            h ^= 0xA5u;
            h *= 16777619u;
            continue;
        }
        const Mm2RosterRecord &rec = roster.records[idx];
        h ^= static_cast<uint32_t>(rec.hp_max);
        h *= 16777619u;
        h ^= static_cast<uint32_t>(rec.condition);
        h *= 16777619u;
    }
    return h;
}
#endif

}  // namespace

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

    /* Amiga seeds A4-$60E2 via 0x2407C; a fixed ctor seed made every launch
     * replay the same encounter/step-random sequence. Mix VBlank clock. */
    {
        const uint32_t ticks = platform::nowTicks();
        const uint32_t seed = (ticks ? ticks : 1u) ^ 0xA5A5A5A5u ^ (start_flags * 0x9E3779B9u);
        rng_.reseed(seed ? seed : 1u);
    }

    /* Stock roster.dat keeps inn-stored starters at hp_current(+$74)=0 with
     * condition bit6 (0x40 unconscious). The party strip draws working HP at
     * +$5E (codec hp_max), so they look healthy, but encounter_xp_budget_init
     * @ 0x11E58 sums the +$74 ceiling — budget 0 → empty random fights. Wake
     * them on leave-inn / play start the same way Rest does (0x19BFC: restore
     * +$74 from +$60 aux, then 0x19C2A sync +$5E ← +$74 when not diseased). */
    for (int i = 0; i < launch_.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch_.roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        Mm2RosterRecord &rec = roster_.records[idx];
        if (rec.condition >= 0x80) {
            continue; /* dead/stoned stay down */
        }
        if (rec.hp_current == 0) {
            const uint16_t restore = rec.hp_aux != 0 ? rec.hp_aux : rec.hp_max;
            if (restore != 0) {
                rec.hp_current = restore;
                if (rec.hp_max == 0) {
                    rec.hp_max = restore;
                }
            }
        }
        /* Ensure working HP is present for combat (0x4AAA damages +$5E). */
        if (rec.hp_max == 0 && rec.hp_current != 0) {
            rec.hp_max = rec.hp_current;
        }
        rec.condition = static_cast<uint8_t>(rec.condition & static_cast<uint8_t>(~0x40u));
        /* Stock starters (Gene Eric / Cassandra) ship +$20=1 while +$71=4.
         * Rest @ 0x19C9A multiplies SP by +$20 — align working copies now. */
        gameplay::syncRosterWorkingLevelFields(rec);
    }

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
    torch_phase_ = 0;
    torch_tick_ = 0;
    hood_has_torches_ = false;
    hood_torch_cache_valid_ = false;

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
    events::eventVmInitStrBankOffsets(gs_.a4()); /* 0x9666 bank table A4-$71E8 */
    mm2_party_launch_apply(gs_.a4(), &launch_);
    /* Reload path @ 0x86F6 parses the roster.dat global tail back into A4;
     * restore the 24-byte event/quest bank (-$798B) so quest flags and
     * hireling A..X unlocks persist across sessions. */
    for (int i = 0; i < MM2_ROSTER_TAIL_EVENT_BANK_LEN; ++i) {
        mm2_gs_set_u8(gs_.a4(), MM2_GS_EVENT_VAR_BANK + i,
                      mm2_roster_tail_u8(&roster_, MM2_ROSTER_TAIL_EVENT_BANK + i));
    }

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
    combat_.bindItems(has_items_ ? &items_ : nullptr);
    combat_.bindRng(&rng_);

    const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
    bool has_env = false;
    bool has_sky = false;
    if (has_world) {
        gameplay::materializeScreenAttrib(gs_, world_);
        gameplay::syncCurrentCellFlags(gs_, world_);
        gameplay::sessionInteractionGate(gs_);
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
        combat_.bindItems(has_items_ ? &items_ : nullptr);
        combat_.bindRng(&rng_);
        bootstrap_step_ = 1;
        markDirty();
        return;
    case 1: {
        MM2_DBG("MM2 GOTO: bootstrap world load screen=%d\n", gs_.screenId());
        const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
        if (has_world) {
            gameplay::materializeScreenAttrib(gs_, world_);
            gameplay::syncCurrentCellFlags(gs_, world_);
            gameplay::sessionInteractionGate(gs_);
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
    clearSearchContainerArt();
    if (has_endgame_) {
        mm2_image32_free(&endgame_);
        has_endgame_ = false;
    }
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
    hood_torch_cache_valid_ = false;

    if (move.acted && data_dir_) {
        if (move.screen_changed) {
            world_.enterScreen(gs_.screenId());
            gameplay::materializeScreenAttrib(gs_, world_);
            const bool has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
            assets_ok_ = world_.loaded() && env_.loadGlobal(data_dir_) && has_env;
            combat::encounterSyncScreenContext(gs_, world_);
            if (events_loaded_) {
                refreshEventsForScreen();
            }
        }
        if (move.moved || move.turned || move.screen_changed) {
            gameplay::syncCurrentCellFlags(gs_, world_);
            gameplay::sessionInteractionGate(gs_);
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
    if (!gs_.valid()) {
        return;
    }

    /* Corak copy comes from event.dat (Middlegate evt 18 @ (7,4) N → OP_0E 0x09
     * → loc-60 OP_03 str[1]), not a ScriptedSceneEngine string-index shortcut.
     * Area-enter first_time_flag @ 0x6E84 still arms so retail gates fire. */
    if (on_start && gs_.screenId() == 0) {
        gs_.setFirstTimeFlag(true);
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
    gameplay::materializeScreenAttrib(gs_, world_);
    gameplay::syncCurrentCellFlags(gs_, world_);
    gameplay::sessionInteractionGate(gs_);
    const bool has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
    assets_ok_ = world_.loaded() && env_.loadGlobal(data_dir_) && has_env;
    combat::encounterSyncScreenContext(gs_, world_);
    if (events_loaded_) {
        /* OP_0C map_transition: drop OP_0B portraits / popups from the prior screen
         * (enterLocation → text_.reset). Without this, Middlegate shop .anm overlays
         * the overland viewport after exiting town. */
        refreshEventsForScreen();
    }
    /* Keep AnmPlanarPool chip cels across map changes - reloading NN.anm
     * on every shop/town hop is the main Amiga hitch. Evict only when full. */
    hood_torch_cache_valid_ = false;
    markDirty();
}

void GameSession::finishCombat()
{
    unloadCombatSprites();
    /* Leave unreferenced combat .anm cels in AnmPlanarPool for the next fight. */
    combat_character_sheet_ = false;
    if (overlay_ == PlayOverlay::CharacterSheet) {
        overlay_ = PlayOverlay::None;
    }
#if MM2_HOST_AMIGA
    combat_backdrop_cached_ = false;
    mm2_amiga_viewport_cache_set_size(208, 120);
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

    /* Total wipe: retail plays funeral tones @ 0x7DCC then leaves corpses at the
     * entry square for temple raise. Remake must not persist an all-dead party
     * (finishCombat used to save immediately → soft-lock saves). */
    const bool total_wipe =
        combat_.lastOutcome() == combat::CombatOutcome::Defeated && partyAllDead();
    if (!total_wipe) {
        saveRosterWithGlobalTail(); /* persist combat XP/gold/HP */
    }

    runPendingEvents();
    refreshWorldAfterEventTransition();
    /* Defeat @ 0x1164A may have restored coords from -$560C — refresh cell/gate. */
    if (world_.loaded()) {
        gameplay::syncCurrentCellFlags(gs_, world_);
        gameplay::sessionInteractionGate(gs_);
    }
    if (total_wipe) {
        beginPartyWipeGameOver();
    } else if (fd_await_combat_) {
        resumeOp0eFdAfterCombat();
    }
    markDirty();
}

void GameSession::saveRosterWithGlobalTail()
{
    if (!data_dir_) {
        return;
    }
    /* Mirror of the save_game_state serializer (@ 0x823C stream order): the
     * party roster-index table + size and the 24-byte event/quest bank live in
     * the roster.dat global tail, so party selection and hireling A..X
     * availability survive relaunch. */
    for (int i = 0; i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int16_t slot = launch_.roster_slots[i];
        const uint16_t word =
            (i < launch_.party_count && slot >= 0) ? static_cast<uint16_t>(slot) : 0xFFFFu;
        mm2_roster_tail_set_u16(&roster_, MM2_ROSTER_TAIL_PARTY_ROSTER_IDX + i * 2, word);
    }
    mm2_roster_tail_set_u16(&roster_, MM2_ROSTER_TAIL_PARTY_SIZE,
                            static_cast<uint16_t>(launch_.party_count));
    if (gs_.valid()) {
        for (int i = 0; i < MM2_ROSTER_TAIL_EVENT_BANK_LEN; ++i) {
            mm2_roster_tail_set_u8(&roster_, MM2_ROSTER_TAIL_EVENT_BANK + i,
                                   mm2_gs_u8(gs_.a4(), MM2_GS_EVENT_VAR_BANK + i));
        }
    }
    char *const path = mm2_path_scratch_a();
    if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "roster.dat")) {
        (void)mm2_roster_save_file(path, &roster_);
    }
}

bool GameSession::partyAllDead() const
{
    return events::eventVmCountLivingPartyMembers(gs_.valid() ? gs_.a4() : nullptr, &roster_,
                                                  &launch_) == 0;
}

void GameSession::beginPartyWipeGameOver()
{
    if (overlay_ == PlayOverlay::GameOver) {
        return;
    }
    /* 0x7DCC funeral tones are generative play_tone voices; remake stand-in is
     * play_sound_seq id 6 (same family as party KO @ 0xFE0C). */
    audio::playSoundSeq(6, gs_.soundsEnabled(), gs_.walkBeepEnabled());
    /* 0x11646 / 0x7DCC funeral presentation — message band, then title on dismiss. */
    std::snprintf(status_message_, sizeof(status_message_),
                  "The party has been defeated...\nAll members have died!");
    overlay_ = PlayOverlay::GameOver;
}

void GameSession::maybeBeginPartyWipeGameOver()
{
    /* Overworld / post-event wipe @ 0x9F22 — same funeral as combat defeat. */
    if (overlay_ == PlayOverlay::GameOver || combat_.active()) {
        return;
    }
    if (partyAllDead()) {
        beginPartyWipeGameOver();
    }
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
        maybeBeginPartyWipeGameOver();
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
    const int entry_rev_before = events_.textView().textEntryRevision();
    events_.continueInput(gs_, world_, keys);
    refreshWorldAfterEventTransition();
    maybeFinishInnRegistry();
    maybeOpenDeathStrikes();
    maybeOpenFdPrintChrome();
    maybeOpenTownServiceMenu();
    if (events_.blocksMovement() != blocking_before || events_.textView().layerCount() != layers_before ||
        events_.textView().textEntryRevision() != entry_rev_before) {
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

void GameSession::beginExchangeOrder()
{
    /* 0x20F58: prompt both slots via 0x20DE0 (arg=$FF). Host: status-line digits. */
    if (launch_.party_count < 1) {
        return;
    }
    exchange_first_slot_ = -1;
    std::snprintf(status_message_, sizeof(status_message_), "Exchange (1 - %d)",
                  launch_.party_count);
    overlay_ = PlayOverlay::ExchangeOrder;
    markDirty();
}

void GameSession::exchangeExplorePartySlots(int slot_a, int slot_b)
{
    /* 0x20F94..0x20FD0: swap word -$796A[a] ↔ -$796A[b]. */
    if (slot_a < 0 || slot_b < 0 || slot_a >= launch_.party_count || slot_b >= launch_.party_count) {
        return;
    }
    if (slot_a == slot_b) {
        return;
    }
    const int16_t tmp = launch_.roster_slots[slot_a];
    launch_.roster_slots[slot_a] = launch_.roster_slots[slot_b];
    launch_.roster_slots[slot_b] = tmp;
    const uint16_t wa = mm2_gs_u16(gs_.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_a * 2);
    const uint16_t wb = mm2_gs_u16(gs_.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_b * 2);
    mm2_gs_set_u16(gs_.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_a * 2, wb);
    mm2_gs_set_u16(gs_.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_b * 2, wa);
    /* 0x20FE0: jsr -$7EA2 party panel redraw when slots differ. */
}

void GameSession::beginDismissHireling()
{
    /* 0x141F4: "Dismiss whom (1-N)?"; only roster index >= $18. */
    if (launch_.party_count < 1) {
        return;
    }
    mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 1); /* 0x1421E -$7950 := 1 */
    std::snprintf(status_message_, sizeof(status_message_), "Dismiss whom (1 - %d)?",
                  launch_.party_count);
    overlay_ = PlayOverlay::DismissHireling;
    markDirty();
}

bool GameSession::removeHirelingFromParty(int16_t roster_index)
{
    /* 0x362C: find slot with matching -$796A word; FFFF it; compact; dec count. */
    if (roster_index < 0) {
        return false;
    }
    int found = -1;
    for (int i = 0; i < launch_.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        if (launch_.roster_slots[i] == roster_index) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        return false;
    }
    for (int i = found; i < launch_.party_count - 1 && i < MM2_PARTY_LAUNCH_SLOTS - 1; ++i) {
        launch_.roster_slots[i] = launch_.roster_slots[i + 1];
        const uint16_t w = mm2_gs_u16(gs_.a4(), MM2_GS_ROSTER_INDEX_TBL + (i + 1) * 2);
        mm2_gs_set_u16(gs_.a4(), MM2_GS_ROSTER_INDEX_TBL + i * 2, w);
    }
    const int last = launch_.party_count - 1;
    if (last >= 0 && last < MM2_PARTY_LAUNCH_SLOTS) {
        launch_.roster_slots[last] = -1;
        mm2_gs_set_u16(gs_.a4(), MM2_GS_ROSTER_INDEX_TBL + last * 2, 0xFFFF);
    }
    /* 0x36A0: -$795C := $FFFF; subq -$795A. */
    mm2_gs_set_u16(gs_.a4(), -0x795C, 0xFFFF);
    if (launch_.party_count > 0) {
        --launch_.party_count;
    }
    mm2_gs_set_u16(gs_.a4(), MM2_GS_PARTY_COUNT, static_cast<uint16_t>(launch_.party_count));
    return true;
}

void GameSession::applyExitFlagCleanup()
{
    /* 0x171AC / -$7D40: drop retained console layers (roster/status overwrite in ASM),
     * clear EXIT_FLAGS, host markDirty for chrome redraw. */
    bool redraw_status = false;
    bool redraw_roster = false;
    bool redraw_divider = false;
    events_.textView().applyScriptExitCleanup(&redraw_status, &redraw_roster, &redraw_divider);
    (void)redraw_status;
    (void)redraw_roster;
    (void)redraw_divider;
    mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 0);
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
        beginDismissHireling();
        break;
    case gameplay::PlaySessionAction::ExchangeOrder:
        beginExchangeOrder();
        break;
    case gameplay::PlaySessionAction::Automap:
        if (!assets_ok_ || !env_.automapReady()) {
            showStatusMessage("Automap tiles missing.");
        } else {
            overlay_ = PlayOverlay::Automap;
        }
        markDirty();
        break;
    case gameplay::PlaySessionAction::Search: {
        /* Key S @ 0x4800 → -$7D1C → 0x1B19C. Long path (sentinel==0) opens
         * Identify modal '1'..'4' @ 0x1B3F2; short path distributes immediately. */
        events::SearchPrepareOut prep{};
        const events::SearchPrepareResult r = events::eventVmSearchPrepare(
            gs_.a4(), &roster_, &launch_, &rng_, &prep);
        if (r == events::SearchPrepareResult::Nothing) {
            showStatusMessage(prep.msg[0] ? prep.msg : "Nothing Here!");
        } else if (r == events::SearchPrepareResult::NeedIdentify) {
            search_identify_rating_ = prep.rating;
            search_identify_pick_member_ = false;
            search_identify_find_traps_ = false;
            std::snprintf(search_identify_container_, sizeof(search_identify_container_), "%s",
                          prep.container_name);
            std::snprintf(status_message_, sizeof(status_message_), "%s", prep.msg);
            overlay_ = PlayOverlay::SearchIdentify;
            armSearchContainerArt();
        } else {
            showStatusMessage(prep.msg);
        }
        markDirty();
        break;
    }
    case gameplay::PlaySessionAction::Unlock:
        handleUnlockDoor();
        break;
    case gameplay::PlaySessionAction::Rest:
        /* Rest @ 0x19E20: set modal flag; 0x19E32 btst #3,-$55D6 → "Too dangerous!".
         * Current-cell latch -$55D6 is maintained by syncCurrentCellFlags. */
        mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 1); /* 0x19E24: -$7950 := 1 */
        gameplay::syncCurrentCellFlags(gs_, world_);
        if ((mm2_gs_u8(gs_.a4(), MM2_GS_TILE_RT_FLAGS) & 0x08) != 0) {
            mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 0);
            showStatusMessage("Too dangerous!"); /* 0x19EBC */
            markDirty();
            break;
        }
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

}  // namespace

void GameSession::applyDoorTrapDamage()
{
    /* Trap victim pick @ 0x1A9A6 + damage @ 0x1A90E (rng 1..100).
     * 0x20DA2: -$7950 := 3 then -$7D40 cleanup. */
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
    /* 0x1A902 → -$7F08 → 0x4952/0x4AAA: subtract from +$5E (working HP). */
    events::eventVmApplyOp31Damage(&rec, static_cast<uint16_t>(damage));
    mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 3);
    applyExitFlagCleanup();
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&rec, name, sizeof(name));
    char buf[96];
    std::snprintf(buf, sizeof(buf), "Trap! %s takes %d damage.", name, damage);
    showStatusMessage(buf);
    maybeBeginPartyWipeGameOver();
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
        gameplay::MoveResult move = gameplay::step(world_, gs_, true, &roster_, &launch_);
        refreshWorldAfterMove(move);
        return;
    }

    /* 0x9B88: door already unlocked (no wall bit) -> step through. */
    if (!forwardDoorBlocked(world_, x, y, facing)) {
        maybeTriggerStepEncounter();
        gameplay::MoveResult move = gameplay::step(world_, gs_, true, &roster_, &launch_);
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
    gameplay::MoveResult move = gameplay::step(world_, gs_, true, &roster_, &launch_);
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

    /* 0x20D0E → 0x1AE2E who-tries picker (condition & $F0 == 0). */
    if (launch_.party_count < 1) {
        return;
    }
    std::snprintf(status_message_, sizeof(status_message_), "Who will try (1 - %d)?",
                  launch_.party_count);
    overlay_ = PlayOverlay::UnlockWho;
    markDirty();
}

void GameSession::finishUnlockWithPartySlot(int party_slot)
{
    /* 0x20D26.. after 0x1AE2E returns a live party slot. */
    if (party_slot < 0 || party_slot >= launch_.party_count) {
        return;
    }
    const int idx = launch_.roster_slots[party_slot];
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
        return;
    }
    const int thievery = unlockThieveryFor(roster_.records[idx]);
    const int lock_d100 = rng_.range(1, 100);  /* 0x20D2E */
    const int trap_d100 = rng_.range(1, 100);  /* 0x20D64 */
    const int trap_byte = mm2_attrib_door_trap_byte(&world_.attrib());

    const gameplay::UnlockDecision d =
        gameplay::unlockDoorRoll(thievery, lock_d100, trap_byte, trap_d100);

    if (d.outcome == gameplay::UnlockOutcome::TrapSprung) {
        applyDoorTrapDamage();
        return;
    }

    const int x = gs_.coordX();
    const int y = gs_.coordY();
    const char facing = gs_.facingKey();
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
    /* 0x19B1E: -$7E6C → 0x6ACE — heroes only (index < $18); pool remainder on first.
     * Always called (even amount 0) so Rest consolidates hero gold like retail. */
    if (!events::eventVmPartyTryPayGold(a4, &roster_, &launch_, hireling_pay)) {
        /* 0x19EA0 inline string. */
        mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 0);
        showStatusMessage("Not enough gold - Dismiss hirelings");
        return;
    }

    /* --- 0x19D64 rest-interruption (encounter) check ---------------------- */
    /* ASM gates (all suppress the roll → return without ambush):
     *   -$55D6 (current-cell collision latch) >= $80  (0x19D7C)
     *   Guard Dog -$79A6 nonzero                     (0x19D88)
     *   saved move-counter -$796C nonzero            (0x19D90; cleared @ 0x19D76)
     * -$55D6 is the single current-cell byte (0x1B1C), not an array. */
    gameplay::syncCurrentCellFlags(gs_, world_);
    const uint8_t move_counter = mm2_gs_u8(a4, -0x796C);
    mm2_gs_set_u8(a4, -0x796C, 0); /* 0x19D76: clr -$796C */
    const uint8_t tile_rt = mm2_gs_u8(a4, MM2_GS_TILE_RT_FLAGS);
    const bool on_event_tile = tile_rt >= 0x80; /* 0x19D7C exact */
    const bool guard_dog = mm2_gs_u8(a4, MM2_GS_GUARD_DOG_FLAG) != 0;

    bool encounter = false;
    if (!on_event_tile && !guard_dog && move_counter == 0) {
        encounter = (rng_.range(1, 50) == 2); /* 0x19D98: rng(1,50)==2 */
    }
    if (encounter) {
        /* 0x19DAC–0x19DE6: for each living member (cond < $80), bset #4,$26
         * (wake from sleep). Then mode=3, clr -$77BE, zero 11 battle slots,
         * jsr -$7EDE. Skip heal + day advance (rest interrupted). */
        for (int i = 0; i < launch_.party_count; ++i) {
            const int idx = launch_.roster_slots[i];
            if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
                continue;
            }
            Mm2RosterRecord &rec = roster_.records[idx];
            if (rec.condition < 0x80) {
                rec.condition = static_cast<uint8_t>(rec.condition | 0x10);
            }
        }
        mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 0);
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 3);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 0); /* 0x19DEE: clr -$77BE */
        for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
            mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
        }
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
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

        /* 0x19C30 SP recompute — sync working +$20/+23 first (stock roster drift). */
        gameplay::syncRosterWorkingLevelFields(rec);
        gameplay::recomputeRestSpellPoints(rec);
    }

    /* 0x19CEC: advance the clock by 0x55 sub-day units (one rest = ~85 ticks),
     * rolling the calendar over so the day/night cycle progresses. */
    gameplay::advanceTimeTick(gs_, 0x55, &roster_, &launch_);

    /* 0x19CF6: if era != 9, rng(1,$3C); if roll < $A → era=9 and
     * -$7FDA(screen=0, x=$FF, y=$FF) @ 0x1B2A: Middlegate + entry_coord. */
    if (mm2_gs_u16(a4, MM2_GS_ERA) != 9) {
        const int roll = rng_.range(1, 0x3C);
        if (roll < 0x0A) {
            mm2_gs_set_u16(a4, MM2_GS_ERA, 9);
            gs_.setScreenId(0);
            world_.enterScreen(0);
            gameplay::materializeScreenAttrib(gs_, world_);
            /* 0x1C6A: $FF args → unpack -$560C (attrib 0x0E) → -$79F1/-$79F0. */
            const uint8_t packed = mm2_gs_u8(a4, MM2_GS_ENTRY_COORD);
            gs_.setCoordX(static_cast<uint8_t>(packed & 0x0F));
            gs_.setCoordY(static_cast<uint8_t>((packed >> 4) & 0x0F));
            mm2_gs_set_u8(a4, MM2_GS_PENDING_EVENT_LATCH, 1); /* 0x19D28 */
            mm2_gs_set_u8(a4, -0x79E4, 1); /* 0x19D2E */
            if (events_loaded_) {
                refreshEventsForScreen();
            }
        }
    }

    /* Light was cleared above — recompute can't-see from live tile (0x53C0). */
    gameplay::syncCurrentCellFlags(gs_, world_);
    gameplay::sessionInteractionGate(gs_);

    mm2_gs_set_u8(a4, MM2_GS_PENDING_EVENT_LATCH, 1); /* dispatcher epilogue $1420 */
    mm2_gs_set_u8(a4, MM2_GS_BUSY_STATUS, 1);
    mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, 0);

    showStatusMessage("Rest complete, no encounters."); /* 0x19D46 inline string */
    /* Age-illness @ 0x19B96 can wipe the party outside combat (0x9F22 path). */
    maybeBeginPartyWipeGameOver();
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

    if (overlay_ == PlayOverlay::SearchIdentify) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (search_identify_pick_member_) {
            if (keys.escape || ch == 27) {
                search_identify_pick_member_ = false;
                std::snprintf(status_message_, sizeof(status_message_),
                              "Search...\nThe Party Has found a:\nTreasure!\n%s\n"
                              "1) Open It\n2) Find Trap\n3) Detect Magic\n4) Leave it",
                              search_identify_container_[0] ? search_identify_container_
                                                            : "Treasure!");
                markDirty();
                return;
            }
            if (ch >= '1' && ch <= '8') {
                const int slot = ch - '1';
                const events::SearchOpenResult open = events::eventVmSearchOpenOrFind(
                    gs_.a4(), &roster_, &launch_, slot, search_identify_rating_,
                    search_identify_find_traps_, &rng_);
                search_identify_pick_member_ = false;
                if (open.aborted) {
                    markDirty();
                    return;
                }
                char buf[96];
                events::eventVmSearchDistribute(gs_.a4(), &roster_, &launch_, buf, sizeof(buf));
                if (open.trapped) {
                    std::snprintf(status_message_, sizeof(status_message_),
                                  "Trap! %u damage.\n%s",
                                  static_cast<unsigned>(open.trap_damage), buf);
                } else {
                    std::snprintf(status_message_, sizeof(status_message_), "%s", buf);
                }
                overlay_ = PlayOverlay::StatusMessage;
                clearSearchContainerArt();
                mm2_gs_set_u8(gs_.a4(), -0x79E4, 0);
                maybeBeginPartyWipeGameOver();
                markDirty();
            }
            return;
        }
        if (keys.escape || ch == '4') {
            events::eventVmSearchLeave(gs_.a4());
            overlay_ = PlayOverlay::None;
            status_message_[0] = '\0';
            clearSearchContainerArt();
            markDirty();
            return;
        }
        if (ch == '1' || ch == '2') {
            search_identify_find_traps_ = (ch == '2');
            search_identify_pick_member_ = true;
            std::snprintf(status_message_, sizeof(status_message_),
                          "Who Will Try (1 - %d) ", launch_.party_count);
            markDirty();
            return;
        }
        if (ch == '3') {
            char det[96];
            events::eventVmSearchDetectMagic(gs_.a4(), search_identify_rating_, det, sizeof(det));
            std::snprintf(status_message_, sizeof(status_message_),
                          "%s\n1) Open It\n2) Find Trap\n3) Detect Magic\n4) Leave it", det);
            markDirty();
            return;
        }
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

    if (overlay_ == PlayOverlay::DeathStrikes) {
        /* 0x14106: wait key #$D (ENTER) then Goto Town @ 0x1A1F8. */
        const char ch = static_cast<char>(keys.last_ascii);
        if (ch == '\r' || ch == '\n' || keys.enter) {
            overlay_ = PlayOverlay::None;
            status_message_[0] = '\0';
            events_.armInnGotoTown();
            maybeFinishInnRegistry();
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::FdPrintChrome) {
        /* 0x14A8E -$7DDC: scripted buffer first (incl. bit7 -$7FBC places), else real keys. */
        platform::KeyState fd_keys = keys;
        events::ScriptedKeyPlace place{};
        const int sk = events::eventVmScriptedKeyPoll(gs_.a4(), &place);
        if (place.active && events_loaded_) {
            if (place.clear) {
                events_.textView().clearServiceSignSprite();
            } else if (events_.textView().hasServicePortrait()) {
                events_.textView().applyScriptedSignPlace(place.placement, place.dst_x, place.dst_y);
            }
            markDirty();
        }
        if (sk == ' ' || sk == '\r' || sk == '\n') {
            fd_keys.space = true;
            fd_keys.any_key = true;
            fd_keys.enter = (sk == '\r' || sk == '\n');
        } else if (sk == 0x1B) {
            fd_keys.escape = true;
        } else if (sk > 0 && fd_print_stage_ == 3) {
            fd_keys.last_ascii = static_cast<char>(sk);
            fd_keys.any_key = true;
        }
        const char ch = static_cast<char>(fd_keys.last_ascii);

        if (fd_print_stage_ == 3) {
            /* 0x14FC0 -$7F92: 10-char WAFE preamble entry. */
            if (fd_keys.escape || ch == 0x1B) {
                overlay_ = PlayOverlay::None;
                status_message_[0] = '\0';
                fd_name_len_ = 0;
                fd_name_buf_[0] = '\0';
                fd_print_stage_ = 0;
                mm2_gs_set_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT, 3);
                events_.armDeathStrikes();
                maybeOpenDeathStrikes();
                markDirty();
                return;
            }
            if (fd_keys.backspace && fd_name_len_ > 0) {
                fd_name_buf_[--fd_name_len_] = '\0';
                markDirty();
                return;
            }
            if ((fd_keys.enter || ch == '\r' || ch == '\n') && fd_name_len_ > 0) {
                char padded[11];
                for (int i = 0; i < 10; ++i) {
                    padded[i] = (i < fd_name_len_) ? fd_name_buf_[i] : ' ';
                }
                padded[10] = '\0';
                fd_name_len_ = 0;
                fd_name_buf_[0] = '\0';
                if (op0eFdPasswordOk(padded)) {
                    fd_print_stage_ = 4;
                    loadFdPrintChromePage();
                } else {
                    /* 0x15474: print Incorrect!; RTS out of 0x14EA4. */
                    std::snprintf(status_message_, sizeof(status_message_), "Incorrect!");
                    fd_print_stage_ = 7; /* dismiss on SPACE */
                }
                markDirty();
                return;
            }
            if (ch >= 32 && ch < 127 && fd_name_len_ < 10) {
                fd_name_buf_[fd_name_len_++] = ch;
                fd_name_buf_[fd_name_len_] = '\0';
                markDirty();
            }
            return;
        }

        if (fd_keys.escape) {
            overlay_ = PlayOverlay::None;
            status_message_[0] = '\0';
            fd_print_stage_ = 0;
            markDirty();
            return;
        }
        if (fd_keys.space || fd_keys.any_key) {
            if (fd_print_stage_ == 0) {
                /* 0x14A8E key → 0x14A92 fight arm. */
                overlay_ = PlayOverlay::None;
                status_message_[0] = '\0';
                startOp0eFdEncounter();
                markDirty();
                return;
            }
            if (fd_print_stage_ == 2) {
                fd_print_stage_ = 3;
                fd_name_len_ = 0;
                fd_name_buf_[0] = '\0';
                std::snprintf(status_message_, sizeof(status_message_),
                              "Answer= Preamble     Code=");
                markDirty();
                return;
            }
            if (fd_print_stage_ == 4) {
                fd_print_stage_ = 5;
                loadFdPrintChromePage();
                if (status_message_[0] == '\0') {
                    fd_print_stage_ = 6;
                    loadFdPrintChromePage();
                }
                markDirty();
                return;
            }
            if (fd_print_stage_ == 5) {
                fd_print_stage_ = 6;
                loadFdPrintChromePage();
                markDirty();
                return;
            }
            if (fd_print_stage_ >= 6) {
                if (fd_print_stage_ == 7) {
                    /* Incorrect! dismiss — SCRIPT_ABORT stays 1. */
                    overlay_ = PlayOverlay::None;
                    status_message_[0] = '\0';
                    fd_print_stage_ = 0;
                    mm2_gs_set_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT, 1);
                    markDirty();
                    return;
                }
                overlay_ = PlayOverlay::None;
                status_message_[0] = '\0';
                fd_print_stage_ = 0;
                mm2_gs_set_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT, 2);
                mm2_gs_set_u8(gs_.a4(), MM2_GS_SCREEN_MODE_ID,
                              mm2_gs_u8(gs_.a4(), MM2_GS_SAVED_TOWN_ID));
                gs_.setScreenId(mm2_gs_u8(gs_.a4(), MM2_GS_SAVED_TOWN_ID));
                mm2_gs_set_u16(gs_.a4(), MM2_GS_OP0E_SUBMODE, 1);
                events_.armInnGotoTown();
                maybeFinishInnRegistry();
                markDirty();
                return;
            }
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::ExchangeOrder) {
        /* 0x20DE0: two -$7F98('1',max) reads; ESC aborts (0x20F84). */
        const char ch = static_cast<char>(keys.last_ascii);
        if (keys.escape || ch == 0x1B) {
            exchange_first_slot_ = -1;
            overlay_ = PlayOverlay::None;
            status_message_[0] = '\0';
            markDirty();
            return;
        }
        if (ch < '1' || ch > '9') {
            return;
        }
        const int slot = ch - '1';
        if (slot < 0 || slot >= launch_.party_count) {
            return;
        }
        if (exchange_first_slot_ < 0) {
            exchange_first_slot_ = slot;
            std::snprintf(status_message_, sizeof(status_message_), "with (1 - %d)",
                          launch_.party_count);
            markDirty();
            return;
        }
        exchangeExplorePartySlots(exchange_first_slot_, slot);
        exchange_first_slot_ = -1;
        overlay_ = PlayOverlay::None;
        status_message_[0] = '\0';
        markDirty();
        return;
    }

    if (overlay_ == PlayOverlay::DismissHireling) {
        /* 0x141F4: digit pick; only -$796A[slot] >= $18; else re-prompt. */
        const char ch = static_cast<char>(keys.last_ascii);
        if (keys.escape || ch == 0x1B) {
            mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 0);
            overlay_ = PlayOverlay::None;
            status_message_[0] = '\0';
            markDirty();
            return;
        }
        if (ch < '1' || ch > '9') {
            return;
        }
        const int slot = ch - '1';
        if (slot < 0 || slot >= launch_.party_count) {
            return; /* 0x14298 → re-prompt */
        }
        const int16_t ridx = launch_.roster_slots[slot];
        if (ridx < 0x18) {
            return; /* not a hireling — re-prompt */
        }
        if (!removeHirelingFromParty(ridx)) {
            return;
        }
        mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 3); /* 0x1428A */
        applyExitFlagCleanup(); /* 0x1429E → -$7D40 */
        showStatusMessage("Come back real soon."); /* $142A6 */
        return;
    }

    if (overlay_ == PlayOverlay::UnlockWho) {
        /* 0x1AE2E: '1'..'8'; slot < count; condition & $F0 == 0; ESC → $1B. */
        const char ch = static_cast<char>(keys.last_ascii);
        if (keys.escape || ch == 0x1B) {
            overlay_ = PlayOverlay::None;
            status_message_[0] = '\0';
            markDirty();
            return;
        }
        if (ch < '1' || ch > '8') {
            return;
        }
        const int slot = ch - '1';
        if (slot < 0 || slot >= launch_.party_count) {
            return; /* re-prompt */
        }
        const int idx = launch_.roster_slots[slot];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            return;
        }
        if ((roster_.records[idx].condition & 0xF0) != 0) {
            return; /* 0x1AE9E — not eligible; re-prompt */
        }
        overlay_ = PlayOverlay::None;
        status_message_[0] = '\0';
        finishUnlockWithPartySlot(slot);
        return;
    }

    if (overlay_ == PlayOverlay::GameOver) {
        /* Dismiss → title. Reload roster.dat so the menu sees the last good save
         * (total wipe deliberately skipped the post-combat write). */
        if (keys.escape || keys.any_key) {
            if (data_dir_) {
                char *const path = mm2_path_scratch_a();
                if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "roster.dat")) {
                    (void)mm2_roster_load_file(path, &roster_);
                }
            }
            status_message_[0] = '\0';
            overlay_ = PlayOverlay::None;
            back_to_title_ = true;
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

    if (overlay_ == PlayOverlay::None && !combat_.active() && combat_.sheetCastPending()) {
        if (keys.escape) {
            combat_.tickSheetCastAux(gs_, 0x1B);
            markDirty();
            return;
        }
        const char ch = static_cast<char>(keys.last_ascii);
        if (ch != 0) {
            combat_.tickSheetCastAux(gs_, ch);
            markDirty();
        }
        return;
    }

    if (overlay_ == PlayOverlay::CharacterSheet) {
        if (combat_.sheetCastPending()) {
            if (keys.escape) {
                combat_.tickSheetCastAux(gs_, 0x1B);
                markDirty();
                return;
            }
            const char ch = static_cast<char>(keys.last_ascii);
            if (ch != 0) {
                combat_.tickSheetCastAux(gs_, ch);
                if (combat_.statusLine()[0]) {
                    std::snprintf(sheet_session_.status_line, sizeof(sheet_session_.status_line), "%s",
                                  combat_.statusLine());
                }
                markDirty();
            }
            return;
        }
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
        const int cast_before = sheet_session_.cast_spell_flat;
        const int use_before = sheet_session_.pending_use_slot;
        const gameplay::SheetKeyOutcome outcome =
            ingame_sheet_.handleKey(ch, sheet_session_, roster_, launch_, items_ptr, combat_character_sheet_);
        if (sheet_session_.cast_spell_flat >= 0 && sheet_session_.cast_spell_flat != cast_before) {
            combat_.castSpellFromSheet(gs_, sheet_session_.party_slot, sheet_session_.cast_spell_flat);
            sheet_session_.cast_spell_flat = -1;
            if (combat_.statusLine()[0]) {
                std::snprintf(sheet_session_.status_line, sizeof(sheet_session_.status_line), "%s",
                              combat_.statusLine());
            }
        }
        if (sheet_session_.pending_use_slot >= 0 && sheet_session_.pending_use_slot != use_before) {
            const int u = sheet_session_.pending_use_slot;
            sheet_session_.pending_use_slot = -1;
            const bool backpack = u >= 6;
            const int slot = backpack ? u - 6 : u;
            combat_.applyItemUse(gs_, sheet_session_.party_slot, backpack, slot);
            if (combat_.statusLine()[0]) {
                std::snprintf(sheet_session_.status_line, sizeof(sheet_session_.status_line), "%s",
                              combat_.statusLine());
            }
        }
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
    /* Combat waits on edge-triggered ACE keys; include so AppHost can re-poll
     * after a slow redraw the same way as SPACE continue prompts. */
    if (combat_.active() && combat_.awaitingInput()) {
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
        /* Do not accumulate a catch-up debt while the viewport is covered. */
        overlay_anim_clock_valid_ = false;
        return;
    }

    /* Pace .anm delays off the VBlank clock, not main-loop iterations. Multi-sprite
     * combat frames often take several vblanks; without catch-up the sprites crawl. */
    const uint32_t now = platform::nowTicks();
    int steps = 1;
    if (overlay_anim_clock_valid_) {
        steps = static_cast<int>(static_cast<int16_t>(static_cast<uint16_t>(now) -
                                                      static_cast<uint16_t>(overlay_anim_clock_)));
        if (steps <= 0) {
            return;
        }
        /* Cap so a long hitch does not skip an entire clip in one frame. */
        if (steps > 8) {
            steps = 8;
        }
    }
    overlay_anim_clock_ = now;
    overlay_anim_clock_valid_ = true;

    bool changed = false;
    for (int step = 0; step < steps; ++step) {
        if (combat_.active()) {
            /* ASM player/ack waits (0x119C2 / 0x6798) only poll + Delay(1) —
             * they do not re-place or tick the combat BOB. Advancing .anm here
             * forced a full-screen copy + masked blit every cel (Amiga hitch). */
            if (!combat_.awaitingInput()) {
                if (step == 0) {
                    refreshCombatSprites();
                }
                for (int i = 0; i < combat_sprite_count_; ++i) {
                    changed |= combat_sprites_[i].tick();
                }
            }
        }
        if (scripted_loaded_ && scripted_scene_.active()) {
            changed |= scripted_scene_.tickAnimation();
        }
        if (events_loaded_) {
            if (events_.textView().hasServicePortrait() || events_.textView().hasPegasusIllustration()) {
                changed |= events_.textView().tickAnimation();
            }
        }
        if (overlay_ == PlayOverlay::SearchIdentify && search_container_.loaded()) {
            changed |= search_container_.tick();
        }
    }
    if (changed) {
#if MM2_HOST_AMIGA
        markOverlayAnimDirty();
#else
        markDirty();
#endif
    }
}

void GameSession::refreshHoodTorchCache()
{
    if (hood_torch_cache_valid_) {
        return;
    }
    hood_has_torches_ = false;
    if (assets_ok_ && !world_.isOutdoor() && gs_.valid()) {
        gfx::View3DCamera camera{};
        camera.x = gs_.coordX();
        camera.y = gs_.coordY();
        camera.facing = gs_.facing03();
        const gfx::View3DScene scene = gfx::buildView3DScene(world_.buildView3DMapBuffers(), camera);
        hood_has_torches_ = scene.num_torch_blits > 0;
    }
    hood_torch_cache_valid_ = true;
}

void GameSession::tickTorchAnimation()
{
    if (!assets_ok_ || world_.isOutdoor() || viewportHiddenByOverlay()) {
        return;
    }
    if (overlay_ == PlayOverlay::Automap || combat_.active()) {
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

    refreshHoodTorchCache();
    if (!hood_has_torches_) {
        return;
    }

#if MM2_HOST_AMIGA
    /* Torches live on top of the cached hood — do not invalidate the 3D rebuild
     * or force a full-frame text replot while a SPACE prompt is up. */
    markOverlayAnimDirty();
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
            /* Sheet is a full overlay; only redraw when input changes it. */
            const gameplay::SheetSubMode sub_before = sheet_session_.sub_mode;
            const int slot_before = sheet_session_.party_slot;
            tickCombatCharacterSheetInput(keys);
            if (overlay_ != PlayOverlay::CharacterSheet) {
                markDirty(); /* closed — restore combat playfield */
                return;
            }
            if (keys.last_ascii != 0 || keys.escape || sheet_session_.sub_mode != sub_before ||
                sheet_session_.party_slot != slot_before) {
                markDirty();
            }
            return;
        }
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        /* 0x11B6E: V or Q → -$7E00 view active slot (digit = '1'+-$4F5); turn not spent. */
        if ((ch == 'V' || ch == 'Q') && combat_.awaitingCommand()) {
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
        /* Combat 'C' is handled inside CombatSession (0x11A90 → 0x79EE level/number
         * on the message band). Spell grid is only via sheet 'V' → SpellBook. */
        /* Idle combat waits draw nothing (ASM 0x119C2 / 0x6798). State/message
         * changes only patch roster + party strip + message band (0x129CC /
         * 0x12848 / 0x1119E) — never a full play-screen rebuild. */
        const combat::CombatState state_before = combat_.state();
        const bool round_before = combat_.roundLayoutActive();
        const int leader_disk_before = combat_.leaderSpriteDiskIndex();
        char status_before[160];
        std::snprintf(status_before, sizeof(status_before), "%s", combat_.statusLine());
        const bool ended = combat_.tick(gs_, world_, keys);
        if (ended) {
            markDirty();
            finishCombat();
            return;
        }
        /* Advance/compact can replace slot A's type — hood BOB must reload (0x316E),
         * not stick to the previous front-rank .anm via HUD-only copy_front_to_back. */
        const int leader_disk_after = combat_.leaderSpriteDiskIndex();
        if (round_before != combat_.roundLayoutActive() ||
            leader_disk_before != leader_disk_after) {
            /* 0x12A22 → 0x135BE / post-advance front change: rebuild hood + sprites. */
            markDirty();
        } else if (combat_.state() != state_before ||
                   std::strcmp(status_before, combat_.statusLine()) != 0) {
#if MM2_HOST_AMIGA
            markCombatHudDirty();
#else
            markDirty();
#endif
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
    maybeOpenDeathStrikes();
    maybeOpenFdPrintChrome();

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
                    move = gameplay::step(world_, gs_, true, &roster_, &launch_);
                    break;
                case gameplay::ExploreCode::Back:
                    maybeTriggerStepEncounter();
                    move = gameplay::step(world_, gs_, false, &roster_, &launch_);
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

    /* Main-loop auto-Search @ 0x1276: when -$794C == $FE (temple five-donate
     * Farthing arm, etc.), bump sentinel to $FF and run Search @ 0x4800→0x1B19C.
     * Combat victory CLEARS the sentinel (0x1243e) — never invent post-fight $FE. */
    if (mm2_gs_u8(gs_.a4(), MM2_GS_FOUND_SENTINEL) == MM2_GS_FOUND_SENTINEL_PENDING) {
        mm2_gs_set_u8(gs_.a4(), MM2_GS_FOUND_SENTINEL, MM2_GS_FOUND_SENTINEL_FILLED);
        events::SearchPrepareOut prep{};
        const events::SearchPrepareResult r = events::eventVmSearchPrepare(
            gs_.a4(), &roster_, &launch_, &rng_, &prep);
        if (r == events::SearchPrepareResult::NeedIdentify) {
            search_identify_rating_ = prep.rating;
            search_identify_pick_member_ = false;
            search_identify_find_traps_ = false;
            std::snprintf(search_identify_container_, sizeof(search_identify_container_), "%s",
                          prep.container_name);
            std::snprintf(status_message_, sizeof(status_message_), "%s", prep.msg);
            overlay_ = PlayOverlay::SearchIdentify;
            armSearchContainerArt();
        } else if (prep.msg[0]) {
            showStatusMessage(prep.msg);
        }
        markDirty();
    }

    runPendingEvents();
    if (combat_.active()) {
        return;
    }
    refreshWorldAfterEventTransition();
    /* session_interaction_gate @ 0x53A0 — maintain -$79E1 from live tile/light. */
    if (world_.loaded()) {
        gameplay::syncCurrentCellFlags(gs_, world_);
        gameplay::sessionInteractionGate(gs_);
    }
    maybeOpenDeathStrikes();
    maybeOpenFdPrintChrome();
    maybeOpenTownServiceMenu();
}

void GameSession::maybeFinishInnRegistry()
{
    if (!events_.takePendingInnGotoTown()) {
        return;
    }
    /* ASM @ 0x1A1CE: roster+$0B <- A4-$79F2+1 (town map index 0..4, not event.dat id).
     * 0x1A1E2: -$79AC := -$79F2 before Goto Town. */
    const int map_id = static_cast<int>(gs_.screenId());
    mm2_gs_set_u8(gs_.a4(), MM2_GS_SAVED_TOWN_ID, static_cast<uint8_t>(gs_.screenId()));
    events::eventInnApplyRegistry(&roster_, &launch_, map_id);
    if (map_id >= 0 && map_id < 5) {
        goto_town_filter_ = static_cast<uint8_t>(map_id + 1);
    }
    saveRosterWithGlobalTail();
    /* 0x1A1F8 Goto Town epilogue GS (UI chrome omitted): */
    mm2_gs_set_u8(gs_.a4(), MM2_GS_SCREEN_MODE_PREV, 0xFF);
    mm2_gs_set_u8(gs_.a4(), MM2_GS_SCREEN_MODE_ID, 0xFF);
    mm2_gs_set_u8(gs_.a4(), MM2_GS_SIGN_ENV_ID, 7);
    mm2_gs_set_u8(gs_.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    mm2_gs_set_u8(gs_.a4(), -0x79E4, 0);
    mm2_gs_set_u8(gs_.a4(), MM2_GS_EXIT_FLAGS, 0);
    back_to_goto_town_ = true;
}

void GameSession::maybeOpenDeathStrikes()
{
    if (!events_.takePendingDeathStrikes() || overlay_ != PlayOverlay::None) {
        return;
    }
    events::eventVmDeathStrikesLines(status_message_, sizeof(status_message_));
    overlay_ = PlayOverlay::DeathStrikes;
    markDirty();
}

void GameSession::loadFdPrintChromePage()
{
    /* 0x1493C / 0x14EA4 print consumers. */
    status_message_[0] = '\0';
    char a[200];
    char b[200];
    a[0] = b[0] = '\0';
    switch (fd_print_stage_) {
    case 0:
        events::eventVmFormatOp0eFdPtrTable(gs_.a4(), MM2_GS_OP0E_FD_PTR0, 4, status_message_,
                                           sizeof(status_message_));
        break;
    case 2: {
        events::eventVmFormatOp0eFdPtrTable(gs_.a4(), MM2_GS_OP0E_FD_PTR1, 4, a, sizeof(a));
        std::snprintf(status_message_, sizeof(status_message_),
                      "A vaccuum tube sucks the party into\n"
                      "        the control room...\n%s",
                      a[0] ? a : "");
        break;
    }
    case 4:
        std::snprintf(status_message_, sizeof(status_message_),
                      "         Thank you.\n"
                      "Recalculating trajectory now...\n"
                      "Error, Error!  Computer malfunction.\n"
                      "    Internal program override...");
        break;
    case 5:
        events::eventVmFormatOp0eFdPtrTable(gs_.a4(), MM2_GS_OP0E_FD_PTR2, 14, a, sizeof(a));
        events::eventVmFormatOp0eFdPtrTable(gs_.a4(), MM2_GS_OP0E_FD_PTR3, 4, b, sizeof(b));
        if (a[0] && b[0]) {
            std::snprintf(status_message_, sizeof(status_message_), "%s\n%s", a, b);
        } else if (a[0]) {
            std::snprintf(status_message_, sizeof(status_message_), "%s", a);
        } else {
            std::snprintf(status_message_, sizeof(status_message_), "%s", b);
        }
        break;
    case 6:
        events::eventVmFormatOp0eFdPtrTable(gs_.a4(), MM2_GS_OP0E_FD_PTR4, 11, a, sizeof(a));
        events::eventVmFormatOp0eFdPtrTable(gs_.a4(), MM2_GS_OP0E_FD_PTR5, 10, b, sizeof(b));
        if (a[0] && b[0]) {
            std::snprintf(status_message_, sizeof(status_message_), "%s\n%s", a, b);
        } else if (a[0]) {
            std::snprintf(status_message_, sizeof(status_message_), "%s", a);
        } else {
            std::snprintf(status_message_, sizeof(status_message_), "%s", b);
        }
        break;
    default:
        break;
    }
}

void GameSession::startOp0eFdEncounter()
{
    fd_await_combat_ = true;
    fd_print_stage_ = 1;
    events::eventRunOp0eFdEncounter(gs_, has_monsters_ ? &combat_ : nullptr, &world_);
    if (!combat_.active()) {
        /* Fight refused (no monsters.dat) — skip to defeat path. */
        fd_await_combat_ = false;
        fd_print_stage_ = 0;
        mm2_gs_set_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT, 1);
        showStatusMessage("Encounter! (monsters.dat missing — fight skipped)");
    }
}

void GameSession::resumeOp0eFdAfterCombat()
{
    fd_await_combat_ = false;
    if (mm2_gs_u8(gs_.a4(), MM2_GS_COMBAT_VICTORY_LATCH) != 0) {
        /* 0x14AD8 victory → 0x14EA4 control-room + endgame.32 + PTR1 + WAFE. */
        ensureEndgameArtLoaded();
        fd_print_stage_ = 2;
        loadFdPrintChromePage();
        overlay_ = PlayOverlay::FdPrintChrome;
    } else {
        /* 0x14AF6: SCRIPT_ABORT=1 */
        fd_print_stage_ = 0;
        mm2_gs_set_u8(gs_.a4(), MM2_GS_SCRIPT_ABORT, 1);
    }
}

bool GameSession::op0eFdPasswordOk(const char *typed) const
{
    /* 0x15060: strcmp vs "WAFE      ".
     * 0x15034 also requires a party member with +$81 bit5, then 0x15074 re-checks
     * the count. CODE never bset bit5 (only bit3 @ 0x1528C after success); stock
     * roster.dat +$81 is all-zero — enforcing bit5 makes the hosted endgame
     * unwinnable. Remake: password match is the completable gate. */
    static const char kWafe[11] = "WAFE      ";
    if (!typed) {
        return false;
    }
    for (int i = 0; i < 10; ++i) {
        if (typed[i] != kWafe[i]) {
            return false;
        }
    }
    return true;
}

void GameSession::ensureEndgameArtLoaded()
{
    /* 0x14EC2: -$7C2C(A4-$77D2) loads endgame.32; blit -$7C20 frame0 @ (x=$20,y=$40). */
    if (has_endgame_ || !data_dir_) {
        return;
    }
    char *path = mm2_path_scratch_a();
    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "endgame.32")) {
        return;
    }
    if (mm2_image32_load_file(path, &endgame_) == MM2_IMAGE32_OK && endgame_.frame_count > 0) {
        has_endgame_ = true;
    }
}

void GameSession::armSearchContainerArt()
{
    /* 0x1B2CE: jsr -$7FC2(id) — same sign_sprite_load path as OP_0B. */
    clearSearchContainerArt();
    if (!data_dir_ || !gs_.valid()) {
        return;
    }
    const int id = events::eventVmSearchContainerAnmId(gs_.a4());
    /* id $47 → 71.anm: absent from retail ADF + repo; loadFromTableId no-ops. */
    (void)search_container_.loadFromTableId(data_dir_, id, gfx::AnmLoopMode::Loop, false);
}

void GameSession::clearSearchContainerArt()
{
    if (search_container_.loaded()) {
        search_container_.unload();
    }
}

void GameSession::maybeOpenFdPrintChrome()
{
    if (overlay_ != PlayOverlay::None) {
        return;
    }
    if (!events_.takePendingOp0eFdPrintChrome()) {
        return;
    }
    fd_print_stage_ = 0;
    fd_await_combat_ = false;
    fd_name_len_ = 0;
    fd_name_buf_[0] = '\0';
    events::eventVmScriptedKeyReset(gs_.a4());
    loadFdPrintChromePage();
    if (status_message_[0] == '\0') {
        /* Empty PTR0 — still arm fight after a blank SPACE page. */
        std::snprintf(status_message_, sizeof(status_message_), "(continue)");
    }
    overlay_ = PlayOverlay::FdPrintChrome;
    markDirty();
}

void GameSession::maybeOpenTownServiceMenu()
{
    if (!town_service_ui_.pending() || overlay_ != PlayOverlay::None) {
        return;
    }
    /* ASM -$7ED8(2): clear (1,17)-(38,22) before shop menu; drop the intro
     * OP_02 layer so it does not paint over menu rows 0x11..0x16. */
    events_.textView().clearConsoleMessageLayers();
    town_service_ui_.begin();
    overlay_ = PlayOverlay::TownService;
    markDirty();
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

void GameSession::renderIndoorView3DBase()
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
}

void GameSession::blitIndoorTorches()
{
    using namespace gfx;

    if (!assets_ok_ || world_.isOutdoor() || env_.torches().img.frame_count <= 0) {
        return;
    }

    View3DCamera camera{};
    camera.x = gs_.coordX();
    camera.y = gs_.coordY();
    camera.facing = gs_.facing03();

    const View3DMapBuffers bufs = world_.buildView3DMapBuffers();
    const View3DScene scene = buildView3DScene(bufs, camera);
    for (int i = 0; i < scene.num_torch_blits; ++i) {
        const View3DBlit &b = scene.torch_blits[static_cast<size_t>(i)];
        View3DTorchBlit tb{};
        if (view3dTorchBlitFor(b, torch_phase_, &tb)) {
            blitImageFrame(compositor_, env_.torches(), tb.frame, tb.x, tb.y, 0);
        }
    }
}

void GameSession::renderIndoorView3D()
{
    renderIndoorView3DBase();
    blitIndoorTorches();
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

        blitImageFrame(compositor_, env_.floor(), 0, kView3DOriginX, kCombatView3DFloorY, 1);

        const uint16_t subday = mm2_gs_u16(gs_.a4(), MM2_GS_TIME_SUBDAY);
        if (subday < static_cast<uint16_t>(kOutdoorNightSubdayThreshold)) {
            blitImageFrame(compositor_, env_.sky(), 0, kView3DOriginX, kCombatView3DSkyY, 1);
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
            compositor_.fillRect(kOutdoorSkyFillX, kCombatView3DSkyY, kCombatView3DViewportW,
                                 kCombatView3DViewportH / 2, fr, fg, fb);

            std::array<OutdoorStarBlit, kOutdoorStarCount> stars{};
            const int nstars = buildOutdoorStars(camera.facing, stars);
            for (int i = 0; i < nstars; ++i) {
                const auto &s = stars[static_cast<size_t>(i)];
                if (s.x >= kView3DOriginX + kCombatView3DViewportW || s.y >= kCombatView3DSkyY + kCombatView3DViewportH / 2) {
                    continue;
                }
                uint8_t r = 0, g = 0, b = 0;
                penRgb(s.pen, r, g, b);
                compositor_.fillRect(s.x, s.y, 1, 1, r, g, b);
            }
        }

        const OutdoorScene scene = buildOutdoorScene(world_, camera);
        for (int i = 0; i < scene.num_decor; ++i) {
            const OutdoorSpriteBlit &b = scene.decor[static_cast<size_t>(i)];
            if (b.x >= kView3DOriginX + kCombatView3DViewportW) {
                continue;
            }
            blitImageFrame(compositor_, env_.biomeSheet(b.biome), b.frame, b.x, b.y, 0);
        }
        for (int i = 0; i < scene.num_horizon; ++i) {
            const OutdoorSpriteBlit &b = scene.horizon[static_cast<size_t>(i)];
            if (b.x >= kView3DOriginX + kCombatView3DViewportW) {
                continue;
            }
            blitImageFrame(compositor_, env_.horizonSheet(b.horizon), b.frame, b.x, b.y, 0);
        }
        maskCombatBackdropBleed(compositor_);
        return;
    }

    /* Retail encounter entry (0x51C2) ends in session_interaction_gate (0x53A0)
     * → view_3d_master (0x2ECE): full indoor hood (floor/sky/walls/torches).
     * Combat chrome then covers the right band; mask the wall bleed past the
     * narrow combat viewport (cols 1..0x0E). */
    renderIndoorView3D();
    maskCombatBackdropBleed(compositor_);
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
        /* 0x6150 win_print(name): stop at first NUL inside the 11-byte field —
         * do not strip trailing spaces (mm2_roster_name_to_cstr does). */
        {
            int n = 0;
            while (n < MM2_ROSTER_NAME_SIZE && rec.name[n] != '\0' &&
                   n + 1 < static_cast<int>(sizeof(slots[i].name))) {
                slots[i].name[n] = rec.name[n];
                ++n;
            }
            slots[i].name[n] = '\0';
        }
        /* draw_party_status_panel @ 0x624C: roster word +$5E (hp_max), not +$74. */
        slots[i].hp = rec.hp_max;
        slots[i].bad_condition = rec.condition != 0;
        if (combat_.active()) {
            slots[i].in_combat = true;
            /* Check glyph = front-rank cutoff A4-$5E4D (0x12892), not acted. */
            slots[i].combat_front_rank = combat_.partySlotInFrontRank(i);
        }
    }

    gfx::drawPlayPartyPanel(compositor_, slots);
}

void GameSession::unloadCombatSprites()
{
    for (int i = 0; i < gfx::kAgaCombatSpriteCap; ++i) {
        combat_sprites_[i].unload();
        combat_sprite_disks_[i] = -1;
        combat_sprite_stacks_[i] = 0;
    }
    combat_sprite_count_ = 0;
}

void GameSession::refreshCombatSprites()
{
    if (!combat_.active() || !data_dir_ || !has_monsters_) {
        return;
    }
    const gfx::CombatPanelView view = combat_.panelView();

    /* Retail centers a single leader sprite (first alive battle slot PIC) —
     * 0x316E / 41-aga-port-plan §8.2. Prefer battle order over distinct-disk
     * gallery order so slot A always owns the hood after advances/swaps. */
    int leader_disk = combat_.leaderSpriteDiskIndex();
    if (leader_disk < 0) {
        leader_disk =
            view.sprite_slot_count > 0 ? view.sprite_slots[0].disk_index : view.sprite_disk_index;
    }
    if (leader_disk < 0) {
        unloadCombatSprites();
        return;
    }

    /* Drop any extra BOBs left over from a prior multi-sprite frame. */
    for (int i = 1; i < gfx::kAgaCombatSpriteCap; ++i) {
        combat_sprites_[i].unload();
        combat_sprite_disks_[i] = -1;
        combat_sprite_stacks_[i] = 0;
    }

    const int leader_stack = view.sprite_slot_count > 0 ? view.sprite_slots[0].stack_count : 1;

    /* Reuse the warm pool ref when the leader picture is unchanged. */
    if (combat_sprite_count_ == 1 && combat_sprite_disks_[0] == leader_disk &&
        combat_sprites_[0].loaded()) {
        combat_sprite_stacks_[0] = leader_stack;
        return;
    }

    if (!combat_sprites_[0].loadFromDiskIndex(data_dir_, leader_disk, gfx::AnmLoopMode::Loop, false)) {
        unloadCombatSprites();
        return;
    }
    combat_sprite_disks_[0] = leader_disk;
    combat_sprite_stacks_[0] = leader_stack;
    combat_sprite_count_ = 1;
}

void GameSession::blitCombatSprites()
{
    if (combat_sprite_count_ <= 0) {
        return;
    }

    const gfx::CombatPanelView view = combat_.panelView();
    const bool round_layout = view.label_monster_slots;
    const int slot_x = round_layout ? gfx::kView3DOriginX : gfx::play_layout::kViewOriginX;
    const int slot_y = round_layout ? gfx::play_layout::kCombatView3DSkyY : gfx::play_layout::kViewOriginY;
    const int slot_w =
        round_layout ? gfx::play_layout::kCombatView3DViewportW : gfx::play_layout::kViewW;
    const int slot_h =
        round_layout ? gfx::play_layout::kCombatView3DViewportH : gfx::play_layout::kViewH;

    for (int i = 0; i < combat_sprite_count_; ++i) {
        if (!combat_sprites_[i].loaded()) {
            continue;
        }
        /* Always clamp into the active hood — oversized .anm cels must not paint
         * the roster band (cols 0x10+). Multi-BOB uses layout offsets as bias. */
        if (combat_sprite_count_ == 1) {
            combat_sprites_[i].blitCenteredInViewport(compositor_, 0, slot_x, slot_y, slot_w, slot_h,
                                                      /*apply_content_offset=*/false);
        } else {
            const int layout_i = i % gfx::play_layout::kAgaCombatSpriteLayoutCount;
            const gfx::play_layout::AgaCombatSpriteLayout &lay =
                gfx::play_layout::kAgaCombatSpriteLayout[layout_i];
            combat_sprites_[i].blitCenteredInViewport(compositor_, 0, slot_x + lay.x, slot_y + lay.y,
                                                      slot_w - lay.x, slot_h - lay.y,
                                                      /*apply_content_offset=*/false);
        }
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
        /* Status row 0x11 only — same clear as EventTextView Op01 (doc 44). No host ESC line. */
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, status_message_, 255, 255, 255, 255);
        break;
    case PlayOverlay::ExchangeOrder:
        /* 0x20DE0 strings A4-$6482 / -$647E — host status-line digits. */
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, status_message_, 255, 255, 255, 255);
        break;
    case PlayOverlay::DismissHireling:
    case PlayOverlay::UnlockWho:
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, status_message_, 255, 255, 255, 255);
        break;
    case PlayOverlay::GameOver: {
        /* Funeral band after combat_defeat_retreat @ 0x11646 / tones @ 0x7DCC. */
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 4);
        int row = 0x11;
        const char *p = status_message_;
        while (*p && row <= 0x14) {
            char line[40];
            int n = 0;
            while (*p && *p != '\n' && n < 38) {
                line[n++] = *p++;
            }
            line[n] = '\0';
            if (*p == '\n') {
                ++p;
            }
            compositor_.drawText(8, row * 8, line, 255, 80, 80, 255);
            ++row;
        }
        compositor_.drawText(8, row * 8, "(any key — return to title)", 180, 180, 180, 255);
        break;
    }
    case PlayOverlay::DeathStrikes: {
        /* 0x14106: win_define(7,6,$21,$11) → cells (7,6)-(33,17) = 27×12;
         * drawConsoleBox(row,col,w,h) = (6,7,0x1B,0x0C). Print at win-relative (2,1+i). */
        compositor_.drawConsoleBox(6, 7, 0x1B, 0x0C, 255, 0, 0);
        gfx::fillCellRect(compositor_, 8, 7, 0x19, 0x0A);
        int row = 7;
        const char *p = status_message_;
        while (*p && row <= 0x10) {
            char line[40];
            int n = 0;
            while (*p && *p != '\n' && n < 38) {
                line[n++] = *p++;
            }
            line[n] = '\0';
            if (*p == '\n') {
                ++p;
            }
            /* Window-relative (2, 1+i) → absolute col 9, row 7+i. */
            compositor_.drawText(9 * 8, row * 8, line, 255, 255, 255, 255);
            ++row;
        }
        break;
    }
    case PlayOverlay::FdPrintChrome: {
        /* 0x14EA4: endgame.32 @ (32,64) then message band; stage 3 = WAFE entry. */
        if (has_endgame_ && fd_print_stage_ >= 2 && fd_print_stage_ != 7) {
#if MM2_HOST_AMIGA
            platform::blitImage32(&endgame_, 0, 0x20, 0x40, 0);
#else
            if (endgame_.frames && endgame_.frame_count > 0 && endgame_.frames[0].rgba) {
                compositor_.blitRgba(endgame_.frames[0].rgba, endgame_.frames[0].width,
                                     endgame_.frames[0].height, 0x20, 0x40);
            }
#endif
        }
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 8);
        int row = 0x11;
        const char *p = status_message_;
        while (*p && row <= 0x17) {
            char line[40];
            int n = 0;
            while (*p && *p != '\n' && n < 38) {
                line[n++] = *p++;
            }
            line[n] = '\0';
            if (*p == '\n') {
                ++p;
            }
            compositor_.drawText(8, row * 8, line, 255, 255, 128, 255);
            ++row;
        }
        if (fd_print_stage_ == 3) {
            compositor_.drawText(8, row * 8, fd_name_buf_, 255, 255, 255, 255);
            compositor_.drawGlyph((2 + fd_name_len_) * 8, row * 8, 0x0C, 255, 255, 255);
        } else if (fd_print_stage_ != 7) {
            compositor_.drawText(8, 0x18 * 8, "('Space' to continue)", 180, 180, 180, 255);
        } else {
            compositor_.drawText(8, 0x18 * 8, "('Space' to dismiss)", 180, 180, 180, 255);
        }
        break;
    }
    case PlayOverlay::SearchIdentify: {
        /* Identify menu @ 0x1B3F2 — container -$7FC2 over viewport + console band. */
        if (search_container_.loaded()) {
            search_container_.blitCentered(compositor_, 0);
        }
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 8);
        int row = 0x11;
        const char *p = status_message_;
        while (*p && row <= 0x18) {
            char line[40];
            int n = 0;
            while (*p && *p != '\n' && n < 38) {
                line[n++] = *p++;
            }
            line[n] = '\0';
            if (*p == '\n') {
                ++p;
            }
            compositor_.drawText(8, row * 8, line, 255, 255, 128, 255);
            ++row;
        }
        break;
    }
    case PlayOverlay::Automap:
        gfx::renderAutomap(compositor_, env_, world_, automap_, gs_);
        break;
    case PlayOverlay::QuitConfirm: {
        /* -$7EC0 status line — Y/N only, no spinner/cursor (doc 44 §3.7). */
        static const char kQuitPrompt[] = "Quit without game save (y/n)?";
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, kQuitPrompt, 255, 255, 255, 255);
        break;
    }
    case PlayOverlay::RestConfirm: {
        /* 0x19ECB inline string — Y/N only, no spinner/cursor. */
        static const char kRestPrompt[] = "Rest here? (Y/N)";
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, kRestPrompt, 255, 255, 255, 255);
        break;
    }
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
    const gfx::CombatPanelView combat_view = combat_active ? combat_.panelView() : gfx::CombatPanelView{};
    /* Pre-combat (encounter entry @ 0x12C6E) keeps the wide exploration hood +
     * name box; the narrow hood/roster layout starts with the round loop
     * (combat_display_refresh @ 0x135BE). */
    const bool combat_round_layout = combat_active && combat_view.label_monster_slots;

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
        /* 0x135BE clear only when rebuilding combat chrome; a wipe+restore every
         * full combat dirty was racing the roster and costing a huge fill. */
        if (combat_round_layout &&
            (!combat_backdrop_cached_ || combat_backdrop_round_layout_ != combat_round_layout)) {
            gfx::drawCombatScreenChrome(compositor_);
        }
    }
#else
    (void)overlay_anim_only;
    compositor_.clear(0, 0, 0, 255);
    gfx::drawPlayScreenChrome(compositor_);
#endif

    if (combat_active) {
#if MM2_HOST_AMIGA
        if (!combat_backdrop_cached_ || combat_backdrop_round_layout_ != combat_round_layout) {
            mm2_amiga_restore_play_world_palette();
            if (combat_round_layout) {
                /* Narrow hood only — a 208-wide restore would wipe the roster. */
                mm2_amiga_viewport_cache_set_size(
                    static_cast<UWORD>(gfx::play_layout::kCombatView3DViewportW),
                    static_cast<UWORD>(gfx::play_layout::kCombatView3DViewportH));
                renderCombatBackdrop();
            } else {
                mm2_amiga_viewport_cache_set_size(208, 120);
                renderView3D();
            }
            mm2_amiga_viewport_cache_save();
            combat_backdrop_cached_ = true;
            combat_backdrop_round_layout_ = combat_round_layout;
        } else {
            mm2_amiga_viewport_cache_restore();
            if (combat_round_layout) {
                /* Exploration chrome present can leave the wrong dividers —
                 * re-stamp combat rules without wiping the hood/roster. */
                gfx::drawCombatScreenLines(compositor_);
            }
        }
#else
        if (combat_round_layout) {
            renderCombatBackdrop();
        } else {
            renderView3D();
        }
#endif
        refreshCombatSprites();
        if (combat_round_layout) {
#if MM2_HOST_AMIGA
            /* Rebuild path already stamped lines via drawCombatScreenChrome. */
#else
            gfx::drawCombatScreenLines(compositor_);
#endif
            blitCombatSprites();
            /* Sprites may paint past the narrow hood — re-mask the divider col. */
            maskCombatBackdropBleed(compositor_);
            gfx::drawCombatViewportFrame(compositor_);
            gfx::drawCombatViewportDivider(compositor_);
        } else {
            blitCombatSprites();
            gfx::drawPlayViewportDivider(compositor_);
        }
    } else if (!scripted_active || !scripted_scene_.hidesView3D()) {
        if (overlay_ != PlayOverlay::Automap && !viewportHiddenByOverlay()) {
#if MM2_HOST_AMIGA
            if (overlay_anim_only) {
                /* Retail buf_copy_rect @ 0x171AC: restore the saved 3D viewport
                 * instead of rebuilding floor+sky+wall slices every ghost cel. */
                mm2_amiga_viewport_cache_restore();
                if (!world_.isOutdoor()) {
                    blitIndoorTorches();
                }
            } else {
                mm2_amiga_viewport_cache_set_size(208, 120);
                /* Cache floor/sky/walls only — torch flicker reuses this snapshot. */
                if (world_.isOutdoor()) {
                    renderView3D();
                } else {
                    renderIndoorView3DBase();
                }
                drawSpellEyeOverlayIfActive(compositor_, env_, world_, gs_, assets_ok_);
                mm2_amiga_viewport_cache_save();
                if (!world_.isOutdoor()) {
                    blitIndoorTorches();
                }
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
        gfx::drawCombatRightColumn(compositor_, combat_view);
        gfx::drawCombatOptionsBar(compositor_, combat_view);
    } else {
        gfx::PlayRightPanel panel = right_panel_;
        if (scripted_active) {
            panel = scripted_scene_.panelMode() == 0 ? gfx::PlayRightPanel::Options : gfx::PlayRightPanel::Protect;
        }

        if (overlay_ == PlayOverlay::None || overlay_ == PlayOverlay::StatusMessage ||
            overlay_ == PlayOverlay::GameOver || overlay_ == PlayOverlay::QuitConfirm ||
            overlay_ == PlayOverlay::RestConfirm || overlay_ == PlayOverlay::TownService ||
            overlay_ == PlayOverlay::SearchIdentify || overlay_ == PlayOverlay::ExchangeOrder ||
            overlay_ == PlayOverlay::DismissHireling || overlay_ == PlayOverlay::UnlockWho ||
            overlay_ == PlayOverlay::DeathStrikes || overlay_ == PlayOverlay::FdPrintChrome) {
            /* Status-line prompts and town services draw in the lower console band
             * only (faithful, not fullscreen modals) — keep 3D view + right column. */
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
        if (!world_.isOutdoor() && !combat_.active()) {
            blitIndoorTorches();
        }
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

    const gfx::CombatPanelView view = combat_.panelView();
    const bool round_layout = view.label_monster_slots;
    if (round_layout != combat_backdrop_round_layout_ || !mm2_amiga_viewport_cache_valid()) {
        /* Cached hood was rendered for the other layout — full repaint. */
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    mm2_amiga_viewport_cache_restore();
    refreshCombatSprites();
    blitCombatSprites();
    if (round_layout) {
        maskCombatBackdropBleed(compositor_);
        gfx::drawCombatViewportFrame(compositor_);
        gfx::drawCombatViewportDivider(compositor_);
    } else {
        gfx::drawPlayViewportDivider(compositor_);
    }
}

void GameSession::renderFrameCombatHudOnly()
{
    /* Surgical combat redraw matching ASM round patches:
     *   0x129CC monster column, 0x12848 party strip, 0x1119E message band.
     * Copy last frame (keeps hood/sprite), then text only — no cache restore,
     * no BOB blit, no bleed mask (those are for layout/entry frames).
     * Cast/menu keystrokes only need 0x1119E; skip roster/party when unchanged. */
    if (!mm2_amiga_copy_front_to_back()) {
        combat_hud_sigs_valid_ = false;
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    const gfx::CombatPanelView view = combat_.panelView();
    if (view.label_monster_slots != combat_backdrop_round_layout_) {
        combat_hud_sigs_valid_ = false;
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    const uint32_t roster_sig = combatRosterHudSig(view, combat_.activeMonsterSlot());
    const uint32_t party_sig = combatPartyHudSig(roster_, launch_);
    const bool roster_changed = !combat_hud_sigs_valid_ || roster_sig != combat_hud_roster_sig_;
    const bool party_changed = !combat_hud_sigs_valid_ || party_sig != combat_hud_party_sig_;

    if (roster_changed) {
        gfx::drawCombatRightColumn(compositor_, view);
        combat_hud_roster_sig_ = roster_sig;
    }
    /* 0x1119E message / options / cast prompts — always patch on HUD dirty. */
    gfx::drawCombatOptionsBar(compositor_, view);
    if (party_changed) {
        renderPartyPanel();
        combat_hud_party_sig_ = party_sig;
    }
    combat_hud_sigs_valid_ = true;
}

void GameSession::renderFrameView3DOnly()
{
    if (!mm2_amiga_copy_front_to_back()) {
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    if (overlay_ != PlayOverlay::Automap) {
        /* Cache floor/sky/walls; torch cels are phase-dependent and go on after. */
        if (world_.isOutdoor()) {
            renderView3D();
        } else {
            renderIndoorView3DBase();
        }
        drawSpellEyeOverlayIfActive(compositor_, env_, world_, gs_, assets_ok_);
        mm2_amiga_viewport_cache_save();
        if (!world_.isOutdoor()) {
            blitIndoorTorches();
        }
        gfx::drawPlayViewportDivider(compositor_);
    }

    if (text_dirty_) {
        const bool protect_panel = right_panel_ == gfx::PlayRightPanel::Protect;
        gfx::drawPlayStatusBar(compositor_, gs_.valid() ? gs_.day() : 0, gs_.valid() ? gs_.year() : 0,
                               gs_.valid() ? gs_.facingKey() : 'N', protect_panel);
        /* Console message band (OP_01/02/03 + text-entry cursor). Partial 3D
         * refresh previously skipped this and left torn/stale glyphs. */
        if (events_loaded_ && !viewportHiddenByOverlay()) {
            if (overlay_ == PlayOverlay::TownService) {
                events_.textView().drawPersistentViewportOverlays(compositor_);
                events_.textView().drawServiceSignOverlay(compositor_);
                events_.textView().drawPegasusIllustration(compositor_);
            } else {
                events_.textView().draw(compositor_);
            }
        }
        renderOverlays();
    } else if (events_loaded_) {
        events_.textView().drawPersistentViewportOverlays(compositor_);
        events_.textView().drawServiceSignOverlay(compositor_);
        events_.textView().drawPegasusIllustration(compositor_);
    }
}

void GameSession::renderFrameTextOnly()
{
    /* Console / status-line text refresh without rebuilding the 3D hood. */
    if (!mm2_amiga_copy_front_to_back()) {
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    const bool protect_panel = right_panel_ == gfx::PlayRightPanel::Protect;
    gfx::drawPlayStatusBar(compositor_, gs_.valid() ? gs_.day() : 0, gs_.valid() ? gs_.year() : 0,
                           gs_.valid() ? gs_.facingKey() : 'N', protect_panel);

    /* Clear the lower console band before redraw so partial updates cannot leave
     * stale spinner/cursor ink (cols 1..38, rows 17..22). */
    gfx::fillCellRect(compositor_, 1, 0x11, 38, 6);

    if (events_loaded_ && !combat_.active() &&
        !(scripted_loaded_ && scripted_scene_.active()) && !viewportHiddenByOverlay()) {
        if (overlay_ == PlayOverlay::TownService) {
            events_.textView().drawPersistentViewportOverlays(compositor_);
            events_.textView().drawServiceSignOverlay(compositor_);
            events_.textView().drawPegasusIllustration(compositor_);
        } else {
            events_.textView().draw(compositor_);
        }
    }

    renderOverlays();
}

#endif

void GameSession::render()
{
#if MM2_HOST_AMIGA
    /* Combat HUD patches (message / roster / party) — ASM never rebuilds chrome. */
    if (combat_.active() && text_dirty_ && !view3d_dirty_ && !chrome_dirty_ &&
        overlay_ == PlayOverlay::None && combat_backdrop_cached_) {
        renderFrameCombatHudOnly();
        play_buffer_valid_ = true;
        return;
    }

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

    /* Status / console text only (facing, OP_2F entry echo, quit/rest prompts). */
    if (text_dirty_ && !view3d_dirty_ && !chrome_dirty_ && !overlay_anim_dirty_) {
        renderFrameTextOnly();
        play_buffer_valid_ = true;
        return;
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
