#ifndef MM2_MONSTERS_CODEC_H
#define MM2_MONSTERS_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ASM-confirmed facts:
 * - monsters.dat is loaded into A4-$77C2 (startup loader around 0x26658).
 * - accessor routine at 0x99C8 copies index * 0x1A bytes.
 * - file size in this project inventory is 6656 bytes.
 *
 * Therefore: 256 records * 26 bytes (0x1A).
 */
enum {
    MM2_MONSTER_RECORD_SIZE = 0x1A,
    MM2_MONSTER_RECORD_COUNT = 256,
    MM2_MONSTER_FILE_SIZE = MM2_MONSTER_RECORD_SIZE * MM2_MONSTER_RECORD_COUNT,
    MM2_MONSTER_NAME_SIZE = 14
};

/* Absolute byte offsets inside a 26-byte record (confirmed by the unpacker at
 * asm 0x4C8E). The first MM2_MONSTER_NAME_SIZE bytes are the name. */
enum {
    MM2_MON_OFF_HP       = 0x0E,
    MM2_MON_OFF_XP       = 0x0F,
    MM2_MON_OFF_TREASURE = 0x10,
    MM2_MON_OFF_PABIL    = 0x11, /* group attack */
    MM2_MON_OFF_SABIL    = 0x12, /* single-target attack */
    MM2_MON_OFF_OABIL    = 0x13, /* other behaviour */
    MM2_MON_OFF_SPEED    = 0x14,
    MM2_MON_OFF_PICTURE  = 0x15,
    MM2_MON_OFF_AC       = 0x16,
    MM2_MON_OFF_DAMAGE   = 0x17,
    MM2_MON_OFF_SPEED2   = 0x18,
    MM2_MON_OFF_MRES     = 0x19
};

/* Single-target status-effect names (Sabil low 5 bits), index 0..31. From the
 * victim-message table in the binary (data hunk pointers, asm ~0xFA1A). */
#define MM2_SINGLE_EFFECT_COUNT 32
extern const char *const mm2_single_effect_names[MM2_SINGLE_EFFECT_COUNT];

/* Group/party attack verbs (Pabil low 5 bits), index 0..31. From the verb
 * table the combat dispatcher prints at asm 0x10002 (data hunk pointers,
 * strings at 0xFB98+). Index 29 = "frenzies" (e.g. the Cuisinart). */
#define MM2_PARTY_VERB_COUNT 32
extern const char *const mm2_party_verb_names[MM2_PARTY_VERB_COUNT];

const char *mm2_single_effect_name(uint8_t index);
const char *mm2_party_verb_name(uint8_t index);

/* Decoded combat abilities for one monster (mirrors the asm 0x4C8E unpacker). */
typedef struct Mm2MonsterAbilities {
    uint8_t party_verb;     /* Pabil & 0x1F  -> mm2_party_verb_names      */
    uint8_t party_chance;   /* (Pabil >> 5) & 7 (use-frequency tier)      */
    uint8_t single_effect;  /* Sabil & 0x1F  -> mm2_single_effect_names   */
    uint8_t single_misc;    /* (Sabil >> 5) & 1 (misc flag)               */
    uint8_t archer;         /* Sabil bit 6                                */
    uint8_t undead;         /* Sabil bit 7                                */
    uint8_t add_friends_base; /* Oabil & 0x0F (reinforcement count - 1)   */
    uint8_t add_friends_x10;  /* Oabil bit 4 (multiply the count by 10)   */
    uint8_t flee_tier;        /* (Oabil >> 5) & 3 (flee/run chance)       */
    uint8_t multiplies;       /* Oabil bit 7 (breeds/duplicates)          */
} Mm2MonsterAbilities;

/* Effective "adds friends" reinforcement count = (base+1) * (x10 ? 10 : 1). */
#define MM2_MON_ADD_FRIENDS_COUNT(a) \
    (((a)->add_friends_base + 1) * ((a)->add_friends_x10 ? 10 : 1))

typedef struct Mm2MonsterRecord {
    /*
     * Name appears first in the record and is commonly treated as a
     * fixed-width text field in legacy tools.
     */
    char name[MM2_MONSTER_NAME_SIZE];

    /*
     * Remaining 12 bytes are stat/behavior fields. Keep raw until each byte
     * is proven by call-site traces.
     */
    uint8_t fields[MM2_MONSTER_RECORD_SIZE - MM2_MONSTER_NAME_SIZE];
} Mm2MonsterRecord;

typedef struct Mm2MonstersFile {
    Mm2MonsterRecord records[MM2_MONSTER_RECORD_COUNT];
} Mm2MonstersFile;

typedef enum Mm2MonstersError {
    MM2_MONSTERS_OK = 0,
    MM2_MONSTERS_ERR_IO = 1,
    MM2_MONSTERS_ERR_BAD_ARGS = 2,
    MM2_MONSTERS_ERR_BAD_SIZE = 3
} Mm2MonstersError;

Mm2MonstersError mm2_monsters_decode(const uint8_t *bytes, size_t bytes_size, Mm2MonstersFile *out);
Mm2MonstersError mm2_monsters_encode(const Mm2MonstersFile *monsters, uint8_t *out_bytes, size_t out_size);
Mm2MonstersError mm2_monsters_load_file(const char *path, Mm2MonstersFile *out);
Mm2MonstersError mm2_monsters_save_file(const char *path, const Mm2MonstersFile *monsters);

int mm2_monster_slot_is_empty(const Mm2MonsterRecord *record);
void mm2_monster_name_to_cstr(const Mm2MonsterRecord *record, char *out, size_t out_size);
void mm2_monster_set_name(Mm2MonsterRecord *record, const char *name);

/* Decode/encode the Pabil/Sabil/Oabil ability bytes. Encoding preserves the
 * bits the unpacker ignores (e.g. add_friends x10 flag), so a decode->encode
 * round-trip is byte-stable. */
void mm2_monster_decode_abilities(const Mm2MonsterRecord *record, Mm2MonsterAbilities *out);
void mm2_monster_encode_abilities(Mm2MonsterRecord *record, const Mm2MonsterAbilities *in);

#ifdef __cplusplus
}
#endif

#endif
