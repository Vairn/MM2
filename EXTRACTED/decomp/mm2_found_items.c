#include "mm2_found_items.h"

/* Signed A4 displacements (mirror MM2_GS_FOUND_* in mm2_gamestate.h). */
enum {
    OFF_ID       = -0x3F1C,
    OFF_FLAGS    = -0x3F19,
    OFF_CHARGES  = -0x3F16,
    OFF_GEMS     = -0x3F12,
    OFF_GOLD     = -0x3F10,
    OFF_SENTINEL = -0x794C
};

/* Big-endian RAM accessors (68k move.w / move.l store order). */
static uint16_t be_u16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void set_be_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static uint32_t be_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void set_be_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void mm2_found_items_read(const uint8_t *a4, Mm2FoundItems *out)
{
    int i;
    for (i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        out->id[i] = a4[OFF_ID + i];
        out->flags[i] = a4[OFF_FLAGS + i];
        out->charges[i] = a4[OFF_CHARGES + i];
    }
    out->gems = be_u16(a4 + OFF_GEMS);
    out->gold_exp = be_u32(a4 + OFF_GOLD);
    out->sentinel = a4[OFF_SENTINEL];
}

void mm2_found_items_write(uint8_t *a4, const Mm2FoundItems *in)
{
    int i;
    for (i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        a4[OFF_ID + i] = in->id[i];
        a4[OFF_FLAGS + i] = in->flags[i];
        a4[OFF_CHARGES + i] = in->charges[i];
    }
    set_be_u16(a4 + OFF_GEMS, in->gems);
    set_be_u32(a4 + OFF_GOLD, in->gold_exp);
    a4[OFF_SENTINEL] = in->sentinel;
}

void mm2_found_items_op2a_fill(uint8_t *a4, const uint8_t block[14])
{
    /* event_op2a_set_reward_block @ 0x16D16.
     *   $155fc: gold/exp = block[0] | block[1]<<8 | block[2]<<16  (u24, LE stream)
     *           -> move.l to -$3F10 (stored big-endian).
     *   $155da: gems     = block[3] | block[4]<<8                 (u16, LE stream)
     *           -> move.w to -$3F12 (stored big-endian).
     *   loop i=0..2: id=block[5+3i] -> -$3F1C[i]; charges=block[6+3i] -> -$3F16[i];
     *                flags=block[7+3i] -> -$3F19[i].
     *   move.b #$FF,-$794C. */
    int i;
    uint32_t gold = (uint32_t)block[0] | ((uint32_t)block[1] << 8) |
                    ((uint32_t)block[2] << 16);
    uint16_t gems = (uint16_t)((uint16_t)block[3] | ((uint16_t)block[4] << 8));

    set_be_u32(a4 + OFF_GOLD, gold);
    set_be_u16(a4 + OFF_GEMS, gems);
    for (i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        a4[OFF_ID + i] = block[5 + i * 3];
        a4[OFF_CHARGES + i] = block[6 + i * 3];
        a4[OFF_FLAGS + i] = block[7 + i * 3];
    }
    a4[OFF_SENTINEL] = (uint8_t)MM2_FOUND_SENTINEL_FILLED;
}

int mm2_found_items_overflow_append(uint8_t *a4, uint8_t id, uint8_t charges, uint8_t flags)
{
    /* OP_19 overflow tail @ 0x166A0: scan i=0,1 (cmpi.w #$2) for an empty id;
     * if both occupied, fall through with i=2 (overwrites slot 2). Then write
     * id->-$3F1C[i], flags->-$3F19[i], charges->-$3F16[i]; move.b #$FF,-$794C. */
    int i = 0;
    while (i < 2 && a4[OFF_ID + i] != 0) {
        ++i;
    }
    a4[OFF_ID + i] = id;
    a4[OFF_FLAGS + i] = flags;
    a4[OFF_CHARGES + i] = charges;
    a4[OFF_SENTINEL] = (uint8_t)MM2_FOUND_SENTINEL_FILLED;
    return i;
}

int mm2_found_items_has_loot(const uint8_t *a4)
{
    /* Search handler @ 0x4832..0x4870: empty unless an id, gold, or gems is set. */
    int i;
    for (i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        if (a4[OFF_ID + i] != 0)
            return 1;
    }
    if (be_u32(a4 + OFF_GOLD) != 0)
        return 1;
    if (be_u16(a4 + OFF_GEMS) != 0)
        return 1;
    return 0;
}

void mm2_found_items_decode_block(const uint8_t block[14], Mm2FoundItems *out)
{
    int i;
    out->gold_exp = (uint32_t)block[0] | ((uint32_t)block[1] << 8) |
                    ((uint32_t)block[2] << 16);
    out->gems = (uint16_t)((uint16_t)block[3] | ((uint16_t)block[4] << 8));
    for (i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        out->id[i] = block[5 + i * 3];
        out->charges[i] = block[6 + i * 3];
        out->flags[i] = block[7 + i * 3];
    }
    out->sentinel = (uint8_t)MM2_FOUND_SENTINEL_FILLED;
}

int mm2_found_items_encode_block(const Mm2FoundItems *in, uint8_t block[14])
{
    int i;
    block[0] = (uint8_t)(in->gold_exp & 0xFF);
    block[1] = (uint8_t)((in->gold_exp >> 8) & 0xFF);
    block[2] = (uint8_t)((in->gold_exp >> 16) & 0xFF);
    block[3] = (uint8_t)(in->gems & 0xFF);
    block[4] = (uint8_t)((in->gems >> 8) & 0xFF);
    for (i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        block[5 + i * 3] = in->id[i];
        block[6 + i * 3] = in->charges[i];
        block[7 + i * 3] = in->flags[i];
    }
    return 14;
}

int mm2_found_items_count(const Mm2FoundItems *in)
{
    int i, n = 0;
    for (i = 0; i < MM2_FOUND_ITEM_SLOTS; ++i) {
        if (in->id[i] != 0)
            ++n;
    }
    return n;
}
