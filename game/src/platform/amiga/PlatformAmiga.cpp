#include "mm2/platform/Platform.h"

#if defined(AMIGA) || MM2_HOST_AMIGA

#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"

#include "mm2_image32_codec.h"

#include <ace/managers/blit.h>
#include <ace/managers/copper.h>
#include <ace/managers/key.h>
#include <ace/managers/log.h>
#include <ace/managers/memory.h>
#include <ace/managers/system.h>
#include <ace/managers/timer.h>
#include <ace/utils/disk_file.h>
#include <ace/utils/file.h>

namespace mm2::platform {

namespace {

static bool g_ace_ready = false;

static char asciiFromKey(UBYTE ubKey)
{
    if (ubKey >= KEY_COUNT) {
        return 0;
    }
    return static_cast<char>(g_pToAscii[ubKey]);
}

}  // namespace

bool init(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;

    systemCreate();
#ifdef ACE_DEBUG
    logOpen(nullptr);
    logWrite("MM2: systemCreate ok\n");
#endif
    memCreate();
    timerCreate();
    blitManagerCreate();
    copCreate();
    keyCreate();

    if (!mm2AmigaDisplayCreate(MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, 1)) {
#ifdef ACE_DEBUG
        logWrite("MM2: mm2AmigaDisplayCreate failed\n");
#endif
        keyDestroy();
        copDestroy();
        blitManagerDestroy();
        timerDestroy();
        memDestroy();
        systemDestroy();
        return false;
    }

    mm2AmigaDisplayActivate();
    g_ace_ready = true;
#ifdef ACE_DEBUG
    logWrite("MM2: platform init ok (AGA %ux%u %ubpp)\n", MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT,
             MM2_AGA_SCREEN_BPP);
#endif
    return true;
}

void shutdown()
{
    if (!g_ace_ready) {
        return;
    }

    mm2AmigaDisplayDispose();
    keyDestroy();
    copDestroy();
    blitManagerDestroy();
    timerDestroy();
    memDestroy();
    systemDestroy();
    g_ace_ready = false;
}

bool readFile(const char *path, uint8_t **out_data, std::size_t *out_size)
{
    if (!path || !out_data || !out_size) {
        return false;
    }

    tFile *fp = diskFileOpen(path, DISK_FILE_MODE_READ, 1);
    if (!fp) {
        return false;
    }

    const ULONG sz = fileGetSize(fp);
    if (sz == 0) {
        fileClose(fp);
        return false;
    }

    uint8_t *buf = static_cast<uint8_t *>(mm2_malloc(static_cast<std::size_t>(sz)));
    if (!buf) {
        fileClose(fp);
        return false;
    }

    const ULONG got = fileRead(fp, buf, sz);
    fileClose(fp);
    if (got != sz) {
        mm2_free(buf);
        return false;
    }

    *out_data = buf;
    *out_size = static_cast<std::size_t>(sz);
    return true;
}

void freeFileBuffer(uint8_t *data) { mm2_free(data); }

KeyState pollInput()
{
    KeyState keys{};

    if (!g_ace_ready) {
        return keys;
    }

    keyProcess();

    keys.escape = keyCheck(KEY_ESCAPE) != 0;
    keys.enter = keyCheck(KEY_RETURN) != 0;
    keys.space = keyCheck(KEY_SPACE) != 0;
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

    if (keyUse(KEY_ESCAPE)) {
        keys.quit = true;
        keys.any_key = true;
    }

    for (UBYTE i = 0; i < KEY_COUNT; ++i) {
        if (keyUse(i)) {
            keys.any_key = true;
            const char ch = asciiFromKey(i);
            if (ch) {
                keys.last_ascii = ch;
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

    if (!g_ace_ready) {
        return;
    }

    /* Drawing is planar blits into pBack during render; here we only end the frame. */
    mm2AmigaDisplayFrameEnd();
}

void clearScreen() { mm2_amiga_clear_screen(); }

void blitImage32(const ::mm2_image32_file *img, int frame_index, int x, int y)
{
    if (!g_ace_ready || !img || frame_index < 0) {
        return;
    }
    mm2_amiga_blit_frame(img, static_cast<uint16_t>(frame_index), static_cast<UWORD>(x),
                         static_cast<UWORD>(y));
}

void setWindowTitle(const char *title) { (void)title; }

void setWindowSize(int width, int height) { (void)width; (void)height; }

void setPresentScale(int scale) { (void)scale; }

void delayMs(int ms)
{
    if (ms <= 0) {
        return;
    }
    timerWaitUs(static_cast<UWORD>(ms * 1000));
}

const char *hostName() { return "ACE-AGA-6bpp"; }

}  // namespace mm2::platform

#endif
