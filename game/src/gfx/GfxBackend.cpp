#include "mm2/gfx/GfxBackend.h"

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

#include <cstdio>
#include <cstring>

namespace mm2::gfx {

namespace {

GfxSettings g_settings{};

struct SheetMap {
    const char *amiga;
    const char *cga;
    const char *ega;
};

constexpr SheetMap kSheetMaps[] = {
    {"town.32", "TOWN.4", "TOWN.16"},
    {"townf.32", "TOWNF.4", "TOWNF.16"},
    {"townt.32", "TOWNT.4", "TOWNT.16"},
    {"townb.32", "TOWNB.4", "TOWNB.16"},
    {"cave.32", "CAVE.4", "CAVE.16"},
    {"cavef.32", "CAVEF.4", "CAVEF.16"},
    {"cavet.32", "CAVET.4", "CAVET.16"},
    {"caveb.32", "CAVEB.4", "CAVEB.16"},
    {"castle.32", "CASTLE.4", "CASTLE.16"},
    {"castlef.32", "CASTLEF.4", "CASTLEF.16"},
    {"castlet.32", "CASTLET.4", "CASTLET.16"},
    {"castleb.32", "CASTLEB.4", "CASTLEB.16"},
    {"sky.32", "SKY.4", "SKY.16"},
    {"outf.32", "OUTF.4", "OUTF.16"},
    {"outb.32", "OUTB.4", "OUTB.16"},
    {"outdoor1.32", "OUTDOOR1.4", "OUTDOOR1.16"},
    {"outdoor2.32", "OUTDOOR2.4", "OUTDOOR2.16"},
    {"outdoor3.32", "OUTDOOR3.4", "OUTDOOR3.16"},
    {"desert.32", "DESERT.4", "DESERT.16"},
    {"ocean.32", "OCEAN.4", "OCEAN.16"},
    {"tundra.32", "TUNDRA.4", "TUNDRA.16"},
    {"swamp.32", "SWAMP.4", "SWAMP.16"},
    {"nwcp.32", "NWCP.4", "NWCP.16"},
    {"book.32", "BOOK.4", "BOOK.16"},
    {"throw.32", "THROW.4", "THROW.16"},
    {"globe.32", "GLOBE.4", "GLOBE.16"},
};

bool fileExists(const char *data_dir, const char *name)
{
    char *path = mm2_path_scratch_a();
    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, name)) {
        return false;
    }
    FILE *f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fclose(f);
    return true;
}

const SheetMap *findMap(const char *amiga_filename)
{
    if (!amiga_filename) {
        return nullptr;
    }
    for (const SheetMap &m : kSheetMaps) {
        if (std::strcmp(m.amiga, amiga_filename) == 0) {
            return &m;
        }
    }
    return nullptr;
}

const char *filenameForBackend(const SheetMap *map, GfxBackend backend)
{
    if (!map) {
        return nullptr;
    }
    switch (backend) {
        case GfxBackend::Cga:
            return map->cga;
        case GfxBackend::Ega:
            return map->ega;
        default:
            return map->amiga;
    }
}

}  // namespace

GfxSettings &gfxSettings()
{
    return g_settings;
}

GfxBackend gfxBackendFromString(const char *text)
{
    if (!text || !*text) {
        return GfxBackend::Auto;
    }
    if (std::strcmp(text, "amiga") == 0 || std::strcmp(text, "32") == 0) {
        return GfxBackend::Amiga;
    }
    if (std::strcmp(text, "cga") == 0 || std::strcmp(text, "4") == 0) {
        return GfxBackend::Cga;
    }
    if (std::strcmp(text, "ega") == 0 || std::strcmp(text, "16") == 0) {
        return GfxBackend::Ega;
    }
    if (std::strcmp(text, "auto") == 0) {
        return GfxBackend::Auto;
    }
    return GfxBackend::Auto;
}

int cgaPaletteFromString(const char *text)
{
    if (!text || !*text) {
        return 1;
    }
    return (text[0] == '0' && text[1] == '\0') ? 0 : 1;
}

const char *gfxBackendLabel(GfxBackend backend)
{
    switch (backend) {
        case GfxBackend::Cga:
            return "cga";
        case GfxBackend::Ega:
            return "ega";
        case GfxBackend::Amiga:
            return "amiga";
        default:
            return "auto";
    }
}

bool pcGfxAssetsPresent(const char *data_dir, GfxBackend backend)
{
    if (backend == GfxBackend::Ega) {
        return fileExists(data_dir, "SKY.16") && fileExists(data_dir, "TOWN.16");
    }
    if (backend == GfxBackend::Cga) {
        return fileExists(data_dir, "SKY.4") && fileExists(data_dir, "TOWN.4");
    }
    return false;
}

bool monstersAtlasPresent(const char *data_dir)
{
    return fileExists(data_dir, "MONSTERS.16") || fileExists(data_dir, "MONSTERS.4");
}

