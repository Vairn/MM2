// Amiga / ACE entry (no SDL).
#include "mm2/CppStdCompat.h"
#include "mm2/Game.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/CharacterUiKind.h"

#include <ace/managers/log.h>

namespace {

mm2::ui::CharacterUiKind parseUiKind(int argc, char **argv)
{
#if defined(MM2_UI_STUB)
    return mm2::ui::CharacterUiKind::Stub;
#elif defined(MM2_UI_CLASSIC)
    return mm2::ui::CharacterUiKind::AmigaClassic;
#else
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--ui=", 5) == 0) {
            return mm2::ui::characterUiKindFromString(arg + 5);
        }
    }
    return mm2::ui::CharacterUiKind::AmigaClassic;
#endif
}

const char *parseDataDir(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--ui=", 5) == 0) {
            continue;
        }
        return argv[i];
    }
    return "";
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir_hint = parseDataDir(argc, argv);
    const auto ui_kind = parseUiKind(argc, argv);

    if (!mm2::platform::init(&argc, &argv)) {
        return 1;
    }

#ifdef ACE_DEBUG
    logWrite("MM2: startup build 2026-06-04-nofade\n");
#endif

    static char data_dir_buf[512];
    const char *data_dir = data_dir_hint;
    if (mm2::platform::resolveDataDir(data_dir_hint, data_dir_buf, sizeof(data_dir_buf))) {
        data_dir = data_dir_buf;
#ifdef ACE_DEBUG
        logWrite("MM2: data dir '%s'\n", data_dir);
#endif
    } else {
#ifdef ACE_DEBUG
        logWrite("MM2: WARN data dir not found (hint '%s'), using bare filenames\n", data_dir_hint);
#endif
        if (data_dir_hint && data_dir_hint[0] == '.' && data_dir_hint[1] == '\0') {
            data_dir = "";
        }
    }

    if (!mm2::platform::beginDisplay()) {
        mm2::platform::shutdown();
        return 1;
    }

    /* Game + TitleScreen are too large for the Amiga stack; gs_image is heap-allocated
     * only when entering town (see GameSession::kGsImageBytes). */
    mm2::Game *const game = new mm2::Game();
    if (!game->init(data_dir, ui_kind)) {
        game->shutdown();
        delete game;
        mm2::platform::shutdown();
        return 1;
    }

    while (!game->shouldQuit()) {
        game->tick();
    }

    game->shutdown();
    delete game;
    mm2::platform::shutdown();
    return 0;
}
