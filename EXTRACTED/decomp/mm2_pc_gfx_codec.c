#include "mm2_pc_gfx_codec.h"

#include "mm2_codec_platform.h"

#include <stdio.h>

static int g_cga_palette = 1;

static const int k_lzw_mask[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1FF, 0x3FF, 0x7FF, 0xFFF};

static const uint8_t k_cga_palette_0[4][3] = {{0, 0, 0}, {0, 170, 0}, {170, 0, 0}, {170, 85, 0}};
static const uint8_t k_cga_palette_1[4][3] = {{0, 0, 0}, {85, 255, 255}, {255, 85, 255}, {255, 255, 255}};
static const uint8_t k_ega_rgb[16][3] = {
    {0, 0, 0},       {0, 0, 170},     {0, 170, 0},     {0, 170, 170},   {170, 0, 0},
    {170, 0, 170},   {170, 85, 0},    {170, 170, 170}, {85, 85, 85},    {85, 85, 255},
    {85, 255, 85},   {85, 255, 255},  {255, 85, 85},   {255, 85, 255},  {255, 255, 85},
    {255, 255, 255},
};

typedef struct pc_wall_frame {
    int index;
    int width;
    int height;
    uint8_t *pixels;
    size_t pixels_len;
} pc_wall_frame;

typedef struct pc_wall_blob {
    uint8_t *decompressed;
    size_t decompressed_len;
    int bpp;
    pc_wall_frame *frames;
    int frame_count;
} pc_wall_blob;

void mm2_pc_gfx_set_cga_palette(int palette)
{
    g_cga_palette = palette ? 1 : 0;
}

int mm2_pc_gfx_cga_palette(void)
{
    return g_cga_palette;
}

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int pc_bpp_for_ext(const char *path)
{
    const char *dot = path ? strrchr(path, '.') : NULL;
    if (!dot) {
        return 2;
    }
    if (dot[1] == '1' && dot[2] == '6' && dot[3] == '\0') {
        return 4;
    }
    return 2;
}

static int pc_row_bytes(int width, int bpp)
{
    return bpp == 2 ? (width + 3) / 4 : (width + 1) / 2;
}

static void pc_palette_color(int idx, int bpp, int cga_palette, uint8_t out[3])
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

static void pc_fill_sheet_palette(int bpp, int cga_palette, mm2_image32_file *img)
{
    int i;
    uint8_t rgb[3];
    for (i = 0; i < MM2_IMAGE32_PALETTE_COLORS; ++i) {
        if (bpp == 2 && i >= 4) {
            img->palette_rgba[i][0] = 0;
            img->palette_rgba[i][1] = 0;
            img->palette_rgba[i][2] = 0;
            img->palette_rgba[i][3] = 255;
#if defined(MM2_CODEC_AMIGA)
            img->palette_pen[i] = 0;
#endif
            continue;
        }
        if (bpp == 4 && i >= 16) {
            img->palette_rgba[i][0] = 0;
            img->palette_rgba[i][1] = 0;
            img->palette_rgba[i][2] = 0;
            img->palette_rgba[i][3] = 255;
#if defined(MM2_CODEC_AMIGA)
            img->palette_pen[i] = 0;
#endif
            continue;
        }
        pc_palette_color(i, bpp, cga_palette, rgb);
        img->palette_rgba[i][0] = rgb[0];
        img->palette_rgba[i][1] = rgb[1];
        img->palette_rgba[i][2] = rgb[2];
        img->palette_rgba[i][3] = 255;
#if defined(MM2_CODEC_AMIGA)
        img->palette_pen[i] = ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | (uint32_t)rgb[2];
#endif
    }
}

static void set_debug_label(mm2_image32_file *img, const char *path)
{
    const char *base = path;
    size_t n;
    if (!img || !path) {
        return;
    }
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    n = 0;
    while (base[n] && n < sizeof(img->debug_label) - 1u) {
        img->debug_label[n] = base[n];
        ++n;
    }
    img->debug_label[n] = '\0';
}

static int lzw_read_code(const uint8_t *source, size_t source_len, size_t *bits_read, int width)
{
    size_t base = *bits_read / 8;
    uint32_t word;
    if (base >= source_len) {
        return -1;
    }
    word = source[base];
    if (base + 1 < source_len) {
        word |= (uint32_t)source[base + 1] << 8;
    }
    if (base + 2 < source_len) {
        word |= (uint32_t)source[base + 2] << 16;
    }
    word >>= (*bits_read % 8);
    *bits_read += (size_t)width;
    return (int)(word & (uint32_t)k_lzw_mask[width]);
}

static int lzw_expand(const int *dict_root, const int *dict_code, int code, uint8_t *out_chars, int out_cap)
{
    uint8_t rev[4096];
    int rev_len = 0;
    int i;
    while (code > 0xFF) {
        if (rev_len >= (int)sizeof(rev)) {
            return -1;
        }
        rev[rev_len++] = (uint8_t)dict_root[code];
        code = dict_code[code];
    }
    if (rev_len >= (int)sizeof(rev)) {
        return -1;
    }
    rev[rev_len++] = (uint8_t)code;
    if (rev_len > out_cap) {
        return -1;
    }
    for (i = 0; i < rev_len; ++i) {
        out_chars[i] = rev[rev_len - 1 - i];
    }
    return rev_len;
}

