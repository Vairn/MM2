/* Dump composed 96x96 combat RGBA from mm2_pc_monsters_codec for golden tests. */
#include "mm2_pc_monsters_codec.h"
#include "mm2_pc_gfx_codec.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
    mm2_pc_monsters_atlas atlas = {};
    mm2_pc_monster_picture pic = {};
    int picture_id = 1;
    int frame_idx = 0;
    int cga_palette = 1;
    uint8_t rgba[MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H * 4];

    if (argc < 3) {
        fprintf(stderr, "usage: pc_monsters_rgba_dump <MONSTERS.4|.16> <picture_id> [frame] [cga_palette]\n");
        return 2;
    }
    picture_id = atoi(argv[2]);
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
    if (mm2_pc_monsters_atlas_load(argv[1], &atlas) != MM2_IMAGE32_OK) {
        fprintf(stderr, "atlas load failed\n");
        return 1;
    }
    if (mm2_pc_monsters_picture_load(&atlas, picture_id, &pic) != MM2_IMAGE32_OK) {
        fprintf(stderr, "picture load failed\n");
        mm2_pc_monsters_atlas_free(&atlas);
        return 1;
    }
    if (mm2_pc_monsters_composite_rgba(&pic, frame_idx, cga_palette, rgba) != MM2_IMAGE32_OK) {
        fprintf(stderr, "composite failed\n");
        mm2_pc_monster_picture_free(&pic);
        mm2_pc_monsters_atlas_free(&atlas);
        return 1;
    }
    printf("OK %dx%d %zu\n", MM2_PC_COMBAT_CANVAS_W, MM2_PC_COMBAT_CANVAS_H, sizeof(rgba));
    if (fwrite(rgba, 1, sizeof(rgba), stdout) != sizeof(rgba)) {
        fprintf(stderr, "write failed\n");
        mm2_pc_monster_picture_free(&pic);
        mm2_pc_monsters_atlas_free(&atlas);
        return 1;
    }
    mm2_pc_monster_picture_free(&pic);
    mm2_pc_monsters_atlas_free(&atlas);
    return 0;
}
