#include "mm2_copyprot_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)((((uint16_t)p[0]) << 8) | ((uint16_t)p[1]));
}

static void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

void mm2_copyprot_free(mm2_copyprot_table *t) {
    if (!t) {
        return;
    }
    free(t->entries);
    t->entries = NULL;
    t->count = 0;
}

mm2_copyprot_error mm2_copyprot_decode(const uint8_t *raw, size_t raw_size, mm2_copyprot_table *out) {
    uint32_t i, count;

    if (!raw || !out) {
        return MM2_COPYPROT_ERR_BAD_FORMAT;
    }
    if ((raw_size % MM2_COPYPROT_ENTRY_SIZE) != 0) {
        return MM2_COPYPROT_ERR_BAD_FORMAT;
    }

    mm2_copyprot_free(out);
    count = (uint32_t)(raw_size / MM2_COPYPROT_ENTRY_SIZE);
    if (count == 0) {
        return MM2_COPYPROT_OK;
    }

    out->entries = (mm2_copyprot_entry *)calloc(count, sizeof(mm2_copyprot_entry));
    if (!out->entries) {
        return MM2_COPYPROT_ERR_NOMEM;
    }
    out->count = count;

    for (i = 0; i < count; i++) {
        size_t o = (size_t)i * MM2_COPYPROT_ENTRY_SIZE;
        mm2_copyprot_entry *e = &out->entries[i];
        e->w0 = read_be16(raw + o + 0);
        e->w1 = read_be16(raw + o + 2);
        e->checksum = read_be16(raw + o + 4);
        e->page_tens = (uint8_t)((e->w0 >> 8) & 0x0F);
        e->page_ones = (uint8_t)(e->w0 & 0x0F);
        e->paragraph = (uint8_t)((e->w1 >> 8) & 0x0F);
        e->line = (uint8_t)(e->w1 & 0x7F);
        e->page = (uint16_t)(e->page_tens * 10u + e->page_ones);
    }

    return MM2_COPYPROT_OK;
}

mm2_copyprot_error mm2_copyprot_encode(const mm2_copyprot_table *t, uint8_t **out_raw, size_t *out_size) {
    uint8_t *raw;
    uint32_t i;

    if (!t || !out_raw || !out_size) {
        return MM2_COPYPROT_ERR_BAD_FORMAT;
    }

    *out_raw = NULL;
    *out_size = 0;
    if (t->count == 0) {
        return MM2_COPYPROT_OK;
    }

    raw = (uint8_t *)malloc((size_t)t->count * MM2_COPYPROT_ENTRY_SIZE);
    if (!raw) {
        return MM2_COPYPROT_ERR_NOMEM;
    }

    for (i = 0; i < t->count; i++) {
        size_t o = (size_t)i * MM2_COPYPROT_ENTRY_SIZE;
        write_be16(raw + o + 0, t->entries[i].w0);
        write_be16(raw + o + 2, t->entries[i].w1);
        write_be16(raw + o + 4, t->entries[i].checksum);
    }

    *out_raw = raw;
    *out_size = (size_t)t->count * MM2_COPYPROT_ENTRY_SIZE;
    return MM2_COPYPROT_OK;
}

mm2_copyprot_error mm2_copyprot_load_file(const char *path, mm2_copyprot_table *out) {
    FILE *fp = NULL;
    uint8_t *buf = NULL;
    size_t sz, got;
    mm2_copyprot_error rc;

    if (!path || !out) {
        return MM2_COPYPROT_ERR_BAD_FORMAT;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_COPYPROT_ERR_IO;
    }
    fseek(fp, 0, SEEK_END);
    sz = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (uint8_t *)malloc(sz ? sz : 1);
    if (!buf) {
        fclose(fp);
        return MM2_COPYPROT_ERR_NOMEM;
    }

    got = fread(buf, 1, sz, fp);
    fclose(fp);
    if (got != sz) {
        free(buf);
        return MM2_COPYPROT_ERR_IO;
    }

    rc = mm2_copyprot_decode(buf, sz, out);
    free(buf);
    return rc;
}

mm2_copyprot_error mm2_copyprot_save_file(const char *path, const mm2_copyprot_table *t) {
    FILE *fp;
    uint8_t *raw = NULL;
    size_t raw_sz = 0;
    mm2_copyprot_error rc;
    size_t wrote;

    if (!path || !t) {
        return MM2_COPYPROT_ERR_BAD_FORMAT;
    }

    rc = mm2_copyprot_encode(t, &raw, &raw_sz);
    if (rc != MM2_COPYPROT_OK) {
        return rc;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        free(raw);
        return MM2_COPYPROT_ERR_IO;
    }
    wrote = fwrite(raw, 1, raw_sz, fp);
    fclose(fp);
    free(raw);

    if (wrote != raw_sz) {
        return MM2_COPYPROT_ERR_IO;
    }
    return MM2_COPYPROT_OK;
}

