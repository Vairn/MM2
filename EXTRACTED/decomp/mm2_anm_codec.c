#include "mm2_anm_codec.h"

#include "mm2_codec_platform.h"

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void write_be16(FILE *fp, uint16_t v) {
    uint8_t b[2];
    b[0] = (uint8_t)((v >> 8) & 0xFF);
    b[1] = (uint8_t)(v & 0xFF);
    fwrite(b, 1, 2, fp);
}

size_t mm2_anm_rassize(uint16_t width, uint16_t height) {
    return (size_t)height * (size_t)((((uint32_t)width + 15U) >> 3U) & 0xFFFEU);
}

static int decode_stream(const uint8_t *src, size_t src_size, size_t *src_off, uint8_t *dst, size_t dst_size) {
    size_t off = *src_off;
    size_t out = 0;
    int have_hi = 0;
    uint8_t hi = 0;

    while (out < dst_size) {
        uint8_t p;
        uint8_t cmd;
        uint8_t nib;
        int times;
        int t;

        if (off >= src_size) {
            return 0;
        }
        p = src[off++];
        cmd = p & 0xF0;

        if (cmd == 0x00 || cmd == 0xF0) {
            nib = (uint8_t)((p >> 4) & 0xF);
            times = (p & 0x0F) + 1;
            for (t = 0; t < times; t++) {
                if (!have_hi) {
                    hi = nib;
                    have_hi = 1;
                } else {
                    dst[out++] = (uint8_t)((hi << 4) | nib);
                    have_hi = 0;
                    if (out >= dst_size) {
                        break;
                    }
                }
            }
        } else {
            uint8_t n0 = (uint8_t)((p >> 4) & 0xF);
            uint8_t n1 = (uint8_t)(p & 0xF);
            if (!have_hi) {
                hi = n0;
                have_hi = 1;
            } else {
                dst[out++] = (uint8_t)((hi << 4) | n0);
                have_hi = 0;
            }
            if (out < dst_size) {
                if (!have_hi) {
                    hi = n1;
                    have_hi = 1;
                } else {
                    dst[out++] = (uint8_t)((hi << 4) | n1);
                    have_hi = 0;
                }
            }
        }
    }

    *src_off = off;
    return 1;
}

static int encode_stream(const uint8_t *src, size_t src_size, uint8_t **out_buf, size_t *out_size) {
    size_t cap = src_size * 2 + 64;
    uint8_t *buf = (uint8_t *)malloc(cap);
    size_t out = 0;
    size_t i = 0;
    size_t nib_count = src_size * 2;

    if (!buf) {
        return 0;
    }

    while (i < nib_count) {
        uint8_t nib;
        if ((i & 1) == 0) {
            nib = (uint8_t)((src[i >> 1] >> 4) & 0xF);
        } else {
            nib = (uint8_t)(src[i >> 1] & 0xF);
        }

        if (nib == 0 || nib == 0xF) {
            size_t run = 1;
            while (i + run < nib_count && run < 16) {
                uint8_t n2;
                size_t j = i + run;
                if ((j & 1) == 0) {
                    n2 = (uint8_t)((src[j >> 1] >> 4) & 0xF);
                } else {
                    n2 = (uint8_t)(src[j >> 1] & 0xF);
                }
                if (n2 != nib) {
                    break;
                }
                run++;
            }
            buf[out++] = (uint8_t)((nib << 4) | ((run - 1) & 0xF));
            i += run;
        } else {
            uint8_t n1;
            if (i + 1 >= nib_count) {
                free(buf);
                return 0;
            }
            if (((i + 1) & 1) == 0) {
                n1 = (uint8_t)((src[(i + 1) >> 1] >> 4) & 0xF);
            } else {
                n1 = (uint8_t)(src[(i + 1) >> 1] & 0xF);
            }
            buf[out++] = (uint8_t)((nib << 4) | n1);
            i += 2;
        }
    }

    *out_buf = buf;
    *out_size = out;
    return 1;
}

static int find_image_chunk(const uint8_t *buf, size_t size, size_t *hdr_off, size_t *ff_off) {
    size_t i;
    for (i = 0x33; i + 10 < size; i++) {
        uint16_t frames;
        uint16_t depth;
        uint16_t width;
        uint16_t height;
        if (buf[i] != 0xFF || buf[i + 1] != 0x00) {
            continue;
        }
        frames = read_be16(buf + i + 1);
        depth = read_be16(buf + i + 3);
        width = read_be16(buf + i + 5);
        height = read_be16(buf + i + 7);
        if (frames > 0 && frames < 256 && depth > 0 && depth < 64 && width > 0 && height > 0) {
            *ff_off = i;
            *hdr_off = i + 1;
            return 1;
        }
    }
    return 0;
}

void mm2_anm_free(mm2_anm_file *anm) {
    uint16_t i;
    if (!anm) {
        return;
    }
    if (anm->frames) {
        for (i = 0; i < anm->frame_count; i++) {
            free(anm->frames[i].planes);
        }
        free(anm->frames);
    }
    free(anm->sequence_blob);
    memset(anm, 0, sizeof(*anm));
}

