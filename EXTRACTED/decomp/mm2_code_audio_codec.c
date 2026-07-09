#include "mm2_code_audio_codec.h"

#include "mm2_codec_platform.h"

static uint16_t mm2_be_u16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t mm2_be_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void mm2_be_store_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static void mm2_be_store_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

static int mm2_code_audio_min_size_ok(size_t n)
{
    /* Highest covered region ends at 0x7392 (exclusive). */
    return n >= (size_t)(MM2_CODE_AUDIO_OFF_730C + (MM2_CODE_AUDIO_SEQ_730C_COUNT * 4));
}

Mm2CodeAudioError mm2_code_audio_decode_from_data_hunk(
    const uint8_t *data_hunk,
    size_t data_hunk_size,
    Mm2CodeAudioTables *out)
{
    int i;
    if (!data_hunk || !out) {
        return MM2_CODE_AUDIO_ERR_BAD_ARGS;
    }
    if (!mm2_code_audio_min_size_ok(data_hunk_size)) {
        return MM2_CODE_AUDIO_ERR_BAD_SIZE;
    }

    memcpy(out->seq_73de, data_hunk + MM2_CODE_AUDIO_OFF_73DE, MM2_CODE_AUDIO_SEQ_73DE_COUNT);
    memcpy(out->seq_73d5, data_hunk + MM2_CODE_AUDIO_OFF_73D5, MM2_CODE_AUDIO_SEQ_73D5_COUNT);

    for (i = 0; i < MM2_CODE_AUDIO_SEQ_73A8_COUNT; i++) {
        out->seq_73a8[i] = mm2_be_u16(data_hunk + MM2_CODE_AUDIO_OFF_73A8 + (size_t)i * 2u);
        out->seq_7388[i] = mm2_be_u16(data_hunk + MM2_CODE_AUDIO_OFF_7388 + (size_t)i * 2u);
        out->seq_734a[i] = mm2_be_u16(data_hunk + MM2_CODE_AUDIO_OFF_734A + (size_t)i * 2u);
        out->seq_7338[i] = mm2_be_u16(data_hunk + MM2_CODE_AUDIO_OFF_7338 + (size_t)i * 2u);
    }
    for (i = 0; i < MM2_CODE_AUDIO_SEQ_7326_COUNT; i++) {
        out->seq_7326[i] = mm2_be_u16(data_hunk + MM2_CODE_AUDIO_OFF_7326 + (size_t)i * 2u);
    }
    for (i = 0; i < MM2_CODE_AUDIO_SEQ_730C_COUNT; i++) {
        out->seq_730c[i] = mm2_be_u32(data_hunk + MM2_CODE_AUDIO_OFF_730C + (size_t)i * 4u);
    }
    return MM2_CODE_AUDIO_OK;
}

Mm2CodeAudioError mm2_code_audio_encode_into_data_hunk(
    const Mm2CodeAudioTables *tables,
    uint8_t *data_hunk,
    size_t data_hunk_size)
{
    int i;
    if (!tables || !data_hunk) {
        return MM2_CODE_AUDIO_ERR_BAD_ARGS;
    }
    if (!mm2_code_audio_min_size_ok(data_hunk_size)) {
        return MM2_CODE_AUDIO_ERR_BAD_SIZE;
    }

    memcpy(data_hunk + MM2_CODE_AUDIO_OFF_73DE, tables->seq_73de, MM2_CODE_AUDIO_SEQ_73DE_COUNT);
    memcpy(data_hunk + MM2_CODE_AUDIO_OFF_73D5, tables->seq_73d5, MM2_CODE_AUDIO_SEQ_73D5_COUNT);

    for (i = 0; i < MM2_CODE_AUDIO_SEQ_73A8_COUNT; i++) {
        mm2_be_store_u16(data_hunk + MM2_CODE_AUDIO_OFF_73A8 + (size_t)i * 2u, tables->seq_73a8[i]);
        mm2_be_store_u16(data_hunk + MM2_CODE_AUDIO_OFF_7388 + (size_t)i * 2u, tables->seq_7388[i]);
        mm2_be_store_u16(data_hunk + MM2_CODE_AUDIO_OFF_734A + (size_t)i * 2u, tables->seq_734a[i]);
        mm2_be_store_u16(data_hunk + MM2_CODE_AUDIO_OFF_7338 + (size_t)i * 2u, tables->seq_7338[i]);
    }
    for (i = 0; i < MM2_CODE_AUDIO_SEQ_7326_COUNT; i++) {
        mm2_be_store_u16(data_hunk + MM2_CODE_AUDIO_OFF_7326 + (size_t)i * 2u, tables->seq_7326[i]);
    }
    for (i = 0; i < MM2_CODE_AUDIO_SEQ_730C_COUNT; i++) {
        mm2_be_store_u32(data_hunk + MM2_CODE_AUDIO_OFF_730C + (size_t)i * 4u, tables->seq_730c[i]);
    }
    return MM2_CODE_AUDIO_OK;
}

Mm2CodeAudioError mm2_code_audio_load_data_hunk_file(
    const char *path,
    Mm2CodeAudioTables *out)
{
    FILE *fp;
    uint8_t *buf;
    size_t size;
    size_t got;
    Mm2CodeAudioError err;

    if (!path || !out) {
        return MM2_CODE_AUDIO_ERR_BAD_ARGS;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_CODE_AUDIO_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    size = (size_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    buf = (uint8_t *)malloc(size);
    if (!buf) {
        fclose(fp);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    got = fread(buf, 1, size, fp);
    fclose(fp);
    if (got != size) {
        free(buf);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    err = mm2_code_audio_decode_from_data_hunk(buf, size, out);
    free(buf);
    return err;
}

Mm2CodeAudioError mm2_code_audio_save_data_hunk_file(
    const char *path,
    const Mm2CodeAudioTables *tables)
{
    FILE *fp;
    uint8_t *buf;
    size_t size;
    size_t got;
    Mm2CodeAudioError err;

    if (!path || !tables) {
        return MM2_CODE_AUDIO_ERR_BAD_ARGS;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_CODE_AUDIO_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    size = (size_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    buf = (uint8_t *)malloc(size);
    if (!buf) {
        fclose(fp);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    got = fread(buf, 1, size, fp);
    fclose(fp);
    if (got != size) {
        free(buf);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    err = mm2_code_audio_encode_into_data_hunk(tables, buf, size);
    if (err != MM2_CODE_AUDIO_OK) {
        free(buf);
        return err;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        free(buf);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    got = fwrite(buf, 1, size, fp);
    free(buf);
    if (got != size) {
        fclose(fp);
        return MM2_CODE_AUDIO_ERR_IO;
    }
    if (fclose(fp) != 0) {
        return MM2_CODE_AUDIO_ERR_IO;
    }
    return MM2_CODE_AUDIO_OK;
}

