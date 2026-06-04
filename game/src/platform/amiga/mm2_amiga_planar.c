/**
 * Present .32 assets on the ACE playfield — planar blit only (no RGBA / chunky).
 */

#include "mm2/platform/amiga/Mm2AmigaPlanar.h"

#ifdef AMIGA

#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"

#include <mini_std/string.h>

#include <ace/managers/blit.h>
#include <ace/managers/log.h>
#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>

void mm2_amiga_clear_screen(void)
{
    tSimpleBufferManager *pBfr = mm2AmigaDisplayGetBuffer();
    tBitMap *pDst;
    UBYTE ubPlane;

    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    for (ubPlane = 0; ubPlane < pDst->Depth; ++ubPlane) {
        memset(pDst->Planes[ubPlane], 0, (size_t)pDst->Rows * (size_t)pDst->BytesPerRow);
    }
}

void mm2_amiga_apply_palette(const mm2_image32_file *img)
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
    /* AGA 6bpp: pens 32–63 unused by .32 assets — leave or mirror low pens. */
    for (i = MM2_IMAGE32_PALETTE_COLORS; i < (int)MM2_AGA_PALETTE_PENS; ++i) {
        pPal[i] = pPal[i - MM2_IMAGE32_PALETTE_COLORS + 1];
    }
}

void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;
    const mm2_image32_frame *frame;
    tBitMap *pSrc;

    if (!img || !img->frames || frame_index >= img->frame_count) {
        return;
    }
    frame = &img->frames[frame_index];
    pSrc = (tBitMap *)frame->bitmap;
    if (!pSrc) {
        return;
    }

    mm2_amiga_apply_palette(img);

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;

#ifdef ACE_DEBUG
    if (pSrc->Depth != pDst->Depth) {
        logWrite("MM2 blit: depth mismatch src=%u dst=%u (%ux%u)\n", pSrc->Depth, pDst->Depth,
                 frame->width, frame->height);
    }
#endif
    blitCopy(
        pSrc, 0, 0,
        pDst, uwDstX, uwDstY,
        (WORD)frame->width, (WORD)frame->height,
        MINTERM_COOKIE);
}

#endif /* AMIGA */
