#include "mm2/platform/Platform.h"

#if defined(AMIGA) || MM2_HOST_AMIGA
#include "mm2/DataPath.h"
#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"
#include "mm2/Mm2Dbg.h"
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#include "mm2/gfx/mm2_font8x8.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2/platform/amiga/mm2_amiga_file.h"
#include <ace/types.h>
#include "mm2_image32_codec.h"
#include <ace/managers/blit.h>
#include <ace/managers/copper.h>
#include <ace/managers/key.h>
#include <ace/managers/log.h>
#include <ace/managers/memory.h>
#include <ace/managers/system.h>
#include <ace/managers/timer.h>
#include <ace/utils/disk_file.h>

namespace mm2::platform {

namespace {
static bool g_system_ready = false;
static bool g_display_ready = false;

static char asciiFromKey(UBYTE ubKey)
{
    if (ubKey >= KEY_COUNT) {
        return 0;
    }
    return static_cast<char>(g_pToAscii[ubKey]);
}

/** Roster/party slot labels A..X map to these Amiga key codes (not sequential indices). */
static const UBYTE kRosterSlotKeys[24] = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L,
    KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X,
};

static void pollRosterSlotLetters(KeyState &keys)
{
    for (int i = 0; i < 24; ++i) {
        if (keyUse(kRosterSlotKeys[i])) {
            keys.any_key = true;
            keys.last_ascii = static_cast<char>('A' + i);
            return;
        }
    }
}

}  // namespace
bool init(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    systemCreate();
#ifdef ACE_DEBUG
    logOpen(nullptr);
    MM2_DBG("MM2 DBG: systemCreate ok\n");
#endif
    memCreate();
    timerCreate();
    blitManagerCreate();
    copCreate();
    keyCreate();
    g_system_ready = true;
    MM2_DBG("MM2 DBG: ACE system ok (display deferred)\n");
    return true;
}

bool beginDisplay()
{
    if (!g_system_ready || g_display_ready) {
        return g_display_ready;
    }

    if (!mm2AmigaDisplayCreate(MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, 1)) {
        MM2_DBG("MM2 DBG: mm2AmigaDisplayCreate failed\n");
        return false;
    }

    mm2AmigaDisplayActivate();
    mm2_amiga_font_init();
    g_display_ready = true;
    MM2_DBG("MM2 DBG: Display active AGA %ux%u %ubpp\n", MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT,
            MM2_AGA_SCREEN_BPP);
    return true;
}

void shutdown()
{
    if (g_display_ready) {
        mm2AmigaDisplayDispose();
        g_display_ready = false;
    }

    if (!g_system_ready) {
        return;
    }

    keyDestroy();
    copDestroy();
    blitManagerDestroy();
    timerDestroy();
    memDestroy();
    systemDestroy();
    g_system_ready = false;
}

namespace {
void stripDotSlash(const char *path, char *out, size_t out_cap)
{
    if (!path || !out || out_cap == 0) {
        return;
    }
    if (path[0] == '.' && (path[1] == '/' || path[1] == '\\') && path[2]) {
        snprintf(out, out_cap, "%s", path + 2);
        return;
    }
    snprintf(out, out_cap, "%s", path);
}

}  // namespace
bool readFile(const char *path, uint8_t **out_data, std::size_t *out_size)
{
    if (!path || !out_data || !out_size) {
        return false;
    }

    char *const name = mm2_path_scratch_a();
    stripDotSlash(path, name, MM2_PATH_SCRATCH_CAP);
    char *const candidate = mm2_path_scratch_b();
    static const char *const kPrefixes[] = {"", "dh1:", "dh0:"};
    for (const char *prefix : kPrefixes) {
        if (!joinDataPath(candidate, MM2_PATH_SCRATCH_CAP, prefix, name)) {
            continue;
        }
        if (mm2_amiga_read_file(candidate, out_data, out_size)) {
            MM2_DBG("MM2 DBG: readFile ok '%s' (%lu bytes)\n", candidate,
                    (unsigned long)*out_size);
            return true;
        }
    }

    return mm2_amiga_read_file(name, out_data, out_size) != 0;
}

