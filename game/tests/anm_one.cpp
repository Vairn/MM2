#include <stdio.h>
#include "mm2_anm_codec.h"
#include "mm2_anm_preview.h"

static void writeppm(const char *out, mm2_anm_composite_rgba *c)
{
    FILE *f = fopen(out, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", c->width, c->height);
    for (int y = 0; y < c->height; ++y) {
        for (int x = 0; x < c->width; ++x) {
            uint8_t *p = c->rgba + ((size_t)y * c->width + x) * 4;
            fputc(p[0], f);
            fputc(p[1], f);
            fputc(p[2], f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        return 1;
    }
    char path[64];
    snprintf(path, sizeof(path), "../../%s.anm", argv[1]);
    mm2_anm_file anm{};
    if (mm2_anm_load_file(path, &anm) != MM2_ANM_OK) {
        puts("fail");
        return 1;
    }
    const int cf = mm2_anm_default_overlay_composed_frame(&anm);
    mm2_anm_composite_rgba comp{};
    mm2_anm_composite_frame_rgba(&anm, cf, &comp);
    char out[64];
    snprintf(out, sizeof(out), "%s_c.ppm", argv[1]);
    writeppm(out, &comp);
    printf("%s composed=%d %dx%d\n", argv[1], cf, comp.width, comp.height);
    mm2_anm_composite_rgba_free(&comp);
    mm2_anm_free(&anm);
    return 0;
}
