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

void blitImageFrame(gfx::ScreenCompositor &c, const mm2_image32_file &img, int frame, int dst_x, int dst_y)
{
    if (frame < 0 || frame >= img.frame_count) {
        return;
    }

    const mm2_image32_frame &f = img.frames[frame];
#if MM2_HOST_AMIGA
    if (!f.bitmap) {
        return;
    }
    platform::blitImage32(&img, frame, dst_x, dst_y, 1);
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

bool GameSession::start(const char *data_dir, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch)
{
    data_dir_ = data_dir;
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

    ingame_sheet_.loadAssets(data_dir_);

    char *path = mm2_path_scratch_a();
    if (joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "items.dat")) {
        has_items_ = mm2_items_load_file(path, &items_) == MM2_ITEMS_OK;
    }

    mm2_image32_set_preview_opaque(0);
    const bool has_world = world_.load(data_dir_) && world_.enterScreen(gs_.screenId());
    bool has_env = false;
    bool has_sky = false;
    if (has_world) {
        has_sky = env_.loadGlobal(data_dir_);
        has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
    }
    assets_ok_ = has_world && has_env && has_sky;

#if !MM2_NO_STL
    if (!assets_ok_) {
        std::fprintf(stderr, "mm2: play view missing assets in %s (world=%d env=%d sky=%d)\n", data_dir_,
                     has_world, has_env, has_sky);
    }
#endif

    return true;
}

void GameSession::shutdown()
{
    env_.unloadAll();
    if (gs_image_) {
        mm2::runtime::deallocate(gs_image_, kGsImageBytes);
        gs_image_ = nullptr;
    }
    gs_ = GameStateView();

    data_dir_ = nullptr;
    assets_ok_ = false;
    overlay_ = PlayOverlay::None;
    has_items_ = false;
}

void GameSession::refreshWorldAfterMove(const gameplay::MoveResult &move)
{
    if (!move.acted) {
        return;
    }

    if (move.screen_changed && data_dir_) {
        world_.enterScreen(gs_.screenId());
        const bool has_env = env_.loadEnv(data_dir_, gfx::envKindFromAttrib(world_.attrib()));
        assets_ok_ = world_.loaded() && env_.loadGlobal(data_dir_) && has_env;
    }
}

bool GameSession::overlayBlocksInput() const
{
    return overlay_ != PlayOverlay::None;
}

void GameSession::showStatusMessage(const char *msg)
{
    if (!msg) {
        status_message_[0] = '\0';
        overlay_ = PlayOverlay::None;
        return;
    }
    std::snprintf(status_message_, sizeof(status_message_), "%s", msg);
    overlay_ = PlayOverlay::StatusMessage;
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
        }
        return;
    }

    if (overlay_ == PlayOverlay::Controls) {
        if (keys.escape) {
            overlay_ = PlayOverlay::None;
            return;
        }
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch >= '1' && ch <= '4') {
            controls_screen_.handleKey(ch, gs_);
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
        }
        return;
    }

    if (overlay_ == PlayOverlay::QuickRef) {
        if (keys.escape) {
            overlay_ = PlayOverlay::None;
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
                sheet_session_.status_line[0] = '\0';
                overlay_ = PlayOverlay::CharacterSheet;
            }
        }
        return;
    }

    if (overlay_ == PlayOverlay::CharacterSheet) {
        if (keys.escape) {
            overlay_ = PlayOverlay::None;
            sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
            sheet_session_.status_line[0] = '\0';
            return;
        }
        if (keys.key_q && !keys.ctrl) {
            overlay_ = PlayOverlay::QuickRef;
            return;
        }

        int party_slot = -1;
        gameplay::PlaySessionAction action = gameplay::PlaySessionAction::None;
        if (gameplay::pollPlaySessionAction(keys, launch_.party_count, &action, &party_slot)) {
            if (action == gameplay::PlaySessionAction::ViewCharacter) {
                /* 0x907A digit chain: switch character without closing sheet. */
                sheet_session_.party_slot = party_slot;
                sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
                sheet_session_.status_line[0] = '\0';
                return;
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
        return;
    }
}

void GameSession::tickPlayInput(const platform::KeyState &keys)
{
    if (keys.ctrl && keys.key_q) {
        overlay_ = PlayOverlay::QuitConfirm;
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
        break;
    case gameplay::PlaySessionAction::QuickRef:
        overlay_ = PlayOverlay::QuickRef;
        break;
    case gameplay::PlaySessionAction::ViewCharacter:
        sheet_session_.party_slot = party_slot;
        sheet_session_.sub_mode = gameplay::SheetSubMode::Normal;
        sheet_session_.status_line[0] = '\0';
        overlay_ = PlayOverlay::CharacterSheet;
        break;
    case gameplay::PlaySessionAction::PanelOptions:
        if (right_panel_ == gfx::PlayRightPanel::Protect) {
            right_panel_ = gfx::PlayRightPanel::Options;
            gs_.setRightPanelMode(0);
        }
        break;
    case gameplay::PlaySessionAction::PanelProtect:
        if (right_panel_ == gfx::PlayRightPanel::Options) {
            right_panel_ = gfx::PlayRightPanel::Protect;
            gs_.setRightPanelMode(1);
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

void GameSession::tick(const platform::KeyState &keys)
{
    if (!gs_.valid()) {
        return;
    }

    if (overlayBlocksInput()) {
        tickOverlayInput(keys);
        return;
    }

    tickPlayInput(keys);
    if (overlayBlocksInput()) {
        return;
    }

    gameplay::ExploreCode code{};
    if (!gameplay::pollExploreCode(keys, &code)) {
        return;
    }

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

void GameSession::renderView3D()
{
    using namespace gfx;
    using namespace gfx::play_layout;

    if (!assets_ok_) {
        compositor_.drawTextShadow(kViewOriginX, kViewOriginY, "(missing town.32 / map.dat)", 200, 120, 120);
        return;
    }

    View3DCamera camera{};
    camera.x = gs_.coordX();
    camera.y = gs_.coordY();
    camera.facing = gs_.facing03();

    const View3DMapBuffers bufs = world_.buildView3DMapBuffers();
    const View3DScene scene = buildView3DScene(bufs, camera);

    const int sky_frame = world_.roofBitAt(camera.x, camera.y) ? 1 : 0;
    blitImageFrame(compositor_, env_.floor(), 0, kView3DOriginX, kView3DFloorY);
    blitImageFrame(compositor_, env_.sky(), sky_frame, kView3DOriginX, kView3DSkyY);

    for (const View3DBlit &b : scene.blits) {
        blitImageFrame(compositor_, env_.walls(), b.frame, b.x, b.y);
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
        slots[i].hp = rec.hp_max;
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
        compositor_.drawText(8, 17 * 8, status_message_, 255, 255, 128, 255);
        compositor_.drawText(8, 18 * 8, "(ESC to dismiss)", 180, 180, 180, 255);
        break;
    case PlayOverlay::QuitConfirm:
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
    renderView3D();

    const bool protect_panel = right_panel_ == gfx::PlayRightPanel::Protect;
    gfx::drawPlayStatusBar(compositor_, gs_.valid() ? gs_.day() : 0, gs_.valid() ? gs_.year() : 0,
                           gs_.valid() ? gs_.facingKey() : 'N', protect_panel);
    renderPartyPanel();

    if (overlay_ == PlayOverlay::None || overlay_ == PlayOverlay::StatusMessage) {
        const gfx::PlayProtectValues prot = protectValues();
        gfx::drawPlayRightColumn(compositor_, right_panel_, protect_panel ? &prot : nullptr);
    }

    renderOverlays();
}

}  // namespace mm2
