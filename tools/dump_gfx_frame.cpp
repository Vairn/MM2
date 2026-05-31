// Dump one or all frames from a .32/.anm file as PPM.
// Build: g++ -std=c++17 -I../editor/src dump_gfx_frame.cpp ../editor/src/core/Gfx.cpp
//        ../editor/src/core/ByteIO.cpp -o dump_gfx_frame
#include <cstdio>
#include <cstring>
#include <string>

#include "core/Gfx.h"

static void writePPM(const char* path, const mm2::GfxFrame& f) {
    FILE* fp = fopen(path, "wb");
    if (!fp) return;
    fprintf(fp, "P6\n%d %d\n255\n", f.width, f.height);
    for (int i = 0; i < f.width * f.height; ++i) {
        fputc(f.rgba[i * 4 + 0], fp);
        fputc(f.rgba[i * 4 + 1], fp);
        fputc(f.rgba[i * 4 + 2], fp);
    }
    fclose(fp);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: dump_gfx_frame <file.32> <out_dir> [frame_index|-1=all]\n");
        return 1;
    }
    const std::string path = argv[1];
    const std::string outDir = argv[2];
    int only = (argc >= 4) ? std::atoi(argv[3]) : -1;

    bool isAnm = path.size() > 4 && path.substr(path.size() - 4) == ".anm";
    mm2::GfxImage img;
    mm2::gfxLoad(path, isAnm, img);
    if (!img.ok) {
        std::fprintf(stderr, "decode failed: %s\n", img.error.c_str());
        return 1;
    }

    std::fprintf(stdout, "%s frames=%d depth=%d\n", path.c_str(), img.frameCount, img.depth);
    for (int i = 0; i < static_cast<int>(img.frames.size()); ++i) {
        const auto& fr = img.frames[i];
        std::fprintf(stdout, "  [%02d] %dx%d flags=0x%x\n", i, fr.width, fr.height, fr.flags);
        if (only >= 0 && i != only) continue;
        char out[512];
        const char* base = strrchr(path.c_str(), '/');
        if (!base) base = strrchr(path.c_str(), '\\');
        base = base ? base + 1 : path.c_str();
        std::snprintf(out, sizeof(out), "%s/%s_f%02d.ppm", outDir.c_str(), base, i);
        writePPM(out, fr);
    }
    return 0;
}
