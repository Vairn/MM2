#pragma once

/* LoL-style palette fade (from LandsOfLore src/misc/fade.c), ptplayer stripped for MM2. */

#ifdef AMIGA

#include <ace/utils/extview.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tMm2FadeState {
    MM2_FADE_STATE_IN,
    MM2_FADE_STATE_OUT,
    MM2_FADE_STATE_IDLE,
    MM2_FADE_STATE_EVENT_FIRED
} tMm2FadeState;

typedef void (*tMm2CbFadeOnDone)(void);

typedef struct tMm2Fade {
    tMm2FadeState eState;
    UBYTE ubColorCount;
    UBYTE ubCnt;
    UBYTE ubCntEnd;
    void *pPaletteRef;
    tMm2CbFadeOnDone cbOnDone;
    tView *pView;
} tMm2Fade;

tMm2Fade *mm2_fade_create(tView *pView, void *pPalette, UBYTE ubColorCount);
void mm2_fade_destroy(tMm2Fade *pFade);
void mm2_fade_set(tMm2Fade *pFade, tMm2FadeState eState, UBYTE ubFrames, tMm2CbFadeOnDone cbOnDone);
tMm2FadeState mm2_fade_process(tMm2Fade *pFade);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA */
