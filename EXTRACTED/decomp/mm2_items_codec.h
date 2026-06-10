#ifndef MM2_ITEMS_CODEC_H
#define MM2_ITEMS_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * items.dat — 256 records × 20 bytes (0x14) = 5120 bytes.
 * See EXTRACTED/docs/18-items-dat-format.md.
 *
 * Per record:
 *   0x00-0x0B  name (12 ASCII, space-padded)
 *   0x0C       separator (typically 0)
 *   0x0D       forbidden-class bitmask (set bit = class CANNOT use)
 *   0x0E       bonus byte (hi=type, lo=amount)
 *   0x0F       use-power byte (flat spell index, not a nibble pair)
 *   0x10       damage
 *   0x11       pad
 *   0x12-0x13  gold (uint16 LE on disk)
 */

enum {
    MM2_ITEMS_RECORD_COUNT = 256,
    MM2_ITEMS_RECORD_SIZE = 0x14,
    MM2_ITEMS_NAME_SIZE = 12,
    MM2_ITEMS_FILE_SIZE = MM2_ITEMS_RECORD_COUNT * MM2_ITEMS_RECORD_SIZE
};

/* Class bit order K P A C S R N B = 0x80..0x01 (class_id 0..7). */
enum {
    MM2_ITEM_CLASS_KNIGHT = 0x80,
    MM2_ITEM_CLASS_PALADIN = 0x40,
    MM2_ITEM_CLASS_ARCHER = 0x20,
    MM2_ITEM_CLASS_CLERIC = 0x10,
    MM2_ITEM_CLASS_SORCERER = 0x08,
    MM2_ITEM_CLASS_ROBBER = 0x04,
    MM2_ITEM_CLASS_NINJA = 0x02,
    MM2_ITEM_CLASS_BARBARIAN = 0x01
};

typedef struct Mm2ItemRecord {
    char name[MM2_ITEMS_NAME_SIZE];
    uint8_t separator;
    uint8_t forbidden_class_mask;
    uint8_t bonus_byte;
    uint8_t effect_byte;
    uint8_t damage;
    uint8_t pad;
    uint16_t gold;
} Mm2ItemRecord;

typedef struct Mm2ItemsFile {
    Mm2ItemRecord records[MM2_ITEMS_RECORD_COUNT];
} Mm2ItemsFile;

typedef enum Mm2ItemsError {
    MM2_ITEMS_OK = 0,
    MM2_ITEMS_ERR_IO = 1,
    MM2_ITEMS_ERR_BAD_ARGS = 2,
    MM2_ITEMS_ERR_BAD_SIZE = 3
} Mm2ItemsError;

Mm2ItemsError mm2_items_decode(const uint8_t *bytes, size_t bytes_size, Mm2ItemsFile *out);
Mm2ItemsError mm2_items_encode(const Mm2ItemsFile *items, uint8_t *out_bytes, size_t out_size);
Mm2ItemsError mm2_items_load_file(const char *path, Mm2ItemsFile *out);
Mm2ItemsError mm2_items_save_file(const char *path, const Mm2ItemsFile *items);

void mm2_item_name_to_cstr(const Mm2ItemRecord *item, char *out, size_t out_size);
int mm2_item_class_can_use(const Mm2ItemRecord *item, uint8_t class_id);
const Mm2ItemRecord *mm2_items_lookup(const Mm2ItemsFile *items, uint8_t item_id);

#ifdef __cplusplus
}
#endif

#endif
