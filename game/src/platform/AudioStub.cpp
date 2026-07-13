#include "mm2/platform/Audio.h"

namespace mm2::audio {

/* No-op backend for unit tests and Amiga (Paula port TBD). Desktop game uses AudioSDL. */

bool init(const char * /*data_dir*/) { return true; }
void shutdown() {}
void playSoundSeq(uint8_t /*id*/, bool /*sounds_enabled*/, bool /*walk_beep_enabled*/) {}
void playTitleTheme(bool /*loop*/) {}
void stopTitleTheme() {}
void stopAll() {}

}  // namespace mm2::audio
