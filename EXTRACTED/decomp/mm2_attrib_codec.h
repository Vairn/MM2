#ifndef MM2_ATTRIB_CODEC_H
#define MM2_ATTRIB_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * attrib.dat - per-screen map attribute table.
 *
 * Confirmed facts:
 * - File is 3840 bytes = 60 screens * 64 bytes; record indexed by area * 64.
 * - Bytes used by legacy loaders (b3dmm2 mm2ed.bb / main.bb):
 *     +0x03 env type, +0x04 outside flag, +0x15 outside label,
 *     +0x20..0x3F roof bitmap (256 bits, one per 16x16 tile).
 * - Bytes 0x05..0x08 are a 4-way world-adjacency table (neighbour screen ids),
 *   verified fully symmetric across all 60 records; interior screens point all
 *   four neighbours at themselves.
 *
 * The record is kept as raw[64] so encode/decode are byte-exact and the still
 * unresolved fields round-trip untouched. Named accessors expose the confirmed
 * fields. See EXTRACTED/docs/12-attrib-dat-format.md.
 */
enum {
    MM2_ATTRIB_RECORD_SIZE = 0x40,
    MM2_ATTRIB_RECORD_COUNT = 60,
    MM2_ATTRIB_FILE_SIZE = MM2_ATTRIB_RECORD_SIZE * MM2_ATTRIB_RECORD_COUNT,
    MM2_ATTRIB_ROOF_OFFSET = 0x20,
    MM2_ATTRIB_ROOF_BYTES = 32,
    MM2_ATTRIB_TILE_COUNT = 256
};

/* Confirmed field offsets within a 64-byte record. */
enum {
    MM2_ATTRIB_OFF_AREA_ID = 0x00,
    MM2_ATTRIB_OFF_MAP_CATEGORY = 0x01,
    MM2_ATTRIB_OFF_TILESET = 0x02,
    MM2_ATTRIB_OFF_ENV_TYPE = 0x03,
    MM2_ATTRIB_OFF_SURFACE_FLAG = 0x04,
    MM2_ATTRIB_OFF_NEIGHBOR0 = 0x05, /* opposite of NEIGHBOR2 */
    MM2_ATTRIB_OFF_NEIGHBOR1 = 0x06, /* opposite of NEIGHBOR3 */
    MM2_ATTRIB_OFF_NEIGHBOR2 = 0x07,
    MM2_ATTRIB_OFF_NEIGHBOR3 = 0x08,
    MM2_ATTRIB_OFF_ENTRY_COORD = 0x0E,  /* packed (Y<<4)|X party entry pos */
    MM2_ATTRIB_OFF_ERA_GATE = 0x0F,     /* compared vs current era index */
    MM2_ATTRIB_OFF_COMPLEX_ID = 0x15,   /* big-endian word (interior) */
    MM2_ATTRIB_OFF_TRANSITION_COORD = 0x16, /* packed (Y<<4)|X dest pos */
    MM2_ATTRIB_OFF_LEVEL = 0x17,        /* floor/level (interior) */
    MM2_ATTRIB_OFF_LINK_AREA = 0x18,    /* a.k.a. transition_screen */
    MM2_ATTRIB_OFF_FLAGS = 0x1A,        /* btst bitfield (bits 0,3,4,5,6) */
    MM2_ATTRIB_OFF_DOOR_STRENGTH = 0x12, /* materialized A4-$5608; bash @ 0x9C2A */
    MM2_ATTRIB_OFF_DOOR_TRAP = 0x13     /* materialized A4-$5607; unlock @ 0x20D6E */
};

/* attrib byte 0x1A flag bits (asm btst sites). */
enum {
    MM2_ATTRIB_FLAG_BIT0 = 0x01, /* 0xBCCA */
    MM2_ATTRIB_FLAG_BIT3 = 0x08, /* 0xBB98 */
    MM2_ATTRIB_FLAG_BIT4 = 0x10, /* 0xADE8 */
    MM2_ATTRIB_FLAG_BIT5 = 0x20, /* 0xB006 */
    MM2_ATTRIB_FLAG_BIT6 = 0x40, /* 0xB2C2: gates the 0x18 transition */
    MM2_ATTRIB_FLAG_BIT7 = 0x80  /* set on caverns/underground screens */
};

/* env_type values (record +0x03). */
enum {
    MM2_ENV_TOWN = 0x11,
    MM2_ENV_CAVERN = 0x12,
    MM2_ENV_CASTLE_A = 0x13,
    MM2_ENV_CASTLE_B = 0x14
};

