/**
 * Present .32 assets on the ACE playfield — planar blit only (no RGBA / chunky).
 * Rect fills use ACE blitRect; 8×8 text uses CPU pen plot (fast enough when cached).
 */

#include "mm2/platform/amiga/Mm2AmigaPlanar.h"

#ifdef AMIGA

#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"
#include "mm2/gfx/mm2_font8x8.h"

#include <mini_std/string.h>

#include <ace/managers/blit.h>
#ifdef ACE_DEBUG
#include <ace/managers/log.h>
#endif
#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>

static const mm2_image32_file *s_applied_palette_img;

static UBYTE s_palette_dirty;

static tBitMap *s_ui_cache_bm;
static tBitMap *s_pixel_target;

/* Retail buf_copy_rect @ 0x171AC — 3D viewport before sprite overlay. */
static tBitMap *s_viewport_cache_bm;
static UBYTE s_viewport_cache_valid;

#define MM2_VIEWPORT_CACHE_X 8u
#define MM2_VIEWPORT_CACHE_Y 8u
#define MM2_VIEWPORT_CACHE_W 208u
#define MM2_VIEWPORT_CACHE_H 120u

void mm2_amiga_push_palette(void);

static tBitMap *mm2_amiga_pixel_dest(void)
{
    if (s_pixel_target) {
        return s_pixel_target;
    }
    {
        tSimpleBufferManager *pBfr = mm2AmigaDisplayGetBuffer();
        return (pBfr && pBfr->pBack) ? pBfr->pBack : NULL;
    }
}

static UBYTE mm2_amiga_rgb_to_ui_pen(UBYTE ubR, UBYTE ubG, UBYTE ubB)
{
    if (ubR <= 48 && ubG <= 48 && ubB <= 48) {
        return 0;
    }
    if (ubR >= 200 && ubG <= 96 && ubB <= 96) {
        return MM2_UI_PEN_RED;
    }
    if (ubR >= 200 && ubG >= 100 && ubG <= 170 && ubB >= 100 && ubB <= 170) {
        return MM2_UI_PEN_WARN;
    }
    if (ubR >= 200 && ubG >= 240 && ubB >= 80 && ubB <= 130) {
        return MM2_UI_PEN_YELLOW;
    }
    if (ubR >= 200 && ubG >= 170 && ubG < 240 && ubB <= 120) {
        return MM2_UI_PEN_GOLD;
    }
    if (ubR >= 210 && ubG >= 210 && ubB >= 210) {
        return MM2_UI_PEN_GREY_LIGHT;
    }
    if (ubR >= 180 && ubG >= 180 && ubB >= 180) {
        return MM2_UI_PEN_GREY_MID;
    }
    if (ubR >= 130 && ubG >= 130 && ubB >= 130) {
        return MM2_UI_PEN_GREY_FOOTER;
    }
    if (ubR >= 90 && ubG >= 90 && ubB >= 90) {
        return MM2_UI_PEN_GREY_DIM;
    }
    if (ubR > 96 || ubG > 96 || ubB > 96) {
        return MM2_UI_PEN_WHITE;
    }
    return 0;
}

static void mm2_amiga_brect(tBitMap *pDst, UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE pen)
{
    if (!pDst || uwW == 0 || uwH == 0 || !bitmapIsChip(pDst)) {
        return;
    }
    blitRect(pDst, (WORD)uwX, (WORD)uwY, (WORD)uwW, (WORD)uwH, pen);
}

void mm2_amiga_put_pixel_pen(UWORD uwX, UWORD uwY, UBYTE pen)
{
    tBitMap *pDst;
    UBYTE pl;
    UWORD bpr;

    pDst = mm2_amiga_pixel_dest();
    if (!pDst) {
        return;
    }
    bpr = pDst->BytesPerRow;
    for (pl = 0; pl < pDst->Depth; ++pl) {
        UBYTE *row = pDst->Planes[pl] + (ULONG)uwY * (ULONG)bpr;
        UBYTE bit = (UBYTE)(0x80 >> (uwX & 7));
        UWORD byte_x = uwX >> 3;
        if (pen & (1u << pl)) {
            row[byte_x] |= bit;
        } else {
            row[byte_x] &= (UBYTE)~bit;
        }
    }
}

void mm2_amiga_put_pixel_rgb(UWORD uwX, UWORD uwY, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA)
{
    if (ubA == 0 || uwX >= MM2_AGA_SCREEN_WIDTH || uwY >= MM2_AGA_SCREEN_HEIGHT) {
        return;
    }
    mm2_amiga_put_pixel_pen(uwX, uwY, mm2_amiga_rgb_to_ui_pen(ubR, ubG, ubB));
}

