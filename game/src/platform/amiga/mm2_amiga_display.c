/**
 * AGA playfield via ACE — patterned on LandsOfLore src/misc/screen.c.
 * AGA palette hardware applies at any bpp (4bp on AGA is still AGA palettes).
 * MM2 uses TAG_VPORT_BPP 6 (64 indices) per 41-aga-port-plan.md; retail .32 is ECS on disk.
 * ACE extview sets BPLCON2 KillEHB when ubBpp == 6 on AGA (see ACE src/ace/utils/extview.c).
 */

#include "mm2/platform/amiga/Mm2AmigaDisplay.h"
#include "mm2/Mm2Dbg.h"
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#include "mm2_image32_codec.h"
#include "mm2/platform/amiga/mm2_fade.h"

#ifdef AMIGA

#include <mini_std/stdlib.h>
#include <mini_std/string.h>

#include <ace/generic/screen.h>
#include <ace/managers/copper.h>
#include <ace/managers/memory.h>
#include <ace/managers/system.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>
#include <ace/utils/palette.h>

#ifdef ACE_DEBUG
#include <ace/managers/log.h>
#endif

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
    for (i = 1; i < MM2_IMAGE32_PALETTE_COLORS; ++i) {
        const UBYTE v = (UBYTE)((i * 255u) / (MM2_IMAGE32_PALETTE_COLORS - 1u));
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
        TAG_VIEW_WINDOW_HEIGHT, (ULONG)uwHeight,
        TAG_END);
    if (!s_display.pView) {
        return 0;
    }

    /* ACE tagGet only reads ULONG tag values; ensure view window matches VPort (LoL bounds). */
    if (systemIsPal()) {
        s_display.pView->ubPosY = SCREEN_PAL_YOFFSET + (SCREEN_PAL_HEIGHT - uwHeight) / 2;
    } else {
        s_display.pView->ubPosY = SCREEN_NTSC_YOFFSET;
    }
    s_display.pView->uwHeight = uwHeight;

    s_display.pVp = vPortCreate(
        0,
        MM2_AGA_VPORT_TAG_BPP, MM2_AGA_SCREEN_BPP,
        TAG_VPORT_VIEW, s_display.pView,
        MM2_AGA_VPORT_TAG_USES_AGA, 1,
        MM2_AGA_VPORT_TAG_FMODE, MM2_AGA_VPORT_FMODE_VALUE,
        TAG_VPORT_HEIGHT, (ULONG)uwHeight,
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
#ifdef ACE_DEBUG
    {
        const ULONG ulPlane = (ULONG)(uwWidth / 8) * (ULONG)uwHeight;
        const ULONG ulPerBfr = ulPlane * (ULONG)MM2_AGA_SCREEN_BPP;
        logWrite(
            "MM2: playfield %ux%u %ubpp buffers=%u chip~%lu (2x~%lu)\n",
            (unsigned)uwWidth, (unsigned)uwHeight, (unsigned)MM2_AGA_SCREEN_BPP,
            (unsigned)(ubDoubleBuffer ? 2u : 1u), (unsigned long)ulPerBfr,
            (unsigned long)(ubDoubleBuffer ? ulPerBfr * 2u : ulPerBfr)
        );
        logWrite(
            "MM2: playfield DB front=%p back=%p same=%u chip free %lu\n",
            s_display.pBfr->pFront, s_display.pBfr->pBack,
            (unsigned)(s_display.pBfr->pFront == s_display.pBfr->pBack),
            (unsigned long)memGetFreeChipSize()
        );
    }
#endif

    mm2AmigaDisplayInitPalette(s_display.pVp);
    s_display.pFade = mm2_fade_create(
        s_display.pView, s_display.pVp->pPalette, MM2_AGA_PALETTE_PENS);
    if (s_display.pView) {
        viewUpdateGlobalPalette(s_display.pView);
    }
    return 1;
}

void mm2AmigaDisplayActivate(void)
{
    if (s_display.pView) {
        viewLoad(s_display.pView);
        viewUpdateGlobalPalette(s_display.pView);
        MM2_DBG("MM2 DBG: viewLoad (activate)\n");
    }
}

void mm2AmigaDisplayUnload(void)
{
    viewLoad(0);
}

void mm2AmigaDisplayFrameEnd(void)
{
    if (!s_display.pView || !s_display.pVp) {
        return;
    }
    if (s_display.pFade) {
        mm2_fade_process(s_display.pFade);
    }
    /* ACE simpleBuffer: updates copper to pBack, then swaps pBack/pFront. */
    viewProcessManagers(s_display.pView);
    copProcessBlocks();
    vPortWaitForEnd(s_display.pVp);
    mm2_amiga_push_palette();
}

