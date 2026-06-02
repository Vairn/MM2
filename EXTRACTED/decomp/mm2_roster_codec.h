#ifndef MM2_ROSTER_CODEC_H
#define MM2_ROSTER_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MM2_ROSTER_RECORD_SIZE = 0x82,
    MM2_ROSTER_RECORD_COUNT = 64,
    MM2_ROSTER_NAME_SIZE = 11,
    MM2_ROSTER_ITEM_SLOTS = 6,
    MM2_ROSTER_ITEM_SLOT_SIZE = 3,
    MM2_ROSTER_SPELL_BYTES = 12
};

typedef struct Mm2RosterItemSlot {
    uint8_t item_id;
    uint8_t bonus;
    uint8_t flags;
} Mm2RosterItemSlot;

typedef struct Mm2RosterRecord {
    char name[MM2_ROSTER_NAME_SIZE]; /* Not guaranteed NUL-terminated. */
    uint8_t town_flags;              /* low 7 bits town, bit7 likely in-party. */
    uint8_t sex;
    uint8_t alignment_current;
    uint8_t race;
    uint8_t class_id;

    uint8_t might_current;
    uint8_t intelligence_current;
    uint8_t personality_current;
    uint8_t speed_current;
    uint8_t accuracy_current;
    uint8_t luck_current;
    uint8_t thievery_percent;

    uint8_t secondary_skills[3]; /* offsets 0x17..0x19 (not fully resolved). */
    uint8_t unknown_1a_20[7];

    uint8_t age;
    uint16_t unknown_22;
    uint8_t armor_class;
    uint8_t food;
    uint8_t condition;
    uint8_t endurance_current;

    Mm2RosterItemSlot equipped[MM2_ROSTER_ITEM_SLOTS];
    Mm2RosterItemSlot backpack[MM2_ROSTER_ITEM_SLOTS];

    uint8_t spells[MM2_ROSTER_SPELL_BYTES];

    uint16_t sp_max;
    uint16_t sp_current;
    uint16_t gems;
    uint16_t hp_max;
    uint16_t hp_aux;
    uint32_t experience;
    uint32_t gold;

    uint8_t alignment_base;
    uint8_t might_base;
    uint8_t intelligence_base;
    uint8_t personality_base;
    uint8_t speed_base;
    uint8_t accuracy_base;
    uint8_t luck_base;
    uint8_t level;
    uint8_t spell_level;
    uint8_t endurance_base;
    uint16_t hp_current;

    uint16_t temp_score_word;       /* +0x76: u16 LE; purpose unknown */
    uint8_t script_work_flag;       /* +0x78: transient script/work flag */
    uint8_t class_quest_guild_mask; /* +0x79: class-quest/guild bitmask; bit7 = in-game class '+' */
    uint8_t tail_padding_7a_81[10]; /* +0x7A..+0x81: mostly unknown/padding */
} Mm2RosterRecord;

typedef struct Mm2RosterFile {
    Mm2RosterRecord records[MM2_ROSTER_RECORD_COUNT];
} Mm2RosterFile;

typedef enum Mm2RosterError {
    MM2_ROSTER_OK = 0,
    MM2_ROSTER_ERR_IO = 1,
    MM2_ROSTER_ERR_BAD_ARGS = 2,
    MM2_ROSTER_ERR_BAD_SIZE = 3
} Mm2RosterError;

Mm2RosterError mm2_roster_decode(const uint8_t *bytes, size_t bytes_size, Mm2RosterFile *out);
Mm2RosterError mm2_roster_encode(const Mm2RosterFile *roster, uint8_t *out_bytes, size_t out_size);
Mm2RosterError mm2_roster_load_file(const char *path, Mm2RosterFile *out);
Mm2RosterError mm2_roster_save_file(const char *path, const Mm2RosterFile *roster);

int mm2_roster_slot_is_empty(const Mm2RosterRecord *record);
void mm2_roster_name_to_cstr(const Mm2RosterRecord *record, char *out, size_t out_size);
void mm2_roster_set_name(Mm2RosterRecord *record, const char *name);

#ifdef __cplusplus
}
#endif

#endif
