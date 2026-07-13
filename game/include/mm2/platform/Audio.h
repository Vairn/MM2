#pragma once

#include "mm2/Config.h"
#include "mm2/CppStdCompat.h"

namespace mm2::audio {

/** Load WAVs from data_dir/audio (or EXTRACTED/audio fallbacks). Soft-fails if missing. */
bool init(const char *data_dir);
void shutdown();

/**
 * play_sound_seq (ASM -$7E42 / 0x6FB8) — ids 0..9.
 * id 0 gated by walk_beep; ids 1..9 by sounds.
 * id 2 = combat "oh noes" (0x51C2 @ 0x51D8; WAV 02_phrase_short.wav).
 */
void playSoundSeq(uint8_t id, bool sounds_enabled, bool walk_beep_enabled);

/** Overlay title theme (DATA 0x1D79). Loops while attract is up. */
void playTitleTheme(bool loop = true);
void stopTitleTheme();
void stopAll();

}  // namespace mm2::audio
