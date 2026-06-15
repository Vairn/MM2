#ifndef MM2_ANM_PREVIEW_H
#define MM2_ANM_PREVIEW_H

#include "mm2_anm_codec.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mm2_anm_composite_rgba {
    uint8_t *rgba;
    int width;
    int height;
} mm2_anm_composite_rgba;

typedef struct mm2_anm_compose_canvas {
    int min_x;
    int min_y;
    int width;
    int height;
} mm2_anm_compose_canvas;

/** Union canvas for runtime-composed .anm states (doc 07-anm-tv-format.md). */
int mm2_anm_compose_canvas_of(const mm2_anm_file *anm, mm2_anm_compose_canvas *out);

/**
 * Composite composed frame `frame_idx` (0 = base cel only; N>0 = base + prelude patch).
 * Pen 0 is transparent. Caller frees out->rgba via mm2_anm_composite_rgba_free().
 */
int mm2_anm_composite_frame_rgba(const mm2_anm_file *anm, int frame_idx, mm2_anm_composite_rgba *out);

/** Default overlay cel: composed frame 1 when multi-frame, else 0. */
int mm2_anm_default_overlay_composed_frame(const mm2_anm_file *anm);

/** Composite .anm frame 0 (raw base cel) to RGBA; pen 0 transparent. */
int mm2_anm_composite_frame0_rgba(const mm2_anm_file *anm, mm2_anm_composite_rgba *out);

void mm2_anm_composite_rgba_free(mm2_anm_composite_rgba *img);

#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
/** Chip bitmap + pen-0 mask for blitCopyMask (Amiga playfield depth). */
typedef struct mm2_anm_composite_planar {
    void *bitmap; /* tBitMap* — reused across frames; allocated once, freed on _free() */
    void *mask;   /* tBitMap* 1bpp — reused across frames */
    uint16_t palette_words[MM2_ANM_PALETTE_COLORS];
    int width;
    int height;
    /* Input hint: when set, remaps .anm pens to nearest env pen (debug only — retail
     * uses hardware pens 3-17 for overlays; see MM2_ANM_OVERLAY_PALETTE_*). */
    uint8_t map_to_host_palette;
} mm2_anm_composite_planar;

/**
 * Composed .anm cel → chip bitmap; pen 0 transparent. The destination bitmap/mask
 * in `out` are allocated on first call and REUSED on later calls (no per-frame
 * bitmapCreate). Caller frees once via mm2_anm_composite_planar_free().
 */
int mm2_anm_composite_frame_planar(const mm2_anm_file *anm, int frame_idx, mm2_anm_composite_planar *out);

void mm2_anm_composite_planar_free(mm2_anm_composite_planar *img);
#endif

#ifdef __cplusplus
}
#endif

#endif
