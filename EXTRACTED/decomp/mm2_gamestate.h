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
#define MM2_GS_OP0E_SUBMODE     (-0x7946)  /* word  ($86BA) OP_0E entry=2; inn/0xFD=1 @ 0x160D4 */
#define MM2_GS_SAVED_TOWN_ID    (-0x79AC)  /* byte  ($8654) inn 0x1A1E2: copy -$79F2; 0xFD→GotoTown */
#define MM2_GS_FIRST_TIME_FLAG  (-0x79E9)  /* byte  ($8617) */
/* -$79E8 ($8618): a real engine boolean flag (set to 1 / tst / clr in ~15 sites,
 * e.g. 0x6DFE/0x8C98/0x8E2C) — purpose not fully traced (screen/redraw/state).
 * It is NOT party quest progress; the port previously misused it as such. Party
 * class-quest/guild bits live PER CHARACTER at roster record +0x79
 * (class_quest_guild_mask), written by OP_15/18 selector 0x74. Define kept for
 * the offset record only; do not use it for quest bits. */
#define MM2_GS_ENGINE_FLAG_79E8 (-0x79E8)  /* byte  ($8618) untraced engine flag */
#define MM2_GS_SCREEN_MODE_PREV (-0x79E6)  /* byte  ($861A) */
#define MM2_GS_VIEW_MODE        (-0x79E2)  /* byte  ($861E) 0=dungeon 3D, 1=outdoor surface @ 0x10D6 */
/* -$79E1 ($861F): "can't see" / interaction-suppress flag. Set by
 * session_interaction_gate @ 0x53C0 (darkness / tile bit5). OP_04/05/06
 * skip their draw when nonzero (0x15A00 / 0x15A52 / 0x15B24). */
#define MM2_GS_CANT_SEE_FLAG    (-0x79E1)  /* byte  ($861F) */

/* Current-screen attrib buffer @ A4-$561A (loader 0x923E copies 64 bytes).
 * Field reads are A4-$561A + record offset (doc 12). */