mm2_anm_error mm2_anm_load_file(const char *path, mm2_anm_file *out) {
    FILE *fp = NULL;
    uint8_t *buf = NULL;
    size_t fsize;
    size_t got;
    size_t hdr_off = 0;
    size_t ff_off = 0;
    size_t i;
    size_t off;
    mm2_anm_file anm;

    if (!out) {
        return MM2_ANM_ERR_BAD_FORMAT;
    }
    memset(&anm, 0, sizeof(anm));

#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
    systemUse();
#endif
    fp = fopen(path, "rb");
    if (!fp) {
#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
        systemUnuse();
#endif
        return MM2_ANM_ERR_IO;
    }
    fseek(fp, 0, SEEK_END);
    fsize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize < 64) {
        fclose(fp);
#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
        systemUnuse();
#endif
        return MM2_ANM_ERR_BAD_FORMAT;
    }

    buf = (uint8_t *)malloc(fsize);
    if (!buf) {
        fclose(fp);
#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
        systemUnuse();
#endif
        return MM2_ANM_ERR_NOMEM;
    }
    got = fread(buf, 1, fsize, fp);
    fclose(fp);
#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
    systemUnuse();
#endif
    if (got != fsize) {
        free(buf);
        return MM2_ANM_ERR_IO;
    }

    memcpy(anm.magic, buf, 4);
    if (!(anm.magic[0] == 0x00 && anm.magic[1] == 0x00 && anm.magic[2] == 0x54 && anm.magic[3] == 0x56)) {
        free(buf);
        return MM2_ANM_ERR_BAD_MAGIC;
    }

    for (i = 0; i < MM2_ANM_PRELUDE_SLOTS; i++) {
        size_t p = 4 + i * 4;
        if (buf[p] == 0xFF && buf[p + 1] == 0xFF && buf[p + 2] == 0xFF && buf[p + 3] == 0xFF) {
            continue;
        }
        anm.prelude[i].is_used = 1;
        anm.prelude[i].x_offset = buf[p];
        anm.prelude[i].y_offset = buf[p + 1];
        anm.prelude[i].w = buf[p + 2];
        anm.prelude[i].h = buf[p + 3];
    }

    anm.seq_header_a = buf[0x30];
    anm.seq_header_b = buf[0x31];
    anm.seq_header_c = buf[0x32];

    if (!find_image_chunk(buf, fsize, &hdr_off, &ff_off)) {
        free(buf);
        return MM2_ANM_ERR_BAD_FORMAT;
    }

    if (ff_off > 0x33) {
        anm.sequence_blob_size = ff_off - 0x33;
        anm.sequence_blob = (uint8_t *)malloc(anm.sequence_blob_size);
        if (!anm.sequence_blob) {
            free(buf);
            return MM2_ANM_ERR_NOMEM;
        }
        memcpy(anm.sequence_blob, buf + 0x33, anm.sequence_blob_size);
    }

    anm.frame_count = read_be16(buf + hdr_off + 0);
    anm.depth_or_mode = read_be16(buf + hdr_off + 2);
    anm.frames = (mm2_anm_frame_info *)calloc(anm.frame_count, sizeof(mm2_anm_frame_info));
    if (!anm.frames) {
        free(buf);
        mm2_anm_free(&anm);
        return MM2_ANM_ERR_NOMEM;
    }

    off = hdr_off + 4;
    for (i = 0; i < anm.frame_count; i++) {
        anm.frames[i].width = read_be16(buf + off + 0);
        anm.frames[i].height = read_be16(buf + off + 2);
        anm.frames[i].flags = read_be16(buf + off + 4);
        off += 6;
    }
    for (i = 0; i < MM2_ANM_PALETTE_COLORS; i++) {
        anm.palette_words[i] = read_be16(buf + off);
        off += 2;
    }

    for (i = 0; i < anm.frame_count; i++) {
        size_t need = mm2_anm_rassize(anm.frames[i].width, anm.frames[i].height) * MM2_ANM_PLANES;
        anm.frames[i].planes_size = need;
        anm.frames[i].planes = (uint8_t *)malloc(need);
        if (!anm.frames[i].planes) {
            free(buf);
            mm2_anm_free(&anm);
            return MM2_ANM_ERR_NOMEM;
        }
        if (!decode_stream(buf, fsize, &off, anm.frames[i].planes, need)) {
            free(buf);
            mm2_anm_free(&anm);
            return MM2_ANM_ERR_BAD_FORMAT;
        }
    }

    free(buf);
    *out = anm;
    return MM2_ANM_OK;
}

