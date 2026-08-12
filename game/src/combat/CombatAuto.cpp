#include "mm2/combat/CombatAuto.h"

#include "mm2/combat/CombatSession.h"

#include "mm2_gamestate.h"

namespace mm2::combat {

namespace {

/* Condition bits used by Awaken / Cure Poison / Cure Disease masks. */
constexpr uint8_t kCondSleep = 0x10;
constexpr uint8_t kCondPoison = 0x08;
constexpr uint8_t kCondDisease = 0x04;

SpellAutoKind sorcKind(int flat0)
{
    switch (flat0) {
    case 36:
    case 39:
    case 41:
    case 42:
    case 45:
    case 46:
        return SpellAutoKind::OffenseAoE;
    case 2:
    case 3:
    case 6:
    case 8:
    case 9:
    case 14:
    case 17:
    case 18:
    case 20:
    case 21:
    case 22:
    case 26:
    case 27:
    case 28:
    case 31:
    case 33:
    case 35:
    case 40:
    case 44:
        return SpellAutoKind::OffenseSingle;
    case 16: /* Invisibility */
    case 24: /* Shield */
    case 43: /* Power Shield */
        return SpellAutoKind::Buff;
    default:
        return SpellAutoKind::UtilitySkip;
    }
}

SpellAutoKind clerKind(int flat0)
{
    switch (flat0) {
    case 3:  /* First Aid */
    case 5:  /* Power Cure */
    case 7:  /* Cure Wounds */
        return SpellAutoKind::Heal;
    case 1:  /* Awaken */
    case 16: /* Cure Poison */
    case 22: /* Cure Disease */
    case 30: /* Remove Condition */
        return SpellAutoKind::Cure;
    case 2:  /* Bless */
    case 8:  /* Heroism */
        return SpellAutoKind::Buff;
    case 0:
    case 13:
    case 27:
    case 29:
    case 38:
        return SpellAutoKind::OffenseAoE;
    case 10:
    case 12:
    case 14:
    case 17:
    case 20:
    case 26:
    case 34:
    case 36:
    case 37:
    case 40:
    case 42:
        return SpellAutoKind::OffenseSingle;
    default:
        return SpellAutoKind::UtilitySkip;
    }
}

int spellCostSp(const Mm2RosterRecord &caster, const gameplay::SpellMeta &meta)
{
    int sp = meta.sp;
    if (meta.per_level) {
        sp *= (caster.level > 0 ? caster.level : 1);
    }
    return sp;
}

bool allyNeedsCure(uint8_t cond, int flat)
{
    if (flat == 1) {
        return (cond & kCondSleep) != 0;
    }
    if (flat == 16) {
        return (cond & kCondPoison) != 0;
    }
    if (flat == 22) {
        return (cond & kCondDisease) != 0;
    }
    if (flat == 30) {
        return (cond & 0x7F) != 0 && cond < 0x80;
    }
    return false;
}

int healFlatPreference[] = {5, 7, 3}; /* Power Cure, Cure Wounds, First Aid */
int cureFlatPreference[] = {16, 22, 1, 30};

}  // namespace

SpellAutoKind spellAutoKind(gameplay::SpellSchool school, int flat0)
{
    if (flat0 < 0 || flat0 >= gameplay::kSpellsPerSchool) {
        return SpellAutoKind::UtilitySkip;
    }
    if (school == gameplay::SpellSchool::Sorcerer) {
        return sorcKind(flat0);
    }
    if (school == gameplay::SpellSchool::Cleric) {
        return clerKind(flat0);
    }
    return SpellAutoKind::UtilitySkip;
}

bool spellAutoAffordable(const Mm2RosterRecord &caster, gameplay::SpellSchool school, int flat0)
{
    if (!gameplay::spellKnownInBook(caster, flat0)) {
        return false;
    }
    const gameplay::SpellMeta *table = gameplay::schoolSpellTable(school);
    if (!table) {
        return false;
    }
    const gameplay::SpellMeta &meta = table[flat0];
    if (static_cast<int>(caster.spell_level) < static_cast<int>(meta.level)) {
        return false;
    }
    if (static_cast<int>(caster.sp_current) < spellCostSp(caster, meta)) {
        return false;
    }
    if (meta.gems != 0 && caster.gems < meta.gems) {
        return false;
    }
    return true;
}

AutoDecision decideAuto(CombatSession &combat, GameStateView &gs, bool &buff_latch)
{
    AutoDecision d{};
    bool melee = false, shoot = false, can_cast = false;
    combat.autoCommandFlags(melee, shoot, can_cast);

    const int active = combat.activePartySlot();
    if (active < 0) {
        d.action = AutoAction::EndTurn;
        return d;
    }

    const Mm2RosterRecord *caster = combat.autoRosterRecord(active);
    if (!caster) {
        d.action = AutoAction::EndTurn;
        return d;
    }

    const gameplay::SpellSchool school = gameplay::spellSchoolForClass(caster->class_id);
    const int live_mons = combat.autoAliveMonsterCount();

    auto strikeFallback = [&]() {
        if (shoot) {
            /* Ninja (class 5) and Robber (class 6) want Melee — assassinate/backstab */
            if (caster->class_id == 5 || caster->class_id == 6) {
                if (melee) {
                    d.action = AutoAction::StrikeMelee;
                } else {
                    d.action = AutoAction::EndTurn;
                }
            } else {
                d.action = AutoAction::StrikeShoot;
            }
        } else if (melee) {
            d.action = AutoAction::StrikeMelee;
        } else {
            d.action = AutoAction::EndTurn;
        }
        return d;
    };

    if (!can_cast || school == gameplay::SpellSchool::None) {
        return strikeFallback();
    }

    /* --- 1. Emergency cure --- */
    if (school == gameplay::SpellSchool::Cleric) {
        for (int fi = 0; fi < 4; ++fi) {
            const int flat = cureFlatPreference[fi];
            if (!spellAutoAffordable(*caster, school, flat)) {
                continue;
            }
            for (int p = 0; p < combat.autoPartyCount(); ++p) {
                const Mm2RosterRecord *ally = combat.autoRosterRecord(p);
                if (!ally || ally->hp_max <= 0 || ally->condition >= 0x80) {
                    continue;
                }
                if (allyNeedsCure(ally->condition, flat)) {
                    d.action = AutoAction::Cast;
                    d.flat = flat;
                    d.party_slot = p;
                    return d;
                }
            }
        }

        /* --- 1b. Heal any injured member (priority: lowest HP) --- */
        int worst_slot = -1;
        int worst_hp = 0x7fffffff;
        for (int p = 0; p < combat.autoPartyCount(); ++p) {
            const Mm2RosterRecord *ally = combat.autoRosterRecord(p);
            if (!ally || ally->hp_max <= 0 || ally->condition >= 0x80) {
                continue;
            }
            if (ally->hp_max >= ally->hp_current) {
                continue; /* not injured */
            }
            if (static_cast<int>(ally->hp_max) < worst_hp) {
                worst_hp = static_cast<int>(ally->hp_max);
                worst_slot = p;
            }
        }
        if (worst_slot >= 0) {
            for (int hi = 0; hi < 3; ++hi) {
                const int flat = healFlatPreference[hi];
                if (spellAutoAffordable(*caster, school, flat)) {
                    d.action = AutoAction::Cast;
                    d.flat = flat;
                    d.party_slot = worst_slot;
                    return d;
                }
            }
        }
    }

    /* --- 2. Buff (once per fight) --- */
    if (!buff_latch) {
        if (school == gameplay::SpellSchool::Cleric) {
            if (mm2_gs_u8(gs.a4(), MM2_GS_BLESS_COUNTER) == 0 &&
                spellAutoAffordable(*caster, school, 2)) {
                d.action = AutoAction::Cast;
                d.flat = 2; /* Bless — party-wide, no pick */
                buff_latch = true;
                return d;
            }
            if (spellAutoAffordable(*caster, school, 8)) {
                int best = -1;
                int best_might = -1;
                for (int p = 0; p < combat.autoPartyCount(); ++p) {
                    if (!combat.partySlotInFrontRank(p)) {
                        continue;
                    }
                    const Mm2RosterRecord *ally = combat.autoRosterRecord(p);
                    if (!ally || ally->hp_max <= 0 || ally->condition >= 0x80) {
                        continue;
                    }
                    if (static_cast<int>(ally->might_current) > best_might) {
                        best_might = static_cast<int>(ally->might_current);
                        best = p;
                    }
                }
                if (best >= 0) {
                    d.action = AutoAction::Cast;
                    d.flat = 8;
                    d.party_slot = best;
                    buff_latch = true;
                    return d;
                }
            }
        } else if (school == gameplay::SpellSchool::Sorcerer) {
            if (mm2_gs_u8(gs.a4(), MM2_GS_SHIELD_COUNTER) == 0 &&
                spellAutoAffordable(*caster, school, 24)) {
                d.action = AutoAction::Cast;
                d.flat = 24;
                buff_latch = true;
                return d;
            }
            if (mm2_gs_u8(gs.a4(), MM2_GS_POWER_SHIELD_CTR) == 0 &&
                spellAutoAffordable(*caster, school, 43)) {
                d.action = AutoAction::Cast;
                d.flat = 43;
                buff_latch = true;
                return d;
            }
            if (mm2_gs_u8(gs.a4(), MM2_GS_INVIS_COUNTER) == 0 &&
                spellAutoAffordable(*caster, school, 16)) {
                d.action = AutoAction::Cast;
                d.flat = 16;
                buff_latch = true;
                return d;
            }
        }
    }

    /* --- 3. Offense --- */
    if (live_mons > 0) {
        int best_flat = -1;
        int best_level = -1;
        const bool prefer_aoe = live_mons >= 3;

        for (int flat = 0; flat < gameplay::kSpellsPerSchool; ++flat) {
            const SpellAutoKind kind = spellAutoKind(school, flat);
            if (kind != SpellAutoKind::OffenseAoE && kind != SpellAutoKind::OffenseSingle) {
                continue;
            }
            if (prefer_aoe && kind != SpellAutoKind::OffenseAoE) {
                continue;
            }
            if (!prefer_aoe && kind != SpellAutoKind::OffenseSingle) {
                continue;
            }
            if (!spellAutoAffordable(*caster, school, flat)) {
                continue;
            }
            const gameplay::SpellMeta *table = gameplay::schoolSpellTable(school);
            const int lvl = table ? static_cast<int>(table[flat].level) : 0;
            if (lvl > best_level) {
                best_level = lvl;
                best_flat = flat;
            }
        }

        /* If prefer_aoe found nothing, fall back to single-target. */
        if (best_flat < 0 && prefer_aoe) {
            for (int flat = 0; flat < gameplay::kSpellsPerSchool; ++flat) {
                if (spellAutoKind(school, flat) != SpellAutoKind::OffenseSingle) {
                    continue;
                }
                if (!spellAutoAffordable(*caster, school, flat)) {
                    continue;
                }
                const gameplay::SpellMeta *table = gameplay::schoolSpellTable(school);
                const int lvl = table ? static_cast<int>(table[flat].level) : 0;
                if (lvl > best_level) {
                    best_level = lvl;
                    best_flat = flat;
                }
            }
        }

        if (best_flat >= 0) {
            d.action = AutoAction::Cast;
            d.flat = best_flat;
            if (spellAutoKind(school, best_flat) == SpellAutoKind::OffenseSingle) {
                d.monster_slot = combat.autoFirstAliveMonster();
            }
            return d;
        }
    }

    return strikeFallback();
}

}  // namespace mm2::combat
