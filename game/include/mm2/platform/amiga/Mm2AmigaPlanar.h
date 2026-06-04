#pragma once

#ifdef AMIGA

#include <ace/types.h>
#include "mm2_image32_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Clear the playfield back buffer (pen 0). */
void mm2_amiga_clear_screen(void);

/** Pens for title menu UI after clearScreen (white=1, red=2). */
void mm2_amiga_apply_ui_palette(void);

/** Plot one pen index into the ACE pBack buffer (6bpp planar). */
void mm2_amiga_put_pixel_rgb(UWORD uwX, UWORD uwY, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA);

/** Load ECS .32 pens 0-31 into AGA viewport palette (ULONG 0x00RRGGBB). */
void mm2_amiga_apply_palette(const mm2_image32_file *img);

/**
 * Blit one decoded planar frame onto the playfield back buffer.
 * @param opaque 0 = MINTERM_COOKIE (pen 0 transparent), non-zero = MINTERM_COPY.
 * Dest width/height are clamped to the 320x200 viewport (retail intro @ x=3).
 */
void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY,
                          UBYTE opaque);

/** Wait for blitter, then push dirty viewport palette to hardware COLORx. */
void mm2_amiga_present_end(void);

/** Push viewport pPalette -> COLORx (called from frame end when dirty). */
void mm2_amiga_push_palette(void);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA */
