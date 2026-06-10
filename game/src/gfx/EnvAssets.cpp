#include "mm2/gfx/EnvAssets.h"

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

namespace mm2::gfx {

namespace {

/* Doc 15 §1 table (filename-table indices in parentheses).
 * Outdoor horizon: -$7A16/-$7A12/-$7A0E (outdoor1/2/3.32) + biome decor -$7A0A. */
const EnvSheetNames kEnvSheets[4] = {
    /* Town    */ {"town.32" /*13*/, "townf.32" /*10*/, "townt.32" /*11*/, "townb.32" /*12*/},
    /* Cavern  */ {"cave.32" /*17*/, "cavef.32" /*14*/, "cavet.32" /*15*/, "caveb.32" /*16*/},
    /* Castle  */ {"castle.32" /*21*/, "castlef.32" /*18*/, "castlet.32" /*19*/, "castleb.32" /*20*/},
    /* Outdoor */ {nullptr, "outf.32" /*26*/, nullptr, "outb.32" /*25*/},
};

void freeBiomes(mm2_image32_file *biomes, bool *loaded)
{
    for (int i = 0; i < 4; ++i) {
        if (loaded[i]) {
            mm2_image32_free(&biomes[i]);
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

bool EnvAssets::loadImage(const char *data_dir, const char *name, mm2_image32_file *out)
{
    char *const path = mm2_path_scratch_a();
    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, name)) {
        return false;
    }
    ::memset(out, 0, sizeof(*out));
    if (mm2_image32_load_file(path, out) != MM2_IMAGE32_OK || out->frame_count == 0) {
        return false;
    }
#if MM2_HOST_AMIGA
    return out->frames[0].bitmap != nullptr;
#else
    return out->frames[0].rgba != nullptr;
#endif
}

bool EnvAssets::loadGlobal(const char *data_dir)
{
    if (sky_ok_ && data_dir_ == data_dir) {
        return true;
    }
    mm2_image32_free(&sky_);
    data_dir_ = data_dir;
    sky_ok_ = loadImage(data_dir, "sky.32", &sky_);
    return sky_ok_;
}

bool EnvAssets::loadEnv(const char *data_dir, EnvKind kind)
{
    if (env_ok_ && kind_ == kind && data_dir_ == data_dir) {
        return true;
    }
    kind_ = kind;
    data_dir_ = data_dir;
    mm2_image32_free(&walls_);
    mm2_image32_free(&floor_);
    mm2_image32_free(&outdoor1_);
    mm2_image32_free(&outdoor2_);
    mm2_image32_free(&outdoor3_);
    freeBiomes(biomes_, biome_loaded_);
    env_ok_ = false;

    if (kind == EnvKind::Outdoor) {
        const bool has_floor = loadImage(data_dir, "outf.32", &floor_);
        const bool has_h1 = loadImage(data_dir, "outdoor1.32", &outdoor1_);
        loadImage(data_dir, "outdoor2.32", &outdoor2_);
        loadImage(data_dir, "outdoor3.32", &outdoor3_);
        env_ok_ = has_floor && has_h1;
        return env_ok_;
    }

    const EnvSheetNames &names = envSheetNames(kind);
    if (!names.walls || !names.floor) {
        return false;
    }

    const bool has_walls = loadImage(data_dir, names.walls, &walls_);
    const bool has_floor = loadImage(data_dir, names.floor, &floor_);
    env_ok_ = has_walls && has_floor;
    return env_ok_;
}

const mm2_image32_file &EnvAssets::horizonSheet(OutdoorHorizonSheet sheet) const
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

const mm2_image32_file &EnvAssets::biomeSheet(OutdoorBiome biome)
{
    const int idx = static_cast<int>(biome) & 3;
    if (!biome_loaded_[idx] && data_dir_) {
        biome_loaded_[idx] = loadImage(data_dir_, outdoorBiomeFilename(biome), &biomes_[idx]);
    }
    return biomes_[idx];
}

void EnvAssets::unloadAll()
{
    mm2_image32_free(&walls_);
    mm2_image32_free(&floor_);
    mm2_image32_free(&sky_);
    mm2_image32_free(&outdoor1_);
    mm2_image32_free(&outdoor2_);
    mm2_image32_free(&outdoor3_);
    freeBiomes(biomes_, biome_loaded_);
    env_ok_ = false;
    sky_ok_ = false;
    data_dir_ = nullptr;
}

}  // namespace mm2::gfx
