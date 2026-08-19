#include "mm2/gfx/ViewportAnmOverlay.h"
#include "mm2/DataPath.h"
#include "mm2/gfx/GfxBackend.h"
#include "mm2/gfx/View3D.h"
#include "mm2/gfx/ViewportSignPlacement.h"
#include "mm2/CppStdCompat.h"
#include "mm2/platform/Platform.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2_pc_gfx_codec.h"
#if MM2_HOST_AMIGA
#include "mm2/gfx/AnmPlanarPool.h"
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#include "mm2_image32_codec.h"
#else
#include <cstdlib>
#include <cstring>
#include <cstdio>
#endif

namespace mm2::gfx {

namespace {

/* Flipbook fallback when no sequence_blob: 5 ticks/step matches common sign seq delays (e.g. 62.anm). */

constexpr int kFlipbookDelayTicks = 5;

/* Play every non-empty FF-delimited sequence block (doc 07). Frame indices in
 * the stream may exceed header frame_count (63.anm block2 → 6/7/8); wrap. */
int wrapAnmFrame(const mm2_anm_file *anm, int fr)
{
    if (!anm || anm->frame_count == 0) {
        return 0;
    }
    const int n = static_cast<int>(anm->frame_count);
    if (fr < 0) {
        return 0;
    }
    if (fr >= n) {
        return fr % n;
    }
    return fr;
}

bool seqBlockHasPairs(const mm2_anm_sequence_table *seq, int block)
{
    return seq && block >= 0 && block < seq->block_count &&
           mm2_anm_seq_block_pair_count(seq, block) > 0;
}

int nextSeqBlock(const mm2_anm_sequence_table *seq, int cur)
{
    if (!seq || seq->block_count <= 0) {
        return 0;
    }
    for (int n = 1; n <= seq->block_count; ++n) {
        const int b = ((cur < 0 ? -1 : cur) + n) % seq->block_count;
        if (seqBlockHasPairs(seq, b)) {
            return b;
        }
    }
    return cur >= 0 ? cur : 0;
}

int firstSeqBlock(const mm2_anm_sequence_table *seq)
{
    if (seqBlockHasPairs(seq, 0)) {
        return 0;
    }
    return nextSeqBlock(seq, -1);
}

int lastSeqBlock(const mm2_anm_sequence_table *seq)
{
    int last = firstSeqBlock(seq);
    if (!seq) {
        return last;
    }
    for (int b = 0; b < seq->block_count; ++b) {
        if (seqBlockHasPairs(seq, b)) {
            last = b;
        }
    }
    return last;
}

}  // namespace
void ViewportAnmOverlay::freePcState()
{
    mm2_pc_monster_picture_free(&pc_pic_);
#if MM2_HOST_AMIGA
    mm2_pc_gfx_planar_frame_free(&pc_frame_);
#endif
    pc_mode_ = false;
    pc_seq_index_ = -1;
}

void ViewportAnmOverlay::unload()
{
#if MM2_HOST_AMIGA
    const bool had_anm = anm_loaded_;
    hw_palette_live_ = false;
#endif
    freePcState();
#if MM2_HOST_AMIGA
    if (pool_handle_.valid()) {
        AnmPlanarPool::instance().release(pool_handle_);
        pool_handle_ = {};
    }
    hw_palette_live_ = false;
#else
    if (rgba_) {
        std::free(rgba_);
        rgba_ = nullptr;
    }
    if (anm_loaded_) {
        mm2_anm_free(&anm_);
    }
#endif
    anm_loaded_ = false;
    disk_index_ = -1;
    memset(&seq_, 0, sizeof(seq_));
    use_sequence_ = false;
    animating_ = false;
    composed_frame_ = 0;
    seq_block_ = 0;
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

const mm2_anm_file *ViewportAnmOverlay::anmFile() const
{
#if MM2_HOST_AMIGA
    return AnmPlanarPool::instance().anm(pool_handle_);
#else
    return anm_loaded_ ? &anm_ : nullptr;
#endif
}

bool ViewportAnmOverlay::setPcComposedFrame(int frame_idx)
{
    if (!pc_mode_ || frame_idx < 0) {
        return false;
    }

    const bool same = (frame_idx == composed_frame_);
#if MM2_HOST_AMIGA
    if (same && pc_frame_.bitmap) {
        return false;
    }

    mm2_pc_gfx_planar_frame_free(&pc_frame_);
    if (mm2_pc_monsters_composite_planar(&pc_pic_, frame_idx, gfx::gfxSettings().cga_palette, &pc_frame_) !=
        MM2_IMAGE32_OK) {
        return false;
    }

    w_ = static_cast<int>(pc_frame_.width);
    h_ = static_cast<int>(pc_frame_.height);
#else
    if (same && rgba_) {
        return false;
    }

    if (rgba_) {
        std::free(rgba_);
        rgba_ = nullptr;
    }

    rgba_ = static_cast<uint8_t *>(
        std::malloc(static_cast<size_t>(MM2_PC_COMBAT_CANVAS_W) * MM2_PC_COMBAT_CANVAS_H * 4u));
    if (!rgba_) {
        return false;
    }

    if (mm2_pc_monsters_composite_rgba(&pc_pic_, frame_idx, gfx::gfxSettings().cga_palette, rgba_) !=
        MM2_IMAGE32_OK) {
        return false;
    }

    /* Whole-canvas blit (96×96). Crop-to-bbox + re-anchor shifts innkeep/signs. */
    w_ = MM2_PC_COMBAT_CANVAS_W;
    h_ = MM2_PC_COMBAT_CANVAS_H;
#endif

    composed_frame_ = frame_idx;
    return !same;
}

bool ViewportAnmOverlay::ensurePcAtlas(const char *data_dir)
{
    const gfx::GfxBackend backend = gfx::gfxSettings().resolved;
    const char *primary = gfx::resolvePcMonstersFilename(backend);
    const char *secondary = (backend == gfx::GfxBackend::Cga) ? "MONSTERS.16" : "MONSTERS.4";
    char *path = mm2_path_scratch_b();
    auto tryLoad = [&](const char *dir, const char *filename) -> bool {
        if (!dir || !filename || !joinDataPath(path, MM2_PATH_SCRATCH_CAP, dir, filename)) {
            return false;
        }

        if (std::strcmp(path, pc_atlas_path_) == 0 && pc_atlas_.raw) {
            return true;
        }

        mm2_pc_monsters_atlas_free(&pc_atlas_);
        if (mm2_pc_monsters_atlas_load(path, &pc_atlas_) != MM2_IMAGE32_OK) {
            return false;
        }

        std::snprintf(pc_atlas_path_, sizeof(pc_atlas_path_), "%s", path);
        return true;
    };

    auto tryDir = [&](const char *dir) -> bool {
        return dir && (tryLoad(dir, primary) || tryLoad(dir, secondary));
    };

    const char *fallback = gfx::gfxSettings().pc_gfx_dir;
    return tryDir(data_dir) || (fallback[0] && tryDir(fallback));
}

bool ViewportAnmOverlay::loadFromPcPictureId(const char *data_dir, int picture_id, AnmLoopMode loop)
{
    unload();
    if (!data_dir || picture_id < 1) {
        return false;
    }

    if (!ensurePcAtlas(data_dir)) {
        return false;
    }

    if (mm2_pc_monsters_picture_load(&pc_atlas_, picture_id, &pc_pic_) != MM2_IMAGE32_OK) {
        return false;
    }

    loop_mode_ = loop;
    pc_seq_index_ = mm2_pc_monsters_primary_script_index(&pc_pic_);
    use_sequence_ = pc_seq_index_ >= 0 && pc_pic_.scripts[pc_seq_index_].step_count > 0;
    animating_ = false;
    int initial_frame = 0;
    if (use_sequence_) {
        animating_ = pc_pic_.scripts[pc_seq_index_].step_count > 1;
        initial_frame = mm2_pc_monsters_seq_frame_at(&pc_pic_, pc_seq_index_, 0);
        delay_remaining_ = mm2_pc_monsters_seq_delay_at(&pc_pic_, pc_seq_index_, 0);
        seq_step_ = 0;
    } else if (pc_pic_.frame_count > 1) {
        animating_ = true;
        initial_frame = 0;
        seq_step_ = 0;
        delay_remaining_ = kFlipbookDelayTicks;
    } else {
        initial_frame = 0;
        seq_step_ = 0;
        delay_remaining_ = 0;
    }

    pc_mode_ = true;
    composed_frame_ = -1;
    if (!setPcComposedFrame(initial_frame)) {
        unload();
        return false;
    }

    return true;
}

bool ViewportAnmOverlay::setComposedFrame(int frame_idx)
{
    const mm2_anm_file *anm = anmFile();
    if (!anm_loaded_ || !anm || frame_idx < 0 || frame_idx >= static_cast<int>(anm->frame_count)) {
        return false;
    }

    const bool same = (frame_idx == composed_frame_);

#if MM2_HOST_AMIGA
    AnmPlanarPool &pool = AnmPlanarPool::instance();
    /* After unload/resetPlayback, composed_frame_ is preset to the target index
     * while w_/h_ are still 0. Do not early-out until dimensions are known —
     * otherwise OP_0B portraits load as "loaded" but blitAt no-ops. */
    if (same && w_ > 0 && h_ > 0 && pool.cel(pool_handle_, frame_idx) != nullptr) {
        return false;
    }
    const mm2_anm_composite_planar *cel = pool.cel(pool_handle_, frame_idx);
    if (!cel) {
        return false;
    }
    w_ = cel->width;
    h_ = cel->height;
#else
    if (same && rgba_) {
        return false;
    }

    mm2_anm_composite_rgba comp{};
    if (!mm2_anm_composite_frame_rgba(anm, frame_idx, &comp)) {
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
    const mm2_anm_planar_cache *c = AnmPlanarPool::instance().cache(pool_handle_);
    if (c) {
        compose_min_x_ = c->canvas.min_x;
        compose_min_y_ = c->canvas.min_y;
    } else {
        compose_min_x_ = 0;
        compose_min_y_ = 0;
    }
#else
    const mm2_anm_file *anm = anmFile();
    mm2_anm_compose_canvas canvas{};
    if (anm && mm2_anm_compose_canvas_of(anm, &canvas)) {
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
    const mm2_anm_file *anm = anmFile();
    if (!anm) {
        return;
    }

    loop_mode_ = loop;
    mm2_anm_build_sequence_table(anm, &seq_);
    seq_block_ = firstSeqBlock(&seq_);
    use_sequence_ = seqBlockHasPairs(&seq_, seq_block_);
    animating_ = mm2_anm_has_animatable_frames(anm) != 0;
    seq_step_ = 0;
    delay_remaining_ = 0;

    if (use_sequence_) {
        composed_frame_ = wrapAnmFrame(anm, mm2_anm_seq_frame_at(&seq_, seq_block_, 0));
        delay_remaining_ = mm2_anm_seq_delay_at(&seq_, seq_block_, 0);
    } else {
        composed_frame_ = mm2_anm_default_overlay_composed_frame(anm);
        seq_step_ = composed_frame_;
        delay_remaining_ = kFlipbookDelayTicks;
    }

    setComposedFrame(composed_frame_);
}

#if MM2_HOST_AMIGA
void ViewportAnmOverlay::applyHardwarePalette() const
{
    const mm2_anm_file *anm = anmFile();
    if (!anm_loaded_ || !anm || use_host_palette_ || hw_palette_live_) {
        return;
    }
    mm2_amiga_apply_anm_palette(anm->palette_words);
    hw_palette_live_ = true;
}
#endif

bool ViewportAnmOverlay::loadFromPath(const char *path, AnmLoopMode loop, bool apply_hw_palette)
{
#if MM2_HOST_AMIGA
    (void)path;
    (void)loop;
    (void)apply_hw_palette;
    /* Amiga loads go through AnmPlanarPool via loadFromPool. */
    return false;
#else
    unload();
    if (!path) {
        return false;
    }

    if (mm2_anm_load_file(path, &anm_) != MM2_ANM_OK) {
        return false;
    }

    anm_loaded_ = true;
    resetPlayback(loop);
    syncComposeOriginFromCache();
    (void)apply_hw_palette;
    return loaded();
#endif
}

#if MM2_HOST_AMIGA
bool ViewportAnmOverlay::loadFromPool(const char *data_dir, int disk_index, AnmLoopMode loop,
                                      bool apply_hw_palette)
{
    if (!data_dir || disk_index < 0 || disk_index > 99) {
        return false;
    }

    /* Same disk already held - skip unload/acquire/recompose hitch. */
    if (anm_loaded_ && pool_handle_.valid() && disk_index_ == disk_index) {
        resetPlayback(loop);
        if (w_ <= 0 || h_ <= 0) {
            unload();
            return false;
        }
        if (apply_hw_palette) {
            hw_palette_live_ = false;
            applyHardwarePalette();
        }
        return true;
    }

    unload();

    pool_handle_ = AnmPlanarPool::instance().acquire(data_dir, disk_index, use_host_palette_);
    if (!pool_handle_.valid()) {
        return false;
    }

    anm_loaded_ = true;
    disk_index_ = disk_index;
    resetPlayback(loop);
    syncComposeOriginFromCache();
    if (w_ <= 0 || h_ <= 0) {
        unload();
        return false;
    }
    if (apply_hw_palette) {
        applyHardwarePalette();
    }
    return loaded();
}
#endif

bool ViewportAnmOverlay::loadFromTableId(const char *data_dir, int table_id, AnmLoopMode loop,
                                         bool apply_hw_palette)
{
    if (!data_dir || table_id <= 0 || table_id > 99) {
        return false;
    }

    /* sign_sprite_load @ 0x316E subq #1 ($31E8) then 0x9A30 addq #1 ($9A4C) — net: table id → NN.anm. */
    return loadFromDiskIndex(data_dir, table_id, loop, apply_hw_palette);
}

bool ViewportAnmOverlay::loadFromDiskIndex(const char *data_dir, int disk_index, AnmLoopMode loop,
                                           bool apply_hw_palette)
{
    gfx::resolveGfxBackend(data_dir);
    const gfx::GfxBackend backend = gfx::gfxSettings().resolved;
    if (backend == gfx::GfxBackend::Cga || backend == gfx::GfxBackend::Ega) {
        return loadFromPcPictureId(data_dir, disk_index, loop);
    }

    if (!data_dir) {
        return false;
    }

#if MM2_HOST_AMIGA
    return loadFromPool(data_dir, disk_index, loop, apply_hw_palette);
#else
    unload();
    char *path = mm2_path_scratch_a();
    if (!joinAnmPath(path, MM2_PATH_SCRATCH_CAP, data_dir, disk_index)) {
        return false;
    }
    disk_index_ = disk_index;
    return loadFromPath(path, loop, apply_hw_palette);
#endif
}

bool ViewportAnmOverlay::tick()
{
    if (!animating_ || (!anm_loaded_ && !pc_mode_)) {
        return false;
    }

    /* Retail: subq delay; bne still_waiting; advance. Hold exactly N vblanks. */
    if (delay_remaining_ > 0) {
        --delay_remaining_;
        if (delay_remaining_ > 0) {
            return false;
        }
    }

    const int prev_frame = composed_frame_;
    int next_frame = prev_frame;

    if (pc_mode_) {
        if (use_sequence_) {
            const int pair_count = pc_pic_.scripts[pc_seq_index_].step_count;
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

            next_frame = mm2_pc_monsters_seq_frame_at(&pc_pic_, pc_seq_index_, seq_step_);
            if (animating_) {
                delay_remaining_ = mm2_pc_monsters_seq_delay_at(&pc_pic_, pc_seq_index_, seq_step_);
            }
        } else {
            const int frame_count = pc_pic_.frame_count;
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
            next_frame = pc_pic_.frames[seq_step_].frame_index;
            if (animating_) {
                delay_remaining_ = kFlipbookDelayTicks;
            }
        }

        if (next_frame == prev_frame) {
            return false;
        }
        return setPcComposedFrame(next_frame);
    }

    if (use_sequence_) {
        int pair_count = mm2_anm_seq_block_pair_count(&seq_, seq_block_);
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
                /* Loop: every FF block in the stream, then wrap. */
                const int prev_block = seq_block_;
                seq_block_ = nextSeqBlock(&seq_, seq_block_);
                seq_step_ = 0;
            }
        }

        {
            const int raw = mm2_anm_seq_frame_at(&seq_, seq_block_, seq_step_);
            next_frame = wrapAnmFrame(anmFile(), raw);
        }
        if (animating_) {
            delay_remaining_ = mm2_anm_seq_delay_at(&seq_, seq_block_, seq_step_);
        }
    } else {
        const mm2_anm_file *anm = anmFile();
        const int frame_count = anm ? static_cast<int>(anm->frame_count) : 0;
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
            const int pair_count = mm2_anm_seq_block_pair_count(&seq_, seq_block_);
            if (pair_count > 0) {
                int warm_step = seq_step_ + 1;
                int warm_block = seq_block_;
                if (warm_step >= pair_count) {
                    if (loop_mode_ == AnmLoopMode::Loop) {
                        warm_block = nextSeqBlock(&seq_, seq_block_);
                        warm_step = 0;
                    } else {
                        warm_step = pair_count - 1;
                    }
                }
                warm_frame = wrapAnmFrame(anmFile(), mm2_anm_seq_frame_at(&seq_, warm_block, warm_step));
            }
        } else {
            const mm2_anm_file *warm_anm = anmFile();
            const int warm_count = warm_anm ? static_cast<int>(warm_anm->frame_count) : 0;
            if (warm_count > 1) {
                int warm_step = seq_step_ + 1;
                if (warm_step >= warm_count) {
                    warm_step = (loop_mode_ == AnmLoopMode::Loop) ? 0 : warm_count - 1;
                }
                warm_frame = warm_step;
            }
        }
        if (warm_frame != next_frame) {
            (void)AnmPlanarPool::instance().cel(pool_handle_, warm_frame);
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
    if (pc_mode_) {
        if (!pc_frame_.bitmap) {
            return;
        }

        applyHardwarePalette();
        mm2_image32_file wrap{};
        wrap.frame_count = 1;
        wrap.frames = const_cast<mm2_image32_frame *>(&pc_frame_);
        platform::blitImage32(&wrap, 0, dst_x, dst_y, 0);
        return;
    }

    const mm2_anm_composite_planar *cel = AnmPlanarPool::instance().cel(pool_handle_, composed_frame_);
    if (!cel || !cel->bitmap) {
        return;
    }

    /* Retail loads .anm pens 3-17 into hardware immediately before the blit
     * (combat / OP_0B / scripted sprites pass apply_hw_palette=false on load
     * so env backdrop can keep pens 3-17 while the hood is painted). */
    applyHardwarePalette();
    platform::blitAnmComposed(cel, dst_x, dst_y);
#else
    if (pc_mode_) {
        if (!rgba_) {
            return;
        }

        c.blitRgba(rgba_, w_, h_, dst_x, dst_y, true);
        return;
    }

    if (!rgba_) {
        return;
    }

    c.blitRgba(rgba_, w_, h_, dst_x, dst_y);
#endif
}

void ViewportAnmOverlay::blitCanvasAt(gfx::ScreenCompositor &c, int dst_x, int dst_y) const
{
    /* Scripted OP_0B place: canvas origin == blit origin (whole-canvas blit). */
    blitAt(c, dst_x, dst_y);
}

bool ViewportAnmOverlay::placeOverlayFrame(int frame_idx)
{
    /* 0x23C8C: tst.w pos; ble → simple path (cel 0). pos >= object+$4 → same. */
    animating_ = false;
    int frame = frame_idx;
    if (frame <= 0) {
        frame = 0;
    }
    if (pc_mode_) {
        if (frame >= pc_pic_.frame_count) {
            frame = 0;
        }
        return setPcComposedFrame(frame);
    }
    const mm2_anm_file *anm = anmFile();
    if (!anm_loaded_ || !anm) {
        return false;
    }
    if (frame >= static_cast<int>(anm->frame_count)) {
        frame = 0;
    }
    return setComposedFrame(frame);
}

void ViewportAnmOverlay::playLastSequenceHold()
{
    if (!anm_loaded_ && !pc_mode_) {
        return;
    }
    resetPlayback(AnmLoopMode::HoldLast);
    if (!use_sequence_) {
        const mm2_anm_file *anm = anmFile();
        if (anm && anm->frame_count > 1) {
            (void)placeOverlayFrame(mm2_anm_default_overlay_composed_frame(anm));
        }
        return;
    }
    seq_block_ = lastSeqBlock(&seq_);
    const int n = mm2_anm_seq_block_pair_count(&seq_, seq_block_);
    if (n <= 0) {
        return;
    }
    seq_step_ = n - 1;
    delay_remaining_ = 0;
    animating_ = false;
    const int frame = wrapAnmFrame(anmFile(), mm2_anm_seq_frame_at(&seq_, seq_block_, seq_step_));
    if (pc_mode_) {
        (void)setPcComposedFrame(frame);
    } else {
        (void)setComposedFrame(frame);
    }
}

void ViewportAnmOverlay::blitCentered(gfx::ScreenCompositor &c, int placement_index) const
{
    constexpr int kView3DViewportBottomY = 127;
    blitCenteredInViewport(c, placement_index, kView3DOriginX, kView3DSkyY, kView3DViewportW,
                           kView3DViewportBottomY - kView3DSkyY + 1);
}

void ViewportAnmOverlay::blitCenteredInViewport(gfx::ScreenCompositor &c, int placement_index, int slot_x,
                                                int slot_y, int slot_w, int slot_h,
                                                bool apply_content_offset) const
{
    if (w_ <= 0 || h_ <= 0) {
        return;
    }

    /* sign_sprite_place(pos, $40, $20) @ 0x3266 → mode $17 @ 0x23C8C. */
    int dst_x = 0;
    int dst_y = 0;
    resolveViewportSignPlacement(placement_index, w_, 0, 0, &dst_x, &dst_y);

    /* Service signs: retail coords, clip (96×96 hangs 8px past y=127).
     * Combat gallery: apply_content_offset=false, clamp into the slot. */
    if (apply_content_offset) {
        blitAt(c, dst_x, dst_y);
        return;
    }

    /* The retail simple-path coords (64, 40) were sized for the full explore
     * viewport (kViewW×kViewH); combat's round hood box is narrower/shorter,
     * so clamping those raw coords into the box pinned the sprite to one
     * edge instead of centering it (feet/side flush, gap opposite side).
     * Center the composed canvas in the box instead. */
    dst_x = slot_x + (slot_w - w_) / 2;
    dst_y = slot_y + (slot_h - h_) / 2;
    blitAt(c, dst_x, dst_y);
}

}  // namespace mm2::gfx

