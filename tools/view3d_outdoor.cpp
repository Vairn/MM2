// Outdoor first-person renderer CLI (ports tools/view3d_outdoor.py).
//
// Build from repo root (MSYS2):
//   g++ -std=c++17 -O2 -I editor/src tools/view3d_outdoor.cpp \
//       editor/src/core/Outdoor3D.cpp editor/src/core/Outdoor3DRender.cpp \
//       editor/src/core/View3D.cpp editor/src/core/MapFile.cpp \
//       editor/src/core/AttribFile.cpp editor/src/core/Gfx.cpp \
//       editor/src/core/ByteIO.cpp -o tools/view3d_outdoor
//
// Usage:
//   tools/view3d_outdoor map.dat attrib.dat <screen> <x> <y> <facing>
//   tools/view3d_outdoor map.dat attrib.dat 11 7 3 0 --data . --out view.ppm --trace

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/AttribFile.h"
#include "core/MapFile.h"
#include "core/Outdoor3D.h"
#include "core/Outdoor3DRender.h"

namespace {

void usage(const char* prog) {
    std::fprintf(stderr,
                 "usage: %s <map.dat> <attrib.dat> <screen> <x> <y> <facing 0=N..3=W>\n"
                 "       [--data <.32 dir>] [--out <file.ppm>] [--trace]\n",
                 prog);
}

bool argEq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 7) {
        usage(argv[0]);
        return 1;
    }

    const std::string mapPath = argv[1];
    const std::string attribPath = argv[2];
    const int screen = std::atoi(argv[3]);
    const int x = std::atoi(argv[4]);
    const int y = std::atoi(argv[5]);
    const int facing = std::atoi(argv[6]);

    std::string dataDir = ".";
    std::string outPath = "EXTRACTED/docs/img/_view3d_outdoor_cpp.ppm";
    bool trace = false;

    for (int i = 7; i < argc; ++i) {
        if (argEq(argv[i], "--data") && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (argEq(argv[i], "--out") && i + 1 < argc) {
            outPath = argv[++i];
        } else if (argEq(argv[i], "--trace")) {
            trace = true;
        }
    }

    mm2::MapFile map;
    if (!map.load(mapPath)) {
        std::fprintf(stderr, "failed to load %s\n", mapPath.c_str());
        return 1;
    }
    mm2::AttribFile attrib;
    if (!attrib.load(attribPath)) {
        std::fprintf(stderr, "failed to load %s\n", attribPath.c_str());
        return 1;
    }
    if (screen < 0 || screen >= mm2::kMapScreens) {
        std::fprintf(stderr, "screen out of range\n");
        return 1;
    }

    if (!mm2::initOutdoorTerrainLookup("EXTRACTED/ghidra/mm2_data_00.bin"))
        std::fprintf(stderr, "warning: terrain lookup not loaded (EXTRACTED/ghidra/mm2_data_00.bin)\n");

    mm2::OutdoorMapGrid grid(map, attrib, screen);
    mm2::OutdoorScene scene = mm2::buildOutdoorScene(grid, x, y, facing, attrib);

    if (trace) mm2::dumpOutdoorTrace(scene, screen, x, y, facing, stdout);

    mm2::OutdoorSheetCache cache(dataDir);
    mm2::OutdoorCanvas canvas;
    mm2::renderOutdoorView(scene, cache, canvas);

    if (!mm2::writeOutdoorCanvasPpm(canvas, outPath)) {
        std::fprintf(stderr, "failed to write %s\n", outPath.c_str());
        return 1;
    }
    std::printf("wrote %s\n", outPath.c_str());
    return 0;
}
