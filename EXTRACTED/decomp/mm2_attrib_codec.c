#include "mm2_attrib_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Mm2AttribError mm2_attrib_decode(const uint8_t *bytes, size_t bytes_size, Mm2AttribFile *out)
{
    size_t i;
    if (!bytes || !out) {
        return MM2_ATTRIB_ERR_BAD_ARGS;
    }
    if (bytes_size != MM2_ATTRIB_FILE_SIZE) {
        return MM2_ATTRIB_ERR_BAD_SIZE;
    }
    for (i = 0; i < MM2_ATTRIB_RECORD_COUNT; i++) {
        memcpy(out->records[i].raw, bytes + (i * MM2_ATTRIB_RECORD_SIZE), MM2_ATTRIB_RECORD_SIZE);
    }
    return MM2_ATTRIB_OK;
}

Mm2AttribError mm2_attrib_encode(const Mm2AttribFile *attrib, uint8_t *out_bytes, size_t out_size)
{
    size_t i;
    if (!attrib || !out_bytes) {
        return MM2_ATTRIB_ERR_BAD_ARGS;
    }
    if (out_size != MM2_ATTRIB_FILE_SIZE) {
        return MM2_ATTRIB_ERR_BAD_SIZE;
    }
    for (i = 0; i < MM2_ATTRIB_RECORD_COUNT; i++) {
        memcpy(out_bytes + (i * MM2_ATTRIB_RECORD_SIZE), attrib->records[i].raw, MM2_ATTRIB_RECORD_SIZE);
    }
    return MM2_ATTRIB_OK;
}

Mm2AttribError mm2_attrib_load_file(const char *path, Mm2AttribFile *out)
{
    FILE *fp;
    uint8_t *buf;
    size_t got;
    Mm2AttribError err;

    if (!path || !out) {
        return MM2_ATTRIB_ERR_BAD_ARGS;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_ATTRIB_ERR_IO;
    }
    buf = (uint8_t *)malloc(MM2_ATTRIB_FILE_SIZE);
    if (!buf) {
        fclose(fp);
        return MM2_ATTRIB_ERR_IO;
    }
    got = fread(buf, 1, MM2_ATTRIB_FILE_SIZE, fp);
    if (got != MM2_ATTRIB_FILE_SIZE) {
        free(buf);
        fclose(fp);
        return MM2_ATTRIB_ERR_BAD_SIZE;
    }
    if (fgetc(fp) != EOF) {
        free(buf);
        fclose(fp);
        return MM2_ATTRIB_ERR_BAD_SIZE;
    }
    fclose(fp);
    err = mm2_attrib_decode(buf, MM2_ATTRIB_FILE_SIZE, out);
    free(buf);
    return err;
}

Mm2AttribError mm2_attrib_save_file(const char *path, const Mm2AttribFile *attrib)
{
    FILE *fp;
    uint8_t *buf;
    size_t wrote;
    Mm2AttribError err;

    if (!path || !attrib) {
        return MM2_ATTRIB_ERR_BAD_ARGS;
    }
    buf = (uint8_t *)malloc(MM2_ATTRIB_FILE_SIZE);
    if (!buf) {
        return MM2_ATTRIB_ERR_IO;
    }
    err = mm2_attrib_encode(attrib, buf, MM2_ATTRIB_FILE_SIZE);
    if (err != MM2_ATTRIB_OK) {
        free(buf);
        return err;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        free(buf);
        return MM2_ATTRIB_ERR_IO;
    }
    wrote = fwrite(buf, 1, MM2_ATTRIB_FILE_SIZE, fp);
    free(buf);
    if (wrote != MM2_ATTRIB_FILE_SIZE) {
        fclose(fp);
        return MM2_ATTRIB_ERR_IO;
    }
    if (fclose(fp) != 0) {
        return MM2_ATTRIB_ERR_IO;
    }
    return MM2_ATTRIB_OK;
}

uint8_t mm2_attrib_area_id(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_AREA_ID] : 0; }
uint8_t mm2_attrib_map_category(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_MAP_CATEGORY] : 0; }
uint8_t mm2_attrib_tileset(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_TILESET] : 0; }
uint8_t mm2_attrib_env_type(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_ENV_TYPE] : 0; }
uint8_t mm2_attrib_surface_flag(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_SURFACE_FLAG] : 0; }

int mm2_attrib_is_outside(const Mm2AttribRecord *r)
{
    return r && r->raw[MM2_ATTRIB_OFF_SURFACE_FLAG] != 0;
}

int mm2_attrib_is_outdoor(const Mm2AttribRecord *r)
{
    if (!r) {
        return 0;
    }
    return mm2_is_outdoor_area((int)r->raw[MM2_ATTRIB_OFF_AREA_ID],
                               r->raw[MM2_ATTRIB_OFF_SURFACE_FLAG]);
}

uint8_t mm2_attrib_neighbor(const Mm2AttribRecord *r, int slot)
{
    if (!r || slot < 0 || slot > 3) {
        return 0;
    }
    return r->raw[MM2_ATTRIB_OFF_NEIGHBOR0 + slot];
}

uint16_t mm2_attrib_complex_id(const Mm2AttribRecord *r)
{
    if (!r) {
        return 0;
    }
    return (uint16_t)((r->raw[MM2_ATTRIB_OFF_COMPLEX_ID] << 8) | r->raw[MM2_ATTRIB_OFF_COMPLEX_ID + 1]);
}

uint8_t mm2_attrib_level(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_LEVEL] : 0; }
uint8_t mm2_attrib_era_gate(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_ERA_GATE] : 0; }
uint8_t mm2_attrib_transition_screen(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_LINK_AREA] : 0; }
uint8_t mm2_attrib_flags(const Mm2AttribRecord *r) { return r ? r->raw[MM2_ATTRIB_OFF_FLAGS] : 0; }

int mm2_attrib_flag_bit(const Mm2AttribRecord *r, int bit)
{
    if (!r || bit < 0 || bit > 7) {
        return 0;
    }
    return (r->raw[MM2_ATTRIB_OFF_FLAGS] >> bit) & 1;
}

void mm2_attrib_unpack_coord(uint8_t packed, int *x, int *y)
{
    if (x) {
        *x = packed & 0x0F;
    }
    if (y) {
        *y = (packed >> 4) & 0x0F;
    }
}

int mm2_attrib_roof_bit(const Mm2AttribRecord *r, int tile)
{
    int byte;
    if (!r || tile < 0 || tile >= MM2_ATTRIB_TILE_COUNT) {
        return 0;
    }
    byte = MM2_ATTRIB_ROOF_OFFSET + (tile >> 3);
    return (r->raw[byte] >> (tile & 7)) & 1;
}

void mm2_attrib_set_roof_bit(Mm2AttribRecord *r, int tile, int value)
{
    int byte;
    uint8_t mask;
    if (!r || tile < 0 || tile >= MM2_ATTRIB_TILE_COUNT) {
        return;
    }
    byte = MM2_ATTRIB_ROOF_OFFSET + (tile >> 3);
    mask = (uint8_t)(1u << (tile & 7));
    if (value) {
        r->raw[byte] |= mask;
    } else {
        r->raw[byte] &= (uint8_t)~mask;
    }
}
