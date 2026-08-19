#pragma once

#include "mm2/CppStdCompat.h"

namespace mm2::gfx {

enum class GfxBackend : uint8_t {
    Auto = 0,
    Amiga,
    Cga,
    Ega,
};

struct GfxSettings {
    GfxBackend backend = GfxBackend::Auto;
    GfxBackend resolved = GfxBackend::Amiga;
    int cga_palette = 1;
    /** Fallback directory for PC .4/.16 sheets when data_dir lacks them. */
    char pc_gfx_dir[512] = {};
};

GfxSettings &gfxSettings();
GfxBackend detectGfxBackend(const char *data_dir);
GfxBackend resolveGfxBackend(const char *data_dir);

/** Amiga lowercase town.32 stem → PC filename for backend. */
const char *resolveGfxFilename(GfxBackend backend, const char *amiga_filename);

/** CGA/EGA combat atlas (MONSTERS.4 / MONSTERS.16). */
const char *resolvePcMonstersFilename(GfxBackend backend);

/** Paired silhouette (.4 when loading .16, .16 when loading .4). */
const char *resolveGfxSilhouetteFilename(GfxBackend backend, const char *amiga_filename);

GfxBackend gfxBackendFromString(const char *text);
int cgaPaletteFromString(const char *text);
const char *gfxBackendLabel(GfxBackend backend);

bool pcGfxAssetsPresent(const char *data_dir, GfxBackend backend);
bool monstersAtlasPresent(const char *data_dir);
/** Probe hybrid dirs into settings.pc_gfx_dir (does not replace data_dir). */
void initPcGfxFallbackDir(const char *data_dir, const char *exe_base_dir);

}  // namespace mm2::gfx
