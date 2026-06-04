#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace mm2 {

/** Join data_dir + filename. Handles ".", "", and Amiga volumes (dh1:file). */
inline bool joinDataPath(char *out, size_t out_cap, const char *dir, const char *name)
{
    if (!out || !name || out_cap == 0) {
        return false;
    }
    if (!dir || dir[0] == '\0' || (dir[0] == '.' && dir[1] == '\0')) {
        const size_t name_len = strlen(name);
        if (name_len + 1 > out_cap) {
            return false;
        }
        snprintf(out, out_cap, "%s", name);
        return true;
    }

    const size_t dir_len = strlen(dir);
    const size_t name_len = strlen(name);
    const bool volume = dir_len > 0 && dir[dir_len - 1] == ':';
    const bool need_sep =
        !volume && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\';
    if (dir_len + name_len + (need_sep ? 1u : 0u) + 1u > out_cap) {
        return false;
    }
    snprintf(out, out_cap, "%s%s%s", dir, need_sep ? "/" : "", name);
    return true;
}

}  // namespace mm2