void mm2_amiga_fill_rect_pen(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE pen)
{
    mm2_amiga_brect(mm2_amiga_pixel_dest(), uwX, uwY, uwW, uwH, pen);
}

void mm2_amiga_fill_rect_rgb(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE ubR, UBYTE ubG, UBYTE ubB,
                             UBYTE ubA)
{
    if (ubA == 0) {
        return;
    }
    mm2_amiga_fill_rect_pen(uwX, uwY, uwW, uwH, mm2_amiga_rgb_to_ui_pen(ubR, ubG, ubB));
}

void mm2_amiga_draw_glyph8_pen(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE pen, UBYTE ubA)
{
    const uint8_t *table;
    const uint8_t *glyph;
    UBYTE row;
    UBYTE col;

    if (ubA == 0 || ubCodepoint >= MM2_FONT8X8_GLYPHS) {
        return;
    }
    table = mm2_font8x8_live();
    glyph = table + (ULONG)ubCodepoint * (ULONG)MM2_FONT8X8_ROWS;
    for (row = 0; row < MM2_FONT8X8_ROWS; ++row) {
        const UBYTE bits = glyph[row];
        for (col = 0; col < 8; ++col) {
            if ((bits >> col) & 1u) {
                mm2_amiga_put_pixel_pen(uwX + (UWORD)col, uwY + (UWORD)row, pen);
            }
        }
    }
}

void mm2_amiga_apply_ui_palette(void)
{
    tVPort *pVp;
    ULONG *pPal;
    pVp = mm2AmigaDisplayGetVPort();
    if (!pVp || !pVp->pPalette) {
        return;
    }
    pPal = (ULONG *)pVp->pPalette;
    pPal[0] = 0;
    pPal[MM2_UI_PEN_WHITE] = 0x00FFFFFFUL;
    pPal[MM2_UI_PEN_RED] = 0x00FF0000UL;
    pPal[MM2_UI_PEN_GOLD] = 0x00FFDC5AUL;
    pPal[MM2_UI_PEN_YELLOW] = 0x00FFFF64UL;
    pPal[MM2_UI_PEN_GREY_LIGHT] = 0x00E6E6E6UL;
    pPal[MM2_UI_PEN_GREY_MID] = 0x00C8C8C8UL;
    pPal[MM2_UI_PEN_GREY_FOOTER] = 0x00969696UL;
    pPal[MM2_UI_PEN_GREY_DIM] = 0x006E6E6EUL;
    pPal[MM2_UI_PEN_WARN] = 0x00FF8080UL;
    s_applied_palette_img = NULL;
    s_palette_dirty = 1;
    mm2_amiga_push_palette();
}

void mm2_amiga_blit_sync(void)
{
    blitWait();
}

void mm2_amiga_draw_glyph8(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA)
{
    mm2_amiga_draw_glyph8_pen(uwX, uwY, ubCodepoint, mm2_amiga_rgb_to_ui_pen(ubR, ubG, ubB), ubA);
}

void mm2_amiga_clear_screen(void)
{
    tBitMap *pDst = mm2_amiga_pixel_dest();
    if (!pDst) {
        return;
    }
    mm2_amiga_brect(pDst, 0, 0, MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, 0);
}

UBYTE mm2_amiga_ui_cache_create(void)
{
    if (s_ui_cache_bm) {
        return 1;
    }
    s_ui_cache_bm = bitmapCreate(MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
    return s_ui_cache_bm != NULL;
}

void mm2_amiga_ui_cache_begin(void)
{
    if (!s_ui_cache_bm) {
        return;
    }
    s_pixel_target = s_ui_cache_bm;
    mm2_amiga_brect(s_ui_cache_bm, 0, 0, MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, 0);
}

void mm2_amiga_ui_cache_end(void)
{
    s_pixel_target = NULL;
}

void mm2_amiga_ui_cache_present(void)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;

    if (!s_ui_cache_bm) {
        return;
    }
    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(s_ui_cache_bm) || !bitmapIsChip(pDst)) {
        return;
    }
    blitCopy(s_ui_cache_bm, 0, 0, pDst, 0, 0, (WORD)MM2_AGA_SCREEN_WIDTH, (WORD)MM2_AGA_SCREEN_HEIGHT,
             MINTERM_COPY);
}

void mm2_amiga_ui_cache_invalidate(void)
{
    s_pixel_target = NULL;
}

