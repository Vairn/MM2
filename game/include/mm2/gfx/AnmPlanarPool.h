#pragma once

#include "mm2/Config.h"

#if MM2_HOST_AMIGA

#include "mm2_anm_codec.h"
#include "mm2_anm_preview.h"

namespace mm2::gfx {

/** Opaque handle into the process-wide NN.anm planar pool (Amiga chip cels). */
struct AnmPlanarHandle {
    int slot = -1;
    bool valid() const { return slot >= 0; }
};

/**
 * Refcounted NN.anm load + lazy planar cel cache, shared across combat / OP_0B /
 * scripted overlays so the same disk index is composed once.
 */
class AnmPlanarPool {
public:
    static constexpr int kMaxEntries = 16;

    static AnmPlanarPool &instance();

    /** Load (or bump ref) ``NN.anm`` for disk_index. Invalid handle on failure. */
    AnmPlanarHandle acquire(const char *data_dir, int disk_index, bool map_host_palette);

    void release(AnmPlanarHandle h);

    /** Free all entries with refcount 0 (call after combat / map transition). */
    void purge();

    /** Drop every entry (only when no live handles — used at shutdown). */
    void clear();

    const mm2_anm_file *anm(AnmPlanarHandle h) const;
    mm2_anm_planar_cache *cache(AnmPlanarHandle h);
    const mm2_anm_planar_cache *cache(AnmPlanarHandle h) const;

    /** Ensure frame is composed; returns cel or nullptr. */
    const mm2_anm_composite_planar *cel(AnmPlanarHandle h, int frame_idx);

private:
    struct Entry {
        int disk_index = -1;
        uint8_t map_host_palette = 0;
        int refcount = 0;
        mm2_anm_file anm{};
        mm2_anm_planar_cache cache{};
        bool live = false;
    };

    AnmPlanarPool() = default;
    void freeEntry(Entry &e);

    Entry entries_[kMaxEntries]{};
};

}  // namespace mm2::gfx

#endif /* MM2_HOST_AMIGA */
