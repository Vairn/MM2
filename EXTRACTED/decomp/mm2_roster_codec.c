#include "mm2_roster_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MM2_ROSTER_FILE_SIZE = MM2_ROSTER_RECORD_SIZE * MM2_ROSTER_RECORD_COUNT
};

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static void write_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void write_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void decode_item_slots(const uint8_t *src, Mm2RosterItemSlot *dst)
{
    int i;
    for (i = 0; i < MM2_ROSTER_ITEM_SLOTS; i++) {
        dst[i].item_id = src[0];
        dst[i].bonus = src[1];
        dst[i].flags = src[2];
        src += MM2_ROSTER_ITEM_SLOT_SIZE;
    }
}

static void encode_item_slots(const Mm2RosterItemSlot *src, uint8_t *dst)
{
    int i;
    for (i = 0; i < MM2_ROSTER_ITEM_SLOTS; i++) {
        dst[0] = src[i].item_id;
        dst[1] = src[i].bonus;
        dst[2] = src[i].flags;
        dst += MM2_ROSTER_ITEM_SLOT_SIZE;
    }
}

static void decode_record(const uint8_t *src, Mm2RosterRecord *dst)
{
    memcpy(dst->name, src + 0x00, MM2_ROSTER_NAME_SIZE);
    dst->town_flags = src[0x0B];
    dst->sex = src[0x0C];
    dst->alignment_current = src[0x0D];
    dst->race = src[0x0E];
    dst->class_id = src[0x0F];

    dst->might_current = src[0x10];
    dst->intelligence_current = src[0x11];
    dst->personality_current = src[0x12];
    dst->speed_current = src[0x13];
    dst->accuracy_current = src[0x14];
    dst->luck_current = src[0x15];
    dst->thievery_percent = src[0x16];

    memcpy(dst->secondary_skills, src + 0x17, sizeof(dst->secondary_skills));
    memcpy(dst->unknown_1a_20, src + 0x1A, sizeof(dst->unknown_1a_20));

    dst->age = src[0x21];
    dst->unknown_22 = read_le16(src + 0x22);
    dst->armor_class = src[0x24];
    dst->food = src[0x25];
    dst->condition = src[0x26];
    dst->endurance_current = src[0x27];

    decode_item_slots(src + 0x28, dst->equipped);
    decode_item_slots(src + 0x3A, dst->backpack);
    memcpy(dst->spells, src + 0x4C, MM2_ROSTER_SPELL_BYTES);

    dst->sp_max = read_le16(src + 0x58);
    dst->sp_current = read_le16(src + 0x5A);
    dst->gems = read_le16(src + 0x5C);
    dst->hp_max = read_le16(src + 0x5E);
    dst->hp_aux = read_le16(src + 0x60);
    dst->experience = read_le32(src + 0x62);
    dst->gold = read_le32(src + 0x66);

    dst->alignment_base = src[0x6A];
    dst->might_base = src[0x6B];
    dst->intelligence_base = src[0x6C];
    dst->personality_base = src[0x6D];
    dst->speed_base = src[0x6E];
    dst->accuracy_base = src[0x6F];
    dst->luck_base = src[0x70];
    dst->level = src[0x71];
    dst->spell_level = src[0x72];
    dst->endurance_base = src[0x73];
    dst->hp_current = read_le16(src + 0x74);
    dst->temp_score_word = read_le16(src + 0x76);
    dst->script_work_flag = src[0x78];
    dst->class_quest_guild_mask = src[0x79];
    memcpy(dst->tail_padding_7a_81, src + 0x7A, sizeof(dst->tail_padding_7a_81));
}

static void encode_record(const Mm2RosterRecord *src, uint8_t *dst)
{
    memset(dst, 0, MM2_ROSTER_RECORD_SIZE);

    memcpy(dst + 0x00, src->name, MM2_ROSTER_NAME_SIZE);
    dst[0x0B] = src->town_flags;
    dst[0x0C] = src->sex;
    dst[0x0D] = src->alignment_current;
    dst[0x0E] = src->race;
    dst[0x0F] = src->class_id;

    dst[0x10] = src->might_current;
    dst[0x11] = src->intelligence_current;
    dst[0x12] = src->personality_current;
    dst[0x13] = src->speed_current;
    dst[0x14] = src->accuracy_current;
    dst[0x15] = src->luck_current;
    dst[0x16] = src->thievery_percent;

    memcpy(dst + 0x17, src->secondary_skills, sizeof(src->secondary_skills));
    memcpy(dst + 0x1A, src->unknown_1a_20, sizeof(src->unknown_1a_20));

    dst[0x21] = src->age;
    write_le16(dst + 0x22, src->unknown_22);
    dst[0x24] = src->armor_class;
    dst[0x25] = src->food;
    dst[0x26] = src->condition;
    dst[0x27] = src->endurance_current;

    encode_item_slots(src->equipped, dst + 0x28);
    encode_item_slots(src->backpack, dst + 0x3A);
    memcpy(dst + 0x4C, src->spells, MM2_ROSTER_SPELL_BYTES);

    write_le16(dst + 0x58, src->sp_max);
    write_le16(dst + 0x5A, src->sp_current);
    write_le16(dst + 0x5C, src->gems);
    write_le16(dst + 0x5E, src->hp_max);
    write_le16(dst + 0x60, src->hp_aux);
    write_le32(dst + 0x62, src->experience);
    write_le32(dst + 0x66, src->gold);

    dst[0x6A] = src->alignment_base;
    dst[0x6B] = src->might_base;
    dst[0x6C] = src->intelligence_base;
    dst[0x6D] = src->personality_base;
    dst[0x6E] = src->speed_base;
    dst[0x6F] = src->accuracy_base;
    dst[0x70] = src->luck_base;
    dst[0x71] = src->level;
    dst[0x72] = src->spell_level;
    dst[0x73] = src->endurance_base;
    write_le16(dst + 0x74, src->hp_current);
    write_le16(dst + 0x76, src->temp_score_word);
    dst[0x78] = src->script_work_flag;
    dst[0x79] = src->class_quest_guild_mask;
    memcpy(dst + 0x7A, src->tail_padding_7a_81, sizeof(src->tail_padding_7a_81));
}

