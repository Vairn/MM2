#include "mm2/platform/Audio.h"

#include "mm2/platform/amiga/mm2_amiga_audio.h"

#include <ace/managers/key.h>

namespace mm2::audio {
namespace {

int abortIfAnyKey(void)
{
    /* New presses only — held keys from logo→attract must not kill the theme. */
    for (UBYTE k = 0; k < KEY_COUNT; ++k) {
        if (keyUse(k)) {
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

bool pumpTitleTheme()
{
    /* One cooperative frame: advance Delay ticks from real time, keep animating.
     * Returns true when theme ended/aborted (leave attract). */
    if (!mm2_amiga_audio_title_active()) {
        return false;
    }
    if (!mm2_amiga_audio_title_pump()) {
        return true; /* aborted or finished */
    }
    return false; /* still playing — draw another attract frame */
}

}  // namespace mm2::audio
