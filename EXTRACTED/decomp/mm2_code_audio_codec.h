#ifndef MM2_CODE_AUDIO_CODEC_H
#define MM2_CODE_AUDIO_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Code-driven audio sequence tables extracted from mm2_data_00.bin.
 *
 * These are not a standalone game file; they live in the A4 workspace image
 * loaded from DATA hunk 0. Offsets use A4 base 0x7FFE.
 */
enum {
    MM2_A4_BASE = 0x7FFE,
    MM2_CODE_AUDIO_OFF_73DE = MM2_A4_BASE - 0x73DE,
    MM2_CODE_AUDIO_OFF_73D5 = MM2_A4_BASE - 0x73D5,
    MM2_CODE_AUDIO_OFF_73A8 = MM2_A4_BASE - 0x73A8,
    MM2_CODE_AUDIO_OFF_7388 = MM2_A4_BASE - 0x7388,
    MM2_CODE_AUDIO_OFF_734A = MM2_A4_BASE - 0x734A,
    MM2_CODE_AUDIO_OFF_7338 = MM2_A4_BASE - 0x7338,
    MM2_CODE_AUDIO_OFF_7326 = MM2_A4_BASE - 0x7326,
    MM2_CODE_AUDIO_OFF_730C = MM2_A4_BASE - 0x730C
};

enum {
    MM2_CODE_AUDIO_SEQ_73DE_COUNT = 10,
    MM2_CODE_AUDIO_SEQ_73D5_COUNT = 10,
    MM2_CODE_AUDIO_SEQ_73A8_COUNT = 16,
    MM2_CODE_AUDIO_SEQ_7388_COUNT = 16,
    MM2_CODE_AUDIO_SEQ_734A_COUNT = 16,
    MM2_CODE_AUDIO_SEQ_7338_COUNT = 16,
    MM2_CODE_AUDIO_SEQ_7326_COUNT = 9,
    MM2_CODE_AUDIO_SEQ_730C_COUNT = 8
};

typedef struct Mm2CodeAudioTables {
    uint8_t seq_73de[MM2_CODE_AUDIO_SEQ_73DE_COUNT];
    uint8_t seq_73d5[MM2_CODE_AUDIO_SEQ_73D5_COUNT];
    uint16_t seq_73a8[MM2_CODE_AUDIO_SEQ_73A8_COUNT];
    uint16_t seq_7388[MM2_CODE_AUDIO_SEQ_7388_COUNT];
    uint16_t seq_734a[MM2_CODE_AUDIO_SEQ_734A_COUNT];
    uint16_t seq_7338[MM2_CODE_AUDIO_SEQ_7338_COUNT];
    uint16_t seq_7326[MM2_CODE_AUDIO_SEQ_7326_COUNT];
    uint32_t seq_730c[MM2_CODE_AUDIO_SEQ_730C_COUNT];
} Mm2CodeAudioTables;

typedef enum Mm2CodeAudioError {
    MM2_CODE_AUDIO_OK = 0,
    MM2_CODE_AUDIO_ERR_BAD_ARGS = 1,
    MM2_CODE_AUDIO_ERR_BAD_SIZE = 2,
    MM2_CODE_AUDIO_ERR_IO = 3
} Mm2CodeAudioError;

Mm2CodeAudioError mm2_code_audio_decode_from_data_hunk(
    const uint8_t *data_hunk,
    size_t data_hunk_size,
    Mm2CodeAudioTables *out);

Mm2CodeAudioError mm2_code_audio_encode_into_data_hunk(
    const Mm2CodeAudioTables *tables,
    uint8_t *data_hunk,
    size_t data_hunk_size);

Mm2CodeAudioError mm2_code_audio_load_data_hunk_file(
    const char *path,
    Mm2CodeAudioTables *out);

Mm2CodeAudioError mm2_code_audio_save_data_hunk_file(
    const char *path,
    const Mm2CodeAudioTables *tables);

#ifdef __cplusplus
}
#endif

#endif

