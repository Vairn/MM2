#pragma once

#include "mm2/states/mm2_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Mm2StateTransitionOp {
    MM2_TRANS_NONE = 0,
    MM2_TRANS_PUSH,
    MM2_TRANS_POP,
    MM2_TRANS_CHANGE,
} Mm2StateTransitionOp;

/** Queued at end of frame — never call mm2_statePush from deep C++ tick. */
typedef struct Mm2PendingTransition {
    Mm2StateTransitionOp op;
    Mm2State *target;
} Mm2PendingTransition;

extern Mm2StateManager *g_mm2_sm;

void mm2_pending_push(Mm2State *target);
void mm2_pending_pop(void);
void mm2_pending_change(Mm2State *target);
void mm2_state_drain_pending(void);

#ifdef __cplusplus
}
#endif
