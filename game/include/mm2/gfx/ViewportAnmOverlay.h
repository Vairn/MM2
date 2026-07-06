#pragma once



#include "mm2/Config.h"

#include "mm2/gfx/ScreenCompositor.h"



#include "mm2_anm_codec.h"

#include "mm2_anm_preview.h"



namespace mm2::gfx {



/** Sequence end behaviour when playback reaches the last (frame,delay) pair. */

enum class AnmLoopMode : uint8_t {

    Loop = 0,      /* service signs / idle clips — retail loops during Y/N wait (untraced; editor default) */

    HoldLast = 1,  /* one-shot raise/hold (portcullis-style; untraced — opt-in later) */

};



/**

 * Load NN.anm, advance composed frames each game tick, blit over the 3D viewport.

 * Sequence delays are in retail VBlank ticks (~60 Hz NTSC / 50 Hz PAL); desktop

 * platform::nowTicks() approximates 60 Hz (see Platform.h).

 */

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



    bool loaded() const { return anm_loaded_; }

    bool animating() const { return animating_; }

    int width() const { return w_; }

    int height() const { return h_; }

    int composedFrame() const { return composed_frame_; }



    /** Advance one game tick; returns true when the visible cel changed. */

    bool tick();



    /** OP_0B / mode $17 @ 0x23C8C, clamped to viewport (8,8)–(215,127).
     *  Simple path @ 0x23E24: sign_sprite_place(pos,$40,$20) → dst (64, 40). */
    void blitCentered(gfx::ScreenCompositor &c, int placement_index = 0) const;

    void blitAt(gfx::ScreenCompositor &c, int dst_x, int dst_y) const;



private:

    bool loadFromPath(const char *path, AnmLoopMode loop, bool apply_hw_palette);

    bool setComposedFrame(int frame_idx);

    void resetPlayback(AnmLoopMode loop);

    void syncComposeOriginFromCache();



#if MM2_HOST_AMIGA

    /* Per-cel chip cache: compose each frame once, then index-only playback. */
    mm2_anm_planar_cache cache_{};

    mutable bool hw_palette_live_ = false;

#else

    uint8_t *rgba_ = nullptr;

#endif

    mm2_anm_file anm_{};

    mm2_anm_sequence_table seq_{};

    bool anm_loaded_ = false;

    bool use_sequence_ = false;

    bool animating_ = false;

    bool use_host_palette_ = false;

    AnmLoopMode loop_mode_ = AnmLoopMode::Loop;

    int composed_frame_ = 0;

    int seq_step_ = 0;

    int delay_remaining_ = 0;

    int w_ = 0;

    int h_ = 0;

    /* mm2_anm_compose_canvas_of origin — retail blits the full cel at (0x20,0x48). */
    int compose_min_x_ = 0;

    int compose_min_y_ = 0;

};



}  // namespace mm2::gfx

