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
    MM2_ROSTER_SPELL_BYTES = 12,
    /* 48 playable slots; same split on Amiga and PC (MM2.EXE @0x2F72). */
    MM2_ROSTER_CHAR_SECTION_SIZE = 0x1860,
    /* Global stream from save_game_state @0x823C; last byte @0x803. */
    MM2_ROSTER_GLOBAL_USED_SIZE = 0x804,
    /* GOG ROSTER.DAT: char section + global stream, no slot-63 tail padding. */
    MM2_ROSTER_PC_FILE_SIZE =
        MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_GLOBAL_USED_SIZE,
    MM2_ROSTER_FILE_SIZE = MM2_ROSTER_RECORD_SIZE * MM2_ROSTER_RECORD_COUNT
};

/* A single equipped/backpack item, as a convenience aggregate. The ON-DISK and
 * in-memory storage is Structure-of-Arrays (see Mm2RosterRecord), NOT an array
 * of these triplets — use the slot accessors below to read/write a logical slot.
 *
 * ASM-confirmed field roles (68k disassembly, see docs/06-roster-format.md):
 *   item_id  +$28/+$3A run : item table id (0 = empty slot).
 *   charges  +$2E/+$40 run : usable charges; decremented on use at $137F0 (pack)
 *                            and $138A6 (equipped); Blitz3D editor calls it "plus".
 *   flags    +$34/+$46 run : per-instance flags; cleared on depletion ($138EE);
 *                            Blitz3D editor calls it "efft" (effect). */
typedef struct Mm2RosterItemSlot {
    uint8_t item_id;
    uint8_t charges;
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

    /* Equipped + backpack items, 0x28..0x4B (36 bytes). The Amiga stores these as
     * Structure-of-Arrays: six contiguous ids, then six charges, then six flags
     * (NOT interleaved id/charges/flags triplets). Confirmed in the 68k ASM:
     *   - OP_16 item scan @ 0x16520 reads ids at +$28+i / +$3A+i with stride 1.
     *   - OP_19 add-item @ 0x165D8 writes id/charges/flags at +$3A+i, +$40+i, +$46+i.
     *   - equip @ 0x0EB9A copies +$28->+$3A, +$2E->+$40, +$34->+$46 (id/charges/flags).
     *   - item-use @ 0x137F0/0x138A6 decrements +$40/+$2E (charges).
     * Cross-checked with the Blitz3D editor (Character.bb): six equiped, six
     * equiplus, six equiefft, then six backpack/backplus/backefft, read in order.
     * Use the slot accessors below for logical per-slot access. */
    uint8_t equipped_id[MM2_ROSTER_ITEM_SLOTS];      /* 0x28..0x2D */
    uint8_t equipped_charges[MM2_ROSTER_ITEM_SLOTS]; /* 0x2E..0x33 */
    uint8_t equipped_flags[MM2_ROSTER_ITEM_SLOTS];   /* 0x34..0x39 */
    uint8_t backpack_id[MM2_ROSTER_ITEM_SLOTS];      /* 0x3A..0x3F */
    uint8_t backpack_charges[MM2_ROSTER_ITEM_SLOTS]; /* 0x40..0x45 */
    uint8_t backpack_flags[MM2_ROSTER_ITEM_SLOTS];   /* 0x46..0x4B */

    /* 0x4C..0x57: tentatively "spells" (UNVERIFIED — MM2 gates spells by spell
     * level, not per-spell flags, so this label is suspect). Byte +0x50 is NOT a
     * spell field: the event VM (OP_32 @ 0x17190 -> 0x04614/0x45C4) reads +0x50
     * as a packed pair of alignment/profession-title nibbles and counts party
     * members matching a title id (0x04 Crusader, 0x08 Heroic, 0x09 druid/pagan).
     * ASM annotates +0x50 "class nibble". */
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

/* Logical per-slot access over the Structure-of-Arrays storage. idx in 0..5. */
Mm2RosterItemSlot mm2_roster_equipped(const Mm2RosterRecord *record, int idx);
Mm2RosterItemSlot mm2_roster_backpack(const Mm2RosterRecord *record, int idx);
void mm2_roster_set_equipped(Mm2RosterRecord *record, int idx, Mm2RosterItemSlot slot);
void mm2_roster_set_backpack(Mm2RosterRecord *record, int idx, Mm2RosterItemSlot slot);

int mm2_roster_slot_is_empty(const Mm2RosterRecord *record);
void mm2_roster_name_to_cstr(const Mm2RosterRecord *record, char *out, size_t out_size);
void mm2_roster_set_name(Mm2RosterRecord *record, const char *name);
void mm2_roster_clear_record(Mm2RosterRecord *record);

#ifdef __cplusplus
}
#endif

#endif
