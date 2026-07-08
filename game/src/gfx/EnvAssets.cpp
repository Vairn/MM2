#include "mm2/gfx/EnvAssets.h"

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

#include <cstdio>

#include "mm2_image32_codec.h"
#include "mm2_pc_gfx_codec.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#endif

namespace mm2::gfx {

namespace {

const EnvSheetNames kEnvSheets[4] = {
    /* Town    */ {"town.32" /*13*/, "townf.32" /*10*/, "townt.32" /*11*/, "townb.32" /*12*/},
    /* Cavern  */ {"cave.32" /*17*/, "cavef.32" /*14*/, "cavet.32" /*15*/, "caveb.32" /*16*/},
    /* Castle  */ {"castle.32" /*21*/, "castlef.32" /*18*/, "castlet.32" /*19*/, "castleb.32" /*20*/},
    /* Outdoor */ {nullptr, "outf.32" /*26*/, nullptr, "outb.32" /*25*/},
};

void freeBiomes(mm2_gfx_sheet *biomes, bool *loaded)
{
    for (int i = 0; i < 4; ++i) {
        if (loaded[i]) {
            mm2_gfx_sheet_free(&biomes[i]);
            loaded[i] = false;
        }
    }
}

}  // namespace

const EnvSheetNames &envSheetNames(EnvKind kind)
{
    return kEnvSheets[static_cast<uint8_t>(kind) & 3];
}

EnvKind envKindFromAttrib(const Mm2AttribRecord &rec)
{
    if (mm2_attrib_is_outdoor(&rec)) {
        return EnvKind::Outdoor;
    }
    switch (mm2_attrib_env_type(&rec)) {
        case MM2_ENV_TOWN:
            return EnvKind::Town;
        case MM2_ENV_CAVERN:
            return EnvKind::Cavern;
        case MM2_ENV_CASTLE_A:
        case MM2_ENV_CASTLE_B:
            return EnvKind::Castle;
        default:
            return EnvKind::Castle;
    }
}

bool EnvAssets::loadSheet(const char *data_dir, const char *amiga_name, mm2_gfx_sheet_role role,
                          mm2_gfx_sheet *out)
{
    char *const path = mm2_path_scratch_a();
    char *const sil_path = mm2_path_scratch_b();
    const char *filename = resolveGfxFilename(backend_, amiga_name);
    const char *sil_name = resolveGfxSilhouetteFilename(backend_, amiga_name);

    mm2_gfx_sheet_free(out);

    auto resolveSilhouette = [&](const char *dir) -> const char * {
        if (!sil_name || !dir) {
            return nullptr;
        }
        if (!joinDataPath(sil_path, MM2_PATH_SCRATCH_CAP, dir, sil_name)) {
            return nullptr;
        }
        FILE *probe = std::fopen(sil_path, "rb");
        if (!probe) {
            return nullptr;
        }
        std::fclose(probe);
        return sil_path;
    };

    auto tryPcLoad = [&](const char *dir) -> bool {
        if (!dir || !joinDataPath(path, MM2_PATH_SCRATCH_CAP, dir, filename)) {
            return false;
        }
        mm2_pc_gfx_set_cga_palette(gfxSettings().cga_palette);
        const char *sil = resolveSilhouette(dir);
        if (mm2_pc_wall_sheet_load(path, role, sil, out) != MM2_IMAGE32_OK || out->img.frame_count == 0) {
            return false;
        }
#if MM2_HOST_AMIGA
        return out->img.frames[0].bitmap != nullptr;
#else
        return out->img.frames[0].rgba != nullptr;
#endif
    };

    if (backend_ == GfxBackend::Amiga) {
        if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, filename)) {
            return false;
        }
        mm2_image32_file img{};
        if (mm2_image32_load_file(path, &img) != MM2_IMAGE32_OK || img.frame_count == 0) {
            return false;
        }
        if (mm2_gfx_sheet_adopt_image32(&img, MM2_GFX_BACKEND_AMIGA32, role, out) != MM2_IMAGE32_OK) {
            return false;
        }
    } else {
        if (!tryPcLoad(data_dir)) {
            const char *fallback = gfxSettings().pc_gfx_dir;
            if (!fallback[0] || !tryPcLoad(fallback)) {
                return false;
            }
        }
    }

#if MM2_HOST_AMIGA
    return out->img.frames[0].bitmap != nullptr;
#else
    return out->img.frames[0].rgba != nullptr;
#endif
}

bool EnvAssets::loadGlobal(const char *data_dir)
{
    if (sky_ok_ && data_dir_ == data_dir && backend_ == gfxSettings().resolved) {
        return true;
    }
    backend_ = resolveGfxBackend(data_dir);
    mm2_gfx_sheet_free(&sky_);
    data_dir_ = data_dir;
    sky_ok_ = loadSheet(data_dir, "sky.32", MM2_GFX_ROLE_SKY, &sky_);
    return sky_ok_;
}

