#include "mm2_anm_preview.h"

#include "mm2_codec_platform.h"

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

static int pen_at(const uint8_t *planes, size_t rs, int bpr, uint16_t w, uint16_t h, int x, int y)
{
    if (x < 0 || y < 0 || x >= (int)w || y >= (int)h) {
        return 0;
    }
    int idx = 0;
    for (int pl = 0; pl < MM2_ANM_PLANES; ++pl) {
        const size_t byte_pos = pl * rs + (size_t)y * (size_t)bpr + (size_t)(x >> 3);
        const int bit = (planes[byte_pos] >> (7 - (x & 7))) & 1;
        idx |= bit << pl;
    }
    return idx;
}

static void put_pen_rgba(uint8_t *rgba, int cw, int ch, int x, int y, const uint8_t pen[4])
{
    if (x < 0 || y < 0 || x >= cw || y >= ch || pen[3] == 0) {
        return;
    }
    uint8_t *o = rgba + ((size_t)y * (size_t)cw + (size_t)x) * 4u;
    o[0] = pen[0];
    o[1] = pen[1];
    o[2] = pen[2];
    o[3] = pen[3];
}

static void clear_rect_rgba(uint8_t *rgba, int cw, int ch, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    for (int py = 0; py < h; ++py) {
        const int oy = y + py;
        if (oy < 0 || oy >= ch) {
            continue;
        }
        for (int px = 0; px < w; ++px) {
            const int ox = x + px;
            if (ox < 0 || ox >= cw) {
                continue;
            }
            uint8_t *d = rgba + ((size_t)oy * (size_t)cw + (size_t)ox) * 4u;
            d[0] = d[1] = d[2] = d[3] = 0;
        }
    }
}

static void blit_planes_rgba(const mm2_anm_file *anm, const uint8_t *planes, uint16_t sw, uint16_t sh,
                             int dst_x, int dst_y, int copy_w, int copy_h, int cw, int ch, uint8_t *rgba)
{
    const size_t rs = mm2_anm_rassize(sw, sh);
    const int bpr = (int)((((uint32_t)sw + 15U) >> 3) & 0xFFFEU);
    uint8_t pal[MM2_ANM_PALETTE_COLORS][4];

    for (int i = 0; i < MM2_ANM_PALETTE_COLORS; ++i) {
        palette_word_to_rgba(anm->palette_words[i], pal[i]);
        if (i == 0) {
            pal[i][3] = 0;
        }
    }

    if (copy_w > (int)sw) {
        copy_w = (int)sw;
    }
    if (copy_h > (int)sh) {
        copy_h = (int)sh;
    }

    for (int y = 0; y < copy_h; ++y) {
        for (int x = 0; x < copy_w; ++x) {
            const int idx = pen_at(planes, rs, bpr, sw, sh, x, y);
            if (idx == 0) {
                continue;
            }
            put_pen_rgba(rgba, cw, ch, dst_x + x, dst_y + y, pal[idx]);
        }
    }
}

int mm2_anm_compose_canvas_of(const mm2_anm_file *anm, mm2_anm_compose_canvas *out)
{
    if (!anm || !out || anm->frame_count <= 0 || !anm->frames) {
        return 0;
    }

    int min_x = 0;
    int min_y = 0;
    int max_x = (int)anm->frames[0].width;
    int max_y = (int)anm->frames[0].height;

    for (int i = 1; i < (int)anm->frame_count; ++i) {
        const int pre_idx = i - 1;
        if (pre_idx < 0 || pre_idx >= MM2_ANM_PRELUDE_SLOTS || !anm->prelude[pre_idx].is_used) {
            continue;
        }
        const mm2_anm_prelude_slot *pe = &anm->prelude[pre_idx];
        int w = pe->w;
        int h = pe->h;
        if (w <= 0) {
            w = (int)anm->frames[i].width;
        }
        if (h <= 0) {
            h = (int)anm->frames[i].height;
        }
        if (w > (int)anm->frames[i].width) {
            w = (int)anm->frames[i].width;
        }
        if (h > (int)anm->frames[i].height) {
            h = (int)anm->frames[i].height;
        }
        if (w <= 0 || h <= 0) {
            continue;
        }
        const int x = pe->x_offset;
        const int y = pe->y_offset;
        if (x < min_x) {
            min_x = x;
        }
        if (y < min_y) {
            min_y = y;
        }
        if (x + w > max_x) {
            max_x = x + w;
        }
        if (y + h > max_y) {
            max_y = y + h;
        }
    }

    out->min_x = min_x;
    out->min_y = min_y;
    out->width = max_x - min_x;
    out->height = max_y - min_y;
    return out->width > 0 && out->height > 0;
}

