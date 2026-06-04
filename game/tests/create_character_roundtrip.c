#include "mm2_create_character.h"
#include "mm2_roster_codec.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "roster.dat";
    Mm2RosterFile roster;
    Mm2PendingCharacter pending;
    uint32_t rng = 42;
    int slot = -1;
    int i;
    char name[16];

    if (mm2_roster_load_file(path, &roster) != MM2_ROSTER_OK) {
        fprintf(stderr, "load failed: %s\n", path);
        return 1;
    }

    for (i = 0; i < 48; ++i) {
        if (mm2_roster_slot_is_empty(&roster.records[i])) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fprintf(stderr, "no empty slot\n");
        return 1;
    }

    mm2_create_pending_init(&pending);
    mm2_create_roll_stats(&pending.rolled, &rng);
    pending.class_id = 3;
    pending.race = 0;
    pending.alignment = 1;
    pending.sex = 0;
    strncpy(pending.name, "TestHero", sizeof(pending.name) - 1);
    mm2_create_apply_race(pending.race, &pending.rolled, &pending.modified);
    mm2_create_build_record(&pending, &roster.records[slot]);

    if (mm2_roster_save_file(path, &roster) != MM2_ROSTER_OK) {
        fprintf(stderr, "save failed\n");
        return 1;
    }

    memset(&roster, 0, sizeof(roster));
    if (mm2_roster_load_file(path, &roster) != MM2_ROSTER_OK) {
        fprintf(stderr, "reload failed\n");
        return 1;
    }

    mm2_roster_name_to_cstr(&roster.records[slot], name, sizeof(name));
    if (strcmp(name, "TestHero") != 0) {
        fprintf(stderr, "name mismatch: '%s'\n", name);
        return 1;
    }
    if (roster.records[slot].class_id != 3 || roster.records[slot].level != 1) {
        fprintf(stderr, "record fields mismatch\n");
        return 1;
    }

    printf("ok slot=%d name=%s class=%u hp=%u/%u\n", slot, name, roster.records[slot].class_id,
           roster.records[slot].hp_current, roster.records[slot].hp_max);
    return 0;
}
