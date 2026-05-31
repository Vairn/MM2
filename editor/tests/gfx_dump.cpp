// Offline validator for the gfx decoder. Not part of the CMake build.
// Compile: g++ -std=c++17 -I../src gfx_dump.cpp ../src/core/Gfx.cpp ../src/core/ByteIO.cpp -o gfx_dump
#include <cstdio>

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
    for (int a = 1; a < argc; ++a) {
        std::string path = argv[a];
        bool isAnm = path.size() > 4 && path.substr(path.size() - 4) == ".anm";
        mm2::GfxImage img;
        mm2::gfxLoad(path, isAnm, img);
        printf("%s: ok=%d err='%s' frames=%d depth=%d chunk@0x%zx\n", path.c_str(), img.ok,
               img.error.c_str(), img.frameCount, img.depth, img.chunkOffset);
        for (int i = 0; i < (int)img.frames.size() && i < 5; ++i)
            printf("   frame %d: %dx%d flags=0x%x\n", i, img.frames[i].width,
                   img.frames[i].height, img.frames[i].flags);
        if (!img.frames.empty()) {
            char out[256];
            snprintf(out, sizeof(out), "dump_%d.ppm", a);
            writePPM(out, img.frames[0]);
            printf("   wrote %s\n", out);
        }
    }
    return 0;
}
