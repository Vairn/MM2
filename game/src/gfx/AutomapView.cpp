#include "mm2/gfx/AutomapView.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/world/Cartography.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/Platform.h"
#endif

namespace mm2::gfx {

namespace {

constexpr int kTileW = 14;
constexpr int kTileH = 11;

void blitCartoFrame(ScreenCompositor &c, const mm2_image32_file &sheet, int frame, int dst_x, int dst_y)
{
    if (frame < 0 || frame >= sheet.frame_count) {
        return;
    }
    const mm2_image32_frame &f = sheet.frames[frame];
#if MM2_HOST_AMIGA
    (void)c;
    if (!f.bitmap) {
        return;
    }
    platform::blitImage32(&sheet, frame, dst_x, dst_y, 0);
#else
    if (!f.rgba) {
        return;
    }
    c.blitRgba(f.rgba, f.width, f.height, dst_x, dst_y);
#endif
}

}  // namespace

int automapPartyArrowFrame(char facing_key)
{
    switch (facing_key) {
        case 'N':
            return 0x20;
        case 'S':
            return 0x21;
        case 'E':
            return 0x22;
        case 'W':
            return 0x23;
        default:
            return 0x20;
    }
}

void renderAutomap(ScreenCompositor &c, const EnvAssets &env, const world::MapWorld &world,
                   const world::AutomapState &vis, const GameStateView &gs, const AutomapRenderParams &params)
{
    if (!world.loaded() || !gs.valid() || !env.automapReady()) {
        return;
    }

    using namespace play_layout;
    const int origin_x = params.origin_x;
    const int origin_y = params.origin_y;

    c.fillRect(kViewOriginX, kViewOriginY, kViewW, kViewH, 12, 12, 22, 255);

    const int screen = world.currentScreen();
    const bool outdoor = world::cartoUsesOutb(screen, world.isOutdoor());
    const mm2_image32_file &sheet = env.automap();
    const uint8_t *visual = world.visualPage();

    for (int cy = 0; cy < MM2_MAP_GRID_DIM; ++cy) {
        for (int cx = 0; cx < MM2_MAP_GRID_DIM; ++cx) {
            const int disk_y = (MM2_MAP_GRID_DIM - 1) - cy;
            const int idx = (disk_y << 4) | cx;
            const int px = origin_x + cx * kTileW;
            const int py = origin_y + cy * kTileH;

            if (!vis.isVisited(screen, cx, disk_y)) {
                c.fillRect(px, py, kTileW, kTileH, 20, 20, 30, 255);
                continue;
            }

            const uint8_t cell = visual[idx];
            const int frame = world::cartoFrame(screen, cell, outdoor);
            blitCartoFrame(c, sheet, frame, px, py);

            if (params.draw_edge_overlay && !outdoor) {
                const int edge_kind = cell & 0x03;
                if (edge_kind != 0) {
                    const int edge_frame = world::kCartoEdge[edge_kind - 1];
                    blitCartoFrame(c, sheet, edge_frame, px, py);
                }
            }
        }
    }

    if (params.draw_party_marker) {
        const int px = origin_x + static_cast<int>(gs.coordX()) * kTileW;
        const int py = origin_y + (MM2_MAP_GRID_DIM - 1 - static_cast<int>(gs.coordY())) * kTileH;
        c.drawBoxBorder(px, py, kTileW, kTileH, 255, 220, 0, 210);
        blitCartoFrame(c, sheet, automapPartyArrowFrame(gs.facingKey()), px, py);
    }

    c.drawText(origin_x, origin_y + MM2_MAP_GRID_DIM * kTileH + 2, "(ESC) close map", 180, 180, 180, 255);
}

}  // namespace mm2::gfx
