/* Vendored from AmigaPorts/ACE src/ace/managers/state.c (MPL-2.0). */
#include "mm2/states/mm2_state.h"

#include <stddef.h>

extern void *mm2_malloc(size_t size);
extern void mm2_free(void *ptr);
Mm2StateManager *mm2_stateManagerCreate(void)
{
    Mm2StateManager *p = (Mm2StateManager *)mm2_malloc(sizeof(Mm2StateManager));
    if (p) {
        p->pCurrent = NULL;
    }
    return p;
}

void mm2_stateManagerDestroy(Mm2StateManager *pStateManager)
{
    if (!pStateManager) {
        return;
    }
    mm2_statePopAll(pStateManager);
    mm2_free(pStateManager);
}

Mm2State *mm2_stateCreate(Mm2StateCb cbCreate, Mm2StateCb cbLoop, Mm2StateCb cbDestroy,
                          Mm2StateCb cbSuspend, Mm2StateCb cbResume)
{
    Mm2State *pState = (Mm2State *)mm2_malloc(sizeof(Mm2State));
    if (!pState) {
        return NULL;
    }
    pState->cbCreate = cbCreate;
    pState->cbLoop = cbLoop;
    pState->cbDestroy = cbDestroy;
    pState->cbSuspend = cbSuspend;
    pState->cbResume = cbResume;
    pState->pPrev = NULL;
    return pState;
}

void mm2_stateDestroy(Mm2State *pState)
{
    mm2_free(pState);
}

void mm2_statePush(Mm2StateManager *pStateManager, Mm2State *pState)
{
    if (!pStateManager || !pState) {
        return;
    }
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbSuspend) {
        pStateManager->pCurrent->cbSuspend();
    }
    pState->pPrev = pStateManager->pCurrent;
    pStateManager->pCurrent = pState;
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbCreate) {
        pStateManager->pCurrent->cbCreate();
    }
}

void mm2_statePop(Mm2StateManager *pStateManager)
{
    Mm2State *pOld;
    if (!pStateManager || !pStateManager->pCurrent) {
        return;
    }
    if (pStateManager->pCurrent->cbDestroy) {
        pStateManager->pCurrent->cbDestroy();
    }
    pOld = pStateManager->pCurrent;
    pStateManager->pCurrent = pOld->pPrev;
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbResume) {
        pStateManager->pCurrent->cbResume();
    }
}

void mm2_statePopAll(Mm2StateManager *pStateManager)
{
    if (!pStateManager) {
        return;
    }
    while (pStateManager->pCurrent) {
        if (pStateManager->pCurrent->cbDestroy) {
            pStateManager->pCurrent->cbDestroy();
        }
        pStateManager->pCurrent = pStateManager->pCurrent->pPrev;
    }
}

void mm2_stateChange(Mm2StateManager *pStateManager, Mm2State *pState)
{
    if (!pStateManager || !pState) {
        return;
    }
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbDestroy) {
        pStateManager->pCurrent->cbDestroy();
    }
    if (pStateManager->pCurrent) {
        pState->pPrev = pStateManager->pCurrent->pPrev;
    } else {
        pState->pPrev = NULL;
    }
    pStateManager->pCurrent = pState;
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbCreate) {
        pStateManager->pCurrent->cbCreate();
    }
}

void mm2_stateProcess(Mm2StateManager *pStateManager)
{
    if (!pStateManager) {
        return;
    }
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbLoop) {
        pStateManager->pCurrent->cbLoop();
    }
}
