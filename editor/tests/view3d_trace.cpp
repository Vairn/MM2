// Offline trace for the 3D frustum port. Not part of the CMake build.
// Compile from editor/:
//   g++ -std=c++17 -I src tests/view3d_trace.cpp \
//       src/core/View3D.cpp src/core/MapFile.cpp src/core/ByteIO.cpp \
//       -o view3d_trace
//
// Usage: view3d_trace <map.dat> <screen> <x> <y> <facing 0-3>

#include <cstdio>
#include <cstdlib>

#include "core/MapFile.h"
#include "core/View3D.h"

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr,
                     "usage: %s <map.dat> <screen> <x> <y> <facing 0=N 1=E 2=S 3=W>\n",
                     argv[0]);
        return 1;
    }

    mm2::MapFile map;
    if (!map.load(argv[1])) {
        std::fprintf(stderr, "failed to load %s\n", argv[1]);
        return 1;
    }

    const int screen = std::atoi(argv[2]);
    mm2::View3DCamera cam{};
    cam.x = std::atoi(argv[3]);
    cam.y = std::atoi(argv[4]);
    cam.facing = std::atoi(argv[5]);

    if (screen < 0 || screen >= mm2::kMapScreens) {
        std::fprintf(stderr, "screen out of range\n");
        return 1;
    }

    mm2::View3DMapBuffers bufs{};
    bufs.center = map.screens[static_cast<size_t>(screen)].visual;

    mm2::View3DScene scene = mm2::buildView3DScene(bufs, cam);

    std::printf("screen %d  camera (%d,%d)\n", screen, cam.x, cam.y);
    mm2::dumpView3DTrace(scene, cam.x, cam.y, cam.facing, stdout);
    return 0;
}
