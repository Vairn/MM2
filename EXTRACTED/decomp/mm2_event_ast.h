#ifndef MM2_EVENT_AST_H
#define MM2_EVENT_AST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lifted event-script AST — mirrors tools/mm2_event_lang/ast.py.
 * JSON interchange via mm2_event_ast_codec.c (stub). */

enum Mm2EvtRecordKind {
    MM2_EVT_KIND_STANDARD = 0,
    MM2_EVT_KIND_STRING_BANK = 1,
    MM2_EVT_KIND_MIXED_POOL = 2,
    MM2_EVT_KIND_CASTLE_BLOB = 3,
    MM2_EVT_KIND_UNKNOWN = 4
};

enum Mm2EvtTriggerCond {
    MM2_EVT_COND_ALWAYS = 0,
    MM2_EVT_COND_ENTER = 1,
    MM2_EVT_COND_FROM_NORTH = 2,
    MM2_EVT_COND_DIR_SPECIAL = 3,
    MM2_EVT_COND_ANY_DIRECTION = 4,
    MM2_EVT_COND_FACING_NS = 5,
    MM2_EVT_COND_ENTER_SPECIAL = 6,
    MM2_EVT_COND_RAW = 7
};

enum Mm2EvtStmtKind {
    MM2_EVT_STMT_SAY = 0,
    MM2_EVT_STMT_WAIT = 1,
    MM2_EVT_STMT_ASK_YES_NO = 2,
    MM2_EVT_STMT_IF = 3,
    MM2_EVT_STMT_QUEST = 4,
    MM2_EVT_STMT_SHOP = 5,
    MM2_EVT_STMT_GO_TO = 6,
    MM2_EVT_STMT_FIGHT = 7,
    MM2_EVT_STMT_ABORT = 8,
    MM2_EVT_STMT_END = 9,
    MM2_EVT_STMT_PLAIN_TEXT = 10,
    MM2_EVT_STMT_RAW_OP = 11,
    MM2_EVT_STMT_OTHER = 12
};

#define MM2_EVT_MAX_STRINGS   256
#define MM2_EVT_MAX_TRIGGERS  512
#define MM2_EVT_MAX_SCRIPTS   256
#define MM2_EVT_MAX_STMTS     512
#define MM2_EVT_NAME_LEN      64
#define MM2_EVT_TEXT_LEN      4096

typedef struct Mm2EvtStringDef {
    char name[MM2_EVT_NAME_LEN];
    char text[MM2_EVT_TEXT_LEN];
    int index;
} Mm2EvtStringDef;

typedef struct Mm2EvtTrigger {
    int y;
    int x;
    enum Mm2EvtTriggerCond cond;
    uint8_t cond_raw;
    char script_name[MM2_EVT_NAME_LEN];
    int event_id;
} Mm2EvtTrigger;

typedef struct Mm2EvtStmt {
    enum Mm2EvtStmtKind kind;
    char payload[MM2_EVT_TEXT_LEN];
} Mm2EvtStmt;

typedef struct Mm2EvtScript {
    char name[MM2_EVT_NAME_LEN];
    int event_id;
    int is_plain_text;
    int stmt_count;
    Mm2EvtStmt stmts[MM2_EVT_MAX_STMTS];
    const uint8_t *raw_segment;
    size_t raw_segment_len;
} Mm2EvtScript;

typedef struct Mm2EvtLocation {
    int id;
    enum Mm2EvtRecordKind record_kind;
    int terminated;
    int modified;
    int string_count;
    Mm2EvtStringDef strings[MM2_EVT_MAX_STRINGS];
    int trigger_count;
    Mm2EvtTrigger triggers[MM2_EVT_MAX_TRIGGERS];
    int script_count;
    Mm2EvtScript scripts[MM2_EVT_MAX_SCRIPTS];
    const uint8_t *raw_blob;
    size_t raw_blob_len;
} Mm2EvtLocation;

typedef struct Mm2EvtFile {
    int location_count;
    Mm2EvtLocation locations[71];
} Mm2EvtFile;

/* Stub codec — load/save JSON produced by Python mm2_event_lang (future). */
int mm2_event_ast_load_json(const char *path, Mm2EvtFile *out);
int mm2_event_ast_save_json(const char *path, const Mm2EvtFile *file);

#ifdef __cplusplus
}
#endif

#endif