void mm2_amiga_ui_cache_destroy(void)
{
    s_pixel_target = NULL;
    if (s_ui_cache_bm) {
        bitmapDestroy(s_ui_cache_bm);
        s_ui_cache_bm = NULL;
    }
}

UBYTE mm2_amiga_ui_cache_ready(void)
{
    return s_ui_cache_bm != NULL;
}

UBYTE mm2_amiga_viewport_cache_create(void)
{
    if (s_viewport_cache_bm) {
        return 1;
    }
    s_viewport_cache_bm =
        bitmapCreate(MM2_VIEWPORT_CACHE_W + 16, MM2_VIEWPORT_CACHE_H, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
    return s_viewport_cache_bm != NULL;
}

void mm2_amiga_viewport_cache_save(void)
{
    tBitMap *pSrc;

    if (!s_viewport_cache_bm) {
        return;
    }
    pSrc = mm2_amiga_pixel_dest();
    if (!pSrc || !bitmapIsChip(s_viewport_cache_bm) || !bitmapIsChip(pSrc)) {
        return;
    }
    blitCopy(pSrc, (WORD)MM2_VIEWPORT_CACHE_X, (WORD)MM2_VIEWPORT_CACHE_Y, s_viewport_cache_bm, 0, 0,
             (WORD)MM2_VIEWPORT_CACHE_W, (WORD)MM2_VIEWPORT_CACHE_H, MINTERM_COPY);
    s_viewport_cache_valid = 1;
}

void mm2_amiga_viewport_cache_restore(void)
{
    tBitMap *pDst;

    if (!s_viewport_cache_valid || !s_viewport_cache_bm) {
        return;
    }
    pDst = mm2_amiga_pixel_dest();
    if (!pDst || !bitmapIsChip(s_viewport_cache_bm) || !bitmapIsChip(pDst)) {
        return;
    }
    blitCopy(s_viewport_cache_bm, 0, 0, pDst, (WORD)MM2_VIEWPORT_CACHE_X, (WORD)MM2_VIEWPORT_CACHE_Y,
             (WORD)MM2_VIEWPORT_CACHE_W, (WORD)MM2_VIEWPORT_CACHE_H, MINTERM_COPY);
}

void mm2_amiga_viewport_cache_invalidate(void)
{
    s_viewport_cache_valid = 0;
}

UBYTE mm2_amiga_viewport_cache_valid(void)
{
    return s_viewport_cache_valid;
}

UBYTE mm2_amiga_copy_front_to_back(void)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pFront;
    tBitMap *pBack;

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pFront || !pBfr->pBack) {
        return 0;
    }
    pFront = pBfr->pFront;
    pBack = pBfr->pBack;
    if (pFront == pBack) {
        return 0;
    }
    if (!bitmapIsChip(pFront) || !bitmapIsChip(pBack)) {
        return 0;
    }
    blitCopy(pFront, 0, 0, pBack, 0, 0, (WORD)MM2_AGA_SCREEN_WIDTH, (WORD)MM2_AGA_SCREEN_HEIGHT,
             MINTERM_COPY);
    return 1;
}

static void mm2_amiga_write_asset_palette(const mm2_image32_file *img)
{
    tVPort *pVp;
    ULONG *pPal;
    int i;

    if (!img) {
        return;
    }

    pVp = mm2AmigaDisplayGetVPort();
    if (!pVp || !pVp->pPalette) {
        return;
    }

    pPal = (ULONG *)pVp->pPalette;
    pPal[0] = 0;
    for (i = 1; i < MM2_IMAGE32_PALETTE_COLORS; ++i) {
        pPal[i] = img->palette_pen[i];
    }

    s_applied_palette_img = img;
}

void mm2_amiga_stage_asset_palette(const mm2_image32_file *img)
{
    if (!img || img == s_applied_palette_img) {
        return;
    }
    mm2_amiga_write_asset_palette(img);
}

void mm2_amiga_apply_palette(const mm2_image32_file *img)
{
    if (!img || img == s_applied_palette_img) {
        return;
    }
    mm2_amiga_write_asset_palette(img);
    s_palette_dirty = 1;
    mm2_amiga_push_palette();
}

