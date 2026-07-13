#include "mm2/platform/Audio.h"

#if MM2_HOST_SDL

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>

namespace mm2::audio {
namespace {

constexpr int kSeqCount = 10;
constexpr int kMaxOneShots = 4;
constexpr int kSampleRate = 22050;

struct WavBuf {
    Uint8 *buf = nullptr;
    Uint32 len = 0;
};

struct Voice {
    const Uint8 *data = nullptr;
    Uint32 len = 0;
    Uint32 pos = 0;
    bool active = false;
    bool loop = false;
};

WavBuf g_seq[kSeqCount]{};
WavBuf g_title{};
Voice g_music{};
Voice g_oneshots[kMaxOneShots]{};
SDL_AudioDeviceID g_dev = 0;
bool g_ready = false;

/* Filenames match EXTRACTED/audio export stems. Id 2 WAV is named
 * phrase_short historically but is the combat "oh noes" sting (0x51D8). */
const char *kSeqNames[kSeqCount] = {
    "00_walk_beep.wav", "01_ui_chirp.wav", "02_phrase_short.wav", "03_victory.wav",
    "04_phrase_b.wav",  "05_flourish.wav", "06_combat_a.wav",     "07_menu.wav",
    "08_combat_b.wav",  "09_alert.wav",
};

void freeWav(WavBuf &w)
{
    if (w.buf) {
        SDL_FreeWAV(w.buf);
        w.buf = nullptr;
        w.len = 0;
    }
}

bool tryLoadWav(const char *path, WavBuf &out)
{
    SDL_AudioSpec spec{};
    Uint8 *raw = nullptr;
    Uint32 raw_len = 0;
    if (!SDL_LoadWAV(path, &spec, &raw, &raw_len)) {
        return false;
    }

    SDL_AudioCVT cvt{};
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 1,
                          kSampleRate) < 0) {
        SDL_FreeWAV(raw);
        return false;
    }
    cvt.len = static_cast<int>(raw_len);
    cvt.buf = static_cast<Uint8 *>(SDL_malloc(static_cast<size_t>(cvt.len * cvt.len_mult)));
    if (!cvt.buf) {
        SDL_FreeWAV(raw);
        return false;
    }
    std::memcpy(cvt.buf, raw, raw_len);
    SDL_FreeWAV(raw);
    if (SDL_ConvertAudio(&cvt) < 0) {
        SDL_free(cvt.buf);
        return false;
    }
    freeWav(out);
    out.buf = cvt.buf;
    out.len = static_cast<Uint32>(cvt.len_cvt);
    return out.len > 0;
}

bool loadNamed(const char *data_dir, const char *exe_dir, const char *name, WavBuf &out)
{
    char path[MM2_PATH_SCRATCH_CAP];
    char d0[MM2_PATH_SCRATCH_CAP];
    char d1[MM2_PATH_SCRATCH_CAP];
    char d2[MM2_PATH_SCRATCH_CAP];
    char d3[MM2_PATH_SCRATCH_CAP];
    const char *dirs[4] = {};
    int n = 0;

    if (exe_dir && exe_dir[0] && joinDataPath(d0, sizeof(d0), exe_dir, "audio")) {
        dirs[n++] = d0;
    }
    if (joinDataPath(d1, sizeof(d1), data_dir, "audio")) {
        dirs[n++] = d1;
    }
    if (joinDataPath(d2, sizeof(d2), data_dir, "EXTRACTED/audio")) {
        dirs[n++] = d2;
    }
    if (joinDataPath(d3, sizeof(d3), data_dir, "../EXTRACTED/audio")) {
        dirs[n++] = d3;
    }

    for (int i = 0; i < n; ++i) {
        if (!joinDataPath(path, sizeof(path), dirs[i], name)) {
            continue;
        }
        if (tryLoadWav(path, out)) {
            return true;
        }
    }
    return false;
}

void mixVoice(Voice &v, Sint16 *out, int frames)
{
    if (!v.active || !v.data || v.len == 0) {
        return;
    }
    for (int i = 0; i < frames; ++i) {
        if (v.pos + 1 >= v.len) {
            if (v.loop) {
                v.pos = 0;
            } else {
                v.active = false;
                break;
            }
        }
        const Sint16 s = static_cast<Sint16>(v.data[v.pos] | (v.data[v.pos + 1] << 8));
        v.pos += 2;
        const int mixed = static_cast<int>(out[i]) + static_cast<int>(s);
        if (mixed > 32767) {
            out[i] = 32767;
        } else if (mixed < -32768) {
            out[i] = -32768;
        } else {
            out[i] = static_cast<Sint16>(mixed);
        }
    }
}