int mm2_anm_default_overlay_composed_frame(const mm2_anm_file *anm)
{
    if (!anm || anm->frame_count <= 1) {
        return 0;
    }
    return 1;
}

int mm2_anm_composite_frame_rgba(const mm2_anm_file *anm, int frame_idx, mm2_anm_composite_rgba *out)
{
    mm2_anm_compose_canvas canvas;
    memset(&canvas, 0, sizeof(canvas));

    if (!anm || !out || frame_idx < 0 || frame_idx >= (int)anm->frame_count || !anm->frames) {
        return 0;
    }
    if (!mm2_anm_compose_canvas_of(anm, &canvas)) {
        return 0;
    }

    const size_t rgba_size = (size_t)canvas.width * (size_t)canvas.height * 4u;
    uint8_t *rgba = (uint8_t *)malloc(rgba_size);
    if (!rgba) {
        return 0;
    }
    memset(rgba, 0, rgba_size);

    const mm2_anm_frame_info *base = &anm->frames[0];
    if (base->planes) {
        blit_planes_rgba(anm, base->planes, base->width, base->height, -canvas.min_x, -canvas.min_y,
                         (int)base->width, (int)base->height, canvas.width, canvas.height, rgba);
    }

    if (frame_idx > 0) {
        const mm2_anm_frame_info *patch = &anm->frames[frame_idx];
        int x = -canvas.min_x;
        int y = -canvas.min_y;
        int copy_w = (int)patch->width;
        int copy_h = (int)patch->height;
        const int pre_idx = frame_idx - 1;
        if (pre_idx >= 0 && pre_idx < MM2_ANM_PRELUDE_SLOTS && anm->prelude[pre_idx].is_used &&
            patch->planes) {
            const mm2_anm_prelude_slot *pe = &anm->prelude[pre_idx];
            x = pe->x_offset - canvas.min_x;
            y = pe->y_offset - canvas.min_y;
            if (pe->w > 0 && pe->w < copy_w) {
                copy_w = pe->w;
            }
            if (pe->h > 0 && pe->h < copy_h) {
                copy_h = pe->h;
            }
            clear_rect_rgba(rgba, canvas.width, canvas.height, x, y, copy_w, copy_h);
            blit_planes_rgba(anm, patch->planes, patch->width, patch->height, x, y, copy_w, copy_h,
                             canvas.width, canvas.height, rgba);
        }
    }

    out->rgba = rgba;
    out->width = canvas.width;
    out->height = canvas.height;
    return 1;
}