mm2_anm_error mm2_anm_save_file(const char *path, const mm2_anm_file *anm) {
    FILE *fp;
    uint16_t i;

    if (!anm) {
        return MM2_ANM_ERR_BAD_FORMAT;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        return MM2_ANM_ERR_IO;
    }

    fwrite(anm->magic, 1, 4, fp);
    for (i = 0; i < MM2_ANM_PRELUDE_SLOTS; i++) {
        if (anm->prelude[i].is_used) {
            uint8_t e[4];
            e[0] = anm->prelude[i].x_offset;
            e[1] = anm->prelude[i].y_offset;
            e[2] = anm->prelude[i].w;
            e[3] = anm->prelude[i].h;
            fwrite(e, 1, 4, fp);
        } else {
            uint8_t ff[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            fwrite(ff, 1, 4, fp);
        }
    }

    fputc(anm->seq_header_a, fp);
    fputc(anm->seq_header_b, fp);
    fputc(anm->seq_header_c, fp);
    if (anm->sequence_blob && anm->sequence_blob_size) {
        fwrite(anm->sequence_blob, 1, anm->sequence_blob_size, fp);
    }

    fputc(0xFF, fp);
    write_be16(fp, anm->frame_count);
    write_be16(fp, anm->depth_or_mode);
    for (i = 0; i < anm->frame_count; i++) {
        write_be16(fp, anm->frames[i].width);
        write_be16(fp, anm->frames[i].height);
        write_be16(fp, anm->frames[i].flags);
    }
    for (i = 0; i < MM2_ANM_PALETTE_COLORS; i++) {
        write_be16(fp, anm->palette_words[i]);
    }
    for (i = 0; i < anm->frame_count; i++) {
        uint8_t *enc = NULL;
        size_t enc_sz = 0;
        if (!encode_stream(anm->frames[i].planes, anm->frames[i].planes_size, &enc, &enc_sz)) {
            fclose(fp);
            return MM2_ANM_ERR_BAD_FORMAT;
        }
        fwrite(enc, 1, enc_sz, fp);
        free(enc);
    }

    fclose(fp);
    return MM2_ANM_OK;
}

int mm2_anm_build_sequence_table(const mm2_anm_file *anm, mm2_anm_sequence_table *out)
{
    if (!anm || !out) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (!anm->sequence_blob || anm->sequence_blob_size == 0) {
        return 1;
    }

    size_t cur = 0;
    const size_t seq_end = anm->sequence_blob_size;
    while (cur < seq_end && out->block_count < MM2_ANM_MAX_SEQUENCE_BLOCKS) {
        while (cur < seq_end && anm->sequence_blob[cur] != 0xFF) {
            ++cur;
        }
        if (cur >= seq_end) {
            break;
        }
        ++cur; /* consume delimiter */
        const size_t begin = cur;
        while (cur < seq_end && anm->sequence_blob[cur] != 0xFF) {
            ++cur;
        }
        if (begin >= cur) {
            continue;
        }
        mm2_anm_sequence_block *block = &out->blocks[out->block_count++];
        const size_t byte_count = cur - begin;
        const int pair_count = (int)(byte_count / 2);
        block->pair_count = pair_count;
        if (pair_count > MM2_ANM_MAX_SEQUENCE_PAIRS) {
            block->pair_count = MM2_ANM_MAX_SEQUENCE_PAIRS;
        }
        memcpy(block->frame_delay, anm->sequence_blob + begin,
               (size_t)block->pair_count * 2u);
    }
    return 1;
}

int mm2_anm_seq_block_pair_count(const mm2_anm_sequence_table *tbl, int block_idx)
{
    if (!tbl || block_idx < 0 || block_idx >= tbl->block_count) {
        return 0;
    }
    return tbl->blocks[block_idx].pair_count;
}

int mm2_anm_seq_frame_at(const mm2_anm_sequence_table *tbl, int block_idx, int step)
{
    if (!tbl || block_idx < 0 || block_idx >= tbl->block_count) {
        return 0;
    }
    const mm2_anm_sequence_block *block = &tbl->blocks[block_idx];
    if (block->pair_count <= 0) {
        return 0;
    }
    if (step < 0) {
        step = 0;
    }
    if (step >= block->pair_count) {
        step = block->pair_count - 1;
    }
    return (int)block->frame_delay[(size_t)step * 2u];
}

int mm2_anm_seq_delay_at(const mm2_anm_sequence_table *tbl, int block_idx, int step)
{
    if (!tbl || block_idx < 0 || block_idx >= tbl->block_count) {
        return 1;
    }
    const mm2_anm_sequence_block *block = &tbl->blocks[block_idx];
    if (block->pair_count <= 0) {
        return 1;
    }
    if (step < 0) {
        step = 0;
    }
    if (step >= block->pair_count) {
        step = block->pair_count - 1;
    }
    const int delay = (int)block->frame_delay[(size_t)step * 2u + 1u];
    return delay > 0 ? delay : 1;
}

int mm2_anm_has_animatable_frames(const mm2_anm_file *anm)
{
    if (!anm) {
        return 0;
    }
    if (anm->frame_count > 1) {
        return 1;
    }
    mm2_anm_sequence_table tbl;
    if (!mm2_anm_build_sequence_table(anm, &tbl)) {
        return 0;
    }
    for (int i = 0; i < tbl.block_count; ++i) {
        if (tbl.blocks[i].pair_count > 1) {
            return 1;
        }
    }
    return 0;
}

