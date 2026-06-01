#include "mm2_globe_blob.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t k_table_counts[MM2_GLOBE_TABLE_COUNT] = {14, 18, 7, 15, 17, 10, 23};

static void free_tables(mm2_globe_blob *blob) {
    uint32_t ti, si;
    for (ti = 0; ti < MM2_GLOBE_TABLE_COUNT; ti++) {
        for (si = 0; si < blob->tables[ti].count; si++) {
            free(blob->tables[ti].items[si]);
        }
        free(blob->tables[ti].items);
        blob->tables[ti].items = NULL;
        blob->tables[ti].count = 0;
    }
    blob->parse_end_offset = 0;
}

void mm2_globe_init(mm2_globe_blob *blob) {
    if (!blob) {
        return;
    }
    memset(blob, 0, sizeof(*blob));
}

void mm2_globe_free(mm2_globe_blob *blob) {
    if (!blob) {
        return;
    }
    free_tables(blob);
    free(blob->raw);
    free(blob->decoded);
    memset(blob, 0, sizeof(*blob));
}

mm2_globe_error mm2_globe_load_file(const char *path, mm2_globe_blob *out) {
    FILE *fp = NULL;
    size_t fsize;
    size_t got;
    uint8_t *buf = NULL;

    if (!out) {
        return MM2_GLOBE_ERR_BAD_FORMAT;
    }
    mm2_globe_free(out);

    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_GLOBE_ERR_IO;
    }
    fseek(fp, 0, SEEK_END);
    fsize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (uint8_t *)malloc(fsize ? fsize : 1);
    if (!buf) {
        fclose(fp);
        return MM2_GLOBE_ERR_NOMEM;
    }

    got = fread(buf, 1, fsize, fp);
    fclose(fp);
    if (got != fsize) {
        free(buf);
        return MM2_GLOBE_ERR_IO;
    }

    out->raw = buf;
    out->raw_size = fsize;
    return MM2_GLOBE_OK;
}

mm2_globe_error mm2_globe_save_file(const char *path, const mm2_globe_blob *blob) {
    FILE *fp;
    size_t wrote;

    if (!blob || !blob->raw) {
        return MM2_GLOBE_ERR_BAD_FORMAT;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        return MM2_GLOBE_ERR_IO;
    }
    wrote = fwrite(blob->raw, 1, blob->raw_size, fp);
    fclose(fp);
    if (wrote != blob->raw_size) {
        return MM2_GLOBE_ERR_IO;
    }
    return MM2_GLOBE_OK;
}

mm2_globe_error mm2_globe_decode(mm2_globe_blob *blob, const uint8_t key[MM2_GLOBE_KEY_LEN], size_t xor_len) {
    size_t i, n;
    uint8_t *decoded;

    if (!blob || !blob->raw || !key) {
        return MM2_GLOBE_ERR_BAD_FORMAT;
    }

    free(blob->decoded);
    blob->decoded = NULL;
    blob->decoded_size = 0;
    free_tables(blob);

    decoded = (uint8_t *)malloc(blob->raw_size ? blob->raw_size : 1);
    if (!decoded) {
        return MM2_GLOBE_ERR_NOMEM;
    }
    memcpy(decoded, blob->raw, blob->raw_size);

    memcpy(blob->key, key, MM2_GLOBE_KEY_LEN);
    n = blob->raw_size < xor_len ? blob->raw_size : xor_len;
    for (i = 0; i < n; i++) {
        decoded[i] ^= key[i % MM2_GLOBE_KEY_LEN];
    }

    blob->decoded = decoded;
    blob->decoded_size = blob->raw_size;
    return MM2_GLOBE_OK;
}

mm2_globe_error mm2_globe_encode(mm2_globe_blob *blob, const uint8_t key[MM2_GLOBE_KEY_LEN], size_t xor_len) {
    size_t i, n;
    uint8_t *raw;

    if (!blob || !blob->decoded || !key) {
        return MM2_GLOBE_ERR_BAD_FORMAT;
    }

    free(blob->raw);
    blob->raw = NULL;
    blob->raw_size = 0;

    raw = (uint8_t *)malloc(blob->decoded_size ? blob->decoded_size : 1);
    if (!raw) {
        return MM2_GLOBE_ERR_NOMEM;
    }
    memcpy(raw, blob->decoded, blob->decoded_size);

    memcpy(blob->key, key, MM2_GLOBE_KEY_LEN);
    n = blob->decoded_size < xor_len ? blob->decoded_size : xor_len;
    for (i = 0; i < n; i++) {
        raw[i] ^= key[i % MM2_GLOBE_KEY_LEN];
    }

    blob->raw = raw;
    blob->raw_size = blob->decoded_size;
    return MM2_GLOBE_OK;
}

mm2_globe_error mm2_globe_parse_tables(mm2_globe_blob *blob) {
    uint32_t ti, si;
    size_t off;

    if (!blob || !blob->decoded) {
        return MM2_GLOBE_ERR_BAD_FORMAT;
    }

    free_tables(blob);
    off = 0;

    for (ti = 0; ti < MM2_GLOBE_TABLE_COUNT; ti++) {
        uint32_t count = k_table_counts[ti];
        char **items = (char **)calloc(count, sizeof(char *));
        if (!items) {
            free_tables(blob);
            return MM2_GLOBE_ERR_NOMEM;
        }
        blob->tables[ti].count = count;
        blob->tables[ti].items = items;

        for (si = 0; si < count; si++) {
            size_t end = off;
            size_t len;
            char *s;

            while (end < blob->decoded_size && blob->decoded[end] != 0) {
                end++;
            }
            if (end >= blob->decoded_size) {
                free_tables(blob);
                return MM2_GLOBE_ERR_BAD_FORMAT;
            }
            len = end - off;
            s = (char *)malloc(len + 1);
            if (!s) {
                free_tables(blob);
                return MM2_GLOBE_ERR_NOMEM;
            }
            memcpy(s, blob->decoded + off, len);
            s[len] = '\0';
            items[si] = s;
            off = end + 1;
        }
    }

    blob->parse_end_offset = off;
    return MM2_GLOBE_OK;
}
