#pragma once
// World data owner: map.dat (60 screens x 2 pages) + attrib.dat (60 records).
//
// Models the original screen-enter path:
//   - attrib screen-enter @ 0x923E: current screen id (A4-$79F2) * 64 ->
//     copies the 64-byte attrib record into A4-$561A; env / view-mode are
//     materialized from that record.
//   - map pages: the flat map.dat blob lives at A4-$EEF4; the hood sampler
//     (map_row_sampler @ 0x190C) reads the centre visual page (A4-$55BA),
//     the collision page (A4-$54BA) and the four neighbour visual buffers,
//     selected via the attrib neighbour bytes 0x05..0x08.
//   - screen change LAB_256E @ 0x256E: crossing an edge picks the neighbour
//     screen id from the current attrib record and re-enters.
//
// Neighbour slot -> compass mapping (slot0=N, 1=E, 2=S, 3=W) matches the
// editor View3D buildMapBuffers(); doc 12 confirms slots (0,2)/(1,3) are
// opposite pairs.

#include "mm2/CppStdCompat.h"
#include "mm2/gfx/View3D.h"

#include "mm2_attrib_codec.h"
#include "mm2_map_codec.h"

namespace mm2::world {

class MapWorld {
public:
    /* Load map.dat + attrib.dat from the data dir. */
    bool load(const char *data_dir);

    bool loaded() const { return loaded_; }

    /* Enter a screen: latch id + materialize its attrib record (0x923E). */
    bool enterScreen(int screen_id);

    int currentScreen() const { return screen_; }
    const Mm2MapFile &mapFile() const { return map_; }
    const Mm2AttribFile &attribFile() const { return attrib_; }
    const Mm2AttribRecord &attrib() const { return attrib_.records[screen_]; }

    const uint8_t *visualPage() const { return map_.screens[screen_].visual; }
    const uint8_t *collisionPage() const { return map_.screens[screen_].collision; }

    /* Collision page for the active screen (page 1 @ map.dat +0x100). */
    const Mm2MapScreen &collisionScreen() const { return map_.screens[screen_]; }

    static uint8_t collisionCellAt(const Mm2MapScreen *screen, int x, int y)
    {
        if (!screen || x < 0 || y < 0 || x >= MM2_MAP_GRID_DIM || y >= MM2_MAP_GRID_DIM) {
            return 0;
        }
        return mm2_map_collision_at(screen, x, y);
    }

    uint8_t collisionAt(int x, int y) const { return collisionCellAt(&collisionScreen(), x, y); }

    /* Neighbour screen id for compass slot 0=N 1=E 2=S 3=W (attrib 0x05..0x08). */
    int neighborScreen(int slot) const;

    /* Env / view-mode info from the materialized attrib record. */
    uint8_t envType() const { return mm2_attrib_env_type(&attrib()); }
    uint8_t mapCategory() const { return mm2_attrib_map_category(&attrib()); }
    bool isOutdoor() const { return mm2_attrib_is_outdoor(&attrib()) != 0; }

    /* Roof bit for the 16x16 tile (attrib 0x20..0x3F). */
    bool roofBitAt(int x, int y) const
    {
        return mm2_attrib_roof_bit(&attrib(), (y << 4) | x) != 0;
    }

    /* Neighbour-aware 3D hood buffers (editor View3D buildMapBuffers()):
     * centre visual page + four neighbour visual pages; out-of-world
     * neighbours are filled with 0xFF (solid). */
    gfx::View3DMapBuffers buildView3DMapBuffers() const;

private:
    bool loaded_ = false;
    int screen_ = 0;
    Mm2MapFile map_{};
    Mm2AttribFile attrib_{};
};

}  // namespace mm2::world
