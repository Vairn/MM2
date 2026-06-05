// Offline validator / frame exporter for the gfx decoder (same path as MM2ED).
// Compile: g++ -std=c++17 -I../src gfx_dump.cpp ../src/core/Gfx.cpp ../src/core/ByteIO.cpp -o gfx_dump
#include <cstdio>
#include <cstring>
#include <string>

#include "core/Gfx.h"

static void writeRgbaBin(const char* path, const mm2::GfxFrame& f) {
    FILE* fp = fopen(path, "wb");
    if (!fp) return;
    uint32_t wh[2] = {static_cast<uint32_t>(f.width), static_cast<uint32_t>(f.height)};
    fwrite(wh, sizeof(wh), 1, fp);
    fwrite(f.rgba.data(), 1, f.rgba.size(), fp);
    fclose(fp);
}

static int exportAll(const char* dir, const mm2::GfxImage& img) {
    for (int i = 0; i < img.frameCount; ++i) {
        char path[512];
        snprintf(path, sizeof(path), "%s/f%02d.rgba", dir, i);
        writeRgbaBin(path, img.frames[i]);
    }
    printf("   exported %d frames -> %s\n", img.frameCount, dir);
    return 0;
}

int main(int argc, char** argv) {
    const char* exportDir = nullptr;
    int a = 1;
    if (a < argc && strcmp(argv[a], "--export") == 0) {
        if (a + 1 >= argc) {
            fprintf(stderr, "usage: gfx_dump --export <dir> <file.32>\n");
            return 1;
        }
        exportDir = argv[a + 1];
        a += 2;
    }
    if (a >= argc) {
        fprintf(stderr, "usage: gfx_dump [--export <dir>] <file.32> ...\n");
        return 1;
    }
    for (; a < argc; ++a) {
        std::string path = argv[a];
        bool isAnm = path.size() > 4 && path.substr(path.size() - 4) == ".anm";
        mm2::GfxImage img;
        mm2::gfxLoad(path, isAnm, img);
        printf("%s: ok=%d err='%s' frames=%d depth=%d chunk@0x%zx\n", path.c_str(), img.ok,
               img.error.c_str(), img.frameCount, img.depth, img.chunkOffset);
        if (!img.ok) return 1;
        if (exportDir) {
            if (exportAll(exportDir, img) != 0) return 1;
        } else {
            for (int i = 0; i < (int)img.frames.size() && i < 5; ++i)
                printf("   frame %d: %dx%d flags=0x%x\n", i, img.frames[i].width,
                       img.frames[i].height, img.frames[i].flags);
        }
    }
    return 0;
}
