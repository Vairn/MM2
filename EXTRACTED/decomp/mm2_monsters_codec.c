#include "mm2_monsters_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const mm2_single_effect_names[MM2_SINGLE_EFFECT_COUNT] = {
    "nothing", "loose gold", "loose gems", "Poison", "Disease", "Sleep",
    "Curse", "Silence", "Paralyse", "Collapse", "Kills", "Stones",
    "Eradicates", "Steals an Item", "Steals ALL Items", "Steals food",
    "Steals all Food", "loose all Gold", "Loose all Gems", "Loose all Valuables",
    "Ages (non perm)", "Ages (PERM)", "loose 1 level", "Halves all Stats",
    "gives you Personality", "loose experience levels", "halves experience level",
    "loose Experience", "Items Scrambled", "Spell points loose all",
    "Assassinates", "Random Effect"
};

const char *const mm2_party_verb_names[MM2_PARTY_VERB_COUNT] = {
    "sprays poison", "sprays acid", "casts a curse", "breathes fire",
    "breathes lightning", "breathes cold", "breathes energy", "breathes gas",
    "breathes acid", "explodes", "gazes", "drains magic", "drains spell level",
    "vaporizes valuables", "juggles party", "energy blast", "sleep",
    "lightning bolts", "fireballs", "fingers of death", "disintegrate",
    "super shock", "dancing sword", "incinerate", "invokes power", "implosion",
    "inferno", "pain", "silence", "frenzies", "paralyze", "swarms"
};

const char *mm2_single_effect_name(uint8_t index)
{
    return index < MM2_SINGLE_EFFECT_COUNT ? mm2_single_effect_names[index] : "?";
}

const char *mm2_party_verb_name(uint8_t index)
{
    return index < MM2_PARTY_VERB_COUNT ? mm2_party_verb_names[index] : "?";
}

/* fields[] starts at record offset MM2_MONSTER_NAME_SIZE (0x0E). */
#define MM2_FIELD(rec, off) ((rec)->fields[(off) - MM2_MONSTER_NAME_SIZE])

void mm2_monster_decode_abilities(const Mm2MonsterRecord *record, Mm2MonsterAbilities *out)
{
    uint8_t pabil, sabil, oabil;
    if (!record || !out) {
        return;
    }
    pabil = MM2_FIELD(record, MM2_MON_OFF_PABIL);
    sabil = MM2_FIELD(record, MM2_MON_OFF_SABIL);
    oabil = MM2_FIELD(record, MM2_MON_OFF_OABIL);

    out->party_verb = (uint8_t)(pabil & 0x1F);
    out->party_chance = (uint8_t)((pabil >> 5) & 0x07);

    out->single_effect = (uint8_t)(sabil & 0x1F);
    out->single_misc = (uint8_t)((sabil >> 5) & 0x01);
    out->archer = (uint8_t)((sabil >> 6) & 0x01);
    out->undead = (uint8_t)((sabil >> 7) & 0x01);

    out->add_friends_base = (uint8_t)(oabil & 0x0F);
    out->add_friends_x10 = (uint8_t)((oabil >> 4) & 0x01);
    out->flee_tier = (uint8_t)((oabil >> 5) & 0x03);
    out->multiplies = (uint8_t)((oabil >> 7) & 0x01);
}

void mm2_monster_encode_abilities(Mm2MonsterRecord *record, const Mm2MonsterAbilities *in)
{
    if (!record || !in) {
        return;
    }
    MM2_FIELD(record, MM2_MON_OFF_PABIL) =
        (uint8_t)((in->party_verb & 0x1F) | ((in->party_chance & 0x07) << 5));
    MM2_FIELD(record, MM2_MON_OFF_SABIL) =
        (uint8_t)((in->single_effect & 0x1F) | ((in->single_misc & 0x01) << 5) |
                  ((in->archer & 0x01) << 6) | ((in->undead & 0x01) << 7));
    MM2_FIELD(record, MM2_MON_OFF_OABIL) =
        (uint8_t)((in->add_friends_base & 0x0F) | ((in->add_friends_x10 & 0x01) << 4) |
                  ((in->flee_tier & 0x03) << 5) | ((in->multiplies & 0x01) << 7));
}

Mm2MonstersError mm2_monsters_decode(const uint8_t *bytes, size_t bytes_size, Mm2MonstersFile *out)
{
    size_t i;
    if (!bytes || !out) {
        return MM2_MONSTERS_ERR_BAD_ARGS;
    }
    if (bytes_size != MM2_MONSTER_FILE_SIZE) {
        return MM2_MONSTERS_ERR_BAD_SIZE;
    }

    for (i = 0; i < MM2_MONSTER_RECORD_COUNT; i++) {
        const uint8_t *rec = bytes + (i * MM2_MONSTER_RECORD_SIZE);
        memcpy(out->records[i].name, rec, MM2_MONSTER_NAME_SIZE);
        memcpy(out->records[i].fields, rec + MM2_MONSTER_NAME_SIZE, sizeof(out->records[i].fields));
    }
    return MM2_MONSTERS_OK;
}

