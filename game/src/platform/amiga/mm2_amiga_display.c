/**
 * AGA 6bpp playfield — patterned on LandsOfLore src/misc/screen.c (CreateNewScreenWithBounds).
 * LoL uses TAG_VPORT_BPP 8 (256-colour chunky); MM2 uses 6 (64 pens per 41-aga-port-plan.md).
 * ACE extview sets BPLCON2 KillEHB when ubBpp == 6 on AGA (see ACE src/ace/utils/extview.c).
 */

#include "mm2/platform/amiga/Mm2AmigaDisplay.h"

#ifdef AMIGA

#include <mini_std/stdlib.h>

#include <ace/managers/copper.h>
#include <ace/managers/memory.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/extview.h>

static Mm2AmigaDisplay s_display;

static void mm2AmigaDisplayInitPalette(tVPort *pVp)
{
    ULONG *pPal;
    UBYTE i;

    if (!pVp || !pVp->pPalette) {
        return;
    }
    pPal = (ULONG *)pVp->pPalette;
    pPal[0] = 0x00000000UL;
    for (i = 1; i < MM2_AGA_PALETTE_PENS; ++i) {
        const UBYTE v = (UBYTE)((i * 255u) / (MM2_AGA_PALETTE_PENS - 1u));
        pPal[i] = ((ULONG)v << 16) | ((ULONG)v << 8) | (ULONG)v;
    }
}

UBYTE mm2AmigaDisplayCreate(UWORD uwWidth, UWORD uwHeight, UBYTE ubDoubleBuffer)
{
    mm2AmigaDisplayDispose();

    s_display.pView = viewCreate(
        0,
        MM2_AGA_VIEW_TAG_GLOBAL_PALETTE, 1,
        MM2_AGA_VIEW_TAG_USES_AGA, 1,
        TAG_END);
    if (!s_display.pView) {
        return 0;
    }

    s_display.pVp = vPortCreate(
        0,
        MM2_AGA_VPORT_TAG_BPP, MM2_AGA_SCREEN_BPP,
        TAG_VPORT_VIEW, s_display.pView,
        MM2_AGA_VPORT_TAG_USES_AGA, 1,
        MM2_AGA_VPORT_TAG_FMODE, 1,
        TAG_VPORT_HEIGHT, uwHeight,
        TAG_END);
    if (!s_display.pVp) {
        viewDestroy(s_display.pView);
        s_display.pView = NULL;
        return 0;
    }

    s_display.pBfr = simpleBufferCreate(
        0,
        TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR,
        TAG_SIMPLEBUFFER_VPORT, s_display.pVp,
        TAG_SIMPLEBUFFER_IS_DBLBUF, ubDoubleBuffer ? 1 : 0,
        TAG_SIMPLEBUFFER_USE_X_SCROLLING, 0,
        TAG_SIMPLEBUFFER_BOUND_WIDTH, uwWidth,
        TAG_SIMPLEBUFFER_BOUND_HEIGHT, uwHeight,
        TAG_END);
    if (!s_display.pBfr) {
        viewDestroy(s_display.pView);
        s_display.pView = NULL;
        s_display.pVp = NULL;
        return 0;
    }

    mm2AmigaDisplayInitPalette(s_display.pVp);
    return 1;
}

void mm2AmigaDisplayActivate(void)
{
    if (s_display.pView) {
        viewLoad(s_display.pView);
        viewUpdateGlobalPalette(s_display.pView);
    }
}

void mm2AmigaDisplayFrameEnd(void)
{
    if (!s_display.pView || !s_display.pVp) {
        return;
    }
    viewProcessManagers(s_display.pView);
    copProcessBlocks();
    vPortWaitForEnd(s_display.pVp);
    viewUpdateGlobalPalette(s_display.pView);
}

void mm2AmigaDisplayDispose(void)
{
    if (s_display.pBfr) {
        simpleBufferDestroy(s_display.pBfr);
        s_display.pBfr = NULL;
    }
    if (s_display.pView) {
        viewDestroy(s_display.pView);
        s_display.pView = NULL;
        s_display.pVp = NULL;
    }
}

Mm2AmigaDisplay *mm2AmigaDisplayGet(void) { return &s_display; }

tSimpleBufferManager *mm2AmigaDisplayGetBuffer(void) { return s_display.pBfr; }

tVPort *mm2AmigaDisplayGetVPort(void) { return s_display.pVp; }

#endif /* AMIGA */
