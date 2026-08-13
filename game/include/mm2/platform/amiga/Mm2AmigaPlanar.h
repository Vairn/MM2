#pragma once

#ifdef AMIGA

#include <ace/types.h>
#include "mm2_image32_codec.h"
#include "mm2_anm_preview.h"

#ifdef __cplusplus
extern "C" {
#endif

void mm2_amiga_clear_screen(void);
void mm2_amiga_apply_ui_palette(void);
void mm2_amiga_blit_sync(void);

void mm2_amiga_fill_rect_pen(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE pen);
void mm2_amiga_fill_rect_rgb(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE ubR, UBYTE ubG, UBYTE ubB,
                             UBYTE ubA);

void mm2_amiga_put_pixel_pen(UWORD uwX, UWORD uwY, UBYTE pen);
void mm2_amiga_put_pixel_rgb(UWORD uwX, UWORD uwY, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA);

void mm2_amiga_draw_glyph8_pen(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE pen, UBYTE ubA);
/** Printable runs: assemble a line mask then one pen fill + blitCopyMask (cell-aligned X). */
void mm2_amiga_draw_text_pen(UWORD uwX, UWORD uwY, const char *text, UBYTE pen, UBYTE ubA);
void mm2_amiga_draw_glyph8(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA);

/** Build 1bpp 128×64 glyph mask atlas + 8×8 ink stamp (after mm2_amiga_font_init). */
UBYTE mm2_amiga_font_atlas_create(void);
void mm2_amiga_font_atlas_shutdown(void);

void mm2_amiga_apply_palette(const mm2_image32_file *img);
/** Remember env sheet + load pens 0-31 once (play mode — not per blit). */
void mm2_amiga_apply_play_world_palette(const mm2_image32_file *img);
void mm2_amiga_restore_play_world_palette(void);
void mm2_amiga_clear_play_world_palette(void);
/** Load asset pens into vport RAM only (fade prep — no hardware push). */
void mm2_amiga_stage_asset_palette(const mm2_image32_file *img);
void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY,
                          UBYTE opaque);

/** Load .anm palette words into viewport pens 3-17 only (env pens 0-2, 18-31 unchanged). */
void mm2_amiga_apply_anm_palette(const uint16_t palette_words[MM2_ANM_PALETTE_COLORS]);
/** Masked blit of composed .anm cel (pen 0 skipped via blitCopyMask). */
void mm2_amiga_blit_anm_composed(const mm2_anm_composite_planar *img, UWORD uwDstX, UWORD uwDstY);

void mm2_amiga_present_end(void);
/** Push dirty palette banks to hardware (world pens 0-31 and/or UI pens 32-63). */
void mm2_amiga_push_palette(void);

/** Allocate the 320×200 off-screen menu buffer (call once from init, not tick/render). */
UBYTE mm2_amiga_ui_cache_create(void);
void mm2_amiga_ui_cache_begin(void);
/** Point pixel dest at the UI cache without clearing (incremental patch). */
void mm2_amiga_ui_cache_begin_keep(void);
void mm2_amiga_ui_cache_end(void);
void mm2_amiga_ui_cache_present(void);
/** Present a sub-rect of the UI cache (word-aligned width for the blitter). */
void mm2_amiga_ui_cache_present_rect(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH);
/** Drop pixel target only — never destroys the chip bitmap (avoids deep-stack bitmapDestroy). */
void mm2_amiga_ui_cache_invalidate(void);
void mm2_amiga_ui_cache_destroy(void);
UBYTE mm2_amiga_ui_cache_ready(void);

/** Static play HUD chrome (black fills + red border glyphs) — blit each full frame. */
UBYTE mm2_amiga_play_chrome_cache_create(void);
void mm2_amiga_play_chrome_cache_begin(void);
void mm2_amiga_play_chrome_cache_end(void);
void mm2_amiga_play_chrome_cache_present(void);
void mm2_amiga_play_chrome_cache_invalidate(void);
void mm2_amiga_play_chrome_cache_destroy(void);
UBYTE mm2_amiga_play_chrome_cache_ready(void);

/** 3D hood snapshot — restore before overlay-only .anm cel updates.
 *  Default size is exploration (208×120). Combat round must shrink via
 *  mm2_amiga_viewport_cache_set_size so restore does not wipe the roster. */
UBYTE mm2_amiga_viewport_cache_create(void);
void mm2_amiga_viewport_cache_set_size(UWORD uwW, UWORD uwH);
void mm2_amiga_viewport_cache_save(void);
void mm2_amiga_viewport_cache_restore(void);
void mm2_amiga_viewport_cache_invalidate(void);
UBYTE mm2_amiga_viewport_cache_valid(void);

/** Copy pFront → pBack before patching only the 3D viewport (double-buffer HUD reuse). */
UBYTE mm2_amiga_copy_front_to_back(void);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA */
