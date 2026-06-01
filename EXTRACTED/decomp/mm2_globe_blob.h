#ifndef MM2_GLOBE_BLOB_H
#define MM2_GLOBE_BLOB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ASM-confirmed decode path:
 *   - XOR loop at 0x2613E (duplicate at 0x263A8)
 *   - XOR first 0x0BE1 bytes with a 5-byte rolling key
 *   - Then walk seven NUL string tables with counts:
 *       14, 18, 7, 15, 17, 10, 23
 */
#define MM2_GLOBE_XOR_LEN 0x0BE1u
#define MM2_GLOBE_KEY_LEN 5u
#define MM2_GLOBE_TABLE_COUNT 7u

typedef struct mm2_globe_table {
    uint32_t count;
    char **items; /* NUL-terminated strings, owned by blob */
} mm2_globe_table;

typedef struct mm2_globe_blob {
    uint8_t key[MM2_GLOBE_KEY_LEN];

    uint8_t *raw;
    size_t raw_size;

    uint8_t *decoded;
    size_t decoded_size;

    mm2_globe_table tables[MM2_GLOBE_TABLE_COUNT];
    size_t parse_end_offset;
} mm2_globe_blob;

typedef enum mm2_globe_error {
    MM2_GLOBE_OK = 0,
    MM2_GLOBE_ERR_IO = 1,
    MM2_GLOBE_ERR_BAD_FORMAT = 2,
    MM2_GLOBE_ERR_NOMEM = 3
} mm2_globe_error;

void mm2_globe_init(mm2_globe_blob *blob);
void mm2_globe_free(mm2_globe_blob *blob);

mm2_globe_error mm2_globe_load_file(const char *path, mm2_globe_blob *out);
mm2_globe_error mm2_globe_save_file(const char *path, const mm2_globe_blob *blob);

mm2_globe_error mm2_globe_decode(mm2_globe_blob *blob, const uint8_t key[MM2_GLOBE_KEY_LEN], size_t xor_len);
mm2_globe_error mm2_globe_encode(mm2_globe_blob *blob, const uint8_t key[MM2_GLOBE_KEY_LEN], size_t xor_len);

mm2_globe_error mm2_globe_parse_tables(mm2_globe_blob *blob);

#ifdef __cplusplus
}
#endif

#endif
