#include "mm2/states/AppHost.h"

#include "mm2/states/mm2_app_context.h"
#include "mm2/states/mm2_states.h"
#include "mm2/ui/CharacterUiKind.h"

int mm2_app_run(const char *data_dir, int ui_kind)
{
    const auto kind = static_cast<mm2::ui::CharacterUiKind>(ui_kind);
    mm2::AppHost *const host = &mm2::AppHost::createInStorage(mm2_app_host_storage);

    if (!host->start(data_dir, kind)) {
        return 1;
    }

    g_mm2_sm = mm2_stateManagerCreate();
    if (!g_mm2_sm) {
        host->shutdown();
        return 1;
    }

    mm2_stateChange(g_mm2_sm, &g_mm2_state_boot);

    while (!host->shouldQuit()) {
        host->pollInput();
        mm2_stateProcess(g_mm2_sm);
        mm2_state_drain_pending();
        if (host->shouldQuit()) {
            break;
        }
    }

    mm2_stateManagerDestroy(g_mm2_sm);
    g_mm2_sm = NULL;
    host->shutdown();
    return 0;
}
