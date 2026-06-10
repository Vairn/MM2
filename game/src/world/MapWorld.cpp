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
    /* 0x923E: A4-$79F2 * 64 -> copy record to A4-$561A. Here the record is
     * referenced in place; callers mirror the id into the gamestate. */
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

}  // namespace mm2::world
