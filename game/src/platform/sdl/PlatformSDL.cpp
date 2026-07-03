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
        } else if (ev.type == SDL_KEYDOWN) {
            k.any_key = true;
            if (ev.key.keysym.mod & KMOD_CTRL) {
                k.ctrl = true;
            }

            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE:
                k.escape = true;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                k.enter = true;
                break;
            case SDLK_SPACE:
                k.space = true;
                break;
            case SDLK_UP:
                k.up = true;
                break;
            case SDLK_DOWN:
                k.down = true;
                break;
            case SDLK_LEFT:
                k.left = true;
                break;
            case SDLK_RIGHT:
                k.right = true;
                break;
            case SDLK_KP_2:
                k.down = true;
                break;
            case SDLK_KP_4:
                k.left = true;
                break;
            case SDLK_KP_6:
                k.right = true;
                break;
            case SDLK_KP_8:
                k.up = true;
                break;
            case SDLK_c:
                k.key_c = true;
                break;
            case SDLK_d:
                k.key_d = true;
                break;
            case SDLK_n:
                k.key_n = true;
                break;
            case SDLK_m:
                k.key_m = true;
                break;
            case SDLK_o:
                k.key_o = true;
                break;
            case SDLK_p:
                k.key_p = true;
                break;
            case SDLK_q:
                k.key_q = true;
                break;
            case SDLK_BACKSPACE:
                k.backspace = true;
                break;
            default:
                break;
            }

            const SDL_Keycode sym = ev.key.keysym.sym;
            if (sym >= SDLK_a && sym <= SDLK_z) {
                const bool upper =
                    ((ev.key.keysym.mod & KMOD_SHIFT) != 0) ^ ((ev.key.keysym.mod & KMOD_CAPS) != 0);
                k.last_ascii = static_cast<char>((upper ? 'A' : 'a') + (sym - SDLK_a));
            } else if (sym >= SDLK_0 && sym <= SDLK_9) {
                k.last_ascii = static_cast<char>('0' + (sym - SDLK_0));
            }

        }

    }

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
    static const char *const kCandidates[] = {hint, ".", "..", "../..", "../../.."};

    for (const char *dir : kCandidates) {
        if (!dir || dir[0] == '\0') {
            continue;
        }
        if (!joinDataPath(probe, MM2_PATH_SCRATCH_CAP, dir, "intro.32")) {
            continue;
        }
        SDL_RWops *rw = SDL_RWFromFile(probe, "rb");
        if (!rw) {
            continue;
        }
        SDL_RWclose(rw);
        if (dir[0] == '.' && dir[1] == '\0') {
            out[0] = '\0';
        } else {
            std::snprintf(out, out_cap, "%s", dir);
        }
        return true;
    }

    return false;
}

}  // namespace mm2::platform

#endif
