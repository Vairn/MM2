#pragma once

#include "mm2/Config.h"
#include "mm2/gfx/ScreenCompositor.h"

#include "mm2_anm_codec.h"
#include "mm2_anm_preview.h"
#include "mm2_pc_monsters_codec.h"

#if MM2_HOST_AMIGA
#include "mm2/gfx/AnmPlanarPool.h"
#endif

namespace mm2::gfx {

/** Sequence end behaviour when playback reaches the last (frame,delay) pair. */
enum class AnmLoopMode : uint8_t {
    Loop = 0,     /* service signs / idle clips — retail loops during Y/N wait */
    HoldLast = 1, /* one-shot raise/hold (portcullis-style; untraced — opt-in later) */
};

/** NN.anm overlay: VBlank-tick delays, blit over the 3D hood. Amiga cels from AnmPlanarPool. */
class ViewportAnmOverlay {
public:
    /** OP_0B table id via 0x15756 → sign_sprite_load @ 0x316E / 0x9A30 (file = id). */
    bool loadFromTableId(const char *data_dir, int table_id, AnmLoopMode loop = AnmLoopMode::Loop,
                         bool apply_hw_palette = true);

    /** Direct disk index NN.anm (combat / scripted monster sprites). */
    bool loadFromDiskIndex(const char *data_dir, int disk_index, AnmLoopMode loop = AnmLoopMode::Loop,
                           bool apply_hw_palette = true);

    /** Alias for OP_0B ServiceSignResolver table ids. */
    bool loadFromId(const char *data_dir, int table_id, AnmLoopMode loop = AnmLoopMode::Loop,
                    bool apply_hw_palette = true)
    {
        return loadFromTableId(data_dir, table_id, loop, apply_hw_palette);
    }

#if MM2_HOST_AMIGA
    /** Push this overlay's pens 3-17 to hardware (once per active overlay, not per blit). */
    void applyHardwarePalette() const;
#endif

    void unload();

    /** Legacy opt-in nearest-env remap — retail uses reserved pens 3-17 instead. */
    void setUseHostPalette(bool v) { use_host_palette_ = v; }

    bool loaded() const { return anm_loaded_ || pc_mode_; }
    bool animating() const { return animating_; }
    int width() const { return w_; }
    int height() const { return h_; }
    int composedFrame() const { return composed_frame_; }
    int diskIndex() const { return disk_index_; }

    /** Advance one VBlank-tick of sequence delay; true when the visible cel changed. */
    bool tick();

    /** sign_sprite_place(frame, $40, $20) @ 0x3266 / 0x23C8C: show composed cel
     *  `frame`. frame<=0 or frame>=count → cel 0 (simple path @ 0x23E24).
     *  Stops sequence playback so the host can step cels (chest trap @ 0x1AA8E). */
    bool placeOverlayFrame(int frame_idx);

    /** After Open: hold the last cel of the last FF sequence block (open lid).
     *  Identify uses place(0); trap spring steps explicit cels then returns here. */
    void playLastSequenceHold();

    /** OP_0B / mode $17 @ 0x23C8C. Simple path @ 0x23E24 → dst (64, 40). */
    void blitCentered(gfx::ScreenCompositor &c, int placement_index = 0) const;
    /** apply_content_offset=true: retail place, no slot clamp (service signs).
     *  false: clamp into the slot (combat gallery). */
    void blitCenteredInViewport(gfx::ScreenCompositor &c, int placement_index, int slot_x, int slot_y,
                                int slot_w, int slot_h, bool apply_content_offset = true) const;

    /** Raw blit at (dst_x, dst_y). */
    void blitAt(gfx::ScreenCompositor &c, int dst_x, int dst_y) const;

    /** OP_0B place @ 0x3266 — whole-canvas origin (PC pictures stay 96×96). */
    void blitCanvasAt(gfx::ScreenCompositor &c, int dst_x, int dst_y) const;

private:
    bool loadFromPath(const char *path, AnmLoopMode loop, bool apply_hw_palette);
#if MM2_HOST_AMIGA
    bool loadFromPool(const char *data_dir, int disk_index, AnmLoopMode loop, bool apply_hw_palette);
#endif
    bool loadFromPcPictureId(const char *data_dir, int picture_id, AnmLoopMode loop);
    bool ensurePcAtlas(const char *data_dir);
    bool setPcComposedFrame(int frame_idx);
    void freePcState();
    bool setComposedFrame(int frame_idx);
    void resetPlayback(AnmLoopMode loop);
    void syncComposeOriginFromCache();
    const mm2_anm_file *anmFile() const;

#if MM2_HOST_AMIGA
    AnmPlanarHandle pool_handle_{};
    mm2_image32_frame pc_frame_{};
    mutable bool hw_palette_live_ = false;
#else
    uint8_t *rgba_ = nullptr;
#endif

    bool pc_mode_ = false;
    mm2_pc_monsters_atlas pc_atlas_{};
    mm2_pc_monster_picture pc_pic_{};
    char pc_atlas_path_[512] = {};
    int pc_seq_index_ = -1;

#if !MM2_HOST_AMIGA
    mm2_anm_file anm_{};
#endif
    mm2_anm_sequence_table seq_{};
    bool anm_loaded_ = false;
    bool use_sequence_ = false;
    bool animating_ = false;
    bool use_host_palette_ = false;
    AnmLoopMode loop_mode_ = AnmLoopMode::Loop;
    int disk_index_ = -1;
    int composed_frame_ = 0;
    int seq_block_ = 0; /* FF-delimited sequence index (63.anm has idle + activity) */
    int seq_step_ = 0;
    int delay_remaining_ = 0;
    int w_ = 0;
    int h_ = 0;
    /* mm2_anm_compose_canvas_of origin — retail blits the full cel at (0x20,0x48). */
    int compose_min_x_ = 0;
    int compose_min_y_ = 0;
};

}  // namespace mm2::gfx
