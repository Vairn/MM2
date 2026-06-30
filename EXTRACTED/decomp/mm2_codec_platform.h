#ifndef MM2_CODEC_PLATFORM_H
#define MM2_CODEC_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#if defined(MM2_HOST_AMIGA) || defined(MM2_CODEC_AMIGA)

#include <ace/managers/system.h>
#include <mini_std/stdio.h>
#include <mini_std/stdlib.h>
#include <string.h>

/* ACE 1.8.x bitmap.h lacks BMF_EXTERNAL (master: plane mem owned externally). */
#ifndef BMF_EXTERNAL
#define BMF_EXTERNAL (1 << 7)
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern void *mm2_malloc(size_t size);
extern void *mm2_malloc_fast(size_t size);
extern void mm2_free(void *ptr);
#ifdef __cplusplus
}
#endif

/* Short-lived .32 read/decode scratch (freed before bitmapCreate). CHIP is fine;
 * WinUAE often has no FAST RAM — avoid a failed FAST alloc + WARN per file. */
static inline void *codec_malloc(size_t size) { return mm2_malloc(size); }

static inline void codec_free(void *ptr) { mm2_free(ptr); }

static inline void *codec_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *p = mm2_malloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

#define malloc codec_malloc
#define free codec_free
#define calloc codec_calloc

static inline int fputc(int ch, FILE *stream)
{
    unsigned char byte = (unsigned char)ch;
    return (fwrite(&byte, 1, 1, stream) == 1) ? ch : EOF;
}

static inline int fgetc(FILE *stream)
{
    unsigned char byte = 0;
    if (fread(&byte, 1, 1, stream) != 1) {
        return EOF;
    }
    return (int)byte;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif

#endif /* MM2_CODEC_PLATFORM_H */
