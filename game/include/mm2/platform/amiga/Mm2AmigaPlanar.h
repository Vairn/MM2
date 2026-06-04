#pragma once

#ifdef AMIGA

#include <ace/types.h>
#include "mm2_image32_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Clear the playfield back buffer (pen 0). */
void mm2_amiga_clear_screen(void);

/** Load a .32 file's 32-color palette into the active AGA viewport. */
void mm2_amiga_apply_palette(const mm2_image32_file *img);

/** Blit one decoded planar frame onto the playfield (MINTERM_COOKIE, pen 0 = skip). */
void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA */
