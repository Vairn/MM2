#include "mm2/gfx/AnmPlanarPool.h"

#if MM2_HOST_AMIGA

#include "mm2/CppStdCompat.h"
#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

namespace mm2::gfx {

namespace {

bool joinAnmPath(char *path, size_t cap, const char *data_dir, int disk_index)
{
    if (disk_index < 0 || disk_index > 99) {
        return false;
    }
    char name[16];
    std::snprintf(name, sizeof(name), "%02d.anm", disk_index);
    return joinDataPath(path, cap, data_dir, name);
}

}  // namespace

AnmPlanarPool &AnmPlanarPool::instance()
{
    static AnmPlanarPool pool;
    return pool;
}

void AnmPlanarPool::freeEntry(Entry &e)
{
    if (!e.live) {
        return;
    }
    mm2_anm_composite_cache_free(&e.cache);
    mm2_anm_free(&e.anm);
    e.disk_index = -1;
    e.map_host_palette = 0;
    e.refcount = 0;
    e.live = false;
}

AnmPlanarHandle AnmPlanarPool::acquire(const char *data_dir, int disk_index, bool map_host_palette)
{
    AnmPlanarHandle out{};
    if (!data_dir || disk_index < 0 || disk_index > 99) {
        return out;
    }
    const uint8_t map_flag = map_host_palette ? 1u : 0u;

    for (int i = 0; i < kMaxEntries; ++i) {
        Entry &e = entries_[i];
        if (e.live && e.disk_index == disk_index && e.map_host_palette == map_flag) {
            ++e.refcount;
            out.slot = i;
            return out;
        }
    }

    int free_slot = -1;
    for (int i = 0; i < kMaxEntries; ++i) {
        if (!entries_[i].live) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) {
        /* Evict an unreferenced entry to make room. */
        for (int i = 0; i < kMaxEntries; ++i) {
            if (entries_[i].live && entries_[i].refcount <= 0) {
                freeEntry(entries_[i]);
                free_slot = i;
                break;
            }
        }
    }
    if (free_slot < 0) {
        return out;
    }

    char *path = mm2_path_scratch_a();
    if (!joinAnmPath(path, MM2_PATH_SCRATCH_CAP, data_dir, disk_index)) {
        return out;
    }

    Entry &e = entries_[free_slot];
    std::memset(&e, 0, sizeof(e));
    if (mm2_anm_load_file(path, &e.anm) != MM2_ANM_OK) {
        return out;
    }
    if (!mm2_anm_composite_cache_init(&e.anm, map_flag, &e.cache)) {
        mm2_anm_free(&e.anm);
        return out;
    }
    e.disk_index = disk_index;
    e.map_host_palette = map_flag;
    e.refcount = 1;
    e.live = true;
    out.slot = free_slot;
    return out;
}

void AnmPlanarPool::release(AnmPlanarHandle h)
{
    if (!h.valid() || h.slot >= kMaxEntries) {
        return;
    }
    Entry &e = entries_[h.slot];
    if (!e.live || e.refcount <= 0) {
        return;
    }
    --e.refcount;
    /* Keep chip cels until purge() so the next fight can reuse them. */
}

void AnmPlanarPool::purge()
{
    for (int i = 0; i < kMaxEntries; ++i) {
        if (entries_[i].live && entries_[i].refcount <= 0) {
            freeEntry(entries_[i]);
        }
    }
}

void AnmPlanarPool::clear()
{
    for (int i = 0; i < kMaxEntries; ++i) {
        freeEntry(entries_[i]);
    }
}

const mm2_anm_file *AnmPlanarPool::anm(AnmPlanarHandle h) const
{
    if (!h.valid() || h.slot >= kMaxEntries) {
        return nullptr;
    }
    const Entry &e = entries_[h.slot];
    return e.live ? &e.anm : nullptr;
}

mm2_anm_planar_cache *AnmPlanarPool::cache(AnmPlanarHandle h)
{
    if (!h.valid() || h.slot >= kMaxEntries) {
        return nullptr;
    }
    Entry &e = entries_[h.slot];
    return e.live ? &e.cache : nullptr;
}

const mm2_anm_planar_cache *AnmPlanarPool::cache(AnmPlanarHandle h) const
{
    if (!h.valid() || h.slot >= kMaxEntries) {
        return nullptr;
    }
    const Entry &e = entries_[h.slot];
    return e.live ? &e.cache : nullptr;
}

const mm2_anm_composite_planar *AnmPlanarPool::cel(AnmPlanarHandle h, int frame_idx)
{
    mm2_anm_planar_cache *c = cache(h);
    if (!c) {
        return nullptr;
    }
    if (!mm2_anm_composite_cache_ensure(c, frame_idx)) {
        return nullptr;
    }
    return mm2_anm_composite_cache_at(c, frame_idx);
}

}  // namespace mm2::gfx

#endif /* MM2_HOST_AMIGA */
