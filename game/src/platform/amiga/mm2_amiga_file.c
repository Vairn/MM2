/**
 * Minimal-stack file read for Bartman 4K process stack.
 * Uses fopen + systemUse (same as mm2_roster_codec / mm2_image32_codec) — not
 * diskFileOpen, which overflows the stack from deep C++ call chains.
 */

#include "mm2/platform/amiga/mm2_amiga_file.h"

#ifdef AMIGA

#include "mm2/runtime/PathScratch.h"

#include <ace/managers/system.h>
#include <mini_std/stdio.h>
#include <mini_std/string.h>
#include <stddef.h>

void *mm2_malloc(size_t size);
void mm2_free(void *ptr);

static void strip_dot_slash(const char *path, char *out, size_t out_cap)
{
    if (!path || !out || out_cap == 0) {
        return;
    }
    if (path[0] == '.' && (path[1] == '/' || path[1] == '\\') && path[2]) {
        snprintf(out, out_cap, "%s", path + 2);
        return;
    }
    snprintf(out, out_cap, "%s", path);
}

static int join_path(char *out, size_t out_cap, const char *dir, const char *name)
{
    if (!out || !name || out_cap == 0) {
        return 0;
    }
    if (!dir || dir[0] == '\0' || (dir[0] == '.' && dir[1] == '\0')) {
        if (strlen(name) + 1 > out_cap) {
            return 0;
        }
        snprintf(out, out_cap, "%s", name);
        return 1;
    }
    const size_t dir_len = strlen(dir);
    const size_t name_len = strlen(name);
    const int volume = (dir_len > 0 && dir[dir_len - 1] == ':');
    const int need_sep = !volume && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\';
    if (dir_len + name_len + (need_sep ? 1u : 0u) + 1u > out_cap) {
        return 0;
    }
    snprintf(out, out_cap, "%s%s%s", dir, need_sep ? "/" : "", name);
    return 1;
}

static int read_at_path(const char *path, uint8_t **out_data, size_t *out_size)
{
    FILE *fp;
    long fsize_l;
    size_t fsize;
    size_t got;
    uint8_t *buf;

    systemUse();
    fp = fopen(path, "rb");
    if (!fp) {
        systemUnuse();
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        systemUnuse();
        return 0;
    }
    fsize_l = ftell(fp);
    if (fsize_l <= 0) {
        fclose(fp);
        systemUnuse();
        return 0;
    }
    fsize = (size_t)fsize_l;
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        systemUnuse();
        return 0;
    }

    buf = (uint8_t *)mm2_malloc(fsize);
    if (!buf) {
        fclose(fp);
        systemUnuse();
        return 0;
    }

    got = fread(buf, 1, fsize, fp);
    fclose(fp);
    systemUnuse();

    if (got != fsize) {
        mm2_free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = fsize;
    return 1;
}

int mm2_amiga_read_file(const char *path, uint8_t **out_data, size_t *out_size)
{
    if (!path || !out_data || !out_size) {
        return 0;
    }

    char *name = mm2_path_scratch_a();
    char *candidate = mm2_path_scratch_b();
    strip_dot_slash(path, name, MM2_PATH_SCRATCH_CAP);

    static const char *const k_prefixes[] = {"", "dh1:", "dh0:"};
    for (size_t i = 0; i < sizeof(k_prefixes) / sizeof(k_prefixes[0]); ++i) {
        if (!join_path(candidate, MM2_PATH_SCRATCH_CAP, k_prefixes[i], name)) {
            continue;
        }
        if (read_at_path(candidate, out_data, out_size)) {
            return 1;
        }
    }

    return read_at_path(name, out_data, out_size);
}

#endif /* AMIGA */
