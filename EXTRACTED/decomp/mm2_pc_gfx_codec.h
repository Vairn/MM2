#ifndef MM2_PC_GFX_CODEC_H
#define MM2_PC_GFX_CODEC_H

#include "mm2_gfx_sheet.h"

#ifdef __cplusplus
extern "C" {
#endif

void mm2_pc_gfx_set_cga_palette(int palette);
int mm2_pc_gfx_cga_palette(void);

/** Decode a PC wall/sprite sheet (.4 / .16). ``silhouette_path`` may be NULL. */
mm2_image32_error mm2_pc_wall_sheet_load(const char *path, mm2_gfx_sheet_role role,
                                         const char *silhouette_path, mm2_gfx_sheet *out);

/** Test helper: raw packed frame bytes for ``frame_index`` (caller frees ``*out_pixels``). */
mm2_image32_error mm2_pc_wall_frame_packed_pixels(const char *path, int frame_index, uint8_t **out_pixels,
                                                  size_t *out_len, uint16_t *out_w, uint16_t *out_h);

/** Test helper: LZW-decompress file payload (caller frees return value). */
uint8_t *mm2_pc_lzw_decompress(const uint8_t *source, size_t source_len, size_t dest_size, size_t *out_len);

#if defined(MM2_CODEC_AMIGA)
/** Pack index grid + alpha into ACE planar bitmap + mask (caller frees via mm2_pc_gfx_planar_frame_free). */
mm2_image32_error mm2_pc_gfx_indices_to_planar(const int *indices, const uint8_t *alpha, uint16_t w, uint16_t h,
                                                 mm2_image32_frame *out);
void mm2_pc_gfx_planar_frame_free(mm2_image32_frame *frame);
#endif

#ifdef __cplusplus
}
#endif

#endif
