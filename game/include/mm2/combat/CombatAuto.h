#pragma once
// Remake-only Full Auto combat policy (Tier 3). Not present in retail ASM —
// Ctrl-A @ 0x11A2A is the only quick path. Taxonomy mirrors CombatSession's
// resolvePlayerCast / kSorcCombat / kClerAutoAoE routing lists.

#include "mm2/GameState.h"
#include "mm2/gameplay/SpellBook.h"
#include "mm2_roster_codec.h"

#include <cstdint>

namespace mm2::combat {

class CombatSession;

enum class SpellAutoKind : uint8_t {
    UtilitySkip = 0,
    Heal,
    Cure,
    Buff,
    OffenseAoE,
    OffenseSingle,
};

enum class AutoAction : uint8_t {
    EndTurn = 0,
    StrikeMelee,
    StrikeShoot,
    Cast,
};

struct AutoDecision {
    AutoAction action = AutoAction::EndTurn;
    int flat = -1;           /* school flat 0..47 when Cast */
    int party_slot = -1;     /* heal/buff target party slot, or -1 */
    int monster_slot = -1;   /* letter-pick monster battle slot, or -1 (AoE) */
};

/** Remake QoL kind for school flat (Sorcerer or Cleric table index). */
SpellAutoKind spellAutoKind(gameplay::SpellSchool school, int flat0);

/** True if caster knows flat, has spell_level, and can afford SP (+gems). */
bool spellAutoAffordable(const Mm2RosterRecord &caster, gameplay::SpellSchool school, int flat0);

/**
 * Heal/cure → buff → offense → Ctrl-A strike policy for the active party slot.
 * `buff_latch` is a per-fight flag (set when Auto casts a buff).
 */
AutoDecision decideAuto(CombatSession &combat, GameStateView &gs, bool &buff_latch);

}  // namespace mm2::combat
