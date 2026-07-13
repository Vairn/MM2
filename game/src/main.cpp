#include <SDL.h>

#include "mm2/states/AppHost.h"
#include "mm2/platform/Platform.h"
#include "mm2/platform/Audio.h"
#include "mm2/gfx/GfxBackend.h"
#include "mm2/ui/CharacterUiKind.h"
#include "mm2_pc_gfx_codec.h"

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

void parseGfxOptions(int argc, char **argv)
{
    auto &settings = mm2::gfx::gfxSettings();
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--gfx=", 6) == 0) {
            settings.backend = mm2::gfx::gfxBackendFromString(arg + 6);
            continue;
        }
        if (std::strncmp(arg, "--cga-palette=", 14) == 0) {
            settings.cga_palette = mm2::gfx::cgaPaletteFromString(arg + 14);
            continue;
        }
    }
    mm2_pc_gfx_set_cga_palette(settings.cga_palette);
}

const char *parseDataDir(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--ui=", 5) == 0 || std::strncmp(arg, "--gfx=", 6) == 0 ||
            std::strncmp(arg, "--cga-palette=", 14) == 0) {
            continue;
        }
        return arg;
    }
    return ".";
}

}  // namespace

int SDL_main(int argc, char **argv)
{
    parseGfxOptions(argc, argv);
    const char *data_dir_hint = parseDataDir(argc, argv);
    const auto ui_kind = parseUiKind(argc, argv);

    if (!mm2::platform::init(&argc, &argv)) {
        return 1;
    }

    static char data_dir_buf[512];
    const char *data_dir = data_dir_hint;
    if (mm2::platform::resolveDataDir(data_dir_hint, data_dir_buf, sizeof(data_dir_buf))) {
        data_dir = data_dir_buf;
    }
    char exe_base[512] = {};
    char *sdl_base = SDL_GetBasePath();
    if (sdl_base) {
        std::snprintf(exe_base, sizeof(exe_base), "%s", sdl_base);
        SDL_free(sdl_base);
    }
    mm2::gfx::initPcGfxFallbackDir(data_dir, exe_base[0] ? exe_base : nullptr);
    (void)mm2::audio::init(data_dir);

    if (!mm2::platform::beginDisplay()) {
        mm2::audio::shutdown();
        mm2::platform::shutdown();
        return 1;
    }

    const int rc = mm2_app_run(data_dir, static_cast<int>(ui_kind));

    mm2::audio::shutdown();
    mm2::platform::shutdown();
    return rc;
}
