#ifndef MM2_PC_MONSTERS_CODEC_H
#define MM2_PC_MONSTERS_CODEC_H

#include "mm2_image32_codec.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MM2_PC_COMBAT_CANVAS_W = 96,
    MM2_PC_COMBAT_CANVAS_H = 96,
    MM2_PC_MONSTERS_MAX_PICTURE_ID = 75
};

typedef struct mm2_pc_monster_frame {
    int frame_index;
    int x;
    int y;
    int width;
    int height;
    uint8_t *stream;
    size_t stream_len;
} mm2_pc_monster_frame;

typedef struct mm2_pc_monster_script_step {
    int frame;
    int delay;
} mm2_pc_monster_script_step;

typedef struct mm2_pc_monster_script {
    mm2_pc_monster_script_step *steps;
    int step_count;
} mm2_pc_monster_script;

typedef struct mm2_pc_monster_picture {
    int picture_id;
    int bpp;
    int flags;
    mm2_pc_monster_frame *frames;
    int frame_count;
    mm2_pc_monster_script *scripts;
    int script_count;
} mm2_pc_monster_picture;

typedef struct mm2_pc_monsters_atlas {
    uint8_t *raw;
    size_t raw_len;
    int bpp;
    size_t header_bytes;
} mm2_pc_monsters_atlas;

void mm2_pc_monsters_atlas_free(mm2_pc_monsters_atlas *atlas);
void mm2_pc_monster_picture_free(mm2_pc_monster_picture *pic);

mm2_image32_error mm2_pc_monsters_atlas_load(const char *path, mm2_pc_monsters_atlas *out);

/** File offset for picture id (1-based), or 0 if absent/invalid. */
size_t mm2_pc_monsters_blob_offset_for_id(const mm2_pc_monsters_atlas *atlas, int picture_id);

/**
 * Retail DOS fallback: advance to next non-empty picture when slot is missing.
 * Returns MM2_IMAGE32_OK and sets *out_used_id on success.
 */
mm2_image32_error mm2_pc_monsters_resolve_picture_id(const mm2_pc_monsters_atlas *atlas, int picture_id,
                                                     int *out_used_id);

mm2_image32_error mm2_pc_monsters_picture_load(const mm2_pc_monsters_atlas *atlas, int picture_id,
                                               mm2_pc_monster_picture *out);

/** Longest script with more than one step; else first multi-step; else -1. */
int mm2_pc_monsters_primary_script_index(const mm2_pc_monster_picture *pic);

int mm2_pc_monsters_seq_frame_at(const mm2_pc_monster_picture *pic, int seq_index, int step);
int mm2_pc_monsters_seq_delay_at(const mm2_pc_monster_picture *pic, int seq_index, int step);

/**
 * Composite combat frame onto 96×96 RGBA8 (caller provides MM2_PC_COMBAT_CANVAS_W*H*4 bytes).
 */
mm2_image32_error mm2_pc_monsters_composite_rgba(const mm2_pc_monster_picture *pic, int frame_idx,
                                                 int cga_palette, uint8_t *rgba_out);

#if defined(MM2_CODEC_AMIGA)
mm2_image32_error mm2_pc_monsters_composite_planar(const mm2_pc_monster_picture *pic, int frame_idx, int cga_palette,
                                                   mm2_image32_frame *out);
#endif

#ifdef __cplusplus
}
#endif

#endif