static uint8_t *lzw_decompress(const uint8_t *source, size_t source_len, size_t dest_size, size_t *out_len)
{
    int dict_root[4096] = {0};
    int dict_code[4096] = {0};
    size_t bits_read = 0;
    uint8_t *out;
    size_t out_pos = 0;
    int code_width = 9;
    int dict_limit = 0x200;
    int dict_next = 0x102;
    int prev = -1;
    int have_prev = 0;

    *out_len = 0;
    out = (uint8_t *)malloc(dest_size);
    if (!out) {
        return NULL;
    }

    while (out_pos < dest_size) {
        int code = lzw_read_code(source, source_len, &bits_read, code_width);
        uint8_t chars[4096];
        int chars_len;
        int first;
        int i;

        if (code < 0) {
            break;
        }
        if (code == 0x100) {
            code_width = 9;
            dict_limit = 0x200;
            dict_next = 0x102;
            code = lzw_read_code(source, source_len, &bits_read, code_width);
            if (code < 0) {
                break;
            }
            out[out_pos++] = (uint8_t)(code & 0xFF);
            prev = code;
            have_prev = 1;
            continue;
        }
        if (code == 0x101) {
            break;
        }
        if (!have_prev) {
            break;
        }

        if (code < dict_next) {
            chars_len = lzw_expand(dict_root, dict_code, code, chars, (int)sizeof(chars));
        } else {
            chars_len = lzw_expand(dict_root, dict_code, prev, chars, (int)sizeof(chars));
            if (chars_len >= 0 && chars_len < (int)sizeof(chars)) {
                first = chars[0];
                chars[chars_len++] = (uint8_t)first;
            }
        }
        if (chars_len <= 0) {
            break;
        }
        first = chars[0];

        for (i = 0; i < chars_len && out_pos < dest_size; ++i) {
            out[out_pos++] = chars[i];
        }

        if (dict_next < 4096) {
            dict_root[dict_next] = first;
            dict_code[dict_next] = prev;
            ++dict_next;
            if (dict_next >= dict_limit && code_width < 12) {
                ++code_width;
                dict_limit <<= 1;
            }
        }
        prev = code;
        have_prev = 1;
    }

    *out_len = out_pos;
    return out;
}

uint8_t *mm2_pc_lzw_decompress(const uint8_t *source, size_t source_len, size_t dest_size, size_t *out_len)
{
    return lzw_decompress(source, source_len, dest_size, out_len);
}

static int valid_frame_dims(int width, int height, int bpp)
{
    if (width < 4 || height < 1) {
        return 0;
    }
    if (bpp == 2) {
        return width <= 320 && height <= 200;
    }
    return width <= 640 && height <= 200;
}

static size_t frame_end(const size_t *offsets, int offset_count, int index, size_t dec_len)
{
    size_t start = offsets[index];
    int i;
    for (i = index + 1; i < offset_count; ++i) {
        if (offsets[i] > start) {
            return offsets[i];
        }
    }
    return dec_len;
}

static size_t computed_frame_end(const uint8_t *dec, size_t dec_len, size_t off, int bpp, size_t end_limit)
{
    int width;
    int height;
    int rb;
    size_t pix_end;
    if (off + 4 > end_limit || off + 4 > dec_len) {
        return 0;
    }
    width = (int)read_u16_le(dec + off);
    height = (int)read_u16_le(dec + off + 2);
    if (!valid_frame_dims(width, height, bpp)) {
        return 0;
    }
    rb = pc_row_bytes(width, bpp);
    pix_end = off + 4 + (size_t)rb * (size_t)height;
    if (pix_end > end_limit || pix_end > dec_len) {
        return 0;
    }
    return pix_end;
}

static int is_grouped_u16_table(const uint8_t *dec, size_t dec_len, const size_t *offsets, int offset_count,
                                int bpp)
{
    int pairs = 0;
    int matches = 0;
    int i;
    if (offset_count < 4) {
        return 0;
    }
    for (i = 0; i + 1 < offset_count; i += 2) {
        size_t start = offsets[i];
        int width;
        int height;
        size_t end_limit;
        size_t expected;
        if (start < 2 || start + 4 > dec_len) {
            continue;
        }
        width = (int)read_u16_le(dec + start);
        height = (int)read_u16_le(dec + start + 2);
        if (!valid_frame_dims(width, height, bpp)) {
            continue;
        }
        end_limit = frame_end(offsets, offset_count, i, dec_len);
        expected = computed_frame_end(dec, dec_len, start, bpp, end_limit);
        if (expected <= start) {
            continue;
        }
        ++pairs;
        if (offsets[i + 1] == expected) {
            ++matches;
        }
    }
    return pairs >= 2 && matches >= (pairs * 2 / 3);
}