void SDLCALL audioCallback(void * /*userdata*/, Uint8 *stream, int len)
{
    auto *out = reinterpret_cast<Sint16 *>(stream);
    const int frames = len / static_cast<int>(sizeof(Sint16));
    std::memset(stream, 0, static_cast<size_t>(len));
    mixVoice(g_music, out, frames);
    for (Voice &v : g_oneshots) {
        mixVoice(v, out, frames);
    }
}

void startVoice(Voice &v, const WavBuf &w, bool loop)
{
    if (!w.buf || w.len == 0) {
        return;
    }
    SDL_LockAudioDevice(g_dev);
    v.data = w.buf;
    v.len = w.len;
    v.pos = 0;
    v.loop = loop;
    v.active = true;
    SDL_UnlockAudioDevice(g_dev);
}

void stopVoice(Voice &v)
{
    SDL_LockAudioDevice(g_dev);
    v.active = false;
    v.data = nullptr;
    v.len = 0;
    v.pos = 0;
    SDL_UnlockAudioDevice(g_dev);
}

}  // namespace

bool init(const char *data_dir)
{
    shutdown();
    if (!data_dir) {
        data_dir = ".";
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "[MM2 audio] SDL_INIT_AUDIO: %s\n", SDL_GetError());
        return false;
    }

    char exe_dir[MM2_PATH_SCRATCH_CAP] = {};
    char *base = SDL_GetBasePath();
    if (base) {
        std::snprintf(exe_dir, sizeof(exe_dir), "%s", base);
        SDL_free(base);
    }

    int loaded = 0;
    for (int i = 0; i < kSeqCount; ++i) {
        if (loadNamed(data_dir, exe_dir, kSeqNames[i], g_seq[i])) {
            ++loaded;
        }
    }
    if (loadNamed(data_dir, exe_dir, "title_theme.wav", g_title)) {
        ++loaded;
    }

    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audioCallback;

    g_dev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (!g_dev) {
        std::fprintf(stderr, "[MM2 audio] OpenAudioDevice: %s\n", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(g_dev, 0);
    g_ready = true;
    std::fprintf(stderr, "[MM2 audio] ready (%d clips)\n", loaded);
    return loaded > 0;
}

void shutdown()
{
    if (g_dev) {
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
    }
    for (WavBuf &w : g_seq) {
        freeWav(w);
    }
    freeWav(g_title);
    g_music = {};
    for (Voice &v : g_oneshots) {
        v = {};
    }
    g_ready = false;
}

void playSoundSeq(uint8_t id, bool sounds_enabled, bool walk_beep_enabled)
{
    if (!g_ready || id >= kSeqCount) {
        return;
    }
    if (id == 0) {
        if (!walk_beep_enabled) {
            return;
        }
    } else if (!sounds_enabled) {
        return;
    }
    if (!g_seq[id].buf) {
        return;
    }

    SDL_LockAudioDevice(g_dev);
    Voice *slot = nullptr;
    for (Voice &v : g_oneshots) {
        if (!v.active) {
            slot = &v;
            break;
        }
    }
    if (!slot) {
        slot = &g_oneshots[0];
    }
    slot->data = g_seq[id].buf;
    slot->len = g_seq[id].len;
    slot->pos = 0;
    slot->loop = false;
    slot->active = true;
    SDL_UnlockAudioDevice(g_dev);
}

void playTitleTheme(bool loop)
{
    if (!g_ready || !g_title.buf) {
        return;
    }
    startVoice(g_music, g_title, loop);
}

void stopTitleTheme()
{
    if (!g_ready) {
        return;
    }
    stopVoice(g_music);
}

void stopAll()
{
    if (!g_ready) {
        return;
    }
    SDL_LockAudioDevice(g_dev);
    g_music.active = false;
    for (Voice &v : g_oneshots) {
        v.active = false;
    }
    SDL_UnlockAudioDevice(g_dev);
}

}  // namespace mm2::audio

#endif
