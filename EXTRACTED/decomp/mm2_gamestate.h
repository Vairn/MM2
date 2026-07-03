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
/* -$79E8 ($8618): a real engine boolean flag (set to 1 / tst / clr in ~15 sites,
 * e.g. 0x6DFE/0x8C98/0x8E2C) — purpose not fully traced (screen/redraw/state).
 * It is NOT party quest progress; the port previously misused it as such. Party
 * class-quest/guild bits live PER CHARACTER at roster record +0x79
 * (class_quest_guild_mask), written by OP_15/18 selector 0x74. Define kept for
 * the offset record only; do not use it for quest bits. */
#define MM2_GS_ENGINE_FLAG_79E8 (-0x79E8)  /* byte  ($8618) untraced engine flag */
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

/* ---- Combat / encounter setup block (OP_12/OP_13 @ 0x16300) ----
 *
 * The event VM seeds these fields; the combat engine (-$7EDE @ 0x051C2)
 * consumes them. Layout (all A4-relative):
 *
 *   struct Mm2EncounterSetup {
 *       uint8_t  monster_slot[10];   // A4-$11DE: up to 10 visible monster-type ids
 *       uint8_t  overflow_type;      // A4-$11D4: type id for monsters beyond the
 *                                    //   10 slots; also the breed/multiply target
 *                                    //   (0x100B0) and XP-budget tier for the
 *                                    //   overflow group (0x120E2). Acts as a flag:
 *                                    //   if !=0, `count` is added to the live total.
 *       uint8_t  mode;               // A4-$796B: 0x80 = fixed/pre-filled fight
 *                                    //   (skip random picker 0x1213E); 0 = seeded
 *                                    //   random (OP_13 lets 0x1213E top up by XP
 *                                    //   budget + attrib min/max). Other engine
 *                                    //   modes: 3 = rest ambush (0x19D64).
 *       uint8_t  live_count;         // A4-$77BE: live monster count. Combat setup
 *                                    //   @ 0x12CE0 recomputes:
 *                                    //     j = #non-zero monster_slot entries
 *                                    //     if overflow_type != 0: j += live_count
 *                                    //     live_count = j
 *   };
 *
 * OP_12 (mode=0x80) reads 12 inline bytes: monster_slot[0..9] + overflow_type +
 *   live_count. OP_13 (mode=0) reads only the 10 monster_slot bytes and clears
 *   overflow_type and live_count to 0 (then the random picker augments).
 */
#define MM2_GS_MONSTER_SLOTS        (-0x11DE)  /* byte[11] ($EE22) monster-type ids; [10]=overflow_type */
#define MM2_GS_ENCOUNTER_OVERFLOW_TYPE (-0x11D4)  /* byte ($EE2C) overflow/breed monster type */
#define MM2_GS_ENCOUNTER_MODE       (-0x796B)  /* byte  ($8693) 0x80=fixed fight, 0=seeded-random, 3=rest ambush */
#define MM2_GS_MONSTER_COUNT        (-0x77BE)  /* byte  ($8842) live monster count (0..11) */
#define MM2_GS_ENCOUNTER_REDRAW     (-0x4F4E)  /* word  ($B0B2) combat/redraw flag (cleared by OP_12) */
#define MM2_GS_MONSTER_SLOT_COUNT   10
#define MM2_GS_MONSTER_BATTLE_SLOTS 11 /* live_count can reach 11 (slot 10 aliases overflow_type) */

/* ---- Combat round loop (0x12A22) battle arrays — doc 17-combat-system.md ----
 * Parallel per-monster-slot arrays, index 0..10 (MM2_GS_MONSTER_BATTLE_SLOTS).
 * Per-party-member acted flags are index 0..7 (party slot, not roster index). */
#define MM2_GS_MONSTER_HP           (-0x53A)   /* word[11] ($7AC4) current HP */
#define MM2_GS_MONSTER_STATUS       (-0x519)   /* byte[11] ($7AE5) bit0=awake/active */
#define MM2_GS_MONSTER_SPEED        (-0x50E)   /* byte[11] ($7AF0) initiative */
#define MM2_GS_MONSTER_ACTED        (-0x5E4A)  /* byte[10] ($21B4) acted-this-round flags */
#define MM2_GS_PARTY_ACTED          (-0x5E40)  /* byte[8]  ($21BE) acted-this-round flags */

/* ---- Random encounter picker (0x1213E/0x12072/0x11F0A) — doc 35 ----
 * party_xp_budget/tier_mod computed once per encounter by encounter_xp_budget_init
 * (0x11E58) from total party HP + disposition and from the highest party level. */
