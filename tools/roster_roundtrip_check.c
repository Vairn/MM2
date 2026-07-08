/* Temporary round-trip / layout validation for the SoA roster item layout.
 * Compile: gcc -I EXTRACTED/decomp tools/roster_roundtrip_check.c \
 *              EXTRACTED/decomp/mm2_roster_codec.c -o build_rt.exe
 */
#include <stdio.h>
#include <string.h>

#include "mm2_roster_codec.h"

static int fails = 0;
static void check(int cond, const char *msg)
{
    if (!cond) { printf("FAIL: %s\n", msg); ++fails; }
}

int main(int argc, char **argv)
{
    /* 1. Discriminating layout test: distinct id/charges/flags per slot. */
    Mm2RosterFile rf;
    memset(&rf, 0, sizeof(rf));
    Mm2RosterRecord *r = &rf.records[0];
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; i++) {
        r->equipped_id[i]      = (uint8_t)(10 + i);
        r->equipped_charges[i] = (uint8_t)(1 + i);
        r->equipped_flags[i]   = (uint8_t)(20 + i);
        r->backpack_id[i]      = (uint8_t)(30 + i);
        r->backpack_charges[i] = (uint8_t)(40 + i);
        r->backpack_flags[i]   = (uint8_t)(50 + i);
    }

    uint8_t buf[MM2_ROSTER_RECORD_SIZE * MM2_ROSTER_RECORD_COUNT];
    check(mm2_roster_encode(&rf, buf, sizeof(buf)) == MM2_ROSTER_OK, "encode ok");

    /* Bytes must be Structure-of-Arrays at the documented offsets. */
    for (int i = 0; i < 6; i++) {
        check(buf[0x28 + i] == 10 + i, "equipped id run @0x28");
        check(buf[0x2E + i] == 1 + i,  "equipped charges run @0x2E");
        check(buf[0x34 + i] == 20 + i, "equipped flags run @0x34");
        check(buf[0x3A + i] == 30 + i, "backpack id run @0x3A");
        check(buf[0x40 + i] == 40 + i, "backpack charges run @0x40");
        check(buf[0x46 + i] == 50 + i, "backpack flags run @0x46");
    }

    /* Decode back and verify equality (struct round-trip). */
    Mm2RosterFile rf2;
    memset(&rf2, 0, sizeof(rf2));
    check(mm2_roster_decode(buf, sizeof(buf), &rf2) == MM2_ROSTER_OK, "decode ok");
    Mm2RosterRecord *r2 = &rf2.records[0];
    int eq = 1;
    for (int i = 0; i < 6; i++) {
        if (r2->equipped_id[i] != r->equipped_id[i]) eq = 0;
        if (r2->equipped_charges[i] != r->equipped_charges[i]) eq = 0;
        if (r2->equipped_flags[i] != r->equipped_flags[i]) eq = 0;
        if (r2->backpack_id[i] != r->backpack_id[i]) eq = 0;
        if (r2->backpack_charges[i] != r->backpack_charges[i]) eq = 0;
        if (r2->backpack_flags[i] != r->backpack_flags[i]) eq = 0;
    }
    check(eq, "struct decode matches encode");

    /* Slot accessors map to the right runs. */
    Mm2RosterItemSlot s = mm2_roster_equipped(r, 3);
    check(s.item_id == 13 && s.charges == 4 && s.flags == 23, "equipped slot accessor");
    s = mm2_roster_backpack(r, 5);
    check(s.item_id == 35 && s.charges == 45 && s.flags == 55, "backpack slot accessor");

    /* 2. Load real roster.dat / ROSTER.DAT (Amiga 8320 or PC 8292), if provided. */
    if (argc > 1) {
        Mm2RosterFile rr, rr2;
        check(mm2_roster_load_file(argv[1], &rr) == MM2_ROSTER_OK, "load_file ok");
        static uint8_t rt[MM2_ROSTER_FILE_SIZE];
        check(mm2_roster_encode(&rr, rt, sizeof(rt)) == MM2_ROSTER_OK, "real encode ok");
        check(mm2_roster_decode(rt, sizeof(rt), &rr2) == MM2_ROSTER_OK, "real re-decode ok");
        static uint8_t rt2[MM2_ROSTER_FILE_SIZE];
        check(mm2_roster_encode(&rr2, rt2, sizeof(rt2)) == MM2_ROSTER_OK, "real re-encode ok");
        check(memcmp(rt, rt2, sizeof(rt)) == 0, "real roster encode/decode stable");
    }

    if (fails == 0) { printf("OK: roster_roundtrip_check (all checks passed)\n"); return 0; }
    return 1;
}
