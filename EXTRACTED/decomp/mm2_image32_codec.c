#include "mm2_image32_codec.h"

#include "mm2_codec_platform.h"

static int g_preview_opaque_mask = 0;

void mm2_image32_set_preview_opaque(int enabled)
{
    g_preview_opaque_mask = enabled ? 1 : 0;
}

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

size_t mm2_image32_rassize(uint16_t width, uint16_t height)
{
    return (size_t)height * (size_t)((((width) + 15u) >> 3) & 0xFFFEu);
}

static int plausible_header(const uint8_t *d, size_t size, size_t off)
{
    size_t info_off;
    uint16_t frames;
    uint16_t w;
    uint16_t h;

    if (off + 12 > size) {
        return 0;
    }
    frames = read_be16(d + off);
    if (frames < 1 || frames > 1024) {
        return 0;
    }
    info_off = off + 4;
    if (info_off + (size_t)frames * 6u + 64u > size) {
        return 0;
    }
    w = read_be16(d + info_off);
    h = read_be16(d + info_off + 2);
    return w > 0 && w <= 1024 && h > 0 && h <= 1024;
}

static size_t find_image_chunk(const uint8_t *d, size_t size, size_t from)
{
    size_t i;

    if (size < 11) {
        return (size_t)-1;
    }
    for (i = from; i + 10 < size; ++i) {
        size_t hdr;
        uint16_t frames;
        uint16_t depth;
        uint16_t w;
        uint16_t h;

        if (d[i] != 0xFF || d[i + 1] != 0x00) {
            continue;
        }
        hdr = i + 1;
        frames = read_be16(d + hdr);
        depth = read_be16(d + hdr + 2);
        w = read_be16(d + hdr + 4);
        h = read_be16(d + hdr + 6);
        if (frames > 0 && frames < 256 && depth < 64 && w > 0 && w <= 1024 && h > 0 && h <= 1024) {
            return hdr;
        }
    }
    return (size_t)-1;
}

static void emit_nib(uint8_t nib, int *have_pending, uint8_t *pending, uint8_t *out, size_t *out_pos,
                     size_t out_bytes)
{
    if (*out_pos >= out_bytes) {
        return;
    }
    if (!*have_pending) {
        *pending = nib;
        *have_pending = 1;
    } else {
        out[*out_pos] = (uint8_t)((*pending << 4) | nib);
        ++(*out_pos);
        *have_pending = 0;
    }
}

static size_t decode_plane_stream(const uint8_t *data, size_t size, size_t off, size_t out_bytes,
                                  uint8_t *out)
{
    size_t out_pos = 0;
    int have_pending = 0;
    uint8_t pending = 0;

    while (out_pos < out_bytes && off < size) {
        uint8_t p = data[off++];
        uint8_t cmd = (uint8_t)(p & 0xF0u);

        if (cmd == 0x00 || cmd == 0xF0u) {
            uint8_t nib = (uint8_t)((p >> 4) & 0x0Fu);
            int times = (p & 0x0F) + 1;
            int t;

            for (t = 0; t < times && out_pos < out_bytes; ++t) {
                emit_nib(nib, &have_pending, &pending, out, &out_pos, out_bytes);
            }
        } else {
            emit_nib((uint8_t)((p >> 4) & 0x0Fu), &have_pending, &pending, out, &out_pos, out_bytes);
            emit_nib((uint8_t)(p & 0x0Fu), &have_pending, &pending, out, &out_pos, out_bytes);
        }
    }

    if (out_pos != out_bytes) {
        return (size_t)-1;
    }
    return off;
}

