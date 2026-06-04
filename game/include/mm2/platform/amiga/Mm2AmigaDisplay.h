#pragma once

#include "mm2/platform/amiga/Mm2AmigaConfig.h"

#ifdef AMIGA

#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/extview.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mm2AmigaDisplay {
    tView *pView;
    tVPort *pVp;
    tSimpleBufferManager *pBfr;
} Mm2AmigaDisplay;

/** Create 320×200 (or custom) 6bpp AGA view + simple buffer. Returns 0 on failure. */
UBYTE mm2AmigaDisplayCreate(UWORD uwWidth, UWORD uwHeight, UBYTE ubDoubleBuffer);

void mm2AmigaDisplayActivate(void);

/** Copper refresh + wait for beam (call once per frame after drawing). */
void mm2AmigaDisplayFrameEnd(void);

void mm2AmigaDisplayDispose(void);

Mm2AmigaDisplay *mm2AmigaDisplayGet(void);
tSimpleBufferManager *mm2AmigaDisplayGetBuffer(void);
tVPort *mm2AmigaDisplayGetVPort(void);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA */
