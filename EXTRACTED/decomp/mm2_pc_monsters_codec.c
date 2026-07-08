#include "mm2_pc_monsters_codec.h"

#include "mm2_codec_platform.h"
#include "mm2_pc_gfx_codec.h"

#include <stdio.h>

static const int k_ega_monster_xlat[16] = {0, 1, 2, 9, 6, 8, 10, 3, 4, 5, 7, 11, 12, 13, 14, 15};

static const uint8_t k_cga_palette_0[4][3] = {{0, 0, 0}, {0, 170, 0}, {170, 0, 0}, {170, 85, 0}};
static const uint8_t k_cga_palette_1[4][3] = {{0, 0, 0}, {85, 255, 255}, {255, 85, 255}, {255, 255, 255}};
static const uint8_t k_ega_rgb[16][3] = {
    {0, 0, 0},       {0, 0, 170},     {0, 170, 0},     {0, 170, 170},   {170, 0, 0},
    {170, 0, 170},   {170, 85, 0},    {170, 170, 170}, {85, 85, 85},    {85, 85, 255},
    {85, 255, 85},   {85, 255, 255},  {255, 85, 85},   {255, 85, 255},  {255, 255, 85},
    {255, 255, 255},
};

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int pc_bpp_for_path(const char *path)
{
    const char *dot = path ? strrchr(path, '.') : NULL;
    if (dot && dot[1] == '1' && dot[2] == '6' && dot[3] == '\0') {
        return 4;
    }
    return 2;
}

static void palette_color(int idx, int bpp, int cga_palette, uint8_t out[3])
{
    if (bpp == 2) {
        const uint8_t (*pal)[3] = cga_palette ? k_cga_palette_1 : k_cga_palette_0;
        idx &= 3;
        out[0] = pal[idx][0];
        out[1] = pal[idx][1];
        out[2] = pal[idx][2];
    } else {
        idx &= 0xF;
        out[0] = k_ega_rgb[idx][0];
        out[1] = k_ega_rgb[idx][1];
        out[2] = k_ega_rgb[idx][2];
    }
}

static mm2_image32_error read_file_bytes(const char *path, uint8_t **out_data, size_t *out_len)
{
    FILE *f;
    long sz;
    size_t n;
    uint8_t *buf;

    if (!path || !out_data || !out_len) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    *out_data = NULL;
    *out_len = 0;
    f = fopen(path, "rb");
    if (!f) {
        return MM2_IMAGE32_ERR_IO;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return MM2_IMAGE32_ERR_IO;
    }
    sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return MM2_IMAGE32_ERR_IO;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return MM2_IMAGE32_ERR_IO;
    }
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return MM2_IMAGE32_ERR_NOMEM;
    }
    n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        return MM2_IMAGE32_ERR_IO;
    }
    *out_data = buf;
    *out_len = (size_t)sz;
    return MM2_IMAGE32_OK;
}

static uint8_t *decompress_monster_blob(const uint8_t *raw, size_t raw_len, size_t off, size_t *out_len)
{
    uint32_t dec_size;
    size_t got;
    uint8_t *dec;

    if (!raw || off + 8 > raw_len || !out_len) {
        return NULL;
    }
    dec_size = read_u32_le(raw + off);
    if (dec_size < 4 || dec_size > 512u * 1024u) {
        return NULL;
    }
    dec = mm2_pc_lzw_decompress(raw + off + 4, raw_len - off - 4, (size_t)dec_size, &got);
    if (!dec || got < dec_size) {
        free(dec);
        return NULL;
    }
    *out_len = got;
    return dec;
}

static int monster_blob_valid(const uint8_t *raw, size_t raw_len, size_t file_off, size_t table_end)
{
    size_t dec_len = 0;
    uint8_t *dec;
    int ok;

    if (file_off < table_end) {
        return 0;
    }
    dec = decompress_monster_blob(raw, raw_len, file_off, &dec_len);
    if (!dec || dec_len < 1) {
        free(dec);
        return 0;
    }
    ok = (dec[0] & 0x3F) > 0;
    free(dec);
    return ok;
}

static size_t monster_seek_from_header(const uint8_t *raw, size_t raw_len, int entry, size_t header_bytes)
{
    if (entry < 0 || (size_t)entry * 4u + 4u > header_bytes || header_bytes > raw_len) {
        return 0;
    }
    return (size_t)read_u32_le(raw + entry * 4);
}

void mm2_pc_monsters_atlas_free(mm2_pc_monsters_atlas *atlas)
{
    if (!atlas) {
        return;
    }
    free(atlas->raw);
    memset(atlas, 0, sizeof(*atlas));
}

mm2_image32_error mm2_pc_monsters_atlas_load(const char *path, mm2_pc_monsters_atlas *out)
{
    mm2_image32_error err;

    if (!path || !out) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    mm2_pc_monsters_atlas_free(out);
    err = read_file_bytes(path, &out->raw, &out->raw_len);
    if (err != MM2_IMAGE32_OK) {
        return err;
    }
    if (out->raw_len < 4) {
        mm2_pc_monsters_atlas_free(out);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    out->bpp = pc_bpp_for_path(path);
    out->header_bytes = (size_t)read_u32_le(out->raw);
    if (out->header_bytes < 4 || out->header_bytes > out->raw_len) {
        mm2_pc_monsters_atlas_free(out);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    return MM2_IMAGE32_OK;
}

size_t mm2_pc_monsters_blob_offset_for_id(const mm2_pc_monsters_atlas *atlas, int picture_id)
{
    size_t fo;

    if (!atlas || !atlas->raw || picture_id < 1) {
        return 0;
    }
    fo = monster_seek_from_header(atlas->raw, atlas->raw_len, picture_id - 1, atlas->header_bytes);
    if (fo != 0 && monster_blob_valid(atlas->raw, atlas->raw_len, fo, atlas->header_bytes)) {
        return fo;
    }
    return 0;
}

mm2_image32_error mm2_pc_monsters_resolve_picture_id(const mm2_pc_monsters_atlas *atlas, int picture_id,
                                                     int *out_used_id)
{
    int pid;

    if (!atlas || picture_id < 1 || !out_used_id) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    pid = picture_id;
    while (pid <= MM2_PC_MONSTERS_MAX_PICTURE_ID) {
        if (mm2_pc_monsters_blob_offset_for_id(atlas, pid) != 0) {
            *out_used_id = pid;
            return MM2_IMAGE32_OK;
        }
        ++pid;
    }
    return MM2_IMAGE32_ERR_IO;
}

static int parse_frame_header(const uint8_t *dec, size_t dec_len, size_t frame_off, size_t frame_end_off, int *x,
                              int *y, int *width, int *height)
{
    if (frame_off + 4 > dec_len || frame_end_off > dec_len || frame_end_off <= frame_off + 4) {
        return 0;
    }
    *x = dec[frame_off];
    *y = dec[frame_off + 1];
    *width = dec[frame_off + 2];
    *height = dec[frame_off + 3];
    if (*width < 1 || *height < 1 || *width > 128 || *height > 128) {
        return 0;
    }
    return 1;
}

static void sort_size_t(size_t *vals, int count)
{
    int i;
    int j;
    for (i = 0; i < count - 1; ++i) {
        for (j = i + 1; j < count; ++j) {
            if (vals[j] < vals[i]) {
                size_t tmp = vals[i];
                vals[i] = vals[j];
                vals[j] = tmp;
            }
        }
    }
}

static mm2_image32_error parse_picture_frames(const uint8_t *dec, size_t dec_len, mm2_pc_monster_picture *pic)
{
    int frame_count;
    size_t table_need;
    size_t *inner;
    size_t *ordered;
    int ordered_count;
    int fi;

    if (!dec || dec_len < 2) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    frame_count = dec[0] & 0x3F;
    table_need = 2u + (size_t)frame_count * 2u;
    if (frame_count <= 0 || table_need > dec_len) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }

    inner = (size_t *)calloc((size_t)frame_count, sizeof(size_t));
    ordered = (size_t *)calloc((size_t)frame_count, sizeof(size_t));
    if (!inner || !ordered) {
        free(inner);
        free(ordered);
        return MM2_IMAGE32_ERR_NOMEM;
    }
    for (fi = 0; fi < frame_count; ++fi) {
        inner[fi] = read_u16_le(dec + 2 + fi * 2);
    }
    ordered_count = 0;
    for (fi = 0; fi < frame_count; ++fi) {
        if (inner[fi] > 0 && inner[fi] <= dec_len) {
            ordered[ordered_count++] = inner[fi];
        }
    }
    sort_size_t(ordered, ordered_count);

    pic->frames = (mm2_pc_monster_frame *)calloc((size_t)frame_count, sizeof(mm2_pc_monster_frame));
    if (!pic->frames) {
        free(inner);
        free(ordered);
        return MM2_IMAGE32_ERR_NOMEM;
    }
    pic->frame_count = 0;

    for (fi = 0; fi < frame_count; ++fi) {
        size_t inner_off = inner[fi];
        size_t frame_end_off = dec_len;
        int oi;
        int x;
        int y;
        int width;
        int height;
        mm2_pc_monster_frame *fr;
        size_t stream_len;

        if (inner_off == 0 || inner_off >= dec_len) {
            continue;
        }
        for (oi = 0; oi < ordered_count; ++oi) {
            if (ordered[oi] > inner_off) {
                frame_end_off = ordered[oi];
                break;
            }
        }
        if (!parse_frame_header(dec, dec_len, inner_off, frame_end_off, &x, &y, &width, &height)) {
            continue;
        }
        stream_len = frame_end_off - inner_off - 4u;
        fr = &pic->frames[pic->frame_count++];
        fr->frame_index = fi;
        fr->x = x;
        fr->y = y;
        fr->width = width;
        fr->height = height;
        fr->stream = (uint8_t *)malloc(stream_len);
        if (!fr->stream) {
            free(inner);
            free(ordered);
            return MM2_IMAGE32_ERR_NOMEM;
        }
        memcpy(fr->stream, dec + inner_off + 4, stream_len);
        fr->stream_len = stream_len;
    }

    free(inner);
    free(ordered);
    return pic->frame_count > 0 ? MM2_IMAGE32_OK : MM2_IMAGE32_ERR_BAD_FORMAT;
}

static mm2_image32_error parse_picture_scripts(const uint8_t *dec, size_t dec_len, int frame_count,
                                               mm2_pc_monster_picture *pic)
{
    size_t table_end;
    size_t *inner;
    size_t first;
    int have_first;
    int fi;
    size_t i;
    mm2_pc_monster_script_step *current = NULL;
    int current_count = 0;
    int current_cap = 0;
    mm2_pc_monster_script *scripts = NULL;
    int script_count = 0;
    int script_cap = 0;

    if (frame_count <= 0) {
        return MM2_IMAGE32_OK;
    }
    table_end = 2u + (size_t)frame_count * 2u;
    if (table_end > dec_len) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    inner = (size_t *)calloc((size_t)frame_count, sizeof(size_t));
    if (!inner) {
        return MM2_IMAGE32_ERR_NOMEM;
    }
    for (fi = 0; fi < frame_count; ++fi) {
        inner[fi] = read_u16_le(dec + 2 + fi * 2);
    }
    first = table_end;
    have_first = 0;
    for (fi = 0; fi < frame_count; ++fi) {
        if (inner[fi] > 0 && (!have_first || inner[fi] < first)) {
            first = inner[fi];
            have_first = 1;
        }
    }
    if (!have_first) {
        first = table_end;
    }
    if (first > dec_len) {
        first = dec_len;
    }
    if (table_end > first) {
        free(inner);
        return MM2_IMAGE32_OK;
    }

    i = table_end;
    while (i < first) {
        uint8_t b = dec[i];
        if (b == 0xFF) {
            if (i + 1 < first && dec[i + 1] == 0xFF) {
                if (current_count > 0) {
                    mm2_pc_monster_script *slot;
                    if (script_count >= script_cap) {
                        script_cap = script_cap ? script_cap * 2 : 4;
                        scripts = (mm2_pc_monster_script *)realloc(scripts, (size_t)script_cap * sizeof(*scripts));
                        if (!scripts) {
                            free(inner);
                            free(current);
                            for (fi = 0; fi < script_count; ++fi) {
                                free(scripts[fi].steps);
                            }
                            free(scripts);
                            return MM2_IMAGE32_ERR_NOMEM;
                        }
                    }
                    slot = &scripts[script_count++];
                    slot->steps = current;
                    slot->step_count = current_count;
                    current = NULL;
                    current_count = 0;
                    current_cap = 0;
                }
                break;
            }
            if (current_count > 0) {
                mm2_pc_monster_script *slot;
                if (script_count >= script_cap) {
                    script_cap = script_cap ? script_cap * 2 : 4;
                    scripts = (mm2_pc_monster_script *)realloc(scripts, (size_t)script_cap * sizeof(*scripts));
                    if (!scripts) {
                        free(inner);
                        free(current);
                        return MM2_IMAGE32_ERR_NOMEM;
                    }
                }
                slot = &scripts[script_count++];
                slot->steps = current;
                slot->step_count = current_count;
                current = NULL;
                current_count = 0;
                current_cap = 0;
            }
            ++i;
            continue;
        }
        if (i + 1 < first) {
            if (current_count >= current_cap) {
                current_cap = current_cap ? current_cap * 2 : 8;
                current = (mm2_pc_monster_script_step *)realloc(current, (size_t)current_cap * sizeof(*current));
                if (!current) {
                    free(inner);
                    for (fi = 0; fi < script_count; ++fi) {
                        free(scripts[fi].steps);
                    }
                    free(scripts);
                    return MM2_IMAGE32_ERR_NOMEM;
                }
            }
            current[current_count].frame = dec[i];
            current[current_count].delay = dec[i + 1];
            ++current_count;
            i += 2;
        } else {
            ++i;
        }
    }
    if (current_count > 0) {
        if (script_count >= script_cap) {
            script_cap = script_cap ? script_cap * 2 : 4;
            scripts = (mm2_pc_monster_script *)realloc(scripts, (size_t)script_cap * sizeof(*scripts));
            if (!scripts) {
                free(inner);
                free(current);
                return MM2_IMAGE32_ERR_NOMEM;
            }
        }
        scripts[script_count].steps = current;
        scripts[script_count].step_count = current_count;
        ++script_count;
    } else {
        free(current);
    }

    free(inner);
    pic->scripts = scripts;
    pic->script_count = script_count;
    return MM2_IMAGE32_OK;
}

void mm2_pc_monster_picture_free(mm2_pc_monster_picture *pic)
{
    int i;

    if (!pic) {
        return;
    }
    if (pic->frames) {
        for (i = 0; i < pic->frame_count; ++i) {
            free(pic->frames[i].stream);
        }
        free(pic->frames);
    }
    if (pic->scripts) {
        for (i = 0; i < pic->script_count; ++i) {
            free(pic->scripts[i].steps);
        }
        free(pic->scripts);
    }
    memset(pic, 0, sizeof(*pic));
}

mm2_image32_error mm2_pc_monsters_picture_load(const mm2_pc_monsters_atlas *atlas, int picture_id,
                                               mm2_pc_monster_picture *out)
{
    size_t fo;
    size_t dec_len = 0;
    uint8_t *dec;
    int frame_count;
    mm2_image32_error err;
    int used_id;

    if (!atlas || !out) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    mm2_pc_monster_picture_free(out);
    err = mm2_pc_monsters_resolve_picture_id(atlas, picture_id, &used_id);
    if (err != MM2_IMAGE32_OK) {
        return err;
    }
    fo = mm2_pc_monsters_blob_offset_for_id(atlas, used_id);
    if (fo == 0) {
        return MM2_IMAGE32_ERR_IO;
    }
    dec = decompress_monster_blob(atlas->raw, atlas->raw_len, fo, &dec_len);
    if (!dec || dec_len < 2) {
        free(dec);
        return MM2_IMAGE32_ERR_IO;
    }
    frame_count = dec[0] & 0x3F;
    if (frame_count <= 0) {
        free(dec);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    out->picture_id = used_id;
    out->bpp = atlas->bpp;
    out->flags = dec[1];
    err = parse_picture_frames(dec, dec_len, out);
    if (err != MM2_IMAGE32_OK) {
        mm2_pc_monster_picture_free(out);
        free(dec);
        return err;
    }
    err = parse_picture_scripts(dec, dec_len, frame_count, out);
    free(dec);
    if (err != MM2_IMAGE32_OK) {
        mm2_pc_monster_picture_free(out);
    }
    return err;
}

int mm2_pc_monsters_primary_script_index(const mm2_pc_monster_picture *pic)
{
    int best_si = -1;
    int best_len = 1;
    int si;

    if (!pic) {
        return -1;
    }
    for (si = 0; si < pic->script_count; ++si) {
        if (pic->scripts[si].step_count > best_len) {
            best_len = pic->scripts[si].step_count;
            best_si = si;
        }
    }
    if (best_si >= 0) {
        return best_si;
    }
    for (si = 0; si < pic->script_count; ++si) {
        if (pic->scripts[si].step_count > 1) {
            return si;
        }
    }
    return -1;
}

int mm2_pc_monsters_seq_frame_at(const mm2_pc_monster_picture *pic, int seq_index, int step)
{
    const mm2_pc_monster_script *seq;

    if (!pic || seq_index < 0 || seq_index >= pic->script_count) {
        return -1;
    }
    seq = &pic->scripts[seq_index];
    if (seq->step_count <= 0) {
        return -1;
    }
    if (step < 0) {
        step = 0;
    }
    if (step >= seq->step_count) {
        step = seq->step_count - 1;
    }
    return seq->steps[step].frame;
}

int mm2_pc_monsters_seq_delay_at(const mm2_pc_monster_picture *pic, int seq_index, int step)
{
    const mm2_pc_monster_script *seq;
    int delay;

    if (!pic || seq_index < 0 || seq_index >= pic->script_count) {
        return 1;
    }
    seq = &pic->scripts[seq_index];
    if (seq->step_count <= 0) {
        return 1;
    }
    if (step < 0) {
        step = 0;
    }
    if (step >= seq->step_count) {
        step = seq->step_count - 1;
    }
    delay = seq->steps[step].delay;
    return delay > 0 ? delay : 1;
}

static void decode_sprite_indices(int width, int height, const uint8_t *stream, size_t stream_len, int bpp,
                                  int *grid)
{
    int row = -1;
    int remaining = 0;
    int col = 0;
    int done = 0;
    size_t i = 0;
    int r;

    for (r = 0; r < width * height; ++r) {
        grid[r] = -1;
    }
    while (i < stream_len && !done) {
        uint8_t b = stream[i++];
        int count = (b >> 4) & 0xF;
        int low = b & 0xF;
        int transparent;
        int color;
        int rep;

        if (bpp == 2) {
            transparent = low >= 4;
            color = low & 3;
        } else {
            transparent = low == 5;
            color = k_ega_monster_xlat[low];
        }
        for (rep = 0; rep <= count; ++rep) {
            if (remaining == 0) {
                ++row;
                if (row >= height) {
                    done = 1;
                    break;
                }
                col = 0;
                remaining = width;
            }
            if (!transparent && col < width) {
                grid[row * width + col] = color;
            }
            ++col;
            --remaining;
        }
    }
}

static void clear_monster_rect(int *canvas, int x, int y, int w, int h)
{
    int r;
    int c;
    for (r = 0; r < h; ++r) {
        int yy = y + r;
        if (yy < 0 || yy >= MM2_PC_COMBAT_CANVAS_H) {
            continue;
        }
        for (c = 0; c < w; ++c) {
            int xx = x + c;
            if (xx < 0 || xx >= MM2_PC_COMBAT_CANVAS_W) {
                continue;
            }
            canvas[yy * MM2_PC_COMBAT_CANVAS_W + xx] = -1;
        }
    }
}

static void blit_monster_grid(int *canvas, const int *grid, int w, int h, int ox, int oy)
{
    int r;
    int c;
    for (r = 0; r < h; ++r) {
        int yy = oy + r;
        if (yy < 0 || yy >= MM2_PC_COMBAT_CANVAS_H) {
            continue;
        }
        for (c = 0; c < w; ++c) {
            int v = grid[r * w + c];
            int xx;
            if (v < 0) {
                continue;
            }
            xx = ox + c;
            if (xx < 0 || xx >= MM2_PC_COMBAT_CANVAS_W) {
                continue;
            }
            canvas[yy * MM2_PC_COMBAT_CANVAS_W + xx] = v;
        }
    }
}

static const mm2_pc_monster_frame *find_frame(const mm2_pc_monster_picture *pic, int frame_index)
{
    int i;
    for (i = 0; i < pic->frame_count; ++i) {
        if (pic->frames[i].frame_index == frame_index) {
            return &pic->frames[i];
        }
    }
    return NULL;
}

mm2_image32_error mm2_pc_monsters_composite_rgba(const mm2_pc_monster_picture *pic, int frame_idx, int cga_palette,
                                                 uint8_t *rgba_out)
{
    int canvas[MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H];
    int layers[2];
    int layer_count = 0;
    int li;
    size_t ci;
    uint8_t color[3];

    if (!pic || !rgba_out) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    for (ci = 0; ci < (size_t)(MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H); ++ci) {
        canvas[ci] = -1;
    }
    if (pic->frame_count <= 0) {
        memset(rgba_out, 0, (size_t)MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H * 4u);
        return MM2_IMAGE32_OK;
    }
    layers[layer_count++] = 0;
    if (frame_idx != 0 && find_frame(pic, frame_idx)) {
        layers[layer_count++] = frame_idx;
    }
    for (li = 0; li < layer_count; ++li) {
        const mm2_pc_monster_frame *fr = find_frame(pic, layers[li]);
        int *grid;
        if (!fr) {
            continue;
        }
        if (layers[li] != 0) {
            clear_monster_rect(canvas, fr->x, fr->y, fr->width, fr->height);
        }
        grid = (int *)malloc((size_t)fr->width * (size_t)fr->height * sizeof(int));
        if (!grid) {
            return MM2_IMAGE32_ERR_NOMEM;
        }
        decode_sprite_indices(fr->width, fr->height, fr->stream, fr->stream_len, pic->bpp, grid);
        blit_monster_grid(canvas, grid, fr->width, fr->height, fr->x, fr->y);
        free(grid);
    }
    for (ci = 0; ci < (size_t)(MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H); ++ci) {
        uint8_t *o = rgba_out + ci * 4u;
        int v = canvas[ci];
        if (v < 0) {
            o[0] = o[1] = o[2] = o[3] = 0;
            continue;
        }
        palette_color(v, pic->bpp, cga_palette, color);
        o[0] = color[0];
        o[1] = color[1];
        o[2] = color[2];
        o[3] = 255;
    }
    return MM2_IMAGE32_OK;
}

#if defined(MM2_CODEC_AMIGA)
static mm2_image32_error composite_canvas(const mm2_pc_monster_picture *pic, int frame_idx, int *canvas,
                                          uint8_t *alpha)
{
    int layers[2];
    int layer_count = 0;
    int li;
    const size_t count = (size_t)MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H;

    for (size_t ci = 0; ci < count; ++ci) {
        canvas[ci] = -1;
        alpha[ci] = 0;
    }
    if (pic->frame_count <= 0) {
        return MM2_IMAGE32_OK;
    }
    layers[layer_count++] = 0;
    if (frame_idx != 0 && find_frame(pic, frame_idx)) {
        layers[layer_count++] = frame_idx;
    }
    for (li = 0; li < layer_count; ++li) {
        const mm2_pc_monster_frame *fr = find_frame(pic, layers[li]);
        int *grid;
        int r;
        if (!fr) {
            continue;
        }
        if (layers[li] != 0) {
            clear_monster_rect(canvas, fr->x, fr->y, fr->width, fr->height);
        }
        grid = (int *)malloc((size_t)fr->width * (size_t)fr->height * sizeof(int));
        if (!grid) {
            return MM2_IMAGE32_ERR_NOMEM;
        }
        decode_sprite_indices(fr->width, fr->height, fr->stream, fr->stream_len, pic->bpp, grid);
        for (r = 0; r < fr->height; ++r) {
            int c;
            for (c = 0; c < fr->width; ++c) {
                int v = grid[r * fr->width + c];
                int yy = fr->y + r;
                int xx = fr->x + c;
                size_t ci;
                if (v < 0 || yy < 0 || yy >= MM2_PC_COMBAT_CANVAS_H || xx < 0 || xx >= MM2_PC_COMBAT_CANVAS_W) {
                    continue;
                }
                ci = (size_t)yy * MM2_PC_COMBAT_CANVAS_W + (size_t)xx;
                canvas[ci] = v;
                alpha[ci] = 255;
            }
        }
        free(grid);
    }
    return MM2_IMAGE32_OK;
}

mm2_image32_error mm2_pc_monsters_composite_planar(const mm2_pc_monster_picture *pic, int frame_idx, int cga_palette,
                                                   mm2_image32_frame *out)
{
    int canvas[MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H];
    uint8_t alpha[MM2_PC_COMBAT_CANVAS_W * MM2_PC_COMBAT_CANVAS_H];
    mm2_image32_error err;

    (void)cga_palette;
    if (!pic || !out) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    memset(out, 0, sizeof(*out));
    err = composite_canvas(pic, frame_idx, canvas, alpha);
    if (err != MM2_IMAGE32_OK) {
        return err;
    }
    return mm2_pc_gfx_indices_to_planar(canvas, alpha, MM2_PC_COMBAT_CANVAS_W, MM2_PC_COMBAT_CANVAS_H, out);
}
#endif
