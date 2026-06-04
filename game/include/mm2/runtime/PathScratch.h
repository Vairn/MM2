#pragma once

/**
 * File-path scratch buffers in BSS (not stack).
 * Amiga Bartman builds use a 4K stack; never put 512+ byte path arrays on the stack.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM2_PATH_SCRATCH_CAP 512u

/** Two buffers for nested joins (e.g. readFile name + candidate). */
char *mm2_path_scratch_a(void);
char *mm2_path_scratch_b(void);

#ifdef __cplusplus
}
namespace mm2 {
inline char *pathScratchA() { return mm2_path_scratch_a(); }
inline char *pathScratchB() { return mm2_path_scratch_b(); }
}  // namespace mm2
#endif
