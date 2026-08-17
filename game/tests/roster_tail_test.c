/* roster.dat global-tail accessor checks (save_game_state @ 0x823C layout).
 * Party words at tail +0x028/+0x038 and the 24-byte event bank at +0x7CE
 * (hireling A..X availability) must round-trip byte-exact through the decoded
 * roster records (slots 48..63). */
#include <stdio.h>
#include <string.h>

#include "mm2_roster_codec.h"

static int fails = 0;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        printf("FAIL: %s\n", msg);
        fails++;
    }
}

int main(void)
{
    static uint8_t raw[MM2_ROSTER_FILE_SIZE];
    static uint8_t raw2[MM2_ROSTER_FILE_SIZE];
    Mm2RosterFile roster;
    int i;

    /* Seed the on-disk image with a recognizable tail pattern. */
    memset(raw, 0, sizeof(raw));
    for (i = 0; i < MM2_ROSTER_TAIL_SIZE; i++) {
        raw[MM2_ROSTER_CHAR_SECTION_SIZE + i] = (uint8_t)(i * 7 + 3);
    }
    expect(mm2_roster_decode(raw, sizeof(raw), &roster) == MM2_ROSTER_OK, "decode");

    /* u8 reads must see the raw stream bytes through the decoded records. */
    for (i = 0; i < MM2_ROSTER_TAIL_SIZE; i++) {
        if (mm2_roster_tail_u8(&roster, i) != (uint8_t)(i * 7 + 3)) {
            break;
        }
    }
    expect(i == MM2_ROSTER_TAIL_SIZE, "tail u8 read matches raw stream");

    /* Party words (LE) + hireling unlock byte. */
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_PARTY_ROSTER_IDX, 0x0017);
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_PARTY_ROSTER_IDX + 2, 0xFFFF);
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_PARTY_SIZE, 2);
    mm2_roster_tail_set_u8(&roster, MM2_ROSTER_TAIL_EVENT_BANK + 3, 1); /* hireling D */

    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_PARTY_ROSTER_IDX) == 0x0017,
           "party idx0 readback");
    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_PARTY_ROSTER_IDX + 2) == 0xFFFF,
           "party idx1 readback");
    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_PARTY_SIZE) == 2, "party size readback");
    expect(mm2_roster_tail_u8(&roster, MM2_ROSTER_TAIL_EVENT_BANK + 3) == 1, "bank[D] readback");

    /* Encode: writes land at the documented file offsets, little-endian. */
    expect(mm2_roster_encode(&roster, raw2, sizeof(raw2)) == MM2_ROSTER_OK, "encode");
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_PARTY_ROSTER_IDX] == 0x17 &&
               raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_PARTY_ROSTER_IDX + 1] == 0x00,
           "party idx0 LE on disk");
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_PARTY_SIZE] == 0x02 &&
               raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_PARTY_SIZE + 1] == 0x00,
           "party size LE on disk");
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_EVENT_BANK + 3] == 0x01,
           "bank byte on disk");

    /* Calendar words (LE) at the start of the 0x823C stream. */
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_DAY + 9 * 2, 12);
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_YEAR + 9 * 2, 900);
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_ERA, 9);
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_SUBDAY, 0x55);
    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_DAY + 9 * 2) == 12, "day[9] readback");
    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_YEAR + 9 * 2) == 900, "year[9] readback");
    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_ERA) == 9, "era readback");
    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_SUBDAY) == 0x55, "subday readback");
    expect(mm2_roster_encode(&roster, raw2, sizeof(raw2)) == MM2_ROSTER_OK, "encode calendar");
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_DAY + 18] == 12 &&
               raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_DAY + 19] == 0,
           "day[9] LE on disk");
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_YEAR + 18] == 0x84 &&
               raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_YEAR + 19] == 0x03,
           "year[9] LE on disk");
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_ERA] == 9 &&
               raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_ERA + 1] == 0,
           "era LE on disk");

    /* Automap vis words (LE) sit in the $780-byte hole after battles-lost. */
    expect(MM2_ROSTER_TAIL_AUTOMAP == 0x044, "automap tail offset");
    expect(MM2_ROSTER_TAIL_AUTOMAP + MM2_ROSTER_TAIL_AUTOMAP_SIZE == MM2_ROSTER_TAIL_GATE_BANK,
           "automap abuts gate bank");
    mm2_roster_tail_set_u16(&roster, MM2_ROSTER_TAIL_AUTOMAP + (3 * 16 + 5) * 2, 0x8001);
    expect(mm2_roster_tail_u16(&roster, MM2_ROSTER_TAIL_AUTOMAP + (3 * 16 + 5) * 2) == 0x8001,
           "automap row readback");
    expect(mm2_roster_encode(&roster, raw2, sizeof(raw2)) == MM2_ROSTER_OK, "encode automap");
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_AUTOMAP + (3 * 16 + 5) * 2] == 0x01 &&
               raw2[MM2_ROSTER_CHAR_SECTION_SIZE + MM2_ROSTER_TAIL_AUTOMAP + (3 * 16 + 5) * 2 + 1] ==
                   0x80,
           "automap row LE on disk");

    /* Untouched tail bytes round-trip unchanged. */
    expect(raw2[MM2_ROSTER_CHAR_SECTION_SIZE + 0x100] == (uint8_t)(0x100 * 7 + 3),
           "untouched tail byte preserved");

    /* Out-of-range access is inert. */
    mm2_roster_tail_set_u8(&roster, -1, 0xAA);
    mm2_roster_tail_set_u8(&roster, MM2_ROSTER_TAIL_SIZE, 0xAA);
    expect(mm2_roster_tail_u8(&roster, -1) == 0 &&
               mm2_roster_tail_u8(&roster, MM2_ROSTER_TAIL_SIZE) == 0,
           "out-of-range access inert");

    if (fails) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("roster_tail_test OK\n");
    return 0;
}