bool EnvAssets::loadEnv(const char *data_dir, EnvKind kind)
{
    if (env_ok_ && kind_ == kind && data_dir_ == data_dir && backend_ == gfxSettings().resolved) {
        return true;
    }
    kind_ = kind;
    data_dir_ = data_dir;
    backend_ = resolveGfxBackend(data_dir);
    mm2_gfx_sheet_free(&walls_);
    mm2_gfx_sheet_free(&floor_);
    mm2_gfx_sheet_free(&torches_);
    mm2_gfx_sheet_free(&automap_);
    automap_ok_ = false;
    mm2_gfx_sheet_free(&outdoor1_);
    mm2_gfx_sheet_free(&outdoor2_);
    mm2_gfx_sheet_free(&outdoor3_);
    freeBiomes(biomes_, biome_loaded_);
    env_ok_ = false;

    if (kind == EnvKind::Outdoor) {
        const bool has_floor = loadSheet(data_dir, "outf.32", MM2_GFX_ROLE_FLOOR, &floor_);
        const bool has_h1 = loadSheet(data_dir, "outdoor1.32", MM2_GFX_ROLE_OUTDOOR_HORIZON, &outdoor1_);
        loadSheet(data_dir, "outdoor2.32", MM2_GFX_ROLE_OUTDOOR_HORIZON, &outdoor2_);
        loadSheet(data_dir, "outdoor3.32", MM2_GFX_ROLE_OUTDOOR_HORIZON, &outdoor3_);
        automap_ok_ = envSheetNames(kind).automap &&
                      loadSheet(data_dir, envSheetNames(kind).automap, MM2_GFX_ROLE_AUTOMAP, &automap_);
        env_ok_ = has_floor && has_h1;
#if MM2_HOST_AMIGA
        if (env_ok_) {
            applyWorldPalette();
        }
#endif
        return env_ok_;
    }

    const EnvSheetNames &names = envSheetNames(kind);
    if (!names.walls || !names.floor) {
        return false;
    }

    const bool has_walls = loadSheet(data_dir, names.walls, MM2_GFX_ROLE_WALL, &walls_);
    const bool has_floor = loadSheet(data_dir, names.floor, MM2_GFX_ROLE_FLOOR, &floor_);
    if (names.ceiling) {
        loadSheet(data_dir, names.ceiling, MM2_GFX_ROLE_TORCH, &torches_);
    }
    automap_ok_ = names.automap && loadSheet(data_dir, names.automap, MM2_GFX_ROLE_AUTOMAP, &automap_);
    env_ok_ = has_walls && has_floor;
#if MM2_HOST_AMIGA
    if (env_ok_) {
        applyWorldPalette();
    }
#endif
    return env_ok_;
}

const mm2_gfx_sheet &EnvAssets::horizonSheet(OutdoorHorizonSheet sheet) const
{
    switch (sheet) {
        case OutdoorHorizonSheet::Outdoor2:
            return outdoor2_;
        case OutdoorHorizonSheet::Outdoor3:
            return outdoor3_;
        default:
            return outdoor1_;
    }
}

const mm2_gfx_sheet &EnvAssets::biomeSheet(OutdoorBiome biome)
{
    const int idx = static_cast<int>(biome) & 3;
    if (!biome_loaded_[idx] && data_dir_) {
        biome_loaded_[idx] =
            loadSheet(data_dir_, outdoorBiomeFilename(biome), MM2_GFX_ROLE_OUTDOOR_BIOME, &biomes_[idx]);
    }
    return biomes_[idx];
}

void EnvAssets::applyWorldPalette() const
{
#if MM2_HOST_AMIGA
    if (!env_ok_) {
        return;
    }
    if (kind_ == EnvKind::Outdoor) {
        if (floor_.img.frame_count > 0) {
            mm2_amiga_apply_play_world_palette(&floor_.img);
        }
    } else if (walls_.img.frame_count > 0) {
        mm2_amiga_apply_play_world_palette(&walls_.img);
    }
#endif
}

void EnvAssets::unloadAll()
{
    mm2_gfx_sheet_free(&walls_);
    mm2_gfx_sheet_free(&floor_);
    mm2_gfx_sheet_free(&torches_);
    mm2_gfx_sheet_free(&automap_);
    automap_ok_ = false;
    mm2_gfx_sheet_free(&sky_);
    mm2_gfx_sheet_free(&outdoor1_);
    mm2_gfx_sheet_free(&outdoor2_);
    mm2_gfx_sheet_free(&outdoor3_);
    freeBiomes(biomes_, biome_loaded_);
    env_ok_ = false;
    sky_ok_ = false;
    data_dir_ = nullptr;
#if MM2_HOST_AMIGA
    mm2_amiga_clear_play_world_palette();
#endif
}

}  // namespace mm2::gfx
