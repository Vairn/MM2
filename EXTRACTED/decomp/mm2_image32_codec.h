#ifndef MM2_IMAGE32_CODEC_H
#define MM2_IMAGE32_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM2_IMAGE32_PLANES 5
#define MM2_IMAGE32_PALETTE_COLORS 32

typedef struct mm2_image32_frame {
    uint16_t width;
    uint16_t height;
    uint16_t flags;
    uint8_t *rgba; /* width*height*4, owned (PC / editor preview) */
    size_t rgba_size;
#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
    void *bitmap; /* tBitMap* — 6-plane on AGA playfield (5 from .32 + zero bp5) */
#ifndef MM2_AGA_SCREEN_BPP
#define MM2_AGA_SCREEN_BPP 6
#endif
#endif
} mm2_image32_frame;

typedef struct mm2_image32_file {
    uint16_t frame_count;
    uint16_t depth_or_mode;
    mm2_image32_frame *frames;
    char debug_label[32]; /* basename for ACE_DEBUG logs (copied at load) */
    uint8_t palette_rgba[MM2_IMAGE32_PALETTE_COLORS][4];
#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
    uint32_t palette_pen[MM2_IMAGE32_PALETTE_COLORS]; /* ACE 0x00RRGGBB pens */
#endif
} mm2_image32_file;

typedef enum mm2_image32_error {
    MM2_IMAGE32_OK = 0,
    MM2_IMAGE32_ERR_IO = 1,
    MM2_IMAGE32_ERR_BAD_FORMAT = 2,
    MM2_IMAGE32_ERR_NOMEM = 3
} mm2_image32_error;

size_t mm2_image32_rassize(uint16_t width, uint16_t height);
void mm2_image32_set_preview_opaque(int enabled);
mm2_image32_error mm2_image32_load_file(const char *path, mm2_image32_file *out);
mm2_image32_error mm2_image32_decode_buffer(const uint8_t *data, size_t size, mm2_image32_file *out);
void mm2_image32_free(mm2_image32_file *img);

#ifdef __cplusplus
}
#endif

#endif
