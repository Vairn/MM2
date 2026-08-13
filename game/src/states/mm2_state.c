/* Vendored from AmigaPorts/ACE src/ace/managers/state.c (MPL-2.0). */
#include "mm2/states/mm2_state.h"

#include "mm2/Mm2Dbg.h"
#include "mm2/states/mm2_goto_trace.h"

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
    MM2_DBG(
        "MM2 GOTO: statePush %s -> %s\n",
        mm2_goto_state_name(pStateManager->pCurrent),
        mm2_goto_state_name(pState)
    );
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbSuspend) {
        MM2_DBG("MM2 GOTO: statePush suspend %s\n", mm2_goto_state_name(pStateManager->pCurrent));
        pStateManager->pCurrent->cbSuspend();
    }
    pState->pPrev = pStateManager->pCurrent;
    pStateManager->pCurrent = pState;
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbCreate) {
        MM2_DBG("MM2 GOTO: statePush create %s\n", mm2_goto_state_name(pState));
        pStateManager->pCurrent->cbCreate();
    }
}

void mm2_statePop(Mm2StateManager *pStateManager)
{
    Mm2State *pOld;
    if (!pStateManager || !pStateManager->pCurrent) {
        return;
    }
    MM2_DBG("MM2 GOTO: statePop %s\n", mm2_goto_state_name(pStateManager->pCurrent));
    if (pStateManager->pCurrent->cbDestroy) {
        pStateManager->pCurrent->cbDestroy();
    }
    pOld = pStateManager->pCurrent;
    pStateManager->pCurrent = pOld->pPrev;
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbResume) {
        MM2_DBG("MM2 GOTO: statePop resume %s\n", mm2_goto_state_name(pStateManager->pCurrent));
        pStateManager->pCurrent->cbResume();
    }
}

void mm2_statePopAll(Mm2StateManager *pStateManager)
{
    if (!pStateManager) {
        return;
    }
    while (pStateManager->pCurrent) {
        Mm2State *pState = pStateManager->pCurrent;
        Mm2State *pPrev = pState->pPrev;
        if (pState->cbDestroy) {
            pState->cbDestroy();
        }
        /* States are shared global singletons reused across pushes, so a stale
         * pPrev can point back into the chain. Detach as we go and stop on a
         * self-link so a cycle can never spin here forever (quit hang). */
        pState->pPrev = NULL;
        pStateManager->pCurrent = (pPrev == pState) ? NULL : pPrev;
    }
}

void mm2_stateChange(Mm2StateManager *pStateManager, Mm2State *pState)
{
    if (!pStateManager || !pState) {
        return;
    }
    MM2_DBG(
        "MM2 GOTO: stateChange %s -> %s\n",
        mm2_goto_state_name(pStateManager->pCurrent),
        mm2_goto_state_name(pState)
    );
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbDestroy) {
        pStateManager->pCurrent->cbDestroy();
    }
    if (pStateManager->pCurrent) {
        /* Changing back to the state directly beneath the current one is a
         * pop-to: keep its own pPrev. Overwriting it with current->pPrev (which
         * *is* pState here) would self-link the node and hang statePopAll. */
        if (pState != pStateManager->pCurrent->pPrev) {
            pState->pPrev = pStateManager->pCurrent->pPrev;
        }
    } else {
        pState->pPrev = NULL;
    }
    pStateManager->pCurrent = pState;
    if (pStateManager->pCurrent && pStateManager->pCurrent->cbCreate) {
        MM2_DBG("MM2 GOTO: stateChange create %s\n", mm2_goto_state_name(pState));
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