static pc_wall_frame *extract_grouped_u16_frames(const uint8_t *dec, size_t dec_len, const size_t *offsets,
                                               int offset_count, int bpp, int *out_count)
{
    pc_wall_frame *frames = NULL;
    int logical = 0;
    int i;
    *out_count = 0;
    for (i = 0; i + 1 < offset_count; i += 2) {
        size_t off = offsets[i];
        size_t end;
        int width;
        int height;
        int rb;
        size_t pix_len;
        size_t pix_off;
        pc_wall_frame fr;
        if (off < 2 || off >= dec_len) {
            continue;
        }
        end = (i + 1 < offset_count) ? offsets[i + 1] : dec_len;
        if (end <= off) {
            end = dec_len;
        }
        if (off + 4 > end) {
            continue;
        }
        width = (int)read_u16_le(dec + off);
        height = (int)read_u16_le(dec + off + 2);
        if (!valid_frame_dims(width, height, bpp)) {
            continue;
        }
        rb = pc_row_bytes(width, bpp);
        pix_len = (size_t)rb * (size_t)height;
        pix_off = off + 4;
        if (pix_off + pix_len > end) {
            continue;
        }
        fr.index = logical;
        fr.width = width;
        fr.height = height;
        fr.pixels_len = pix_len;
        fr.pixels = (uint8_t *)malloc(pix_len);
        if (!fr.pixels) {
            goto fail;
        }
        memcpy(fr.pixels, dec + pix_off, pix_len);
        frames = (pc_wall_frame *)realloc(frames, (size_t)(logical + 1) * sizeof(pc_wall_frame));
        if (!frames) {
            free(fr.pixels);
            goto fail;
        }
        frames[logical++] = fr;
    }
    *out_count = logical;
    return frames;
fail:
    if (frames) {
        int j;
        for (j = 0; j < logical; ++j) {
            free(frames[j].pixels);
        }
        free(frames);
    }
    *out_count = 0;
    return NULL;
}

static pc_wall_frame *extract_wall_frames(const uint8_t *dec, size_t dec_len, const size_t *offsets,
                                          int offset_count, int bpp, int *out_count)
{
    if (is_grouped_u16_table(dec, dec_len, offsets, offset_count, bpp)) {
        return extract_grouped_u16_frames(dec, dec_len, offsets, offset_count, bpp, out_count);
    }
    {
        pc_wall_frame *frames = NULL;
        int logical = 0;
        int i;
        *out_count = 0;
        for (i = 0; i < offset_count; ++i) {
            size_t off = offsets[i];
            size_t end;
            int width;
            int height;
            int rb;
            size_t pix_len;
            size_t pix_off;
            pc_wall_frame fr;
            if (off < 2 || off >= dec_len) {
                continue;
            }
            end = frame_end(offsets, offset_count, i, dec_len);
            if (off + 4 > end) {
                continue;
            }
            width = (int)read_u16_le(dec + off);
            height = (int)read_u16_le(dec + off + 2);
            if (!valid_frame_dims(width, height, bpp)) {
                continue;
            }
            rb = pc_row_bytes(width, bpp);
            pix_len = (size_t)rb * (size_t)height;
            pix_off = off + 4;
            if (pix_off + pix_len > end) {
                continue;
            }
            fr.index = logical;
            fr.width = width;
            fr.height = height;
            fr.pixels_len = pix_len;
            fr.pixels = (uint8_t *)malloc(pix_len);
            if (!fr.pixels) {
                goto fail;
            }
            memcpy(fr.pixels, dec + pix_off, pix_len);
            frames = (pc_wall_frame *)realloc(frames, (size_t)(logical + 1) * sizeof(pc_wall_frame));
            if (!frames) {
                free(fr.pixels);
                goto fail;
            }
            frames[logical++] = fr;
        }
        *out_count = logical;
        return frames;
fail:
        if (frames) {
            int j;
            for (j = 0; j < logical; ++j) {
                free(frames[j].pixels);
            }
            free(frames);
        }
        *out_count = 0;
        return NULL;
    }
}

static pc_wall_frame *extract_packed_u32_frames(const uint8_t *dec, size_t dec_len, int frame_count, int bpp,
                                                int *out_count)
{
    pc_wall_frame *frames = NULL;
    int logical = 0;
    int i;
    *out_count = 0;
    for (i = 0; i < frame_count; ++i) {
        size_t pos = 2 + (size_t)i * 4;
        uint32_t u32;
        size_t start;
        size_t end;
        int width;
        int height;
        int rb;
        size_t pix_len;
        size_t pix_off;
        pc_wall_frame fr;
        if (pos + 4 > dec_len) {
            break;
        }
        u32 = read_u32_le(dec + pos);
        start = u32 & 0xFFFFu;
        end = u32 >> 16;
        if (start < 2 || start >= dec_len) {
            continue;
        }
        if (end <= start) {
            end = dec_len;
        }
        if (start + 4 > end) {
            continue;
        }
        width = (int)read_u16_le(dec + start);
        height = (int)read_u16_le(dec + start + 2);
        if (!valid_frame_dims(width, height, bpp)) {
            continue;
        }
        rb = pc_row_bytes(width, bpp);
        pix_len = (size_t)rb * (size_t)height;
        pix_off = start + 4;
        if (pix_off + pix_len > end) {
            continue;
        }
        fr.index = logical;
        fr.width = width;
        fr.height = height;
        fr.pixels_len = pix_len;
        fr.pixels = (uint8_t *)malloc(pix_len);
        if (!fr.pixels) {
            goto fail;
        }
        memcpy(fr.pixels, dec + pix_off, pix_len);
        frames = (pc_wall_frame *)realloc(frames, (size_t)(logical + 1) * sizeof(pc_wall_frame));
        if (!frames) {
            free(fr.pixels);
            goto fail;
        }
        frames[logical++] = fr;
    }
    *out_count = logical;
    return frames;
fail:
    if (frames) {
        int j;
        for (j = 0; j < logical; ++j) {
            free(frames[j].pixels);
        }
        free(frames);
    }
    *out_count = 0;
    return NULL;
}