enum {
    MM2_ELEMENTAL_PLANE_FIRST = 41,
    MM2_ELEMENTAL_PLANE_LAST = 44
};

static inline int mm2_is_elemental_plane(int area_id)
{
    return area_id >= MM2_ELEMENTAL_PLANE_FIRST && area_id <= MM2_ELEMENTAL_PLANE_LAST;
}

static inline int mm2_is_outdoor_area(int area_id, uint8_t surface_flag)
{
    return mm2_is_elemental_plane(area_id) || surface_flag != 0;
}

typedef struct Mm2AttribRecord {
    uint8_t raw[MM2_ATTRIB_RECORD_SIZE];
} Mm2AttribRecord;

typedef struct Mm2AttribFile {
    Mm2AttribRecord records[MM2_ATTRIB_RECORD_COUNT];
} Mm2AttribFile;

typedef enum Mm2AttribError {
    MM2_ATTRIB_OK = 0,
    MM2_ATTRIB_ERR_IO = 1,
    MM2_ATTRIB_ERR_BAD_ARGS = 2,
    MM2_ATTRIB_ERR_BAD_SIZE = 3
} Mm2AttribError;

Mm2AttribError mm2_attrib_decode(const uint8_t *bytes, size_t bytes_size, Mm2AttribFile *out);
Mm2AttribError mm2_attrib_encode(const Mm2AttribFile *attrib, uint8_t *out_bytes, size_t out_size);
Mm2AttribError mm2_attrib_load_file(const char *path, Mm2AttribFile *out);
Mm2AttribError mm2_attrib_save_file(const char *path, const Mm2AttribFile *attrib);

/* Confirmed-field accessors. */
uint8_t mm2_attrib_area_id(const Mm2AttribRecord *r);
uint8_t mm2_attrib_map_category(const Mm2AttribRecord *r);
uint8_t mm2_attrib_tileset(const Mm2AttribRecord *r);
uint8_t mm2_attrib_env_type(const Mm2AttribRecord *r);
uint8_t mm2_attrib_surface_flag(const Mm2AttribRecord *r);
int mm2_attrib_is_outside(const Mm2AttribRecord *r); /* surface_flag != 0 */
int mm2_attrib_is_outdoor(const Mm2AttribRecord *r); /* planes 41..44 or outside */

/* A4-$79E2 on screen enter @ 0x1BDC: outdoor surface / elemental 3D vs dungeon 3D. */
int mm2_attrib_view_mode(const Mm2AttribRecord *r);

/* A4-$7660 materialized from attrib surface_flag low nibble @ 0x1650 (area_env_gfx_loader). */
uint8_t mm2_attrib_runtime_env_id(const Mm2AttribRecord *r);

/* slot: 0..3; slots (0,2) and (1,3) are opposite directions. */
uint8_t mm2_attrib_neighbor(const Mm2AttribRecord *r, int slot);

/* 16-bit complex id at 0x15-0x16 (big-endian); meaningful for interior areas. */
uint16_t mm2_attrib_complex_id(const Mm2AttribRecord *r);
uint8_t mm2_attrib_level(const Mm2AttribRecord *r);

/* era gate (0x0F): event records run when this equals the current era index. */
uint8_t mm2_attrib_era_gate(const Mm2AttribRecord *r);

/* transition destination screen (0x18). */
uint8_t mm2_attrib_transition_screen(const Mm2AttribRecord *r);

/* flags byte (0x1A) and a single-bit test helper (bit 0..7). */
uint8_t mm2_attrib_flags(const Mm2AttribRecord *r);
int mm2_attrib_flag_bit(const Mm2AttribRecord *r, int bit);

static inline uint8_t mm2_attrib_door_strength(const Mm2AttribRecord *r)
{
    return r->raw[MM2_ATTRIB_OFF_DOOR_STRENGTH];
}

static inline uint8_t mm2_attrib_door_trap_byte(const Mm2AttribRecord *r)
{
    return r->raw[MM2_ATTRIB_OFF_DOOR_TRAP];
}

/* Unpack a packed (Y<<4)|X coordinate byte (entry 0x0E / transition 0x16). */
void mm2_attrib_unpack_coord(uint8_t packed, int *x, int *y);

/* Roof bitmap: tile in 0..255. */
int mm2_attrib_roof_bit(const Mm2AttribRecord *r, int tile);
void mm2_attrib_set_roof_bit(Mm2AttribRecord *r, int tile, int value);

#ifdef __cplusplus
}
#endif

#endif
