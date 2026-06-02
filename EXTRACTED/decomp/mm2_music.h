#ifndef MM2_MUSIC_H
#define MM2_MUSIC_H

#include <stdint.h>

#define MM2_MASTER_BLOB_SIZE   0x1860u
#define MM2_MUSIC_SONG_COUNT   60u
#define MM2_MUSIC_STEPS_PER_SONG 16u
#define MM2_MUSIC_PITCH_A_COUNT 10u
#define MM2_MUSIC_PITCH_B_COUNT 10u
#define MM2_MUSIC_NOTE_MAP_COUNT 8u
#define MM2_MUSIC_SONG_BANK_OFF 0x780u
#define MM2_MUSIC_SONG_STRIDE   32u   /* 16 steps * 2 bytes */

typedef struct Mm2MasterMusic {
    uint16_t pitch_a[MM2_MUSIC_PITCH_A_COUNT];   /* blob: first 10 BE u16 */
    uint16_t pitch_b[MM2_MUSIC_PITCH_B_COUNT];
    uint16_t note_map[MM2_MUSIC_NOTE_MAP_COUNT];
    uint16_t voice_cap;
    uint16_t field_79b6;
    uint16_t field_79b4;
    uint16_t transpose[3];   /* 7972, 7970, 796e */
    uint16_t songs[MM2_MUSIC_SONG_COUNT][MM2_MUSIC_STEPS_PER_SONG];
    uint8_t  table_7995[10];
    uint8_t  table_798b[24];
    uint8_t  table_79a4[4];
    uint8_t  sounds_flag;    /* 79b2: 1 = mute title music */
    uint8_t  walk_flags[14]; /* 79b0..7996 single bytes from init */
} Mm2MasterMusic;

/* Decode master blob (big-endian u16 fields) into Mm2MasterMusic. */
int mm2_master_decode(const uint8_t *blob, unsigned len, Mm2MasterMusic *out);

#endif /* MM2_MUSIC_H */