static void packed_u32_end_matches(const uint8_t *dec, size_t dec_len, int frame_count, int bpp, int *out_matches,
                                   int *out_valid)
{
    int matches = 0;
    int valid = 0;
    int i;
    *out_matches = 0;
    *out_valid = 0;
    for (i = 0; i < frame_count; ++i) {
        size_t pos = 2 + (size_t)i * 4;
        uint32_t u32;
        size_t start;
        size_t end;
        int width;
        int height;
        int rb;
        size_t expected;
        if (pos + 4 > dec_len) {
            break;
        }
        u32 = read_u32_le(dec + pos);
        start = u32 & 0xFFFFu;
        end = u32 >> 16;
        if (start < 2 || start + 4 > dec_len) {
            continue;
        }
        width = (int)read_u16_le(dec + start);
        height = (int)read_u16_le(dec + start + 2);
        if (!valid_frame_dims(width, height, bpp)) {
            continue;
        }
        rb = pc_row_bytes(width, bpp);
        expected = start + 4 + (size_t)rb * (size_t)height;
        if (expected > dec_len) {
            continue;
        }
        ++valid;
        if (end == expected || (end == 0 && expected <= dec_len)) {
            ++matches;
        }
    }
    *out_matches = matches;
    *out_valid = valid;
}

static void pick_offset_table(const uint8_t *dec, size_t dec_len, int frame_count, int bpp, int *out_is_u32,
                              int *out_packed_u32, size_t **out_offsets, int *out_offset_count)
{
    size_t *u16 = NULL;
    size_t *u32 = NULL;
    int best_score = -1;
    int best_bonus = -1;
    int best_is_u32 = 0;
    int best_packed = 0;
    size_t *best_table = NULL;
    int best_count = 0;
    int i;

    *out_is_u32 = 0;
    *out_packed_u32 = 0;
    *out_offsets = NULL;
    *out_offset_count = 0;
    if (frame_count <= 0) {
        return;
    }

    u16 = (size_t *)malloc((size_t)frame_count * sizeof(size_t));
    u32 = (size_t *)malloc((size_t)frame_count * sizeof(size_t));
    if (!u16 || !u32) {
        goto done;
    }
    if (2 + (size_t)frame_count * 2u <= dec_len) {
        for (i = 0; i < frame_count; ++i) {
            u16[i] = read_u16_le(dec + 2 + i * 2);
        }
    }
    if (2 + (size_t)frame_count * 4u <= dec_len) {
        for (i = 0; i < frame_count; ++i) {
            u32[i] = read_u32_le(dec + 2 + i * 4);
        }
    }

    if (2 + (size_t)frame_count * 4u <= dec_len) {
        int plain_count = 0;
        pc_wall_frame *plain = extract_wall_frames(dec, dec_len, u32, frame_count, bpp, &plain_count);
        int score = plain_count;
        int bonus = (plain_count == frame_count) ? 1 : 0;
        if (score > best_score || (score == best_score && bonus > best_bonus)) {
            best_score = score;
            best_bonus = bonus;
            best_is_u32 = 1;
            best_packed = 0;
            if (best_table) {
                free(best_table);
            }
            best_table = (size_t *)malloc((size_t)frame_count * sizeof(size_t));
            if (best_table) {
                memcpy(best_table, u32, (size_t)frame_count * sizeof(size_t));
                best_count = frame_count;
            }
        }
        if (plain) {
            for (i = 0; i < plain_count; ++i) {
                free(plain[i].pixels);
            }
            free(plain);
        }

        {
            int packed_count = 0;
            pc_wall_frame *packed = extract_packed_u32_frames(dec, dec_len, frame_count, bpp, &packed_count);
            if (packed) {
                int end_m = 0;
                int end_v = 0;
                score = packed_count;
                packed_u32_end_matches(dec, dec_len, frame_count, bpp, &end_m, &end_v);
                bonus = (end_v > 0 && end_m >= (end_v * 2 / 3) && end_m >= 2) ? 1 : 0;
                if (score > best_score || (score == best_score && bonus > best_bonus)) {
                    best_score = score;
                    best_bonus = bonus;
                    best_is_u32 = 1;
                    best_packed = 1;
                    if (best_table) {
                        free(best_table);
                    }
                    best_table = (size_t *)malloc((size_t)frame_count * sizeof(size_t));
                    if (best_table) {
                        memcpy(best_table, u32, (size_t)frame_count * sizeof(size_t));
                        best_count = frame_count;
                    }
                }
                for (i = 0; i < packed_count; ++i) {
                    free(packed[i].pixels);
                }
                free(packed);
            }
        }
    }

    if (2 + (size_t)frame_count * 2u <= dec_len) {
        int n16 = 0;
        pc_wall_frame *f16 = extract_wall_frames(dec, dec_len, u16, frame_count, bpp, &n16);
        int score = n16;
        int bonus = 0;
        if (score > best_score || (score == best_score && bonus > best_bonus)) {
            best_score = score;
            best_bonus = bonus;
            best_is_u32 = 0;
            best_packed = 0;
            if (best_table) {
                free(best_table);
            }
            best_table = (size_t *)malloc((size_t)frame_count * sizeof(size_t));
            if (best_table) {
                memcpy(best_table, u16, (size_t)frame_count * sizeof(size_t));
                best_count = frame_count;
            }
        }
        if (f16) {
            for (i = 0; i < n16; ++i) {
                free(f16[i].pixels);
            }
            free(f16);
        }
    }

    if (!best_table && u16) {
        best_table = u16;
        u16 = NULL;
        best_count = frame_count;
    }

done:
    *out_is_u32 = best_is_u32;
    *out_packed_u32 = best_packed;
    *out_offsets = best_table;
    *out_offset_count = best_count;
    free(u16);
    free(u32);
}