static mm2_image32_error decode_palette(const uint8_t *data, size_t pal_off, mm2_image32_file *img)
{
    int i;

    for (i = 0; i < MM2_IMAGE32_PALETTE_COLORS; ++i) {
        uint16_t w = read_be16(data + pal_off + (size_t)i * 2u);
        const uint8_t r = (uint8_t)(((w >> 8) & 0x0Fu) * 17u);
        const uint8_t g = (uint8_t)(((w >> 4) & 0x0Fu) * 17u);
        const uint8_t b = (uint8_t)((w & 0x0Fu) * 17u);
        img->palette_rgba[i][0] = r;
        img->palette_rgba[i][1] = g;
        img->palette_rgba[i][2] = b;
        img->palette_rgba[i][3] = g_preview_opaque_mask ? 255u : ((i == 0) ? 0u : 255u);
#if defined(MM2_CODEC_AMIGA)
        img->palette_pen[i] =
            ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
#endif
    }
    return MM2_IMAGE32_OK;
}

#if defined(MM2_CODEC_AMIGA)

#include <ace/utils/bitmap.h>

static mm2_image32_error planar_frame(const uint8_t *planes, uint16_t w, uint16_t h,
                                      mm2_image32_frame *frame)
{
    size_t rs = mm2_image32_rassize(w, h);
    UBYTE pl;

    frame->width = w;
    frame->height = h;
    frame->rgba = NULL;
    frame->rgba_size = 0;
    frame->bitmap = bitmapCreate(w, h, MM2_IMAGE32_PLANES, BMF_CLEAR | BMF_DISPLAYABLE);
    if (!frame->bitmap) {
        return MM2_IMAGE32_ERR_NOMEM;
    }
    {
        tBitMap *bm = (tBitMap *)frame->bitmap;
        for (pl = 0; pl < MM2_IMAGE32_PLANES; ++pl) {
            memcpy(bm->Planes[pl], planes + (size_t)pl * rs, rs);
        }
    }
    return MM2_IMAGE32_OK;
}

#endif

static mm2_image32_error rasterize_frame(const mm2_image32_file *img, const uint8_t *planes,
                                         uint16_t w, uint16_t h, mm2_image32_frame *frame)
{
#if defined(MM2_CODEC_AMIGA)
    (void)img;
    return planar_frame(planes, w, h, frame);
#else
    size_t rs = mm2_image32_rassize(w, h);
    int bpr = (int)((((w) + 15u) >> 3) & 0xFFFEu);
    size_t rgba_size = (size_t)w * (size_t)h * 4u;
    int y;

    frame->width = w;
    frame->height = h;
    frame->rgba_size = rgba_size;
    frame->rgba = (uint8_t *)malloc(rgba_size);
    if (!frame->rgba) {
        return MM2_IMAGE32_ERR_NOMEM;
    }
    memset(frame->rgba, 0, rgba_size);

    for (y = 0; y < (int)h; ++y) {
        int x;
        for (x = 0; x < (int)w; ++x) {
            int idx = 0;
            int pl;
            uint8_t *o;

            for (pl = 0; pl < MM2_IMAGE32_PLANES; ++pl) {
                size_t byte_pos = (size_t)pl * rs + (size_t)y * (size_t)bpr + (size_t)(x >> 3);
                int bit = (planes[byte_pos] >> (7 - (x & 7))) & 1;
                idx |= bit << pl;
            }
            o = frame->rgba + ((size_t)y * (size_t)w + (size_t)x) * 4u;
            o[0] = img->palette_rgba[idx][0];
            o[1] = img->palette_rgba[idx][1];
            o[2] = img->palette_rgba[idx][2];
            o[3] = g_preview_opaque_mask ? 255u : ((idx == 0) ? 0u : 255u);
        }
    }
    return MM2_IMAGE32_OK;
#endif
}

void mm2_image32_free(mm2_image32_file *img)
{
    size_t i;

    if (!img) {
        return;
    }
    if (img->frames) {
        for (i = 0; i < (size_t)img->frame_count; ++i) {
#if defined(MM2_CODEC_AMIGA)
            if (img->frames[i].bitmap) {
                bitmapDestroy((tBitMap *)img->frames[i].bitmap);
                img->frames[i].bitmap = NULL;
            }
#endif
            free(img->frames[i].rgba);
        }
        free(img->frames);
    }
    memset(img, 0, sizeof(*img));
}

