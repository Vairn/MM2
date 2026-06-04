/**
 * Present .32 assets on the ACE playfield — planar blit only (no RGBA / chunky).
 */

#include "mm2/platform/amiga/Mm2AmigaPlanar.h"

#ifdef AMIGA

#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"

#include <mini_std/string.h>

#include "mm2/Mm2Dbg.h"
#include <ace/managers/blit.h>
#include <ace/utils/bitmap.h>
#include <ace/utils/custom.h>
#include <ace/utils/extview.h>

#ifdef ACE_DEBUG
static void mm2_amiga_dbg_dump_palette(const char *tag, const ULONG *pPal, UWORD uwCount)
{
    UWORD i;
    UWORD end;
    if (!tag || !pPal || uwCount == 0) {
        return;
    }
    logWrite("MM2: palette dump [%s] %u pens vp=%p\n", tag, (unsigned)uwCount, pPal);
    for (i = 0; i < uwCount; i += 8) {
        end = (UWORD)((i + 8u < uwCount) ? (i + 8u) : uwCount);
        logWrite(
            "MM2: pal[%s] %02u-%02u:"
            " %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX\n",
            tag, (unsigned)i, (unsigned)(end - 1u),
            (unsigned long)((i + 0u < uwCount) ? (pPal[i + 0u] & 0xFFFFFFUL) : 0UL),
            (unsigned long)((i + 1u < uwCount) ? (pPal[i + 1u] & 0xFFFFFFUL) : 0UL),
            (unsigned long)((i + 2u < uwCount) ? (pPal[i + 2u] & 0xFFFFFFUL) : 0UL),
            (unsigned long)((i + 3u < uwCount) ? (pPal[i + 3u] & 0xFFFFFFUL) : 0UL),
            (unsigned long)((i + 4u < uwCount) ? (pPal[i + 4u] & 0xFFFFFFUL) : 0UL),
            (unsigned long)((i + 5u < uwCount) ? (pPal[i + 5u] & 0xFFFFFFUL) : 0UL),
            (unsigned long)((i + 6u < uwCount) ? (pPal[i + 6u] & 0xFFFFFFUL) : 0UL),
            (unsigned long)((i + 7u < uwCount) ? (pPal[i + 7u] & 0xFFFFFFUL) : 0UL)
        );
    }
}

static void mm2_amiga_dbg_dump_hw_colors(const char *tag)
{
    UWORD i;
    if (!tag) {
        return;
    }
    logWrite("MM2: hw COLOR dump [%s]\n", tag);
    for (i = 0; i < 32; i += 8) {
        logWrite(
            "MM2: hw[%s] %02u-%02u:"
            " %03X %03X %03X %03X %03X %03X %03X %03X\n",
            tag, (unsigned)i, (unsigned)(i + 7u),
            (unsigned)(g_pCustom->color[i + 0] & 0xFFF),
            (unsigned)(g_pCustom->color[i + 1] & 0xFFF),
            (unsigned)(g_pCustom->color[i + 2] & 0xFFF),
            (unsigned)(g_pCustom->color[i + 3] & 0xFFF),
            (unsigned)(g_pCustom->color[i + 4] & 0xFFF),
            (unsigned)(g_pCustom->color[i + 5] & 0xFFF),
            (unsigned)(g_pCustom->color[i + 6] & 0xFFF),
            (unsigned)(g_pCustom->color[i + 7] & 0xFFF)
        );
    }
}

void mm2_amiga_dbg_dump_vport_palette(const char *tag)
{
    tVPort *pVp = mm2AmigaDisplayGetVPort();
    if (pVp && pVp->pPalette) {
        mm2_amiga_dbg_dump_palette(tag, (const ULONG *)pVp->pPalette, MM2_AGA_PALETTE_PENS);
    }
}

void mm2_amiga_dbg_dump_hw_palette(const char *tag)
{
    mm2_amiga_dbg_dump_hw_colors(tag);
}
#endif /* ACE_DEBUG */

static const mm2_image32_file *s_applied_palette_img;

static UBYTE s_palette_dirty;

/**
 * Retail ECS .32 → AGA viewport palette.
 * Art only defines 32 pens; playfield may use fewer or more bitplanes on AGA hardware.
 * ACE viewUpdateGlobalPalette pushes (1<<bpp)/32 banks of 32 ULONGs each; when our
 * playfield is 6bpp that is two banks, so mirror vp[0..31] → vp[32..63]. A 4bpp AGA
 * screen would still use AGA COLOR regs but only needs one bank (no mirror).
 */
