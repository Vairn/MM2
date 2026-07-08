/* Dump LZW-decompressed wall sheet payload for golden debugging. */
#include "mm2_pc_gfx_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
    FILE *f;
    long sz;
    uint8_t *raw = NULL;
    size_t raw_len;
    uint32_t dec_size;
    uint8_t *dec = NULL;
    size_t dec_len = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: pc_lzw_dump <file.4|.16>\n");
        return 2;
    }
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    f = fopen(argv[1], "rb");
    if (!f) {
        return 1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 1;
    }
    sz = ftell(f);
    if (sz <= 8) {
        fclose(f);
        return 1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 1;
    }
    raw = (uint8_t *)malloc((size_t)sz);
    if (!raw || fread(raw, 1, (size_t)sz, f) != (size_t)sz) {
        free(raw);
        fclose(f);
        return 1;
    }
    fclose(f);
    raw_len = (size_t)sz;
    dec_size = raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24);
    dec = mm2_pc_lzw_decompress(raw + 4, raw_len - 4, dec_size, &dec_len);
    free(raw);
    if (!dec) {
        return 1;
    }
    printf("OK %zu\n", dec_len);
    if (fwrite(dec, 1, dec_len, stdout) != dec_len) {
        free(dec);
        return 1;
    }
    free(dec);
    return 0;
}
