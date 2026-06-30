// Regression: townt.32 overlay must not appear on map.dat field-3 doors.
// Gateway Temple door cell (6,7); approach from (6,6) or alley (7,7) facing N.
//
// Usage: view3d_torch_door_test <data_dir>

#include <cstdio>

#include "mm2/gfx/View3D.h"
#include "mm2/world/MapWorld.h"

namespace {

struct Cam {
    int x;
    int y;
    int facing;
    const char *label;
};

constexpr Cam kCases[] = {
    {6, 6, 0, "Gateway Temple approach (6,6) N"},
    {6, 7, 0, "On temple tile (6,7) N"},
    {7, 7, 0, "Alley (7,7) N"},
};

bool torchEligibleForDoor(const mm2::gfx::View3DBlit &b, int phase)
{
    mm2::gfx::View3DTorchBlit tb{};
    return mm2::gfx::view3dTorchBlitFor(b, phase, &tb);
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : ".";

    mm2::world::MapWorld world;
    if (!world.load(data_dir)) {
        std::fprintf(stderr, "FAIL: cannot load map.dat from %s\n", data_dir);
        return 1;
    }
    if (!world.enterScreen(0)) {
        std::fprintf(stderr, "FAIL: enterScreen(0)\n");
        return 1;
    }

    const mm2::gfx::View3DMapBuffers bufs = world.buildView3DMapBuffers();
    int fails = 0;

    for (const Cam &cam : kCases) {
        mm2::gfx::View3DCamera c{};
        c.x = cam.x;
        c.y = cam.y;
        c.facing = cam.facing;
        const mm2::gfx::View3DScene scene = mm2::gfx::buildView3DScene(bufs, c);

        for (int i = 0; i < scene.num_torch_blits; ++i) {
            const mm2::gfx::View3DBlit &b = scene.torch_blits[static_cast<size_t>(i)];
            if (b.code != mm2::gfx::kMapVisualWallTorch) {
                std::fprintf(stderr, "FAIL: %s torch_blits[%d] code=%u (expected 2)\n", cam.label, i,
                             static_cast<unsigned>(b.code));
                ++fails;
            }
        }

        for (int i = 0; i < scene.num_blits; ++i) {
            const mm2::gfx::View3DBlit &b = scene.blits[static_cast<size_t>(i)];
            if (b.code != static_cast<uint8_t>(mm2::gfx::WallField::Door)) {
                continue;
            }
            if (torchEligibleForDoor(b, 0)) {
                std::fprintf(stderr,
                             "FAIL: %s door blit[%d] lat=(%d,%d) frame=%d would get townt.32\n",
                             cam.label, i, b.latX, b.latRow, b.frame);
                ++fails;
            }
        }
    }

    /* Real torch wall at inn spawn (7,3) N must still qualify. */
    {
        mm2::gfx::View3DCamera c{};
        c.x = 7;
        c.y = 3;
        c.facing = 0;
        const mm2::gfx::View3DScene scene = mm2::gfx::buildView3DScene(bufs, c);
        if (scene.num_torch_blits < 1) {
            std::fprintf(stderr, "FAIL: (7,3) N expected >=1 torch_blit, got %d\n",
                         scene.num_torch_blits);
            ++fails;
        }
    }

    if (fails == 0) {
        std::printf("OK: no townt.32 overlay on Middlegate doors (%zu cases)\n", sizeof(kCases) / sizeof(kCases[0]));
        return 0;
    }
    return 1;
}
