#ifndef MM2_GAMESTATE_H
#define MM2_GAMESTATE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * MM2 global game-state accessors.
 *
 * All mutable runtime state lives in one RAM block anchored by A4 = $7FFE
 * (LEA.L $7FFE,A4 at 0x24920). Each field is a fixed *signed* 16-bit
 * displacement from A4. See EXTRACTED/docs/14-game-state-struct.md.
 *
 * Two notations exist in the disassembly/docs:
 *   - true signed offset  (canonical here): e.g. MM2_GS_COND_FLAG = -0x7951
 *   - raw displacement word (legacy docs):  word = offset & 0xFFFF (0x86AF)
 * Effective address: EA = MM2_A4_ANCHOR + offset.
 *
 * 68000 is big-endian; the word/long accessors below are endian-safe.
 */

#define MM2_A4_ANCHOR 0x7FFE

/* ---- Confirmed field offsets (true signed displacement from A4) ---- */

/* Session / control */
#define MM2_GS_SCREEN_MODE_ID   (-0x79F2)  /* byte  ($860E) */
#define MM2_GS_COORD_B          (-0x79F1)  /* byte  ($860F) */
#define MM2_GS_COORD_A          (-0x79F0)  /* byte  ($8610) */
#define MM2_GS_SCRIPT_ABORT     (-0x79EA)  /* byte  ($8616) */
#define MM2_GS_FIRST_TIME_FLAG  (-0x79E9)  /* byte  ($8617) */
#define MM2_GS_PARTY_PROGRESS   (-0x79E8)  /* byte  ($8618) quest bits via apply_party op 0x74 */
#define MM2_GS_SCREEN_MODE_PREV (-0x79E6)  /* byte  ($861A) */
#define MM2_GS_SIGN_ENV_ID      (-0x79E3)  /* byte  ($861D) OP_0B table pick @ 0x15772 */
#define MM2_GS_BUSY_STATUS      (-0x79E5)  /* byte  ($861B) */
#define MM2_GS_ERA              (-0x79B6)  /* word  ($864A) era/timeline 0..9 */
/* Low byte of the era word; read as the "current era" by event gating.
 * (Previously mislabeled CUR_EVENT_ID: there is no standalone byte write to
 * -0x79B5; it is only ever set via word writes to MM2_GS_ERA. OP_22 at
 * 0x16A9E range-checks this value, and the event dispatch at 0x172BC compares
 * it against attrib.dat byte 0x0F.) */
#define MM2_GS_ERA_LOW          (-0x79B5)  /* byte  ($864B) = era & 0xFF */
#define MM2_GS_NEW_GAME_FLAG    (-0x79B2)  /* byte  ($864E) right-panel mode 0=OPTIONS 1=PROTECT 2=combat */
#define MM2_GS_LAST_MOVE_KEY    (-0x79B1)  /* byte  ($864F) N/S/E/W */
#define MM2_GS_SOUNDS_FLAG      (-0x79B0)  /* byte  ($8650) bit0 = SFX on */
#define MM2_GS_WALK_BEEP_FLAG   (-0x79AF)  /* byte  ($8651) bit0 = footstep beep */
#define MM2_GS_DISPOSITION      (-0x79AE)  /* byte  ($8652) 0..3 AI mood */
#define MM2_GS_DELAY            (-0x79AD)  /* byte  ($8653) 0..9 text delay */
#define MM2_GS_LIGHT_FACTOR     (-0x79AB)  /* byte  ($8655) light / Lasting Light */
#define MM2_GS_MAGIC_PROTECT    (-0x79AA)  /* byte  ($8656) magic protection tier */
#define MM2_GS_FORCES_PROTECT   (-0x79A9)  /* byte  ($8657) forces protection tier */
#define MM2_GS_LEVITATE_FLAG    (-0x79A8)  /* byte  ($8658) Levitate active */
#define MM2_GS_WALK_WATER_FLAG  (-0x79A7)  /* byte  ($8659) Walk on Water */
#define MM2_GS_GUARD_DOG_FLAG   (-0x79A6)  /* byte  ($865A) Guard Dog */
#define MM2_GS_EVENT_PARSE_POS  (-0x7956)  /* word  ($86AA) */
#define MM2_GS_EVENT_SCRIPT_ANCHOR (-0x7954)  /* word  ($86AC) */
#define MM2_GS_PENDING_EVENT_LATCH (-0x7952)  /* byte  ($86AE) */
#define MM2_GS_COND_FLAG        (-0x7951)  /* byte  ($86AF) */
#define MM2_GS_EXIT_FLAGS       (-0x7950)  /* byte  ($86B0) */
#define MM2_GS_COMBAT_VICTORY_LATCH (-0x77BD)  /* byte  ($8843) OP_2B skip gate @ 0x16D74 */
#define MM2_GS_EVENT_SCRIPT_START (-0x5C44)  /* word  ($A3BC) */
#define MM2_GS_QUEUED_EVENT_ID  (-0x5D46)  /* byte  ($A2BA) */
#define MM2_GS_SAVED_COND_FLAG  (-0x5D42)  /* byte  ($A2BE) */
#define MM2_GS_STRING_WALK_INDEX (-0x5D44)  /* word  ($A2BC) */
#define MM2_GS_FACING_INDEX     (-0x55D7)  /* byte  ($AA29) */
#define MM2_GS_EVENT_BUSY_SENTINEL (-0x55C8)  /* byte  ($AA38) */
#define MM2_GS_ATTRIB_ERA_GATE  (-0xA9F5)  /* byte  ($560B) */
#define MM2_GS_CONTEXT_MASK_TBL (-0x6BE6)  /* byte[] ($941A) */
#define MM2_GS_INPUT_STATE      (-0x799D)  /* byte[] ($8663..$8667) */
#define MM2_GS_INPUT_STATE_END  (-0x7999)  /* byte  ($8667) */

