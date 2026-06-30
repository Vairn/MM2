#ifndef MM2_MAP_CODEC_H
#define MM2_MAP_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * map.dat - 60 map screens * 512 bytes = 30720 bytes.
 * Loaded flat into runtime memory at the pointer stored in A4-$EEF4
 * (MM2_GS_MAP_BLOB). See EXTRACTED/docs/21-map-dat-format.md.
 *
 * Per screen (512 bytes, 16x16 grid per page; disk row 0 = SOUTH/bottom):
 *   +0x000  256 bytes  page 0 - VISUAL  (3D hood source, map_row_sampler @ 0x190C)
 *   +0x100  256 bytes  page 1 - COLLISION (movement / darkness / events)
 *
 * Page 0 - visual byte: four 2-bit wall fields per cell. NO event bit.
 *   bits 0-1 North | 2-3 East | 4-5 South | 6-7 West
 *   field values: 0 open, 1 wall, 2 wall+torch, 3 door
 * (Outdoor surface screens reuse the low 5 bits as a terrain id for outb.32
 *  auto-map; high bits are overland flags.)
 *
 * Page 1 - collision byte: same direction slots, each (dark<<1)|wall,
 * except West's dark slot is reused as the tile EVENT flag:
 *   bit0 0x01 N wall | bit1 0x02 N dark
 *   bit2 0x04 E wall | bit3 0x08 E dark
 *   bit4 0x10 S wall | bit5 0x20 S dark
 *   bit6 0x40 W wall | bit7 0x80 EVENT flag (aligns with event.dat triplets)
 */

enum {
    MM2_MAP_SCREEN_COUNT = 60,
    MM2_MAP_GRID_DIM = 16,
    MM2_MAP_PAGE_SIZE = 256,  /* 16x16 cells */
    MM2_MAP_SCREEN_SIZE = 512,
    MM2_MAP_FILE_SIZE = MM2_MAP_SCREEN_COUNT * MM2_MAP_SCREEN_SIZE
};

/* Page-0 visual 2-bit field values. */
enum {
    MM2_MAP_WALL_OPEN = 0,
    MM2_MAP_WALL_SOLID = 1,
    MM2_MAP_WALL_DOOR = 2,
    MM2_MAP_WALL_TORCH = 3
};

/* Page-1 collision bit masks. */
enum {
    MM2_MAP_COLL_N_WALL = 0x01,
    MM2_MAP_COLL_N_DARK = 0x02,
    MM2_MAP_COLL_E_WALL = 0x04,
    MM2_MAP_COLL_E_DARK = 0x08,
    MM2_MAP_COLL_S_WALL = 0x10,
    MM2_MAP_COLL_S_DARK = 0x20,
    MM2_MAP_COLL_W_WALL = 0x40,
    MM2_MAP_COLL_EVENT = 0x80,    /* event flag, NOT west-dark */
    /* Wall bits only (N/E/S/W low bit per direction); passability @ 0x9424 AND #$55. */
    MM2_MAP_COLL_PASS_WALL_MASK = 0x55,
    /* Wall+dark bits with event stripped — cartography / display, not movement. */
    MM2_MAP_COLL_WALL_MASK = 0x7F
};

typedef struct Mm2MapScreen {
    uint8_t visual[MM2_MAP_PAGE_SIZE];    /* page 0 */
    uint8_t collision[MM2_MAP_PAGE_SIZE]; /* page 1 */
} Mm2MapScreen;

typedef struct Mm2MapFile {
    Mm2MapScreen screens[MM2_MAP_SCREEN_COUNT];
} Mm2MapFile;

typedef enum Mm2MapError {
    MM2_MAP_OK = 0,
    MM2_MAP_ERR_IO = 1,
    MM2_MAP_ERR_BAD_ARGS = 2,
    MM2_MAP_ERR_BAD_SIZE = 3
} Mm2MapError;

Mm2MapError mm2_map_decode(const uint8_t *bytes, size_t bytes_size, Mm2MapFile *out);
Mm2MapError mm2_map_encode(const Mm2MapFile *map, uint8_t *out_bytes, size_t out_size);
Mm2MapError mm2_map_load_file(const char *path, Mm2MapFile *out);
Mm2MapError mm2_map_save_file(const char *path, const Mm2MapFile *map);

