// Amiga / ACE entry (no SDL).
#include "mm2/CppStdCompat.h"
#include "mm2/Game.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/CharacterUiKind.h"

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
    return ".";
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = parseDataDir(argc, argv);
    const auto ui_kind = parseUiKind(argc, argv);

    if (!mm2::platform::init(&argc, &argv)) {
        return 1;
    }

    mm2::Game game;
    if (!game.init(data_dir, ui_kind)) {
        mm2::platform::shutdown();
        return 1;
    }

    while (!game.shouldQuit()) {
        game.tick();
    }

    game.shutdown();
    mm2::platform::shutdown();
    return 0;
}
