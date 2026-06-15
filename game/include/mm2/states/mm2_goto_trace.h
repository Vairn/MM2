#pragma once

#include "mm2/states/mm2_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Human-readable ACE state stack label (for MM2 GOTO debug traces). */
const char *mm2_goto_state_name(Mm2State *pState);

#ifdef __cplusplus
}
#endif
