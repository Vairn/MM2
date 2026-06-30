// Renders a Middlegate automap snapshot (screen 0) with a small visited patch.
// Usage: automap_golden <data_dir> [out.png]

#include <cstdio>

#include "mm2/gfx/AutomapView.h"
#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/world/AutomapState.h"
#include "mm2/world/MapWorld.h"

#include "mm2_gamestate.h"
#include "mm2_image32_codec.h"

namespace {

void write_png_stub(const mm2::gfx::ScreenCompositor &c, const char *path)
{
    std::fprintf(stderr, "NOTE: PNG write not linked; compositor %dx%d ready at %s\n", c.width(),
                 c.height(), path);
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : "..";
    const char *out_path = (argc > 2) ? argv[2] : "automap_middlegate.png";

    mm2::world::MapWorld world;
    if (!world.load(data_dir) || !world.enterScreen(0)) {
        std::fprintf(stderr, "FAIL: load map screen 0 from %s\n", data_dir);
        return 1;
    }

    mm2::gfx::EnvAssets env;
    if (!env.loadEnv(data_dir, mm2::gfx::envKindFromAttrib(world.attrib())) || !env.automapReady()) {
        std::fprintf(stderr, "FAIL: load townb.32 from %s\n", data_dir);
        return 1;
    }

    uint8_t gs_image[MM2_A4_ANCHOR + 0x8000]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    gs.setCoordX(7);
    gs.setCoordY(3);
    gs.setFacingKey('N');

    mm2::world::AutomapState vis;
    for (int y = 2; y <= 4; ++y) {
        for (int x = 5; x <= 9; ++x) {
            vis.markVisited(0, x, y);
        }
    }
    vis.markVisited(0, gs.coordX(), gs.coordY());

    mm2::gfx::ScreenCompositor comp;
    comp.clear(0, 0, 0, 255);
    mm2::gfx::renderAutomap(comp, env, world, vis, gs);

    write_png_stub(comp, out_path);
    std::printf("OK: automap golden composited (%s)\n", out_path);
    return 0;
}
