// Offline play-screen render: Middlegate inn spawn (7,3,N) with the first
// party found in roster.dat, dumped as build/playscreen_middlegate.ppm for
// visual comparison against the original Amiga play screen.
//
// Usage: playscreen_golden [--gfx=auto|amiga|cga|ega] [--cga-palette=0|1] <data_dir>

#include "mm2/GameSession.h"
#include "mm2/gfx/GfxBackend.h"
#include "mm2_pc_gfx_codec.h"

#include <cstdio>
#include <cstring>

namespace {

const char *parseDataDir(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--gfx=", 6) == 0 || std::strncmp(arg, "--cga-palette=", 14) == 0) {
            continue;
        }
        return arg;
    }
    return "../..";
}

void parseGfxOptions(int argc, char **argv)
{
    auto &settings = mm2::gfx::gfxSettings();
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--gfx=", 6) == 0) {
            settings.backend = mm2::gfx::gfxBackendFromString(arg + 6);
        } else if (std::strncmp(arg, "--cga-palette=", 14) == 0) {
            settings.cga_palette = mm2::gfx::cgaPaletteFromString(arg + 14);
        }
    }
    mm2_pc_gfx_set_cga_palette(settings.cga_palette);
}

}  // namespace

int main(int argc, char **argv)
{
    parseGfxOptions(argc, argv);
    const char *data_dir = parseDataDir(argc, argv);
    mm2::gfx::initPcGfxFallbackDir(data_dir, nullptr);

    char path[512];
    std::snprintf(path, sizeof(path), "%s/roster.dat", data_dir);
    Mm2RosterFile roster{};
    if (mm2_roster_load_file(path, &roster) != MM2_ROSTER_OK) {
        std::fprintf(stderr, "FAIL: cannot load %s\n", path);
        return 1;
    }

    int members[MM2_PARTY_LAUNCH_SLOTS];
    int count = 0;
    for (int i = 0; i < MM2_ROSTER_RECORD_COUNT && count < 6; ++i) {
        const Mm2RosterRecord &rec = roster.records[i];
        if (rec.name[0] != 0 && (rec.town_flags & 0x7F) == 1) {
            members[count++] = i;
        }
    }
    if (count == 0) {
        std::fprintf(stderr, "FAIL: no Middlegate characters in roster.dat\n");
        return 1;
    }

    Mm2PartyLaunch launch{};
    mm2_party_launch_build(&launch, 1, members, count);
    if (launch.area_id != 0 || launch.coord_x != 7 || launch.coord_y != 3 || launch.facing_key != 'N') {
        std::fprintf(stderr, "FAIL: unexpected Middlegate spawn (%d %d,%d %c)\n", launch.area_id,
                     launch.coord_x, launch.coord_y, launch.facing_key);
        return 1;
    }

    mm2::GameSession session;
    if (!session.start(data_dir, roster, launch, mm2::GameSession::kStartSkipScriptedIntros)) {
        std::fprintf(stderr, "FAIL: GameSession::start\n");
        return 1;
    }
    session.render();

    const mm2::gfx::ScreenCompositor &c = session.compositor();
    std::FILE *f = std::fopen("playscreen_golden.ppm", "wb");
    if (!f) {
        std::fprintf(stderr, "FAIL: cannot write ppm\n");
        return 1;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", mm2::gfx::ScreenCompositor::kWidth,
                 mm2::gfx::ScreenCompositor::kHeight);
    const uint8_t *rgba = c.pixels();
    for (int i = 0; i < mm2::gfx::ScreenCompositor::kWidth * mm2::gfx::ScreenCompositor::kHeight; ++i) {
        std::fputc(rgba[i * 4 + 0], f);
        std::fputc(rgba[i * 4 + 1], f);
        std::fputc(rgba[i * 4 + 2], f);
    }
    std::fclose(f);
    session.shutdown();

    std::printf("OK: playscreen_golden.ppm written (party=%d gfx=%d cga_pal=%d)\n", count,
                static_cast<int>(mm2::gfx::gfxSettings().resolved), mm2::gfx::gfxSettings().cga_palette);
    return 0;
}