void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY,
                          UBYTE opaque)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;
    const mm2_image32_frame *frame;
    tBitMap *pSrc;
    tBitMap *pMsk;
    WORD w;
    WORD h;

    if (!img || !img->frames || frame_index >= img->frame_count) {
        return;
    }

    frame = &img->frames[frame_index];
    pSrc = (tBitMap *)frame->bitmap;
    pMsk = (tBitMap *)frame->mask;
    if (!pSrc) {
        return;
    }

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(pSrc) || !bitmapIsChip(pDst)) {
#if defined(ACE_DEBUG)
        logWrite(
            "MM2: blit SKIP %s: src chip=%u (%p) dst chip=%u (%p)\n",
            img->debug_label[0] ? img->debug_label : "?",
            (unsigned)bitmapIsChip(pSrc), pSrc->Planes[0],
            (unsigned)bitmapIsChip(pDst), pDst->Planes[0]
        );
#endif
        return;
    }

    mm2_amiga_apply_palette(img);
    w = (WORD)frame->width;
    h = (WORD)frame->height;
    if (uwDstX + (UWORD)w > MM2_AGA_SCREEN_WIDTH) {
        w = (WORD)(MM2_AGA_SCREEN_WIDTH - uwDstX);
    }

    if (uwDstY + (UWORD)h > MM2_AGA_SCREEN_HEIGHT) {
        h = (WORD)(MM2_AGA_SCREEN_HEIGHT - uwDstY);
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (opaque) {
        blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, MINTERM_COPY);
    } else if (pMsk && pMsk->Planes[0]) {
        blitCopyMask(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, pMsk->Planes[0]);
    }
}

void mm2_amiga_apply_anm_palette(const uint16_t palette_words[MM2_ANM_PALETTE_COLORS])
{
    tVPort *pVp;
    ULONG *pPal;
    int i;

    if (!palette_words) {
        return;
    }

    pVp = mm2AmigaDisplayGetVPort();
    if (!pVp || !pVp->pPalette) {
        return;
    }

    pPal = (ULONG *)pVp->pPalette;
    pPal[0] = 0;
    for (i = 1; i < MM2_ANM_PALETTE_COLORS; ++i) {
        const uint16_t w = palette_words[i];
        const uint8_t r = (uint8_t)(((w >> 8) & 0x0F) * 17);
        const uint8_t g = (uint8_t)(((w >> 4) & 0x0F) * 17);
        const uint8_t b = (uint8_t)((w & 0x0F) * 17);
        pPal[i] = ((ULONG)r << 16) | ((ULONG)g << 8) | (ULONG)b;
    }

    s_applied_palette_img = NULL;
    s_palette_dirty = 1;
}

void mm2_amiga_blit_anm_composed(const mm2_anm_composite_planar *img, UWORD uwDstX, UWORD uwDstY)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;
    tBitMap *pSrc;
    tBitMap *pMsk;
    WORD w;
    WORD h;

    if (!img || !img->bitmap) {
        return;
    }

    pSrc = (tBitMap *)img->bitmap;
    pMsk = (tBitMap *)img->mask;
    if (!pSrc) {
        return;
    }

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(pSrc) || !bitmapIsChip(pDst)) {
        return;
    }

    /* When the composite remapped its pens to the live env palette, leave pens
     * 0-31 alone (walls keep their stone colours); otherwise load .anm pens. */
    if (!img->map_to_host_palette) {
        mm2_amiga_apply_anm_palette(img->palette_words);
    }
    w = (WORD)img->width;
    h = (WORD)img->height;
    if (uwDstX + (UWORD)w > MM2_AGA_SCREEN_WIDTH) {
        w = (WORD)(MM2_AGA_SCREEN_WIDTH - uwDstX);
    }
    if (uwDstY + (UWORD)h > MM2_AGA_SCREEN_HEIGHT) {
        h = (WORD)(MM2_AGA_SCREEN_HEIGHT - uwDstY);
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    if (pMsk && pMsk->Planes[0]) {
        blitCopyMask(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, pMsk->Planes[0]);
    }
}

/* Release -flto inlines this into mm2AmigaDisplayFrameEnd after vPortWaitForEnd;
 * the wait loop leaves a stale a2, so viewUpdateGlobalPalette reads garbage. */
__attribute__((noinline)) void mm2_amiga_push_palette(void)
{
    tView *pView;
    if (!s_palette_dirty) {
        return;
    }
    pView = mm2AmigaDisplayGetVPort() ? mm2AmigaDisplayGetVPort()->pView : NULL;
    if (!pView) {
        s_palette_dirty = 0;
        return;
    }
    viewUpdateGlobalPalette(pView);
    s_palette_dirty = 0;
}

void mm2_amiga_present_end(void)
{
    /* Palette + buffer swap happen in mm2AmigaDisplayFrameEnd(). */
}

#endif /* AMIGA */
