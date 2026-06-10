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

/** Composite .anm frame 0 (base cel) to RGBA; pen 0 transparent. Caller frees rgba. */
int mm2_anm_composite_frame0_rgba(const mm2_anm_file *anm, mm2_anm_composite_rgba *out);

void mm2_anm_composite_rgba_free(mm2_anm_composite_rgba *img);

#ifdef __cplusplus
}
#endif

#endif
