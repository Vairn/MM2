/**
 * AGA 6bpp playfield — patterned on LandsOfLore src/misc/screen.c (CreateNewScreenWithBounds).
 * LoL uses TAG_VPORT_BPP 8 (256-colour chunky); MM2 uses 6 (64 pens per 41-aga-port-plan.md).
 * ACE extview sets BPLCON2 KillEHB when ubBpp == 6 on AGA (see ACE src/ace/utils/extview.c).
 */

#include "mm2/platform/amiga/Mm2AmigaDisplay.h"
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

#ifdef ACE_DEBUG
#include <ace/managers/log.h>
#endif

static Mm2AmigaDisplay s_display;

/* When TAG_SIMPLEBUFFER_IS_DBLBUF is set, AmigaPorts ACE only allocates a second
 * bitmap if pBack already equals pFront at create time; with the default tag
 * list pBack is still NULL so we get a single buffer while the camera is DB. */
static tBitMap *s_pOwnedFront;
static tBitMap *s_pOwnedBack;

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

    s_pOwnedFront = NULL;
    s_pOwnedBack = NULL;

    if (ubDoubleBuffer) {
        s_pOwnedFront = bitmapCreate(uwWidth, uwHeight, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
        s_pOwnedBack = bitmapCreate(uwWidth, uwHeight, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
        if (!s_pOwnedFront || !s_pOwnedBack) {
#ifdef ACE_DEBUG
            logWrite("MM2: ERR playfield bitmap alloc failed\n");
#endif
            if (s_pOwnedFront) {
                bitmapDestroy(s_pOwnedFront);
                s_pOwnedFront = NULL;
            }
            if (s_pOwnedBack) {
                bitmapDestroy(s_pOwnedBack);
                s_pOwnedBack = NULL;
            }
            viewDestroy(s_display.pView);
            s_display.pView = NULL;
            s_display.pVp = NULL;
            return 0;
        }
        s_display.pBfr = simpleBufferCreate(
            0,
            TAG_SIMPLEBUFFER_FRONT_BITMAP, s_pOwnedFront,
            TAG_SIMPLEBUFFER_BACK_BITMAP, s_pOwnedBack,
            TAG_SIMPLEBUFFER_VPORT, s_display.pVp,
            TAG_SIMPLEBUFFER_IS_DBLBUF, 1,
            TAG_SIMPLEBUFFER_USE_X_SCROLLING, 0,
            TAG_SIMPLEBUFFER_BOUND_WIDTH, uwWidth,
            TAG_SIMPLEBUFFER_BOUND_HEIGHT, uwHeight,
            TAG_END);
    } else {
        s_display.pBfr = simpleBufferCreate(
            0,
            TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR,
            TAG_SIMPLEBUFFER_VPORT, s_display.pVp,
            TAG_SIMPLEBUFFER_IS_DBLBUF, 0,
            TAG_SIMPLEBUFFER_USE_X_SCROLLING, 0,
            TAG_SIMPLEBUFFER_BOUND_WIDTH, uwWidth,
            TAG_SIMPLEBUFFER_BOUND_HEIGHT, uwHeight,
            TAG_END);
    }
    if (!s_display.pBfr) {
        if (s_pOwnedFront) {
            bitmapDestroy(s_pOwnedFront);
            s_pOwnedFront = NULL;
        }
        if (s_pOwnedBack) {
            bitmapDestroy(s_pOwnedBack);
            s_pOwnedBack = NULL;
        }
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
    /* Logo/title fades disabled while validating planar blits (mm2_fade_process dims palette). */
    /* ACE simpleBuffer: updates copper to pBack, then swaps pBack/pFront. */
    viewProcessManagers(s_display.pView);
    copProcessBlocks();
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
    viewDestroy(s_display.pView);
    s_display.pView = NULL;
    s_display.pVp = NULL;
    s_display.pBfr = NULL;
    if (s_pOwnedFront) {
        bitmapDestroy(s_pOwnedFront);
        s_pOwnedFront = NULL;
    }
    if (s_pOwnedBack) {
        bitmapDestroy(s_pOwnedBack);
        s_pOwnedBack = NULL;
    }
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
    if (s_display.pFade) {
        mm2_fade_set(s_display.pFade, MM2_FADE_STATE_IN, ubFrames, NULL);
    }
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

Mm2AmigaDisplay *mm2AmigaDisplayGet(void) { return &s_display; }

tSimpleBufferManager *mm2AmigaDisplayGetBuffer(void) { return s_display.pBfr; }

tVPort *mm2AmigaDisplayGetVPort(void) { return s_display.pVp; }

#endif /* AMIGA */
