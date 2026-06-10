#include <SDL.h>

#include "mm2/states/AppHost.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/CharacterUiKind.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {

mm2::ui::CharacterUiKind parseUiKind(int argc, char **argv)
{
#if defined(MM2_UI_STUB)
    return mm2::ui::CharacterUiKind::Stub;
#elif defined(MM2_UI_CLASSIC)
    return mm2::ui::CharacterUiKind::AmigaClassic;
#endif

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--ui=", 5) == 0) {
            return mm2::ui::characterUiKindFromString(arg + 5);
        }
    }
    const char *env = std::getenv("MM2_UI");
    if (env && *env) {
        return mm2::ui::characterUiKindFromString(env);
    }
    return mm2::ui::CharacterUiKind::AmigaClassic;
}

const char *parseDataDir(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--ui=", 5) == 0) {
            continue;
        }
        return argv[i];
    }
    return ".";
}

}  // namespace

int SDL_main(int argc, char **argv)
{
    const char *data_dir = parseDataDir(argc, argv);
    const auto ui_kind = parseUiKind(argc, argv);

    if (!mm2::platform::init(&argc, &argv)) {
        return 1;
    }

    if (!mm2::platform::beginDisplay()) {
        mm2::platform::shutdown();
        return 1;
    }

    const int rc = mm2_app_run(data_dir, static_cast<int>(ui_kind));

    mm2::platform::shutdown();
    return rc;
}
