#ifndef MM2_FOUND_ITEMS_H
#define MM2_FOUND_ITEMS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MM2 "found item / treasure reward" buffer codec.
 *
 * A 16-byte shared "pending loot" buffer in the A4 game-state block, filled by
 * two event opcodes and later consumed by the Search payoff:
 *
 *   - OP_2A "set treasure/reward block"  @ 0x16D16
 *       reads u24 gold/exp, u16 gems, then 3x(id, charges, flags) from the
 *       script stream and writes the whole buffer; sets sentinel A4-$794C=0xFF.
 *   - OP_19 "give item to party"         @ 0x165D8 (overflow tail @ 0x166A0)
 *       when *every* party backpack is full, the one item being given overflows
 *       into the first free buffer slot (scanning ids[0],ids[1], else slot 2),
 *       writing id/charges/flags and setting A4-$794C=0xFF.
 *
 * Consumption (the "you found..." distribution) is the Search handler @ 0x4800
 * → -$7D1C → 0x1B19C, which is presentation/engine territory not yet traced.
 * The Search handler considers the buffer non-empty when any id[0..2]!=0 OR
 * gold!=0 OR gems!=0 (0x4832..0x4870).
 *
 * Buffer memory layout (ascending A4 displacement). All offsets confirmed from
 * the ASM `lea -$3Fxx(a4)` / `move.l/.w/.b` stores in
 * EXTRACTED/mm2.capstone.annotated.asm:
 *
 *   -$3F1C  byte[3]  id        (raw a4 word $C0E4)
 *   -$3F19  byte[3]  flags     ($C0E7)
 *   -$3F16  byte[3]  charges   ($C0EA)
 *   -$3F13  byte     reserved/pad
 *   -$3F12  u16      gems  (big-endian in RAM: move.w on 68k)   ($C0EE)
 *   -$3F10  u32      gold_exp (big-endian, 24-bit value)        ($C0F0)
 *
 * gold/gems are stored big-endian (Motorola RAM), NOT little-endian: this is a
 * runtime RAM buffer, not a .dat file, so the CLAUDE.md disk LE rule is N/A.
 *
 * See EXTRACTED/docs/07-event-script-opcodes.md and mm2_gamestate.h
 * (MM2_GS_FOUND_*).
 */

enum { MM2_FOUND_ITEM_SLOTS = 3 };

/* A4 displacements (negative signed) and the raw 16-bit displacement words. */
enum {
    MM2_FOUND_A4_ANCHOR    = 0x7FFE,
    MM2_FOUND_DISP_ID       = 0xC0E4, /* (-0x3F1C) & 0xFFFF */
    MM2_FOUND_DISP_FLAGS    = 0xC0E7, /* (-0x3F19) */
    MM2_FOUND_DISP_CHARGES  = 0xC0EA, /* (-0x3F16) */
    MM2_FOUND_DISP_GEMS     = 0xC0EE, /* (-0x3F12) */
    MM2_FOUND_DISP_GOLD     = 0xC0F0, /* (-0x3F10) */
    MM2_FOUND_DISP_SENTINEL = 0x86B4  /* (-0x794C) */
};

enum {
    MM2_FOUND_SENTINEL_EMPTY   = 0x00,
    MM2_FOUND_SENTINEL_PENDING = 0xFE, /* combat-victory: auto-Search next loop (0x1276) */
    MM2_FOUND_SENTINEL_FILLED  = 0xFF  /* set by OP_2A / OP_19 overflow */
};

/* Decoded host-order representation of the buffer. */
typedef struct Mm2FoundItems {
    uint8_t  id[MM2_FOUND_ITEM_SLOTS];      /* A4-$3F1C */
    uint8_t  flags[MM2_FOUND_ITEM_SLOTS];   /* A4-$3F19 (OP_19 attr3) */
    uint8_t  charges[MM2_FOUND_ITEM_SLOTS]; /* A4-$3F16 (OP_19 attr2) */
    uint16_t gems;                          /* A4-$3F12 */
    uint32_t gold_exp;                      /* A4-$3F10 (24-bit) */
    uint8_t  sentinel;                      /* A4-$794C */
} Mm2FoundItems;

/* ---- A4-image accessors (operate directly on the runtime game-state block) ----
 * `a4` is the A4 base pointer (== image + MM2_FOUND_A4_ANCHOR). These mirror the
 * exact byte stores the 68k handlers perform.
 */

/* Read the buffer + sentinel out of the A4 image into host order. */
void mm2_found_items_read(const uint8_t *a4, Mm2FoundItems *out);

/* Write a host-order struct back into the A4 image (buffer + sentinel),
 * big-endian for gems/gold, exactly as the 68k stores do. */
void mm2_found_items_write(uint8_t *a4, const Mm2FoundItems *in);

/* OP_2A fill (event treasure): writes gold/exp (24-bit), gems, and 3 item
 * triplets into the A4 image and sets the sentinel to 0xFF. `block` is the 14
 * inline script bytes following the opcode:
 *   [0..2] gold/exp (u24, little-endian in the *script stream*)
 *   [3..4] gems      (u16, little-endian in the script stream)
 *   [5..13] 3x(id, charges, flags)
 * (Script-stream values are little-endian per the byte/word/long readers
 *  $155be/$155da/$155fc; they are stored big-endian into the A4 RAM buffer.) */
void mm2_found_items_op2a_fill(uint8_t *a4, const uint8_t block[14]);

/* OP_19 overflow append: place one item into the first free buffer slot
 * (scan id[0], id[1]; if both occupied use slot 2 — matching the 0x166A0 loop
 * `cmpi.w #2`), writing id/flags/charges and setting sentinel=0xFF.
 * Returns the slot index written (0..2). */
int mm2_found_items_overflow_append(uint8_t *a4, uint8_t id, uint8_t charges, uint8_t flags);

/* Search "has loot" predicate (0x4832..0x4870): true when any id[0..2]!=0 OR
 * gold!=0 OR gems!=0. */
int mm2_found_items_has_loot(const uint8_t *a4);

/* ---- Pure struct helpers (no A4 image; for tooling / round-trip tests) ---- */

/* Decode a 14-byte OP_2A inline block into a struct (sets sentinel=0xFF). */
void mm2_found_items_decode_block(const uint8_t block[14], Mm2FoundItems *out);

/* Encode a struct back into a 14-byte OP_2A inline block (script-stream order,
 * little-endian gold/gems). Returns 14. */
int mm2_found_items_encode_block(const Mm2FoundItems *in, uint8_t block[14]);

/* Number of occupied item slots (id != 0). */
int mm2_found_items_count(const Mm2FoundItems *in);

#ifdef __cplusplus
}
#endif

#endif /* MM2_FOUND_ITEMS_H */
