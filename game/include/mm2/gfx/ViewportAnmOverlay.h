#pragma once

#include "mm2/gfx/ScreenCompositor.h"

#include "mm2_anm_codec.h"
#include "mm2_anm_preview.h"

namespace mm2::gfx {

/** Load NN.anm and blit composite frame 0 over the 3D viewport (mode $17 partial). */
class ViewportAnmOverlay {
public:
    bool loadFromId(const char *data_dir, int anm_id);
    void unload();

    bool loaded() const { return rgba_ != nullptr; }
    int width() const { return w_; }
    int height() const { return h_; }

    /** Centered upper viewport; placement index from OP_0B arg2 adjusts Y (partial 0x23C8C). */
    void blitCentered(gfx::ScreenCompositor &c, int placement_index = 0) const;

private:
    uint8_t *rgba_ = nullptr;
    int w_ = 0;
    int h_ = 0;
};

}  // namespace mm2::gfx