/* Cell accessors; x = column 0..15, y = disk row 0..15 (row 0 = south). */
static inline uint8_t mm2_map_visual_at(const Mm2MapScreen *s, int x, int y)
{
    return s->visual[(size_t)(y * MM2_MAP_GRID_DIM + x)];
}

static inline uint8_t mm2_map_collision_at(const Mm2MapScreen *s, int x, int y)
{
    return s->collision[(size_t)(y * MM2_MAP_GRID_DIM + x)];
}

/* Page-0 visual 2-bit wall fields (N/E/S/W @ bits 0-1/2-3/4-5/6-7). */
static inline int mm2_map_visual_wall_n(uint8_t cell) { return cell & 3; }
static inline int mm2_map_visual_wall_e(uint8_t cell) { return (cell >> 2) & 3; }
static inline int mm2_map_visual_wall_s(uint8_t cell) { return (cell >> 4) & 3; }
static inline int mm2_map_visual_wall_w(uint8_t cell) { return (cell >> 6) & 3; }

/* Page-1 collision event flag (0x80, collision page only). */
static inline int mm2_map_collision_has_event(uint8_t cell)
{
    return (cell & MM2_MAP_COLL_EVENT) != 0;
}

/* Page-1 per-direction wall (low bit) and dark (high bit) for N/E/S/W. */
static inline int mm2_map_coll_wall_n(uint8_t cell) { return cell & 0x01; }
static inline int mm2_map_coll_wall_e(uint8_t cell) { return (cell >> 2) & 1; }
static inline int mm2_map_coll_wall_s(uint8_t cell) { return (cell >> 4) & 1; }
static inline int mm2_map_coll_wall_w(uint8_t cell) { return (cell >> 6) & 1; }
static inline int mm2_map_coll_dark_n(uint8_t cell) { return (cell >> 1) & 1; }
static inline int mm2_map_coll_dark_e(uint8_t cell) { return (cell >> 3) & 1; }
static inline int mm2_map_coll_dark_s(uint8_t cell) { return (cell >> 5) & 1; }

/* map_facing_from_key @ 0x5636: bundle-hi mask + shift for passability @ 0x9424. */
static inline uint8_t mm2_map_facing_mask_hi(char facing_key)
{
    switch (facing_key) {
    case 'N': return 0xC0;
    case 'E': return 0x30;
    case 'S': return 0x0C;
    case 'W': return 0x03;
    default: return 0;
    }
}

static inline uint8_t mm2_map_facing_shift(char facing_key)
{
    switch (facing_key) {
    case 'N': return 6;
    case 'E': return 4;
    case 'S': return 2;
    case 'W': return 0;
    default: return 0;
    }
}

/* facing_to_delta @ 0x5692 (party key 'N'/'E'/'S'/'W'). */
static inline void mm2_map_facing_delta(char facing_key, int8_t *dx, int8_t *dy)
{
    if (dx) {
        *dx = 0;
    }
    if (dy) {
        *dy = 0;
    }
    switch (facing_key) {
    case 'E':
        if (dx) {
            *dx = 1;
        }
        break;
    case 'W':
        if (dx) {
            *dx = -1;
        }
        break;
    case 'N':
        if (dy) {
            *dy = 1;
        }
        break;
    case 'S':
        if (dy) {
            *dy = -1;
        }
        break;
    default:
        break;
    }
}

/*
 * Interior passability first gate @ 0x9424: current collision cell
 * (A4-$55D6) AND facing bundle (A4-$55D8) AND #$55 (wall bits only).
 * Darkness high bits do not block; they drain light @ 0x69DC after a step.
 * Returns 0 if passable (-1 in original), 1 if blocked.
 * GAP: full 0x9424 path (doors, swim, barriers) not traced yet.
 */
static inline int mm2_map_passability_blocked(uint8_t collision_cell, char facing_key)
{
    const uint8_t mask = mm2_map_facing_mask_hi(facing_key);
    const uint8_t walls = (uint8_t)(collision_cell & mask & MM2_MAP_COLL_PASS_WALL_MASK);
    return walls != 0;
}

/* Tile darkness for time-tick @ 0x69DC: bit 5 of A4-$55D6 after a step. */
static inline int mm2_map_collision_is_dark(uint8_t collision_cell)
{
    return (collision_cell & MM2_MAP_COLL_S_DARK) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* MM2_MAP_CODEC_H */