Mm2RosterError mm2_roster_decode(const uint8_t *bytes, size_t bytes_size, Mm2RosterFile *out)
{
    int i;
    if (!bytes || !out) {
        return MM2_ROSTER_ERR_BAD_ARGS;
    }
    if (bytes_size != MM2_ROSTER_FILE_SIZE) {
        return MM2_ROSTER_ERR_BAD_SIZE;
    }

    for (i = 0; i < MM2_ROSTER_RECORD_COUNT; i++) {
        decode_record(bytes + (i * MM2_ROSTER_RECORD_SIZE), &out->records[i]);
    }
    return MM2_ROSTER_OK;
}

Mm2RosterError mm2_roster_encode(const Mm2RosterFile *roster, uint8_t *out_bytes, size_t out_size)
{
    int i;
    if (!roster || !out_bytes) {
        return MM2_ROSTER_ERR_BAD_ARGS;
    }
    if (out_size != MM2_ROSTER_FILE_SIZE) {
        return MM2_ROSTER_ERR_BAD_SIZE;
    }

    for (i = 0; i < MM2_ROSTER_RECORD_COUNT; i++) {
        encode_record(&roster->records[i], out_bytes + (i * MM2_ROSTER_RECORD_SIZE));
    }
    return MM2_ROSTER_OK;
}

Mm2RosterError mm2_roster_load_file(const char *path, Mm2RosterFile *out)
{
    FILE *fp;
    uint8_t *buf;
    size_t got;
    Mm2RosterError err;

    if (!path || !out) {
        return MM2_ROSTER_ERR_BAD_ARGS;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_ROSTER_ERR_IO;
    }

    buf = (uint8_t *)malloc(MM2_ROSTER_FILE_SIZE);
    if (!buf) {
        fclose(fp);
        return MM2_ROSTER_ERR_IO;
    }

    got = fread(buf, 1, MM2_ROSTER_FILE_SIZE, fp);
    if (got != MM2_ROSTER_FILE_SIZE) {
        free(buf);
        fclose(fp);
        return MM2_ROSTER_ERR_BAD_SIZE;
    }

    if (fgetc(fp) != EOF) {
        free(buf);
        fclose(fp);
        return MM2_ROSTER_ERR_BAD_SIZE;
    }

    fclose(fp);
    err = mm2_roster_decode(buf, MM2_ROSTER_FILE_SIZE, out);
    free(buf);
    return err;
}

Mm2RosterError mm2_roster_save_file(const char *path, const Mm2RosterFile *roster)
{
    FILE *fp;
    uint8_t *buf;
    size_t wrote;
    Mm2RosterError err;

    if (!path || !roster) {
        return MM2_ROSTER_ERR_BAD_ARGS;
    }

    buf = (uint8_t *)malloc(MM2_ROSTER_FILE_SIZE);
    if (!buf) {
        return MM2_ROSTER_ERR_IO;
    }

    err = mm2_roster_encode(roster, buf, MM2_ROSTER_FILE_SIZE);
    if (err != MM2_ROSTER_OK) {
        free(buf);
        return err;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        free(buf);
        return MM2_ROSTER_ERR_IO;
    }

    wrote = fwrite(buf, 1, MM2_ROSTER_FILE_SIZE, fp);
    free(buf);

    if (wrote != MM2_ROSTER_FILE_SIZE) {
        fclose(fp);
        return MM2_ROSTER_ERR_IO;
    }
    if (fclose(fp) != 0) {
        return MM2_ROSTER_ERR_IO;
    }
    return MM2_ROSTER_OK;
}

int mm2_roster_slot_is_empty(const Mm2RosterRecord *record)
{
    int i;
    if (!record) {
        return 1;
    }
    for (i = 0; i < MM2_ROSTER_NAME_SIZE; i++) {
        uint8_t c = (uint8_t)record->name[i];
        if (c != 0 && c != (uint8_t)' ') {
            return 0;
        }
    }
    return 1;
}

void mm2_roster_name_to_cstr(const Mm2RosterRecord *record, char *out, size_t out_size)
{
    size_t i;
    size_t end;
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (!record) {
        return;
    }

    end = MM2_ROSTER_NAME_SIZE;
    while (end > 0) {
        uint8_t c = (uint8_t)record->name[end - 1];
        if (c != 0 && c != (uint8_t)' ') {
            break;
        }
        end--;
    }

    if (end >= out_size) {
        end = out_size - 1;
    }
    for (i = 0; i < end; i++) {
        out[i] = record->name[i];
    }
    out[end] = '\0';
}

void mm2_roster_set_name(Mm2RosterRecord *record, const char *name)
{
    size_t i;
    if (!record) {
        return;
    }
    memset(record->name, 0, MM2_ROSTER_NAME_SIZE);
    if (!name) {
        return;
    }
    for (i = 0; i < MM2_ROSTER_NAME_SIZE; i++) {
        if (name[i] == '\0') {
            break;
        }
        record->name[i] = name[i];
    }
}