#define MM2_GS_PARTY_XP_BUDGET      (-0x6FCA)  /* long  ($9036) picker budget = totalHP/8 (scaled by disposition) */
#define MM2_GS_PICKER_TIER_MOD      (-0x6FC2)  /* byte  ($903E) = max(party level)/2, added to tier roll */
#define MM2_GS_PICKER_DONE          (-0x6FC1)  /* byte  ($903F) picker loop done flag (budget/gate exhausted) */
#define MM2_GS_MONSTER_FRIEND_COUNT (-0x11B7)  /* byte  ($6E47) picked monster's "friend count" (set by -$7EF6) */
/* ---- Found-item / treasure-reward buffer (OP_2A @ 0x16D16, OP_19 overflow @ 0x16618) ----
 *
 * A 16-byte shared "pending loot" buffer in the A4 block. It is filled by event
 * treasure blocks (OP_2A) and by OP_19 give-item *backpack-overflow*, then later
 * consumed/displayed by the Search payoff (Search key handler @ 0x4800 →
 * `-$7D1C` → 0x1B19C, the "you found..." distribution flow — not yet ported).
 *
 * Memory layout (ascending A4 displacement), traced from the ASM stores:
 *   -$3F1C  byte[3]  item ids       (OP_2A `move.b d0,(-$3F1C,a0,i)`; OP_19 `(a0)=id`)
 *   -$3F19  byte[3]  item flags     (OP_2A 3rd byte/triplet; OP_19 attr3 → `-$3F19`)
 *   -$3F16  byte[3]  item charges   (OP_2A 2nd byte/triplet; OP_19 attr2 → `-$3F16`)
 *   -$3F13  byte     unused pad
 *   -$3F12  word     gems  (big-endian, `move.w d0,-$3F12`)
 *   -$3F10  long     gold/exp (big-endian, `move.l d0,-$3F10`; value is 24-bit)
 *
 * The fill/overflow handlers set the sentinel byte -$794C := 0xFF. The main
 * loop (0x1276) auto-runs Search when -$794C == 0xFE (combat-victory loot drop),
 * bumping it to 0xFF first. The Search handler treats the buffer as "has loot"
 * when any id[0..2]!=0 OR gold!=0 OR gems!=0 (0x4832..0x4870), else "Nothing
 * Here!" ($48AB). NOTE: gold/gems are stored big-endian in RAM (68k move.l/.w),
 * matching the endian-safe mm2_gs_u16/u32 accessors above — this is RAM, not a
 * .dat file, so the CLAUDE.md little-endian-on-disk rule does not apply here.
 */
#define MM2_GS_FOUND_ITEM_ID      (-0x3F1C)  /* byte[3] ($C0E4) found-item ids */
#define MM2_GS_FOUND_ITEM_FLAGS   (-0x3F19)  /* byte[3] ($C0E7) found-item flags (OP_19 attr3) */
#define MM2_GS_FOUND_ITEM_CHARGES (-0x3F16)  /* byte[3] ($C0EA) found-item charges (OP_19 attr2) */
#define MM2_GS_FOUND_GEMS         (-0x3F12)  /* word  ($C0EE) gems (big-endian) */
#define MM2_GS_FOUND_GOLD_EXP     (-0x3F10)  /* long  ($C0F0) gold/exp (big-endian, 24-bit) */
#define MM2_GS_FOUND_SENTINEL     (-0x794C)  /* byte  ($86B4) 0xFF=filled, 0xFE=auto-search pending */
#define MM2_GS_FOUND_ITEM_SLOTS   3
#define MM2_GS_FOUND_SENTINEL_FILLED  0xFF
#define MM2_GS_FOUND_SENTINEL_PENDING 0xFE

#define MM2_GS_EVENT_SCRIPT_START (-0x5C44)  /* word  ($A3BC) */
#define MM2_GS_QUEUED_EVENT_ID  (-0x5D46)  /* byte  ($A2BA) */
#define MM2_GS_SELECTED_MEMBER  (-0x5D43)  /* byte  ($A2BD) OP_26/27 pick 1..8 */
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
#define MM2_GS_GATE_BANK_B      (-0x7995)  /* byte[4] ($867B) quest gates 0x80..0x83 */
#define MM2_GS_GUARDIAN_CAVE    (-0x7996)  /* byte  ($867A) */
/* NOTE: these two are NOT quest counters. They are the Wizard Eye / Eagle Eye
 * overhead-vision step timers (g=0x2C / g=0x2B), decremented one-per-step by the
 * ticker @ 0x4672 and set by the spell handlers @ 0xAD20 / 0xA91C (5/level). The
 * stale names are kept until the spell-effect system lands; offsets are correct.
 * See docs/06-roster-format.md + docs/19-spells-and-item-use.md. */
#define MM2_GS_QUEST_COUNTER_B  (-0x799F)  /* byte  ($8671) = Wizard Eye step timer (g=0x2C) */
#define MM2_GS_CLASS_QUEST_CNT  (-0x79A0)  /* byte  ($8670) = Eagle Eye step timer  (g=0x2B) */
#define MM2_GS_TALISMAN_BASE    (-0x79A4)  /* byte[4] ($866C) ids 0x27..0x2A */
#define MM2_GS_TEMPLE_DONATION  (-0x799E)  /* byte  ($8672) OP_0E temple quest bits */
#define MM2_GS_EVENT_VAR_BANK   (-0x798B)  /* byte[24] ($8685) g=0x00..0x17 */
#define MM2_GS_TUNDRA_LEVER     (-0x7990)  /* byte  ($8680) */
#define MM2_GS_XABRAN_GATE      (-0x798F)  /* byte  ($8681) */
#define MM2_GS_DAWN_GATE        (-0x798E)  /* byte  ($8682) */
#define MM2_GS_SCRIPT_COUNTER   (-0x79B8)  /* byte  ($8646) OP_2C global counter */
#define MM2_GS_INPUT_BUF        (-0x5C50)  /* byte[] OP_2F/OP_30 answer buffer */
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