#define MM2_GS_ATTRIB_BUF       (-0x561A)  /* byte[64] ($A9E6) */
#define MM2_GS_ENTRY_COORD      (-0x560C)  /* byte  ($A9F4) = attrib 0x0E packed (Y<<4)|X */
#define MM2_GS_RETREAT_DIFF     (-0x560D)  /* byte  ($A9F3) = attrib 0x0D; flee if rng<$thresh @ 0x116CA/0x13148 */
#define MM2_GS_PARTY_RAN_LATCH  (-0x5E4C)  /* byte  ($A1B4) set #1 on successful char Run @ 0x116D2; → 0x11646 */
#define MM2_GS_MELEE_RANGE_N    (-0x0524)  /* byte  ($FADC) monsters in melee reach; 0x11D0C / AI @ 0x1079A */
#define MM2_GS_FRONT_RANK_N     (-0x5E4D)  /* byte  ($A1B3) party front-rank cutoff; 0x11D0C / 0x103BA */
#define MM2_GS_ATTRIB_FLAGS     (-0x5600)  /* byte  ($AA00) = attrib 0x1A flags */
#define MM2_GS_SIGN_ENV_ID      (-0x79E3)  /* byte  ($861D) OP_0B table pick @ 0x15772 */
#define MM2_GS_RUNTIME_ENV      (-0x7660)  /* byte  ($89A0) area gfx env id; surface_flag&0xF @ 0x1650 */
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
#define MM2_GS_BUFF_79A5        (-0x79A5)  /* byte  ($865B) temple-bless / spell buff (addq @ 0x1FC2A) */
#define MM2_GS_BUFF_79A3        (-0x79A3)  /* byte  ($865D) set #1 @ 0xB284 (C flat21 Air Transmute) */
#define MM2_GS_BUFF_79A2        (-0x79A2)  /* byte  ($865E) set #1 @ 0xB400 (C flat41 Fire Transmute) */
#define MM2_GS_BUFF_79A1        (-0x79A1)  /* byte  ($865F) set #1 @ 0xB34C (C flat31 Earth Transmute) */
#define MM2_GS_SPELL_STATUS_MODE (-0x053B) /* byte  ($FAC5) 0=damage 1=status 2=skip-banner (0x108BC) */
#define MM2_GS_SPELL_STATUS_CODE (-0x053C) /* byte  ($FAC4) status opcode; bits via -$6F2E */
#define MM2_GS_ENCASE_TIER      (-0x0521) /* byte  ($FADF) Encase power 0..4; DoT idx @ 0x106BC */
#define MM2_GS_ENCASE_DMG_TBL   (-0x6F26) /* word[4] ($90DA) 10/20/40/80 @ 0x106D4 */
#define MM2_GS_ENCASE_MODE_TBL  (-0x6F1E) /* byte[4] ($90E2) mode_d 6/5/4/5 @ 0x106E4 */
#define MM2_GS_MONSTER_FLEE_TIER (-0x11B6) /* byte  ($EE4A) Oabil bits5-6 @ 0x4E9C */
#define MM2_GS_STATUS_BIT_TBL   (-0x6F2E)  /* byte[8] ($90D2) data hunk 0x10D0: 02 04 08 10 20 40 80 00 */
#define MM2_GS_MRES_CHANCE_TBL  (-0x7464)  /* byte[8] ($8B9C) data 0xB9A: 0,10,20,35,50,75,90,100 */
#define MM2_GS_MONSTER_MRES     (-0x11AF)  /* byte  ($EE51) mres chance tbl[-$7464] (0x4C8E) */
#define MM2_GS_MONSTER_UNDEAD   (-0x11AD)  /* byte  ($EE53) Sabil bit7 (0x4C8E) */
#define MM2_GS_MONSTER_AC_BIT6  (-0x11AB)  /* byte  ($EE55) AC bit6 (0x4C8E) */
#define MM2_GS_MONSTER_AC_BIT7  (-0x11AC)  /* byte  ($EE54) AC bit7 (0x4C8E) */
#define MM2_GS_MONSTER_AC       (-0x11B4)  /* byte  ($EE4C) AC low5+1 (×10 if bit5) @ 0x4F0A */
#define MM2_GS_MONSTER_SWINGS   (-0x11B2)  /* byte  ($EE4E) speed2 low4+1 @ 0x4EAE */
#define MM2_GS_MONSTER_DMG_SIDES (-0x11B3) /* byte  ($EE4D) damage low5+1 (×10 if bit5) @ 0x4F52 */
#define MM2_GS_MONSTER_HIT_TBL  (-0x6F16)  /* byte[16] ($90EA) type>>4 → to-hit chance @ 0x104B2 */
#define MM2_GS_COMBAT_TARGET_SLOT (-0x04F8) /* byte ($FB08) party slot under attack @ 0x103BA */
#define MM2_GS_LLOYD_SCREEN     (-0x7998)  /* byte  ($8668) Lloyd's Beacon screen @ 0xAB4C */
#define MM2_GS_LLOYD_COORD      (-0x7997)  /* byte  ($8669) Lloyd's Beacon (Y<<4)|X @ 0xAB62 */
#define MM2_GS_FLY_SCREEN_TBL   (-0x7120)  /* byte[20] ($8EE0) Fly sector→screen @ 0xACE4 */
/* 0x1086E bank @ -$11AA[mode_d-1]: dmg.b6, dmg.b7, spd2.b6, spd2.b7, mres.b1, mres.b0, mres.b2 */
#define MM2_GS_MONSTER_FLAG_BASE (-0x11AA) /* byte[] ($EE56) */
#define MM2_GS_HP_APPLY         (-0x0F0C)  /* word  ($F0F2) 0x10ED4 damage scratch */
#define MM2_GS_CUR_MON_SLOT     (-0x051D)  /* byte  ($FAE3) current monster slot (0x10894) */
#define MM2_GS_BLESS_COUNTER    (-0x799D)  /* byte  ($8663) Bless addq @ 0xBFC4 */
#define MM2_GS_INVIS_COUNTER    (-0x799C)  /* byte  ($8664) Invisibility addq @ 0xB9C4 */
#define MM2_GS_SHIELD_COUNTER   (-0x799B)  /* byte  ($8665) Shield addq @ 0xBB5C; half dmg @ 0xFD48 */
#define MM2_GS_POWER_SHIELD_CTR (-0x799A)  /* byte  ($8666) Power Shield addq @ 0xBEC8; half @ 0xFD2C */
#define MM2_GS_HOLY_BONUS_CTR   (-0x7999)  /* byte  ($8667) Holy Bonus add @ 0xC320 */
#define MM2_GS_ATTRIB_RECALL_COORD (-0x5604) /* byte ($A9FC) attrib+0x16 packed YX; Surface @ 0xB2B2 */
#define MM2_GS_ATTRIB_RECALL_SCREEN (-0x5602) /* byte ($A9FE) attrib+0x18 dest screen; Surface */
#define MM2_GS_MONSTER_PABIL    (-0x11BC)  /* byte  ($EE44) Pabil&$1F @ 0x4DFA */
#define MM2_GS_MONSTER_PABIL_CHANCE (-0x11BB) /* byte ($EE45) group-special chance @ 0x4E14 */
#define MM2_GS_MONSTER_CHARGE_INIT (-0x11BA) /* byte ($EE46) (spd2>>4)+1 @ 0x4EB8; → -$503 */
#define MM2_GS_MONSTER_SABIL    (-0x11B9)  /* byte  ($EE47) Sabil&$1F @ 0x4E50 */
#define MM2_GS_MONSTER_ARCHER   (-0x11AE)  /* byte  ($EE52) Sabil bit6 @ 0x4E38 */
#define MM2_GS_PABIL_CHANCE_TBL (-0x7464)  /* byte[] ($8B9C) Pabil tier → chance @ 0x4E10 */
#define MM2_GS_MONSTER_SPECIAL_CHARGES (-0x503) /* byte[11] ($7AFB) special uses left @ 0x105B6 */
#define MM2_GS_MONSTER_TURN_SLOT (-0x4F7) /* byte ($FB09) active monster slot @ 0x1065A */
#define MM2_GS_PABIL_RESIST_A   (-0x666A)  /* byte[32] → -$5630 @ 0x1FB5C */
#define MM2_GS_PABIL_RESIST_B   (-0x664A)  /* byte[32] → -$562F @ 0x1FB6C */
#define MM2_GS_PABIL_RESIST_C   (-0x662A)  /* byte[32] −$11 → -$562E @ 0x1FB7C */
#define MM2_GS_PABIL_TARGET_ORD (-0x65FE)  /* byte[32] multi-hit order @ 0x1F4F8 */
#define MM2_GS_RESIST_FLAG_A    (-0x5630)  /* byte ($A9D0) 0x1FB4E resist scratch */
#define MM2_GS_RESIST_FLAG_B    (-0x562F)  /* byte ($A9D1) */
#define MM2_GS_RESIST_FLAG_C    (-0x562E)  /* byte ($A9D2) */
#define MM2_GS_LUCK_THRESH_TBL  (-0x7486)  /* byte[] ($8B7A) 0x4442 luck→bonus */
#define MM2_GS_RESIST_BUFF_A    (-0x79AA)  /* byte ($8656) added to +$16 in 0x4952 */
#define MM2_GS_RESIST_BUFF_C    (-0x79A9)  /* byte ($8657) added in 0x4952 flag-C */
#define MM2_GS_TIME_DISTORT     (-0x0523)  /* byte  ($FADD) Time Distortion addq @ 0xBBCE */
#define MM2_GS_ENTRAPMENT       (-0x0522)  /* byte  ($FADE) Entrapment addq @ 0xBD00 */
#define MM2_GS_HOLY_WORD_GATE   (-0x0520)  /* byte  ($FAE0) Holy Word sets #1 @ 0xC756 */
#define MM2_GS_TURN_UNDEAD_USED (-0x051F)  /* byte  ($FAE1) Turn Undead latch @ 0xC078 */
#define MM2_GS_ERADICATE_SKIP_PRINT (-0x051E) /* byte ($FAE2) 1→0x10DFC skips " goes down!" */
#define MM2_GS_SPELL_SKIP_RESIST (-0x051B) /* byte  ($FAE5) 0x10894 skips resist if set (Frenzy) */
#define MM2_GS_FRENZY_LATCH     (-0x051C)  /* byte  ($FAE4) Frenzy once-per-fight @ 0xC3D8 */
#define MM2_GS_SHELTER_FLAG     (-0x796C)  /* byte  ($8694) Shelter @ 0xADA8; rest ambush gate 0x19D6E */
#define MM2_GS_PARTY_COUNT      (-0x795A)  /* word  ($86A6) active party size */
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
#define MM2_GS_COMBAT_XP_POOL       (-0x6FC6)  /* long  ($903A) fight XP accumulator (0x10B74/0x10E5E → 0x12430) */
#define MM2_GS_PICKER_TIER_MOD      (-0x6FC2)  /* byte  ($903E) = max(party level)/2, added to tier roll */
#define MM2_GS_PICKER_DONE          (-0x6FC1)  /* byte  ($903F) picker loop done flag (budget/gate exhausted) */
#define MM2_GS_MONSTER_FRIEND_COUNT (-0x11B7)  /* byte  ($6E47) picked monster's "friend count" (set by -$7EF6) */
#define MM2_GS_MONSTER_MULTIPLIES (-0x11A1) /* byte ($EE5F) Oabil bit7 @ 0x4E66; gate 0x10082 */
#define MM2_GS_ADDS_FRIENDS_LATCH (-0x5E4B) /* byte ($21B5) once-per-fight adds-friends @ 0x100E8 */
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
#define MM2_GS_LOOT_ITEM_TIER     (-0x5E29)  /* byte  ($A1D7) best treasure bits0-1 this fight (0x10B74) */
#define MM2_GS_LOOT_ITEM_TYPE_HI  (-0x5E28)  /* byte  ($A1D8) type>>4 when tier updates (0x10B74) */
#define MM2_GS_MONSTER_FLEE_PRINT (-0x5E27)  /* byte  ($A1D9) 1→" runs away!" else " goes down!" (0x10DFC) */

