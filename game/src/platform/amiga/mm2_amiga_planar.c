/**
 * Present .32 assets on the ACE playfield — planar blit only (no RGBA / chunky).
 */

#include "mm2/platform/amiga/Mm2AmigaPlanar.h"

#ifdef AMIGA

#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"

#include <mini_std/string.h>

#include <ace/managers/blit.h>
#ifdef ACE_DEBUG
#include <ace/managers/log.h>
#endif
#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>

static const mm2_image32_file *s_applied_palette_img;

static UBYTE s_palette_dirty;

void mm2_amiga_push_palette(void);

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
    pPal[1] = 0x00FFFFFFUL;
    pPal[2] = 0x00FF0000UL;
    s_applied_palette_img = NULL;
    s_palette_dirty = 1;
    mm2_amiga_push_palette();
}

void mm2_amiga_put_pixel_rgb(UWORD uwX, UWORD uwY, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;
    UBYTE pen;
    UBYTE pl;
    UWORD bpr;
    if (ubA == 0 || uwX >= MM2_AGA_SCREEN_WIDTH || uwY >= MM2_AGA_SCREEN_HEIGHT) {
        return;
    }
    if (ubR > 200 && ubG < 80 && ubB < 80) {
        pen = 2;
    } else if (ubR > 160 || ubG > 160 || ubB > 160) {
        pen = 1;
    } else {
        pen = 0;
    }

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
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

void mm2_amiga_clear_screen(void)
{
    tSimpleBufferManager *pBfr = mm2AmigaDisplayGetBuffer();
    tBitMap *pDst;
    UBYTE pl;
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(pDst)) {
        return;
    }
    for (pl = 0; pl < pDst->Depth; ++pl) {
        memset(pDst->Planes[pl], 0, (size_t)pDst->Rows * (size_t)pDst->BytesPerRow);
    }
}

void mm2_amiga_apply_palette(const mm2_image32_file *img)
{
    tVPort *pVp;
    ULONG *pPal;
    int i;

    if (!img || img == s_applied_palette_img) {
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
#ifdef ACE_DEBUG
        if (!blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, MINTERM_COPY)) {
            logWrite(
                "MM2: blitCopy FAIL %s %dx%d@%d,%d\n",
                img->debug_label[0] ? img->debug_label : "?",
                (int)w, (int)h, (int)uwDstX, (int)uwDstY
            );
        }
#else
        blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, MINTERM_COPY);
#endif
    } else if (pMsk && pMsk->Planes[0]) {
#ifdef ACE_DEBUG
        if (!blitCopyMask(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, pMsk->Planes[0])) {
            logWrite(
                "MM2: blitCopyMask FAIL %s %dx%d@%d,%d\n",
                img->debug_label[0] ? img->debug_label : "?",
                (int)w, (int)h, (int)uwDstX, (int)uwDstY
            );
        }
#else
        blitCopyMask(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, pMsk->Planes[0]);
#endif
    }

}

void mm2_amiga_push_palette(void)
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
    /* Intentionally empty: no blitWait() (async blits; bad COOKIE can hang BLTDONE).
     * Palette + buffer swap happen in mm2AmigaDisplayFrameEnd(). */
}

#endif /* AMIGA */
