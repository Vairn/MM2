#include "mm2_event_ast.h"

#include <stdio.h>
#include <string.h>

int mm2_event_ast_load_json(const char *path, Mm2EvtFile *out) {
    (void)path;
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    fprintf(stderr, "mm2_event_ast_load_json: stub (use Python mm2_event_lang)\n");
    return -1;
}

int mm2_event_ast_save_json(const char *path, const Mm2EvtFile *file) {
    (void)path;
    (void)file;
    fprintf(stderr, "mm2_event_ast_save_json: stub (use Python mm2_event_lang)\n");
    return -1;
}