/* Calendar / era */
#define MM2_GS_DAY              (-0x79DE)  /* word[10] ($8622) day 1..180 */
#define MM2_GS_YEAR             (-0x79CA)  /* word[10] ($8636) cap 999 */
#define MM2_GS_TIME_SUBDAY      (-0x79B4)  /* word  ($864C) 256 = 1 day */
#define MM2_GS_PERIOD_FLAG_A    (-0x798C)  /* byte  ($8674) */
#define MM2_GS_PERIOD_FLAG_B    (-0x798D)  /* byte  ($8673) */
#define MM2_GS_ERA_COUNT        10

/* Lookup-table bases */
#define MM2_GS_ROSTER_INDEX_TBL (-0x796A)  /* word[]  ($8696) */
#define MM2_GS_CLASS_NAME_TBL   (-0x7928)  /* ($86D8) */
#define MM2_GS_RACE_NAME_TBL    (-0x7908)  /* ($86F8) */
#define MM2_GS_ALIGN_NAME_TBL   (-0x78F4)  /* ($870C) */
#define MM2_GS_MONTH_TBL        (-0x711C)  /* word[13] ($8EE4) */
#define MM2_GS_SEASON_TBL_A     (-0x7102)  /* ($8EFE) */
#define MM2_GS_SEASON_TBL_B     (-0x70F5)  /* ($8F0B) */
#define MM2_GS_OPCODE_LEN_TBL   (-0x6CC8)  /* ($9338) */

/* Buffers & pointers */
#define MM2_GS_DRAW_CTX         (-0x7A1A)  /* ptr   ($85E6) */
#define MM2_GS_MANX_POOL        (-0x5E62)  /* ptr   ($A19E) */
#define MM2_GS_PARTY_SLOTS      (-0x5E5E)  /* byte[8] ($A1A2) */
#define MM2_GS_TILE_RT_FLAGS    (-0x55D6)  /* byte[] ($AA2A) */
#define MM2_GS_TILE_TABLE_A     (-0x55BA)  /* byte[] ($AA46) */
#define MM2_GS_TILE_VISITED     (-0x54BA)  /* byte[] ($AB46) */
#define MM2_GS_EVENT_WORK_BUF   (-0x47C8)  /* byte[2220] ($B838) */
#define MM2_GS_ROSTER_BASE      (-0x2A3E)  /* byte[] ($D5C2) stride 0x82 */
#define MM2_GS_MAP_BLOB         (-0x110C)  /* ptr   ($EEF4) */
#define MM2_GS_EVENT_BLOB       (-0x1108)  /* ptr   ($EEF8) */

#define MM2_GS_PARTY_SIZE       8
#define MM2_GS_ROSTER_STRIDE    0x82
#define MM2_GS_EVENT_WORK_SIZE  2220

/* ---- Big-endian primitive accessors over the A4 base pointer ---- */

static inline uint8_t mm2_gs_u8(const uint8_t *a4, int32_t off)
{
    return a4[off];
}

static inline void mm2_gs_set_u8(uint8_t *a4, int32_t off, uint8_t v)
{
    a4[off] = v;
}

static inline uint16_t mm2_gs_u16(const uint8_t *a4, int32_t off)
{
    const uint8_t *p = a4 + off;
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline void mm2_gs_set_u16(uint8_t *a4, int32_t off, uint16_t v)
{
    uint8_t *p = a4 + off;
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static inline uint32_t mm2_gs_u32(const uint8_t *a4, int32_t off)
{
    const uint8_t *p = a4 + off;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void mm2_gs_set_u32(uint8_t *a4, int32_t off, uint32_t v)
{
    uint8_t *p = a4 + off;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* Per-era calendar arrays (word-indexed by era 0..9). */
static inline uint16_t mm2_gs_day(const uint8_t *a4, int era)
{
    return mm2_gs_u16(a4, MM2_GS_DAY + era * 2);
}

static inline uint16_t mm2_gs_year(const uint8_t *a4, int era)
{
    return mm2_gs_u16(a4, MM2_GS_YEAR + era * 2);
}

/* Convenience: derive the A4 base pointer from a raw memory image whose
 * byte 0 corresponds to absolute address 0 (so &image[MM2_A4_ANCHOR]). */
static inline uint8_t *mm2_gs_base_from_image(uint8_t *image)
{
    return image + MM2_A4_ANCHOR;
}

#endif /* MM2_GAMESTATE_H */
