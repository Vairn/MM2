#include "mm2_anm_preview.h"

#include <stdlib.h>
#include <string.h>

static void palette_word_to_rgba(uint16_t word, uint8_t out[4])
{
    const uint8_t r = (uint8_t)(((word >> 8) & 0x0F) * 17);
    const uint8_t g = (uint8_t)(((word >> 4) & 0x0F) * 17);
    const uint8_t b = (uint8_t)((word & 0x0F) * 17);
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = 255;
}

static void rasterize_planes(const mm2_anm_file *anm, const uint8_t *planes, uint16_t w, uint16_t h,
                             uint8_t *rgba)
{
    const size_t rs = mm2_anm_rassize(w, h);
    const int bpr = (int)((((uint32_t)w + 15U) >> 3) & 0xFFFEU);
    uint8_t pal[MM2_ANM_PALETTE_COLORS][4];
    int y;

    for (int i = 0; i < MM2_ANM_PALETTE_COLORS; ++i) {
        palette_word_to_rgba(anm->palette_words[i], pal[i]);
    }

    for (y = 0; y < (int)h; ++y) {
        for (int x = 0; x < (int)w; ++x) {
            int idx = 0;
            for (int pl = 0; pl < MM2_ANM_PLANES; ++pl) {
                const size_t byte_pos =
                    pl * rs + (size_t)y * (size_t)bpr + (size_t)(x >> 3);
                const int bit = (planes[byte_pos] >> (7 - (x & 7))) & 1;
                idx |= bit << pl;
            }
            uint8_t *o = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4u;
            o[0] = pal[idx][0];
            o[1] = pal[idx][1];
            o[2] = pal[idx][2];
            o[3] = (idx == 0) ? 0u : 255u;
        }
    }
}

int mm2_anm_composite_frame0_rgba(const mm2_anm_file *anm, mm2_anm_composite_rgba *out)
{
    if (!anm || !out || anm->frame_count <= 0 || !anm->frames[0].planes) {
        return 0;
    }

    const mm2_anm_frame_info *base = &anm->frames[0];
    const size_t rgba_size = (size_t)base->width * (size_t)base->height * 4u;
    uint8_t *rgba = (uint8_t *)malloc(rgba_size);
    if (!rgba) {
        return 0;
    }
    memset(rgba, 0, rgba_size);
    rasterize_planes(anm, base->planes, base->width, base->height, rgba);

    out->rgba = rgba;
    out->width = (int)base->width;
    out->height = (int)base->height;
    return 1;
}

void mm2_anm_composite_rgba_free(mm2_anm_composite_rgba *img)
{
    if (!img) {
        return;
    }
    free(img->rgba);
    img->rgba = NULL;
    img->width = 0;
    img->height = 0;
}
