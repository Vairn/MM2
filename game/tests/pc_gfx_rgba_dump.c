/* Dump frame 0 RGBA from mm2_pc_gfx_codec for golden tests. */
#include "mm2_pc_gfx_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static mm2_gfx_sheet_role parse_role(const char *text)
{
    if (!text || strcmp(text, "sky") == 0) {
        return MM2_GFX_ROLE_SKY;
    }
    if (strcmp(text, "torch") == 0) {
        return MM2_GFX_ROLE_TORCH;
    }
    if (strcmp(text, "floor") == 0) {
        return MM2_GFX_ROLE_FLOOR;
    }
    if (strcmp(text, "title") == 0) {
        return MM2_GFX_ROLE_TITLE;
    }
    return MM2_GFX_ROLE_WALL;
}

int main(int argc, char **argv)
{
    mm2_gfx_sheet sheet = {};
    const char *path;
    mm2_gfx_sheet_role role = MM2_GFX_ROLE_WALL;
    int cga_palette = 1;
    int frame_idx = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: pc_gfx_rgba_dump <file.4|.16> [role] [frame] [cga_palette]\n");
        return 2;
    }
    path = argv[1];
    if (argc >= 3) {
        role = parse_role(argv[2]);
    }
    if (argc >= 4) {
        frame_idx = atoi(argv[3]);
    }
    if (argc >= 5) {
        cga_palette = atoi(argv[4]) ? 1 : 0;
    }

#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    mm2_pc_gfx_set_cga_palette(cga_palette);
    if (mm2_pc_wall_sheet_load(path, role, NULL, &sheet) != MM2_IMAGE32_OK) {
        fprintf(stderr, "decode failed\n");
        return 1;
    }
    if (frame_idx < 0 || frame_idx >= (int)sheet.img.frame_count) {
        fprintf(stderr, "bad frame index\n");
        mm2_gfx_sheet_free(&sheet);
        return 1;
    }
    {
        const mm2_gfx_frame *f = mm2_gfx_sheet_frame(&sheet, frame_idx);
        if (!f || !f->rgba) {
            fprintf(stderr, "missing rgba\n");
            mm2_gfx_sheet_free(&sheet);
            return 1;
        }
        printf("OK %dx%d %zu\n", f->width, f->height, f->rgba_size);
        if (fwrite(f->rgba, 1, f->rgba_size, stdout) != f->rgba_size) {
            fprintf(stderr, "write failed\n");
            mm2_gfx_sheet_free(&sheet);
            return 1;
        }
    }
    mm2_gfx_sheet_free(&sheet);
    return 0;
}
