/* Adapted from LandsOfLore src/misc/fade.c (MPL 2.0) — palette fade for AGA title logo. */

#include "mm2/platform/amiga/mm2_fade.h"

#ifdef AMIGA

#include <ace/managers/memory.h>
#include <ace/utils/palette.h>

static UBYTE mm2_fade_is_aga(const tView *pView)
{
    if (!pView) {
        return 0;
    }
    if (pView->uwFlags & VIEW_FLAG_GLOBAL_AGA) {
        return 1;
    }
    return pView->pFirstVPort && (pView->pFirstVPort->eFlags & VP_FLAG_AGA);
}

tMm2Fade *mm2_fade_create(tView *pView, void *pPalette, UBYTE ubColorCount)
{
    tMm2Fade *pFade;
    UWORD uwMaxColors = 32;
    UBYTE isAga;

    if (!pView || !pPalette) {
        return NULL;
    }

    isAga = mm2_fade_is_aga(pView);
    pFade = memAllocFastClear(sizeof(tMm2Fade));
    if (!pFade) {
        return NULL;
    }

    pFade->eState = MM2_FADE_STATE_IDLE;
    pFade->pView = pView;

    if (isAga) {
        uwMaxColors = (UWORD)(1u << pView->pFirstVPort->ubBpp);
        pFade->pPaletteRef = memAllocFastClear((ULONG)sizeof(ULONG) * uwMaxColors);
    } else {
        pFade->pPaletteRef = memAllocFastClear((ULONG)sizeof(UWORD) * 32u);
    }

    if (!pFade->pPaletteRef) {
        memFree(pFade, sizeof(tMm2Fade));
        return NULL;
    }

    if (ubColorCount > uwMaxColors) {
        pFade->ubColorCount = (UBYTE)uwMaxColors;
    } else {
        pFade->ubColorCount = ubColorCount;
    }

    if (isAga) {
        ULONG *pRef = (ULONG *)pFade->pPaletteRef;
        ULONG *pSrc = (ULONG *)pPalette;
        UBYTE i;
        for (i = 0; i < pFade->ubColorCount; ++i) {
            pRef[i] = pSrc[i];
        }
    } else {
        UWORD *pRef = (UWORD *)pFade->pPaletteRef;
        UWORD *pSrc = (UWORD *)pPalette;
        UBYTE i;
        for (i = 0; i < pFade->ubColorCount; ++i) {
            pRef[i] = pSrc[i];
        }
    }

    return pFade;
}

void mm2_fade_destroy(tMm2Fade *pFade)
{
    if (!pFade) {
        return;
    }
    if (pFade->pPaletteRef) {
        if (mm2_fade_is_aga(pFade->pView)) {
            memFree(pFade->pPaletteRef, (ULONG)sizeof(ULONG) * (1u << pFade->pView->pFirstVPort->ubBpp));
        } else {
            memFree(pFade->pPaletteRef, (ULONG)sizeof(UWORD) * 32u);
        }
    }
    memFree(pFade, sizeof(tMm2Fade));
}

void mm2_fade_set(tMm2Fade *pFade, tMm2FadeState eState, UBYTE ubFrames, tMm2CbFadeOnDone cbOnDone)
{
    if (!pFade) {
        return;
    }
    pFade->eState = eState;
    pFade->ubCnt = 0;
    pFade->ubCntEnd = ubFrames ? ubFrames : 1;
    pFade->cbOnDone = cbOnDone;
}

tMm2FadeState mm2_fade_process(tMm2Fade *pFade)
{
    tMm2FadeState eState;

    if (!pFade) {
        return MM2_FADE_STATE_IDLE;
    }

    eState = pFade->eState;
    if (pFade->eState != MM2_FADE_STATE_IDLE && pFade->eState != MM2_FADE_STATE_EVENT_FIRED) {
        UBYTE ubCnt;

        ++pFade->ubCnt;
        ubCnt = pFade->ubCnt;
        if (pFade->eState == MM2_FADE_STATE_OUT) {
            ubCnt = pFade->ubCntEnd - pFade->ubCnt;
        }

#ifdef ACE_USE_AGA_FEATURES
        if (mm2_fade_is_aga(pFade->pView)) {
            UBYTE ubRatio = (UBYTE)((255u * (ULONG)ubCnt) / (ULONG)pFade->ubCntEnd);
            paletteDimAga(
                (ULONG *)pFade->pPaletteRef,
                (ULONG *)pFade->pView->pFirstVPort->pPalette,
                pFade->ubColorCount, ubRatio);
        } else
#endif
        {
            UBYTE ubRatio = (UBYTE)((15u * (ULONG)ubCnt) / (ULONG)pFade->ubCntEnd);
            paletteDimOcs(
                (UWORD *)pFade->pPaletteRef,
                (volatile UWORD *)pFade->pView->pFirstVPort->pPalette,
                pFade->ubColorCount, ubRatio);
        }
        viewUpdateGlobalPalette(pFade->pView);

        if (pFade->ubCnt >= pFade->ubCntEnd) {
            pFade->eState = MM2_FADE_STATE_EVENT_FIRED;
            eState = pFade->eState;
            if (pFade->cbOnDone) {
                pFade->cbOnDone();
            }
        }
    } else {
        pFade->eState = MM2_FADE_STATE_IDLE;
        eState = MM2_FADE_STATE_IDLE;
    }
    return eState;
}

#endif /* AMIGA */
