#pragma once

#include "mm2/CppStdCompat.h"

#include "mm2/Config.h"

namespace mm2::platform {

struct KeyState {
    bool quit = false;
    bool escape = false;
    bool enter = false;
    bool any_key = false;
    bool space = false;
    bool ctrl = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool key_c = false;
    bool key_d = false;
    bool key_m = false;
    bool key_n = false;
    bool key_o = false;
    bool key_p = false;
    bool key_q = false;
    bool backspace = false;
  /** Printable ASCII letter/digit from last KEYDOWN, else 0 (preserves case). */
    char last_ascii = 0;
};

bool init(int *argc, char ***argv);
/** Amiga: create the 320x200 playfield after data-path probe. SDL: no-op. */
bool beginDisplay();
void shutdown();

bool readFile(const char *path, uint8_t **out_data, std::size_t *out_size);
void freeFileBuffer(uint8_t *data);

KeyState pollInput();
void presentFrame(const uint8_t *rgba, int width, int height);
void setPresentScale(int scale);
void setWindowTitle(const char *title);
void setWindowSize(int width, int height);
void delayMs(int ms);

/**
 * Monotonic frame-tick clock for animation pacing (≈60 ticks/sec).
 * Amiga: ACE timerGet() (VBlank-interrupt counter, true 50 Hz PAL / 60 Hz NTSC).
 * SDL: derived from SDL_GetTicks(). Use signed 16-bit deltas to compare deadlines
 * (the Amiga counter is a 16-bit frame count), matching LoL's timerGet() scheduling.
 */
uint32_t nowTicks();

const char *hostName();

#if MM2_HOST_AMIGA
/** Probe intro.32; writes "" or "dh1:" etc. into out. Returns false if not found. */
bool resolveDataDir(const char *hint, char *out, size_t out_cap);
#endif

}  // namespace mm2::platform

#if MM2_HOST_AMIGA
#include "mm2_image32_codec.h"
namespace mm2::platform {
void clearScreen();
/** @param opaque Non-zero for solid layers (intro.32); zero for pen-0 masked cels. */
void blitImage32(const ::mm2_image32_file *img, int frame_index, int x, int y, int opaque = 0);
void applyUiPalette(void);
void logoFadeCapturePalette(void);
void logoFadeBeginIn(int frames);
void logoFadeBeginOut(int frames);
bool logoFadeConsumeDone(void);
}
#endif
