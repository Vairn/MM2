#ifndef MM2_GFX_SHEET_H
#define MM2_GFX_SHEET_H

#include "mm2_image32_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef mm2_image32_frame mm2_gfx_frame;

typedef enum mm2_gfx_backend_kind {
    MM2_GFX_BACKEND_AMIGA32 = 0,
    MM2_GFX_BACKEND_PC_CGA = 1,
    MM2_GFX_BACKEND_PC_EGA = 2
} mm2_gfx_backend_kind;

typedef enum mm2_gfx_sheet_role {
    MM2_GFX_ROLE_WALL = 0,
    MM2_GFX_ROLE_FLOOR,
    MM2_GFX_ROLE_TORCH,
    MM2_GFX_ROLE_SKY,
    MM2_GFX_ROLE_AUTOMAP,
    MM2_GFX_ROLE_OUTDOOR_HORIZON,
    MM2_GFX_ROLE_OUTDOOR_BIOME,
    MM2_GFX_ROLE_TITLE
} mm2_gfx_sheet_role;

typedef struct mm2_gfx_sheet {
    mm2_image32_file img;
    uint8_t backend;
    uint8_t role;
} mm2_gfx_sheet;

void mm2_gfx_sheet_free(mm2_gfx_sheet *sheet);

/** Move ``src`` contents into ``out``; ``src`` is zeroed on success. */
mm2_image32_error mm2_gfx_sheet_adopt_image32(mm2_image32_file *src, mm2_gfx_backend_kind backend,
                                              mm2_gfx_sheet_role role, mm2_gfx_sheet *out);

static inline const mm2_gfx_frame *mm2_gfx_sheet_frame(const mm2_gfx_sheet *sheet, int index)
{
    if (!sheet || index < 0 || index >= (int)sheet->img.frame_count) {
        return NULL;
    }
    return &sheet->img.frames[index];
}

#ifdef __cplusplus
}
#endif

#endif