static void free_pc_wall_blob(pc_wall_blob *blob)
{
    int i;
    if (!blob) {
        return;
    }
    if (blob->frames) {
        for (i = 0; i < blob->frame_count; ++i) {
            free(blob->frames[i].pixels);
        }
        free(blob->frames);
    }
    free(blob->decompressed);
    memset(blob, 0, sizeof(*blob));
}

static mm2_image32_error parse_wall_blob(const uint8_t *raw, size_t raw_len, int bpp, pc_wall_blob *out)
{
    uint32_t dec_size;
    size_t dec_len = 0;
    int table_slot_count;
    size_t *offsets = NULL;
    int offset_count = 0;
    int is_u32 = 0;
    int packed_u32 = 0;

    memset(out, 0, sizeof(*out));
    if (raw_len < 8) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    dec_size = read_u32_le(raw);
    if (dec_size < 4) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    out->decompressed = lzw_decompress(raw + 4, raw_len - 4, dec_size, &dec_len);
    if (!out->decompressed || dec_len < dec_size) {
        free(out->decompressed);
        memset(out, 0, sizeof(*out));
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    out->decompressed_len = dec_len;
    out->bpp = bpp;
    table_slot_count = out->decompressed[0] & 0x3F;
    if (table_slot_count == 0) {
        free_pc_wall_blob(out);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    pick_offset_table(out->decompressed, out->decompressed_len, table_slot_count, bpp, &is_u32, &packed_u32,
                      &offsets, &offset_count);
    (void)is_u32;
    if (packed_u32) {
        out->frames = extract_packed_u32_frames(out->decompressed, out->decompressed_len, table_slot_count, bpp,
                                                &out->frame_count);
    } else if (offsets && offset_count > 0) {
        out->frames = extract_wall_frames(out->decompressed, out->decompressed_len, offsets, offset_count, bpp,
                                          &out->frame_count);
    }
    free(offsets);
    if (!out->frames || out->frame_count <= 0) {
        free_pc_wall_blob(out);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    return MM2_IMAGE32_OK;
}

static mm2_image32_error read_file_bytes(const char *path, uint8_t **out_data, size_t *out_len)
{
    FILE *f;
    long sz;
    size_t n;
    uint8_t *buf;

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
    *out_len = n;
    return MM2_IMAGE32_OK;
}

static int decode_wall_frame_indices(const pc_wall_frame *frame, int bpp, int *out_indices, int out_cap)
{
    int width = frame->width;
    int height = frame->height;
    int rb = pc_row_bytes(width, bpp);
    int y;
    int pos = 0;
    for (y = 0; y < height; ++y) {
        size_t row_off = (size_t)y * (size_t)rb;
        int x = 0;
        if (bpp == 2) {
            int bi;
            for (bi = 0; bi < rb && x < width; ++bi) {
                uint8_t b = frame->pixels[row_off + (size_t)bi];
                int shift;
                for (shift = 6; shift >= 0; shift -= 2) {
                    if (x >= width) {
                        break;
                    }
                    if (pos >= out_cap) {
                        return 0;
                    }
                    out_indices[pos++] = (b >> shift) & 3;
                    ++x;
                }
            }
        } else {
            int bi;
            for (bi = 0; bi < rb && x < width; ++bi) {
                uint8_t b = frame->pixels[row_off + (size_t)bi];
                int vals[2] = {(b >> 4) & 0xF, b & 0xF};
                int vi;
                for (vi = 0; vi < 2; ++vi) {
                    if (x >= width) {
                        break;
                    }
                    if (pos >= out_cap) {
                        return 0;
                    }
                    out_indices[pos++] = vals[vi];
                    ++x;
                }
            }
        }
    }
    return pos;
}

static int outdoor_overlay_void_key(int eidx, int cidx, int is_side)
{
    if (is_side) {
        return eidx == 8 && cidx < 2;
    }
    return eidx == 0 || (eidx == 8 && cidx < 2);
}

static int outdoor_wall_void_key(int eidx, int cidx)
{
    return eidx == 8 && cidx < 2;
}

static int overlay_cga_void(int cidx)
{
    return cidx == 0 || cidx == 1;
}

static int overlay_ega_void(int idx)
{
    return idx == 0 || idx == 1 || idx == 8 || idx == 9 || idx == 10 || idx == 11;
}

static int sky_void(int bpp, int idx, int cidx, int eidx, int use_ega0_rule)
{
    if (bpp == 4) {
        if (cidx >= 0) {
            return use_ega0_rule ? (idx == 0) : (cidx == 1);
        }
        return idx == 0 || idx == 1 || idx == 9 || idx == 10 || idx == 11;
    }
    if (eidx >= 0) {
        return use_ega0_rule ? (eidx == 0) : (idx == 1);
    }
    return idx == 0 || idx == 1;
}

static int wall_side_void(int bpp, int idx, int cidx, int eidx)
{
    if (bpp == 4) {
        if (cidx >= 0) {
            return idx == 8 && cidx < 2;
        }
        return idx == 8;
    }
    if (eidx >= 0) {
        return eidx == 8 && idx < 2;
    }
    return idx == 1;
}

static int frame_is_side(int frame_index)
{
    int base = frame_index & 0x0F;
    return base >= 4 && base <= 11;
}

static int compute_void_alpha(mm2_gfx_sheet_role role, int frame_index, int bpp, int idx, int cidx, int eidx,
                              int outdoor, int use_ega0_sky)
{
    switch (role) {
        case MM2_GFX_ROLE_SKY:
            return sky_void(bpp, idx, cidx, eidx, use_ega0_sky) ? 0 : 255;
        case MM2_GFX_ROLE_TORCH:
            if (cidx >= 0) {
                return overlay_cga_void(cidx) ? 0 : 255;
            }
            if (bpp == 2) {
                return overlay_cga_void(idx) ? 0 : 255;
            }
            return overlay_ega_void(idx) ? 0 : 255;
        case MM2_GFX_ROLE_OUTDOOR_BIOME:
            if (bpp == 4) {
                if (cidx >= 0) {
                    return outdoor_wall_void_key(idx, cidx) ? 0 : 255;
                }
                return idx == 8 ? 0 : 255;
            }
            if (eidx >= 0) {
                return outdoor_wall_void_key(eidx, idx) ? 0 : 255;
            }
            return idx == 1 ? 0 : 255;
        case MM2_GFX_ROLE_OUTDOOR_HORIZON: {
            int is_side = frame_is_side(frame_index);
            if (bpp == 4) {
                if (cidx >= 0) {
                    return outdoor_overlay_void_key(idx, cidx, is_side) ? 0 : 255;
                }
                if (is_side) {
                    return idx == 8 ? 0 : 255;
                }
                return idx == 0 ? 0 : 255;
            }
            if (eidx >= 0) {
                return outdoor_overlay_void_key(eidx, idx, is_side) ? 0 : 255;
            }
            if (is_side) {
                return idx == 1 ? 0 : 255;
            }
            return idx == 0 ? 0 : 255;
        }
        case MM2_GFX_ROLE_WALL:
        default:
            if (role == MM2_GFX_ROLE_TITLE) {
                if (bpp == 2) {
                    return idx == 0 ? 0 : 255;
                }
                return (idx == 0 || idx == 8) ? 0 : 255;
            }
            if (outdoor) {
                if (bpp == 4) {
                    if (cidx >= 0) {
                        return outdoor_wall_void_key(idx, cidx) ? 0 : 255;
                    }
                    return idx == 8 ? 0 : 255;
                }
                if (eidx >= 0) {
                    return outdoor_wall_void_key(eidx, idx) ? 0 : 255;
                }
                return idx == 1 ? 0 : 255;
            }
            if (frame_is_side(frame_index)) {
                return wall_side_void(bpp, idx, cidx, eidx) ? 0 : 255;
            }
            return 255;
    }
}

static int silhouette_has_any_zero(const int *sil_indices, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (sil_indices[i] == 0) {
            return 1;
        }
    }
    return 0;
}

#if defined(MM2_CODEC_AMIGA)
#include <ace/managers/blit.h>
#include <ace/managers/memory.h>
#include <ace/managers/system.h>
#include <ace/utils/bitmap.h>

static void destroy_bitmap(tBitMap *bm)
{
    UBYTE pl;
    ULONG plane_bytes;
    if (!bm) {
        return;
    }
    blitWait();
    plane_bytes = (ULONG)bm->BytesPerRow * (ULONG)bm->Rows;
    for (pl = 0; pl < bm->Depth; ++pl) {
        if (bm->Planes[pl]) {
            memFree(bm->Planes[pl], plane_bytes);
        }
    }
    memFree(bm, sizeof(tBitMap));
}

static mm2_image32_error build_void_mask(tBitMap *bm, uint16_t w, uint16_t h, const uint8_t *alpha,
                                         tBitMap **out_mask)
{
    tBitMap *msk;
    UWORD uwBmW = (UWORD)(((ULONG)w + 15UL) & ~15UL);
    UWORD y;
    UWORD x;
    if (!bm || !alpha || !out_mask) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    msk = bitmapCreate(uwBmW, h, 1, 0);
    if (!msk) {
        return MM2_IMAGE32_ERR_NOMEM;
    }
    for (y = 0; y < h; ++y) {
        UBYTE *row = (UBYTE *)msk->Planes[0] + (ULONG)y * (ULONG)msk->BytesPerRow;
        for (x = 0; x < w; ++x) {
            const uint8_t a = alpha[(size_t)y * w + x];
            const int bit = 7 - (x & 7);
            if (a) {
                row[x >> 3] |= (UBYTE)(1u << bit);
            } else {
                row[x >> 3] &= (UBYTE)~(1u << bit);
            }
        }
    }
    *out_mask = msk;
    return MM2_IMAGE32_OK;
}

static mm2_image32_error indices_to_planar_frame(const mm2_image32_file *img, const int *indices, const uint8_t *alpha,
                                                 uint16_t w, uint16_t h, mm2_image32_frame *frame)
{
    const UWORD uwBmW = (UWORD)(((ULONG)w + 15UL) & ~15UL);
    tBitMap *bm;
    UWORD y;
    UWORD x;
    UBYTE pl;
    mm2_image32_error err;

    frame->width = w;
    frame->height = h;
    frame->rgba = NULL;
    frame->rgba_size = 0;
    frame->bitmap = bitmapCreate(uwBmW, h, MM2_AGA_SCREEN_BPP, 0);
    if (!frame->bitmap) {
        return MM2_IMAGE32_ERR_NOMEM;
    }
    bm = (tBitMap *)frame->bitmap;
    for (pl = 0; pl < MM2_AGA_SCREEN_BPP; ++pl) {
        if (bm->Planes[pl]) {
            memset(bm->Planes[pl], 0, (size_t)bm->Rows * (size_t)bm->BytesPerRow);
        }
    }
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            const size_t i = (size_t)y * w + x;
            int idx = indices[i];
            int pl;
            if (!alpha[i]) {
                continue;
            }
            for (pl = 0; pl < MM2_AGA_SCREEN_BPP; ++pl) {
                UBYTE *row = (UBYTE *)bm->Planes[pl] + (ULONG)y * (ULONG)bm->BytesPerRow;
                if ((idx >> pl) & 1) {
                    row[x >> 3] |= (UBYTE)(1u << (7 - (x & 7)));
                }
            }
        }
    }
    (void)img;
    frame->mask = NULL;
    err = build_void_mask(bm, w, h, alpha, (tBitMap **)&frame->mask);
    if (err != MM2_IMAGE32_OK) {
        destroy_bitmap(bm);
        frame->bitmap = NULL;
        return err;
    }
    return MM2_IMAGE32_OK;
}