void freeFileBuffer(uint8_t *data) { mm2_free(data); }
bool resolveDataDir(const char *hint, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return false;
    }

    char *const probe = mm2_path_scratch_a();
    static const char *const kDirs[] = {"", "dh1:", "dh0:"};
    if (hint && hint[0] && !(hint[0] == '.' && hint[1] == '\0')) {
        if (joinDataPath(probe, MM2_PATH_SCRATCH_CAP, hint, "intro.32") && diskFileExists(probe)) {
            if (hint[0] == '.' && (hint[1] == '/' || hint[1] == '\\')) {
                out[0] = '\0';
            } else {
                snprintf(out, out_cap, "%s", hint);
            }
            return true;
        }
    }

    for (const char *dir : kDirs) {
        if (!joinDataPath(probe, MM2_PATH_SCRATCH_CAP, dir, "intro.32")) {
            continue;
        }
        if (diskFileExists(probe)) {
            out[0] = '\0';
            if (dir[0]) {
                snprintf(out, out_cap, "%s", dir);
            }
            return true;
        }
    }

    return false;
}

KeyState pollInput()
{
    KeyState keys{};
    if (!g_display_ready) {
        return keys;
    }

    keyProcess();
    if (keyUse(KEY_ESCAPE)) {
        keys.escape = true;
        keys.any_key = true;
    }
    if (keyUse(KEY_RETURN)) {
        keys.enter = true;
        keys.any_key = true;
    }
    if (keyUse(KEY_SPACE)) {
        keys.space = true;
        keys.any_key = true;
    }
    keys.up = keyCheck(KEY_UP) != 0;
    keys.down = keyCheck(KEY_DOWN) != 0;
    keys.left = keyCheck(KEY_LEFT) != 0;
    keys.right = keyCheck(KEY_RIGHT) != 0;
    keys.ctrl = keyCheck(KEY_CONTROL) != 0;
    keys.backspace = keyCheck(KEY_BACKSPACE) != 0;
    keys.key_c = keyCheck(KEY_C) != 0;
    keys.key_d = keyCheck(KEY_D) != 0;
    keys.key_m = keyCheck(KEY_M) != 0;
    keys.key_n = keyCheck(KEY_N) != 0;
    keys.key_o = keyCheck(KEY_O) != 0;
    keys.key_p = keyCheck(KEY_P) != 0;
    keys.key_q = keyCheck(KEY_Q) != 0;

    pollRosterSlotLetters(keys);

    for (UBYTE i = 0; i < KEY_COUNT; ++i) {
        if (keyUse(i)) {
            keys.any_key = true;
            if (keys.last_ascii == 0) {
                const char ch = asciiFromKey(i);
                if (ch) {
                    keys.last_ascii = ch;
                }
            }
        }
    }

    return keys;
}

void presentFrame(const uint8_t *rgba, int width, int height)
{
    (void)rgba;
    (void)width;
    (void)height;
    if (!g_display_ready) {
        return;
    }

    mm2_amiga_present_end();
    mm2AmigaDisplayFrameEnd();
}

void clearScreen() { mm2_amiga_clear_screen(); }
void applyUiPalette() { mm2_amiga_apply_ui_palette(); }
void logoFadeCapturePalette() { mm2AmigaFadeCapturePalette(); }
void logoFadeBeginIn(int frames) { mm2AmigaFadeBeginIn((UBYTE)frames); }
void logoFadeBeginOut(int frames) { mm2AmigaFadeBeginOut((UBYTE)frames); }
bool logoFadeConsumeDone() { return mm2AmigaFadeConsumeDone() != 0; }
void logoFadeCancel() { mm2AmigaFadeCancel(); }
void blitImage32(const ::mm2_image32_file *img, int frame_index, int x, int y, int opaque)
{
    if (!g_display_ready || !img || frame_index < 0) {
        return;
    }
    mm2_amiga_blit_frame(img, static_cast<uint16_t>(frame_index), static_cast<UWORD>(x),
                         static_cast<UWORD>(y), opaque ? 1 : 0);
}

void setWindowTitle(const char *title) { (void)title; }
void setWindowSize(int width, int height) { (void)width; (void)height; }
void setPresentScale(int scale) { (void)scale; }
void delayMs(int ms)
{
    /* vPortWaitForEnd in presentFrame already paces to display refresh. */
    (void)ms;
}

uint32_t nowTicks()
{
    /* ACE timerGet() = g_sTimerManager.uwFrameCounter, bumped by the VERTB int
     * server (AmigaPorts/ACE system.c) — a true display-rate clock (50 Hz PAL /
     * 60 Hz NTSC), independent of how fast the main loop spins. */
    return static_cast<uint32_t>(timerGet());
}

const char *hostName() { return "ACE-AGA-6bpp"; }
}  // namespace mm2::platform
#endif