#define MM2_GS_EVENT_SCRIPT_START (-0x5C44)  /* word  ($A3BC) */
#define MM2_GS_QUEUED_EVENT_ID  (-0x5D46)  /* byte  ($A2BA) */
#define MM2_GS_STRING_WALK_INDEX (-0x5D44)  /* word  ($A2BC) */
/* OP_26/27 @ 0x16C70: writes 1-based slot to BOTH -$5D42 and -$5D3F.
 * OP_15 member-spec 9 @ 0x163CA reads -$5D42 first, falls back to -$5D3F.
 * OP_15 entry also snapshots incoming cond into -$5D3F (overwrites slot copy). */
#define MM2_GS_SELECTED_MEMBER  (-0x5D42)  /* byte  ($A2BE) primary selected slot 1..8 */
#define MM2_GS_SAVED_COND_FLAG  (-0x5D3F)  /* byte  ($A2C1) OP_15 cond snapshot / select mirror */
#define MM2_GS_PARTY_SELECT_CTX (-0x5D3F)  /* alias: same byte as SAVED_COND_FLAG */
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
#define MM2_GS_WIZARD_EYE_TIMER (-0x799F)  /* byte  ($8671) Wizard Eye steps (g=0x2C); was QUEST_COUNTER_B */
#define MM2_GS_EAGLE_EYE_TIMER  (-0x79A0)  /* byte  ($8670) Eagle Eye steps (g=0x2B); was CLASS_QUEST_CNT */
#define MM2_GS_QUEST_COUNTER_B  MM2_GS_WIZARD_EYE_TIMER  /* alias — keep OP_1A / fountain callers */
#define MM2_GS_CLASS_QUEST_CNT  MM2_GS_EAGLE_EYE_TIMER   /* alias */
#define MM2_GS_TALISMAN_BASE    (-0x79A4)  /* byte[4] ($866C) ids 0x27..0x2A; also set #1 @ 0xB3BE */
/* str.dat tip/rumor bank (0x9666 / -$7DE8): raw file @ -$ED6, bank offs @ -$71E8, cursor @ -$71EA */
#define MM2_GS_STR_DAT_PTR      (-0x0ED6)  /* long  ($F12A) loaded str.dat ($1E80) */
#define MM2_GS_STR_BANK_OFFS    (-0x71E8)  /* word[] ($8E18) bank start offsets into str.dat */
#define MM2_GS_STR_BANK_CURSOR  (-0x71EA)  /* word  ($8E16) next C-string offset in decode buf -$ED2 */
#define MM2_GS_STR_FILE_HANDLE  (-0x77CE)  /* long  ($8832) dos handle used when -$ED6 null */
/* OP_0E 0xFD @ 0x1493C: bank-3 next-string ptr tables (A4-relative i32 in remake). */
#define MM2_GS_OP0E_FD_PTR0     (-0x5E26)  /* long[4]  */
#define MM2_GS_OP0E_FD_PTR1     (-0x5E16)  /* long[4]  */
#define MM2_GS_OP0E_FD_PTR2     (-0x5E06)  /* long[14] */
#define MM2_GS_OP0E_FD_PTR3     (-0x5DCE)  /* long[4]  */
#define MM2_GS_OP0E_FD_PTR4     (-0x5DBE)  /* long[11] */
#define MM2_GS_OP0E_FD_PTR5     (-0x5D92)  /* long[10] */
#define MM2_GS_OP0E_FD_MODE     (-0x71DC)  /* byte  ($8E24) set $FD after table fill */
#define MM2_GS_OP0E_FD_CTR      (-0x7972)  /* word  addq @ 0x14112 (abort==3 / -$7ED2) */
/* -$7DDC key_read_scripted @ 0x97A2 (doc 44): replay buffer + cursors. */
#define MM2_GS_SCRIPTED_KEY_BUF (-0x119A)  /* byte[] key stream; $FF end/wrap */
#define MM2_GS_SCRIPTED_KEY_IDX (-0x1110)  /* word  read cursor; $FFFF = reset */
#define MM2_GS_SCRIPTED_KEY_REP (-0x71D6)  /* word  secondary/choreography cursor */
#define MM2_GS_SCRIPTED_KEY_DLY (-0x71DB)  /* byte  delay remaining (0x993e) */
#define MM2_GS_SCRIPTED_KEY_DY  (-0x71DA)  /* word  place arg2 default $40 → dst_x */
#define MM2_GS_SCRIPTED_KEY_DX  (-0x71D8)  /* word  place arg3 default $20 → dst_y-8 */
#define MM2_GS_SCRIPTED_KEY_MAXP (-0x1116) /* byte  placement clamp (sign load @ 0x320C) */
#define MM2_GS_SCRIPTED_KEY_MODE MM2_GS_OP0E_FD_MODE /* -$71DC: $FD wrap, $FF stop */
/* Tavern 0x1D208 bank-1 fill (A4-relative i32 ptrs in remake): */
#define MM2_GS_TAVERN_HDR       (-0x59EE)  /* long[5][4] town×4 menu headers */
#define MM2_GS_TAVERN_DRINK_LBL (-0x599E)  /* long[6] */
#define MM2_GS_TAVERN_MISC14    (-0x5986)  /* long[14] */
#define MM2_GS_TAVERN_RUMORS    (-0x594E)  /* long[5][8] town×8; stride $20 */
#define MM2_GS_TAVERN_TIPS      (-0x58AE)  /* long[5][8] town×8; stride $20 */
#define MM2_GS_TAVERN_BOOST_LBL (-0x580E)  /* long[6] stat-boost captions */
#define MM2_GS_TAVERN_FOOD      (-0x57F6)  /* long[5][6] town×6 food lines */
/* DATA hunk (not BSS): specialty/boost gold + limits + +$76 masks */
#define MM2_GS_TAVERN_SPECIALTY_GOLD (-0x6760) /* BE u16[5][3] @ 0x1CEA4 */
#define MM2_GS_TAVERN_BOOST_GOLD     (-0x6738) /* BE u16[6] @ 0x1CAC4 */
#define MM2_GS_TAVERN_BOOST_LIMIT    (-0x672C) /* BE u16[6] @ 0x1CBEA */
#define MM2_GS_TAVERN_SPECIALTY_MASK (-0x786C) /* BE u16[5][3] OR → +$76 */
#define MM2_GS_SPELL_ACTED      (-0x7958)  /* byte  ($86A8) spell-acted latch (0xD506 / stubs) */
#define MM2_GS_SPELL_DAMAGE     (-0x053E)  /* word  ($FAC2) 0x1338E damage accumulator */
#define MM2_GS_TEMPLE_DONATION  (-0x799E)  /* byte  ($8672) OP_0E temple quest bits */
/* Temple donate blessed path @ 0x1D7FE: writes light/protect/flags then addq.w #1.
 * Offset confirmed in ASM; semantic name is descriptive (not a bootstrap symbol). */
#define MM2_GS_TEMPLE_BLESS_CTR (-0x5770)  /* word  addq @ 0x1D852 after blessed buff */
#define MM2_GS_EVENT_VAR_BANK   (-0x798B)  /* byte[24] ($8685) g=0x00..0x17; hireling A..X unlock */
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
/* -$55D6 ($AA2A): CURRENT-CELL collision byte after hood refresh (0x1B1C),
 * NOT a 256-byte array. Rest/ambush/darkness/event-scan all read this single
 * byte. Source = collision page -$54BA[(y<<4)|x] (MapWorld collision in remake). */
#define MM2_GS_TILE_RT_FLAGS    (-0x55D6)  /* byte  ($AA2A) current-cell collision */
#define MM2_GS_TILE_TABLE_A     (-0x55BA)  /* byte[] ($AA46) visual page / hood */
#define MM2_GS_TILE_VISITED     (-0x54BA)  /* byte[256] ($AB46) collision page copy */
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
