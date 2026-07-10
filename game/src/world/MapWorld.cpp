#include "mm2/world/MapWorld.h"

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

namespace mm2::world {

bool MapWorld::load(const char *data_dir)
{
    loaded_ = false;

    char *const path = mm2_path_scratch_a();
    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, "map.dat")) {
        return false;
    }
    if (mm2_map_load_file(path, &map_) != MM2_MAP_OK) {
        return false;
    }

    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, "attrib.dat")) {
        return false;
    }
    if (mm2_attrib_load_file(path, &attrib_) != MM2_ATTRIB_OK) {
        return false;
    }

    loaded_ = true;
    return true;
}

bool MapWorld::enterScreen(int screen_id)
{
    if (!loaded_ || screen_id < 0 || screen_id >= MM2_MAP_SCREEN_COUNT) {
        return false;
    }
    /* 0x923E: A4-$79F2 * 64 -> copy record to A4-$561A. Callers that own a
     * GameStateView must also call gameplay::materializeScreenAttrib so
     * -$560C (entry_coord) / -$5600 (flags) are live in the GS image. */
    screen_ = screen_id;
    return true;
}

int MapWorld::neighborScreen(int slot) const
{
    const int n = mm2_attrib_neighbor(&attrib(), slot);
    if (n < 0 || n >= MM2_MAP_SCREEN_COUNT) {
        return -1;
    }
    return n;
}

bool MapWorld::screenIsOutdoor(int screen_id) const
{
    if (!loaded_ || screen_id < 0 || screen_id >= MM2_MAP_SCREEN_COUNT) {
        return false;
    }
    return mm2_attrib_is_outdoor(&attrib_.records[screen_id]) != 0;
}

gfx::View3DMapBuffers MapWorld::buildView3DMapBuffers() const
{
    gfx::View3DMapBuffers bufs{};
    if (!loaded_) {
        return bufs;
    }

    ::memcpy(bufs.center.begin(), visualPage(), gfx::kMapPageSize);

    auto loadNeighbor = [&](int slot, std::array<uint8_t, gfx::kMapPageSize> &page) {
        page.fill(0xFF);
        const int n = neighborScreen(slot);
        if (n >= 0 && n != screen_) {
            ::memcpy(page.begin(), map_.screens[n].visual, gfx::kMapPageSize);
        }
    };
    loadNeighbor(0, bufs.north);
    loadNeighbor(1, bufs.east);
    loadNeighbor(2, bufs.south);
    loadNeighbor(3, bufs.west);
    return bufs;
}

bool MapWorld::spellEyeSample(int mx, int my, SpellEyeSample *out) const
{
    if (!loaded_ || !out) {
        return false;
    }

    out->screen = screen_;
    if (!isOutdoor()) {
        /* 0x1D9A indoor: both axes must be in 0..15 or sample returns 0. */
        if (mx < 0 || my < 0 || mx >= MM2_MAP_GRID_DIM || my >= MM2_MAP_GRID_DIM) {
            return false;
        }
        out->cell = visualPage()[(my << 4) | mx];
        return true;
    }

    /* Outdoor: neighbour page pick @ 0x1DEA (E/W then S/N, same order as ASM). */
    const uint8_t *page = visualPage();
    int lx = mx;
    int ly = my;
    const unsigned xb = static_cast<unsigned>(mx) & 0xFFu;
    const unsigned yb = static_cast<unsigned>(my) & 0xFFu;

    if (xb > 0x0Fu && xb < 0x13u) {
        const int n = neighborScreen(1); /* E → A4-$51BA */
        if (n < 0) {
            return false;
        }
        out->screen = n;
        page = map_.screens[n].visual;
        lx = static_cast<int>(xb & 0x0Fu);
    } else if (xb >= 0xFCu) {
        const int n = neighborScreen(3); /* W → A4-$50BA */
        if (n < 0) {
            return false;
        }
        out->screen = n;
        page = map_.screens[n].visual;
        lx = static_cast<int>(xb & 0x0Fu);
    }

    if (yb > 0x0Fu && yb < 0x13u) {
        const int n = neighborScreen(2); /* S → A4-$53BA */
        if (n < 0) {
            return false;
        }
        out->screen = n;
        page = map_.screens[n].visual;
        ly = static_cast<int>(yb & 0x0Fu);
    } else if (yb >= 0xFCu) {
        const int n = neighborScreen(0); /* N → A4-$52BA */
        if (n < 0) {
            return false;
        }
        out->screen = n;
        page = map_.screens[n].visual;
        ly = static_cast<int>(yb & 0x0Fu);
    }

    if (lx < 0 || ly < 0 || lx >= MM2_MAP_GRID_DIM || ly >= MM2_MAP_GRID_DIM) {
        return false;
    }
    out->cell = page[(ly << 4) | lx];
    return true;
}

}  // namespace mm2::world
