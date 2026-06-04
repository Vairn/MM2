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



static const mm2_image32_file *s_applied_palette_img;

static UBYTE s_palette_dirty;

void mm2_amiga_apply_ui_palette(void)
{
    tVPort *pVp;
    ULONG *pPal;
    int i;

    pVp = mm2AmigaDisplayGetVPort();
    if (!pVp || !pVp->pPalette) {
        return;
    }
    pPal = (ULONG *)pVp->pPalette;
    pPal[0] = 0;
    pPal[1] = 0x00FFFFFFUL;
    pPal[2] = 0x00FF0000UL;
    for (i = MM2_IMAGE32_PALETTE_COLORS; i < (int)MM2_AGA_PALETTE_PENS; ++i) {
        pPal[i] = pPal[i - MM2_IMAGE32_PALETTE_COLORS + 1];
    }
    s_applied_palette_img = NULL;
    s_palette_dirty = 1;
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

    for (i = MM2_IMAGE32_PALETTE_COLORS; i < (int)MM2_AGA_PALETTE_PENS; ++i) {

        pPal[i] = pPal[i - MM2_IMAGE32_PALETTE_COLORS + 1];

    }

    s_applied_palette_img = img;

    s_palette_dirty = 1;

}



void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY,

                          UBYTE opaque)

{

    tSimpleBufferManager *pBfr;

    tBitMap *pDst;

    const mm2_image32_frame *frame;

    tBitMap *pSrc;

    WORD w;

    WORD h;

    UBYTE ubMinterm;



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



    ubMinterm = opaque ? MINTERM_COPY : MINTERM_COOKIE;

#ifdef ACE_DEBUG
    if (!blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, ubMinterm)) {
        logWrite(
            "MM2: blitCopy FAIL %dx%d@%d,%d src %hhu bpp dst %hhu bpp\n",
            (int)w, (int)h, (int)uwDstX, (int)uwDstY, pSrc->Depth, pDst->Depth
        );
    }
#else
    blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, ubMinterm);
#endif

}

void mm2_amiga_present_end(void)

{

    tView *pView;



    blitWait();

    if (!s_palette_dirty) {

        return;

    }

    pView = mm2AmigaDisplayGetVPort() ? mm2AmigaDisplayGetVPort()->pView : NULL;

    if (pView) {

        viewUpdateGlobalPalette(pView);

    }

    s_palette_dirty = 0;

}



#endif /* AMIGA */

