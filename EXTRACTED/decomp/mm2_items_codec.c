#include "mm2_items_codec.h"

#include "mm2_codec_platform.h"

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void write_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

Mm2ItemsError mm2_items_decode(const uint8_t *bytes, size_t bytes_size, Mm2ItemsFile *out)
{
    int i;
    if (!bytes || !out) {
        return MM2_ITEMS_ERR_BAD_ARGS;
    }
    if (bytes_size < (size_t)MM2_ITEMS_FILE_SIZE) {
        return MM2_ITEMS_ERR_BAD_SIZE;
    }
    for (i = 0; i < MM2_ITEMS_RECORD_COUNT; i++) {
        const uint8_t *p = bytes + i * MM2_ITEMS_RECORD_SIZE;
        Mm2ItemRecord *r = &out->records[i];
        memcpy(r->name, p, MM2_ITEMS_NAME_SIZE);
        r->separator = p[0x0C];
        r->forbidden_class_mask = p[0x0D];
        r->bonus_byte = p[0x0E];
        r->effect_byte = p[0x0F];
        r->damage = p[0x10];
        r->pad = p[0x11];
        r->gold = read_le16(p + 0x12);
    }
    return MM2_ITEMS_OK;
}

Mm2ItemsError mm2_items_encode(const Mm2ItemsFile *items, uint8_t *out_bytes, size_t out_size)
{
    int i;
    if (!items || !out_bytes) {
        return MM2_ITEMS_ERR_BAD_ARGS;
    }
    if (out_size < (size_t)MM2_ITEMS_FILE_SIZE) {
        return MM2_ITEMS_ERR_BAD_SIZE;
    }
    for (i = 0; i < MM2_ITEMS_RECORD_COUNT; i++) {
        uint8_t *p = out_bytes + i * MM2_ITEMS_RECORD_SIZE;
        const Mm2ItemRecord *r = &items->records[i];
        memcpy(p, r->name, MM2_ITEMS_NAME_SIZE);
        p[0x0C] = r->separator;
        p[0x0D] = r->forbidden_class_mask;
        p[0x0E] = r->bonus_byte;
        p[0x0F] = r->effect_byte;
        p[0x10] = r->damage;
        p[0x11] = r->pad;
        write_le16(p + 0x12, r->gold);
    }
    return MM2_ITEMS_OK;
}

Mm2ItemsError mm2_items_load_file(const char *path, Mm2ItemsFile *out)
{
    FILE *fp;
    uint8_t *buf;
    size_t got;
    Mm2ItemsError err;

    if (!path || !out) {
        return MM2_ITEMS_ERR_BAD_ARGS;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_ITEMS_ERR_IO;
    }
    buf = (uint8_t *)malloc(MM2_ITEMS_FILE_SIZE);
    if (!buf) {
        fclose(fp);
        return MM2_ITEMS_ERR_IO;
    }
    got = fread(buf, 1, MM2_ITEMS_FILE_SIZE, fp);
    if (got != (size_t)MM2_ITEMS_FILE_SIZE) {
        free(buf);
        fclose(fp);
        return MM2_ITEMS_ERR_BAD_SIZE;
    }
    if (fgetc(fp) != EOF) {
        free(buf);
        fclose(fp);
        return MM2_ITEMS_ERR_BAD_SIZE;
    }
    fclose(fp);
    err = mm2_items_decode(buf, MM2_ITEMS_FILE_SIZE, out);
    free(buf);
    return err;
}

Mm2ItemsError mm2_items_save_file(const char *path, const Mm2ItemsFile *items)
{
    FILE *fp;
    uint8_t *buf;
    size_t wrote;
    Mm2ItemsError err;

    if (!path || !items) {
        return MM2_ITEMS_ERR_BAD_ARGS;
    }
    buf = (uint8_t *)malloc(MM2_ITEMS_FILE_SIZE);
    if (!buf) {
        return MM2_ITEMS_ERR_IO;
    }
    err = mm2_items_encode(items, buf, MM2_ITEMS_FILE_SIZE);
    if (err != MM2_ITEMS_OK) {
        free(buf);
        return err;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        free(buf);
        return MM2_ITEMS_ERR_IO;
    }
    wrote = fwrite(buf, 1, MM2_ITEMS_FILE_SIZE, fp);
    free(buf);
    fclose(fp);
    return (wrote == (size_t)MM2_ITEMS_FILE_SIZE) ? MM2_ITEMS_OK : MM2_ITEMS_ERR_IO;
}

void mm2_item_name_to_cstr(const Mm2ItemRecord *item, char *out, size_t out_size)
{
    size_t end;
    if (!item || !out || out_size == 0) {
        return;
    }
    out[0] = '\0';

    end = MM2_ITEMS_NAME_SIZE;
    while (end > 0) {
        const uint8_t c = (uint8_t)item->name[end - 1];
        if (c != 0 && c != (uint8_t)' ') {
            break;
        }
        --end;
    }
    if (end >= out_size) {
        end = out_size - 1;
    }
    memcpy(out, item->name, end);
    out[end] = '\0';
}

int mm2_item_class_can_use(const Mm2ItemRecord *item, uint8_t class_id)
{
    uint8_t bit;
    if (!item || class_id >= 8) {
        return 0;
    }
    bit = (uint8_t)(0x80u >> class_id);
    return (item->forbidden_class_mask & bit) == 0;
}

const Mm2ItemRecord *mm2_items_lookup(const Mm2ItemsFile *items, uint8_t item_id)
{
    if (!items || item_id == 0) {
        return NULL;
    }
    return &items->records[item_id];
}