mm2_image32_error mm2_pc_gfx_indices_to_planar(const int *indices, const uint8_t *alpha, uint16_t w, uint16_t h,
                                               mm2_image32_frame *out)
{
    return indices_to_planar_frame(NULL, indices, alpha, w, h, out);
}

void mm2_pc_gfx_planar_frame_free(mm2_image32_frame *frame)
{
    if (!frame) {
        return;
    }
    if (frame->bitmap) {
        destroy_bitmap((tBitMap *)frame->bitmap);
        frame->bitmap = NULL;
    }
    if (frame->mask) {
        destroy_bitmap((tBitMap *)frame->mask);
        frame->mask = NULL;
    }
    frame->width = 0;
    frame->height = 0;
}
#endif

static mm2_image32_error frame_to_rgba(const mm2_image32_file *img, const pc_wall_frame *frame, int frame_index,
                                       mm2_gfx_sheet_role role, int bpp, int cga_palette,
                                       const int *sil_indices, int sil_bpp, int outdoor,
                                       mm2_image32_frame *out)
{
    int count = frame->width * frame->height;
    int *indices = (int *)malloc((size_t)count * sizeof(int));
    int *sil = NULL;
    int use_ega0_sky = 0;
    int i;
    uint8_t *rgba;
    size_t rgba_size;
    mm2_image32_error err = MM2_IMAGE32_OK;

    if (!indices) {
        return MM2_IMAGE32_ERR_NOMEM;
    }
    if (!decode_wall_frame_indices(frame, bpp, indices, count)) {
        free(indices);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    if (sil_indices && sil_bpp > 0) {
        sil = (int *)malloc((size_t)count * sizeof(int));
        if (!sil) {
            free(indices);
            return MM2_IMAGE32_ERR_NOMEM;
        }
        memcpy(sil, sil_indices, (size_t)count * sizeof(int));
        if (role == MM2_GFX_ROLE_SKY) {
            if (bpp == 4) {
                use_ega0_sky = silhouette_has_any_zero(indices, count);
            } else {
                use_ega0_sky = silhouette_has_any_zero(sil, count);
            }
        }
    }

#if defined(MM2_CODEC_AMIGA)
    {
        uint8_t *alpha = (uint8_t *)malloc((size_t)count);
        if (!alpha) {
            free(indices);
            free(sil);
            return MM2_IMAGE32_ERR_NOMEM;
        }
        for (i = 0; i < count; ++i) {
            int cidx = -1;
            int eidx = -1;
            if (sil) {
                if (bpp == 4) {
                    cidx = sil[i];
                } else {
                    eidx = sil[i];
                }
            }
            if (role == MM2_GFX_ROLE_SKY) {
                alpha[i] = (uint8_t)(sky_void(bpp, indices[i], cidx, eidx, use_ega0_sky) ? 0 : 255);
            } else {
                alpha[i] = (uint8_t)compute_void_alpha(role, frame_index, bpp, indices[i], cidx, eidx, outdoor,
                                                      use_ega0_sky);
            }
        }
        err = indices_to_planar_frame(img, indices, alpha, (uint16_t)frame->width, (uint16_t)frame->height, out);
        free(alpha);
        free(indices);
        free(sil);
        return err;
    }
#else
    rgba_size = (size_t)count * 4u;
    rgba = (uint8_t *)malloc(rgba_size);
    if (!rgba) {
        free(indices);
        free(sil);
        return MM2_IMAGE32_ERR_NOMEM;
    }
    for (i = 0; i < count; ++i) {
        int cidx = -1;
        int eidx = -1;
        uint8_t rgb[3];
        uint8_t *o = rgba + (size_t)i * 4u;
        if (sil) {
            if (bpp == 4) {
                cidx = sil[i];
            } else {
                eidx = sil[i];
            }
        }
        pc_palette_color(indices[i], bpp, cga_palette, rgb);
        o[0] = rgb[0];
        o[1] = rgb[1];
        o[2] = rgb[2];
        if (role == MM2_GFX_ROLE_SKY) {
            o[3] = (uint8_t)(sky_void(bpp, indices[i], cidx, eidx, use_ega0_sky) ? 0 : 255);
        } else {
            o[3] = (uint8_t)compute_void_alpha(role, frame_index, bpp, indices[i], cidx, eidx, outdoor,
                                               use_ega0_sky);
        }
    }
    out->width = (uint16_t)frame->width;
    out->height = (uint16_t)frame->height;
    out->rgba = rgba;
    out->rgba_size = rgba_size;
    free(indices);
    free(sil);
    return MM2_IMAGE32_OK;
#endif
}

static mm2_image32_error decode_blob_to_sheet(const pc_wall_blob *blob, const pc_wall_blob *sil_blob,
                                              mm2_gfx_sheet_role role, int cga_palette, mm2_gfx_sheet *out)
{
    mm2_image32_file *img = &out->img;
    int outdoor = (role == MM2_GFX_ROLE_OUTDOOR_HORIZON || role == MM2_GFX_ROLE_OUTDOOR_BIOME) ? 1 : 0;
    int i;

    memset(out, 0, sizeof(*out));
    out->role = (uint8_t)role;
    out->backend = (uint8_t)(blob->bpp == 2 ? MM2_GFX_BACKEND_PC_CGA : MM2_GFX_BACKEND_PC_EGA);
    img->frame_count = (uint16_t)blob->frame_count;
    img->frames = (mm2_image32_frame *)calloc((size_t)blob->frame_count, sizeof(mm2_image32_frame));
    if (!img->frames) {
        return MM2_IMAGE32_ERR_NOMEM;
    }
    pc_fill_sheet_palette(blob->bpp, cga_palette, img);

    for (i = 0; i < blob->frame_count; ++i) {
        const pc_wall_frame *fr = &blob->frames[i];
        int sil_count = 0;
        int *sil_indices = NULL;
        mm2_image32_error err;
        if (sil_blob && i < sil_blob->frame_count) {
            sil_count = sil_blob->frames[i].width * sil_blob->frames[i].height;
            sil_indices = (int *)malloc((size_t)sil_count * sizeof(int));
            if (sil_indices &&
                decode_wall_frame_indices(&sil_blob->frames[i], sil_blob->bpp, sil_indices, sil_count)) {
                /* ok */
            } else {
                free(sil_indices);
                sil_indices = NULL;
            }
        }
        err = frame_to_rgba(img, fr, fr->index, role, blob->bpp, cga_palette, sil_indices,
                              sil_blob ? sil_blob->bpp : 0, outdoor, &img->frames[i]);
        free(sil_indices);
        if (err != MM2_IMAGE32_OK) {
            mm2_gfx_sheet_free(out);
            return err;
        }
    }
    return MM2_IMAGE32_OK;
}

mm2_image32_error mm2_pc_wall_sheet_load(const char *path, mm2_gfx_sheet_role role, const char *silhouette_path,
                                         mm2_gfx_sheet *out)
{
    uint8_t *raw = NULL;
    size_t raw_len = 0;
    pc_wall_blob blob = {};
    pc_wall_blob sil = {};
    mm2_image32_error err;
    int bpp;

    if (!path || !out) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    mm2_gfx_sheet_free(out);
    err = read_file_bytes(path, &raw, &raw_len);
    if (err != MM2_IMAGE32_OK) {
        return err;
    }
    bpp = pc_bpp_for_ext(path);
    err = parse_wall_blob(raw, raw_len, bpp, &blob);
    free(raw);
    if (err != MM2_IMAGE32_OK) {
        return err;
    }
    if (silhouette_path) {
        uint8_t *sil_raw = NULL;
        size_t sil_len = 0;
        if (read_file_bytes(silhouette_path, &sil_raw, &sil_len) == MM2_IMAGE32_OK) {
            if (parse_wall_blob(sil_raw, sil_len, pc_bpp_for_ext(silhouette_path), &sil) != MM2_IMAGE32_OK) {
                free_pc_wall_blob(&sil);
            }
            free(sil_raw);
        }
    }
    err = decode_blob_to_sheet(&blob, sil.frame_count > 0 ? &sil : NULL, role, mm2_pc_gfx_cga_palette(), out);
    set_debug_label(&out->img, path);
    free_pc_wall_blob(&blob);
    free_pc_wall_blob(&sil);
    return err;
}

mm2_image32_error mm2_pc_wall_frame_packed_pixels(const char *path, int frame_index, uint8_t **out_pixels,
                                                  size_t *out_len, uint16_t *out_w, uint16_t *out_h)
{
    uint8_t *raw = NULL;
    size_t raw_len = 0;
    pc_wall_blob blob = {};
    mm2_image32_error err;
    pc_wall_frame *fr;

    if (out_pixels) {
        *out_pixels = NULL;
    }
    if (out_len) {
        *out_len = 0;
    }
    if (out_w) {
        *out_w = 0;
    }
    if (out_h) {
        *out_h = 0;
    }
    if (!path || frame_index < 0 || !out_pixels || !out_len || !out_w || !out_h) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }

    err = read_file_bytes(path, &raw, &raw_len);
    if (err != MM2_IMAGE32_OK) {
        return err;
    }
    err = parse_wall_blob(raw, raw_len, pc_bpp_for_ext(path), &blob);
    free(raw);
    if (err != MM2_IMAGE32_OK) {
        return err;
    }
    if (frame_index >= blob.frame_count) {
        free_pc_wall_blob(&blob);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    fr = &blob.frames[frame_index];
    *out_w = (uint16_t)fr->width;
    *out_h = (uint16_t)fr->height;
    *out_len = fr->pixels_len;
    *out_pixels = (uint8_t *)malloc(fr->pixels_len);
    if (!*out_pixels) {
        free_pc_wall_blob(&blob);
        return MM2_IMAGE32_ERR_NOMEM;
    }
    memcpy(*out_pixels, fr->pixels, fr->pixels_len);
    free_pc_wall_blob(&blob);
    return MM2_IMAGE32_OK;
}
