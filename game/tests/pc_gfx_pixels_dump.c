#include "mm2_pc_gfx_codec.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
    uint8_t *pixels = NULL;
    size_t len = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    int frame = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: pc_gfx_pixels_dump <file.4|.16> [frame]\n");
        return 2;
    }
    if (argc >= 3) {
        frame = atoi(argv[2]);
    }
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (mm2_pc_wall_frame_packed_pixels(argv[1], frame, &pixels, &len, &w, &h) != MM2_IMAGE32_OK) {
        fprintf(stderr, "parse failed\n");
        return 1;
    }
    printf("OK %u %u %zu\n", w, h, len);
    if (fwrite(pixels, 1, len, stdout) != len) {
        free(pixels);
        return 1;
    }
    free(pixels);
    return 0;
}
