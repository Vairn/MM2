// Amiga / ACE entry (no SDL).
#include "mm2/CppStdCompat.h"
#include "mm2/states/AppHost.h"
#include "mm2/platform/Platform.h"
#include "mm2/platform/Audio.h"
#include "mm2/ui/CharacterUiKind.h"
#include "mm2/Mm2Dbg.h"

namespace {
mm2::ui::CharacterUiKind parseUiKind(int argc, char **argv)
{
#if defined(MM2_UI_STUB)
    (void)argc;
    (void)argv;
    return mm2::ui::CharacterUiKind::Stub;
#elif defined(MM2_UI_CLASSIC)
    (void)argc;
    (void)argv;
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

    MM2_DBG("MM2 DBG: startup build 2026-06-04-dbg\n");
    static char data_dir_buf[512];
    const char *data_dir = data_dir_hint;
    if (mm2::platform::resolveDataDir(data_dir_hint, data_dir_buf, sizeof(data_dir_buf))) {
        data_dir = data_dir_buf;
        MM2_DBG("MM2 DBG: resolveDataDir ok '%s'\n", data_dir);
    } else {
        MM2_DBG("MM2 DBG: WARN data dir not found (hint '%s'), bare filenames\n", data_dir_hint);
        if (data_dir_hint && data_dir_hint[0] == '.' && data_dir_hint[1] == '\0') {
            data_dir = "";
        }
    }

    if (!mm2::platform::beginDisplay()) {
        mm2::audio::shutdown();
        mm2::platform::shutdown();
        return 1;
    }

    (void)mm2::audio::init(data_dir);

    MM2_DBG("MM2 DBG: entering ACE state loop\n");
    const int rc = mm2_app_run(data_dir, static_cast<int>(ui_kind));
    MM2_DBG("MM2 DBG: app quit\n");
    mm2::audio::shutdown();
    mm2::platform::shutdown();
    return rc;
}
