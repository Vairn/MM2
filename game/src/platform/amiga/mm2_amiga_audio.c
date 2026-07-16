/**
 * Amiga Paula generative SFX/title — ASM-faithful tables + play_tone_env timing.
 *
 * ACE owns the machine, so we poke custom.aud[] / DMACON (same end state as
 * audio.device CMD_WRITE in retail 0x738C / 0x7532). Delay(1) → one ACE VBlank
 * tick (PAL 50 Hz ≡ dos.library Delay tick).
 */

#include "mm2/platform/amiga/mm2_amiga_audio.h"

#ifdef AMIGA

#include "mm2_amiga_sfx_tables.h"

#include <ace/managers/memory.h>
#include <ace/managers/timer.h>
#include <ace/utils/custom.h>
#include <hardware/dmabits.h>
#include <mini_std/string.h>

#include <stddef.h>

enum { kWaveBytes = 0x400, kChannels = 4 };

static uint8_t *s_wave = NULL;
static int s_ready = 0;
static Mm2AmigaAudioAbortFn s_abort = NULL;

/* A4-$7352 busy flags; A4-$730C last wave ptr per channel. */
static uint16_t s_busy[kChannels];
static const uint8_t *s_wave_cache[kChannels];

/* Title stream state (overlay 0x283FC). */
static int s_title_active = 0;
static int s_title_loop = 0;
static int s_title_index = 0;

static void delay_one_tick(void)
{
    const ULONG start = timerGet();
    while (timerGetDelta(start, timerGet()) < 1u) {
        /* spin — matches dos.library Delay(1) @ 50 Hz on PAL */
    }
}

static int should_abort(void)
{
    return s_abort && s_abort();
}

/* 0x75E2 fill: ASM writes -$1(a5) after clr.w (always 0). Intended sawtooth is
 * buf[i]=(UBYTE)i via low byte of the loop index (-$2). We use sawtooth. */
static void wave_fill_base(uint8_t *buf)
{
    int i;
    for (i = 0; i < 0x100; ++i) {
        buf[i] = (uint8_t)i;
    }
}

/* 0x760A — 6 octave downsample passes into buf+0x100.. */
static void wave_downsample(uint8_t *buf)
{
    int16_t stride = 1;
    uint8_t *dst = buf + 0x100;
    int16_t pass;
    for (pass = 0; pass < 6; ++pass) {
        int16_t src_i;
        stride = (int16_t)(stride * 2);
        for (src_i = 0; src_i < 0x100; src_i = (int16_t)(src_i + stride)) {
            *dst++ = buf[src_i];
        }
    }
}

static void wave_synth_init(void)
{
    s_wave = (uint8_t *)memAllocChipClear((ULONG)kWaveBytes);
    if (!s_wave) {
        return;
    }
    wave_fill_base(s_wave);
    wave_downsample(s_wave);
}

static void channel_mute(int ch)
{
    if (ch < 0 || ch >= kChannels) {
        return;
    }
    g_pCustom->aud[ch].ac_vol = 0;
    g_pCustom->dmacon = (UWORD)(1u << ch); /* clear DMAF_AUDx (no SETCLR) */
    s_busy[ch] = 0;
    s_wave_cache[ch] = NULL;
}

static void channel_program(int ch, const uint8_t *wave, uint16_t len_bytes, uint16_t period,
                            uint16_t volume)
{
    UWORD len_words;

    if (ch < 0 || ch >= kChannels || !wave || len_bytes < 2) {
        return;
    }
    if (volume > 64u) {
        volume = 64u;
    }
    len_words = (UWORD)(len_bytes / 2u);

    /* Stop DMA briefly so Paula latches new pointer/len (retail AbortIO+BeginIO). */
    g_pCustom->dmacon = (UWORD)(1u << ch);
    g_pCustom->aud[ch].ac_ptr = (volatile UWORD *)(void *)wave;
    g_pCustom->aud[ch].ac_len = len_words;
    g_pCustom->aud[ch].ac_per = period;
    g_pCustom->aud[ch].ac_vol = volume;
    g_pCustom->dmacon = (UWORD)(DMAF_SETCLR | (1u << ch));
    s_busy[ch] = 1;
    s_wave_cache[ch] = wave;
}

