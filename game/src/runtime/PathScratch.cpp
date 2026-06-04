#include "mm2/runtime/PathScratch.h"

static char s_mm2_path_scratch_a[MM2_PATH_SCRATCH_CAP];
static char s_mm2_path_scratch_b[MM2_PATH_SCRATCH_CAP];

char *mm2_path_scratch_a(void) { return s_mm2_path_scratch_a; }

char *mm2_path_scratch_b(void) { return s_mm2_path_scratch_b; }
