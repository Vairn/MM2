#include "mm2/gfx/ViewportAnmOverlay.h"

#include "mm2/DataPath.h"
#include "mm2/gfx/View3D.h"
#include "mm2/runtime/PathScratch.h"

#include <cstdio>

namespace mm2::gfx {

void ViewportAnmOverlay::unload()
{
    if (rgba_) {
        std::free(rgba_);
        rgba_ = nullptr;
    }
    w_ = 0;
    h_ = 0;
}

bool ViewportAnmOverlay::loadFromId(const char *data_dir, int anm_id)
{
    unload();
    if (!data_dir || anm_id <= 0 || anm_id > 99) {
        return false;
    }

    char *path = mm2_path_scratch_a();
    char name[16];
    std::snprintf(name, sizeof(name), "%02d.anm", anm_id);
    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, name)) {
        return false;
    }

    mm2_anm_file anm{};
    if (mm2_anm_load_file(path, &anm) != MM2_ANM_OK) {
        return false;
    }

    mm2_anm_composite_rgba comp{};
    if (!mm2_anm_composite_frame0_rgba(&anm, &comp)) {
        mm2_anm_free(&anm);
        return false;
    }
    mm2_anm_free(&anm);

    rgba_ = comp.rgba;
    w_ = comp.width;
    h_ = comp.height;
    return true;
}

void ViewportAnmOverlay::blitCentered(gfx::ScreenCompositor &c, int placement_index) const
{
    if (!rgba_ || w_ <= 0 || h_ <= 0) {
        return;
    }

    /* Mode $17 @ 0x23C8C: viewport (8,8)-(215,127); retail signs sit upper-center.
     * Placement table A4-$7538 maps slot id -> index into A4-$56E coord quads (not in
     * ghidra extract). Partial: nudge Y by placement_index * 2 px. */
    const int dst_x = kView3DOriginX + (kView3DViewportW - w_) / 2;
    int dst_y = kView3DSkyY + 20;
    if (placement_index > 0 && placement_index < 24) {
        dst_y += placement_index * 2;
    }
    c.blitRgba(rgba_, w_, h_, dst_x, dst_y);
}

}  // namespace mm2::gfx
