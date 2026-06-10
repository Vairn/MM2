/* Vendored from AmigaPorts/ACE include/ace/managers/state.h (MPL-2.0).
 * Uses mm2_malloc/mm2_free so the same API works on SDL and Amiga. */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Mm2StateCb)(void);

typedef struct Mm2State {
    Mm2StateCb cbCreate;
    Mm2StateCb cbLoop;
    Mm2StateCb cbDestroy;
    Mm2StateCb cbSuspend;
    Mm2StateCb cbResume;
    struct Mm2State *pPrev;
} Mm2State;

typedef struct Mm2StateManager {
    Mm2State *pCurrent;
} Mm2StateManager;

Mm2StateManager *mm2_stateManagerCreate(void);
void mm2_stateManagerDestroy(Mm2StateManager *pStateManager);

Mm2State *mm2_stateCreate(Mm2StateCb cbCreate, Mm2StateCb cbLoop, Mm2StateCb cbDestroy,
                          Mm2StateCb cbSuspend, Mm2StateCb cbResume);
void mm2_stateDestroy(Mm2State *pState);

void mm2_statePush(Mm2StateManager *pStateManager, Mm2State *pState);
void mm2_statePop(Mm2StateManager *pStateManager);
void mm2_statePopAll(Mm2StateManager *pStateManager);
void mm2_stateChange(Mm2StateManager *pStateManager, Mm2State *pState);
void mm2_stateProcess(Mm2StateManager *pStateManager);

#ifdef __cplusplus
}
#endif
