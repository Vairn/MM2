#pragma once
#include "mm2/platform/amiga/Mm2AmigaConfig.h"

#ifdef AMIGA
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/extview.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tMm2Fade;

typedef struct Mm2AmigaDisplay {
    tView *pView;
    tVPort *pVp;
    tSimpleBufferManager *pBfr;
    struct tMm2Fade *pFade;
} Mm2AmigaDisplay;
/** Create 320×200 (or custom) 6bpp AGA view + simple buffer. Returns 0 on failure. */
UBYTE mm2AmigaDisplayCreate(UWORD uwWidth, UWORD uwHeight, UBYTE ubDoubleBuffer);

void mm2AmigaDisplayActivate(void);
/** Detach copper/playfield before destroy (avoids teardown glitches). */
void mm2AmigaDisplayUnload(void);
/** Copper refresh + wait for beam (call once per frame after drawing). */
void mm2AmigaDisplayFrameEnd(void);
/** Wait for beam only — no buffer swap (idle in-town frames). */
void mm2AmigaDisplayWaitVblank(void);

void mm2AmigaDisplayDispose(void);
Mm2AmigaDisplay *mm2AmigaDisplayGet(void);
tSimpleBufferManager *mm2AmigaDisplayGetBuffer(void);
tVPort *mm2AmigaDisplayGetVPort(void);
/** Copy current viewport palette into fade reference (call after applying asset palette). */
void mm2AmigaFadeCapturePalette(void);
void mm2AmigaFadeBeginIn(UBYTE ubFrames);
void mm2AmigaFadeBeginOut(UBYTE ubFrames);
/** Returns 1 once when fade finished; clears fired state. */
UBYTE mm2AmigaFadeConsumeDone(void);
/** Abort an in-progress fade and restore the captured reference palette. */
void mm2AmigaFadeCancel(void);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA */
