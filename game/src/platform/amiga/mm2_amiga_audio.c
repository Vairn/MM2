/**
 * Amiga Paula generative SFX/title — ASM-faithful tables + play_tone_env timing.
 *
 * ACE owns the machine, so we poke custom.aud[] / DMACON (same end state as
 * audio.device CMD_WRITE in retail 0x738C / 0x7532).
 *
 * SFX: blocking play_tone_env (retail Delay busy-wait).
 * Title: cooperative — Paula DMA keeps sounding between attract frames; each
 * pump advances envelope ticks from real time and chains the next note with
 * no silence gap (retail froze the attract loop; we don't).
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

/* dos.library Delay(1) = 1/50 s. ACE timerGetPrec: PAL ≈ 0.40 us/tick → 50000. */
enum { kDelayOneTickPrec = 50000u };

static uint8_t *s_wave = NULL;
static int s_ready = 0;
static Mm2AmigaAudioAbortFn s_abort = NULL;

/* A4-$7352 busy flags; A4-$730C last wave ptr per channel. */
static uint16_t s_busy[kChannels];
static const uint8_t *s_wave_cache[kChannels];

/* Title stream (overlay 0x283FC). */
static int s_title_active = 0;
static int s_title_loop = 0;
static int s_title_index = 0;
static ULONG s_title_prec_accum = 0;
static ULONG s_title_prec_last = 0;

/* Async tone state (title path). */
static int s_tone_on = 0;
static int16_t s_tone_note = 0;
static int16_t s_tone_dur = 0;
static int16_t s_tone_vol = 0;
static int16_t s_tone_env = 0;

static void delay_one_tick(void)
{
    const ULONG start = timerGetPrec();
    while ((timerGetPrec() - start) < kDelayOneTickPrec) {
        /* spin — SFX only */
    }
}

static int should_abort(void)
{
    return s_abort && s_abort();
}

static void wave_fill_base(uint8_t *buf)
{
    int i;
    for (i = 0; i < 0x100; ++i) {
        buf[i] = (uint8_t)i;
    }
}

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
    g_pCustom->aud[ch].ac_per = 0;
    g_pCustom->dmacon = (UWORD)(1u << ch);
    s_busy[ch] = 0;
    s_wave_cache[ch] = NULL;
}

static void channel_rest(int ch)
{
    if (ch < 0 || ch >= kChannels) {
        return;
    }
    g_pCustom->aud[ch].ac_vol = 0;
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

    g_pCustom->dmacon = (UWORD)(1u << ch);
    g_pCustom->aud[ch].ac_ptr = (volatile UWORD *)(void *)wave;
    g_pCustom->aud[ch].ac_len = len_words;
    g_pCustom->aud[ch].ac_per = period;
    g_pCustom->aud[ch].ac_vol = volume;
    g_pCustom->dmacon = (UWORD)(DMAF_SETCLR | (1u << ch));
    s_busy[ch] = 1;
    s_wave_cache[ch] = wave;
}

static void play_channel_note(int16_t note, int ch, int16_t volume)
{
    int16_t octave;
    int16_t semi;
    uint16_t period;
    uint16_t woff;
    uint16_t wlen;
    const uint8_t *wave;

    if (note >= 0x64) {
        channel_rest(ch);
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
        g_pCustom->aud[ch].ac_per = period;
        g_pCustom->aud[ch].ac_vol = (UWORD)volume;
        return;
    }
    channel_program(ch, wave, wlen, period, (uint16_t)volume);
}

static void tone_apply_voices(int16_t note, int16_t vol)
{
    const int16_t note_lo = (int16_t)(note - 0x0C);
    if (note_lo >= 0) {
        play_channel_note(note_lo, 0, vol);
        play_channel_note(note_lo, 1, vol);
    }
    play_channel_note(note, 2, vol);
    play_channel_note(note, 3, vol);
}

static void tone_rest_voices(void)
{
    play_channel_note(0x64, 0, 0);
    play_channel_note(0x64, 1, 0);
    play_channel_note(0x64, 2, 0);
    play_channel_note(0x64, 3, 0);
}

static void tone_env_step(void)
{
    /* One iteration of the 0x77AA envelope after Delay(1). */
    s_tone_vol = (int16_t)(s_tone_vol - s_tone_env);
    if (s_tone_env != 0) {
        s_tone_env = (int16_t)(s_tone_env - 2);
    } else if (s_tone_vol > 10) {
        --s_tone_vol;
    } else if (s_tone_vol > 0 && (s_tone_dur & 1) != 0) {
        --s_tone_vol;
    }
    if (s_tone_vol < 0) {
        s_tone_vol = 0;
    }
}

static void tone_start(int16_t note, int16_t duration_ticks)
{
    s_tone_note = note;
    s_tone_dur = duration_ticks;
    s_tone_vol = 0x20;
    s_tone_env = 6;
    s_tone_on = 1;
    tone_apply_voices(s_tone_note, s_tone_vol);
}

static void tone_stop(void)
{
    if (s_tone_on) {
        tone_rest_voices();
    }
    s_tone_on = 0;
    s_tone_dur = 0;
}

/** Advance async tone by one Delay tick (after the wait). Returns 1 if still playing. */
static int tone_tick(void)
{
    if (!s_tone_on) {
        return 0;
    }
    --s_tone_dur;
    tone_env_step();
    if (s_tone_dur <= 0) {
        tone_stop();
        return 0;
    }
    tone_apply_voices(s_tone_note, s_tone_vol);
    return 1;
}

void mm2_amiga_audio_play_tone_env(int16_t note, int16_t duration_ticks)
{
    /* Blocking 0x77AA — used by play_sound_seq. */
    if (!s_ready) {
        return;
    }
    tone_start(note, duration_ticks);
    while (s_tone_on) {
        delay_one_tick();
        (void)tone_tick();
    }
}

void mm2_amiga_audio_stop_channels(void)
{
    int ch;
    tone_stop();
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

static int title_fetch_next_note(void)
{
    uint8_t note_b;
    uint8_t dur_i;
    int16_t note;
    int16_t dur;

    if (!s_title_active) {
        return 0;
    }

    for (;;) {
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
                continue;
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
        tone_start(note, dur);
        return 1;
    }
}

void mm2_amiga_audio_title_start(int loop)
{
    s_title_loop = loop ? 1 : 0;
    s_title_index = 0;
    s_title_active = s_ready ? 1 : 0;
    s_title_prec_last = timerGetPrec();
    s_title_prec_accum = 0;
    tone_stop();
    if (s_title_active) {
        (void)title_fetch_next_note();
    }
}

void mm2_amiga_audio_title_stop(void)
{
    s_title_active = 0;
    s_title_index = 0;
    s_title_prec_accum = 0;
    s_tone_on = 0;
    {
        int ch;
        for (ch = 0; ch < kChannels; ++ch) {
            channel_mute(ch);
        }
    }
}

int mm2_amiga_audio_title_active(void)
{
    return s_title_active;
}

int mm2_amiga_audio_title_pump(void)
{
    ULONG now;
    ULONG delta;
    int due;

    if (!s_ready || !s_title_active) {
        return 0;
    }
    if (should_abort()) {
        mm2_amiga_audio_title_stop();
        return 0; /* finished/aborted */
    }

    now = timerGetPrec();
    delta = now - s_title_prec_last;
    s_title_prec_last = now;
    s_title_prec_accum += delta;

    /* Catch up Delay ticks from real time; chain notes with no frame gap. */
    due = 0;
    while (s_title_prec_accum >= kDelayOneTickPrec) {
        s_title_prec_accum -= kDelayOneTickPrec;
        ++due;
    }
    /* Cap catch-up so a long hitch doesn't blast many notes at once. */
    if (due > 4) {
        due = 4;
    }

    while (due-- > 0) {
        if (s_tone_on) {
            if (!tone_tick()) {
                /* Note ended — start next immediately (no silence). */
                if (!title_fetch_next_note()) {
                    return 0;
                }
            }
        } else if (!title_fetch_next_note()) {
            return 0;
        }
    }

    /* Ensure a note is sounding if stream still active. */
    if (s_title_active && !s_tone_on) {
        if (!title_fetch_next_note()) {
            return 0;
        }
    }

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
    s_tone_on = 0;

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
    mm2_amiga_audio_title_stop();
    if (s_wave) {
        memFree(s_wave, (ULONG)kWaveBytes);
        s_wave = NULL;
    }
    s_ready = 0;
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
int mm2_amiga_audio_title_pump(void) { return 0; }
void mm2_amiga_audio_title_stop(void) {}
int mm2_amiga_audio_title_active(void) { return 0; }

#endif /* AMIGA */
