#include "mm2/states/mm2_app_context.h"

#include "mm2/Mm2Dbg.h"
#include "mm2/states/mm2_goto_trace.h"

Mm2StateManager *g_mm2_sm = NULL;

static Mm2PendingTransition s_pending = {MM2_TRANS_NONE, NULL};

void mm2_pending_push(Mm2State *target)
{
    MM2_DBG("MM2 GOTO: queue PUSH -> %s\n", mm2_goto_state_name(target));
    s_pending.op = MM2_TRANS_PUSH;
    s_pending.target = target;
}

void mm2_pending_pop(void)
{
    MM2_DBG("MM2 GOTO: queue POP\n");
    s_pending.op = MM2_TRANS_POP;
    s_pending.target = NULL;
}

void mm2_pending_change(Mm2State *target)
{
    MM2_DBG("MM2 GOTO: queue CHANGE -> %s\n", mm2_goto_state_name(target));
    s_pending.op = MM2_TRANS_CHANGE;
    s_pending.target = target;
}

void mm2_state_drain_pending(void)
{
    Mm2State *pCur;
    const char *op;

    if (!g_mm2_sm || s_pending.op == MM2_TRANS_NONE) {
        return;
    }

    pCur = g_mm2_sm->pCurrent;
    switch (s_pending.op) {
    case MM2_TRANS_PUSH:
        op = "PUSH";
        break;
    case MM2_TRANS_POP:
        op = "POP";
        break;
    case MM2_TRANS_CHANGE:
        op = "CHANGE";
        break;
    default:
        op = "?";
        break;
    }
    MM2_DBG(
        "MM2 GOTO: drain %s (cur=%s tgt=%s)\n",
        op,
        mm2_goto_state_name(pCur),
        mm2_goto_state_name(s_pending.target)
    );

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
    MM2_DBG("MM2 GOTO: drain done (now=%s)\n", mm2_goto_state_name(g_mm2_sm->pCurrent));
    s_pending.op = MM2_TRANS_NONE;
    s_pending.target = NULL;
}
