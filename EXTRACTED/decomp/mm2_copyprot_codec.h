#ifndef MM2_COPYPROT_CODEC_H
#define MM2_COPYPROT_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM2_COPYPROT_ENTRY_WORDS 3u
#define MM2_COPYPROT_ENTRY_SIZE 6u

typedef struct mm2_copyprot_entry {
    uint16_t w0;
    uint16_t w1;
    uint16_t checksum; /* w2 */

    uint8_t page_tens; /* (w0 >> 8) & 0x0F */
    uint8_t page_ones; /*  w0       & 0x0F */
    uint8_t paragraph; /* (w1 >> 8) & 0x0F */
    uint8_t line;      /*  w1       & 0x7F */
    uint16_t page;     /* page_tens * 10 + page_ones */
} mm2_copyprot_entry;

typedef struct mm2_copyprot_table {
    uint32_t count;
    mm2_copyprot_entry *entries;
} mm2_copyprot_table;

typedef enum mm2_copyprot_error {
    MM2_COPYPROT_OK = 0,
    MM2_COPYPROT_ERR_IO = 1,
    MM2_COPYPROT_ERR_BAD_FORMAT = 2,
    MM2_COPYPROT_ERR_NOMEM = 3
} mm2_copyprot_error;

void mm2_copyprot_free(mm2_copyprot_table *t);

mm2_copyprot_error mm2_copyprot_decode(const uint8_t *raw, size_t raw_size, mm2_copyprot_table *out);
mm2_copyprot_error mm2_copyprot_encode(const mm2_copyprot_table *t, uint8_t **out_raw, size_t *out_size);

mm2_copyprot_error mm2_copyprot_load_file(const char *path, mm2_copyprot_table *out);
mm2_copyprot_error mm2_copyprot_save_file(const char *path, const mm2_copyprot_table *t);

#ifdef __cplusplus
}
#endif

#endif

