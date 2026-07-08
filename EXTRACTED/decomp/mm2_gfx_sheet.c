#include "mm2_gfx_sheet.h"

#include <string.h>

void mm2_gfx_sheet_free(mm2_gfx_sheet *sheet)
{
    if (!sheet) {
        return;
    }
    mm2_image32_free(&sheet->img);
    sheet->backend = MM2_GFX_BACKEND_AMIGA32;
    sheet->role = MM2_GFX_ROLE_WALL;
}

mm2_image32_error mm2_gfx_sheet_adopt_image32(mm2_image32_file *src, mm2_gfx_backend_kind backend,
                                              mm2_gfx_sheet_role role, mm2_gfx_sheet *out)
{
    if (!src || !out) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    mm2_gfx_sheet_free(out);
    out->img = *src;
    out->backend = (uint8_t)backend;
    out->role = (uint8_t)role;
    memset(src, 0, sizeof(*src));
    return MM2_IMAGE32_OK;
}
