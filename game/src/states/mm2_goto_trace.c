#include "mm2/states/mm2_goto_trace.h"

#include "mm2/states/mm2_states.h"

const char *mm2_goto_state_name(Mm2State *pState)
{
    if (!pState) {
        return "null";
    }
    if (pState == &g_mm2_state_boot) {
        return "boot";
    }
    if (pState == &g_mm2_state_logo) {
        return "logo";
    }
    if (pState == &g_mm2_state_attract) {
        return "attract";
    }
    if (pState == &g_mm2_state_menu) {
        return "menu";
    }
    if (pState == &g_mm2_state_controls) {
        return "controls";
    }
    if (pState == &g_mm2_state_options) {
        return "options";
    }
    if (pState == &g_mm2_state_char_view) {
        return "char_view";
    }
    if (pState == &g_mm2_state_char_create) {
        return "char_create";
    }
    if (pState == &g_mm2_state_char_choose) {
        return "char_choose";
    }
    if (pState == &g_mm2_state_in_town) {
        return "in_town";
    }
    return "unknown";
}