void mm2AmigaDisplayWaitVblank(void)
{
    if (!s_display.pVp) {
        return;
    }
    vPortWaitForEnd(s_display.pVp);
}

void mm2AmigaDisplayDispose(void)
{
    if (!s_display.pView) {
        return;
    }
    mm2AmigaDisplayUnload();
    if (s_display.pFade) {
        mm2_fade_destroy(s_display.pFade);
        s_display.pFade = NULL;
    }
    if (s_display.pBfr) {
        simpleBufferDestroy(s_display.pBfr);
        s_display.pBfr = NULL;
    }
    viewDestroy(s_display.pView);
    s_display.pView = NULL;
    s_display.pVp = NULL;
}

void mm2AmigaFadeCapturePalette(void)
{
    ULONG *pRef;
    ULONG *pVpPal;
    UBYTE i;

    if (!s_display.pFade || !s_display.pVp || !s_display.pVp->pPalette) {
        return;
    }
    pRef = (ULONG *)s_display.pFade->pPaletteRef;
    pVpPal = (ULONG *)s_display.pVp->pPalette;
    if (!pRef || !pVpPal) {
        return;
    }
    for (i = 0; i < s_display.pFade->ubColorCount; ++i) {
        pRef[i] = pVpPal[i];
    }
}

void mm2AmigaFadeBeginIn(UBYTE ubFrames)
{
    if (!s_display.pFade || !s_display.pVp || !s_display.pView) {
        return;
    }
    mm2_fade_set(s_display.pFade, MM2_FADE_STATE_IN, ubFrames, NULL);
#ifdef ACE_USE_AGA_FEATURES
    paletteDimAga(
        (ULONG *)s_display.pFade->pPaletteRef,
        (ULONG *)s_display.pVp->pPalette,
        s_display.pFade->ubColorCount, 0);
#else
    paletteDimOcs(
        (UWORD *)s_display.pFade->pPaletteRef,
        (volatile UWORD *)s_display.pVp->pPalette,
        s_display.pFade->ubColorCount, 0);
#endif
    viewUpdateGlobalPalette(s_display.pView);
}

void mm2AmigaFadeBeginOut(UBYTE ubFrames)
{
    if (s_display.pFade) {
        mm2_fade_set(s_display.pFade, MM2_FADE_STATE_OUT, ubFrames, NULL);
    }
}

UBYTE mm2AmigaFadeConsumeDone(void)
{
    if (!s_display.pFade || s_display.pFade->eState != MM2_FADE_STATE_EVENT_FIRED) {
        return 0;
    }
    s_display.pFade->eState = MM2_FADE_STATE_IDLE;
    return 1;
}

void mm2AmigaFadeCancel(void)
{
    UBYTE i;

    if (!s_display.pFade || !s_display.pVp || !s_display.pView) {
        return;
    }
    if (s_display.pFade->eState == MM2_FADE_STATE_IDLE
        || s_display.pFade->eState == MM2_FADE_STATE_EVENT_FIRED) {
        s_display.pFade->eState = MM2_FADE_STATE_IDLE;
        return;
    }
#ifdef ACE_USE_AGA_FEATURES
    {
        ULONG *pRef = (ULONG *)s_display.pFade->pPaletteRef;
        ULONG *pVpPal = (ULONG *)s_display.pVp->pPalette;
        if (pRef && pVpPal) {
            for (i = 0; i < s_display.pFade->ubColorCount; ++i) {
                pVpPal[i] = pRef[i];
            }
        }
    }
#else
    {
        UWORD *pRef = (UWORD *)s_display.pFade->pPaletteRef;
        UWORD *pVpPal = (UWORD *)s_display.pVp->pPalette;
        if (pRef && pVpPal) {
            for (i = 0; i < s_display.pFade->ubColorCount; ++i) {
                pVpPal[i] = pRef[i];
            }
        }
    }
#endif
    viewUpdateGlobalPalette(s_display.pView);
    s_display.pFade->eState = MM2_FADE_STATE_IDLE;
    s_display.pFade->ubCnt = 0;
}

Mm2AmigaDisplay *mm2AmigaDisplayGet(void) { return &s_display; }

tSimpleBufferManager *mm2AmigaDisplayGetBuffer(void) { return s_display.pBfr; }

tVPort *mm2AmigaDisplayGetVPort(void) { return s_display.pVp; }

#endif /* AMIGA */
