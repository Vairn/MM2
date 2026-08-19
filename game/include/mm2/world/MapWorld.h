#pragma once
// map.dat (60 screens × 2 pages) + attrib.dat (60 records).
// attrib screen-enter @ 0x923E: A4-$79F2 * 64 → copy record to A4-$561A.
// map blob A4-$EEF4; hood sampler @ 0x190C: visual A4-$55BA, collision A4-$54BA,
// neighbour visual pages from attrib 0x05..0x08.
// screen change LAB_256E @ 0x256E.
// Neighbour slots 0=N 1=E 2=S 3=W (doc 12: (0,2)/(1,3) opposite).

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
    Mm2MapFile &mapFileMut() { return map_; }
    const Mm2AttribFile &attribFile() const { return attrib_; }
    Mm2AttribFile &attribFileMut() { return attrib_; }
    const Mm2AttribRecord &attrib() const { return attrib_.records[screen_]; }
    Mm2AttribRecord &attribMut() { return attrib_.records[screen_]; }

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
    bool screenIsOutdoor(int screen_id) const;

    /* Roof bit for the 16x16 tile (attrib 0x20..0x3F). */
    bool roofBitAt(int x, int y) const
    {
        return mm2_attrib_roof_bit(&attrib(), (y << 4) | x) != 0;
    }

    /* Neighbour-aware 3D hood buffers (editor View3D buildMapBuffers()):
     * centre visual page + four neighbour visual pages; out-of-world
     * neighbours are filled with 0xFF (solid). */
    gfx::View3DMapBuffers buildView3DMapBuffers() const;

    struct SpellEyeSample {
        uint8_t cell = 0;
        int screen = -1;
    };

    /* spell_eye_cell_sample @ 0x1D9A — map byte for Eagle/Wizard Eye 5×5 grid.
     * Outdoor (-$79E2): peeks into neighbour visual pages (attrib 0x05..0x08),
     * same N/E/S/W as the 3D hood — not a torus wrap of the current 16×16.
     * Indoor: centre page only; out-of-range returns false. */
    bool spellEyeSample(int mx, int my, SpellEyeSample *out) const;

private:
    bool loaded_ = false;
    int screen_ = 0;
    Mm2MapFile map_{};
    Mm2AttribFile attrib_{};
};

}  // namespace mm2::world
