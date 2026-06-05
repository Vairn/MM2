#pragma once

#ifdef AMIGA

#include <ace/types.h>
#include "mm2_image32_codec.h"

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
void mm2_amiga_draw_glyph8(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA);

void mm2_amiga_apply_palette(const mm2_image32_file *img);
/** Load asset pens into the viewport palette without pushing to the display (for fade prep). */
void mm2_amiga_stage_asset_palette(const mm2_image32_file *img);
void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY,
                          UBYTE opaque);

void mm2_amiga_present_end(void);
void mm2_amiga_push_palette(void);

/** Allocate the 320×200 off-screen menu buffer (call once from init, not tick/render). */
UBYTE mm2_amiga_ui_cache_create(void);
void mm2_amiga_ui_cache_begin(void);
void mm2_amiga_ui_cache_end(void);
void mm2_amiga_ui_cache_present(void);
/** Drop pixel target only — never destroys the chip bitmap (avoids deep-stack bitmapDestroy). */
void mm2_amiga_ui_cache_invalidate(void);
void mm2_amiga_ui_cache_destroy(void);
UBYTE mm2_amiga_ui_cache_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA */
