#include "mm2_event_codec.h"

#include "mm2_codec_platform.h"

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

Mm2EventRecordKind mm2_event_classify_record(
    int terminated, int script_length, int string_count, int triplet_count)
{
    if (!terminated) return MM2_EVENT_KIND_CASTLE_BLOB;
    if (script_length == 0 && string_count > 0) return MM2_EVENT_KIND_STRING_BANK;
    if (script_length > 500) return MM2_EVENT_KIND_MIXED_POOL;
    if (triplet_count > 0 && script_length > 0) return MM2_EVENT_KIND_STANDARD;
    return MM2_EVENT_KIND_UNKNOWN;
}

static int decode_triplets(
    const uint8_t *blob, size_t len, Mm2EventLocation *loc, int *pos_out, int *terminated_out)
{
    size_t pos = 0;
    int terminated = 0;
    loc->triplet_count = 0;

    while (pos + 2 < len) {
        uint8_t a = blob[pos], b = blob[pos + 1], c = blob[pos + 2];
        pos += 3;
        if (a == 0 && b == 0 && c == 0) {
            terminated = 1;
            break;
        }
        if (loc->triplet_count < MM2_EVENT_MAX_TRIPLETS) {
            Mm2EventTriplet *t = &loc->triplets[loc->triplet_count++];
            t->pos = a;
            t->event = b;
            t->cond = c;
        }
    }
    *pos_out = (int)pos;
    *terminated_out = terminated;
    return 0;
}

static int count_segments(const uint8_t *script, size_t len)
{
    int count = 0;
    size_t i;
    for (i = 0; i < len; i++) {
        if (script[i] == 0xFF) count++;
    }
    if (len > 0 && script[len - 1] != 0xFF) count++;
    return count;
}

static int count_strings(const uint8_t *blob, size_t len, size_t start)
{
    int n = 0;
    size_t pos = start;
    while (pos < len) {
        while (pos < len && blob[pos] != 0xFF) pos++;
        n++;
        pos++;
    }
    return n;
}

Mm2EventError mm2_event_decode(const uint8_t *bytes, size_t size, Mm2EventFile *out)
{
    int i;
    if (!bytes || !out) return MM2_EVENT_ERR_BAD_ARGS;
    memset(out, 0, sizeof(*out));
    if (size < MM2_EVENT_HEADER_SIZE) return MM2_EVENT_ERR_BAD_SIZE;

    for (i = 0; i < MM2_EVENT_LOCATION_COUNT; i++) {
        const uint8_t *hp = bytes + i * 6;
        out->header[i].offset = read_be32(hp);
        out->header[i].length = read_be16(hp + 4);
    }

    for (i = 0; i < MM2_EVENT_LOCATION_COUNT; i++) {
        Mm2EventLocation *loc = &out->locations[i];
        uint32_t off = out->header[i].offset;
        uint16_t length = out->header[i].length;
        size_t end;
        int pos = 0;
        int terminated = 0;
        int str_rel;
        int script_len;

        loc->id = i;
        loc->header = out->header[i];
        if (off >= size) continue;
        end = off + length;
        if (end > size) end = size;
        loc->raw = bytes + off;
        loc->raw_len = end - off;

        decode_triplets(loc->raw, loc->raw_len, loc, &pos, &terminated);
        loc->terminated = terminated;

        if ((size_t)pos + 1 < loc->raw_len) {
            str_rel = loc->raw[pos] | (loc->raw[pos + 1] << 8);
        } else {
            str_rel = 0;
        }
        loc->script_offset = pos + 2;
        loc->string_table_offset = pos + str_rel;
        if (loc->string_table_offset < loc->script_offset)
            script_len = 0;
        else
            script_len = loc->string_table_offset - loc->script_offset;
        loc->script_length = script_len;

        if (script_len > 0 && loc->script_offset >= 0 &&
            (size_t)loc->script_offset + (size_t)script_len <= loc->raw_len) {
            loc->segment_count = count_segments(
                loc->raw + loc->script_offset, (size_t)script_len);
        }
        if (loc->string_table_offset >= 0 &&
            (size_t)loc->string_table_offset < loc->raw_len) {
            loc->string_count = count_strings(
                loc->raw, loc->raw_len, (size_t)loc->string_table_offset);
        }
        loc->kind = mm2_event_classify_record(
            terminated, script_len, loc->string_count, loc->triplet_count);
    }
    return MM2_EVENT_OK;
}

Mm2EventError mm2_event_encode(const Mm2EventFile *file, uint8_t *out, size_t out_size)
{
    size_t cursor;
    int i;
    if (!file || !out) return MM2_EVENT_ERR_BAD_ARGS;

    cursor = MM2_EVENT_HEADER_SIZE;
    for (i = 0; i < MM2_EVENT_LOCATION_COUNT; i++) {
        cursor += file->header[i].length;
    }
    if (cursor != out_size) return MM2_EVENT_ERR_BAD_SIZE;
    if (out_size < MM2_EVENT_HEADER_SIZE) return MM2_EVENT_ERR_BAD_SIZE;

    for (i = 0; i < MM2_EVENT_LOCATION_COUNT; i++) {
        uint8_t *hp = out + i * 6;
        write_be32(hp, file->header[i].offset);
        write_be16(hp + 4, file->header[i].length);
    }

    for (i = 0; i < MM2_EVENT_LOCATION_COUNT; i++) {
        const Mm2EventLocation *loc = &file->locations[i];
        if (!loc->raw || loc->raw_len == 0) continue;
        if (loc->header.offset + loc->raw_len > out_size) return MM2_EVENT_ERR_BAD_SIZE;
        memcpy(out + loc->header.offset, loc->raw, loc->raw_len);
    }
    return MM2_EVENT_OK;
}

Mm2EventError mm2_event_load_file(const char *path, Mm2EventFile *out)
{
    FILE *f;
    long sz;
    uint8_t *buf;
    Mm2EventError err;

    if (!path || !out) return MM2_EVENT_ERR_BAD_ARGS;
    f = fopen(path, "rb");
    if (!f) return MM2_EVENT_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return MM2_EVENT_ERR_IO; }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return MM2_EVENT_ERR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return MM2_EVENT_ERR_IO; }
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return MM2_EVENT_ERR_IO; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return MM2_EVENT_ERR_IO;
    }
    fclose(f);
    err = mm2_event_decode(buf, (size_t)sz, out);
    if (err != MM2_EVENT_OK) {
        free(buf);
        return err;
    }
    out->data = buf;
    out->data_size = (size_t)sz;
    return MM2_EVENT_OK;
}

Mm2EventError mm2_event_save_file(const char *path, const Mm2EventFile *file)
{
    FILE *f;
    if (!path || !file || !file->data) return MM2_EVENT_ERR_BAD_ARGS;
    f = fopen(path, "wb");
    if (!f) return MM2_EVENT_ERR_IO;
    if (fwrite(file->data, 1, file->data_size, f) != file->data_size) {
        fclose(f);
        return MM2_EVENT_ERR_IO;
    }
    fclose(f);
    return MM2_EVENT_OK;
}

void mm2_event_free(Mm2EventFile *file)
{
    if (!file) return;
    free(file->data);
    file->data = NULL;
    file->data_size = 0;
}
