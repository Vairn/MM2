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
// Indoor milestone: walls (-$7A06), floor (-$7A22), global sky.32 (-$7A02).
// Outdoor milestone: outf.32 floor, sky.32, outdoor1/2/3.32 horizon layers,
// biome decor sheets (desert/ocean/tundra/swamp).

#include "mm2/CppStdCompat.h"

#include "mm2/gfx/GfxBackend.h"
#include "mm2/gfx/OutdoorView3D.h"
#include "mm2_attrib_codec.h"
#include "mm2_gfx_sheet.h"

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

    /* (Re)load the per-environment sheets for the active view mode. */
    bool loadEnv(const char *data_dir, EnvKind kind);

    void unloadAll();

    bool ready() const { return env_ok_ && sky_ok_; }
    EnvKind kind() const { return kind_; }
    GfxBackend backend() const { return backend_; }

    const mm2_gfx_sheet &walls() const { return walls_; }
    const mm2_gfx_sheet &floor() const { return floor_; }
    /** Per-env torch sheet (townt/cavet/castlet.32) — blit source A4-$7A1E @ key_read_3d. */
    const mm2_gfx_sheet &torches() const { return torches_; }
    const mm2_gfx_sheet &sky() const { return sky_; }
    const mm2_gfx_sheet &automap() const { return automap_; }

    bool automapReady() const { return automap_ok_; }

    const mm2_gfx_sheet &horizonSheet(OutdoorHorizonSheet sheet) const;
    const mm2_gfx_sheet &biomeSheet(OutdoorBiome biome);

    /** Amiga: load pens 0-31 from the active env sheet (once per loadEnv). */
    void applyWorldPalette() const;

private:
    bool loadSheet(const char *data_dir, const char *amiga_name, mm2_gfx_sheet_role role, mm2_gfx_sheet *out);

    EnvKind kind_ = EnvKind::Town;
    GfxBackend backend_ = GfxBackend::Amiga;
    bool env_ok_ = false;
    bool sky_ok_ = false;
    const char *data_dir_ = nullptr;

    mm2_gfx_sheet walls_{};
    mm2_gfx_sheet floor_{};
    mm2_gfx_sheet torches_{};
    mm2_gfx_sheet automap_{};
    mm2_gfx_sheet sky_{};
    bool automap_ok_ = false;
    mm2_gfx_sheet outdoor1_{};
    mm2_gfx_sheet outdoor2_{};
    mm2_gfx_sheet outdoor3_{};
    mm2_gfx_sheet biomes_[4]{};
    bool biome_loaded_[4]{};
};

}  // namespace mm2::gfx
