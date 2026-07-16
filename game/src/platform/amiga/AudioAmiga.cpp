#include "mm2/platform/Audio.h"

#include "mm2/platform/amiga/mm2_amiga_audio.h"

#include <ace/managers/key.h>

namespace mm2::audio {
namespace {

int abortIfAnyKey(void)
{
    /* Approximate -$7BD2: any key aborts current sequence / title note. */
    for (UBYTE k = 0; k < KEY_COUNT; ++k) {
        if (keyCheck(k)) {
            return 1;
        }
    }
    return 0;
}

}  // namespace

bool init(const char * /*data_dir*/)
{
    mm2_amiga_audio_set_abort(abortIfAnyKey);
    return mm2_amiga_audio_init() != 0;
}

void shutdown()
{
    mm2_amiga_audio_shutdown();
}

void playSoundSeq(uint8_t id, bool sounds_enabled, bool walk_beep_enabled)
{
    /* Gates mirror play_sound_seq @ 0x6FB8. */
    if (id == 0) {
        if (!walk_beep_enabled) {
            return;
        }
    } else if (!sounds_enabled) {
        return;
    }
    if (id > 9) {
        return;
    }
    mm2_amiga_audio_play_sound_seq(id);
}

void playTitleTheme(bool loop)
{
    mm2_amiga_audio_title_start(loop ? 1 : 0);
}

void stopTitleTheme()
{
    mm2_amiga_audio_title_stop();
}

void stopAll()
{
    mm2_amiga_audio_title_stop();
    mm2_amiga_audio_stop_channels();
}

void pumpTitleTheme()
{
    if (mm2_amiga_audio_title_active()) {
        (void)mm2_amiga_audio_title_advance();
    }
}

}  // namespace mm2::audio
