#ifndef MM2_ANM_CODEC_H
#define MM2_ANM_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM2_ANM_PRELUDE_SLOTS 11
#define MM2_ANM_PLANES 5
#define MM2_ANM_PALETTE_COLORS 32

typedef struct mm2_anm_prelude_slot {
    uint8_t a;
    uint8_t b;
    uint8_t w;
    uint8_t h;
    uint8_t is_used;
} mm2_anm_prelude_slot;

typedef struct mm2_anm_frame_info {
    uint16_t width;
    uint16_t height;
    uint16_t flags;
    uint8_t *planes; /* Owned buffer, size=MM2_ANM_PLANES * rassize(width,height) */
    size_t planes_size;
} mm2_anm_frame_info;

typedef struct mm2_anm_file {
    uint8_t magic[4]; /* usually 00 00 54 56 */
    mm2_anm_prelude_slot prelude[MM2_ANM_PRELUDE_SLOTS];
    uint8_t seq_header_a;
    uint8_t seq_header_b;
    uint8_t seq_header_c;

    uint8_t *sequence_blob; /* raw sequence bytes between 0x33 and image chunk marker */
    size_t sequence_blob_size;

    uint16_t frame_count;
    uint16_t depth_or_mode; /* observed 3 */
    mm2_anm_frame_info *frames;
    uint16_t palette_words[MM2_ANM_PALETTE_COLORS];
} mm2_anm_file;

typedef enum mm2_anm_error {
    MM2_ANM_OK = 0,
    MM2_ANM_ERR_IO = 1,
    MM2_ANM_ERR_BAD_MAGIC = 2,
    MM2_ANM_ERR_BAD_FORMAT = 3,
    MM2_ANM_ERR_NOMEM = 4
} mm2_anm_error;

size_t mm2_anm_rassize(uint16_t width, uint16_t height);
mm2_anm_error mm2_anm_load_file(const char *path, mm2_anm_file *out);
mm2_anm_error mm2_anm_save_file(const char *path, const mm2_anm_file *anm);
void mm2_anm_free(mm2_anm_file *anm);

#ifdef __cplusplus
}
#endif

#endif