int mm2_anm_composite_frame0_rgba(const mm2_anm_file *anm, mm2_anm_composite_rgba *out)
{
    return mm2_anm_composite_frame_rgba(anm, 0, out);
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

#if defined(MM2_CODEC_AMIGA)

#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"

#include <ace/managers/blit.h>
#include <ace/managers/memory.h>
#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>

static int anm_pen_at(const uint8_t *planes, size_t rs, int bpr, uint16_t w, uint16_t h, int x, int y)
{
    if (x < 0 || y < 0 || x >= (int)w || y >= (int)h) {
        return 0;
    }
    int idx = 0;
    for (int pl = 0; pl < MM2_ANM_PLANES; ++pl) {
        const size_t byte_pos = pl * rs + (size_t)y * (size_t)bpr + (size_t)(x >> 3);
        const int bit = (planes[byte_pos] >> (7 - (x & 7))) & 1;
        idx |= bit << pl;
    }
    return idx;
}

/* Build a 32-entry pen remap. Identity unless map_to_host applies, in which case
 * each .anm pen (1..31) maps to the nearest LIVE host/env palette pen (1..31), so
 * the overlay shares the one hardware palette instead of overwriting env pens. */
static void anm_build_pen_remap(const mm2_anm_file *anm, int map_to_host, UBYTE remap[MM2_ANM_PALETTE_COLORS])
{
    int p;
    tVPort *pVp;
    const ULONG *pPal = NULL;

    for (p = 0; p < MM2_ANM_PALETTE_COLORS; ++p) {
        remap[p] = (UBYTE)p;
    }
    if (!map_to_host) {
        return;
    }
    pVp = mm2AmigaDisplayGetVPort();
    if (pVp && pVp->pPalette) {
        pPal = (const ULONG *)pVp->pPalette;
    }
    if (!pPal) {
        return; /* no host palette yet — keep identity (first-frame transient) */
    }

    remap[0] = 0; /* pen 0 stays transparent */
    for (p = 1; p < MM2_ANM_PALETTE_COLORS; ++p) {
        const uint16_t word = anm->palette_words[p];
        const int r = (int)(((word >> 8) & 0x0F) * 17);
        const int g = (int)(((word >> 4) & 0x0F) * 17);
        const int b = (int)((word & 0x0F) * 17);
        long best_d = 0x7FFFFFFFL;
        UBYTE best_q = 1;
        int q;
        for (q = 1; q < MM2_ANM_PALETTE_COLORS; ++q) {
            const ULONG c = pPal[q];
            const int er = (int)((c >> 16) & 0xFF);
            const int eg = (int)((c >> 8) & 0xFF);
            const int eb = (int)(c & 0xFF);
            const long dr = r - er;
            const long dg = g - eg;
            const long db = b - eb;
            const long d = dr * dr + dg * dg + db * db;
            if (d < best_d) {
                best_d = d;
                best_q = (UBYTE)q;
            }
        }
        remap[p] = best_q; /* candidates start at 1, so remapped pens are always >=1 */
    }
}

static void anm_put_pen_bitmap(tBitMap *bm, int x, int y, UBYTE pen)
{
    UBYTE pl;
    const UWORD bpr = bm->BytesPerRow;
    const UBYTE bit = (UBYTE)(0x80 >> (x & 7));
    const UWORD byte_x = (UWORD)(x >> 3);

    /* Clamp BOTH axes to the destination bitmap — byte_x past BytesPerRow would
     * overrun the plane and trash the adjacent allocation's right guard band. */
    if (x < 0 || y < 0 || y >= (int)bm->Rows || byte_x >= bpr) {
        return;
    }
    for (pl = 0; pl < bm->Depth; ++pl) {
        UBYTE *row = bm->Planes[pl] + (ULONG)y * (ULONG)bpr;
        if (pen & (1u << pl)) {
            row[byte_x] |= bit;
        } else {
            row[byte_x] &= (UBYTE)~bit;
        }
    }
}

static void anm_clear_rect_bitmap(tBitMap *bm, int x, int y, int w, int h)
{
    int py;
    int px;
    if (!bm || w <= 0 || h <= 0) {
        return;
    }
    for (py = 0; py < h; ++py) {
        const int oy = y + py;
        if (oy < 0 || oy >= (int)bm->Rows) {
            continue;
        }
        for (px = 0; px < w; ++px) {
            const int ox = x + px;
            if (ox < 0 || ox >= (int)bm->BytesPerRow * 8) {
                continue;
            }
            anm_put_pen_bitmap(bm, ox, oy, 0);
        }
    }
}

static void anm_blit_planes_to_bitmap(tBitMap *dst, int dst_x, int dst_y, const uint8_t *planes, uint16_t sw,
                                      uint16_t sh, int copy_w, int copy_h, const UBYTE remap[MM2_ANM_PALETTE_COLORS])
{
    const size_t rs = mm2_anm_rassize(sw, sh);
    const int bpr = (int)((((uint32_t)sw + 15U) >> 3) & 0xFFFEU);
    int y;
    int x;

    if (!dst || !planes || copy_w <= 0 || copy_h <= 0) {
        return;
    }
    if (copy_w > (int)sw) {
        copy_w = (int)sw;
    }
    if (copy_h > (int)sh) {
        copy_h = (int)sh;
    }

    for (y = 0; y < copy_h; ++y) {
        for (x = 0; x < copy_w; ++x) {
            const int pen = anm_pen_at(planes, rs, bpr, sw, sh, x, y);
            if (pen == 0) {
                continue;
            }
            anm_put_pen_bitmap(dst, dst_x + x, dst_y + y, remap[pen & (MM2_ANM_PALETTE_COLORS - 1)]);
        }
    }
}

/* Clear every plane of a chip bitmap to pen 0 (reuse before recompose). */
static void anm_clear_bitmap_planes(tBitMap *bm)
{
    UBYTE pl;
    const ULONG ulPlaneBytes = (ULONG)bm->BytesPerRow * (ULONG)bm->Rows;
    if (bitmapIsInterleaved(bm)) {
        if (bm->Planes[0]) {
            memset(bm->Planes[0], 0, ulPlaneBytes * (ULONG)bm->Depth);
        }
        return;
    }
    for (pl = 0; pl < bm->Depth; ++pl) {
        if (bm->Planes[pl]) {
            memset(bm->Planes[pl], 0, ulPlaneBytes);
        }
    }
}

/* Rebuild the pen-0 mask IN PLACE: pre-cleared msk, set bit where bm pen != 0. */
static void anm_refresh_pen0_mask(const tBitMap *bm, tBitMap *msk, UWORD uwVisW, UWORD uwVisH)
{
    UWORD y;
    UWORD x;
    UBYTE pl;
    const UWORD bm_bpr = bm->BytesPerRow;
    const UWORD msk_bpr = msk->BytesPerRow;

    if (uwVisH > bm->Rows) {
        uwVisH = bm->Rows;
    }
    if (uwVisW > (UWORD)(bm_bpr * 8)) {
        uwVisW = (UWORD)(bm_bpr * 8);
    }
    for (y = 0; y < uwVisH; ++y) {
        for (x = 0; x < uwVisW; ++x) {
            const UBYTE bit = (UBYTE)(0x80 >> (x & 7));
            const UWORD byte_x = x >> 3;
            UBYTE nonzero = 0;
            for (pl = 0; pl < bm->Depth; ++pl) {
                const UBYTE *row = bm->Planes[pl] + (ULONG)y * (ULONG)bm_bpr;
                if (row[byte_x] & bit) {
                    nonzero = 1;
                    break;
                }
            }
            if (nonzero && byte_x < msk_bpr) {
                UBYTE *mrow = msk->Planes[0] + (ULONG)y * (ULONG)msk_bpr;
                mrow[byte_x] |= bit;
            }
        }
    }
}

/* Allocate (or reuse) the persistent composite bitmap+mask sized to the canvas. */
static int anm_ensure_planar_bitmaps(mm2_anm_composite_planar *out, UWORD uwBmW, UWORD uwVisH)
{
    tBitMap *bm = (tBitMap *)out->bitmap;
    tBitMap *msk = (tBitMap *)out->mask;

    if (bm && ((UWORD)(bm->BytesPerRow * 8) < uwBmW || bm->Rows < uwVisH)) {
        bitmapDestroy(bm);
        bm = NULL;
        out->bitmap = NULL;
    }
    if (!bm) {
        bm = bitmapCreate(uwBmW, uwVisH, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
        if (!bm) {
            return 0;
        }
        out->bitmap = bm;
    } else {
        anm_clear_bitmap_planes(bm);
    }

    if (msk && ((UWORD)(msk->BytesPerRow * 8) < uwBmW || msk->Rows < uwVisH)) {
        bitmapDestroy(msk);
        msk = NULL;
        out->mask = NULL;
    }
    if (!msk) {
        msk = bitmapCreate(uwBmW, uwVisH, 1, BMF_CLEAR);
        if (!msk) {
            return 0;
        }
        out->mask = msk;
    } else {
        anm_clear_bitmap_planes(msk);
    }
    return 1;
}

int mm2_anm_composite_frame_planar(const mm2_anm_file *anm, int frame_idx, mm2_anm_composite_planar *out)
{
    mm2_anm_compose_canvas canvas;
    tBitMap *bm;
    tBitMap *msk;
    UWORD uwVisW;
    UWORD uwVisH;
    UWORD uwBmW;
    UBYTE remap[MM2_ANM_PALETTE_COLORS];

    memset(&canvas, 0, sizeof(canvas));
    if (!anm || !out || frame_idx < 0 || frame_idx >= (int)anm->frame_count || !anm->frames) {
        return 0;
    }
    if (!mm2_anm_compose_canvas_of(anm, &canvas)) {
        return 0;
    }

    uwVisW = (UWORD)canvas.width;
    uwVisH = (UWORD)canvas.height;
    uwBmW = (UWORD)(((ULONG)uwVisW + 15UL) & ~15UL);

    /* The compose canvas is frame-INDEPENDENT (union over all frames), so the
     * bitmap dimensions never change between cels — allocate once, reuse. */
    if (!anm_ensure_planar_bitmaps(out, uwBmW, uwVisH)) {
        return 0;
    }
    bm = (tBitMap *)out->bitmap;
    msk = (tBitMap *)out->mask;

    anm_build_pen_remap(anm, out->map_to_host_palette, remap);

    {
        const mm2_anm_frame_info *base = &anm->frames[0];
        if (base->planes) {
            anm_blit_planes_to_bitmap(bm, -canvas.min_x, -canvas.min_y, base->planes, base->width, base->height,
                                      (int)base->width, (int)base->height, remap);
        }
    }

    if (frame_idx > 0) {
        const mm2_anm_frame_info *patch = &anm->frames[frame_idx];
        int x = -canvas.min_x;
        int y = -canvas.min_y;
        int copy_w = (int)patch->width;
        int copy_h = (int)patch->height;
        const int pre_idx = frame_idx - 1;
        if (pre_idx >= 0 && pre_idx < MM2_ANM_PRELUDE_SLOTS && anm->prelude[pre_idx].is_used && patch->planes) {
            const mm2_anm_prelude_slot *pe = &anm->prelude[pre_idx];
            x = pe->x_offset - canvas.min_x;
            y = pe->y_offset - canvas.min_y;
            if (pe->w > 0 && pe->w < copy_w) {
                copy_w = pe->w;
            }
            if (pe->h > 0 && pe->h < copy_h) {
                copy_h = pe->h;
            }
            anm_clear_rect_bitmap(bm, x, y, copy_w, copy_h);
            anm_blit_planes_to_bitmap(bm, x, y, patch->planes, patch->width, patch->height, copy_w, copy_h, remap);
        }
    }

    anm_refresh_pen0_mask(bm, msk, uwVisW, uwVisH);

    memcpy(out->palette_words, anm->palette_words, sizeof(out->palette_words));
    out->width = canvas.width;
    out->height = canvas.height;
    return 1;
}

void mm2_anm_composite_planar_free(mm2_anm_composite_planar *img)
{
    if (!img) {
        return;
    }
    if (img->mask) {
        bitmapDestroy((tBitMap *)img->mask);
        img->mask = NULL;
    }
    if (img->bitmap) {
        bitmapDestroy((tBitMap *)img->bitmap);
        img->bitmap = NULL;
    }
    img->width = 0;
    img->height = 0;
}

#endif /* MM2_CODEC_AMIGA */