void initPcGfxFallbackDir(const char *data_dir, const char *exe_base_dir)
{
    g_settings.pc_gfx_dir[0] = '\0';
    GfxBackend backend = g_settings.backend;
    if (backend == GfxBackend::Auto) {
        backend = detectGfxBackend(data_dir);
    }

    auto storeIfValid = [&](const char *dir) -> bool {
        if (!dir || !dir[0]) {
            return false;
        }
        if (!pcGfxAssetsPresent(dir, backend)) {
            return false;
        }
        std::snprintf(g_settings.pc_gfx_dir, sizeof(g_settings.pc_gfx_dir), "%s", dir);
        std::fprintf(stderr, "mm2: PC gfx sheets from %s\n", g_settings.pc_gfx_dir);
        return true;
    };

    auto storeMonstersIfValid = [&](const char *dir) -> bool {
        if (!dir || !dir[0] || !monstersAtlasPresent(dir)) {
            return false;
        }
        if (g_settings.pc_gfx_dir[0] == '\0') {
            std::snprintf(g_settings.pc_gfx_dir, sizeof(g_settings.pc_gfx_dir), "%s", dir);
            std::fprintf(stderr, "mm2: MONSTERS atlas from %s\n", g_settings.pc_gfx_dir);
        }
        return true;
    };

#if !MM2_HOST_AMIGA
    /* Viewport sprites (OP_0B signs, combat, Corak ghost) always come from MONSTERS.* on PC. */
    if (!monstersAtlasPresent(data_dir)) {
        if (exe_base_dir && exe_base_dir[0]) {
            char exe_gfx[512];
            std::snprintf(exe_gfx, sizeof(exe_gfx), "%spc_gfx_test_data", exe_base_dir);
            storeMonstersIfValid(exe_gfx);
        }
        if (g_settings.pc_gfx_dir[0] == '\0') {
            static const char *const kCandidates[] = {
                "game/build/pc_gfx_test_data",
                "game\\build\\pc_gfx_test_data",
                "build/pc_gfx_test_data",
                "pc_gfx_test_data",
            };
            for (const char *cand : kCandidates) {
                if (storeMonstersIfValid(cand)) {
                    break;
                }
            }
        }
    }
#endif

    if (backend != GfxBackend::Cga && backend != GfxBackend::Ega) {
        return;
    }
    if (pcGfxAssetsPresent(data_dir, backend)) {
        return;
    }

    if (exe_base_dir && exe_base_dir[0]) {
        char exe_gfx[512];
        std::snprintf(exe_gfx, sizeof(exe_gfx), "%spc_gfx_test_data", exe_base_dir);
        if (storeIfValid(exe_gfx)) {
            return;
        }
    }

    static const char *const kCandidates[] = {
        "game/build/pc_gfx_test_data",
        "game\\build\\pc_gfx_test_data",
        "build/pc_gfx_test_data",
        "pc_gfx_test_data",
    };
    for (const char *cand : kCandidates) {
        if (storeIfValid(cand)) {
            return;
        }
    }
}

GfxBackend detectGfxBackend(const char *data_dir)
{
    if (fileExists(data_dir, "TOWN.16") || fileExists(data_dir, "SKY.16") ||
        fileExists(data_dir, "MONSTERS.16")) {
        return GfxBackend::Ega;
    }
    if (fileExists(data_dir, "TOWN.4") || fileExists(data_dir, "SKY.4") || fileExists(data_dir, "MONSTERS.4")) {
        return GfxBackend::Cga;
    }
    if (fileExists(data_dir, "town.32") || fileExists(data_dir, "sky.32")) {
        return GfxBackend::Amiga;
    }
    return GfxBackend::Amiga;
}

GfxBackend resolveGfxBackend(const char *data_dir)
{
    GfxBackend resolved = g_settings.backend;
    if (resolved == GfxBackend::Auto) {
        resolved = detectGfxBackend(data_dir);
    }
    g_settings.resolved = resolved;
    return resolved;
}

const char *resolveGfxFilename(GfxBackend backend, const char *amiga_filename)
{
    const SheetMap *map = findMap(amiga_filename);
    if (!map) {
        return amiga_filename;
    }
    return filenameForBackend(map, backend);
}

const char *resolvePcMonstersFilename(GfxBackend backend)
{
    switch (backend) {
        case GfxBackend::Cga:
            return "MONSTERS.4";
        case GfxBackend::Ega:
            return "MONSTERS.16";
        default:
            return "MONSTERS.16";
    }
}

const char *resolveGfxSilhouetteFilename(GfxBackend backend, const char *amiga_filename)
{
    const SheetMap *map = findMap(amiga_filename);
    if (!map) {
        return nullptr;
    }
    if (backend == GfxBackend::Ega) {
        return map->cga;
    }
    if (backend == GfxBackend::Cga) {
        return map->ega;
    }
    return nullptr;
}

}  // namespace mm2::gfx
