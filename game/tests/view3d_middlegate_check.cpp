// Offline Milestone-1 check: Middlegate (screen 0) at (7,3) facing N must
// produce the exact blit list traced by tools/view3d_trace.py against the
// real map.dat (frustum @ 0x2900, paint order @ 0x2F7E):
//   left  d=1 frame=5  @ (32,22)
//   right d=1 frame=9  @ (160,22)
//   front d=1 frame=17 @ (64,40)   (door -> +0x10 variant)
//   left  d=0 frame=12 @ (8,22)
//   right d=0 frame=13 @ (192,22)
//
// Usage: view3d_middlegate_check <data_dir>

#include <cstdio>

#include "mm2/gfx/View3D.h"
#include "mm2/world/MapWorld.h"

#include "mm2_party_launch.h"

namespace {

struct Expect {
    int frame, x, y;
};

constexpr Expect kExpected[] = {
    {5, 32, 22}, {9, 160, 22}, {17, 64, 40}, {12, 8, 22}, {13, 192, 22},
};
constexpr int kExpectedCount = static_cast<int>(sizeof(kExpected) / sizeof(kExpected[0]));

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : ".";

    mm2::world::MapWorld world;
    if (!world.load(data_dir)) {
        std::fprintf(stderr, "FAIL: cannot load map.dat/attrib.dat from %s\n", data_dir);
        return 1;
    }
    if (!world.enterScreen(0)) {
        std::fprintf(stderr, "FAIL: enterScreen(0)\n");
        return 1;
    }

    mm2::gfx::View3DCamera cam{};
    cam.x = 7;
    cam.y = 3;
    cam.facing = 0; /* N */

    const mm2::gfx::View3DMapBuffers bufs = world.buildView3DMapBuffers();
    const mm2::gfx::View3DScene scene = mm2::gfx::buildView3DScene(bufs, cam);

    int fails = 0;
    if (static_cast<int>(scene.blits.size()) != kExpectedCount) {
        std::fprintf(stderr, "FAIL: expected %d blits, got %d\n", kExpectedCount,
                     static_cast<int>(scene.blits.size()));
        ++fails;
    }
    for (int i = 0; i < kExpectedCount && i < static_cast<int>(scene.blits.size()); ++i) {
        const mm2::gfx::View3DBlit &b = scene.blits[static_cast<size_t>(i)];
        const Expect &e = kExpected[i];
        if (b.frame != e.frame || b.x != e.x || b.y != e.y) {
            std::fprintf(stderr, "FAIL: blit %d = frame %d @ (%d,%d), expected frame %d @ (%d,%d)\n",
                         i, b.frame, b.x, b.y, e.frame, e.x, e.y);
            ++fails;
        }
    }

    /* Sanity: attrib record for Middlegate (interior town, spawn (7,3)). */
    const Mm2AttribRecord &rec = world.attrib();
    if (mm2_attrib_env_type(&rec) != MM2_ENV_TOWN || mm2_attrib_is_outdoor(&rec)) {
        std::fprintf(stderr, "FAIL: screen 0 attrib env_type=0x%02X outdoor=%d\n",
                     mm2_attrib_env_type(&rec), mm2_attrib_is_outdoor(&rec));
        ++fails;
    }
    /* Spawn comes from the Goto-Town inn table @ DATA -$8990 (party-launch
     * path), NOT from the attrib entry-coord byte (that one is (7,5)). */
    uint8_t area = 0;
    uint8_t sx = 0;
    uint8_t sy = 0;
    char fk = 0;
    mm2_town_inn_spawn_lookup(1, &area, &sx, &sy, &fk);
    if (area != 0 || sx != 7 || sy != 3 || fk != 'N') {
        std::fprintf(stderr, "FAIL: Middlegate spawn = area %d (%d,%d) '%c', expected 0 (7,3) 'N'\n",
                     area, sx, sy, fk);
        ++fails;
    }

    if (fails == 0) {
        std::printf("OK: Middlegate (7,3,N) blit list matches (%d blits)\n", kExpectedCount);
        return 0;
    }
    return 1;
}
