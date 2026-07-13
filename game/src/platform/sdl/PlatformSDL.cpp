#include "mm2/platform/Platform.h"

#if MM2_HOST_SDL

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"

namespace mm2::platform {

namespace {

SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;
SDL_Texture *g_texture = nullptr;
int g_tex_w = 0;
int g_tex_h = 0;
int g_scale = 2;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
constexpr Uint32 kTexturePixelFormat = SDL_PIXELFORMAT_RGBA8888;

#else
constexpr Uint32 kTexturePixelFormat = SDL_PIXELFORMAT_ABGR8888;

#endif

}  // namespace

bool init(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow("MM2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 400,
                                SDL_WINDOW_SHOWN);
    if (!g_window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    return true;
}

bool beginDisplay() { return true; }
void shutdown()
{
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = nullptr;
    }

    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }

    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }

    SDL_Quit();
}

bool readFile(const char *path, uint8_t **out_data, std::size_t *out_size)
{
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw) {
        return false;
    }

    const Sint64 len = SDL_RWsize(rw);
    if (len <= 0) {
        SDL_RWclose(rw);
        return false;
    }

    auto *buf = static_cast<uint8_t *>(std::malloc(static_cast<std::size_t>(len)));
    if (!buf) {
        SDL_RWclose(rw);
        return false;
    }

    if (SDL_RWread(rw, buf, 1, static_cast<std::size_t>(len)) != static_cast<std::size_t>(len)) {
        std::free(buf);
        SDL_RWclose(rw);
        return false;
    }

    SDL_RWclose(rw);
    *out_data = buf;
    *out_size = static_cast<std::size_t>(len);
    return true;
}

void freeFileBuffer(uint8_t *data) { std::free(data); }

KeyState pollInput()
{
    KeyState k;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            k.quit = true;
        }
    }

    /* Level-sample the keyboard. Relying only on SDL_KEYDOWN makes held movement
     * wait on the OS key-repeat delay after the first press (feels laggy). */
    const Uint8 *kb = SDL_GetKeyboardState(nullptr);
    const SDL_Keymod mods = SDL_GetModState();
    k.ctrl = (mods & KMOD_CTRL) != 0;
    k.escape = kb[SDL_SCANCODE_ESCAPE] != 0;
    k.enter = kb[SDL_SCANCODE_RETURN] != 0 || kb[SDL_SCANCODE_KP_ENTER] != 0;
    k.space = kb[SDL_SCANCODE_SPACE] != 0;
    k.backspace = kb[SDL_SCANCODE_BACKSPACE] != 0;

    const bool up = kb[SDL_SCANCODE_UP] != 0 || kb[SDL_SCANCODE_KP_8] != 0;
    const bool down = kb[SDL_SCANCODE_DOWN] != 0 || kb[SDL_SCANCODE_KP_2] != 0;
    const bool left = kb[SDL_SCANCODE_LEFT] != 0 || kb[SDL_SCANCODE_KP_4] != 0;
    const bool right = kb[SDL_SCANCODE_RIGHT] != 0 || kb[SDL_SCANCODE_KP_6] != 0;
    const bool arrow = up || down || left || right;

    /* First arrow frame fires immediately; while held, re-fire ~8 Hz so walk
     * does not burn a step every vsync. */
    static bool s_arrow_held = false;
    static Uint32 s_arrow_last_ms = 0;
    constexpr Uint32 kArrowRepeatMs = 120;
    if (arrow) {
        const Uint32 now = SDL_GetTicks();
        if (!s_arrow_held || now - s_arrow_last_ms >= kArrowRepeatMs) {
            k.up = up;
            k.down = down;
            k.left = left;
            k.right = right;
            s_arrow_last_ms = now;
            s_arrow_held = true;
        }
    } else {
        s_arrow_held = false;
    }

    /* Edge-trigger letters/digits and named menu keys so combat/menus get one
     * shot per press without OS repeat delay or per-frame spam while held. */
    static bool s_prev_letter[26]{};
    static bool s_prev_digit[10]{};
    static bool s_prev_named[7]{};
    const bool upper = ((mods & KMOD_SHIFT) != 0) ^ ((mods & KMOD_CAPS) != 0);
    for (int i = 0; i < 26; ++i) {
        const bool key_down = kb[SDL_SCANCODE_A + i] != 0;
        if (key_down && !s_prev_letter[i] && k.last_ascii == 0) {
            k.last_ascii = static_cast<char>((upper ? 'A' : 'a') + i);
        }
        s_prev_letter[i] = key_down;
    }
    /* SDL_SCANCODE_1..9 are 30..38, then SDL_SCANCODE_0 is 39 — not 0+i.
     * Same for KP_1..9 then KP_0. KP_2/4/6/8 are already movement aliases above. */
    static const SDL_Scancode kRowDigit[10] = {
        SDL_SCANCODE_0, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
        SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,
    };
    static const SDL_Scancode kKpDigit[10] = {
        SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_4,
        SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9,
    };
    for (int i = 0; i < 10; ++i) {
        bool key_down = kb[kRowDigit[i]] != 0;
        if (i != 2 && i != 4 && i != 6 && i != 8) {
            key_down = key_down || kb[kKpDigit[i]] != 0;
        }
        if (key_down && !s_prev_digit[i] && k.last_ascii == 0) {
            k.last_ascii = static_cast<char>('0' + i);
        }
        s_prev_digit[i] = key_down;
    }

    const bool named_down[7] = {
        kb[SDL_SCANCODE_C] != 0, kb[SDL_SCANCODE_D] != 0, kb[SDL_SCANCODE_M] != 0, kb[SDL_SCANCODE_N] != 0,
        kb[SDL_SCANCODE_O] != 0, kb[SDL_SCANCODE_P] != 0, kb[SDL_SCANCODE_Q] != 0,
    };
    bool *named_flags[7] = {&k.key_c, &k.key_d, &k.key_m, &k.key_n, &k.key_o, &k.key_p, &k.key_q};
    for (int i = 0; i < 7; ++i) {
        *named_flags[i] = named_down[i] && !s_prev_named[i];
        s_prev_named[i] = named_down[i];
    }

    /* Escape/Enter/Space/Backspace: edge for one-shot menus; level also kept so
     * held SPACE still works across slow frames (Amiga parity). */
    static bool s_prev_escape = false;
    static bool s_prev_enter = false;
    static bool s_prev_space = false;
    static bool s_prev_backspace = false;
    const bool esc_edge = k.escape && !s_prev_escape;
    const bool enter_edge = k.enter && !s_prev_enter;
    const bool space_edge = k.space && !s_prev_space;
    const bool bs_edge = k.backspace && !s_prev_backspace;
    s_prev_escape = k.escape;
    s_prev_enter = k.enter;
    s_prev_space = k.space;
    s_prev_backspace = k.backspace;
    /* Keep level for space/enter (continue prompts); edge-only for escape/backspace. */
    k.escape = esc_edge;
    k.backspace = bs_edge;
    (void)enter_edge;
    (void)space_edge;

    k.any_key = k.escape || k.enter || k.space || k.backspace || k.last_ascii != 0 || k.up || k.down ||
                k.left || k.right || k.key_c || k.key_d || k.key_m || k.key_n || k.key_o || k.key_p ||
                k.key_q;
    return k;
}

