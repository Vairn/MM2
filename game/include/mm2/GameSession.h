#pragma once



#include "mm2/gfx/ScreenCompositor.h"

#include "mm2/gfx/View3D.h"

#include "mm2/platform/Platform.h"

#include "mm2_party_launch.h"

#include "mm2_roster_codec.h"

#include "mm2_image32_codec.h"



namespace mm2 {



/* In-town play session after title Goto Town → Z (ASM game_world_boot @ 0x0F1C). */

class GameSession {

public:

    static const char *areaName(uint8_t area_id);



    bool start(const char *data_dir, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch);

    void shutdown();



    void tick(const platform::KeyState &keys);

    void render();



    bool shouldQuit() const { return quit_; }

    bool requestTitle() const { return back_to_title_; }

    void clearTitleRequest() { back_to_title_ = false; }



    const gfx::ScreenCompositor &compositor() const { return compositor_; }



private:

    static const char *townName(uint8_t town_filter);

    static int facingFromKey(char key);



    bool loadImage(const char *name, mm2_image32_file *out);

    bool loadMapVisualPage(int screen_idx, uint8_t *out_page);

    void renderView3D();



    const char *data_dir_ = nullptr;

    bool quit_ = false;

    bool back_to_title_ = false;

    bool assets_ok_ = false;



    Mm2PartyLaunch launch_{};

    Mm2RosterFile roster_{};



    gfx::View3DCamera camera_{};

    uint8_t map_visual_[gfx::kMapPageSize]{};



    mm2_image32_file walls_{};

    mm2_image32_file floor_{};

    mm2_image32_file sky_{};



    uint8_t gs_image_[MM2_A4_ANCHOR + 0x8000]{};



    int game_day_ = 83;

    int game_year_ = 900;



    gfx::ScreenCompositor compositor_;

};



}  // namespace mm2