/* 0x76AC audio arm — note < 0x64. */
static void play_channel_note(int16_t note, int ch, int16_t volume)
{
    int16_t octave;
    int16_t semi;
    uint16_t period;
    uint16_t woff;
    uint16_t wlen;
    const uint8_t *wave;

    if (note >= 0x64) {
        channel_mute(ch);
        return;
    }
    if (note < 0) {
        return;
    }
    if (volume > 0x20) {
        volume = 0x20;
    }
    if (volume < 0) {
        volume = 0;
    }

    octave = (int16_t)(note / 12);
    semi = (int16_t)(note % 12);
    if (semi < 0 || semi >= 12) {
        return;
    }
    if (octave < 0) {
        octave = 0;
    }
    if (octave > 7) {
        octave = 7;
    }

    period = kMm2AmigaPeriods[semi];
    woff = kMm2AmigaWaveOffs[octave];
    wlen = kMm2AmigaWaveLens[octave];
    if (!s_wave || woff + wlen > kWaveBytes || wlen < 2) {
        return;
    }
    wave = s_wave + woff;

    if (s_wave_cache[ch] == wave && s_busy[ch]) {
        /* Same wave: period/vol update only (0x7532 path). */
        g_pCustom->aud[ch].ac_per = period;
        g_pCustom->aud[ch].ac_vol = (UWORD)volume;
        return;
    }
    channel_program(ch, wave, wlen, period, (uint16_t)volume);
}

void mm2_amiga_audio_play_tone_env(int16_t note, int16_t duration_ticks)
{
    /* 0x77AA */
    int16_t vol = 0x20;
    int16_t env_step = 6;
    int16_t note_lo = (int16_t)(note - 0x0C);

    if (!s_ready) {
        return;
    }

    while (duration_ticks > 0) {
        if (note_lo >= 0) {
            play_channel_note(note_lo, 0, vol);
            play_channel_note(note_lo, 1, vol);
        }
        play_channel_note(note, 2, vol);
        play_channel_note(note, 3, vol);

        delay_one_tick();
        --duration_ticks;

        vol = (int16_t)(vol - env_step);
        if (env_step != 0) {
            env_step = (int16_t)(env_step - 2);
        } else if (vol > 10) {
            --vol;
        } else if (vol > 0 && (duration_ticks & 1) != 0) {
            /* btst #0 of duration low byte ($b): odd remaining → nudge */
            --vol;
        }
        if (vol < 0) {
            vol = 0;
        }
    }

    /* Rest / silence all channels (note 0x64). */
    play_channel_note(0x64, 0, 0);
    play_channel_note(0x64, 1, 0);
    play_channel_note(0x64, 2, 0);
    play_channel_note(0x64, 3, 0);
}

void mm2_amiga_audio_stop_channels(void)
{
    int ch;
    for (ch = 0; ch < kChannels; ++ch) {
        channel_mute(ch);
    }
}

void mm2_amiga_audio_play_sound_seq(uint8_t id)
{
    const uint8_t *p;
    if (!s_ready || id >= MM2_AMIGA_SFX_SEQ_COUNT) {
        return;
    }
    p = kMm2AmigaSeqs[id];
    if (!p) {
        return;
    }

    for (;;) {
        uint8_t note_b;
        uint8_t dur_i;
        int16_t note;
        int16_t dur;

        if (*p == 0xFF) {
            break;
        }
        /* Retail polls -$7BD2 between notes; remake skips abort here so held
         * movement keys do not silence the walk beep / combat stings. Title
         * stream still honors s_abort via title_advance. */
        note_b = *p++;
        dur_i = *p++;
        note = (int16_t)((int16_t)note_b - 0x10);
        if (dur_i < 16) {
            dur = (int16_t)kMm2AmigaDurSfx[dur_i];
        } else {
            dur = 1;
        }
        if (dur == 0) {
            dur = 1;
        }
        mm2_amiga_audio_play_tone_env(note, dur);
    }
}

