#pragma once

/**
 * Amiga Paula generative audio — ASM leafs 0x7070 / 0x766E / 0x6FB8 / 0x77AA / 0x7562.
 * Under ACE (OS taken over) we program custom.aud[] directly; tables/timing match retail.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Optional abort poll (non-zero aborts current title). May be NULL. */
typedef int (*Mm2AmigaAudioAbortFn)(void);

int mm2_amiga_audio_init(void);
void mm2_amiga_audio_shutdown(void);

void mm2_amiga_audio_set_abort(Mm2AmigaAudioAbortFn fn);

/**
 * play_sound_seq @ 0x6FB8 — id 0..9 (blocking).
 * Gates: id0 = walk_beep_enabled; id1..9 = sounds_enabled (caller applies flags).
 */
void mm2_amiga_audio_play_sound_seq(uint8_t id);

/** play_tone_env @ 0x77AA (blocking). */
void mm2_amiga_audio_play_tone_env(int16_t note, int16_t duration_ticks);

/** stop_channels @ 0x7562 */
void mm2_amiga_audio_stop_channels(void);

/**
 * Title theme (overlay 0x283FC stream).
 * Cooperative: call title_pump once per attract frame — Paula keeps playing
 * between frames; notes chain with no silence gap.
 */
void mm2_amiga_audio_title_start(int loop);
/** Returns 1 while theme still active, 0 if finished/aborted. */
int mm2_amiga_audio_title_pump(void);
void mm2_amiga_audio_title_stop(void);
int mm2_amiga_audio_title_active(void);

#ifdef __cplusplus
}
#endif
