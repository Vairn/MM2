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
    /** Fallback directory for PC .4/.16 sheets when ``data_dir`` lacks them. */
    char pc_gfx_dir[512] = {};
};

GfxSettings &gfxSettings();
GfxBackend detectGfxBackend(const char *data_dir);
GfxBackend resolveGfxBackend(const char *data_dir);

/** Map Amiga lowercase ``town.32`` stem to PC filename for ``backend``. */
const char *resolveGfxFilename(GfxBackend backend, const char *amiga_filename);

/** Combat atlas filename for CGA/EGA (``MONSTERS.4`` / ``MONSTERS.16``). */
const char *resolvePcMonstersFilename(GfxBackend backend);

/** Optional paired silhouette (.4 when loading .16, .16 when loading .4). */
const char *resolveGfxSilhouetteFilename(GfxBackend backend, const char *amiga_filename);

GfxBackend gfxBackendFromString(const char *text);
int cgaPaletteFromString(const char *text);
const char *gfxBackendLabel(GfxBackend backend);

/** True when ``data_dir`` contains minimal PC wall/sky sheets for the given backend. */
bool pcGfxAssetsPresent(const char *data_dir, GfxBackend backend);

/** True when ``data_dir`` contains ``MONSTERS.4`` or ``MONSTERS.16`` (viewport sprite atlas). */
bool monstersAtlasPresent(const char *data_dir);

/** Probe known hybrid dirs; store result in ``settings.pc_gfx_dir`` (does not replace game data_dir). */
void initPcGfxFallbackDir(const char *data_dir, const char *exe_base_dir);

}  // namespace mm2::gfx