void mm2_amiga_audio_title_start(int loop)
{
    s_title_loop = loop ? 1 : 0;
    s_title_index = 0;
    s_title_active = s_ready ? 1 : 0;
}

void mm2_amiga_audio_title_stop(void)
{
    s_title_active = 0;
    s_title_index = 0;
    mm2_amiga_audio_stop_channels();
}

int mm2_amiga_audio_title_active(void)
{
    return s_title_active;
}

int mm2_amiga_audio_title_advance(void)
{
    uint8_t note_b;
    uint8_t dur_i;
    int16_t note;
    int16_t dur;

    if (!s_ready || !s_title_active) {
        return 0;
    }
    if (should_abort()) {
        mm2_amiga_audio_title_stop();
        return 0;
    }
    if (s_title_index >= MM2_AMIGA_TITLE_INDEX_LIMIT) {
        if (s_title_loop) {
            s_title_index = 0;
        } else {
            mm2_amiga_audio_title_stop();
            return 0;
        }
    }

    note_b = kMm2AmigaTitleStream[s_title_index++];
    if (s_title_index > MM2_AMIGA_TITLE_INDEX_LIMIT) {
        mm2_amiga_audio_title_stop();
        return 0;
    }
    dur_i = kMm2AmigaTitleStream[s_title_index++];
    if (note_b == 0xFF) {
        if (s_title_loop) {
            s_title_index = 0;
            return 1;
        }
        mm2_amiga_audio_title_stop();
        return 0;
    }

    note = (int16_t)((int16_t)note_b - 0x10);
    if (dur_i < 16) {
        dur = (int16_t)kMm2AmigaDurTitle[dur_i];
    } else {
        dur = 1;
    }
    if (dur == 0) {
        dur = 1;
    }
    mm2_amiga_audio_play_tone_env(note, dur);
    return s_title_active ? 1 : 0;
}

void mm2_amiga_audio_set_abort(Mm2AmigaAudioAbortFn fn)
{
    s_abort = fn;
}

int mm2_amiga_audio_init(void)
{
    int ch;
    if (s_ready) {
        return 1;
    }
    memset(s_busy, 0, sizeof(s_busy));
    memset(s_wave_cache, 0, sizeof(s_wave_cache));
    s_title_active = 0;
    s_title_index = 0;

    wave_synth_init();
    if (!s_wave) {
        return 0;
    }
    for (ch = 0; ch < kChannels; ++ch) {
        channel_mute(ch);
    }
    s_ready = 1;
    return 1;
}

void mm2_amiga_audio_shutdown(void)
{
    if (!s_ready && !s_wave) {
        return;
    }
    mm2_amiga_audio_stop_channels();
    if (s_wave) {
        memFree(s_wave, (ULONG)kWaveBytes);
        s_wave = NULL;
    }
    s_ready = 0;
    s_title_active = 0;
    s_abort = NULL;
}

#else /* !AMIGA */

int mm2_amiga_audio_init(void) { return 0; }
void mm2_amiga_audio_shutdown(void) {}
void mm2_amiga_audio_set_abort(Mm2AmigaAudioAbortFn fn) { (void)fn; }
void mm2_amiga_audio_play_sound_seq(uint8_t id) { (void)id; }
void mm2_amiga_audio_play_tone_env(int16_t note, int16_t duration_ticks)
{
    (void)note;
    (void)duration_ticks;
}
void mm2_amiga_audio_stop_channels(void) {}
void mm2_amiga_audio_title_start(int loop) { (void)loop; }
int mm2_amiga_audio_title_advance(void) { return 0; }
void mm2_amiga_audio_title_stop(void) {}
int mm2_amiga_audio_title_active(void) { return 0; }

#endif /* AMIGA */
