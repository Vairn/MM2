#include "mm2/states/mm2_app_context.h"

Mm2StateManager *g_mm2_sm = NULL;

static Mm2PendingTransition s_pending = {MM2_TRANS_NONE, NULL};

void mm2_pending_push(Mm2State *target)
{
    s_pending.op = MM2_TRANS_PUSH;
    s_pending.target = target;
}

void mm2_pending_pop(void)
{
    s_pending.op = MM2_TRANS_POP;
    s_pending.target = NULL;
}

void mm2_pending_change(Mm2State *target)
{
    s_pending.op = MM2_TRANS_CHANGE;
    s_pending.target = target;
}

void mm2_state_drain_pending(void)
{
    if (!g_mm2_sm || s_pending.op == MM2_TRANS_NONE) {
        return;
    }
    switch (s_pending.op) {
    case MM2_TRANS_PUSH:
        mm2_statePush(g_mm2_sm, s_pending.target);
        break;
    case MM2_TRANS_POP:
        mm2_statePop(g_mm2_sm);
        break;
    case MM2_TRANS_CHANGE:
        mm2_stateChange(g_mm2_sm, s_pending.target);
        break;
    default:
        break;
    }
    s_pending.op = MM2_TRANS_NONE;
    s_pending.target = NULL;
}
