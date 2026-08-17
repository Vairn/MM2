#include "mm2/ui/AguiAtlas.h"

#include "mm2/DataPath.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/runtime/PathScratch.h"

#include <cstdio>
#include <cstring>

namespace mm2::ui {
namespace {

bool readEntireFile(const char *path, uint8_t **out, std::size_t *out_size)
{
    *out = nullptr;
    *out_size = 0;
    FILE *f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return false;
    }
    const long sz = std::ftell(f);
    if (sz <= 0) {
        std::fclose(f);
        return false;
    }
    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    auto *buf = new uint8_t[static_cast<std::size_t>(sz)];
    if (!buf) {
        std::fclose(f);
        return false;
    }
    if (std::fread(buf, 1, static_cast<std::size_t>(sz), f) != static_cast<std::size_t>(sz)) {
        delete[] buf;
        std::fclose(f);
        return false;
    }
    std::fclose(f);
    *out = buf;
    *out_size = static_cast<std::size_t>(sz);
    return true;
}

/** Minimal JSON: find "key": <int> after a name token. */
bool jsonIntAfter(const char *json, const char *key, int *out)
{
    const char *p = std::strstr(json, key);
    if (!p) {
        return false;
    }
    p = std::strchr(p, ':');
    if (!p) {
        return false;
    }
    ++p;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    *out = 0;
    bool any = false;
    while (*p >= '0' && *p <= '9') {
        any = true;
        *out = *out * 10 + (*p - '0');
        ++p;
    }
    return any;
}

}  // namespace

bool AguiAtlas::load(const char *data_dir)
{
    unload();
    if (!data_dir || !*data_dir) {
        return false;
    }

    /* data_dir may be ``amiga/`` (dist); also accept sibling ``ui/agui`` next to it. */
    const char *rel_paths[] = {
        "ui/agui/agui_atlas.rgba",
        "../ui/agui/agui_atlas.rgba",
        "data/ui/agui/agui_atlas.rgba",
        "game/data/ui/agui/agui_atlas.rgba",
    };
    const char *json_rels[] = {
        "ui/agui/agui_atlas.json",
        "../ui/agui/agui_atlas.json",
        "data/ui/agui/agui_atlas.json",
        "game/data/ui/agui/agui_atlas.json",
    };

    char rgba_path[MM2_PATH_SCRATCH_CAP];
    char json_path[MM2_PATH_SCRATCH_CAP];
    bool found = false;
    for (int i = 0; i < 4; ++i) {
        if (mm2::joinDataPath(rgba_path, MM2_PATH_SCRATCH_CAP, data_dir, rel_paths[i]) &&
            mm2::joinDataPath(json_path, MM2_PATH_SCRATCH_CAP, data_dir, json_rels[i])) {
            FILE *probe = std::fopen(rgba_path, "rb");
            if (probe) {
                std::fclose(probe);
                found = true;
                break;
            }
        }
    }
    if (!found) {
        return false;
    }

    uint8_t *json_bytes = nullptr;
    std::size_t json_size = 0;
    if (!readEntireFile(json_path, &json_bytes, &json_size) || json_size == 0) {
        delete[] json_bytes;
        return false;
    }
    /* Ensure NUL for strstr parsing. */
    auto *json = new char[json_size + 1];
    if (!json) {
        delete[] json_bytes;
        return false;
    }
    std::memcpy(json, json_bytes, json_size);
    json[json_size] = '\0';
    delete[] json_bytes;

    int w = 0;
    int h = 0;
    if (!jsonIntAfter(json, "\"width\"", &w) || !jsonIntAfter(json, "\"height\"", &h) || w <= 0 ||
        h <= 0) {
        delete[] json;
        return false;
    }

    /* Parse sprites: {"name":"icons/cast","x":..,"y":..,"w":..,"h":..} */
    rect_count_ = 0;
    const char *cursor = json;
    while (rect_count_ < kMaxRects) {
        const char *name_key = std::strstr(cursor, "\"name\"");
        if (!name_key) {
            break;
        }
        const char *q1 = std::strchr(name_key + 6, '"');
        if (!q1) {
            break;
        }
        ++q1;
        const char *q2 = std::strchr(q1, '"');
        if (!q2 || q2 - q1 <= 0 || q2 - q1 >= 31) {
            cursor = q1;
            continue;
        }
        char name[32];
        const int nlen = static_cast<int>(q2 - q1);
        std::memcpy(name, q1, static_cast<std::size_t>(nlen));
        name[nlen] = '\0';

        int x = 0, y = 0, rw = 0, rh = 0;
        if (!jsonIntAfter(q2, "\"x\"", &x) || !jsonIntAfter(q2, "\"y\"", &y) ||
            !jsonIntAfter(q2, "\"w\"", &rw) || !jsonIntAfter(q2, "\"h\"", &rh)) {
            cursor = q2 + 1;
            continue;
        }
        std::memcpy(names_[rect_count_], name, 32);
        rects_[rect_count_].x = x;
        rects_[rect_count_].y = y;
        rects_[rect_count_].w = rw;
        rects_[rect_count_].h = rh;
        ++rect_count_;
        cursor = q2 + 1;
    }
    delete[] json;

    uint8_t *rgba = nullptr;
    std::size_t rgba_size = 0;
    if (!readEntireFile(rgba_path, &rgba, &rgba_size)) {
        return false;
    }
    const std::size_t expected = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    if (rgba_size != expected) {
        delete[] rgba;
        return false;
    }

    rgba_ = rgba;
    width_ = w;
    height_ = h;
    return rect_count_ > 0;
}

void AguiAtlas::unload()
{
    delete[] rgba_;
    rgba_ = nullptr;
    width_ = 0;
    height_ = 0;
    rect_count_ = 0;
}

const AguiAtlasRect *AguiAtlas::find(const char *name) const
{
    if (!name) {
        return nullptr;
    }
    for (int i = 0; i < rect_count_; ++i) {
        if (std::strcmp(names_[i], name) == 0) {
            return &rects_[i];
        }
    }
    return nullptr;
}

void AguiAtlas::blitNamed(gfx::ScreenCompositor &c, const char *name, int dst_x, int dst_y) const
{
    const AguiAtlasRect *r = find(name);
    if (r) {
        blit(c, *r, dst_x, dst_y);
    }
}

void AguiAtlas::blit(gfx::ScreenCompositor &c, const AguiAtlasRect &src, int dst_x, int dst_y) const
{
    if (!ready() || src.w <= 0 || src.h <= 0) {
        return;
    }
    if (src.x < 0 || src.y < 0 || src.x + src.w > width_ || src.y + src.h > height_) {
        return;
    }
    /* Row-by-row blit from atlas. */
    for (int row = 0; row < src.h; ++row) {
        const uint8_t *line = rgba_ + ((src.y + row) * width_ + src.x) * 4;
        c.blitRgba(line, src.w, 1, dst_x, dst_y + row, true, 255);
    }
}

}  // namespace mm2::ui
