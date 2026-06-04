/* Dump frame 0 RGBA from mm2_image32_codec for golden tests. */
#include "mm2_image32_codec.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    mm2_image32_file img = {};
    mm2_image32_error err;
    const char *path;
    int frame_idx = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: image32_golden <file.32> [frame_index]\n");
        return 2;
    }
    path = argv[1];
    if (argc >= 3) {
        frame_idx = atoi(argv[2]);
    }

    err = mm2_image32_load_file(path, &img);
    if (err != MM2_IMAGE32_OK) {
        fprintf(stderr, "decode failed: %d\n", (int)err);
        return 1;
    }
    if (frame_idx < 0 || frame_idx >= (int)img.frame_count || !img.frames[frame_idx].rgba) {
        fprintf(stderr, "bad frame index %d (count=%u)\n", frame_idx, img.frame_count);
        mm2_image32_free(&img);
        return 1;
    }

    {
        const mm2_image32_frame *f = &img.frames[frame_idx];
        printf("OK %dx%d %zu\n", f->width, f->height, f->rgba_size);
        if (fwrite(f->rgba, 1, f->rgba_size, stdout) != f->rgba_size) {
            fprintf(stderr, "write failed\n");
            mm2_image32_free(&img);
            return 1;
        }
    }

    mm2_image32_free(&img);
    return 0;
}