void presentFrame(const uint8_t *rgba, int width, int height)
{
    if (!g_renderer || !rgba || width <= 0 || height <= 0) {
        return;
    }

    if (!g_texture || width != g_tex_w || height != g_tex_h) {
        if (g_texture) {
            SDL_DestroyTexture(g_texture);
        }

        g_texture = SDL_CreateTexture(g_renderer, kTexturePixelFormat, SDL_TEXTUREACCESS_STREAMING, width, height);
        g_tex_w = width;
        g_tex_h = height;
    }

    if (!g_texture) {
        return;
    }

    const int scaled_w = width * g_scale;
    const int scaled_h = height * g_scale;
    int win_w = 0;
    int win_h = 0;
    if (SDL_GetRendererOutputSize(g_renderer, &win_w, &win_h) != 0) {
        SDL_GetWindowSize(g_window, &win_w, &win_h);
    }

    const int dst_x = std::max(0, (win_w - scaled_w) / 2);
    const int dst_y = std::max(0, (win_h - scaled_h) / 2);
    SDL_Rect dst{dst_x, dst_y, scaled_w, scaled_h};

    SDL_UpdateTexture(g_texture, nullptr, rgba, width * 4);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, nullptr, &dst);
    SDL_RenderPresent(g_renderer);
}

void waitVblank() {}

void setPresentScale(int scale)
{
    if (scale > 0) {
        g_scale = scale;
    }

}

void setWindowTitle(const char *title)
{
    if (g_window && title) {
        SDL_SetWindowTitle(g_window, title);
    }

}

void setWindowSize(int width, int height)
{
    if (g_window && width > 0 && height > 0) {
        SDL_SetWindowSize(g_window, width, height);
    }

}

void delayMs(int ms) { SDL_Delay(static_cast<Uint32>(ms)); }

uint32_t nowTicks()
{
    /* ~60 ticks/sec to match the Amiga VBlank-frame clock used for animation pacing. */
    return (SDL_GetTicks() * 60u) / 1000u;
}

const char *hostName() { return "SDL2"; }

bool resolveDataDir(const char *hint, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return false;
    }

    char *const probe = mm2_path_scratch_a();

    auto probeMapDat = [&](const char *dir) -> bool {
        if (!dir || dir[0] == '\0') {
            return false;
        }
        if (!joinDataPath(probe, MM2_PATH_SCRATCH_CAP, dir, "map.dat")) {
            return false;
        }
        SDL_RWops *rw = SDL_RWFromFile(probe, "rb");
        if (!rw) {
            return false;
        }
        SDL_RWclose(rw);
        return true;
    };

    auto writeResolved = [&](const char *dir) {
        std::snprintf(out, out_cap, "%s", (dir && dir[0]) ? dir : ".");
    };

    /* Explicit CLI path: validate only that directory (matches playscreen_golden). */
    if (hint && hint[0] && !(hint[0] == '.' && hint[1] == '\0')) {
        if (probeMapDat(hint)) {
            writeResolved(hint);
            return true;
        }
        return false;
    }

    static const char *const kCandidates[] = {hint, ".", "..", "../..", "../../.."};
    for (const char *dir : kCandidates) {
        if (probeMapDat(dir)) {
            writeResolved(dir);
            return true;
        }
    }

    return false;
}

}  // namespace mm2::platform

#endif
