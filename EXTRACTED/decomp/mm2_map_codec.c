#include "mm2_map_codec.h"

#include "mm2_codec_platform.h"

Mm2MapError mm2_map_decode(const uint8_t *bytes, size_t bytes_size, Mm2MapFile *out)
{
    size_t i;
    if (!bytes || !out) {
        return MM2_MAP_ERR_BAD_ARGS;
    }
    if (bytes_size != MM2_MAP_FILE_SIZE) {
        return MM2_MAP_ERR_BAD_SIZE;
    }
    for (i = 0; i < MM2_MAP_SCREEN_COUNT; i++) {
        const uint8_t *rec = bytes + (i * MM2_MAP_SCREEN_SIZE);
        memcpy(out->screens[i].visual, rec, MM2_MAP_PAGE_SIZE);
        memcpy(out->screens[i].collision, rec + MM2_MAP_PAGE_SIZE, MM2_MAP_PAGE_SIZE);
    }
    return MM2_MAP_OK;
}

Mm2MapError mm2_map_encode(const Mm2MapFile *map, uint8_t *out_bytes, size_t out_size)
{
    size_t i;
    if (!map || !out_bytes) {
        return MM2_MAP_ERR_BAD_ARGS;
    }
    if (out_size != MM2_MAP_FILE_SIZE) {
        return MM2_MAP_ERR_BAD_SIZE;
    }
    for (i = 0; i < MM2_MAP_SCREEN_COUNT; i++) {
        uint8_t *rec = out_bytes + (i * MM2_MAP_SCREEN_SIZE);
        memcpy(rec, map->screens[i].visual, MM2_MAP_PAGE_SIZE);
        memcpy(rec + MM2_MAP_PAGE_SIZE, map->screens[i].collision, MM2_MAP_PAGE_SIZE);
    }
    return MM2_MAP_OK;
}

Mm2MapError mm2_map_load_file(const char *path, Mm2MapFile *out)
{
    FILE *fp;
    uint8_t *buf;
    size_t got;
    Mm2MapError err;

    if (!path || !out) {
        return MM2_MAP_ERR_BAD_ARGS;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_MAP_ERR_IO;
    }
    buf = (uint8_t *)malloc(MM2_MAP_FILE_SIZE);
    if (!buf) {
        fclose(fp);
        return MM2_MAP_ERR_IO;
    }
    got = fread(buf, 1, MM2_MAP_FILE_SIZE, fp);
    if (got != MM2_MAP_FILE_SIZE) {
        free(buf);
        fclose(fp);
        return MM2_MAP_ERR_BAD_SIZE;
    }
    if (fgetc(fp) != EOF) {
        free(buf);
        fclose(fp);
        return MM2_MAP_ERR_BAD_SIZE;
    }
    fclose(fp);
    err = mm2_map_decode(buf, MM2_MAP_FILE_SIZE, out);
    free(buf);
    return err;
}

Mm2MapError mm2_map_save_file(const char *path, const Mm2MapFile *map)
{
    FILE *fp;
    uint8_t *buf;
    size_t wrote;
    Mm2MapError err;

    if (!path || !map) {
        return MM2_MAP_ERR_BAD_ARGS;
    }
    buf = (uint8_t *)malloc(MM2_MAP_FILE_SIZE);
    if (!buf) {
        return MM2_MAP_ERR_IO;
    }
    err = mm2_map_encode(map, buf, MM2_MAP_FILE_SIZE);
    if (err != MM2_MAP_OK) {
        free(buf);
        return err;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        free(buf);
        return MM2_MAP_ERR_IO;
    }
    wrote = fwrite(buf, 1, MM2_MAP_FILE_SIZE, fp);
    free(buf);
    if (wrote != MM2_MAP_FILE_SIZE) {
        fclose(fp);
        return MM2_MAP_ERR_IO;
    }
    if (fclose(fp) != 0) {
        return MM2_MAP_ERR_IO;
    }
    return MM2_MAP_OK;
}
