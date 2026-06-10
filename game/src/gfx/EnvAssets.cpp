#include "mm2/gfx/EnvAssets.h"

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

namespace mm2::gfx {

namespace {

/* Doc 15 §1 table (filename-table indices in parentheses).
 * Outdoor walls/ceiling: the outdoor view (outdoor_3d_master @ 0x18870) uses
 * the layer sheets -$7A16/-$7A12/-$7A0E (outdoor1/2/3.32) + biome decor
 * -$7A0A instead of a wall sheet — not loaded yet (outdoor render is a later
 * milestone). */
const EnvSheetNames kEnvSheets[4] = {
    /* Town    */ {"town.32" /*13*/, "townf.32" /*10*/, "townt.32" /*11*/, "townb.32" /*12*/},
    /* Cavern  */ {"cave.32" /*17*/, "cavef.32" /*14*/, "cavet.32" /*15*/, "caveb.32" /*16*/},
    /* Castle  */ {"castle.32" /*21*/, "castlef.32" /*18*/, "castlet.32" /*19*/, "castleb.32" /*20*/},
    /* Outdoor */ {nullptr, "outf.32" /*26*/, nullptr, "outb.32" /*25*/},
};

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
            /* GAP: dispatcher @ 0x1880 has 7 cases; the raw env index for the
             * remaining interiors (dungeon category 3) needs tracing. Castle
             * sheets are the closest documented set. */
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
    mm2_image32_free(&sky_);
    sky_ok_ = loadImage(data_dir, "sky.32", &sky_);
    return sky_ok_;
}

bool EnvAssets::loadEnv(const char *data_dir, EnvKind kind)
{
    kind_ = kind;
    mm2_image32_free(&walls_);
    mm2_image32_free(&floor_);
    env_ok_ = false;

    const EnvSheetNames &names = envSheetNames(kind);
    if (kind == EnvKind::Outdoor) {
        /* Demo / partial outdoor 3D: outf.32 floor + outb.32 wall strips until layer path @ 0x18870. */
        const bool has_floor = loadImage(data_dir, "outf.32", &floor_);
        const bool has_walls = loadImage(data_dir, "outb.32", &walls_);
        env_ok_ = has_floor && has_walls;
        return env_ok_;
    }
    if (!names.walls || !names.floor) {
        return false;
    }

    const bool has_walls = loadImage(data_dir, names.walls, &walls_);
    const bool has_floor = loadImage(data_dir, names.floor, &floor_);
    env_ok_ = has_walls && has_floor;
    return env_ok_;
}

void EnvAssets::unloadAll()
{
    mm2_image32_free(&walls_);
    mm2_image32_free(&floor_);
    mm2_image32_free(&sky_);
    env_ok_ = false;
    sky_ok_ = false;
}

}  // namespace mm2::gfx