static void mm2_amiga_remap_ecs_to_aga(ULONG *pPal)
{
    int i;

    if (!pPal || MM2_AGA_PALETTE_PENS <= MM2_IMAGE32_PALETTE_COLORS) {
        return;
    }
    for (i = 0; i < MM2_IMAGE32_PALETTE_COLORS; ++i) {
        pPal[MM2_IMAGE32_PALETTE_COLORS + i] = pPal[i];
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
    pPal[1] = 0x00FFFFFFUL;
    pPal[2] = 0x00FF0000UL;
    mm2_amiga_remap_ecs_to_aga(pPal);
    s_applied_palette_img = NULL;
    s_palette_dirty = 1;
    MM2_DBG("MM2 DBG: SetPalette UI (menu text pens)\n");
#ifdef ACE_DEBUG
    mm2_amiga_dbg_dump_palette("ui", pPal, MM2_AGA_PALETTE_PENS);
#endif
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
    mm2_amiga_remap_ecs_to_aga(pPal);

    s_applied_palette_img = img;
    s_palette_dirty = 1;
    MM2_DBG("MM2 DBG: SetPalette %s\n", img->debug_label[0] ? img->debug_label : "?");
#ifdef ACE_DEBUG
    {
        const char *lbl = img->debug_label[0] ? img->debug_label : "asset";
        int j;
        logWrite("MM2: ECS src palette [%s] pens 0-31 (0RGB sheet):\n", lbl);
        for (j = 0; j < MM2_IMAGE32_PALETTE_COLORS; j += 8) {
            logWrite(
                "MM2: src[%s] %02d-%02d:"
                " %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX\n",
                lbl, j, j + 7,
                (unsigned long)(img->palette_pen[j + 0] & 0xFFFFFFUL),
                (unsigned long)(img->palette_pen[j + 1] & 0xFFFFFFUL),
                (unsigned long)(img->palette_pen[j + 2] & 0xFFFFFFUL),
                (unsigned long)(img->palette_pen[j + 3] & 0xFFFFFFUL),
                (unsigned long)(img->palette_pen[j + 4] & 0xFFFFFFUL),
                (unsigned long)(img->palette_pen[j + 5] & 0xFFFFFFUL),
                (unsigned long)(img->palette_pen[j + 6] & 0xFFFFFFUL),
                (unsigned long)(img->palette_pen[j + 7] & 0xFFFFFFUL)
            );
        }
        mm2_amiga_dbg_dump_palette("aga-vp-bank0", pPal, MM2_IMAGE32_PALETTE_COLORS);
        mm2_amiga_dbg_dump_palette("aga-vp-bank1", pPal + MM2_IMAGE32_PALETTE_COLORS,
                                   MM2_IMAGE32_PALETTE_COLORS);
    }
#endif

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

    /* ECS .32 = 5bp; playfield = 6bp. ACE blitCopy uses MIN(src,dst) planes (bp5 stays 0). */
    (void)opaque;
    ubMinterm = MINTERM_COPY;

#ifdef ACE_DEBUG
    if (!blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, ubMinterm)) {
        logWrite(
            "MM2: blitCopy FAIL %s %dx%d@%d,%d src %hhu(Ecs) dst %hhu(AGA)\n",
            img->debug_label[0] ? img->debug_label : "?",
            (int)w, (int)h, (int)uwDstX, (int)uwDstY, pSrc->Depth, pDst->Depth
        );
    } else {
        MM2_DBG(
            "MM2 DBG: blit ok %s %dx%d@%u,%u %s\n",
            img->debug_label[0] ? img->debug_label : "?",
            (int)w, (int)h, (unsigned)uwDstX, (unsigned)uwDstY,
            "COPY"
        );
    }
#else
    blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, ubMinterm);
#endif

}

void mm2_amiga_push_palette(void)
{
    tView *pView;
    if (!s_palette_dirty) {
#ifdef ACE_DEBUG
        static UBYTE s_logged_push_skip;
        if (!s_logged_push_skip) {
            s_logged_push_skip = 1;
            MM2_DBG("MM2 DBG: PushPalette skip (not dirty)\n");
        }
#endif
        return;
    }
    pView = mm2AmigaDisplayGetVPort() ? mm2AmigaDisplayGetVPort()->pView : NULL;
    if (!pView) {
        s_palette_dirty = 0;
        return;
    }
    MM2_DBG("MM2 DBG: PushPalette begin\n");
#ifdef ACE_DEBUG
    {
        tVPort *pVp = mm2AmigaDisplayGetVPort();
        if (pVp && pVp->pPalette) {
            mm2_amiga_dbg_dump_palette("pre-push", (const ULONG *)pVp->pPalette, MM2_AGA_PALETTE_PENS);
        }
    }
#endif
    viewUpdateGlobalPalette(pView);
    s_palette_dirty = 0;
    MM2_DBG("MM2 DBG: PushPalette done\n");
#ifdef ACE_DEBUG
    mm2_amiga_dbg_dump_hw_colors("post-push");
#endif
}

void mm2_amiga_present_end(void)
{
    /* Intentionally empty: no blitWait() (async blits; bad COOKIE can hang BLTDONE).
     * Palette + buffer swap happen in mm2AmigaDisplayFrameEnd(). */
}

#endif /* AMIGA */
