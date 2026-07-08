#include "mm2/gfx/AutomapView.h"

#include "mm2/events/EventVmHelpers.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/world/Cartography.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/Platform.h"
#endif

namespace mm2::gfx {

namespace {

constexpr int kTileW = 14;
constexpr int kTileH = 11;

void blitCartoFrame(ScreenCompositor &c, const mm2_gfx_sheet &sheet, int frame, int dst_x, int dst_y)
{
    const mm2_image32_file &img = sheet.img;
    if (frame < 0 || frame >= img.frame_count) {
        return;
    }
    const mm2_image32_frame &f = img.frames[frame];
#if MM2_HOST_AMIGA
    (void)c;
    if (!f.bitmap) {
        return;
    }
    platform::blitImage32(&img, frame, dst_x, dst_y, 0);
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
    const mm2_gfx_sheet &sheet = env.automap();
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
                    /* @0x23BA: indoor edge overlay blits at X - 0xE. */
                    blitCartoFrame(c, sheet, edge_frame, px - kTileW, py);
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

void renderSpellEyeOverlay(ScreenCompositor &c, const EnvAssets &env, const world::MapWorld &world,
                           const GameStateView &gs)
{
    if (!world.loaded() || !gs.valid() || !env.automapReady()) {
        return;
    }
    if (events::eventVmSpellEyeTimer(gs.a4(), world.isOutdoor()) == 0) {
        return;
    }

    using namespace play_layout;

    const mm2_gfx_sheet &sheet = env.automap();
    const int px = static_cast<int>(gs.coordX());
    const int py = static_cast<int>(gs.coordY());

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            const int mx = px + col - 2;
            const int my = py + row - 2;
            const int dst_x = kSpellEyeOriginX + col * kTileW;
            const int dst_y = kSpellEyeBottomY - row * kTileH;

            world::MapWorld::SpellEyeSample sample{};
            if (!world.spellEyeSample(mx, my, &sample)) {
                c.fillRect(dst_x, dst_y, kTileW, kTileH, 12, 12, 22, 255);
                continue;
            }

            const bool cell_outdoor =
                world::cartoUsesOutb(sample.screen, world.screenIsOutdoor(sample.screen));
            const int frame = world::cartoFrame(sample.screen, sample.cell, cell_outdoor);
            blitCartoFrame(c, sheet, frame, dst_x, dst_y);

            if (col == 2 && row == 2) {
                blitCartoFrame(c, sheet, automapPartyArrowFrame(gs.facingKey()), dst_x, dst_y);
            }
        }
    }
}

}  // namespace mm2::gfx
