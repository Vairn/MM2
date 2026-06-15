#include "mm2/GameSession.h"

#include "mm2/Mm2Dbg.h"
#include "mm2/CppStdCompat.h"

#include "mm2/DataPath.h"
#include "mm2/gameplay/Movement.h"
#include "mm2/gameplay/PlaySessionInput.h"
#include "mm2/runtime/Alloc.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/AutomapView.h"
#include "mm2/gfx/PlayScreenChrome.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/Platform.h"
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#endif

namespace mm2 {

namespace {

const char *kTownNames[] = {"?", "Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};

void blitImageFrame(gfx::ScreenCompositor &c, const mm2_image32_file &img, int frame, int dst_x, int dst_y,
                    int opaque = 0)
{
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
    assets_ok_ = false;
    overlay_ = PlayOverlay::None;
    automap_.clearAll();
    sheet_session_ = {};
    sheet_session_.party_slot = 0;
    status_message_[0] = '\0';
    has_items_ = false;

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

    const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
    bool has_env = false;
    bool has_sky = false;
    if (has_world) {
        has_sky = env_.loadGlobal(data_dir_);
        has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
    }
    assets_ok_ = has_world && has_env && has_sky;
    events_loaded_ = events_.load(data_dir_);
    if (events_loaded_) {
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
        std::fprintf(stderr, "mm2: play view missing assets in %s (world=%d env=%d sky=%d)\n", data_dir_,
                     has_world, has_env, has_sky);
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
        }
        bootstrap_step_ = 1;
        markDirty();
        return;
    case 1: {
        MM2_DBG("MM2 GOTO: bootstrap world load screen=%d\n", gs_.screenId());
        const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
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
            if (events_loaded_) {
                refreshEventsForScreen();
            }
        }
        if (scripted_loaded_) {
            maybeQueueScriptedScenes(true);
        }
        automap_.markPartyTileIfCartographer(gs_.screenId(), gs_.coordX(), gs_.coordY(), roster_,
                                               launch_);
    }
}

void GameSession::maybeQueueScriptedScenes(bool on_start)
{
    if ((start_flags_ & kStartSkipScriptedIntros) != 0) {
        return;
    }
    if (!scripted_loaded_ || !gs_.valid() || scripted_scene_.active()) {
        return;
    }

    if (on_start && gs_.screenId() == 0 && !gs_.corakIntroSeen()) {
        gs_.setFirstTimeFlag(true);
        scripted_scene_.queueScene(events::ScriptedSceneId::CorakIntro);
        gs_.setCorakIntroSeen(true);
        markDirty();
        return;
    }

    /* Guardian Pegasus: loc 11 (sector C2) evt 04 @ tile (y,x)=(4,7) ENTER — FAQ (7,4)^. */
    if (!on_start && gs_.screenId() == 11 && gs_.coordY() == 4 && gs_.coordX() == 7 &&
        !gs_.pegasusIntroSeen()) {
        scripted_scene_.queueScene(events::ScriptedSceneId::PegasusC2);
        gs_.setPegasusIntroSeen(true);
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
    if (events_loaded_) {
        /* OP_0C map_transition: drop OP_0B portraits / popups from the prior screen
         * (enterLocation → text_.reset). Without this, Middlegate shop .anm overlays
         * the overland viewport after exiting town. */
        refreshEventsForScreen();
    }
    markDirty();
}

void GameSession::runPendingEvents()
{
    if (!events_loaded_ || !gs_.valid()) {
        return;
    }
    if (mm2_gs_u8(gs_.a4(), MM2_GS_PENDING_EVENT_LATCH)) {
        const bool blocking_before = events_.blocksMovement();
        events_.scanAndRun(gs_, world_);
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
        showStatusMessage("Bash Door (GAP 0x9B48).");
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
        showStatusMessage("Unlock door (GAP 0x20CA2).");
        break;
    case gameplay::PlaySessionAction::Rest: {
        /* Minimal stub: restore HP/SP for party. GAP: full 0x19E20 prompt/pay/day advance. */
        for (int i = 0; i < launch_.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            const int idx = launch_.roster_slots[i];
            if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
                continue;
            }
            Mm2RosterRecord &rec = roster_.records[idx];
            rec.hp_current = rec.hp_max;
            rec.sp_current = rec.sp_max;
        }
        showStatusMessage("Party rested (stub 0x19E20).");
        break;
    }
    default:
        break;
    }
}

void GameSession::tickOverlayInput(const platform::KeyState &keys)
{
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
            markDirty();
            return;
        }
        if (keys.key_q && !keys.ctrl) {
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
            ingame_sheet_.handleKey(ch, sheet_session_, roster_, launch_, items_ptr);
        if (outcome == gameplay::SheetKeyOutcome::Close) {
            overlay_ = PlayOverlay::None;
        }
        markDirty();
        return;
    }
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
    bool changed = false;
    if (scripted_loaded_ && scripted_scene_.active()) {
        changed |= scripted_scene_.tickAnimation();
    }
    if (events_loaded_ && events_.textView().hasServicePortrait()) {
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

void GameSession::tick(const platform::KeyState &keys)
{
    if (!gs_.valid()) {
        return;
    }

    tickOverlayAnimations();

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
                    move = gameplay::step(world_, gs_, true);
                    break;
                case gameplay::ExploreCode::Back:
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
    refreshWorldAfterEventTransition();
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
    blitImageFrame(compositor_, env_.sky(), 0, kView3DOriginX, kView3DSkyY, 1);

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

void GameSession::renderOverlays()
{
    switch (overlay_) {
    case PlayOverlay::QuickRef:
        ingame_sheet_.renderQuickRef(compositor_, roster_, launch_);
        break;
    case PlayOverlay::CharacterSheet:
        ingame_sheet_.renderSheet(compositor_, roster_, launch_, sheet_session_.party_slot,
                                  has_items_ ? &items_ : nullptr, &sheet_session_);
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
    default:
        break;
    }
}

void GameSession::renderFrame(bool overlay_anim_only)
{
    const bool scripted_active = scripted_loaded_ && scripted_scene_.active();

    /* Chrome (red -809E border frame + black interior fills) must be redrawn every
     * frame: with double-buffering the borders live OUTSIDE the cached 3D viewport
     * region, so the fast overlay path still has to paint them into the current
     * back buffer or they vanish on alternating buffers. */
#if MM2_HOST_AMIGA
    if (!overlay_anim_only) {
        compositor_.clear(0, 0, 0, 255);
    }
#else
    (void)overlay_anim_only;
    compositor_.clear(0, 0, 0, 255);
#endif
    gfx::drawPlayScreenChrome(compositor_);

    if (!scripted_active || !scripted_scene_.hidesView3D()) {
        if (overlay_ != PlayOverlay::Automap) {
#if MM2_HOST_AMIGA
            if (overlay_anim_only) {
                /* Retail buf_copy_rect @ 0x171AC: restore the saved 3D viewport
                 * instead of rebuilding floor+sky+wall slices every ghost cel. */
                mm2_amiga_viewport_cache_restore();
            } else {
                renderView3D();
                mm2_amiga_viewport_cache_save();
            }
#else
            renderView3D();
#endif
        }
    }

    gfx::PlayRightPanel panel = right_panel_;
    if (scripted_active) {
        panel = scripted_scene_.panelMode() == 0 ? gfx::PlayRightPanel::Options : gfx::PlayRightPanel::Protect;
    }

    const bool protect_panel = panel == gfx::PlayRightPanel::Protect;
    gfx::drawPlayStatusBar(compositor_, gs_.valid() ? gs_.day() : 0, gs_.valid() ? gs_.year() : 0,
                           gs_.valid() ? gs_.facingKey() : 'N', protect_panel);
    renderPartyPanel();

    if (overlay_ == PlayOverlay::None || overlay_ == PlayOverlay::StatusMessage) {
        const gfx::PlayProtectValues prot = protectValues();
        gfx::drawPlayRightColumn(compositor_, panel, protect_panel ? &prot : nullptr);
    }

    if (scripted_active) {
        scripted_scene_.draw(compositor_);
    }

    renderOverlays();

    if (events_loaded_ && !scripted_active) {
        events_.textView().draw(compositor_);
    }
}

#if MM2_HOST_AMIGA
bool GameSession::canUsePartialView3DRefresh() const
{
    if (!play_buffer_valid_ || !view3d_dirty_ || chrome_dirty_ || overlay_anim_dirty_) {
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

void GameSession::renderFrameView3DOnly()
{
    if (!mm2_amiga_copy_front_to_back()) {
        renderFrame(false);
        play_buffer_valid_ = true;
        return;
    }

    if (overlay_ != PlayOverlay::Automap) {
        renderView3D();
        mm2_amiga_viewport_cache_save();
    }

    if (text_dirty_) {
        const bool protect_panel = right_panel_ == gfx::PlayRightPanel::Protect;
        gfx::drawPlayStatusBar(compositor_, gs_.valid() ? gs_.day() : 0, gs_.valid() ? gs_.year() : 0,
                               gs_.valid() ? gs_.facingKey() : 'N', protect_panel);
    }
}
#endif

void GameSession::render()
{
#if MM2_HOST_AMIGA
    /* Overlay-animation-only frame (ghost/sign cel changed, nothing else): keep the
     * borders + panel + text, restore the cached 3D viewport, repaint the sprite. */
    if (overlay_anim_dirty_ && !view3d_dirty_ && !chrome_dirty_ && !text_dirty_ &&
        mm2_amiga_viewport_cache_valid()) {
        renderFrame(true);
        return;
    }

    if (canUsePartialView3DRefresh()) {
        renderFrameView3DOnly();
        return;
    }
#endif
    renderFrame(false);
#if MM2_HOST_AMIGA
    play_buffer_valid_ = true;
#endif
}

}  // namespace mm2
