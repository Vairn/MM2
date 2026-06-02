#ifndef MM2_EVENT_CODEC_H
#define MM2_EVENT_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MM2_EVENT_LOCATION_COUNT = 71,
    MM2_EVENT_HEADER_SIZE = MM2_EVENT_LOCATION_COUNT * 6,
    MM2_EVENT_MAX_RECORD = 0x8AC,
    MM2_EVENT_MAX_STRINGS = 256,
    MM2_EVENT_MAX_TRIPLETS = 512,
    MM2_EVENT_MAX_SEGMENTS = 256
};

typedef enum Mm2EventRecordKind {
    MM2_EVENT_KIND_STANDARD = 0,
    MM2_EVENT_KIND_STRING_BANK = 1,
    MM2_EVENT_KIND_CASTLE_BLOB = 2,
    MM2_EVENT_KIND_MIXED_POOL = 3,
    MM2_EVENT_KIND_UNKNOWN = 4
} Mm2EventRecordKind;

typedef struct Mm2EventTriplet {
    uint8_t pos;
    uint8_t event;
    uint8_t cond;
} Mm2EventTriplet;

typedef struct Mm2EventHeaderEntry {
    uint32_t offset;
    uint16_t length;
} Mm2EventHeaderEntry;

typedef struct Mm2EventLocation {
    int id;
    Mm2EventHeaderEntry header;
    Mm2EventRecordKind kind;
    int terminated;
    int triplet_count;
    int script_offset;
    int string_table_offset;
    int script_length;
    int segment_count;
    int string_count;
    Mm2EventTriplet triplets[MM2_EVENT_MAX_TRIPLETS];
    /* Raw record bytes (owned by file blob or caller buffer). */
    const uint8_t *raw;
    size_t raw_len;
} Mm2EventLocation;

typedef struct Mm2EventFile {
    uint8_t *data;
    size_t data_size;
    Mm2EventHeaderEntry header[MM2_EVENT_LOCATION_COUNT];
    Mm2EventLocation locations[MM2_EVENT_LOCATION_COUNT];
} Mm2EventFile;

typedef enum Mm2EventError {
    MM2_EVENT_OK = 0,
    MM2_EVENT_ERR_IO = 1,
    MM2_EVENT_ERR_BAD_ARGS = 2,
    MM2_EVENT_ERR_BAD_SIZE = 3
} Mm2EventError;

Mm2EventRecordKind mm2_event_classify_record(
    int terminated, int script_length, int string_count, int triplet_count);

Mm2EventError mm2_event_decode(const uint8_t *bytes, size_t size, Mm2EventFile *out);
Mm2EventError mm2_event_encode(const Mm2EventFile *file, uint8_t *out, size_t out_size);
Mm2EventError mm2_event_load_file(const char *path, Mm2EventFile *out);
Mm2EventError mm2_event_save_file(const char *path, const Mm2EventFile *file);

void mm2_event_free(Mm2EventFile *file);

#ifdef __cplusplus
}
#endif

#endif
