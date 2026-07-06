#include "mm2/gfx/ViewportAnmOverlay.h"



#include "mm2/DataPath.h"

#include "mm2/gfx/View3D.h"

#include "mm2/CppStdCompat.h"

#include "mm2/platform/Platform.h"

#include "mm2/runtime/PathScratch.h"



#if MM2_HOST_AMIGA

#include "mm2/platform/amiga/Mm2AmigaPlanar.h"

#include "mm2/platform/Platform.h"

#else

#include <cstdlib>

#endif



namespace mm2::gfx {



namespace {



/* Flipbook fallback when no sequence_blob: 5 ticks/step matches common sign seq delays (e.g. 62.anm). */

constexpr int kFlipbookDelayTicks = 5;



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



void ViewportAnmOverlay::unload()

{

#if MM2_HOST_AMIGA

    const bool had_anm = anm_loaded_;

    hw_palette_live_ = false;

#endif

#if MM2_HOST_AMIGA

    mm2_anm_composite_cache_free(&cache_);
    hw_palette_live_ = false;

#else

    if (rgba_) {

        std::free(rgba_);

        rgba_ = nullptr;

    }

#endif

    if (anm_loaded_) {

        mm2_anm_free(&anm_);

        anm_loaded_ = false;

    }

    memset(&seq_, 0, sizeof(seq_));

    use_sequence_ = false;

    animating_ = false;

    composed_frame_ = 0;

    seq_step_ = 0;

    delay_remaining_ = 0;

    w_ = 0;

    h_ = 0;

    compose_min_x_ = 0;

    compose_min_y_ = 0;

#if MM2_HOST_AMIGA

    if (had_anm) {

        mm2_amiga_restore_play_world_palette();

    }

#endif

}



bool ViewportAnmOverlay::setComposedFrame(int frame_idx)

{

    if (!anm_loaded_ || frame_idx < 0 || frame_idx >= static_cast<int>(anm_.frame_count)) {

        return false;

    }

    const bool same = (frame_idx == composed_frame_);



#if MM2_HOST_AMIGA

    /* Lazy per-cel chip cache — each frame composed once, then reused. */
    if (same && mm2_anm_composite_cache_at(&cache_, frame_idx) != NULL) {
        return false;
    }
    if (!mm2_anm_composite_cache_ensure(&cache_, frame_idx)) {
        return false;
    }
    {
        const mm2_anm_composite_planar *cel = mm2_anm_composite_cache_at(&cache_, frame_idx);
        if (!cel) {
            return false;
        }
        w_ = cel->width;
        h_ = cel->height;
    }

#else

    if (same && rgba_) {

        return false;

    }

    mm2_anm_composite_rgba comp{};

    if (!mm2_anm_composite_frame_rgba(&anm_, frame_idx, &comp)) {

        return false;

    }

    if (rgba_) {

        std::free(rgba_);

    }

    rgba_ = comp.rgba;

    w_ = comp.width;

    h_ = comp.height;

#endif

    composed_frame_ = frame_idx;

    return !same;

}



void ViewportAnmOverlay::syncComposeOriginFromCache()

{

#if MM2_HOST_AMIGA

    compose_min_x_ = cache_.canvas.min_x;

    compose_min_y_ = cache_.canvas.min_y;

#else

    mm2_anm_compose_canvas canvas{};

    if (mm2_anm_compose_canvas_of(&anm_, &canvas)) {

        compose_min_x_ = canvas.min_x;

        compose_min_y_ = canvas.min_y;

    } else {

        compose_min_x_ = 0;

        compose_min_y_ = 0;

    }

#endif

}



void ViewportAnmOverlay::resetPlayback(AnmLoopMode loop)

{

    loop_mode_ = loop;

    mm2_anm_build_sequence_table(&anm_, &seq_);

    use_sequence_ = mm2_anm_seq_block_pair_count(&seq_, 0) > 0;

    animating_ = mm2_anm_has_animatable_frames(&anm_) != 0;

    seq_step_ = 0;

    delay_remaining_ = 0;



    if (use_sequence_) {

        composed_frame_ = mm2_anm_seq_frame_at(&seq_, 0, 0);

        delay_remaining_ = mm2_anm_seq_delay_at(&seq_, 0, 0);

    } else {

        composed_frame_ = mm2_anm_default_overlay_composed_frame(&anm_);

        seq_step_ = composed_frame_;

        delay_remaining_ = kFlipbookDelayTicks;

    }

    setComposedFrame(composed_frame_);

}



#if MM2_HOST_AMIGA
void ViewportAnmOverlay::applyHardwarePalette() const
{
    if (!anm_loaded_ || use_host_palette_ || hw_palette_live_) {
        return;
    }
    mm2_amiga_apply_anm_palette(anm_.palette_words);
    hw_palette_live_ = true;
}
#endif

bool ViewportAnmOverlay::loadFromPath(const char *path, AnmLoopMode loop, bool apply_hw_palette)

{

    unload();

    if (!path) {

        return false;

    }



    if (mm2_anm_load_file(path, &anm_) != MM2_ANM_OK) {

        return false;

    }

    anm_loaded_ = true;

#if MM2_HOST_AMIGA

    /* Lazy chip cache: metadata at load, compose the active cel on demand. */
    if (!mm2_anm_composite_cache_init(&anm_, use_host_palette_ ? 1 : 0, &cache_)) {

        unload();

        return false;

    }

#endif

    resetPlayback(loop);

#if MM2_HOST_AMIGA
    syncComposeOriginFromCache();
    if (apply_hw_palette) {
        applyHardwarePalette();
    }
#endif

    return loaded();

}



bool ViewportAnmOverlay::loadFromTableId(const char *data_dir, int table_id, AnmLoopMode loop,
                                         bool apply_hw_palette)

{

    unload();

    if (!data_dir || table_id <= 0 || table_id > 99) {

        return false;

    }



    /* sign_sprite_load @ 0x316E subq #1 ($31E8) then 0x9A30 addq #1 ($9A4C) — net: table id → NN.anm. */

    return loadFromDiskIndex(data_dir, table_id, loop, apply_hw_palette);

}



bool ViewportAnmOverlay::loadFromDiskIndex(const char *data_dir, int disk_index, AnmLoopMode loop,
                                           bool apply_hw_palette)

{

    unload();

    if (!data_dir) {

        return false;

    }



    char *path = mm2_path_scratch_a();

    if (!joinAnmPath(path, MM2_PATH_SCRATCH_CAP, data_dir, disk_index)) {

        return false;

    }

    return loadFromPath(path, loop, apply_hw_palette);

}



bool ViewportAnmOverlay::tick()

{

    if (!animating_ || !anm_loaded_) {

        return false;

    }



    if (delay_remaining_ > 0) {

        --delay_remaining_;

        return false;

    }



    const int prev_frame = composed_frame_;
    int next_frame = prev_frame;

    if (use_sequence_) {
        const int pair_count = mm2_anm_seq_block_pair_count(&seq_, 0);
        if (pair_count <= 0) {
            animating_ = false;
            return false;
        }

        ++seq_step_;
        if (seq_step_ >= pair_count) {
            if (loop_mode_ == AnmLoopMode::HoldLast) {
                seq_step_ = pair_count - 1;
                animating_ = false;
            } else {
                seq_step_ = 0;
            }
        }

        next_frame = mm2_anm_seq_frame_at(&seq_, 0, seq_step_);
        if (animating_) {
            delay_remaining_ = mm2_anm_seq_delay_at(&seq_, 0, seq_step_);
        }
    } else {
        const int frame_count = static_cast<int>(anm_.frame_count);
        if (frame_count <= 1) {
            animating_ = false;
            return false;
        }

        ++seq_step_;
        if (seq_step_ >= frame_count) {
            if (loop_mode_ == AnmLoopMode::HoldLast) {
                seq_step_ = frame_count - 1;
                animating_ = false;
            } else {
                seq_step_ = 0;
            }
        }
        next_frame = seq_step_;
        if (animating_) {
            delay_remaining_ = kFlipbookDelayTicks;
        }
    }

    if (next_frame == prev_frame) {
        return false;
    }
    const bool changed = setComposedFrame(next_frame);

#if MM2_HOST_AMIGA
    /* Compose the upcoming cel during the inter-frame delay (retail vblank ticks). */
    if (animating_) {
        int warm_frame = next_frame;
        if (use_sequence_) {
            const int pair_count = mm2_anm_seq_block_pair_count(&seq_, 0);
            if (pair_count > 0) {
                int warm_step = seq_step_ + 1;
                if (warm_step >= pair_count) {
                    warm_step = (loop_mode_ == AnmLoopMode::Loop) ? 0 : pair_count - 1;
                }
                warm_frame = mm2_anm_seq_frame_at(&seq_, 0, warm_step);
            }
        } else {
            const int frame_count = static_cast<int>(anm_.frame_count);
            if (frame_count > 1) {
                int warm_step = seq_step_ + 1;
                if (warm_step >= frame_count) {
                    warm_step = (loop_mode_ == AnmLoopMode::Loop) ? 0 : frame_count - 1;
                }
                warm_frame = warm_step;
            }
        }
        if (warm_frame != next_frame) {
            (void)mm2_anm_composite_cache_ensure(&cache_, warm_frame);
        }
    }
#endif

    return changed;
}



void ViewportAnmOverlay::blitAt(gfx::ScreenCompositor &c, int dst_x, int dst_y) const

{

    if (w_ <= 0 || h_ <= 0) {

        return;

    }

#if MM2_HOST_AMIGA

    (void)c;

    const mm2_anm_composite_planar *cel = mm2_anm_composite_cache_at(&cache_, composed_frame_);
    if (!cel || !cel->bitmap) {
        return;
    }

    /* Retail loads .anm pens 3-17 into hardware immediately before the blit
     * (combat / OP_0B / scripted sprites pass apply_hw_palette=false on load
     * so env backdrop can keep pens 3-17 while the hood is painted). */
    applyHardwarePalette();

    platform::blitAnmComposed(cel, dst_x, dst_y);

#else

    if (!rgba_) {

        return;

    }

    c.blitRgba(rgba_, w_, h_, dst_x, dst_y);

#endif

}



void ViewportAnmOverlay::blitCentered(gfx::ScreenCompositor &c, int placement_index) const

{

    if (w_ <= 0 || h_ <= 0) {

        return;

    }



    /* sign_sprite_place(pos, $40, $20) @ 0x3266 → mode $17 @ 0x23C8C.
     * Simple path @ 0x23E24 when pos <= 0 or pos >= width:
     *   dst_x = arg2 = $40 (64), dst_y = arg3 + 8 = $28 (40).
     * Wide-sprite table path (A4-$56E, 0 < pos < width) is GAP. */

    constexpr int kOp0BSimpleDstX = 0x40;
    constexpr int kOp0BSimpleDstY = 0x20 + 8;
    constexpr int kView3DViewportBottomY = 127;

    const int slot_x = kView3DOriginX;
    const int slot_y = kView3DSkyY;
    const int slot_w = kView3DViewportW;
    const int slot_h = kView3DViewportBottomY - kView3DSkyY + 1;

    int dst_x = kOp0BSimpleDstX;
    int dst_y = kOp0BSimpleDstY;
    (void)placement_index;



    if (dst_x < slot_x) {

        dst_x = slot_x;

    }

    if (dst_x + w_ > slot_x + slot_w) {

        dst_x = slot_x + slot_w - w_;

    }

    if (dst_y < slot_y) {

        dst_y = slot_y;

    }

    if (dst_y + h_ > slot_y + slot_h) {

        dst_y = slot_y + slot_h - h_;

    }

    blitAt(c, dst_x, dst_y);

}



}  // namespace mm2::gfx

