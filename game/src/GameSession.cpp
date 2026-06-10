#include "mm2/GameSession.h"

#include "mm2/CppStdCompat.h"

#include "mm2/DataPath.h"
#include "mm2/gameplay/Movement.h"
#include "mm2/gameplay/PlaySessionInput.h"
#include "mm2/runtime/Alloc.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PlayScreenChrome.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/Platform.h"
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

const char *GameSession::areaName(uint8_t area_id)
{
    static const char *kAreas[] = {"Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};
    if (area_id < 5) {
        return kAreas[area_id];
    }
    return "?";
}

const char *GameSession::townName(uint8_t town_filter)
{
    return (town_filter >= 1 && town_filter <= 5) ? kTownNames[town_filter] : "?";
}

bool GameSession::start(const char *data_dir, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                        uint32_t start_flags)
{
    data_dir_ = data_dir;
    start_flags_ = start_flags;
    roster_ = roster;
    launch_ = launch;
    quit_ = false;
    back_to_title_ = false;
    assets_ok_ = false;
    overlay_ = PlayOverlay::None;
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
    bootstrapping_ = true;
    bootstrap_step_ = 0;
    frame_dirty_ = true;
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

    frame_dirty_ = true;

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

    switch (bootstrap_step_) {
    case 0:
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
        const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
        assets_ok_ = has_world;
        bootstrap_step_ = 2;
        markDirty();
        return;
    }
    case 2: {
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
        events_loaded_ = events_.load(data_dir_);
        if (events_loaded_) {
            refreshEventsForScreen();
        }
        bootstrap_step_ = 4;
        markDirty();
        return;
    default:
        scripted_loaded_ = scripted_scene_.load(data_dir_);
        if (scripted_loaded_) {
            maybeQueueScriptedScenes(true);
        }
        bootstrapping_ = false;
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
    frame_dirty_ = false;
#if MM2_HOST_AMIGA
    bootstrapping_ = false;
    bootstrap_step_ = 0;
#endif
}

void GameSession::refreshWorldAfterMove(const gameplay::MoveResult &move)
{
    if (!move.acted) {
        return;
    }

    markDirty();

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
            maybeQueueScriptedScenes(false);
        }
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
        showStatusMessage("Automap not implemented (GAP 0x223A).");
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
        markDirty();
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
            markDirty();
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

    for (const View3DBlit &b : scene.blits) {
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

    for (const OutdoorSpriteBlit &b : scene.decor) {
        blitImageFrame(compositor_, env_.biomeSheet(b.biome), b.frame, b.x, b.y, 0);
    }
    for (const OutdoorSpriteBlit &b : scene.horizon) {
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
    case PlayOverlay::QuitConfirm:
        gfx::fillCellRect(compositor_, 1, 0x11, 38, 1);
        compositor_.drawText(8, 17 * 8, "Quit without game save (y/n)?", 255, 80, 80, 255);
        break;
    default:
        break;
    }
}

void GameSession::render()
{
    compositor_.clear(0, 0, 0, 255);

    gfx::drawPlayScreenChrome(compositor_);

    const bool scripted_active = scripted_loaded_ && scripted_scene_.active();
    if (!scripted_active || !scripted_scene_.hidesView3D()) {
        renderView3D();
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

}  // namespace mm2
