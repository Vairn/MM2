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
    if (static_cast<int>(scene.decor.size()) != 0) {
        std::fprintf(stderr, "FAIL: expected 0 decor blits, got %d\n",
                     static_cast<int>(scene.decor.size()));
        ++fails;
    }
    if (static_cast<int>(scene.horizon.size()) != kHorizonCount) {
        std::fprintf(stderr, "FAIL: expected %d horizon blits, got %d\n", kHorizonCount,
                     static_cast<int>(scene.horizon.size()));
        ++fails;
    }
    for (int i = 0; i < kHorizonCount && i < static_cast<int>(scene.horizon.size()); ++i) {
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

    if (fails == 0) {
        std::printf("OK: C2 (11,7,3,W) outdoor horizon matches (%d blits)\n", kHorizonCount);
        return 0;
    }
    return 1;
}