Mm2MonstersError mm2_monsters_encode(const Mm2MonstersFile *monsters, uint8_t *out_bytes, size_t out_size)
{
    size_t i;
    if (!monsters || !out_bytes) {
        return MM2_MONSTERS_ERR_BAD_ARGS;
    }
    if (out_size != MM2_MONSTER_FILE_SIZE) {
        return MM2_MONSTERS_ERR_BAD_SIZE;
    }

    for (i = 0; i < MM2_MONSTER_RECORD_COUNT; i++) {
        uint8_t *rec = out_bytes + (i * MM2_MONSTER_RECORD_SIZE);
        memcpy(rec, monsters->records[i].name, MM2_MONSTER_NAME_SIZE);
        memcpy(rec + MM2_MONSTER_NAME_SIZE, monsters->records[i].fields, sizeof(monsters->records[i].fields));
    }
    return MM2_MONSTERS_OK;
}

Mm2MonstersError mm2_monsters_load_file(const char *path, Mm2MonstersFile *out)
{
    FILE *fp;
    uint8_t *buf;
    size_t got;
    Mm2MonstersError err;

    if (!path || !out) {
        return MM2_MONSTERS_ERR_BAD_ARGS;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_MONSTERS_ERR_IO;
    }

    buf = (uint8_t *)malloc(MM2_MONSTER_FILE_SIZE);
    if (!buf) {
        fclose(fp);
        return MM2_MONSTERS_ERR_IO;
    }

    got = fread(buf, 1, MM2_MONSTER_FILE_SIZE, fp);
    if (got != MM2_MONSTER_FILE_SIZE) {
        free(buf);
        fclose(fp);
        return MM2_MONSTERS_ERR_BAD_SIZE;
    }
    if (fgetc(fp) != EOF) {
        free(buf);
        fclose(fp);
        return MM2_MONSTERS_ERR_BAD_SIZE;
    }
    fclose(fp);

    err = mm2_monsters_decode(buf, MM2_MONSTER_FILE_SIZE, out);
    free(buf);
    return err;
}

Mm2MonstersError mm2_monsters_save_file(const char *path, const Mm2MonstersFile *monsters)
{
    FILE *fp;
    uint8_t *buf;
    size_t wrote;
    Mm2MonstersError err;

    if (!path || !monsters) {
        return MM2_MONSTERS_ERR_BAD_ARGS;
    }

    buf = (uint8_t *)malloc(MM2_MONSTER_FILE_SIZE);
    if (!buf) {
        return MM2_MONSTERS_ERR_IO;
    }

    err = mm2_monsters_encode(monsters, buf, MM2_MONSTER_FILE_SIZE);
    if (err != MM2_MONSTERS_OK) {
        free(buf);
        return err;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        free(buf);
        return MM2_MONSTERS_ERR_IO;
    }
    wrote = fwrite(buf, 1, MM2_MONSTER_FILE_SIZE, fp);
    free(buf);
    if (wrote != MM2_MONSTER_FILE_SIZE) {
        fclose(fp);
        return MM2_MONSTERS_ERR_IO;
    }
    if (fclose(fp) != 0) {
        return MM2_MONSTERS_ERR_IO;
    }
    return MM2_MONSTERS_OK;
}

int mm2_monster_slot_is_empty(const Mm2MonsterRecord *record)
{
    size_t i;
    if (!record) {
        return 1;
    }
    for (i = 0; i < MM2_MONSTER_NAME_SIZE; i++) {
        uint8_t c = (uint8_t)record->name[i];
        if (c != 0 && c != (uint8_t)' ') {
            return 0;
        }
    }
    return 1;
}

void mm2_monster_name_to_cstr(const Mm2MonsterRecord *record, char *out, size_t out_size)
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

    end = MM2_MONSTER_NAME_SIZE;
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

void mm2_monster_set_name(Mm2MonsterRecord *record, const char *name)
{
    size_t i;
    if (!record) {
        return;
    }

    memset(record->name, 0, MM2_MONSTER_NAME_SIZE);
    if (!name) {
        return;
    }

    for (i = 0; i < MM2_MONSTER_NAME_SIZE; i++) {
        if (name[i] == '\0') {
            break;
        }
        record->name[i] = name[i];
    }
}
