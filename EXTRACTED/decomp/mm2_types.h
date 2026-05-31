#ifndef MM2_TYPES_H
#define MM2_TYPES_H

#include <stdint.h>
#include <stddef.h>

/*
 * First-pass runtime model for MM2 Amiga executable.
 * Offsets are from workspace base A4=$7FFE in original 68k code.
 * Unknown fields are intentionally preserved as raw words/bytes.
 */

typedef struct Mm2Workspace {
    uint8_t pad_0000_79ed[0x79EE];

    /* A4-$860E / A4-$860F / A4-$8610 / A4-$861A family */
    uint8_t mode_current;      /* guessed: A4-$860E */
    uint8_t map_y;             /* guessed: A4-$860F */
    uint8_t map_x;             /* guessed: A4-$8610 */
    uint8_t pad_79f1_79e3[0x0A];
    uint8_t mode_previous;     /* guessed: A4-$861A */
    uint8_t pad_79fc_79de[0x03];
    uint8_t input_modal;       /* guessed: A4-$861E */
    uint8_t pad_7a00_79e0[0x01];
    uint8_t busy_flag;         /* guessed: A4-$861B */
    uint8_t pad_after_861b[0x2F];
    uint8_t last_key;          /* guessed: A4-$864F */
    uint8_t new_game_flag;     /* guessed: A4-$864E */
    uint8_t pad_after_864f[0x60];
    uint8_t exit_request;      /* guessed: A4-$86B0 */
    uint8_t pad_after_86b0[0x35];
    uint8_t state_86e6;

    /* NOTE: This struct is not complete; use helper accessors for exact offsets. */
} Mm2Workspace;

/* Runtime handles created during startup (`0x248AE` region). */
typedef struct Mm2Runtime {
    void *exec_base;           /* from $4.w */
    void *dos_base;            /* OpenLibrary("dos.library") result */
    void *manx_block;          /* A4-$34E */
    void *arg_table;           /* A4-$FCCC during tokenizer flow */
    void *map_blob_base;       /* A4-$EEF4 */
} Mm2Runtime;

/* A4 thunk table aliases (names are inferred). */
typedef struct Mm2EngineApi {
    int32_t (*pump)(void);                         /* -$83C2 */
    int32_t (*set_mode)(int16_t a, int16_t b);     /* -$83CE */
    int32_t (*draw_cell)(void *ctx, int16_t x, int16_t y, int16_t glyph); /* -$83E0 */
    int16_t (*read_key)(void);                     /* -$842E */
    void (*delay_ticks)(int16_t ticks);            /* -$8440 */
} Mm2EngineApi;

/*
 * Confirmed data-file formats (see EXTRACTED/docs/07-dat-files-and-formats.md).
 */

enum {
    MM2_ITEMS_RECORD_SIZE = 0x14,
    MM2_ITEMS_NAME_SIZE = 0x0C,
    MM2_ITEMS_RECORD_COUNT = 256
};

/* Byte 0x0D is a *restriction* mask: a SET bit means that class CANNOT use the
 * item. Usable-by-class X is the predicate `(mask & bit) == 0`, so 0x00 = every
 * class can use it (basic clubs). Confirmed from items.dat: Katana/Nunchakas =
 * 0x7D (only Knight+Ninja clear), Holy Cudgel = 0xAF (only Paladin+Cleric
 * clear), and every bladed weapon sets the Cleric bit while blunt weapons leave
 * it clear (Clerics can't wield blades).
 *
 * Class-bit order: Knight, Paladin, Archer, Cleric, Sorcerer, Robber, Ninja,
 * Barbarian. NOTE: the legacy Blitz3D editor swapped Robber/Cleric/Sorcerer;
 * the order below is the data-validated one (Cleric=0x10, Sorcerer=0x08,
 * Robber=0x04). */
enum Mm2ItemForbiddenClassMask {
    MM2_ITEM_NOUSE_KNIGHT   = 0x80,
    MM2_ITEM_NOUSE_PALADIN  = 0x40,
    MM2_ITEM_NOUSE_ARCHER   = 0x20,
    MM2_ITEM_NOUSE_CLERIC   = 0x10,
    MM2_ITEM_NOUSE_SORCERER = 0x08,
    MM2_ITEM_NOUSE_ROBBER   = 0x04,
    MM2_ITEM_NOUSE_NINJA    = 0x02,
    MM2_ITEM_NOUSE_BARBARIAN= 0x01
};

typedef struct Mm2ItemRecord {
    char name[MM2_ITEMS_NAME_SIZE]; /* 12-byte ASCII, space-padded */
    uint8_t separator;              /* offset 0x0C */
    uint8_t forbidden_class_mask;   /* offset 0x0D, set bit = class CANNOT use */
    uint8_t bonus_packed;           /* offset 0x0E, hi=type lo=amount */
    uint8_t effect_packed;          /* offset 0x0F, hi=type lo=amount */
    uint8_t damage;                 /* offset 0x10 */
    uint8_t pad;                    /* offset 0x11 */
    uint16_t gold_le;               /* offset 0x12, little-endian shop price */
} Mm2ItemRecord;

typedef struct Mm2ItemsFile {
    Mm2ItemRecord records[MM2_ITEMS_RECORD_COUNT];
} Mm2ItemsFile;

static inline uint8_t mm2_nibble_hi(uint8_t packed)
{
    return (uint8_t)((packed >> 4) & 0x0F);
}

static inline uint8_t mm2_nibble_lo(uint8_t packed)
{
    return (uint8_t)(packed & 0x0F);
}

static inline uint8_t mm2_nibbles_pack(uint8_t hi, uint8_t lo)
{
    return (uint8_t)(((hi & 0x0F) << 4) | (lo & 0x0F));
}

enum {
    MM2_STR_NEWLINE_ENCODED = 0x01,
    MM2_STR_SHIFT = 0x1C
};

static inline uint8_t mm2_str_decode_byte(uint8_t encoded)
{
    if (encoded == MM2_STR_NEWLINE_ENCODED) {
        return (uint8_t)'\n';
    }
    return (uint8_t)(encoded + MM2_STR_SHIFT);
}

static inline uint8_t mm2_str_encode_byte(uint8_t decoded)
{
    if (decoded == (uint8_t)'\n') {
        return MM2_STR_NEWLINE_ENCODED;
    }
    return (uint8_t)(decoded - MM2_STR_SHIFT);
}

/* Symbolic key map extracted from overland handling. */
enum Mm2DirectionCode {
    MM2_DIR_NORTH = 0x20,
    MM2_DIR_SOUTH = 0x21,
    MM2_DIR_EAST  = 0x22,
    MM2_DIR_WEST  = 0x23
};

typedef char mm2_static_assert_item_record_size[
    (sizeof(Mm2ItemRecord) == MM2_ITEMS_RECORD_SIZE) ? 1 : -1
];

/* map.dat page 1 — four 2-bit wall codes per cell (N/E/S/W). */
typedef struct Mm2WallCell {
    uint8_t packed; /* [NN:2][EE:2][SS:2][WW:2] */
} Mm2WallCell;

static inline uint8_t mm2_wall_field(Mm2WallCell c, int dir /* 0=N..3=W */)
{
    return (uint8_t)((c.packed >> (dir * 2)) & 3u);
}

typedef struct Mm2View3DCamera {
    uint8_t x;
    uint8_t y;
    uint8_t facing; /* 0=N, 1=E, 2=S, 3=W */
} Mm2View3DCamera;

#endif

