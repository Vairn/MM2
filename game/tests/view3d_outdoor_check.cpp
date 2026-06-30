// Offline check: C2 overland (screen 11) outdoor scene @ outdoor_3d_master 0x18870.
// Reference trace: python tools/view3d_outdoor.py map.dat attrib.dat 11 7 3 3 --trace
//
// Usage: view3d_outdoor_check <data_dir>

#include <cstdio>

#include "mm2/gfx/OutdoorView3D.h"
#include "mm2/world/MapWorld.h"

namespace {

struct Expect {
    mm2::gfx::OutdoorHorizonSheet sheet;
    int frame;
    int x;
    int y;
};

constexpr Expect kHorizonExpected[] = {
    {mm2::gfx::OutdoorHorizonSheet::Outdoor3, 0, 40, 21},
    {mm2::gfx::OutdoorHorizonSheet::Outdoor2, 4, 8, 36},
    {mm2::gfx::OutdoorHorizonSheet::Outdoor3, 6, 176, 21},
};
constexpr int kHorizonCount = static_cast<int>(sizeof(kHorizonExpected) / sizeof(kHorizonExpected[0]));

/* Night stars for facing W (cam.facing=3) — traced from the star plot loop
 * @0x0687C (base=1*4, no wrap; pen toggles A=20/B=1; x=X*8+8, +12 cols for the
 * last three). */
struct StarExpect {
    int x;
    int y;
    uint8_t pen;
};
constexpr StarExpect kStarsExpectedW[] = {
    {24, 23, 20}, {40, 54, 1}, {80, 56, 20}, {104, 20, 1}, {120, 55, 20}, {136, 22, 1}, {176, 50, 20},
};

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : ".";

    mm2::world::MapWorld world;
    if (!world.load(data_dir)) {
        std::fprintf(stderr, "FAIL: cannot load map.dat/attrib.dat from %s\n", data_dir);
        return 1;
    }
    if (!world.enterScreen(11)) {
        std::fprintf(stderr, "FAIL: enterScreen(11)\n");
        return 1;
    }

    const Mm2AttribRecord &rec = world.attrib();
    if (!mm2_attrib_is_outdoor(&rec)) {
        std::fprintf(stderr, "FAIL: screen 11 should be outdoor (surface=0x%02X)\n",
                     mm2_attrib_surface_flag(&rec));
        return 1;
    }
    if (rec.raw[MM2_ATTRIB_OFF_COMPLEX_ID] != 0xC2) {
        std::fprintf(stderr, "FAIL: screen 11 sector label = 0x%02X, expected 0xC2\n",
                     rec.raw[MM2_ATTRIB_OFF_COMPLEX_ID]);
        return 1;
    }

    mm2::gfx::View3DCamera cam{};
    cam.x = 7;
    cam.y = 3;
    cam.facing = 3; /* W */

    const mm2::gfx::OutdoorScene scene = mm2::gfx::buildOutdoorScene(world, cam);

    int fails = 0;
    if (scene.num_decor != 0) {
        std::fprintf(stderr, "FAIL: expected 0 decor blits, got %d\n", scene.num_decor);
        ++fails;
    }
    if (scene.num_horizon != kHorizonCount) {
        std::fprintf(stderr, "FAIL: expected %d horizon blits, got %d\n", kHorizonCount,
                     scene.num_horizon);
        ++fails;
    }
    for (int i = 0; i < kHorizonCount && i < scene.num_horizon; ++i) {
        const mm2::gfx::OutdoorSpriteBlit &b = scene.horizon[static_cast<size_t>(i)];
        const Expect &e = kHorizonExpected[i];
        if (b.horizon != e.sheet || b.frame != e.frame || b.x != e.x || b.y != e.y) {
            std::fprintf(stderr,
                         "FAIL: horizon %d sheet=%d frame=%d @ (%d,%d), expected sheet=%d frame=%d @ (%d,%d)\n",
                         i, static_cast<int>(b.horizon), b.frame, b.x, b.y, static_cast<int>(e.sheet),
                         e.frame, e.x, e.y);
            ++fails;
        }
    }

    /* Night path (subday >= 0x80): 7 plotted stars, facing W. */
    std::array<mm2::gfx::OutdoorStarBlit, mm2::gfx::kOutdoorStarCount> stars{};
    const int nstars = mm2::gfx::buildOutdoorStars(cam.facing, stars);
    if (nstars != mm2::gfx::kOutdoorStarCount) {
        std::fprintf(stderr, "FAIL: expected %d night stars, got %d\n", mm2::gfx::kOutdoorStarCount,
                     nstars);
        ++fails;
    }
    for (int i = 0; i < mm2::gfx::kOutdoorStarCount && i < nstars; ++i) {
        const mm2::gfx::OutdoorStarBlit &s = stars[static_cast<size_t>(i)];
        const StarExpect &e = kStarsExpectedW[i];
        if (s.x != e.x || s.y != e.y || s.pen != e.pen) {
            std::fprintf(stderr, "FAIL: star %d @ (%d,%d) pen=%d, expected (%d,%d) pen=%d\n", i, s.x,
                         s.y, static_cast<int>(s.pen), e.x, e.y, static_cast<int>(e.pen));
            ++fails;
        }
    }

    if (fails == 0) {
        std::printf("OK: C2 (11,7,3,W) outdoor horizon matches (%d blits); night stars match (%d)\n",
                    kHorizonCount, mm2::gfx::kOutdoorStarCount);
        return 0;
    }
    return 1;
}
