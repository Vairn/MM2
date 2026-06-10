#pragma once
// Per-environment tileset selection + loading.
//
// The area-load dispatcher (ASM 0x1786..0x187E, jump table @ 0x1880, 7 env
// cases) resolves filename-table entries (DATA 0x7B0, 4 bytes/entry) into
// the A4 sheet handles. Roles (EXTRACTED/docs/15-3d-view-and-game-screen.md §1):
//   -$7A06 walls   -$7A22 floor   -$7A1E ceiling   -$7A1A auto-map
//   -$7A16/-$7A12/-$7A0E outdoor sheets   -$7A02 sky (global, entry 6,
//   loaded once @ 0x25FA6)   -$7A0A biome floor decor (outdoor)
//
// Milestone 1 loads only the sheets the indoor 3D view composites:
// walls (-$7A06), floor (-$7A22) and the global sky.32 (-$7A02). The other
// roles are carried as data-driven table entries; loading them is wired but
// deferred (ceiling/auto-map/outdoor passes are not rendered yet).

#include "mm2/CppStdCompat.h"

#include "mm2_attrib_codec.h"
#include "mm2_image32_codec.h"

namespace mm2::gfx {

enum class EnvKind : uint8_t {
    Town = 0,
    Cavern = 1,
    Castle = 2,
    Outdoor = 3,
};

/* Filenames per env, from the 0x1786..0x187E dispatcher (doc 15 §1; numbers
 * in comments are filename-table indices). nullptr = role unused by env. */
struct EnvSheetNames {
    const char *walls;   /* -$7A06 */
    const char *floor;   /* -$7A22 */
    const char *ceiling; /* -$7A1E */
    const char *automap; /* -$7A1A */
};

const EnvSheetNames &envSheetNames(EnvKind kind);

/* Env kind from the materialized attrib record (doc 12: env_type 0x03 +
 * surface_flag 0x04; elemental planes 41..44 render outdoor). */
EnvKind envKindFromAttrib(const Mm2AttribRecord &rec);

class EnvAssets {
public:
    /* Load the global sky sheet (sky.32, filename-table entry 6). */
    bool loadGlobal(const char *data_dir);

    /* (Re)load the per-environment wall + floor sheets. */
    bool loadEnv(const char *data_dir, EnvKind kind);

    void unloadAll();

    bool ready() const { return env_ok_ && sky_ok_; }
    EnvKind kind() const { return kind_; }

    const mm2_image32_file &walls() const { return walls_; }
    const mm2_image32_file &floor() const { return floor_; }
    const mm2_image32_file &sky() const { return sky_; }

private:
    static bool loadImage(const char *data_dir, const char *name, mm2_image32_file *out);

    EnvKind kind_ = EnvKind::Town;
    bool env_ok_ = false;
    bool sky_ok_ = false;

    mm2_image32_file walls_{};
    mm2_image32_file floor_{};
    mm2_image32_file sky_{};
};

}  // namespace mm2::gfx