mm2_image32_error mm2_image32_decode_buffer(const uint8_t *data, size_t size, mm2_image32_file *out)
{
    size_t off = 0;
    size_t info_off;
    size_t pal_off;
    size_t cur;
    size_t i;
    mm2_image32_error err;

    if (!data || !out || size < 12) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    memset(out, 0, sizeof(*out));

    if (!plausible_header(data, size, 0)) {
        off = find_image_chunk(data, size, 0);
        if (off == (size_t)-1) {
            return MM2_IMAGE32_ERR_BAD_FORMAT;
        }
    }

    out->frame_count = read_be16(data + off);
    out->depth_or_mode = read_be16(data + off + 2);
    if (out->frame_count == 0 || out->frame_count > 1024) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }

    info_off = off + 4;
    if (info_off + (size_t)out->frame_count * 6u + 64u > size) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }

    out->frames = (mm2_image32_frame *)calloc(out->frame_count, sizeof(mm2_image32_frame));
    if (!out->frames) {
        return MM2_IMAGE32_ERR_NOMEM;
    }

    pal_off = info_off + (size_t)out->frame_count * 6u;
    err = decode_palette(data, pal_off, out);
    if (err != MM2_IMAGE32_OK) {
        mm2_image32_free(out);
        return err;
    }

    cur = pal_off + 64u;
    for (i = 0; i < (size_t)out->frame_count; ++i) {
        uint16_t w = read_be16(data + info_off + i * 6u);
        uint16_t h = read_be16(data + info_off + i * 6u + 2u);
        size_t rs;
        size_t frame_bytes;
        uint8_t *planes;
        size_t next;

        out->frames[i].flags = read_be16(data + info_off + i * 6u + 4u);
        if (w == 0 || h == 0 || w > 1024 || h > 1024) {
            mm2_image32_free(out);
            return MM2_IMAGE32_ERR_BAD_FORMAT;
        }

        rs = mm2_image32_rassize(w, h);
        frame_bytes = (size_t)MM2_IMAGE32_PLANES * rs;
        planes = (uint8_t *)malloc(frame_bytes);
        if (!planes) {
            mm2_image32_free(out);
            return MM2_IMAGE32_ERR_NOMEM;
        }

        next = decode_plane_stream(data, size, cur, frame_bytes, planes);
        if (next == (size_t)-1) {
            free(planes);
            mm2_image32_free(out);
            return MM2_IMAGE32_ERR_BAD_FORMAT;
        }
        cur = next;

        err = rasterize_frame(out, planes, w, h, &out->frames[i]);
        free(planes);
        if (err != MM2_IMAGE32_OK) {
            mm2_image32_free(out);
            return err;
        }
    }
    return MM2_IMAGE32_OK;
}

mm2_image32_error mm2_image32_load_file(const char *path, mm2_image32_file *out)
{
    FILE *fp;
    long fsize_l;
    size_t fsize;
    size_t got;
    uint8_t *buf;
    mm2_image32_error err;

    if (!path || !out) {
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return MM2_IMAGE32_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return MM2_IMAGE32_ERR_IO;
    }
    fsize_l = ftell(fp);
    if (fsize_l <= 0) {
        fclose(fp);
        return MM2_IMAGE32_ERR_BAD_FORMAT;
    }
    fsize = (size_t)fsize_l;
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return MM2_IMAGE32_ERR_IO;
    }

    buf = (uint8_t *)malloc(fsize);
    if (!buf) {
        fclose(fp);
        return MM2_IMAGE32_ERR_NOMEM;
    }
    got = fread(buf, 1, fsize, fp);
    fclose(fp);
    if (got != fsize) {
        free(buf);
        return MM2_IMAGE32_ERR_IO;
    }

    err = mm2_image32_decode_buffer(buf, fsize, out);
    free(buf);
    return err;
}
