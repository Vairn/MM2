#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read an entire file into a mm2_malloc buffer. Flat call chain for 4K-stack Amiga builds.
 * Tries bare path, then dh1:/dh0: prefixes (same as platform readFile).
 * Returns 1 on success; caller mm2_free's *out_data.
 */
int mm2_amiga_read_file(const char *path, uint8_t **out_data, size_t *out_size);

#ifdef __cplusplus
}
#endif
