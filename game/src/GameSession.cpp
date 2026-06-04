#include "mm2/GameSession.h"

#include "mm2/CppStdCompat.h"
#include "mm2/DataPath.h"
#include "mm2/runtime/Alloc.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"

#include "mm2/gfx/PlayScreenChrome.h"

#include "mm2/gfx/View3D.h"






namespace mm2 {



namespace {



const char *kTownNames[] = {"?", "Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};

void blitImageFrame(gfx::ScreenCompositor &c, const mm2_image32_file &img, int frame, int dst_x, int dst_y)

{

    if (frame < 0 || frame >= img.frame_count) {

        return;

    }

    const mm2_image32_frame &f = img.frames[frame];

    if (!f.rgba) {

        return;

    }

    c.blitRgba(f.rgba, f.width, f.height, dst_x, dst_y);

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



int GameSession::facingFromKey(char key)

{

    switch (key) {

        case 'N':

            return 0;

        case 'E':

            return 1;

        case 'S':

            return 2;

        case 'W':

            return 3;

        default:

            return 0;

    }

}



bool GameSession::loadImage(const char *name, mm2_image32_file *out)

{

    char path[512];

    if (!joinDataPath(path, sizeof(path), data_dir_, name)) {

        return false;

    }

    ::memset(out, 0, sizeof(*out));

    return mm2_image32_load_file(path, out) == MM2_IMAGE32_OK && out->frame_count > 0 && out->frames[0].rgba;

}



bool GameSession::loadMapVisualPage(int screen_idx, uint8_t *out_page)

{

    char path[512];

    if (!joinDataPath(path, sizeof(path), data_dir_, "map.dat")) {

        return false;

    }

    FILE *f = std::fopen(path, "rb");

    if (!f) {

        return false;

    }

    const long offset = static_cast<long>(screen_idx) * 512L;

    if (std::fseek(f, offset, SEEK_SET) != 0) {

        std::fclose(f);

        return false;

    }

    const std::size_t n = std::fread(out_page, 1, gfx::kMapPageSize, f);

    std::fclose(f);

    return n == gfx::kMapPageSize;

}



bool GameSession::start(const char *data_dir, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch)

{

    data_dir_ = data_dir;

    roster_ = roster;

    launch_ = launch;

    quit_ = false;

    back_to_title_ = false;

    assets_ok_ = false;

    if (!gs_image_) {
        gs_image_ = static_cast<uint8_t *>(mm2::runtime::allocate(kGsImageBytes));
        if (!gs_image_) {
            return false;
        }
    }
    ::memset(gs_image_, 0, kGsImageBytes);

    mm2_party_launch_apply(mm2_gs_base_from_image(gs_image_), &launch_);



    camera_.x = launch_.coord_x;

    camera_.y = launch_.coord_y;

    camera_.facing = facingFromKey(launch_.facing_key);



    mm2_image32_set_preview_opaque(0);

    const bool has_walls = loadImage("town.32", &walls_);

    const bool has_floor = loadImage("townf.32", &floor_);

    const bool has_sky = loadImage("sky.32", &sky_);

    const bool has_map = loadMapVisualPage(launch_.area_id, map_visual_);

    assets_ok_ = has_walls && has_floor && has_sky && has_map;



#if !MM2_NO_STL
    if (!assets_ok_) {
        std::fprintf(stderr,
                     "mm2: play view missing assets in %s (town.32=%d townf.32=%d sky.32=%d map.dat=%d)\n",
                     data_dir_, has_walls, has_floor, has_sky, has_map);
    }
#endif

    return true;

}



void GameSession::shutdown()

{

    mm2_image32_free(&walls_);

    mm2_image32_free(&floor_);

    mm2_image32_free(&sky_);

    if (gs_image_) {
        mm2::runtime::deallocate(gs_image_, kGsImageBytes);
        gs_image_ = nullptr;
    }

    data_dir_ = nullptr;

    assets_ok_ = false;

}



void GameSession::tick(const platform::KeyState &keys)

{

    if (keys.quit || keys.escape) {

        back_to_title_ = true;

        return;

    }

    if (keys.key_q) {

        quit_ = true;

    }

    if (keys.left) {

        camera_.facing = (camera_.facing + 3) & 3;

        launch_.facing_key = "NWSE"[camera_.facing];

    } else if (keys.right) {

        camera_.facing = (camera_.facing + 1) & 3;

        launch_.facing_key = "NWSE"[camera_.facing];

    }

}



void GameSession::renderView3D()

{

    using namespace gfx;

    using namespace gfx::play_layout;



    if (!assets_ok_) {

        compositor_.drawTextShadow(kViewOriginX, kViewOriginY, "(missing town.32 / map.dat)", 200, 120, 120);

        return;

    }



    const View3DMapBuffers bufs = buildView3DMapBuffers(map_visual_);

    const View3DScene scene = buildView3DScene(bufs, camera_);



    blitImageFrame(compositor_, floor_, 0, kView3DOriginX, kView3DFloorY);

    blitImageFrame(compositor_, sky_, 0, kView3DOriginX, kView3DSkyY);



    for (const View3DBlit &b : scene.blits) {

        blitImageFrame(compositor_, walls_, b.frame, b.x, b.y);

    }

}



void GameSession::render()

{

    compositor_.clear(0, 0, 0, 255);



    gfx::drawPlayScreenChrome(compositor_);

    renderView3D();



    gfx::drawPlayStatusBar(compositor_, game_day_, game_year_, launch_.facing_key);



    char names[8][16];

    int hp_cur[8]{};

    int hp_max[8]{};

    for (int i = 0; i < launch_.party_count && i < 8; ++i) {

        const int slot = launch_.roster_slots[i];

        if (slot < 0 || slot >= MM2_ROSTER_RECORD_COUNT) {

            names[i][0] = '\0';

            continue;

        }

        const Mm2RosterRecord &rec = roster_.records[slot];

        mm2_roster_name_to_cstr(&rec, names[i], sizeof(names[i]));

        hp_cur[i] = rec.hp_current;

        hp_max[i] = rec.hp_max;

    }

    gfx::drawPlayPartyList(compositor_, launch_.party_count, names, hp_cur, hp_max);



    compositor_.drawTextShadow(8, 192, "ESC title  Q quit  Left/Right turn", 140, 140, 160);

}



}  // namespace mm2

