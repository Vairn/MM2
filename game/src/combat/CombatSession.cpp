#include "mm2/combat/CombatSession.h"

#include "mm2/CppStdCompat.h"
#include "mm2/combat/EncounterPicker.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/TownServiceTransactions.h"
#include "mm2/gameplay/SpellBook.h"
#include "mm2/platform/Audio.h"
#include "mm2_create_character.h"

#include <cstring>

namespace mm2::combat {

namespace {

/* Initiative (-$50E turn-order stat) is sourced by the unpacker 0x4C8E from
 * byte 0x18 "speed2": -$11B1 = (speed2 & 0x1F)+1, ×10 (capped 250) if bit5
 * (0x4FA2-0x4FD4), copied to -$50E[slot] at 0x11C68. Byte 0x14 "speed" is the
 * SWINGS/charge byte (low4+1 / high4+1), NOT initiative — see loadMonsterCombatGlobals. */
int decodeMonsterSpeed(const Mm2MonsterRecord &rec)
{
    const uint8_t b = rec.fields[MM2_MON_OFF_SPEED2 - MM2_MONSTER_NAME_SIZE];
    int v = (b & 0x1F) + 1;
    if (b & 0x20) {
        v = (v > 0x19) ? 0xFA : v * 10;
    }
    return v;
}

/* doc 16: byte 0x17 damage = low5+1, x10 if bit5, capped 250. Used by
 * 0x4C8E → -$11B3 (monster melee sides @ 0x10478). */
int decodeMonsterMaxDamage(const Mm2MonsterRecord &rec)
{
    const uint8_t b = rec.fields[MM2_MON_OFF_DAMAGE - MM2_MONSTER_NAME_SIZE];
    int dmg = ((b & 0x1F) + 1) * ((b & 0x20) ? 10 : 1);
    return dmg > 250 ? 250 : dmg;
}

/* FriendCountLookup for encounterAddsFriends (0x11F0A / -$7EF6): monsters.dat
 * Oabil low nibble via MM2_MON_ADD_FRIENDS_COUNT. */
uint8_t friendCountFromMonsters(uint8_t type, const void *ctx)
{
    const auto *monsters = static_cast<const Mm2MonstersFile *>(ctx);
    if (!monsters) {
        return 1;
    }
    Mm2MonsterAbilities ab{};
    mm2_monster_decode_abilities(&monsters->records[type], &ab);
    const int count = MM2_MON_ADD_FRIENDS_COUNT(&ab);
    return static_cast<uint8_t>(count > 255 ? 255 : count);
}

}  // namespace

void CombatSession::setStatus(const char *msg)
{
    std::snprintf(status_line_, sizeof(status_line_), "%s", msg ? msg : "");
}

int CombatSession::rosterIndexForPartySlot(int party_slot) const
{
    if (!launch_ || party_slot < 0 || party_slot >= party_count_ ||
        party_slot >= MM2_GS_PARTY_SIZE) {
        return -1;
    }
    return launch_->roster_slots[party_slot];
}

bool CombatSession::partySlotAlive(int party_slot) const
{
    const int idx = rosterIndexForPartySlot(party_slot);
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT || !roster_) {
        return false;
    }
    /* Working HP is roster +$5E (codec hp_max). Combat damage @ 0x4AAA
     * subtracts from +$5E and clears it on KO; +$74 is the HP ceiling. */
    return roster_->records[idx].hp_max > 0;
}

bool CombatSession::partySlotInFight(int party_slot) const
{
    const int idx = rosterIndexForPartySlot(party_slot);
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT || !roster_) {
        return false;
    }
    /* combat_round_end_check @ 0x13282: cmpi.b #$10, $26(a0); bhi skips.
     * Also require working HP (+$5E) — KO clears it and sets condition bit6. */
    return roster_->records[idx].hp_max > 0 && roster_->records[idx].condition <= 0x10;
}

bool CombatSession::partySlotEligibleForRewards(int party_slot) const
{
    const int idx = rosterIndexForPartySlot(party_slot);
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT || !roster_) {
        return false;
    }
    /* combat_victory_rewards @ 0x12430: cmp.w #$80, d0; bcc skips dead/stoned. */
    return roster_->records[idx].condition < 0x80;
}

int CombatSession::firstAliveMonster() const
{
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (slots_[i].alive) {
            return i;
        }
    }
    return -1;
}

void CombatSession::syncMonsterSlotsToGs(GameStateView &gs) const
{
    uint8_t *a4 = gs.a4();
    if (!a4) {
        return;
    }
    int alive = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        const int hp = slots_[i].hp < 0 ? 0 : slots_[i].hp;
        mm2_gs_set_u16(a4, MM2_GS_MONSTER_HP + i * 2, static_cast<uint16_t>(hp));
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_STATUS + i, slots_[i].alive ? slots_[i].status : 0);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SPEED + i, static_cast<uint8_t>(slots_[i].speed));
        if (slots_[i].alive) {
            ++alive;
        }
    }
    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, static_cast<uint8_t>(alive));
}

bool CombatSession::enter(GameStateView &gs, const world::MapWorld &world)
{
    if (!gs.valid() || !roster_ || !launch_ || !monsters_ || !rng_) {
        return false; /* GAP: not fully bound — do not fabricate a fight. */
    }

    saved_panel_mode_ = gs.rightPanelMode();
    gs.setRightPanelMode(2); /* 0x12C6E: A4-$79B2 := 2 (combat panel). */
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 0);

    /* 0x12CE0: recompute live count from script-seeded slots *before* the
     * random picker; 0x12CFE: jsr 0x1213E only when mode < $80; else subi #$80
     * (so OP_0E FD mode $83 → fixed fight, then mode becomes $03). */
    const uint8_t mode = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE);
    encounterRecomputeLiveCount(gs);
    if (mode < 0x80) {
        encounterInitXpBudget(gs, *roster_, *launch_);
        encounterRunRandomPicker(gs, world.attrib(), *rng_, &friendCountFromMonsters, monsters_);
    } else {
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE, static_cast<uint8_t>(mode - 0x80));
    }
    const uint8_t live = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT);
    if (live == 0) {
        /* Random picker with a zero XP budget (or a fixed fight with empty
         * slots) must not open a 0-monster combat UI — restore the prior
         * panel and refuse the fight. */
        gs.setRightPanelMode(saved_panel_mode_);
        state_ = CombatState::Inactive;
        outcome_ = CombatOutcome::None;
        return false;
    }
    encounter_live_total_ = live;
    overflow_type_ = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_OVERFLOW_TYPE);
    const int slot_count = live < MM2_GS_MONSTER_BATTLE_SLOTS ? live : MM2_GS_MONSTER_BATTLE_SLOTS;
    const uint8_t overflow_type = overflow_type_;

    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        monster_acted_[i] = false;
        if (i >= slot_count) {
            slots_[i] = CombatMonster{};
            continue;
        }
        const uint8_t type = (i < MM2_GS_MONSTER_SLOT_COUNT) ? mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + i)
                                                              : overflow_type;
        const Mm2MonsterRecord &rec = monsters_->records[type];
        CombatMonster &m = slots_[i];
        m.type = type;
        m.max_hp = static_cast<int>(mm2_monster_decode_hp(&rec));
        m.hp = m.max_hp;
        m.speed = decodeMonsterSpeed(rec);
        m.alive = m.max_hp > 0;
        m.status = m.alive ? 1 : 0; /* bit0 awake */
    }
    /* 0x4C8E unpack: treasure byte 0x10 bits 5/6/7 → A4-$11C2/$11C1/$11C0 (bribe gates).
     * Accumulate across live slots (addq.b per monster). */
    uint8_t bribe_food = 0, bribe_gold = 0, bribe_gems = 0;
    for (int i = 0; i < slot_count; ++i) {
        if (!slots_[i].alive) {
            continue;
        }
        const Mm2MonsterRecord &rec = monsters_->records[slots_[i].type];
        const uint8_t treasure = rec.fields[MM2_MON_OFF_TREASURE - MM2_MONSTER_NAME_SIZE];
        if (treasure & 0x20) {
            ++bribe_food;
        }
        if (treasure & 0x40) {
            ++bribe_gold;
        }
        if (treasure & 0x80) {
            ++bribe_gems;
        }
    }
    mm2_gs_set_u8(gs.a4(), -0x11C2, bribe_food);
    mm2_gs_set_u8(gs.a4(), -0x11C1, bribe_gold);
    mm2_gs_set_u8(gs.a4(), -0x11C0, bribe_gems);
    for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
        const int idx = rosterIndexForPartySlot(i);
        if (idx >= 0) {
            mm2_roster_refresh_spell_points(&roster_->records[idx]);
        }
    }
    for (int i = 0; i < MM2_GS_PARTY_SIZE; ++i) {
        party_acted_[i] = false;
        party_blocking_[i] = false;
    }

    /* 0x51C2 combat_encounter_entry GS: clear per-fight buff counters
     * (-$799D..-$7999 Bless/Invis/Shield/PowerShield/HolyBonus). */
    mm2_gs_set_u8(gs.a4(), MM2_GS_BLESS_COUNTER, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_INVIS_COUNTER, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SHIELD_COUNTER, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_POWER_SHIELD_CTR, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_HOLY_BONUS_CTR, 0);

    /* 0x521E..0x5252: 0x4476 sync current→base, then +$27→+$73 / +$15→+$70. */
    for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
        const int idx = rosterIndexForPartySlot(i);
        if (idx < 0) {
            continue;
        }
        Mm2RosterRecord &rec = roster_->records[idx];
        auto *raw = reinterpret_cast<uint8_t *>(&rec);
        /* Remake keeps canonical spell_level at +$72; Amiga base is +$23.
         * Seed +$23 so 0x4476's +$23→+$72 mirror preserves it. */
        raw[0x23] = rec.spell_level;
        /* 0x4476 sync_party_secondary_stats */
        raw[0x6B] = raw[0x10]; /* might */
        raw[0x6F] = raw[0x14]; /* accuracy */
        raw[0x6D] = raw[0x12]; /* personality */
        raw[0x6C] = raw[0x11]; /* intelligence */
        raw[0x71] = raw[0x20];
        raw[0x72] = raw[0x23];
        raw[0x6E] = raw[0x13]; /* speed */
        /* 0x5244 / 0x5252 */
        raw[0x73] = raw[0x27]; /* endurance_current → endurance_base */
        raw[0x70] = raw[0x15]; /* luck_current → luck_base */
    }

    xp_pool_ = 0;
    mm2_gs_set_u32(gs.a4(), MM2_GS_COMBAT_XP_POOL, 0); /* 0x12CB4 clr.l -$6FC6 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TYPE_HI, 0); /* 0x12C80 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER, 0);    /* 0x12C84 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLEE_PRINT, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_ADDS_FRIENDS_LATCH, 0); /* 0x12C94 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_ENCASE_TIER, 0);        /* 0x12CA0 -$521 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_ENTRAPMENT, 0);         /* 0x12CA4 -$522 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_TIME_DISTORT, 0);       /* 0x12CA8 -$523 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_RAN_LATCH, 0);    /* 0x12C88 -$5E4C */
    msg_queue_len_ = 0;
    msg_queue_idx_ = 0;
    outcome_ = CombatOutcome::None;
    round_layout_active_ = false;
    active_party_slot_ = -1;
    active_monster_slot_ = -1;
    melee_range_count_ = 0;
    front_rank_count_ = 0;
    mm2_gs_set_u8(gs.a4(), MM2_GS_MELEE_RANGE_N, 0); /* 0x12C90 -$524 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_FRONT_RANK_N, 0);  /* 0x12C8C -$5E4D */
    surprise_mode_ = 0;
    seedCombatStaticTables(gs);
    /* 0x923E buffer: flee thresh is attrib 0x0D at -$560D (even if MapWorld
     * was not file-loaded — tests synthesize attrib bytes). */
    mm2_gs_set_u8(gs.a4(), MM2_GS_RETREAT_DIFF, world.attrib().raw[0x0D]);
    syncMonsterSlotsToGs(gs);
    /* 0x11C82/0x11C2C: seed -$503[slot] from -$11BA after 0x4C8E unpack. */
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + i, 0);
    }
    for (int i = 0; i < slot_count; ++i) {
        if (!slots_[i].alive) {
            continue;
        }
        loadMonsterCombatGlobals(gs, i);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + i,
                      mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_CHARGE_INIT));
    }
    if (launch_) {
        party_count_ = launch_->party_count;
        if (party_count_ < 0) {
            party_count_ = 0;
        }
        if (party_count_ > MM2_GS_PARTY_SIZE) {
            party_count_ = MM2_GS_PARTY_SIZE;
        }
        mm2_gs_set_u16(gs.a4(), MM2_GS_PARTY_COUNT, static_cast<uint16_t>(party_count_));
        for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
            mm2_gs_set_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + i * 2,
                           static_cast<uint16_t>(launch_->roster_slots[i]));
        }
    } else {
        party_count_ = 0;
    }

    beginEncounterUi(gs, world);
    /* 0x51C2 combat_encounter_entry (-$7EDE): play_sound_seq id 2 @ 0x51D8/0x51DC
     * ("oh noes" — WAV 02_phrase_short.wav). */
    audio::playSoundSeq(2, gs.soundsEnabled(), gs.walkBeepEnabled());
    return true;
}

void CombatSession::applySurpriseRoll(GameStateView &gs)
{
    /* Encounter-setup surprise @ 0x12EE2: rng(1,100) vs front-rank speed and
     * levitate; mode 2 = party surprised, 3 = monsters surprised. */
    const int roll = rng_->range(1, 100);
    int front_speed = 10;
    for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
        if (!partySlotAlive(i)) {
            continue;
        }
        const int idx = rosterIndexForPartySlot(i);
        front_speed = roster_->records[idx].speed_current;
        break;
    }
    if (roll <= 40 && roll <= front_speed) {
        surprise_mode_ = 2;
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE, 2);
    } else if (gs.levitateFlag() == 0 && roll >= 90) {
        surprise_mode_ = 3;
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE, 3);
    }
}

void CombatSession::beginEncounterUi(GameStateView &gs, const world::MapWorld &world)
{
    const uint8_t mode = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE);
    if (mode == 3) {
        surprise_mode_ = 3;
        setStatus("The monsters are surprised!");
        state_ = CombatState::AwaitingSurpriseDismiss;
        return;
    }

    if (mode == 0) {
        applySurpriseRoll(gs);
    }

    if (surprise_mode_ == 2) {
        setStatus("The monsters surprised you!");
        state_ = CombatState::AwaitingSurpriseDismiss;
        return;
    }
    if (surprise_mode_ == 3) {
        setStatus("The monsters are surprised!");
        state_ = CombatState::AwaitingSurpriseDismiss;
        return;
    }

    state_ = CombatState::AwaitingPartyOptions;
    setStatus("");
    (void)world;
}

void CombatSession::recomputeRangeCounts(GameStateView &gs)
{
    /* combat_state_recompute @ 0x11D0C — run once at round-loop entry (0x12A3C). */
    const uint8_t view_mode = mm2_gs_u8(gs.a4(), MM2_GS_VIEW_MODE);
    const uint8_t handicap = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE);
    const int party_count = launch_ ? party_count_ : 0;

    /* A4-$524: monsters within melee reach. 0x11D10: if already nonzero, skip
     * the roll (keep prior base) but still apply surprise handicaps below. */
    int melee = static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_MELEE_RANGE_N));
    if (melee == 0) {
        /* Indoor rng(10,39)/10+3 = 4..6; outdoor party/2 + rng(10,69)/10
         * (0x11D1C / 0x11D38). */
        if (view_mode == 0) {
            melee = rng_->range(10, 39) / 10 + 3;
        } else {
            melee = party_count / 2 + rng_->range(10, 69) / 10;
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_MELEE_RANGE_N, static_cast<uint8_t>(melee)); /* 0x11D60 */
    }
    if (handicap == 2) {
        melee /= 2; /* 0x11D66 party surprised */
    } else if (handicap == 3) {
        melee *= 2; /* 0x11D82 monsters surprised */
    }
    int alive = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (slots_[i].alive) {
            ++alive;
        }
    }
    const int live = encounter_live_total_ > alive ? encounter_live_total_ : alive;
    if (melee > live) {
        melee = live; /* 0x11D98 clamp to -$77BE */
    }
    if (melee > 10) {
        melee = 10; /* 0x11DA8 */
    }
    melee_range_count_ = melee;
    mm2_gs_set_u8(gs.a4(), MM2_GS_MELEE_RANGE_N, static_cast<uint8_t>(melee)); /* 0x11DB4 -$524 */

    /* A4-$5E4D: party front-rank cutoff. Indoor (rng(10,79)/10+3)>>1 = 2..5;
     * outdoor party count, or rng/2 + party - 2 when >= 6 (0x11DBC / 0x11DDE). */
    int front;
    if (view_mode == 0) {
        front = (rng_->range(10, 79) / 10 + 3) >> 1;
    } else {
        front = party_count;
        if (front >= 6) {
            front = (rng_->range(10, 39) / 10) / 2 + party_count - 2;
        }
    }
    if (handicap == 3) {
        front *= 2; /* 0x11E14 */
    }
    if (handicap == 2) {
        if (party_count < 2) {
            front = 2;
        }
        front -= 1; /* 0x11E2A..0x11E40 */
    }
    if (front > party_count) {
        front = party_count; /* 0x11E44 clamp to -$7959 */
    }
    front_rank_count_ = front;
    mm2_gs_set_u8(gs.a4(), MM2_GS_FRONT_RANK_N, static_cast<uint8_t>(front > 0 ? front : 0));
}

void CombatSession::commandFlagsForActiveSlot(bool &melee, bool &shoot, bool &cast) const
{
    /* combat_command_bar_build @ 0x11866 capability flags -$5E36/-$5E35/-$5E34. */
    melee = false;
    shoot = false;
    cast = false;
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    if (idx < 0) {
        return;
    }
    const Mm2RosterRecord &rec = roster_->records[idx];
    melee = active_party_slot_ < front_rank_count_;
    /* +$4E bow flag; shoot requires class 2 (Archer) OR back rank (0x118CC..0x118E8). */
    const bool bow = rec.spells[0x4E - 0x4C] != 0;
    shoot = (rec.class_id == 2 || active_party_slot_ >= front_rank_count_) && bow;
    /* not silenced (+$26 bit1); +$72 spell-level after 0x4476; SP +$58. */
    cast = (rec.condition & 0x02) == 0 && rec.spell_level != 0 && rec.sp_current > 0;
}

void CombatSession::startRoundLoop(GameStateView &gs, const world::MapWorld &world)
{
    /* 0x12A22 → 0x135BE chrome; 0x11D0C range roll once per fight (not each round). */
    round_layout_active_ = true;
    recomputeRangeCounts(gs);
    beginRound(gs); /* 0x12A6A: clr acted / status lottery / seed -$4F8 */
    runUntilDecisionOrEnd(gs, world);
}

void CombatSession::resolvePartyAttack(GameStateView &gs, const world::MapWorld &world)
{
    startRoundLoop(gs, world);
}

void CombatSession::resolvePartyRun(GameStateView &gs, const world::MapWorld &world)
{
    /* 0x1312A..0x1315A: ENCOUNTER_MODE==2 → leave; else rng(1,100) < -$560D
     * (attrib 0x0D) → 0x11646; fail → force Attack. */
    const uint8_t mode = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE);
    bool ok = (mode == 2);
    if (!ok) {
        const uint8_t thresh = mm2_gs_u8(gs.a4(), MM2_GS_RETREAT_DIFF);
        const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
        ok = (roll < static_cast<int>(thresh));
    }
    if (ok) {
        finishLeave(gs, true);
        return;
    }
    setStatus("The party fails to run!");
    resolvePartyAttack(gs, world);
}

void CombatSession::resolvePartyHide(GameStateView &gs, const world::MapWorld &world)
{
    /* 0x13104: rng(1,100) from party-options entry vs A4-$5E4E (0x12D42 ← -$7EFC→0x4B60).
     * 0x4B60: mean of party +$1E thievery, divu by -$795A, cap $FF. Success → leave. */
    (void)world;
    const int roll = rng_ ? rng_->range(1, 100) : 1;
    int sum = 0;
    int n = 0;
    for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
        if (!partySlotAlive(i)) {
            continue;
        }
        sum += roster_->records[rosterIndexForPartySlot(i)].thievery_percent;
        ++n;
    }
    int thresh = (n > 0) ? (sum / n) : 0;
    if (thresh > 0xFF) {
        thresh = 0xFF;
    }
    if (roll < thresh) {
        finishLeave(gs, true);
        setStatus("Success!");
        return;
    }
    setStatus("You failed to hide!");
    state_ = CombatState::AwaitingPartyOptions;
}

void CombatSession::beginBribeSubmenu(GameStateView &gs)
{
    /* 0x12FB8 after B at party options: latch rng + attrib demand, then kind UI. */
    bribe_kind_ = 0;
    bribe_digits_ = 0;
    bribe_amount_ = 0;
    bribe_roll_ = static_cast<uint8_t>(rng_ ? rng_->range(1, 100) : 1);
    /* 0x12F58: move.b A4-$5609 → demand; -$5609 = attrib buf+0x11. */
    bribe_demand_ = mm2_gs_u8(gs.a4(), MM2_GS_ATTRIB_BUF + 0x11);
    state_ = CombatState::AwaitingBribeKind;
    setStatus("");
}

bool CombatSession::tickBribeSubmenu(GameStateView &gs, char key, bool escape)
{
    if (state_ == CombatState::AwaitingBribeKind) {
        if (escape || key == 0x1B) {
            state_ = CombatState::AwaitingPartyOptions;
            setStatus("");
            return true;
        }
        if (key < '1' || key > '3') {
            return false;
        }
        bribe_kind_ = key - '0';
        bribe_digits_ = 0;
        bribe_amount_ = 0;
        state_ = CombatState::AwaitingBribeAmount;
        setStatus("");
        return true;
    }
    if (state_ != CombatState::AwaitingBribeAmount) {
        return false;
    }
    if (escape || key == 0x1B) {
        state_ = CombatState::AwaitingPartyOptions;
        setStatus("");
        return true;
    }
    /* 0x3EE0: up to 4 decimal digits; Return/Enter commits (ASM uses length arg=4). */
    if (key >= '0' && key <= '9' && bribe_digits_ < 4) {
        bribe_amount_ = static_cast<uint16_t>(bribe_amount_ * 10u + static_cast<uint16_t>(key - '0'));
        ++bribe_digits_;
        return false;
    }
    if (key != '\r' && key != '\n' && key != ' ') {
        return false;
    }
    if (bribe_digits_ == 0 || bribe_amount_ == 0) {
        state_ = CombatState::AwaitingPartyOptions;
        setStatus("");
        return true;
    }
    resolveBribeTry(gs);
    return true;
}

void CombatSession::resolveBribeTry(GameStateView &gs)
{
    /* 0x1301C..0x130FE: pay food/gold/gems; success → leave (S / -$77BD). */
    const uint8_t amount_lo = static_cast<uint8_t>(bribe_amount_ & 0xFF);
    if (bribe_kind_ != 2) {
        /* Non-gold: amount_lo must be >= monster type at -$11DE[0]. */
        const uint8_t mon0 = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS);
        if (amount_lo < mon0) {
            setStatus("Your bribe is refused!");
            state_ = CombatState::AwaitingPartyOptions;
            return;
        }
    }

    bool paid = false;
    uint8_t gate = 0;
    if (bribe_kind_ == 1) {
        gate = mm2_gs_u8(gs.a4(), -0x11C2);
        paid = gate != 0 && bribe_roll_ >= bribe_demand_ &&
               events::eventVmPartyTryPayFood(gs.a4(), roster_, launch_, amount_lo);
    } else if (bribe_kind_ == 2) {
        gate = mm2_gs_u8(gs.a4(), -0x11C1);
        paid = gate != 0 && bribe_roll_ >= bribe_demand_ &&
               events::eventVmPartyTryPayGold(gs.a4(), roster_, launch_, bribe_amount_);
    } else if (bribe_kind_ == 3) {
        gate = mm2_gs_u8(gs.a4(), -0x11C0);
        paid = gate != 0 && bribe_roll_ >= bribe_demand_ &&
               events::eventVmPartyTryPayGems(gs.a4(), roster_, launch_, bribe_amount_);
    }

    if (paid) {
        finishLeave(gs, true);
        setStatus("Success!");
        return;
    }
    setStatus("Your bribe is refused!");
    state_ = CombatState::AwaitingPartyOptions;
}

/* 0x132E6 → -$7E84 (0x6798): arg = DELAY*$19+1; half = max(1, arg/2);
 * loop does delay(1) then tests the pre-decrement count, so hold = half+1
 * frames (Delay 0 → 2 VBLs). Any key skips (poll -$7BD2). */
static int combatActionAckFrames(const GameStateView &gs)
{
    const int arg = static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_DELAY)) * 0x19 + 1;
    int half = arg / 2;
    if (half < 1) {
        half = 1;
    }
    return half + 1;
}

void CombatSession::beginActionAck(GameStateView &gs)
{
    /* 0x132E6: d0 = DELAY(-$79AD)*$19 + 1; jsr -$7E84 timed wait. */
    for (int i = 0; i < 10; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + i, monster_acted_[i] ? 1 : 0);
    }
    for (int i = 0; i < MM2_GS_PARTY_SIZE; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_ACTED + i, party_acted_[i] ? 1 : 0);
    }
    ack_frames_left_ = combatActionAckFrames(gs);
    state_ = CombatState::AwaitingActionAck;
}

bool CombatSession::tick(GameStateView &gs, const world::MapWorld &world, const platform::KeyState &keys)
{
    if (state_ == CombatState::AwaitingActionAck) {
        /* Key skip matches 0x6798; otherwise burn one hold frame per tick.
         * Old logic decremented then advanced when hitting 0 on the same tick,
         * so Delay 0 (1 programmed frame) never left the message on screen. */
        const bool keyed = keys.last_ascii != 0 || keys.space || keys.enter;
        if (!keyed && ack_frames_left_ > 0) {
            --ack_frames_left_;
            return false;
        }
        ack_frames_left_ = 0;
        /* 0x132E6 host: pace multi-target spell lines one ack each. */
        if (advanceCombatMessageQueue()) {
            ack_frames_left_ = combatActionAckFrames(gs);
            return false;
        }
        runUntilDecisionOrEnd(gs, world);
        return state_ == CombatState::Inactive;
    }

    if (state_ == CombatState::AwaitingSurpriseDismiss) {
        if (keys.last_ascii == 0) {
            return false;
        }
        if (surprise_mode_ == 3) {
            startRoundLoop(gs, world);
        } else {
            state_ = CombatState::AwaitingPartyOptions;
            setStatus("");
        }
        return state_ == CombatState::Inactive;
    }

    if (state_ == CombatState::AwaitingPartyOptions) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch != 'A' && ch != 'B' && ch != 'H' && ch != 'R') {
            return false;
        }
        switch (ch) {
        case 'A':
            resolvePartyAttack(gs, world);
            break;
        case 'B':
            beginBribeSubmenu(gs);
            break;
        case 'H':
            resolvePartyHide(gs, world);
            break;
        case 'R':
            resolvePartyRun(gs, world);
            break;
        default:
            break;
        }
        return state_ == CombatState::Inactive;
    }

    if (state_ == CombatState::AwaitingBribeKind || state_ == CombatState::AwaitingBribeAmount) {
        const char ch = static_cast<char>(keys.last_ascii);
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (!tickBribeSubmenu(gs, ch, keys.escape)) {
            return false;
        }
        return state_ == CombatState::Inactive;
    }

    if (state_ == CombatState::AwaitingCastLevel || state_ == CombatState::AwaitingCastNumber) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (keys.escape || ch == 0x1B) {
            cast_level_ = 0;
            pending_cast_flat_ = -1;
            state_ = CombatState::AwaitingCommand;
            setStatus("");
            return false;
        }
        if (!tickCastPicker(gs, ch)) {
            return false;
        }
        if (state_ == CombatState::AwaitingCastLevel || state_ == CombatState::AwaitingCastNumber ||
            state_ == CombatState::AwaitingCastTarget || state_ == CombatState::AwaitingPartyPick ||
            state_ == CombatState::AwaitingItemPick) {
            return false;
        }
        if (state_ == CombatState::AwaitingActionAck) {
            party_acted_[active_party_slot_] = true;
            active_party_slot_ = -1;
        }
        return false;
    }

    if (state_ == CombatState::AwaitingCastTarget) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (keys.escape || ch == 0x1B) {
            /* 0xD4F8: ESC → no -$7958, return to command. */
            pending_cast_flat_ = -1;
            cast_target_slot_ = -1;
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0);
            state_ = CombatState::AwaitingCommand;
            setStatus("");
            return false;
        }
        if (!tickCastTarget(gs, ch)) {
            return false;
        }
        if (state_ == CombatState::AwaitingActionAck) {
            party_acted_[active_party_slot_] = true;
            active_party_slot_ = -1;
        }
        return false;
    }

    if (state_ == CombatState::AwaitingAttackTarget) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (keys.escape || ch == 0x1B) {
            /* 0x112C4: ESC cancels Fight/Shoot picker → re-prompt command. */
            attack_pick_shooting_ = false;
            state_ = CombatState::AwaitingCommand;
            setStatus("");
            return false;
        }
        if (!tickAttackTarget(gs, ch)) {
            return false;
        }
        if (state_ == CombatState::AwaitingActionAck) {
            party_acted_[active_party_slot_] = true;
            active_party_slot_ = -1;
        }
        return false;
    }

    if (state_ == CombatState::AwaitingPartyPick) {
        const char ch = static_cast<char>(keys.last_ascii);
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (keys.escape || ch == 0x1B) {
            pending_cast_flat_ = -1;
            pending_cast_school_ = -1;
            cast_aux_ = -1;
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0);
            state_ = CombatState::AwaitingCommand;
            setStatus("");
            return false;
        }
        if (!tickPartyPick(gs, ch)) {
            return false;
        }
        if (state_ == CombatState::AwaitingActionAck) {
            party_acted_[active_party_slot_] = true;
            active_party_slot_ = -1;
        }
        return false;
    }

    if (state_ == CombatState::AwaitingItemPick) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (keys.escape || ch == 0x1B) {
            pending_cast_flat_ = -1;
            pending_cast_school_ = -1;
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0);
            mm2_gs_set_u16(gs.a4(), -0x3F0A, 0);
            mm2_gs_set_u16(gs.a4(), -0x3F08, 0);
            state_ = CombatState::AwaitingCommand;
            setStatus("");
            return false;
        }
        if (!tickItemPick(gs, ch)) {
            return false;
        }
        if (state_ == CombatState::AwaitingActionAck) {
            party_acted_[active_party_slot_] = true;
            active_party_slot_ = -1;
        }
        return false;
    }

    if (state_ == CombatState::AwaitingExchangeWith) {
        const char ch = static_cast<char>(keys.last_ascii);
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (!tickExchange(gs, ch, keys.escape || ch == 0x1B)) {
            return false;
        }
        if (state_ == CombatState::AwaitingCommand) {
            return false; /* ESC — re-prompt same turn (0x11C24 → 0x119F4) */
        }
        /* Swap done: acted flags already exchanged; do not re-stamp acted. */
        active_party_slot_ = -1;
        beginActionAck(gs);
        return false;
    }

    if (state_ != CombatState::AwaitingCommand) {
        return false;
    }

    /* combat_read_command_key @ 0x1175C + dispatch @ 0x11BD0:
     *   A → $1162C(arg=0)  Attack first target, no picker
     *   F → $1162C(arg=$FF) Fight picker (auto if 1 live)
     *   S → $11610(arg=$FF) Shoot picker
     *   Ctrl-A ($01) → shoot if -$5E35 else melee if -$5E36 else end turn
     * B/R always; A/F gated on melee; S on shoot; C on cast. */
    bool can_melee = false, can_shoot = false, can_cast = false;
    commandFlagsForActiveSlot(can_melee, can_shoot, can_cast);

    const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));

    /* Ctrl-A @ 0x11A2A: quick strike — Shoot if able, else Attack, else Block.
     * Retail key code $01; SDL delivers ctrl+'A'. */
    if ((keys.ctrl && ch == 'A') || keys.last_ascii == 1) {
        if (can_shoot) {
            beginPlayerStrike(gs, true, false);
        } else if (can_melee) {
            beginPlayerStrike(gs, false, false);
        }
        /* 0x11A2A..0x11A48: neither flag → just end turn (no Block). */
        if (state_ == CombatState::AwaitingAttackTarget) {
            return false;
        }
        if (state_ == CombatState::Inactive) {
            return true;
        }
        party_acted_[active_party_slot_] = true;
        active_party_slot_ = -1;
        beginActionAck(gs);
        return false;
    }

    switch (ch) {
    case 'A':
        /* 0x11A50 → $1162C(0): Attack monster A / first live, no prompt. */
        if (!can_melee) {
            return false;
        }
        beginPlayerStrike(gs, false, false);
        break;
    case 'F':
        /* 0x11B4C → $1162C($FF): Fight — 'which (A-x)?' when >1 live. */
        if (!can_melee) {
            return false;
        }
        beginPlayerStrike(gs, false, true);
        break;
    case 'S':
        /* 0x11BA4 → $11610($FF): Shoot picker. */
        if (!can_shoot) {
            return false;
        }
        beginPlayerStrike(gs, true, true);
        break;
    case 'C':
        if (!can_cast) {
            return false;
        }
        beginCastPicker();
        return false;
    case 'U':
        /* Combat use @ 0x133EC → same charge/effect path as explore 0xE94A.
         * 0x11BD0 also maps 'P' → Use. */
        pending_cast_flat_ = -2;
        state_ = CombatState::AwaitingItemPick;
        std::snprintf(status_line_, sizeof(status_line_), "Use which (1-6/A-F)?");
        return false;
    case 'P':
        pending_cast_flat_ = -2;
        state_ = CombatState::AwaitingItemPick;
        std::snprintf(status_line_, sizeof(status_line_), "Use which (1-6/A-F)?");
        return false;
    case 'B':
        /* 0x11A60: B only marks turn done — no Block GS. */
        break;
    case 'D':
        /* 0x11B0A Block. */
        resolvePlayerBlock();
        break;
    case 'R':
        resolvePlayerRun(gs, world);
        break;
    case 'E':
        /* 0x11B1A → -$7CC2 → 0x20FF2 Exchange (not Run — ASM label is wrong). */
        beginExchange();
        return false;
    case 'V':
    case 'Q':
        /* 0x11B6E: GameSession opens sheet for active slot (turn not spent). */
        return false;
    default:
        return false;
    }

    if (state_ == CombatState::AwaitingAttackTarget) {
        return false;
    }

    if (state_ == CombatState::Inactive) {
        return true;
    }

    /* 0x20FF2 leaves acted[] as swapped; other commands stamp acted. */
    party_acted_[active_party_slot_] = true;
    if (active_party_slot_ >= 0 && active_party_slot_ < MM2_GS_PARTY_SIZE) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_ACTED + active_party_slot_, 1);
    }
    active_party_slot_ = -1;
    beginActionAck(gs);
    return false;
}

void CombatSession::beginRound(GameStateView &gs)
{
    /* 0x12A6A new-round restart (NOT 0x12A22): clr acted arrays + status lottery.
     * Melee/front ranks stay — 0x11D0C only at fight entry (startRoundLoop). */
    active_monster_slot_ = -1;
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLEE_PRINT, 0);   /* 0x12A30 -$5E27 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 0); /* 0x12A34 -$51E */
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        monster_acted_[i] = false;
        if (i < 10) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + i, 0); /* 0x12A74 -$5E4A */
        }
    }
    for (int i = 0; i < MM2_GS_PARTY_SIZE; ++i) {
        party_acted_[i] = false;
        party_blocking_[i] = false;
        mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_ACTED + i, 0); /* 0x12A94 -$5E40 */
    }
    /* 0x12A44..0x12A56: rng(1, party_count) → -$4F8, then subq #1. */
    const int pc = launch_ ? party_count_ : 1;
    const int roll = rng_ ? rng_->range(1, pc > 0 ? pc : 1) : 1;
    uint8_t tgt = static_cast<uint8_t>(roll > 0 ? roll - 1 : 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, tgt);

    /* 0x12AC8..0x12B36: per-monster status bit lottery (clear bit0, mask via rng). */
    int live = static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT));
    if (live > 10) {
        live = 10;
    }
    for (int i = 0; i < live; ++i) {
        uint8_t st = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_STATUS + i);
        const uint8_t masked = static_cast<uint8_t>(st & 0xFE);
        if (masked == 0) {
            continue;
        }
        const uint8_t mon_type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + i);
        int hi = static_cast<int>(mon_type);
        if (hi < 1) {
            hi = 1;
        }
        uint8_t r = static_cast<uint8_t>(rng_ ? rng_->range(1, hi) : 1);
        r = static_cast<uint8_t>((r & 0xFE) ^ 0xFE);
        st = static_cast<uint8_t>(masked & r);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_STATUS + i, st);
        if (i < MM2_GS_MONSTER_BATTLE_SLOTS) {
            slots_[i].status = st;
        }
    }
    /* 0x12B40..0x12B4C: initiative scratch */
    mm2_gs_set_u8(gs.a4(), -0x4F5, 0);
    mm2_gs_set_u8(gs.a4(), -0x5E37, 0);
    mm2_gs_set_u8(gs.a4(), -0x4F7, 0);
    mm2_gs_set_u8(gs.a4(), -0x5E38, 0);

    syncMonsterSlotsToGs(gs);
}

bool CombatSession::checkOutcome(GameStateView &gs, const world::MapWorld & /*world*/)
{
    if (firstAliveMonster() < 0) {
        finishVictory(gs);
        return true;
    }

    /* 0x13282: -$795A==0 → end; 0x12C5A latch/TD → 0x11646 flee. */
    if (party_count_ <= 0) {
        const bool fled = mm2_gs_u8(gs.a4(), MM2_GS_PARTY_RAN_LATCH) != 0 ||
                          mm2_gs_u8(gs.a4(), MM2_GS_TIME_DISTORT) != 0;
        finishLeave(gs, fled);
        return true;
    }

    for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
        if (partySlotInFight(i)) {
            return false;
        }
    }
    finishLeave(gs, false);
    return true;
}

void CombatSession::runUntilDecisionOrEnd(GameStateView &gs, const world::MapWorld &world)
{
    for (;;) {
        if (checkOutcome(gs, world)) {
            return;
        }

        int best_monster = -1;
        int best_monster_speed = -1;
        for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
            if (slots_[i].alive && !monster_acted_[i] && slots_[i].speed > best_monster_speed) {
                best_monster = i;
                best_monster_speed = slots_[i].speed;
            }
        }

        int best_party = -1;
        int best_party_speed = -1;
        for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
            if (!partySlotAlive(i) || party_acted_[i]) {
                continue;
            }
            const int idx = rosterIndexForPartySlot(i);
            const int speed = roster_->records[idx].speed_current;
            if (speed > best_party_speed) {
                best_party = i;
                best_party_speed = speed;
            }
        }

        if (best_monster < 0 && best_party < 0) {
            /* 0x12C3E..0x12C66 round epilogue: latch / Time Distortion → leave. */
            if (mm2_gs_u8(gs.a4(), MM2_GS_PARTY_RAN_LATCH) != 0 ||
                mm2_gs_u8(gs.a4(), MM2_GS_TIME_DISTORT) != 0) {
                finishLeave(gs, true);
                return;
            }
            beginRound(gs);
            continue;
        }

        if (best_party >= 0 && (best_monster < 0 || best_party_speed > best_monster_speed)) {
            active_monster_slot_ = -1;
            active_party_slot_ = best_party;
            /* 0x12BB8..0x12BF0: best party speed → -$5E37, slot → -$4F5. */
            mm2_gs_set_u8(gs.a4(), -0x5E37, static_cast<uint8_t>(best_party_speed & 0xFF));
            mm2_gs_set_u8(gs.a4(), -0x4F5, static_cast<uint8_t>(best_party));
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_TURN_SLOT, 0xFF);
            /* 0x119C2: addq PARTY_ACTED[slot]; skip if condition >= $10. */
            if (best_party < MM2_GS_PARTY_SIZE) {
                const uint8_t acted = mm2_gs_u8(gs.a4(), MM2_GS_PARTY_ACTED + best_party);
                mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_ACTED + best_party,
                              static_cast<uint8_t>(acted + 1));
                party_acted_[best_party] = true;
            }
            const int ri = rosterIndexForPartySlot(best_party);
            if (ri >= 0 && roster_->records[ri].condition >= 0x10) {
                active_party_slot_ = -1;
                continue;
            }
            state_ = CombatState::AwaitingCommand;
            setStatus(""); /* the command grid (0x11866) is the whole prompt */
            return;
        }

        resolveMonsterTurn(gs, best_monster);
        /* 0x10118 advance may relocate the actor into a front slot and rewrite
         * -$4F7 / active_monster_slot_. Stamp acted on the post-turn slot, not
         * the pre-advance index (otherwise the displaced Soldier is marked and
         * the Thug who advanced can act again). */
        const int done_slot = (active_monster_slot_ >= 0) ? active_monster_slot_ : best_monster;
        if (done_slot >= 0 && done_slot < MM2_GS_MONSTER_BATTLE_SLOTS) {
            monster_acted_[done_slot] = true;
            if (done_slot < 10) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + done_slot, 1);
            }
        }
        /* Speed latch from initiative pick; slot latch stays on post-turn -$4F7. */
        mm2_gs_set_u8(gs.a4(), -0x5E38, static_cast<uint8_t>(best_monster_speed & 0xFF));
        if (done_slot >= 0) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_TURN_SLOT, static_cast<uint8_t>(done_slot));
        }
        if (checkOutcome(gs, world)) {
            return;
        }
        beginActionAck(gs);
        return;
    }
}

void CombatSession::beginCastPicker()
{
    /* 0x11A90: -$7E4E ESC hint, then -$7E12 / 0x79EE — no LAB_6622 spell grid. */
    cast_level_ = 0;
    state_ = CombatState::AwaitingCastLevel;
    setStatus("");
}

bool CombatSession::tickCastPicker(GameStateView &gs, char key)
{
    if (key < '1' || key > '9') {
        return false;
    }
    const int digit = key - '0';
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    if (idx < 0) {
        state_ = CombatState::AwaitingCommand;
        return true;
    }
    const Mm2RosterRecord &rec = roster_->records[idx];

    if (state_ == CombatState::AwaitingCastLevel) {
        const int max_sl = rec.spell_level > 0 ? static_cast<int>(rec.spell_level) : 0;
        if (digit < 1 || digit > max_sl || digit > gameplay::kSpellLevels) {
            return false;
        }
        cast_level_ = digit;
        state_ = CombatState::AwaitingCastNumber;
        return true;
    }

    const int flat = gameplay::spellFlatFromLevelNumber(cast_level_, digit);
    if (flat < 0 || !gameplay::spellKnownInBook(rec, flat)) {
        /* 0x7A8C: re-prompt number until valid / ESC. */
        return false;
    }
    resolvePlayerCast(gs, flat);
    cast_level_ = 0;
    /* resolvePlayerCast may enter target/party/item pick; otherwise ack. */
    if (state_ != CombatState::AwaitingCastTarget && state_ != CombatState::AwaitingPartyPick &&
        state_ != CombatState::AwaitingItemPick) {
        beginActionAck(gs);
    }
    return true;
}

void CombatSession::resolvePlayerCast(GameStateView &gs, int flat0)
{
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    Mm2RosterRecord &caster = roster_->records[idx];
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&caster, name, sizeof(name));

    gameplay::SpellSchool school = gameplay::spellSchoolForClass(caster.class_id);
    if (force_cast_school_ == 0) {
        school = gameplay::SpellSchool::Sorcerer;
    } else if (force_cast_school_ == 1) {
        school = gameplay::SpellSchool::Cleric;
    }
    const gameplay::SpellMeta *table = gameplay::schoolSpellTable(school);
    const gameplay::SpellMeta *meta =
        (table && flat0 >= 0 && flat0 < gameplay::kSpellsPerSchool) ? &table[flat0] : nullptr;
    const char *spell_name = meta ? meta->name : "?";

    /* Cost staging → 0x6DC6: A4-$3F0A SP, A4-$3F08 gems; clr both; may set -$79E8.
     * Item-use F470 sets -$3F0C and skips the deduct (host mirrors via skip_cast_cost_). */
    uint16_t sp_cost = 0;
    uint16_t gem_cost = 0;
    if (meta && !skip_cast_cost_) {
        sp_cost = meta->sp;
        if (meta->per_level) {
            sp_cost = static_cast<uint16_t>(sp_cost * (caster.level > 0 ? caster.level : 1));
        }
        gem_cost = meta->gems;
    }
    mm2_gs_set_u16(gs.a4(), -0x3F0A, sp_cost);
    mm2_gs_set_u16(gs.a4(), -0x3F08, gem_cost);
    /* 0x6DC6: always sub gems from +$5C; SP from +$58 only if enough. */
    if (!skip_cast_cost_) {
        if (caster.gems >= gem_cost) {
            caster.gems = static_cast<uint16_t>(caster.gems - gem_cost);
        } else {
            caster.gems = 0;
        }
        if (caster.sp_current >= sp_cost) {
            caster.sp_current = static_cast<uint16_t>(caster.sp_current - sp_cost);
        }
    }
    mm2_gs_set_u16(gs.a4(), -0x3F0A, 0);
    mm2_gs_set_u16(gs.a4(), -0x3F08, 0);
    if (mm2_gs_u8(gs.a4(), -0x79EA) == 0) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
    }

    /* $CDB8/$CFF8 sparse spell-effect jump tables (doc 26). GS-clear leaves: */
    bool effect = false;
    auto add_eye = [&](int32_t off) {
        /* Eagle @ 0xA91C / Wizard @ 0xAD1C: +5 per caster level, clamp $FA→$FF. */
        const int levels = caster.level > 0 ? static_cast<int>(caster.level) : 1;
        uint8_t t = mm2_gs_u8(gs.a4(), off);
        if (t > 0xFA) {
            mm2_gs_set_u8(gs.a4(), off, 0xFF);
        }
        for (int i = 0; i < levels; ++i) {
            t = mm2_gs_u8(gs.a4(), off);
            if (t < 0xFA) {
                mm2_gs_set_u8(gs.a4(), off, static_cast<uint8_t>(t + 5));
            }
        }
        mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
        mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
    };
    auto addq_flag = [&](int32_t off) {
        /* Levitate/Guard Dog/Shelter: addq #1 if < $FF; set -$79EA, clr -$79E4. */
        uint8_t v = mm2_gs_u8(gs.a4(), off);
        if (v < 0xFF) {
            mm2_gs_set_u8(gs.a4(), off, static_cast<uint8_t>(v + 1));
        }
        mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
        mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
    };

    if (meta && school == gameplay::SpellSchool::Sorcerer) {
        if (flat0 == 0) { /* S1/1 Awaken @ 0xC740 (shared) */
            for (int p = 0; p < party_count_ && p < MM2_GS_PARTY_SIZE; ++p) {
                const int ri = rosterIndexForPartySlot(p);
                if (ri < 0) {
                    continue;
                }
                Mm2RosterRecord &r = roster_->records[ri];
                if (r.condition < 0x80) {
                    r.condition = static_cast<uint8_t>(r.condition & 0x6F);
                }
            }
            mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
            effect = true;
        } else if (flat0 == 1) { /* S1/2 Detect Magic @ 0xA812: Charges: A-n … F-n */
            char line[160];
            int n = std::snprintf(line, sizeof(line), "Charges:");
            for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS && n > 0 && n < static_cast<int>(sizeof(line) - 8); ++i) {
                n += std::snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " %c-%u",
                                   static_cast<char>('A' + i),
                                   static_cast<unsigned>(caster.backpack_charges[i]));
            }
            std::snprintf(status_line_, sizeof(status_line_), "%s", line);
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
            return;
        } else if (flat0 == 5) { /* S1/6 Location host */
            std::snprintf(status_line_, sizeof(status_line_), "Location: map %u at (%u,%u).",
                          static_cast<unsigned>(gs.screenId()), static_cast<unsigned>(gs.coordX()),
                          static_cast<unsigned>(gs.coordY()));
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
            return;
        } else if (flat0 == 4) { /* S1/5 Light @ 0xA8D8 */
            uint8_t light = mm2_gs_u8(gs.a4(), MM2_GS_LIGHT_FACTOR);
            if (light < 0xFE) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_LIGHT_FACTOR, static_cast<uint8_t>(light + 1));
            }
            mm2_gs_set_u8(gs.a4(), -0x79E4, 1);
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
            effect = true;
        } else if (flat0 == 7) { /* S2/1 Eagle Eye @ 0xA91C */
            add_eye(MM2_GS_EAGLE_EYE_TIMER);
            effect = true;
        } else if (flat0 == 11) { /* S2/5 Levitate @ 0xAAAE */
            addq_flag(MM2_GS_LEVITATE_FLAG);
            effect = true;
        } else if (flat0 == 10) { /* S2/4 Jump @ 0xAFFA: 0x5692 facing step */
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x20) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            int8_t dx = 0, dy = 0;
            facingDelta5692(gs, dx, dy);
            gs.setCoordX(static_cast<uint8_t>((gs.coordX() + dx) & 0x0F));
            gs.setCoordY(static_cast<uint8_t>((gs.coordY() + dy) & 0x0F));
            mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
            mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
            effect = true;
        } else if (flat0 == 12) { /* S2/6 Lloyd's Beacon @ 0xAADC */
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x40) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "1-Set 2-Recall?");
            return;
        } else if (flat0 == 13) { /* S2/7 Protection from Magic @ 0xC7A2 */
            const uint8_t lv = caster.level > 0 ? caster.level : 1;
            mm2_gs_set_u8(gs.a4(), MM2_GS_MAGIC_PROTECT, static_cast<uint8_t>(lv + 0x0A));
            mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
            mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
            effect = true;
        } else if (flat0 == 15) { /* S3/2 Fly @ 0xABCC */
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x40) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            cast_aux_ = -1;
            beginPartyPickCast(gs, flat0, "Fly to (A-E)?");
            return;
        } else if (flat0 == 16) { /* S3/3 Invisibility @ 0xB9C4 → -$799C */
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_INVIS_COUNTER);
            if (v < 0xFF) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_INVIS_COUNTER, static_cast<uint8_t>(v + 1));
            }
            effect = true;
        } else if (flat0 == 30) { /* S5/5 Teleport @ 0xADD6 */
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x10) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "Teleport (1-9)?");
            return;
        } else if (flat0 == 19) {
            /* Explore: Wizard Eye @ 0xAD1C → -$799F. Combat: 0xBDA6 random status. */
            if (exploration_cast_) {
                add_eye(MM2_GS_WIZARD_EYE_TIMER);
                effect = true;
            } else {
                const int st = rng_ ? rng_->range(1, 9) : 1;
                if (st == 7) {
                    const int tier = rng_ ? rng_->range(1, 4) : 1;
                    mm2_gs_set_u8(gs.a4(), MM2_GS_ENCASE_TIER, static_cast<uint8_t>(tier));
                }
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, static_cast<uint8_t>(st));
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 1);
                applySpell108BC(gs, 0, 0x0A, 0);
                if (msg_queue_len_ > 0) {
                    std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[0]);
                    msg_queue_idx_ = 0;
                } else {
                    std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
                }
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
        } else if (flat0 == 23) { /* S4/4 Guard Dog @ 0xAD7A */
            addq_flag(MM2_GS_GUARD_DOG_FLAG);
            effect = true;
        } else if (flat0 == 24) { /* S4/5 Shield @ 0xBB84 */
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_SHIELD_COUNTER);
            if (v < 0xFF) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_SHIELD_COUNTER, static_cast<uint8_t>(v + 1));
            }
            effect = true;
        } else if (flat0 == 25) { /* S4/6 Time Distortion @ 0xBB86: D390 + bit3 gate */
            if (exploration_cast_) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x08) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_TIME_DISTORT);
            mm2_gs_set_u8(gs.a4(), MM2_GS_TIME_DISTORT, static_cast<uint8_t>(v + 1));
            effect = true;
        } else if (flat0 == 29) { /* S5/4 Shelter @ 0xADA8 */
            addq_flag(MM2_GS_SHELTER_FLAG);
            effect = true;
        } else if (flat0 == 32) { /* S6/2 Entrapment @ 0xBCBC: D390 + bit0 gate */
            if (exploration_cast_) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x01) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_ENTRAPMENT);
            mm2_gs_set_u8(gs.a4(), MM2_GS_ENTRAPMENT, static_cast<uint8_t>(v + 1));
            effect = true;
        } else if (flat0 == 43) { /* S8/4 Power Shield @ 0xBEE2 */
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_POWER_SHIELD_CTR);
            if (v < 0xFF) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_POWER_SHIELD_CTR, static_cast<uint8_t>(v + 1));
            }
            effect = true;
        } else if (flat0 == 34) { /* S6/4 Recharge Item @ 0xAEAC */
            beginItemPickCast(gs, flat0, "Recharge which (A-F)?");
            return;
        } else if (flat0 == 37) { /* S7/2 Duplication @ 0xAF02 */
            beginItemPickCast(gs, flat0, "Duplicate which (A-F)?");
            return;
        } else if (flat0 == 38) { /* S7/3 Etherealize @ 0xAFFA: Jump-equivalent facing step */
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x20) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            int8_t dx = 0, dy = 0;
            facingDelta5692(gs, dx, dy);
            gs.setCoordX(static_cast<uint8_t>((gs.coordX() + dx) & 0x0F));
            gs.setCoordY(static_cast<uint8_t>((gs.coordY() + dy) & 0x0F));
            mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
            mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
            effect = true;
        } else if (flat0 == 47) { /* S9/4 Enchant Item @ 0xB050 */
            beginItemPickCast(gs, flat0, "Enchant which (A-F)?");
            return;
        }
    } else if (meta && school == gameplay::SpellSchool::Cleric) {
        if (flat0 == 1) { /* C1/2 Awaken @ 0xC740 */
            for (int p = 0; p < party_count_ && p < MM2_GS_PARTY_SIZE; ++p) {
                const int ri = rosterIndexForPartySlot(p);
                if (ri < 0) {
                    continue;
                }
                Mm2RosterRecord &r = roster_->records[ri];
                if (r.condition < 0x80) {
                    r.condition = static_cast<uint8_t>(r.condition & 0x6F);
                }
            }
            mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
            effect = true;
        } else if (flat0 == 2) { /* C1/3 Bless @ 0xBFEC */
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_BLESS_COUNTER);
            if (v < 0xFF) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_BLESS_COUNTER, static_cast<uint8_t>(v + 1));
            }
            effect = true;
        } else if (flat0 == 3) { /* C1/4 First Aid @ 0xC7D0 */
            if (party_count_ <= 1) {
                applyHealCa58(gs, 0, 8, spell_name);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 5) { /* C1/6 Power Cure @ 0xC7E2 */
            if (party_count_ <= 1) {
                uint16_t heal = 0;
                const int lv = caster.level > 0 ? static_cast<int>(caster.level) : 1;
                for (int i = 0; i < lv; ++i) {
                    heal = static_cast<uint16_t>(heal + (rng_ ? rng_->range(1, 10) : 1));
                }
                applyHealCa58(gs, 0, heal, spell_name);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 7) { /* C2/1 Cure Wounds @ 0xC822 */
            if (party_count_ <= 1) {
                applyHealCa58(gs, 0, 15, spell_name);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 16) { /* C3/3 Cure Poison @ 0xC862 */
            if (party_count_ <= 1) {
                applyCureMaskToPartySlot(gs, 0, 0x77, false, spell_name);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 22) { /* C4/3 Cure Disease @ 0xC8BC */
            if (party_count_ <= 1) {
                applyCureMaskToPartySlot(gs, 0, 0x7B, false, spell_name);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 11) { /* C2/5 Protection From Elements @ 0xC834 */
            const uint8_t lv = caster.level > 0 ? caster.level : 1;
            mm2_gs_set_u8(gs.a4(), MM2_GS_FORCES_PROTECT, static_cast<uint8_t>(lv + 0x14));
            mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
            mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
            effect = true;
        } else if (flat0 == 30) { /* C5/5 Remove Condition @ 0xC916 */
            if (party_count_ <= 1) {
                applyCureMaskToPartySlot(gs, 0, 0x00, true, spell_name);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 4) { /* C1/5 Light @ 0xA8D8 family */
            uint8_t light = mm2_gs_u8(gs.a4(), MM2_GS_LIGHT_FACTOR);
            if (light < 0xFE) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_LIGHT_FACTOR, static_cast<uint8_t>(light + 1));
            }
            mm2_gs_set_u8(gs.a4(), -0x79E4, 1);
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
            effect = true;
        } else if (flat0 == 15) { /* C3/2 Create Food @ 0xB1BE */
            if (caster.food < 0x28) {
                caster.food = static_cast<uint8_t>(caster.food + 8);
                mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
                effect = true;
            }
        } else if (flat0 == 18) { /* C3/5 Lasting Light @ 0xB1F2 */
            uint8_t light = mm2_gs_u8(gs.a4(), MM2_GS_LIGHT_FACTOR);
            if (light > 0xEB) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_LIGHT_FACTOR, 0xFF);
            } else {
                mm2_gs_set_u8(gs.a4(), MM2_GS_LIGHT_FACTOR,
                              static_cast<uint8_t>(light + 0x14));
            }
            mm2_gs_set_u8(gs.a4(), -0x79E4, 1);
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
            effect = true;
        } else if (flat0 == 19) { /* C3/6 Walk on Water @ 0xB22C */
            mm2_gs_set_u8(gs.a4(), MM2_GS_WALK_WATER_FLAG, 1);
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
            mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
            effect = true;
        } else if (flat0 == 21) { /* C4/2 @ 0xB278: move.b #1,-$79A3 (unnamed) */
            mm2_gs_set_u8(gs.a4(), MM2_GS_BUFF_79A3, 1);
            effect = true;
        } else if (flat0 == 31) { /* C6/1 @ 0xB340: move.b #1,-$79A1 (unnamed) */
            mm2_gs_set_u8(gs.a4(), MM2_GS_BUFF_79A1, 1);
            effect = true;
        } else if (flat0 == 35) { /* C6/5 @ 0xB3DA: move.b #1,-$79A4 (unnamed) */
            mm2_gs_set_u8(gs.a4(), MM2_GS_TALISMAN_BASE, 1);
            effect = true;
        } else if (flat0 == 41) { /* C8/2 @ 0xB3F4: move.b #1,-$79A2 (unnamed) */
            mm2_gs_set_u8(gs.a4(), MM2_GS_BUFF_79A2, 1);
            effect = true;
        } else if (flat0 == 6) { /* C1/7 Turn Undead @ 0xBFEE → 0xC028 */
            uint8_t arg = static_cast<uint8_t>(caster.level);
            if (arg < 0x10) {
                arg = static_cast<uint8_t>(arg * 0x10);
            }
            const int killed = applyTurnUndeadC050(gs, arg);
            if (msg_queue_len_ == 0) {
                std::snprintf(status_line_, sizeof(status_line_), "%s casts %s (%d).", name, spell_name,
                              killed);
            }
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
            return;
        } else if (flat0 == 8) { /* C2/2 Heroism @ 0xC12A → 0xD2EA */
            const int pc = party_count_;
            if (pc <= 1) {
                applyHeroismToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            } else {
                beginPartyPickCast(gs, flat0, "On whom");
                return;
            }
        } else if (flat0 == 9) { /* C2/3 Nature's Gate @ 0xB0EA */
            int fail = 0;
            if ((mm2_gs_u8(gs.a4(), MM2_GS_ATTRIB_FLAGS) & 0x40) != 0) {
                ++fail;
            }
            if (mm2_gs_u16(gs.a4(), MM2_GS_ERA) != 9) {
                ++fail;
            }
            if (fail == 0) {
                /* Day[era9] at -$79CC; odd → walk month_tbl; even → tier 12. */
                const uint16_t day = mm2_gs_u16(gs.a4(), MM2_GS_DAY + 9 * 2);
                static const uint16_t kMonth[13] = {20, 40, 60, 80, 93, 94, 100, 120, 130, 140, 150, 181, 255};
                static const uint8_t kSeasonA[13] = {11, 14, 33, 9, 37, 8, 37, 6, 14, 39, 8, 11, 11};
                static const uint8_t kSeasonB[13] = {0xB5, 0x61, 0x74, 0x2C, 0x55, 0x65, 0x55,
                                                    0xD4, 0x0F, 0x73, 0xEC, 0x37, 0xB8};
                int tier = 12;
                if ((day & 1) != 0) {
                    tier = 0;
                    while (tier < 13 && day >= kMonth[tier]) {
                        ++tier;
                    }
                }
                if (day >= 0x96) {
                    mm2_gs_set_u16(gs.a4(), MM2_GS_ERA, 8);
                    tier = 11;
                }
                if (tier > 12) {
                    tier = 12;
                }
                gs.setScreenId(kSeasonA[tier]);
                const uint8_t packed = kSeasonB[tier];
                gs.setCoordX(static_cast<uint8_t>(packed & 0x0F));
                gs.setCoordY(static_cast<uint8_t>((packed >> 4) & 0x0F));
                mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS, 1);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
                effect = true;
            } else {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
        } else if (flat0 == 23) { /* C4/4 Restore Alignment @ 0xB26A: +$0D → +$6A */
            if (party_count_ <= 1) {
                applyRestoreAlignmentToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 24) { /* C4/5 Surface @ 0xB2B2 */
            int fail = 0;
            if ((mm2_gs_u8(gs.a4(), MM2_GS_ATTRIB_FLAGS) & 0x40) != 0) {
                ++fail;
            }
            const uint8_t packed = mm2_gs_u8(gs.a4(), MM2_GS_ATTRIB_RECALL_COORD);
            if (packed == 0) {
                ++fail;
            }
            if (fail == 0) {
                gs.setCoordX(static_cast<uint8_t>(packed & 0x0F));
                gs.setCoordY(static_cast<uint8_t>((packed >> 4) & 0x0F));
                gs.setScreenId(mm2_gs_u8(gs.a4(), MM2_GS_ATTRIB_RECALL_SCREEN));
                mm2_gs_set_u8(gs.a4(), -0x79E4, 1);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
                effect = true;
            } else {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
        } else if (flat0 == 25) { /* C4/6 Holy Bonus @ 0xC308: add level>>1 to -$7999 */
            const int ri = rosterIndexForPartySlot(active_party_slot_);
            const int lv = (ri >= 0 && roster_->records[ri].level > 0)
                               ? static_cast<int>(roster_->records[ri].level)
                               : 1;
            const uint8_t add = static_cast<uint8_t>(lv >> 1);
            const uint8_t cur = mm2_gs_u8(gs.a4(), MM2_GS_HOLY_BONUS_CTR);
            mm2_gs_set_u8(gs.a4(), MM2_GS_HOLY_BONUS_CTR,
                          static_cast<uint8_t>(cur > 0xFF - add ? 0xFF : cur + add));
            effect = true;
        } else if (flat0 == 28) { /* C5/3 Frenzy @ 0xC3AE → 0xD2EA */
            if (mm2_gs_u8(gs.a4(), MM2_GS_FRENZY_LATCH) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            const int pc = party_count_;
            if (pc <= 1) {
                applyFrenzyToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 32) { /* C6/2 Rejuvenate @ 0xB332 */
            if (party_count_ <= 1) {
                applyRejuvenateToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 39) { /* C7/4 Raise Dead @ 0xB492 */
            if (party_count_ <= 1) {
                applyRaiseDeadToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 33) { /* C6/3 Stone to Flesh @ 0xC96E */
            if (party_count_ <= 1) {
                applyStoneToFleshToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 43) { /* C8/4 Town Portal @ 0xB3E6 */
            if ((mm2_gs_u8(gs.a4(), MM2_GS_ATTRIB_FLAGS) & 0x40) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "Town (1-5)?");
            return;
        } else if (flat0 == 46) { /* C9/3 Resurrection @ 0xC9B6 */
            if (party_count_ <= 1) {
                applyResurrectionToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            beginPartyPickCast(gs, flat0, "On whom");
            return;
        } else if (flat0 == 47) { /* C9/4 Uncurse Item @ 0xB524 */
            beginItemPickCast(gs, flat0, "Uncurse which (A-F)?");
            return;
        } else if (flat0 == 44) { /* C9/1 Divine Intervention @ 0xC6D6: D390 */
            if (exploration_cast_) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            /* 0xC6C0: if -$51A already set → 0xD29C "already" path; else addq + heal. */
            if (mm2_gs_u8(gs.a4(), -0x51A) != 0) {
                std::snprintf(status_line_, sizeof(status_line_),
                              "Divine Intervention already used.");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            mm2_gs_set_u8(gs.a4(), -0x51A, 1);
            for (int p = 0; p < party_count_ && p < MM2_GS_PARTY_SIZE; ++p) {
                const int ri = rosterIndexForPartySlot(p);
                if (ri < 0) {
                    continue;
                }
                Mm2RosterRecord &r = roster_->records[ri];
                /* 0xC6FC: skip eradicated ($FF); else clr cond. */
                if (r.condition != 0xFF) {
                    r.condition = 0;
                }
                /* 0xC712: move.w +$74 → +$5E (working HP := ceiling). */
                r.hp_max = r.hp_current;
            }
            effect = true;
        } else if (flat0 == 45) { /* C9/2 Holy Word @ 0xC752 → 0xC028 arg0 */
            mm2_gs_set_u8(gs.a4(), MM2_GS_HOLY_WORD_GATE, 1);
            const int killed = applyTurnUndeadC050(gs, 0);
            if (msg_queue_len_ == 0) {
                std::snprintf(status_line_, sizeof(status_line_), "%s casts %s (%d).", name, spell_name,
                              killed);
            }
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
            return;
        }
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1); /* spell-acted latch */

    if (effect) {
        std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
        return;
    }

    /* D390 auto-AoE via combat effect table (D23E/D1AE, subq#2):
     * Sorc: Dancing Sword, Prismatic, Mega Volts, Inferno, Star Burst.
     * Cleric (effect=flat+0x30): Apparition, Weaken, Deadly Swarm, Paralyze,
     * Moon Ray. Fire Ball / Fiery Flail are D43C letter-pick. */
    static const uint8_t kSorcAutoAoE[] = {36, 39, 41, 42, 45, 46};
    static const uint8_t kClerAutoAoE[] = {0, 13, 27, 29, 38};
    /* D43C letter-pick combat leaves. */
    static const uint8_t kSorcCombat[] = {2,  3,  6,  8,  9,  14, 17, 18, 20, 21, 22,
                                          26, 27, 28, 31, 33, 35, 40, 44};
    static const uint8_t kClerCombat[] = {10, 12, 14, 17, 20, 26, 34, 36, 37, 40, 42};

    auto in_list = [](const uint8_t *tab, int n, int flat) {
        for (int i = 0; i < n; ++i) {
            if (tab[i] == static_cast<uint8_t>(flat)) {
                return true;
            }
        }
        return false;
    };

    if ((school == gameplay::SpellSchool::Sorcerer &&
         in_list(kSorcAutoAoE, static_cast<int>(sizeof(kSorcAutoAoE)), flat0)) ||
        (school == gameplay::SpellSchool::Cleric &&
         in_list(kClerAutoAoE, static_cast<int>(sizeof(kClerAutoAoE)), flat0))) {
        if (exploration_cast_) {
            std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
            return;
        }
        applyCastToMonsterTarget(gs, 0, flat0);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
        return;
    }

    bool combat_target = false;
    if (school == gameplay::SpellSchool::Sorcerer) {
        combat_target = in_list(kSorcCombat, static_cast<int>(sizeof(kSorcCombat)), flat0);
    } else if (school == gameplay::SpellSchool::Cleric) {
        combat_target = in_list(kClerCombat, static_cast<int>(sizeof(kClerCombat)), flat0);
    }
    if (combat_target) {
        if (exploration_cast_) {
            /* 0xD390 gate: -$3F0C clear outside combat → fail, no letter UI. */
            std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
            return;
        }
        pending_cast_flat_ = flat0;
        cast_target_slot_ = -1;
        state_ = CombatState::AwaitingCastTarget;
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0); /* 0xD440 clr until pick confirms */
        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, 0);
        std::snprintf(status_line_, sizeof(status_line_), "Which monster?");
        return;
    }
    std::snprintf(status_line_, sizeof(status_line_), "%s casts %s (stub).", name, spell_name);
}

bool CombatSession::tickCastTarget(GameStateView &gs, char key)
{
    /* 0xD43C: letter 'A'..'J' → slot 0..9; must be < live count -$77BE. */
    if (key < 'A' || key > 'J') {
        return false;
    }
    const int slot = key - 'A';
    int alive = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (slots_[i].alive) {
            ++alive;
        }
    }
    if (slot >= alive) {
        return false;
    }
    cast_target_slot_ = slot;
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1); /* 0xD506 */
    const int flat = pending_cast_flat_;
    applyCastToMonsterTarget(gs, slot, flat);
    pending_cast_flat_ = -1;
    beginActionAck(gs);
    return true;
}

bool CombatSession::tickPartyPick(GameStateView &gs, char key)
{
    const int flat = pending_cast_flat_;
    const int pc = launch_ ? party_count_ : 0;
    const char uk = static_cast<char>(std::toupper(static_cast<unsigned char>(key)));

    /* S2/6 Lloyd @ 0xAADC: '1' set, else recall. */
    if (pending_cast_school_ == 0 && flat == 12) {
        if (key == '1') {
            mm2_gs_set_u8(gs.a4(), MM2_GS_LLOYD_SCREEN, gs.screenId());
            mm2_gs_set_u8(gs.a4(), MM2_GS_LLOYD_COORD,
                          static_cast<uint8_t>((gs.coordY() << 4) | (gs.coordX() & 0x0F)));
            std::snprintf(status_line_, sizeof(status_line_), "Beacon set.");
        } else if (key == '2') {
            gs.setScreenId(mm2_gs_u8(gs.a4(), MM2_GS_LLOYD_SCREEN));
            const uint8_t packed = mm2_gs_u8(gs.a4(), MM2_GS_LLOYD_COORD);
            gs.setCoordX(static_cast<uint8_t>(packed & 0x0F));
            gs.setCoordY(static_cast<uint8_t>((packed >> 4) & 0x0F));
            mm2_gs_set_u8(gs.a4(), -0x79E4, 1);
            mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
            std::snprintf(status_line_, sizeof(status_line_), "Beacon recalled.");
        } else {
            return false;
        }
        pending_cast_flat_ = -1;
        pending_cast_school_ = -1;
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
        beginActionAck(gs);
        return true;
    }

    /* S5/5 Teleport @ 0xADD6: digit steps via 0x5692. */
    if (pending_cast_school_ == 0 && flat == 30) {
        if (key < '1' || key > '9') {
            return false;
        }
        int steps = key - '0';
        int8_t dx = 0, dy = 0;
        facingDelta5692(gs, dx, dy);
        while (steps-- > 0) {
            gs.setCoordX(static_cast<uint8_t>((gs.coordX() + dx) & 0x0F));
            gs.setCoordY(static_cast<uint8_t>((gs.coordY() + dy) & 0x0F));
        }
        mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
        mm2_gs_set_u8(gs.a4(), -0x79E4, 0);
        pending_cast_flat_ = -1;
        pending_cast_school_ = -1;
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
        std::snprintf(status_line_, sizeof(status_line_), "Teleport.");
        beginActionAck(gs);
        return true;
    }

    /* S3/2 Fly @ 0xABCC: A-E then 1-4. */
    if (pending_cast_school_ == 0 && flat == 15) {
        if (cast_aux_ < 0) {
            if (uk < 'A' || uk > 'E') {
                return false;
            }
            cast_aux_ = uk - 'A';
            std::snprintf(status_line_, sizeof(status_line_), "(1-4)?");
            return true; /* stay in pick */
        }
        if (key < '1' || key > '4') {
            return false;
        }
        const int row = key - '1';
        const int idx = cast_aux_ * 4 + row;
        gs.setCoordY(0xFF);
        gs.setCoordX(0xFF);
        gs.setScreenId(mm2_gs_u8(gs.a4(), MM2_GS_FLY_SCREEN_TBL + idx));
        mm2_gs_set_u8(gs.a4(), -0x79E4, 1);
        mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
        cast_aux_ = -1;
        pending_cast_flat_ = -1;
        pending_cast_school_ = -1;
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
        std::snprintf(status_line_, sizeof(status_line_), "Fly.");
        beginActionAck(gs);
        return true;
    }

    /* C8/4 Town Portal @ 0xB3E6: digit 1-5 → screen 0..4, coords $FF. */
    if (pending_cast_school_ == 1 && flat == 43) {
        if (key < '1' || key > '5') {
            return false;
        }
        gs.setScreenId(static_cast<uint8_t>(key - '1'));
        gs.setCoordX(0xFF);
        gs.setCoordY(0xFF);
        mm2_gs_set_u8(gs.a4(), -0x79E4, 1);
        mm2_gs_set_u8(gs.a4(), -0x79EA, 1);
        pending_cast_flat_ = -1;
        pending_cast_school_ = -1;
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
        std::snprintf(status_line_, sizeof(status_line_), "Town Portal.");
        beginActionAck(gs);
        return true;
    }

    /* 0xD2EA / 0xD312: '1'..party_count → party slot index. */
    if (key < '1' || key > static_cast<char>('0' + pc)) {
        return false;
    }
    const int party_slot = key - '1';
    pending_cast_flat_ = -1;
    pending_cast_school_ = -1;
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
    if (flat == 8) {
        applyHeroismToPartySlot(gs, party_slot);
    } else if (flat == 28) {
        applyFrenzyToPartySlot(gs, party_slot);
    } else if (flat == 3) {
        applyHealCa58(gs, party_slot, 8, "First Aid");
    } else if (flat == 5) {
        const int idx = rosterIndexForPartySlot(active_party_slot_);
        const int lv = (idx >= 0 && roster_->records[idx].level > 0)
                           ? static_cast<int>(roster_->records[idx].level)
                           : 1;
        uint16_t heal = 0;
        for (int i = 0; i < lv; ++i) {
            heal = static_cast<uint16_t>(heal + (rng_ ? rng_->range(1, 10) : 1));
        }
        applyHealCa58(gs, party_slot, heal, "Power Cure");
    } else if (flat == 7) {
        applyHealCa58(gs, party_slot, 15, "Cure Wounds");
    } else if (flat == 16) {
        applyCureMaskToPartySlot(gs, party_slot, 0x77, false, "Cure Poison");
    } else if (flat == 22) {
        applyCureMaskToPartySlot(gs, party_slot, 0x7B, false, "Cure Disease");
    } else if (flat == 30) {
        applyCureMaskToPartySlot(gs, party_slot, 0x00, true, "Remove Condition");
    } else if (flat == 23) {
        applyRestoreAlignmentToPartySlot(gs, party_slot);
    } else if (flat == 32) {
        applyRejuvenateToPartySlot(gs, party_slot);
    } else if (flat == 39) {
        applyRaiseDeadToPartySlot(gs, party_slot);
    } else if (flat == 33) {
        applyStoneToFleshToPartySlot(gs, party_slot);
    } else if (flat == 46) {
        applyResurrectionToPartySlot(gs, party_slot);
    }
    beginActionAck(gs);
    return true;
}

void CombatSession::beginPartyPickCast(GameStateView &gs, int flat0, const char *prompt)
{
    pending_cast_flat_ = flat0;
    pending_cast_school_ = 0;
    if (force_cast_school_ == 1) {
        pending_cast_school_ = 1;
    } else if (force_cast_school_ == 0) {
        pending_cast_school_ = 0;
    } else {
        const int ri = rosterIndexForPartySlot(active_party_slot_);
        if (ri >= 0 &&
            gameplay::spellSchoolForClass(roster_->records[ri].class_id) ==
                gameplay::SpellSchool::Cleric) {
            pending_cast_school_ = 1;
        }
    }
    state_ = CombatState::AwaitingPartyPick;
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0);
    const int pc = launch_ ? party_count_ : 1;
    if (prompt && std::strcmp(prompt, "On whom") == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "On whom (1-%d)?", pc);
    } else if (prompt) {
        std::snprintf(status_line_, sizeof(status_line_), "%s", prompt);
    }
}

void CombatSession::beginItemPickCast(GameStateView &gs, int flat0, const char *prompt)
{
    pending_cast_flat_ = flat0;
    pending_cast_school_ = 0;
    if (force_cast_school_ == 1) {
        pending_cast_school_ = 1;
    } else if (force_cast_school_ != 0) {
        const int ri = rosterIndexForPartySlot(active_party_slot_);
        if (ri >= 0 &&
            gameplay::spellSchoolForClass(roster_->records[ri].class_id) ==
                gameplay::SpellSchool::Cleric) {
            pending_cast_school_ = 1;
        }
    }
    state_ = CombatState::AwaitingItemPick;
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0);
    if (prompt) {
        std::snprintf(status_line_, sizeof(status_line_), "%s", prompt);
    }
}

bool CombatSession::tickItemPick(GameStateView &gs, char key)
{
    const int flat = pending_cast_flat_;
    const char uk = static_cast<char>(std::toupper(static_cast<unsigned char>(key)));

    /* Combat/sheet Use pick: 1-6 equip, A-F backpack. */
    if (flat == -2) {
        bool backpack = false;
        int slot = -1;
        if (key >= '1' && key <= '6') {
            slot = key - '1';
            backpack = false;
        } else if (uk >= 'A' && uk <= 'F') {
            slot = uk - 'A';
            backpack = true;
        } else {
            return false;
        }
        pending_cast_flat_ = -1;
        const bool ok = applyItemUse(gs, active_party_slot_, backpack, slot);
        if (!ok) {
            beginActionAck(gs);
            return true;
        }
        if (state_ == CombatState::AwaitingPartyPick || state_ == CombatState::AwaitingItemPick ||
            state_ == CombatState::AwaitingCastTarget) {
            return true;
        }
        beginActionAck(gs);
        return true;
    }

    if (uk < 'A' || uk > 'F') {
        return false;
    }
    const int pack_slot = uk - 'A';
    const int ri = rosterIndexForPartySlot(active_party_slot_);
    if (ri < 0 || roster_->records[ri].backpack_id[pack_slot] == 0) {
        return false; /* 0xB56E: empty slot rejected, re-prompt */
    }
    const int school = pending_cast_school_;
    pending_cast_flat_ = -1;
    pending_cast_school_ = -1;
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
    if (flat == 34) {
        applyRechargeToBackpackSlot(gs, pack_slot);
    } else if (flat == 37) {
        applyDuplicationFromBackpackSlot(gs, pack_slot);
    } else if (flat == 47 && school == 0) {
        applyEnchantToBackpackSlot(gs, pack_slot);
    } else if (flat == 47) {
        applyUncurseToBackpackSlot(gs, pack_slot);
    }
    beginActionAck(gs);
    return true;
}

void CombatSession::applyRechargeToBackpackSlot(GameStateView &gs, int pack_slot)
{
    (void)gs;
    const int ri = rosterIndexForPartySlot(active_party_slot_);
    if (ri < 0 || pack_slot < 0 || pack_slot >= MM2_ROSTER_ITEM_SLOTS) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &rec = roster_->records[ri];
    if (rec.backpack_charges[pack_slot] == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    const int add = rng_ ? rng_->range(1, 6) : 1;
    const int sum = static_cast<int>(rec.backpack_charges[pack_slot]) + add;
    rec.backpack_charges[pack_slot] = static_cast<uint8_t>(sum > 255 ? 255 : sum);
    std::snprintf(status_line_, sizeof(status_line_), "Recharged +%d.", add);
}

void CombatSession::applyDuplicationFromBackpackSlot(GameStateView &gs, int pack_slot)
{
    (void)gs;
    const int ri = rosterIndexForPartySlot(active_party_slot_);
    if (ri < 0 || pack_slot < 0 || pack_slot >= MM2_ROSTER_ITEM_SLOTS) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &rec = roster_->records[ri];
    const uint8_t id = rec.backpack_id[pack_slot];
    if (id == 0 || id >= 0xD0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    int empty = -1;
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.backpack_id[i] == 0) {
            empty = i;
            break;
        }
    }
    if (empty < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    rec.backpack_id[empty] = id;
    rec.backpack_charges[empty] = rec.backpack_charges[pack_slot];
    rec.backpack_flags[empty] = rec.backpack_flags[pack_slot];
    mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
    std::snprintf(status_line_, sizeof(status_line_), "Duplicated.");
}

void CombatSession::applyEnchantToBackpackSlot(GameStateView &gs, int pack_slot)
{
    const int ri = rosterIndexForPartySlot(active_party_slot_);
    if (ri < 0 || pack_slot < 0 || pack_slot >= MM2_ROSTER_ITEM_SLOTS) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &rec = roster_->records[ri];
    const uint8_t flags = rec.backpack_flags[pack_slot];
    const uint8_t hi = static_cast<uint8_t>(flags & 0xC0);
    uint8_t plus = static_cast<uint8_t>(flags & 0x3F);
    /* 0xB0A0: plus*$32 → -$3F0A; cmp vs +$58 (sp_max). */
    const uint16_t need = static_cast<uint16_t>(static_cast<uint16_t>(plus) * 0x32);
    mm2_gs_set_u16(gs.a4(), -0x3F0A, need);
    if (rec.sp_max < need) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    /* cmpi #$3F / bhi skip addq; plus already ≤$3F so always addq (may become $40). */
    plus = static_cast<uint8_t>(plus + 1);
    rec.backpack_flags[pack_slot] = static_cast<uint8_t>(hi | plus);
    std::snprintf(status_line_, sizeof(status_line_), "Enchanted.");
}

void CombatSession::applyUncurseToBackpackSlot(GameStateView &gs, int pack_slot)
{
    (void)gs;
    const int ri = rosterIndexForPartySlot(active_party_slot_);
    if (ri < 0 || pack_slot < 0 || pack_slot >= MM2_ROSTER_ITEM_SLOTS) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &rec = roster_->records[ri];
    if (rec.backpack_charges[pack_slot] == 0xFF) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    rec.backpack_charges[pack_slot] = 1;
    std::snprintf(status_line_, sizeof(status_line_), "Uncursed.");
}

void CombatSession::applyStoneToFleshToPartySlot(GameStateView &gs, int party_slot)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    if (tgt.condition != 0x82) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    tgt.condition = 0;
    if (party_slot == 0) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
    }
    char tname[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&tgt, tname, sizeof(tname));
    std::snprintf(status_line_, sizeof(status_line_), "Stone to Flesh: %s.", tname);
}

void CombatSession::applyResurrectionToPartySlot(GameStateView &gs, int party_slot)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    if (tgt.condition != 0x81) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    /* $4c60 age+1 caster and target. */
    const int cri = rosterIndexForPartySlot(active_party_slot_);
    if (cri >= 0) {
        uint16_t ca = static_cast<uint16_t>(roster_->records[cri].age + 1);
        if (ca > 0xC8) {
            ca = 0xC8;
        }
        roster_->records[cri].age = static_cast<uint8_t>(ca);
    }
    {
        uint16_t ta = static_cast<uint16_t>(tgt.age + 1);
        if (ta > 0xC8) {
            ta = 0xC8;
        }
        tgt.age = static_cast<uint8_t>(ta);
    }
    const int roll = rng_ ? rng_->range(1, 100) : 50;
    if (roll >= 11) {
        tgt.condition = 0;
        /* 0xCA4A: move.w #$1, $5e(a0) — working HP (+$5E / codec hp_max). */
        tgt.hp_max = 1;
        char tname[MM2_ROSTER_NAME_SIZE + 1];
        mm2_roster_name_to_cstr(&tgt, tname, sizeof(tname));
        std::snprintf(status_line_, sizeof(status_line_), "Resurrection: %s.", tname);
        if (party_slot == 0) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
        }
        return;
    }
    if (roll == 10) {
        tgt.condition = 0xFF;
    }
    std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
}

void CombatSession::castSpellFromSheet(GameStateView &gs, int party_slot, int flat0)
{
    if (!roster_ || !launch_ || party_slot < 0 || party_slot >= party_count_) {
        return;
    }
    exploration_cast_ = (state_ == CombatState::Inactive);
    active_party_slot_ = party_slot;
    force_cast_school_ = -1;
    skip_cast_cost_ = false;
    if (exploration_cast_) {
        state_ = CombatState::AwaitingCommand; /* allow resolve to enter pick states */
    }
    resolvePlayerCast(gs, flat0);
    if (exploration_cast_) {
        if (state_ == CombatState::AwaitingActionAck || state_ == CombatState::AwaitingCommand) {
            state_ = CombatState::Inactive;
            exploration_cast_ = false;
        }
        /* else leave AwaitingPartyPick / ItemPick / CastTarget for tickSheetCastAux */
    }
}

bool CombatSession::tickSheetCastAux(GameStateView &gs, char key)
{
    if (!exploration_cast_) {
        return false;
    }
    if (key == 0x1B) {
        pending_cast_flat_ = -1;
        pending_cast_school_ = -1;
        cast_aux_ = -1;
        force_cast_school_ = -1;
        skip_cast_cost_ = false;
        state_ = CombatState::Inactive;
        exploration_cast_ = false;
        setStatus("");
        return true;
    }
    bool progressed = false;
    if (state_ == CombatState::AwaitingPartyPick) {
        progressed = tickPartyPick(gs, key);
    } else if (state_ == CombatState::AwaitingItemPick) {
        progressed = tickItemPick(gs, static_cast<char>(std::toupper(static_cast<unsigned char>(key))));
    } else if (state_ == CombatState::AwaitingCastTarget) {
        progressed = tickCastTarget(gs, static_cast<char>(std::toupper(static_cast<unsigned char>(key))));
    } else if (state_ == CombatState::AwaitingAttackTarget) {
        progressed = tickAttackTarget(gs, static_cast<char>(std::toupper(static_cast<unsigned char>(key))));
    }
    if (state_ == CombatState::AwaitingActionAck || state_ == CombatState::AwaitingCommand) {
        state_ = CombatState::Inactive;
        exploration_cast_ = false;
        force_cast_school_ = -1;
        skip_cast_cost_ = false;
        active_party_slot_ = -1;
    }
    return progressed;
}

bool CombatSession::applyItemUse(GameStateView &gs, int party_slot, bool backpack, int slot_index)
{
    if (!roster_ || !launch_ || !items_ || party_slot < 0 || party_slot >= party_count_ ||
        slot_index < 0 || slot_index >= MM2_ROSTER_ITEM_SLOTS) {
        std::snprintf(status_line_, sizeof(status_line_), "No power.");
        return false;
    }
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        return false;
    }
    Mm2RosterRecord &r = roster_->records[ri];
    uint8_t item_id = 0;
    uint8_t flags_before = 0;
    if (backpack) {
        item_id = r.backpack_id[slot_index];
        flags_before = r.backpack_flags[slot_index];
        if (item_id == 0 || item_id == 0xFF) {
            std::snprintf(status_line_, sizeof(status_line_), "No power.");
            return false;
        }
        if (r.backpack_charges[slot_index] == 0) {
            std::snprintf(status_line_, sizeof(status_line_), "No charges.");
            return false;
        }
        r.backpack_charges[slot_index] =
            static_cast<uint8_t>(r.backpack_charges[slot_index] - 1);
        if (r.backpack_charges[slot_index] == 0) {
            r.backpack_id[slot_index] = 0xFF;
            r.backpack_flags[slot_index] = 0;
        }
    } else {
        item_id = r.equipped_id[slot_index];
        flags_before = r.equipped_flags[slot_index];
        if (item_id == 0) {
            std::snprintf(status_line_, sizeof(status_line_), "No power.");
            return false;
        }
        if (r.equipped_charges[slot_index] == 0) {
            std::snprintf(status_line_, sizeof(status_line_), "No charges.");
            return false;
        }
        /* Combat equip path decrements; explore 0xE8D8 does not. */
        if (state_ != CombatState::Inactive && !exploration_cast_) {
            r.equipped_charges[slot_index] =
                static_cast<uint8_t>(r.equipped_charges[slot_index] - 1);
            if (r.equipped_charges[slot_index] == 0) {
                r.equipped_id[slot_index] = 0xFF;
                r.equipped_flags[slot_index] = 0;
            }
        }
    }
    const Mm2ItemRecord *irec = mm2_items_lookup(items_, item_id);
    if (!irec || irec->effect_byte == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "No power.");
        return false;
    }
    const uint8_t effect = irec->effect_byte;
    active_party_slot_ = party_slot;
    if (effect >= 0x80) {
        const int code = static_cast<int>(effect & 0x7F) - 1;
        if (code < 0) {
            std::snprintf(status_line_, sizeof(status_line_), "No power.");
            return false;
        }
        const int flat = code >= 0x30 ? code - 0x30 : code;
        force_cast_school_ = code >= 0x30 ? 1 : 0;
        skip_cast_cost_ = true;
        const bool was_inactive = (state_ == CombatState::Inactive);
        if (was_inactive) {
            exploration_cast_ = true;
            state_ = CombatState::AwaitingCommand;
        }
        resolvePlayerCast(gs, flat);
        force_cast_school_ = -1;
        skip_cast_cost_ = false;
        if (was_inactive && exploration_cast_) {
            if (state_ == CombatState::AwaitingActionAck || state_ == CombatState::AwaitingCommand) {
                state_ = CombatState::Inactive;
                exploration_cast_ = false;
            }
        }
        return true;
    }
    /* 0xF4DE: amount = flags + lo nibble; kind = (effect>>4)&7. */
    const uint8_t amount = static_cast<uint8_t>(flags_before + (effect & 0x0F));
    const int kind = (effect >> 4) & 7;
    uint8_t *dst = nullptr;
    switch (kind) {
    case 0:
        dst = reinterpret_cast<uint8_t *>(&r.hp_current) + 1;
        break;
    case 1:
        dst = &r.might_base;
        break;
    case 2:
        dst = &r.speed_base;
        break;
    case 3:
        dst = &r.accuracy_base;
        break;
    case 4:
        dst = &r.alignment_base;
        break;
    case 5:
        dst = &r.level;
        break;
    case 6:
        dst = &r.spell_level;
        break;
    case 7:
        dst = reinterpret_cast<uint8_t *>(&r.sp_max);
        break;
    default:
        break;
    }
    if (dst) {
        const int sum = static_cast<int>(*dst) + amount;
        *dst = static_cast<uint8_t>(sum > 255 ? 255 : sum);
    }
    std::snprintf(status_line_, sizeof(status_line_), "Item used.");
    return true;
}

void CombatSession::facingDelta5692(GameStateView &gs, int8_t &dx, int8_t &dy) const
{
    /* 0x5692: E→(+1,0) N→(0,+1) S→(0,-1) W→(-1,0). */
    dx = 0;
    dy = 0;
    switch (gs.facingKey()) {
        case 'E': dx = 1; break;
        case 'N': dy = 1; break;
        case 'S': dy = -1; break;
        case 'W': dx = -1; break;
        default: break;
    }
}

void CombatSession::applyHealCa58(GameStateView &gs, int party_slot, uint16_t heal,
                                  const char *spell_name)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    if (tgt.condition >= 0x80) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    tgt.condition = static_cast<uint8_t>(tgt.condition & 0x2F);
    /* 0xCA9E..0xCAC0: add heal to +$5E (working HP), clamp to +$74 ceiling. */
    const uint32_t sum = static_cast<uint32_t>(tgt.hp_max) + heal;
    tgt.hp_max = static_cast<uint16_t>(sum > 0xFFFFu ? 0xFFFFu : sum);
    if (tgt.hp_max > tgt.hp_current) {
        tgt.hp_max = tgt.hp_current;
    }
    if (party_slot == 0) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
    }
    char tname[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&tgt, tname, sizeof(tname));
    std::snprintf(status_line_, sizeof(status_line_), "%s: %s +%u HP.", spell_name, tname,
                  static_cast<unsigned>(heal));
}

void CombatSession::applyCureMaskToPartySlot(GameStateView &gs, int party_slot, uint8_t and_mask,
                                             bool clear_all, const char *spell_name)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    if (tgt.condition >= 0x80 && !clear_all) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    if (clear_all) {
        if (tgt.condition >= 0x80) {
            std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
            return;
        }
        tgt.condition = 0;
    } else {
        tgt.condition = static_cast<uint8_t>(tgt.condition & and_mask);
    }
    if (party_slot == 0) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
    }
    char tname[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&tgt, tname, sizeof(tname));
    std::snprintf(status_line_, sizeof(status_line_), "%s: %s.", spell_name, tname);
}

void CombatSession::applyRestoreAlignmentToPartySlot(GameStateView &gs, int party_slot)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    /* 0xB294: move.b $d(a0), $6a(a1) — current → base (temple same). */
    tgt.alignment_base = tgt.alignment_current;
    if (party_slot == active_party_slot_) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
    }
    char tname[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&tgt, tname, sizeof(tname));
    std::snprintf(status_line_, sizeof(status_line_), "Restore Alignment: %s.", tname);
}

void CombatSession::applyRejuvenateToPartySlot(GameStateView &gs, int party_slot)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    const int delta = rng_ ? rng_->range(1, 10) : 1;
    const int roll = rng_ ? rng_->range(1, 100) : 1;
    /* Success only if roll < 50 and age >= 18: subtract delta. Else $4c60 add + fail. */
    if (roll < 50 && tgt.age >= 0x12) {
        if (tgt.age > static_cast<uint8_t>(delta)) {
            tgt.age = static_cast<uint8_t>(tgt.age - delta);
        } else {
            tgt.age = 0;
        }
        char tname[MM2_ROSTER_NAME_SIZE + 1];
        mm2_roster_name_to_cstr(&tgt, tname, sizeof(tname));
        std::snprintf(status_line_, sizeof(status_line_), "Rejuvenate: %s (-%d yr).", tname, delta);
    } else {
        uint16_t age = static_cast<uint16_t>(tgt.age + delta);
        if (age > 0xC8) {
            age = 0xC8;
        }
        tgt.age = static_cast<uint8_t>(age);
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
    }
    (void)gs;
}

void CombatSession::applyRaiseDeadToPartySlot(GameStateView &gs, int party_slot)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    if (tgt.condition < 0x80) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    /* $4c60: caster age+1, target age+5, clamp 200. */
    const int cri = rosterIndexForPartySlot(active_party_slot_);
    if (cri >= 0) {
        uint16_t ca = static_cast<uint16_t>(roster_->records[cri].age + 1);
        if (ca > 0xC8) {
            ca = 0xC8;
        }
        roster_->records[cri].age = static_cast<uint8_t>(ca);
    }
    {
        uint16_t ta = static_cast<uint16_t>(tgt.age + 5);
        if (ta > 0xC8) {
            ta = 0xC8;
        }
        tgt.age = static_cast<uint8_t>(ta);
    }
    if (tgt.endurance_current == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    tgt.endurance_current = static_cast<uint8_t>(tgt.endurance_current - 1);
    tgt.endurance_base = tgt.endurance_current;
    tgt.condition = 0;
    if (party_slot == active_party_slot_) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENGINE_FLAG_79E8, 1);
    }
    char tname[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&tgt, tname, sizeof(tname));
    std::snprintf(status_line_, sizeof(status_line_), "Raise Dead: %s.", tname);
}

void CombatSession::applyHeroismToPartySlot(GameStateView &gs, int party_slot)
{
    (void)gs;
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    if (tgt.level < 0xF9) {
        tgt.level = static_cast<uint8_t>(tgt.level + 6);
    } else {
        tgt.level = 0xFF;
    }
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&tgt, name, sizeof(name));
    std::snprintf(status_line_, sizeof(status_line_), "%s gains Heroism.", name);
}

void CombatSession::applyFrenzyToPartySlot(GameStateView &gs, int party_slot)
{
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    Mm2RosterRecord &tgt = roster_->records[ri];
    if (tgt.condition != 0 || tgt.endurance_base == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return;
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_FRENZY_LATCH, 1);
    tgt.condition = 0x40;
    tgt.endurance_base = static_cast<uint8_t>(tgt.endurance_base - 1);
    tgt.hp_max = 0; /* clr.w $5e @ 0xC41E */
    const uint16_t dmg = static_cast<uint16_t>(
        (static_cast<unsigned>(tgt.spells[0]) + static_cast<unsigned>(tgt.spells[1]) + 10u) * 2u);
    mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, dmg);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_SKIP_RESIST, 1);
    applySpell108BC(gs, 0, 10, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_SKIP_RESIST, 0);
}

int CombatSession::applyTurnUndeadC050(GameStateView &gs, uint8_t arg)
{
    /* 0xC028: latch -$51F; loop undead; kill → 0x10E5E compact (-$7D82); -$7D64 redraw. */
    if (mm2_gs_u8(gs.a4(), MM2_GS_TURN_UNDEAD_USED) != 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
        return 0;
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_TURN_UNDEAD_USED, 1);
    uint8_t remain = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT);
    if (arg != 0 && remain > 0x0A) {
        remain = 0x0A;
    }
    int killed = 0;
    int slot = 0;
    msg_queue_len_ = 0;
    msg_queue_idx_ = 0;
    while (remain > 0 && slot < MM2_GS_MONSTER_BATTLE_SLOTS) {
        if (!slots_[slot].alive || !monsters_) {
            ++slot;
            --remain;
            continue;
        }
        loadMonsterCombatGlobals(gs, slot);
        if (mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_UNDEAD) == 0) {
            ++slot;
            --remain;
            continue;
        }
        uint8_t roll = 0xFF;
        if (arg != 0) {
            roll = static_cast<uint8_t>(rng_ ? rng_->range(1, arg) : 1);
        }
        const uint8_t mon_type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + slot);
        if (roll < mon_type) {
            ++slot;
            --remain;
            continue;
        }
        char mon_name[MM2_MONSTER_NAME_SIZE + 1];
        mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], mon_name, sizeof(mon_name));
        const uint32_t kill_xp = mm2_monster_decode_xp(&monsters_->records[slots_[slot].type]);
        char line[160];
        std::snprintf(line, sizeof(line), "%s is eradicated!", mon_name);
        enqueueCombatMessage(line);
        /* 0xC0D2: -$51E=1 around 0x10E5E so 10DFC skips " goes down!". */
        mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 1);
        onMonsterKilled(gs, slot, kill_xp);
        mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 0);
        ++killed;
        --remain;
        /* do not ++slot — compact shifted successor into this index */
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_HOLY_WORD_GATE, 0);
    if (killed == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
    } else if (msg_queue_len_ > 0) {
        std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[0]);
        msg_queue_idx_ = 0;
    }
    return killed;
}

uint16_t CombatSession::rollSpellDamage133B6(GameStateView &gs, int level, uint8_t sides,
                                             uint8_t addend)
{
    /* 0x1338E: clr -$53E; for i in 0..level-1: piece = sides? rng(1,sides):0; += piece+addend. */
    uint16_t total = 0;
    const int n = level > 0 ? level : 1;
    for (int i = 0; i < n; ++i) {
        uint16_t piece = 0;
        if (sides != 0) {
            piece = static_cast<uint16_t>(rng_ ? rng_->range(1, sides) : 1);
        }
        total = static_cast<uint16_t>(total + piece + addend);
    }
    mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, total);
    return total;
}

void CombatSession::seedCombatStaticTables(GameStateView &gs)
{
    /* Data hunk mm2_data_00.bin @ 0x10D0 / 0xB9A / 0x10E4 / 0x10C0 / 0x10C8. */
    static const uint8_t kStatusBits[8] = {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00};
    static const uint8_t kMresChance[8] = {0, 10, 20, 35, 50, 75, 90, 100};
    static const uint8_t kFleeChance[4] = {3, 9, 24, 255}; /* A4-$6F1A */
    /* A4-$6F26 / -$6F1E: encased DoT @ 0x106B0 (data 0x10D8 / 0x10E0). */
    static const uint16_t kEncaseDmg[4] = {10, 20, 40, 80};
    static const uint8_t kEncaseMode[4] = {6, 5, 4, 5};
    /* A4-$6F3E / -$6F36: class → to-hit level divisor / swing-count divisor. */
    static const uint8_t kHitDiv[8] = {1, 1, 1, 3, 4, 2, 2, 1};
    static const uint8_t kSwingDiv[8] = {4, 4, 5, 7, 10, 5, 5, 4};
    /* A4-$6F16: monster type>>4 → to-hit chance (data @ 0x10E8). */
    static const uint8_t kMonHit[16] = {40, 45, 50, 55, 60, 65, 70, 75, 80, 90, 100, 120, 150, 250, 100, 200};
    /* Fly sector→screen @ data 0xECE (A4-$7120 host); col*4+row. */
    static const uint8_t kFlyScreens[20] = {5,  9,  12, 15, 6,  10, 13, 16, 7,  11,
                                           14, 38, 8,  34, 36, 39, 33, 35, 37, 40};
    /* A4-$7464: Pabil high-nibble index → special chance (data @ 0xB9A). */
    static const uint8_t kPabilChance[16] = {0, 10, 20, 35, 50, 75, 90, 100, 1, 1, 1, 1, 0, 1, 1, 15};
    /* A4-$666A/-$664A/-$662A/-$65FE: 0x1FB4E resist + multi-hit order (data @ 0x1994). */
    static const uint8_t kPabilResA[32] = {1, 1, 99, 1, 1, 1, 1, 1, 1, 1, 1, 99, 99, 99, 1, 0,
                                          1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0};
    static const uint8_t kPabilResB[32] = {0, 0, 99, 0, 0, 0, 0, 0, 0, 0, 1, 99, 99, 99, 1, 1,
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0};
    static const uint8_t kPabilResC[32] = {28, 29, 99, 23, 24, 25, 26, 28, 29, 26, 21, 99, 99, 99, 26, 26,
                                          21, 24, 23, 0,  26, 24, 0,  23, 26, 0,  23, 0,  0,  31, 28, 0};
    static const uint8_t kPabilOrd[32] = {0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 0,
                                         0, 2, 4, 6, 1, 3, 5, 7, 1, 3, 5, 7, 0, 2, 4, 6};
    /* A4-$7486: 0x4442 luck thresholds (data @ 0xB78). */
    static const uint8_t kLuckThresh[23] = {4,  6,  9,  13, 15, 17, 19, 22, 26, 30, 45, 60,
                                           75, 90, 105, 120, 135, 150, 175, 200, 225, 250, 255};
    for (int i = 0; i < 8; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_STATUS_BIT_TBL + i, kStatusBits[i]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MRES_CHANCE_TBL + i, kMresChance[i]);
        mm2_gs_set_u8(gs.a4(), -0x6F3E + i, kHitDiv[i]);
        mm2_gs_set_u8(gs.a4(), -0x6F36 + i, kSwingDiv[i]);
    }
    for (int i = 0; i < 4; ++i) {
        mm2_gs_set_u8(gs.a4(), -0x6F1A + i, kFleeChance[i]);
        mm2_gs_set_u16(gs.a4(), MM2_GS_ENCASE_DMG_TBL + i * 2, kEncaseDmg[i]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENCASE_MODE_TBL + i, kEncaseMode[i]);
    }
    for (int i = 0; i < 16; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_HIT_TBL + i, kMonHit[i]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_PABIL_CHANCE_TBL + i, kPabilChance[i]);
    }
    for (int i = 0; i < 20; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_FLY_SCREEN_TBL + i, kFlyScreens[i]);
    }
    for (int i = 0; i < 32; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_PABIL_RESIST_A + i, kPabilResA[i]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_PABIL_RESIST_B + i, kPabilResB[i]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_PABIL_RESIST_C + i, kPabilResC[i]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_PABIL_TARGET_ORD + i, kPabilOrd[i]);
    }
    for (int i = 0; i < 23; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_LUCK_THRESH_TBL + i, kLuckThresh[i]);
    }
}

uint8_t CombatSession::spellTargetPower13362(GameStateView &gs, int level) const
{
    /* 0x1333A (mid-label 0x13362): result=10; live=min(-$77BE,10);
     * if level<7: level+4; else if level>=live: 11. Sleep/Web/Silence → $d. */
    uint8_t result = 0x0A;
    uint8_t live = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT);
    if (live > 0x0A) {
        live = 0x0A;
    }
    const int lv = level > 0 ? level : 1;
    if (lv < 7) {
        result = static_cast<uint8_t>(lv + 4);
    } else if (lv >= static_cast<int>(live)) {
        result = static_cast<uint8_t>(result + 1);
    }
    return result;
}

void CombatSession::loadMonsterCombatGlobals(GameStateView &gs, int slot)
{
    /* 0x4C8E: clear then fill mres/undead + -$11AA[0..6] resist-flag bank. */
    for (int i = 0; i < 7; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + i, 0);
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_MRES, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_UNDEAD, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_AC_BIT6, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_AC_BIT7, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_AC, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SWINGS, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_DMG_SIDES, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_PABIL, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_PABIL_CHANCE, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_CHARGE_INIT, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SABIL, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ARCHER, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_MULTIPLIES, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FRIEND_COUNT, 0);
    if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS || !monsters_ || !slots_[slot].alive) {
        return;
    }
    const Mm2MonsterRecord &rec = monsters_->records[slots_[slot].type];
    Mm2MonsterAbilities ab{};
    mm2_monster_decode_abilities(&rec, &ab);
    const uint8_t pabil = rec.fields[MM2_MON_OFF_PABIL - MM2_MONSTER_NAME_SIZE];
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_PABIL, static_cast<uint8_t>(pabil & 0x1F));
    /* 0x4E0A: index = (Pabil&$E0)>>4 into -$7464. */
    const uint8_t pch_idx = static_cast<uint8_t>((pabil & 0xE0) >> 4);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_PABIL_CHANCE,
                  mm2_gs_u8(gs.a4(), MM2_GS_PABIL_CHANCE_TBL + pch_idx));
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SABIL, ab.single_effect);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ARCHER, ab.archer ? 1 : 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_MULTIPLIES, ab.multiplies ? 1 : 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FRIEND_COUNT,
                  static_cast<uint8_t>(MM2_MON_ADD_FRIENDS_COUNT(&ab)));
    const uint8_t ac = rec.fields[MM2_MON_OFF_AC - MM2_MONSTER_NAME_SIZE];
    const uint8_t dmg = rec.fields[MM2_MON_OFF_DAMAGE - MM2_MONSTER_NAME_SIZE];
    const uint8_t speed = rec.fields[MM2_MON_OFF_SPEED - MM2_MONSTER_NAME_SIZE];
    const uint8_t spd2 = rec.fields[MM2_MON_OFF_SPEED2 - MM2_MONSTER_NAME_SIZE];
    const uint8_t mres = rec.fields[MM2_MON_OFF_MRES - MM2_MONSTER_NAME_SIZE];

    if (ac & 0x80) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_AC_BIT7, 1);
    }
    if (ac & 0x40) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_AC_BIT6, 1);
    }
    /* 0x4F0A: AC = (low5)+1; ×10 if bit5. */
    uint8_t ac_val = static_cast<uint8_t>((ac & 0x1F) + 1);
    if (ac & 0x20) {
        ac_val = static_cast<uint8_t>(ac_val * 10);
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_AC, ac_val);
    /* 0x4EAE: swings = (speed[0x14] low4)+1. Byte 0x18 (speed2) feeds initiative
     * (-$11B1 @ 0x4FAA), NOT swings — reading spd2 here inflated melee damage
     * (e.g. Orc speed2=0x0F → 16 swings instead of the correct 1). */
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SWINGS, static_cast<uint8_t>((speed & 0x0F) + 1));
    /* 0x4EB8: -$11BA = (speed[0x14]>>4)+1 — special-charge seed for -$503. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_CHARGE_INIT, static_cast<uint8_t>((speed >> 4) + 1));
    uint8_t sides = static_cast<uint8_t>((dmg & 0x1F) + 1);
    if (dmg & 0x20) {
        if (sides > 0x19) {
            sides = 0xFA;
        } else {
            sides = static_cast<uint8_t>(sides * 10);
        }
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_DMG_SIDES, sides);
    /* -$11AA[0..6]: dmg.b6, dmg.b7, spd2.b6, spd2.b7, mres.b1, mres.b0, mres.b2 */
    if (dmg & 0x40) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + 0, 1);
    }
    if (dmg & 0x80) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + 1, 1);
    }
    if (spd2 & 0x40) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + 2, 1);
    }
    if (spd2 & 0x80) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + 3, 1);
    }
    if (mres & 0x02) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + 4, 1); /* Sleep $d=5 */
    }
    if (mres & 0x01) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + 5, 1); /* Web/Silence $d=6 */
    }
    if (mres & 0x04) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + 6, 1);
    }
    const uint8_t tier = static_cast<uint8_t>((mres >> 5) & 7);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_MRES,
                  mm2_gs_u8(gs.a4(), MM2_GS_MRES_CHANCE_TBL + tier));
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_UNDEAD, ab.undead ? 1 : 0);
    /* 0x4E9C: -$11B6 = (Oabil>>5)&3 flee tier. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLEE_TIER, static_cast<uint8_t>(ab.flee_tier & 3));
}

bool CombatSession::applyHp10ED4(GameStateView &gs, int slot, uint16_t dmg)
{
    /* 0x10EAC/0x10ED4: wake status, then HP subtract or kill → 0x10E5E. */
    CombatMonster &m = slots_[slot];
    uint8_t st = m.status;
    if (st <= 0x80) {
        st = 0;
    }
    st = static_cast<uint8_t>((st & 0xEF) | 0x01);
    m.status = st;
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_STATUS + slot, st);
    mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, dmg);
    mm2_gs_set_u8(gs.a4(), MM2_GS_CUR_MON_SLOT, static_cast<uint8_t>(slot));

    const uint16_t hp = static_cast<uint16_t>(m.hp > 0 ? m.hp : 0);
    if (hp <= dmg) {
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, 0xFFFF);
        const uint32_t kill_xp =
            (monsters_ && m.type < MM2_MONSTER_RECORD_COUNT)
                ? mm2_monster_decode_xp(&monsters_->records[m.type])
                : 0;
        onMonsterKilled(gs, slot, kill_xp);
        return true;
    }
    m.hp = static_cast<int>(hp - dmg);
    mm2_gs_set_u16(gs.a4(), MM2_GS_MONSTER_HP + slot * 2, static_cast<uint16_t>(m.hp));
    return false;
}

void CombatSession::enqueueCombatMessage(const char *msg)
{
    if (!msg || msg[0] == '\0' || msg_queue_len_ >= kMsgQueueCap) {
        return;
    }
    std::snprintf(msg_queue_[msg_queue_len_], sizeof(msg_queue_[0]), "%s", msg);
    ++msg_queue_len_;
}

bool CombatSession::advanceCombatMessageQueue()
{
    /* Returns true if another queued line was shown (stay in AwaitingActionAck). */
    if (msg_queue_len_ <= 0) {
        return false;
    }
    ++msg_queue_idx_;
    if (msg_queue_idx_ >= msg_queue_len_) {
        msg_queue_len_ = 0;
        msg_queue_idx_ = 0;
        return false;
    }
    std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[msg_queue_idx_]);
    return true;
}

void CombatSession::compactMonsterSlot(GameStateView &gs, int slot)
{
    /* 0x10CCE: dec -$77BE; clamp -$524; shift parallel arrays from slot. */
    uint8_t live = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT);
    if (live > 0) {
        --live;
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_COUNT, live);
    }
    if (encounter_live_total_ > 0) {
        --encounter_live_total_;
    }
    uint8_t melee = mm2_gs_u8(gs.a4(), MM2_GS_MELEE_RANGE_N);
    if (melee > live) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_MELEE_RANGE_N, live);
        melee_range_count_ = live;
    }
    /* Overflow-only kill (slot >= 10): no visible-row shift. */
    if (slot >= 0x0A) {
        if (slot < MM2_GS_MONSTER_BATTLE_SLOTS) {
            slots_[slot] = CombatMonster{};
            monster_acted_[slot] = false;
        }
        syncMonsterSlotsToGs(gs);
        /* 0x10CCE tail: play_sound_seq id 8 after compact. */
        audio::playSoundSeq(8, gs.soundsEnabled(), gs.walkBeepEnabled());
        return;
    }
    const int last = (live < 0x0A) ? static_cast<int>(live) : 0x0A;
    for (int i = slot; i < last; ++i) {
        slots_[i] = slots_[i + 1];
        monster_acted_[i] = monster_acted_[i + 1];
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + i,
                      mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + i + 1));
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + i,
                      mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_ACTED + i + 1));
    }
    if (last < MM2_GS_MONSTER_BATTLE_SLOTS) {
        slots_[last] = CombatMonster{};
        monster_acted_[last] = false;
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + last, 0);
        if (last < 10) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + last, 0);
        }
    }
    syncMonsterSlotsToGs(gs);
    /* 0x10CCE tail: play_sound_seq id 8 after compact. */
    audio::playSoundSeq(8, gs.soundsEnabled(), gs.walkBeepEnabled());
}

void CombatSession::applyTreasure10B74(GameStateView &gs, uint8_t mon_type)
{
    /* 0x10B74: treasure byte 0x10 → -$3F12 gems / -$3F10 gold / -$5E28/-$5E29 item tier. */
    if (!monsters_ || mon_type >= MM2_MONSTER_RECORD_COUNT) {
        return;
    }
    const uint8_t treasure =
        monsters_->records[mon_type].fields[MM2_MON_OFF_TREASURE - MM2_MONSTER_NAME_SIZE];
    /* bit2 → gems: rng(1,10) add.w -$3F12 */
    if (treasure & 0x04) {
        const int g = rng_ ? rng_->range(1, 0x0A) : 1;
        const uint16_t cur = mm2_gs_u16(gs.a4(), MM2_GS_FOUND_GEMS);
        mm2_gs_set_u16(gs.a4(), MM2_GS_FOUND_GEMS, static_cast<uint16_t>(cur + g));
    }
    /* bits3-4 → gold tier into -$3F10 (type from mon_type / -$11DE[slot]). */
    const uint8_t gold_tier = static_cast<uint8_t>((treasure >> 3) & 0x03);
    if (gold_tier != 0) {
        uint8_t t = mon_type;
        if (gold_tier == 2) {
            t = static_cast<uint8_t>(t / 0x10);
        } else if (gold_tier >= 3) {
            t = static_cast<uint8_t>(t / 2);
        }
        /* Always rng(1,t) then add — even when t==0 (ASM pushes t then #1). */
        int type_roll = 0;
        if (t > 0 && rng_) {
            type_roll = rng_->range(1, t);
        } else if (t > 0) {
            type_roll = 1;
        }
        uint8_t type_contrib = static_cast<uint8_t>(t + type_roll);
        int base = rng_ ? rng_->range(1, 0x32) : 1;
        base += 6;
        if (gold_tier == 1) {
            type_contrib = 0;
        }
        uint32_t gold = mm2_gs_u32(gs.a4(), MM2_GS_FOUND_GOLD_EXP);
        gold += static_cast<uint32_t>(base);
        gold += static_cast<uint32_t>(type_contrib) << 8;
        mm2_gs_set_u32(gs.a4(), MM2_GS_FOUND_GOLD_EXP, gold);
    }
    /* bits0-1 → item drop tier; keep max in -$5E29, type>>4 in -$5E28 */
    const uint8_t item_tier = static_cast<uint8_t>(treasure & 0x03);
    if (item_tier != 0) {
        const uint8_t best = mm2_gs_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER);
        if (item_tier >= best) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TYPE_HI, static_cast<uint8_t>(mon_type >> 4));
            mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER, item_tier);
        }
    }
}

void CombatSession::printMonsterDown10DFC(GameStateView &gs, int slot, const char *mon_name)
{
    /* 0x10DFC: if -$51E set, skip (eradicate already printed). Else name + runs/goes. */
    if (mm2_gs_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT) != 0) {
        return;
    }
    (void)slot;
    const char *tail =
        mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_FLEE_PRINT) != 0 ? " runs away!" : " goes down!";
    char line[160];
    std::snprintf(line, sizeof(line), "%s%s", mon_name ? mon_name : "?", tail);
    enqueueCombatMessage(line);
}

void CombatSession::monsterFlee1075E(GameStateView &gs, int slot)
{
    /* 0x1075E: set -$5E27, 10DFC, 10CCE, clr -$5E27 — no 10B74 treasure/XP. */
    if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS || !slots_[slot].alive) {
        return;
    }
    char mon_name[MM2_MONSTER_NAME_SIZE + 1] = "?";
    if (monsters_ && slots_[slot].type < MM2_MONSTER_RECORD_COUNT) {
        mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], mon_name, sizeof(mon_name));
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLEE_PRINT, 1);
    mm2_gs_set_u8(gs.a4(), MM2_GS_CUR_MON_SLOT, static_cast<uint8_t>(slot));
    printMonsterDown10DFC(gs, slot, mon_name);
    slots_[slot].hp = 0;
    slots_[slot].alive = false;
    slots_[slot].status = 0;
    compactMonsterSlot(gs, slot);
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLEE_PRINT, 0);
    if (msg_queue_len_ > 0) {
        std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[0]);
        msg_queue_idx_ = 0;
    }
}

void CombatSession::fillFoundItemSlot12212(GameStateView &gs, int slot_index)
{
    /* 0x12212: one FOUND_ITEM slot from -$5E29 tier + -$5E28 type_hi. */
    if (slot_index < 0 || slot_index >= MM2_GS_FOUND_ITEM_SLOTS) {
        return;
    }
    static const uint8_t kThresh[7] = {25, 40, 50, 55, 70, 75, 100};
    /* 8×4: [base, span_t0, span_t1, span_t2] — data hunk A4-$6DCC. */
    static const uint8_t kRows[8][4] = {
        {0, 24, 53, 65},   {65, 13, 19, 26}, {91, 6, 13, 19},  {114, 3, 10, 12},
        {126, 8, 23, 28},  {154, 2, 4, 5},   {159, 22, 34, 48}, {0, 0, 6, 97},
    };
    static const uint8_t kCharges[4] = {5, 60, 200, 0}; /* A4-$6DCF */

    uint8_t tier = mm2_gs_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER);
    if (tier > 2) {
        tier = 2;
        mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER, 2);
    }
    const int roll100 = rng_ ? rng_->range(1, 0x64) : 1;
    int bucket = 0;
    for (; bucket < 7; ++bucket) {
        if (static_cast<uint8_t>(roll100) <= kThresh[bucket]) {
            break;
        }
    }
    const uint8_t base = kRows[bucket][0];
    const uint8_t span = kRows[bucket][tier + 1];
    int id = base;
    if (span > 0) {
        id = base + (rng_ ? rng_->range(1, span) : 1);
    }
    /* mulu #$14 → items.dat record; remake uses bound file. */
    uint8_t charges = 0;
    uint8_t flags = 0;
    if (items_ && id >= 0 && id < MM2_ITEMS_RECORD_COUNT) {
        const Mm2ItemRecord &irec = items_->records[id];
        if (irec.effect_byte != 0) {
            charges = kCharges[tier];
        }
        if (irec.bonus_byte != 0xF0) {
            const uint8_t type_hi = mm2_gs_u8(gs.a4(), MM2_GS_LOOT_ITEM_TYPE_HI);
            if (type_hi >= 2) {
                int fr = rng_ ? rng_->range(1, 7) : 1;
                if (type_hi >= 2 && type_hi <= 0x0C) {
                    fr = rng_ ? rng_->range(1, type_hi) : 1;
                }
                if (type_hi == 0x0D) {
                    fr = (rng_ ? rng_->range(1, 0x15) : 1) + 0x0B;
                }
                flags = static_cast<uint8_t>(fr);
                if (flags >= 5) {
                    const int r2 = rng_ ? rng_->range(1, 0x64) : 0x64;
                    if (r2 < 0x29) {
                        flags = static_cast<uint8_t>(flags | 0x80);
                    } else if (r2 < 0x47) {
                        flags = static_cast<uint8_t>(flags | 0x40);
                    } else {
                        flags = static_cast<uint8_t>(flags | 0xC0);
                    }
                }
            }
        }
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_FOUND_ITEM_ID + slot_index, static_cast<uint8_t>(id));
    mm2_gs_set_u8(gs.a4(), MM2_GS_FOUND_ITEM_FLAGS + slot_index, flags);
    mm2_gs_set_u8(gs.a4(), MM2_GS_FOUND_ITEM_CHARGES + slot_index, charges);
}

void CombatSession::generateVictoryItems123B0(GameStateView &gs)
{
    /* 0x123B0: rng → item count 0..3; each call 0x12206/0x12212, dec -$5E29. */
    if (mm2_gs_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER) == 0 &&
        mm2_gs_u8(gs.a4(), MM2_GS_LOOT_ITEM_TYPE_HI) == 0) {
        return;
    }
    const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
    uint8_t count = 0;
    if (roll < 0x0B) {
        count = 3;
    } else if (roll < 0x2E) {
        count = 2;
    } else if (roll < 0x5B) {
        count = 1;
    }
    for (uint8_t i = 0; i < count && i < MM2_GS_FOUND_ITEM_SLOTS; ++i) {
        if (mm2_gs_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER) != 0) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER,
                          static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER) - 1));
        }
        fillFoundItemSlot12212(gs, static_cast<int>(i));
    }
}

void CombatSession::onMonsterKilled(GameStateView &gs, int slot, uint32_t kill_xp)
{
    /* 0x10E5E: 10C66 drink-match, 10B74 treasure+XP, 10DFC print, 10CCE compact. */
    if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS) {
        return;
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_CUR_MON_SLOT, static_cast<uint8_t>(slot));
    /* 0x10C66: type from -$11DE[-$51D], not a side channel. */
    syncMonsterSlotsToGs(gs);
    const uint8_t mon_type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + slot);
    char mon_name[MM2_MONSTER_NAME_SIZE + 1] = "?";
    if (monsters_ && mon_type < MM2_MONSTER_RECORD_COUNT) {
        mm2_monster_name_to_cstr(&monsters_->records[mon_type], mon_name, sizeof(mon_name));
    }
    events::townSvcArmDrinkMatchOnKill(roster_, launch_, mon_type);

    applyTreasure10B74(gs, mon_type);
    mm2_gs_set_u32(gs.a4(), -0x119E, kill_xp);
    xp_pool_ += kill_xp;
    mm2_gs_set_u32(gs.a4(), MM2_GS_COMBAT_XP_POOL, xp_pool_);

    printMonsterDown10DFC(gs, slot, mon_name);

    slots_[slot].hp = 0;
    slots_[slot].alive = false;
    slots_[slot].status = 0;

    const int killed_slot = slot;
    compactMonsterSlot(gs, killed_slot);

    if (killed_slot >= 0x0A) {
        uint8_t live = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT);
        if (live > 9) {
            const int n = static_cast<int>(live) - 9;
            for (int i = 0; i < n; ++i) {
                xp_pool_ += kill_xp;
            }
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_COUNT, 0x0A);
            encounter_live_total_ = 0x0A;
            mm2_gs_set_u32(gs.a4(), MM2_GS_COMBAT_XP_POOL, xp_pool_);
        }
    }
}

void CombatSession::applySpell108BC(GameStateView &gs, int start_slot, int hit_count, uint8_t mode_d)
{
    /* 0x10894: per-target resist/halve/status/damage + hostable messages. */
    static const char *kStatusNames[] = {"silenced", "weakened",  "frightened", "slept",
                                         "held",     "mindless",  "encased",    "killed"};
    int live = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (slots_[i].alive) {
            ++live;
        }
    }
    if (hit_count > live) {
        hit_count = live;
    }
    if (hit_count < 1) {
        hit_count = 1;
    }
    /* 0x108B2: Holy Word gate bumps max slot scan. */
    int max_slot = 0x0A;
    if (mm2_gs_u8(gs.a4(), MM2_GS_HOLY_WORD_GATE) != 0) {
        max_slot = 0x0B;
    }

    /* 0x108BC: mode 2 → clr mode (skip cast banner) then damage path. */
    if (mm2_gs_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE) == 2) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
    }

    const uint8_t status_mode = mm2_gs_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE);
    uint8_t status_code = mm2_gs_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE);
    const uint16_t saved_dmg = mm2_gs_u16(gs.a4(), MM2_GS_SPELL_DAMAGE);
    const int caster_idx = rosterIndexForPartySlot(active_party_slot_);
    const int caster_level =
        (caster_idx >= 0 && roster_->records[caster_idx].level > 0)
            ? static_cast<int>(roster_->records[caster_idx].level)
            : 1;

    char last_msg[160] = {};
    msg_queue_len_ = 0;
    msg_queue_idx_ = 0;
    int applied = 0;
    for (int s = start_slot; s < MM2_GS_MONSTER_BATTLE_SLOTS && applied < hit_count; ) {
        if (s >= max_slot) {
            break;
        }
        if (!slots_[s].alive) {
            ++s;
            continue;
        }
        loadMonsterCombatGlobals(gs, s);
        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, saved_dmg);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, status_mode);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, status_code);
        mm2_gs_set_u8(gs.a4(), MM2_GS_CUR_MON_SLOT, static_cast<uint8_t>(s));

        char mon_name[MM2_MONSTER_NAME_SIZE + 1] = "?";
        if (monsters_) {
            mm2_monster_name_to_cstr(&monsters_->records[slots_[s].type], mon_name, sizeof(mon_name));
        }

        bool miss = false;
        if (mm2_gs_u8(gs.a4(), MM2_GS_SPELL_SKIP_RESIST) == 0) {
            const uint8_t mres = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_MRES);
            if (mres != 0) {
                const int roll = rng_ ? rng_->range(caster_level, 0x5A) : caster_level;
                if (roll < static_cast<int>(mres)) {
                    miss = true;
                }
            }
            if (!miss && mode_d != 0) {
                const uint8_t idx = static_cast<uint8_t>(mode_d - 1);
                if (mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_FLAG_BASE + idx) != 0) {
                    miss = true;
                }
            }
            if (!miss) {
                const int tr = rng_ ? rng_->range(1, 0xBF) : 1;
                const uint8_t mon_type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + s);
                if (tr <= static_cast<int>(mon_type)) {
                    if (status_mode == 0) {
                        uint16_t d = mm2_gs_u16(gs.a4(), MM2_GS_SPELL_DAMAGE);
                        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, static_cast<uint16_t>(d / 2));
                    } else if (status_mode == 1) {
                        if (status_code < 9) {
                            miss = true;
                        } else {
                            mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, 0x32);
                            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
                        }
                    }
                }
            }
        }
        if (miss && status_mode == 1 && status_code == 8 &&
            mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_UNDEAD) != 0) {
            miss = false;
        }
        if (!miss && mm2_gs_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE) == 0 &&
            mm2_gs_u16(gs.a4(), MM2_GS_SPELL_DAMAGE) == 0) {
            miss = true;
        }

        if (miss) {
            std::snprintf(last_msg, sizeof(last_msg), "%s is not affected!", mon_name);
            enqueueCombatMessage(last_msg);
            ++applied;
            ++s;
            continue;
        }

        bool did_kill = false;
        if (mm2_gs_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE) != 0) {
            uint8_t code = mm2_gs_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE);
            if (code > 0) {
                --code;
            }
            if (code < 7) {
                const uint8_t bit = mm2_gs_u8(gs.a4(), MM2_GS_STATUS_BIT_TBL + code);
                slots_[s].status = static_cast<uint8_t>(slots_[s].status | bit);
                mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_STATUS + s, slots_[s].status);
                const char *sn = (code < 8) ? kStatusNames[code] : "affected";
                std::snprintf(last_msg, sizeof(last_msg), "%s is %s!", mon_name, sn);
                enqueueCombatMessage(last_msg);
            } else {
                const uint32_t kill_xp =
                    (monsters_ && slots_[s].type < MM2_MONSTER_RECORD_COUNT)
                        ? mm2_monster_decode_xp(&monsters_->records[slots_[s].type])
                        : 0;
                std::snprintf(last_msg, sizeof(last_msg), "%s is eradicated!", mon_name);
                enqueueCombatMessage(last_msg);
                /* 0x10AD4: -$51E around 10E5E — skip " goes down!". */
                mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 1);
                onMonsterKilled(gs, s, kill_xp);
                mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 0);
                did_kill = true;
            }
        } else {
            const uint16_t dmg = mm2_gs_u16(gs.a4(), MM2_GS_SPELL_DAMAGE);
            if (slots_[s].hp <= static_cast<int>(dmg)) {
                std::snprintf(last_msg, sizeof(last_msg), "%s takes %u Pts", mon_name,
                              static_cast<unsigned>(dmg));
            } else {
                std::snprintf(last_msg, sizeof(last_msg), "%s takes %u %s", mon_name,
                              static_cast<unsigned>(dmg), dmg == 1 ? "Pt" : "Pts");
            }
            enqueueCombatMessage(last_msg);
            did_kill = applyHp10ED4(gs, s, dmg);
        }
        ++applied;
        /* After compact kill, successor is at same index — do not ++s. */
        if (!did_kill) {
            ++s;
        }
    }
    mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, saved_dmg);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, status_mode);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, status_code);
    mm2_gs_set_u8(gs.a4(), MM2_GS_HOLY_WORD_GATE, 0);
    syncMonsterSlotsToGs(gs);
    if (msg_queue_len_ > 0) {
        std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[0]);
        msg_queue_idx_ = 0;
    } else if (last_msg[0] != '\0') {
        std::snprintf(status_line_, sizeof(status_line_), "%s", last_msg);
    }
}

void CombatSession::applyCastToMonsterTarget(GameStateView &gs, int slot, int flat0)
{
    /* Per-stub dice from $CFF8 dense handlers (picker flat / flat+$30, then subq #2). */
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    Mm2RosterRecord &caster = roster_->records[idx];
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&caster, name, sizeof(name));

    const gameplay::SpellSchool school = gameplay::spellSchoolForClass(caster.class_id);
    const gameplay::SpellMeta *table = gameplay::schoolSpellTable(school);
    const gameplay::SpellMeta *meta =
        (table && flat0 >= 0 && flat0 < gameplay::kSpellsPerSchool) ? &table[flat0] : nullptr;
    const char *spell_name = meta ? meta->name : "?";
    const int level = caster.level > 0 ? static_cast<int>(caster.level) : 1;

    /* Identify Monster @ 0xB760: D43C letter-pick; print name/HP/AC + Y/N flags. */
    if (school == gameplay::SpellSchool::Sorcerer && flat0 == 9) {
        if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS || !slots_[slot].alive || !monsters_) {
            std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
            return;
        }
        loadMonsterCombatGlobals(gs, slot);
        char mon_name[MM2_MONSTER_NAME_SIZE + 1];
        mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], mon_name, sizeof(mon_name));
        const uint16_t hp = mm2_gs_u16(gs.a4(), MM2_GS_MONSTER_HP + slot * 2);
        const uint8_t ac = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_AC);
        const char yn_u = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_UNDEAD) ? 'Y' : 'N';
        const char yn_sp = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_CHARGE_INIT) ? 'Y' : 'N';
        const char yn_bt = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SABIL) ? 'Y' : 'N';
        const char yn_mr = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_MRES) ? 'Y' : 'N';
        msg_queue_len_ = 0;
        msg_queue_idx_ = 0;
        char line[160];
        std::snprintf(line, sizeof(line), "%c %s:", static_cast<char>('A' + slot), mon_name);
        enqueueCombatMessage(line);
        std::snprintf(line, sizeof(line), "HP = %u", static_cast<unsigned>(hp));
        enqueueCombatMessage(line);
        std::snprintf(line, sizeof(line), "AC = %u", static_cast<unsigned>(ac));
        enqueueCombatMessage(line);
        std::snprintf(line, sizeof(line), "Undead (%c)", yn_u);
        enqueueCombatMessage(line);
        std::snprintf(line, sizeof(line), "Special Power (%c)", yn_sp);
        enqueueCombatMessage(line);
        std::snprintf(line, sizeof(line), "Bonus on Touch (%c)", yn_bt);
        enqueueCombatMessage(line);
        std::snprintf(line, sizeof(line), "Magic Resistance (%c)", yn_mr);
        enqueueCombatMessage(line);
        std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[0]);
        msg_queue_idx_ = 0;
        return;
    }

    /* Mass Distortion @ 0xC68E: -$53E = -$53A[slot]/2; 10894 hit=2 mode=0. */
    if (school == gameplay::SpellSchool::Cleric && flat0 == 42) {
        if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS || !slots_[slot].alive) {
            std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
            return;
        }
        const uint16_t hp = mm2_gs_u16(gs.a4(), MM2_GS_MONSTER_HP + slot * 2);
        const uint16_t dmg = static_cast<uint16_t>(hp >> 1);
        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, dmg);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
        applySpell108BC(gs, slot, 2, 0);
        return;
    }

    /* Inferno @ D23E effect=45 → 0xBF44: D390, 1338E($10,$4), hit=10 from slot 0. */
    if (school == gameplay::SpellSchool::Sorcerer && flat0 == 45) {
        const uint16_t dmg = rollSpellDamage133B6(gs, level, 16, 4);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, 0);
        (void)dmg;
        applySpell108BC(gs, 0, 10, 0);
        if (status_line_[0] == '\0') {
            std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
        }
        return;
    }

    /* Moon Ray @ D23E effect=flat+0x30=86 → ASM 0xC566: D390, rng(1,$5B)+9 →
     * monsters hit=10 from slot 0; then party loop: re-roll, cond&=$2F,
     * +$5E += roll, clamp +$5E to +$74. */
    if (school == gameplay::SpellSchool::Cleric && flat0 == 38) {
        auto moon_roll = [this]() -> uint16_t {
            const int roll = rng_ ? rng_->range(1, 0x5B) : 1;
            return static_cast<uint16_t>(roll + 9);
        };
        const uint16_t mon_dmg = moon_roll();
        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, mon_dmg);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, 0);
        applySpell108BC(gs, 0, 10, 0);
        if (roster_ && launch_) {
            for (int p = 0; p < party_count_; ++p) {
                const int ri = rosterIndexForPartySlot(p);
                if (ri < 0 || ri >= MM2_ROSTER_RECORD_COUNT) {
                    continue;
                }
                Mm2RosterRecord &rec = roster_->records[ri];
                const uint16_t heal = moon_roll();
                mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, heal);
                if (rec.condition >= 0x80) {
                    continue;
                }
                rec.condition = static_cast<uint8_t>(rec.condition & 0x2F);
                const unsigned sum =
                    static_cast<unsigned>(rec.hp_max) + static_cast<unsigned>(heal);
                rec.hp_max = static_cast<uint16_t>(sum > 0xFFFFu ? 0xFFFFu : sum);
                if (rec.hp_max > rec.hp_current) {
                    rec.hp_max = rec.hp_current;
                }
            }
        }
        if (status_line_[0] == '\0') {
            std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
        }
        return;
    }

    /* Prismatic Light @ sparse→0xBDCE: D390 random status (same leaf family as
     * combat Wizard Eye 0xBDA6): rng(1,9)→-$53C; if 7: rng(1,4)→-$521; hit=10. */
    if (school == gameplay::SpellSchool::Sorcerer && flat0 == 39) {
        const int st = rng_ ? rng_->range(1, 9) : 1;
        if (st == 7) {
            const int tier = rng_ ? rng_->range(1, 4) : 1;
            mm2_gs_set_u8(gs.a4(), MM2_GS_ENCASE_TIER, static_cast<uint8_t>(tier));
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, static_cast<uint8_t>(st));
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 1);
        applySpell108BC(gs, 0, 0x0A, 0);
        if (msg_queue_len_ > 0) {
            std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[0]);
            msg_queue_idx_ = 0;
        } else if (status_line_[0] == '\0') {
            std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
        }
        return;
    }

    /* kind: 0=1338E  1=direct rng  2=status  3=fixed
     * hit_count 0xFF → 0x1333A(level); mode_d = 10894 $d (0x1086E index) */
    struct SpellFx {
        uint8_t flat;
        uint8_t kind;
        uint16_t a;
        uint16_t b;
        uint8_t c;
        uint8_t hit_count;
        uint8_t status_mode;
        uint8_t mode_d;
    };

    static const SpellFx kSorcFx[] = {
        /* 10894 pushes: mode_d, slot, hit (first/last). 1338E RTL (b,a) from push a,b. */
        {2, 0, 5, 1, 0, 1, 0, 0},       /* Toxic Cloud: 1338E(5,1), hit=1, $d=0 */
        {3, 1, 5, 1, 3, 1, 0, 1},       /* Acid Spray: rng(1,5)+3, hit=1, $d=1 */
        {6, 2, 0, 0, 4, 0xFF, 1, 5},    /* Sleep: status=4, hit=13362, $d=5 */
        {8, 1, 9, 1, 7, 1, 0, 2},       /* Lightning Bolt: rng(1,9)+7, hit=1, $d=2 */
        {14, 0, 5, 3, 0, 1, 0, 4},      /* Fire Bolt: 1338E(5,3), hit=1, $d=4 */
        {17, 0, 5, 1, 0, 4, 0, 2},      /* Lightning Bolt II?: 1338E(5,1), hit=4, $d=2 */
        {18, 2, 0, 0, 5, 0xFF, 1, 6},   /* Web: status=5, hit=13362, $d=6 */
        {20, 0, 0, 6, 0, 1, 0, 3},      /* Ice Bolt: 1338E(0,6), hit=1, $d=3 */
        {21, 2, 0, 0, 6, 5, 1, 0},      /* status=6, hit=5, $d=0 */
        /* Fire Ball: 1338E(5,1), hit=6, $d=1 */
        {22, 0, 5, 1, 0, 6, 0, 1},
        {26, 3, 0x64, 0, 0, 1, 0, 0},   /* Finger of Death: dmg=100, hit=1, $d=0 */
        {27, 2, 0, 0, 8, 3, 1, 6},      /* status=8, hit=3, $d=6 */
        /* Sand Storm: 1338E(7,1), hit=10, $d=0 */
        {28, 0, 7, 1, 0, 10, 0, 0},
        {31, 2, 0, 0, 9, 3, 1, 0},      /* Feeble Mind: status=9, hit=3, $d=0 */
        {33, 0, 0, 10, 0, 3, 0, 3},     /* 1338E(0,10), hit=3, $d=3 */
        {35, 0, 0, 20, 0, 1, 0, 2},     /* 1338E(0,20), hit=1, $d=2 */
        /* Dancing Sword @ D390 */
        {36, 0, 11, 1, 0, 10, 0, 0},
        {40, 0, 21, 19, 0, 1, 0, 1},    /* Implosion?: 1338E(21,19), hit=1, $d=1 */
        /* Mega Volts / Meteor / Inferno / Star Burst handled above or auto */
        {41, 0, 9, 7, 0, 10, 0, 0},
        {42, 1, 0x15, 1, 0x18, 0xFE, 0, 0},
        {44, 3, 0x3E8, 0, 0, 1, 0, 0},  /* Incinerate: dmg=1000, hit=1, $d=0 */
        {46, 1, 0xA1, 1, 0x27, 0xFE, 0, 0},
    };
    static const SpellFx kClerFx[] = {
        {0, 2, 0, 0, 3, 6, 1, 0},
        {10, 1, 12, 1, 3, 1, 0, 0},     /* Pain: rng(1,12)+3, hit=1, $d=0 */
        {12, 2, 0, 0, 1, 0xFF, 1, 6},   /* Silence */
        {13, 2, 0, 0, 2, 10, 1, 0},
        /* Cold Ray: fixed 25, hit=5, $d=3 */
        {14, 3, 0x19, 0, 0, 5, 0, 3},
        /* Immobilize: status=5, hit=5, $d=6 (not fixed dmg) */
        {17, 2, 0, 0, 5, 5, 1, 6},
        /* Acid Spray: rng(1,49)+11, hit=3, $d=4 */
        {20, 1, 49, 1, 11, 3, 0, 4},
        {26, 2, 0, 0, 7, 1, 1, 0}, /* Air Encasement: -$521=1, hit=1, $d=0 */
        {27, 1, 33, 1, 7, 10, 0, 0},
        {29, 2, 0, 0, 5, 6, 1, 0},
        {34, 2, 0, 0, 7, 1, 1, 3}, /* Water Encasement: -$521=2, $d=3 */
        {36, 2, 0, 0, 7, 1, 1, 4}, /* Earth Encasement: -$521=3, $d=4 */
        {37, 1, 400, 255, 0, 1, 0, 0}, /* Fiery Flail: rng($FF,$190), hit=1 */
        {40, 2, 0, 0, 7, 1, 1, 1}, /* Fire Encasement: -$521=4, $d=1 */
    };

    const SpellFx *fx = nullptr;
    const SpellFx *fx_table = (school == gameplay::SpellSchool::Sorcerer) ? kSorcFx : kClerFx;
    const int fx_n = (school == gameplay::SpellSchool::Sorcerer)
                         ? static_cast<int>(sizeof(kSorcFx) / sizeof(kSorcFx[0]))
                         : static_cast<int>(sizeof(kClerFx) / sizeof(kClerFx[0]));
    for (int i = 0; i < fx_n; ++i) {
        if (fx_table[i].flat == static_cast<uint8_t>(flat0)) {
            fx = &fx_table[i];
            break;
        }
    }

    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, 0);
    mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, 0);

    if (!fx) {
        std::snprintf(status_line_, sizeof(status_line_), "%s casts %s (stub).", name, spell_name);
        return;
    }

    /* Front-rank / in-melee fail @ 0xBA56/BB28/BBD0/BD02/C242/C2CC:
     * cmp.b -$524,slot; bcc apply; else 0xD29C "* Spell Failed *".
     * Sorc 18/22/26/33, Cleric 14/20 (Cold Ray / Acid Spray). */
    {
        bool gated = false;
        if (school == gameplay::SpellSchool::Sorcerer) {
            gated = (flat0 == 18 || flat0 == 22 || flat0 == 26 || flat0 == 33);
        } else if (school == gameplay::SpellSchool::Cleric) {
            gated = (flat0 == 14 || flat0 == 20);
        }
        if (gated) {
            const uint8_t melee_n = mm2_gs_u8(gs.a4(), MM2_GS_MELEE_RANGE_N);
            if (slot >= 0 && static_cast<uint8_t>(slot) < melee_n) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                return;
            }
        }
    }

    uint8_t hits = fx->hit_count;
    if (hits == 0xFF) {
        hits = spellTargetPower13362(gs, level);
    } else if (hits == 0xFE) {
        /* 0xBE78/0xBF50: hit count = live monsters -$77BE. */
        hits = static_cast<uint8_t>(countAliveMonsters());
        if (hits == 0) {
            hits = 1;
        }
        /* addq.b #1,-$520 (Holy Word gate / extended scan). */
        uint8_t gate = mm2_gs_u8(gs.a4(), MM2_GS_HOLY_WORD_GATE);
        if (gate < 0xFF) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_HOLY_WORD_GATE, static_cast<uint8_t>(gate + 1));
        }
    }
    const uint8_t mode_d = fx->mode_d;

    uint16_t dmg = 0;
    if (fx->kind == 0) {
        dmg = rollSpellDamage133B6(gs, level, static_cast<uint8_t>(fx->a & 0xFF),
                                   static_cast<uint8_t>(fx->b & 0xFF));
    } else if (fx->kind == 1) {
        const int hi = fx->a > 0 ? static_cast<int>(fx->a) : 1;
        const int lo = fx->b > 0 ? static_cast<int>(fx->b) : 1;
        const int roll = rng_ ? rng_->range(lo, hi) : lo;
        dmg = static_cast<uint16_t>(roll + fx->c);
        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, dmg);
    } else if (fx->kind == 3) {
        dmg = fx->a;
        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE, dmg);
    } else {
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, fx->status_mode);
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_CODE, fx->c);
        /* 0xC332/C494/C4DA/C620: Encasement spells write -$521 tier. */
        if (school == gameplay::SpellSchool::Cleric && fx->c == 7) {
            uint8_t tier = 0;
            if (flat0 == 26) {
                tier = 1;
            } else if (flat0 == 34) {
                tier = 2;
            } else if (flat0 == 36) {
                tier = 3;
            } else if (flat0 == 40) {
                tier = 4;
            }
            if (tier != 0) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_ENCASE_TIER, tier);
            }
        }
    }

    applySpell108BC(gs, slot, hits > 0 ? hits : 1, mode_d);
    if (fx->kind == 2 && status_line_[0] == '\0') {
        std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
    }
}

int CombatSession::countAliveMonsters() const
{
    int n = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (slots_[i].alive) {
            ++n;
        }
    }
    return n;
}

int CombatSession::nthAliveMonster(int n) const
{
    int seen = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (!slots_[i].alive) {
            continue;
        }
        if (seen == n) {
            return i;
        }
        ++seen;
    }
    return -1;
}

void CombatSession::beginPlayerStrike(GameStateView &gs, bool shooting, bool pick_target)
{
    /* 0x111DA: arg $FF → prompt; else use arg as target index. Single live
     * forces arg←0 (auto). A pushes 0; F/S push $FF. */
    const int alive = countAliveMonsters();
    if (alive <= 0) {
        return;
    }
    if (!pick_target || alive <= 1) {
        resolvePlayerAttack(gs, shooting, nthAliveMonster(0));
        return;
    }
    attack_pick_shooting_ = shooting;
    state_ = CombatState::AwaitingAttackTarget;
    /* Strings @ 0x11604 "Fight" / 0x1160A "Shoot" + which (A-x). */
    std::snprintf(status_line_, sizeof(status_line_), "%s which (A-%c)?",
                  shooting ? "Shoot" : "Fight", static_cast<char>('A' + alive - 1));
}

bool CombatSession::tickAttackTarget(GameStateView &gs, char key)
{
    /* 0x11290..0x112D2: 'A'.. last live letter → index; then 0x112DE resolve. */
    if (key < 'A' || key > 'J') {
        return false;
    }
    const int letter = key - 'A';
    const int alive = countAliveMonsters();
    if (letter < 0 || letter >= alive) {
        return false;
    }
    const int slot = nthAliveMonster(letter);
    if (slot < 0) {
        return false;
    }
    const bool shooting = attack_pick_shooting_;
    attack_pick_shooting_ = false;
    resolvePlayerAttack(gs, shooting, slot);
    beginActionAck(gs);
    return true;
}

void CombatSession::resolvePlayerAttack(GameStateView &gs, bool shooting, int target_slot)
{
    /* Player attack resolve inside 0x111DA @ 0x112DE..0x115F8 (not 0xCD90 —
     * that is the skill/spell dispatcher; 0x10118 is monster-slot swap). */
    const int target = target_slot >= 0 ? target_slot : firstAliveMonster();
    if (target < 0 || target >= MM2_GS_MONSTER_BATTLE_SLOTS || !slots_[target].alive) {
        return;
    }
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    Mm2RosterRecord &attacker = roster_->records[idx];
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&attacker, name, sizeof(name));

    loadMonsterCombatGlobals(gs, target);
    mm2_gs_set_u8(gs.a4(), MM2_GS_CUR_MON_SLOT, static_cast<uint8_t>(target));
    mm2_gs_set_u8(gs.a4(), -0x5E32, shooting ? 1 : 0); /* ranged latch */
    mm2_gs_set_u8(gs.a4(), -0x5E2A, 0);
    mm2_gs_set_u8(gs.a4(), -0x5E2F, 0);
    mm2_gs_set_u8(gs.a4(), -0x5E30, 0);
    mm2_gs_set_u8(gs.a4(), -0x5E31, 0);
    mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, 0);

    const uint8_t cls = attacker.class_id;
    const uint8_t level = attacker.level > 0 ? attacker.level : 1;
    const uint8_t hit_div = mm2_gs_u8(gs.a4(), -0x6F3E + (cls < 8 ? cls : 0));
    const uint8_t swing_div = mm2_gs_u8(gs.a4(), -0x6F36 + (cls < 8 ? cls : 0));
    const uint8_t hd = hit_div > 0 ? hit_div : 1;
    const uint8_t sd = swing_div > 0 ? swing_div : 1;
    mm2_gs_set_u8(gs.a4(), -0x5E2C, static_cast<uint8_t>(level / hd));
    uint8_t swings = static_cast<uint8_t>(level / sd + 1);
    mm2_gs_set_u8(gs.a4(), -0x5E2E, swings);
    mm2_gs_set_u8(gs.a4(), -0x5E2D, swings);

    /* +$4C..+$4F are combat weapon sides/bonus (roster "spells[0..3]"). */
    mm2_gs_set_u8(gs.a4(), -0x5E30, attacker.spells[0]); /* +$4C */
    mm2_gs_set_u8(gs.a4(), -0x5E2B, attacker.spells[1]); /* +$4D */
    if (shooting) {
        mm2_gs_set_u8(gs.a4(), -0x5E2B, attacker.spells[3]); /* +$4F */
        if (cls == 2) { /* Archer: addend = rng(1,min(level,100)) */
            uint8_t lv = level > 0x64 ? 0x64 : level;
            mm2_gs_set_u8(gs.a4(), -0x5E2F, static_cast<uint8_t>(rng_->range(1, lv > 0 ? lv : 1)));
        } else {
            mm2_gs_set_u8(gs.a4(), -0x5E30, attacker.spells[2]); /* +$4E */
        }
    }
    {
        uint8_t add = mm2_gs_u8(gs.a4(), -0x5E2F);
        add = static_cast<uint8_t>(add + mm2_gs_u8(gs.a4(), -0x5E2B));
        add = static_cast<uint8_t>(add + gameplay::restSpellBonusFactor(attacker.might_base));
        mm2_gs_set_u8(gs.a4(), -0x5E2F, add);
        uint8_t hit_b = mm2_gs_u8(gs.a4(), -0x5E2B);
        hit_b = static_cast<uint8_t>(hit_b + gameplay::restSpellBonusFactor(attacker.accuracy_base));
        hit_b = static_cast<uint8_t>(hit_b + mm2_gs_u8(gs.a4(), MM2_GS_BLESS_COUNTER));
        mm2_gs_set_u8(gs.a4(), -0x5E2B, hit_b);
    }

    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[slots_[target].type], mon_name, sizeof(mon_name));
    const char *verb = shooting ? "shoots" : "attacks";

    int hits = 0;
    for (uint8_t s = 0; s < swings; ++s) {
        bool hit = false;
        const int r100 = rng_->range(1, 0x64);
        if (r100 < 6) {
            hit = true;
        } else if (r100 < 9) {
            hit = false;
        } else {
            uint8_t base = 0x19;
            if ((attacker.condition & 0x01) != 0) {
                base = 3;
            }
            int ceiling = static_cast<int>(base) + mm2_gs_u8(gs.a4(), -0x5E2C);
            if (ceiling > 0xFA) {
                ceiling = 0xFA;
            }
            int roll = rng_->range(1, ceiling > 0 ? ceiling : 1);
            roll += mm2_gs_u8(gs.a4(), -0x5E2B);
            if (roll > 0xFF) {
                hit = true;
            } else if (roll <= 0x0A) {
                hit = false;
            } else {
                int lo = roll & 0xFF;
                lo -= mm2_gs_u8(gs.a4(), MM2_GS_BUFF_79A5);
                if (lo >= 0x80) {
                    hit = false;
                } else {
                    hit = (lo >= static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_AC)));
                }
            }
        }
        if (!hit) {
            continue;
        }
        ++hits;
        uint8_t sides = mm2_gs_u8(gs.a4(), -0x5E30);
        int piece = rng_->range(1, sides > 0 ? sides : 1);
        piece += mm2_gs_u8(gs.a4(), -0x5E2F);
        if (piece > 0xFA) {
            piece = 1;
        }
        mm2_gs_set_u8(gs.a4(), -0x5E31, static_cast<uint8_t>(mm2_gs_u8(gs.a4(), -0x5E31) + 1));
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY,
                       static_cast<uint16_t>(mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY) + piece));
    }
    if (hits > 0) {
        const uint16_t bonus = mm2_gs_u8(gs.a4(), -0x7999);
        if (bonus != 0) {
            mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY,
                           static_cast<uint16_t>(mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY) + bonus));
        }
    }
    if (!shooting) {
        /* Crit: Ninja ×2 / Robber ×4 via roll vs +$72 (roster byte; labeled spell_level). */
        uint8_t luck = attacker.spell_level;
        if (luck > 0x64) {
            luck = 0x64;
        }
        const int lr = rng_->range(1, static_cast<int>(luck) + 0x64);
        if (cls == 5) { /* Ninja */
            if (lr > 0x5A || lr < 5) {
                mm2_gs_set_u8(gs.a4(), -0x5E2A, 1);
                mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY,
                               static_cast<uint16_t>(mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY) * 2));
            }
        } else if (cls == 6) { /* Robber */
            if (lr > 0x5E || lr < 5) {
                mm2_gs_set_u8(gs.a4(), -0x5E2A, 2);
                mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY,
                               static_cast<uint16_t>(mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY) * 4));
            }
        }
    }

    const uint16_t dmg = mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY);
    if (hits == 0 || dmg == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s - %s is not affected!", name, verb,
                      mon_name, mon_name);
        syncMonsterSlotsToGs(gs);
        return;
    }
    /* Connecting player swing — same alert family as monster hit (id 9). */
    audio::playSoundSeq(9, gs.soundsEnabled(), gs.walkBeepEnabled());
    const bool killed = applyHp10ED4(gs, target, dmg);
    if (killed) {
        if (msg_queue_len_ > 0) {
            std::snprintf(status_line_, sizeof(status_line_), "%s %s %s for %u - %s", name, verb, mon_name,
                          static_cast<unsigned>(dmg), msg_queue_[0]);
            msg_queue_len_ = 0;
            msg_queue_idx_ = 0;
        } else {
            std::snprintf(status_line_, sizeof(status_line_), "%s %s %s for %u.", name, verb, mon_name,
                          static_cast<unsigned>(dmg));
        }
    } else {
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s for %u damage.", name, verb, mon_name,
                      static_cast<unsigned>(dmg));
        syncMonsterSlotsToGs(gs);
    }
}

void CombatSession::resolvePlayerBlock()
{
    party_blocking_[active_party_slot_] = true; /* 0x11B0A: halves incoming melee damage this round. */
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&roster_->records[idx], name, sizeof(name));
    std::snprintf(status_line_, sizeof(status_line_), "%s blocks.", name);
}

void CombatSession::beginExchange()
{
    /* 0x20FF2: first pick pre-filled from -$4F5; prompt second digit. */
    const int pc = party_count_ > 0 ? party_count_ : 1;
    state_ = CombatState::AwaitingExchangeWith;
    std::snprintf(status_line_, sizeof(status_line_), "Exchange with (1-%d)?", pc);
}

bool CombatSession::tickExchange(GameStateView &gs, char key, bool escape)
{
    if (escape || key == 0x1B) {
        state_ = CombatState::AwaitingCommand;
        setStatus("");
        return true;
    }
    if (key < '1' || key > '9') {
        return false;
    }
    const int with = key - '1';
    if (with < 0 || with >= party_count_) {
        return false;
    }
    const int cur = active_party_slot_;
    if (cur < 0 || cur >= party_count_) {
        state_ = CombatState::AwaitingCommand;
        setStatus("");
        return true;
    }
    if (with != cur) {
        exchangePartySlots(gs, cur, with);
    }
    /* 0x20FF2 returns 1 → 0x11B44 may 0x12848 redraw (host). */
    state_ = CombatState::AwaitingActionAck; /* sentinel: caller finishes turn */
    setStatus("Exchanged.");
    return true;
}

void CombatSession::exchangePartySlots(GameStateView &gs, int slot_a, int slot_b)
{
    /* 0x20FF2: swap word -$796A[a/b] and byte acted table -$5E40[a/b]. */
    if (!launch_ || slot_a < 0 || slot_b < 0 || slot_a >= party_count_ || slot_b >= party_count_) {
        return;
    }
    auto *launch = const_cast<Mm2PartyLaunch *>(launch_);
    const int16_t ra = launch->roster_slots[slot_a];
    launch->roster_slots[slot_a] = launch->roster_slots[slot_b];
    launch->roster_slots[slot_b] = ra;

    const uint16_t wa = mm2_gs_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_a * 2);
    const uint16_t wb = mm2_gs_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_b * 2);
    mm2_gs_set_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_a * 2, wb);
    mm2_gs_set_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + slot_b * 2, wa);

    const bool acted = party_acted_[slot_a];
    party_acted_[slot_a] = party_acted_[slot_b];
    party_acted_[slot_b] = acted;
    const uint8_t ga = mm2_gs_u8(gs.a4(), MM2_GS_PARTY_ACTED + slot_a);
    const uint8_t gb = mm2_gs_u8(gs.a4(), MM2_GS_PARTY_ACTED + slot_b);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_ACTED + slot_a, gb);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_ACTED + slot_b, ga);

    const bool blk = party_blocking_[slot_a];
    party_blocking_[slot_a] = party_blocking_[slot_b];
    party_blocking_[slot_b] = blk;
}

void CombatSession::shrinkPartyAfterCharRun(GameStateView &gs, int runner_slot)
{
    /* 0x116D8..0x1174A: subq -$795A; if runner not last, swap with last then
     * write $FFFF into vacated end slot; clamp -$5E4D; clr -$4F8. */
    if (party_count_ <= 0 || !launch_) {
        return;
    }
    if (runner_slot < 0 || runner_slot >= party_count_) {
        return;
    }
    auto *launch = const_cast<Mm2PartyLaunch *>(launch_);
    const int last = party_count_ - 1;
    if (runner_slot < last) {
        exchangePartySlots(gs, runner_slot, last);
    }
    launch->roster_slots[last] = -1;
    mm2_gs_set_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + last * 2, 0xFFFF);
    party_acted_[last] = false;
    party_blocking_[last] = false;
    mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_ACTED + last, 0);

    --party_count_;
    launch->party_count = party_count_;
    mm2_gs_set_u16(gs.a4(), MM2_GS_PARTY_COUNT, static_cast<uint16_t>(party_count_));

    if (front_rank_count_ > party_count_) {
        front_rank_count_ = party_count_;
        mm2_gs_set_u8(gs.a4(), MM2_GS_FRONT_RANK_N,
                      static_cast<uint8_t>(front_rank_count_ > 0 ? front_rank_count_ : 0));
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, 0);
}

void CombatSession::resolvePlayerRun(GameStateView &gs, const world::MapWorld &world)
{
    /* 0x116B0 / 0x11B9A: rng(1,100) < -$560D → -$5E4C=1 + shrink; leave at
     * round-end check (0x13282). Fail: message; turn always spent. */
    (void)world;
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    char name[MM2_ROSTER_NAME_SIZE + 1] = "Hero";
    if (idx >= 0) {
        mm2_roster_name_to_cstr(&roster_->records[idx], name, sizeof(name));
    }
    const uint8_t thresh = mm2_gs_u8(gs.a4(), MM2_GS_RETREAT_DIFF);
    const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
    if (roll < static_cast<int>(thresh)) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_PARTY_RAN_LATCH, 1);
        shrinkPartyAfterCharRun(gs, active_party_slot_);
        std::snprintf(status_line_, sizeof(status_line_), "%s flees!", name);
        return;
    }
    std::snprintf(status_line_, sizeof(status_line_), "%s fails to flee!", name);
}

void CombatSession::resolveMonsterTurn(GameStateView &gs, int slot)
{
    /* 0x1064C: slot → -$51D; inc MONSTER_ACTED; 0x4C8E; clr -$4F6; status gates. */
    active_monster_slot_ = slot;
    active_party_slot_ = -1;
    if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS || !slots_[slot].alive) {
        return;
    }
    CombatMonster &m = slots_[slot];
    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[m.type], mon_name, sizeof(mon_name));

    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_TURN_SLOT, static_cast<uint8_t>(slot));
    mm2_gs_set_u8(gs.a4(), MM2_GS_CUR_MON_SLOT, static_cast<uint8_t>(slot));
    if (slot < 10) {
        const uint8_t acted = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_ACTED + slot);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + slot, static_cast<uint8_t>(acted + 1));
    }
    loadMonsterCombatGlobals(gs, slot);
    mm2_gs_set_u8(gs.a4(), -0x4F6, 0); /* 0x10684 */

    /* 0x10688: status & $30 (held/asleep) → RTS. */
    if ((m.status & 0x30) != 0) {
        return;
    }

    /* 0x106B0: status >= $80 (encased) → DoT via -$6F26/-$6F1E + 0x10894. */
    if (m.status >= 0x80) {
        uint8_t idx = mm2_gs_u8(gs.a4(), MM2_GS_ENCASE_TIER);
        if (idx != 0) {
            --idx;
        }
        if (idx > 3) {
            idx = 3;
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 2); /* 0x108BC clears → damage */
        mm2_gs_set_u16(gs.a4(), MM2_GS_SPELL_DAMAGE,
                       mm2_gs_u16(gs.a4(), MM2_GS_ENCASE_DMG_TBL + idx * 2));
        const uint8_t mode_d = mm2_gs_u8(gs.a4(), MM2_GS_ENCASE_MODE_TBL + idx);
        applySpell108BC(gs, slot, 1, mode_d);
        if (msg_queue_len_ > 0) {
            std::snprintf(status_line_, sizeof(status_line_), "%s", msg_queue_[0]);
            msg_queue_idx_ = 0;
        }
        return;
    }

    /* 0x10716 / 0x10814: status bit6 → "wanders mindlessly!" (no flee/attack). */
    if ((m.status & 0x40) != 0) {
        std::snprintf(status_line_, sizeof(status_line_), "%s wanders mindlessly!", mon_name);
        enqueueCombatMessage(status_line_);
        beginActionAck(gs);
        return;
    }

    /* 0x10720–0x10776 flee: Entrapment blocks; else -$11B6 tier vs -$6FC2 + rng. */
    if (mm2_gs_u8(gs.a4(), MM2_GS_ENTRAPMENT) == 0) {
        const uint8_t tier = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_FLEE_TIER) & 3;
        const uint8_t chance = mm2_gs_u8(gs.a4(), -0x6F1A + tier);
        const uint8_t party_mod = mm2_gs_u8(gs.a4(), MM2_GS_PICKER_TIER_MOD);
        if (chance < party_mod) {
            const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
            if (roll <= 0x32) {
                monsterFlee1075E(gs, slot);
                return;
            }
        }
    }

    /* 0x10786 → 0x105B6 special gate → 0x10002 group / else melee/ranged/10118. */
    if (monsterSpecialGate105B6(gs, slot)) {
        const uint8_t pabil = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_PABIL);
        if (pabil < 0x0F || pabil == 0x1D || pabil >= 0x1F) {
            monsterGroupAttack10002(gs, slot);
        } else if ((slots_[slot].status & 0x02) != 0 ||
                   (mm2_gs_u8(gs.a4(), MM2_GS_TILE_RT_FLAGS) & 0x02) != 0) {
            /* 0x107EE: status bit1 or tile -$55D6 bit1 → 0x1042C. */
            spellFailedBanner1042C(gs);
        } else {
            monsterGroupAttack10002(gs, slot);
        }
    } else {
        const uint8_t melee_n = mm2_gs_u8(gs.a4(), MM2_GS_MELEE_RANGE_N);
        if (static_cast<uint8_t>(slot) < melee_n) {
            /* 0x1059E: clr ranged latch; 0x103BA target; 0x10478 damage. */
            mm2_gs_set_u8(gs.a4(), -0x5E32, 0); /* 0x105A2: melee, not shoots */
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_STATUS_MODE, 0);
            const int target_slot = pickMonsterMeleeTarget103BA(gs);
            if (target_slot >= 0) {
                deliverMonsterHit10478(gs, slot, target_slot, mon_name);
            }
        } else if (mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_ARCHER) != 0) {
            const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
            if (roll <= 0x50) {
                monsterRangedAttack10584(gs, slot);
            } else {
                monsterAdvance10118(gs, slot);
            }
        } else {
            /* 0x10118: out-of-melee / archer-miss → advance swap if AC bit6. */
            monsterAdvance10118(gs, slot);
        }
    }

    /* 0x10838: clr -$5E32; 0x1061E/0x1265E highlight via panelView highlight_slot. */
    mm2_gs_set_u8(gs.a4(), -0x5E32, 0);
}

void CombatSession::spellFailedBanner1042C(GameStateView &gs)
{
    /* 0x1042C: clr panel; print "*** Spell Failed ***"; delay; 0x132E6. */
    std::snprintf(status_line_, sizeof(status_line_), "*** Spell Failed ***");
    enqueueCombatMessage(status_line_);
    beginActionAck(gs);
}

void CombatSession::monsterAdvance10118(GameStateView &gs, int slot)
{
    /* 0x10118: if current has -$11AB (AC bit6), swap with first melee-range
     * slot that lacks it; print "advances!"; else fall through to ack. */
    loadMonsterCombatGlobals(gs, slot);
    if (mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_AC_BIT6) == 0) {
        return;
    }
    syncMonsterSlotsToGs(gs);
    const uint8_t melee_n = mm2_gs_u8(gs.a4(), MM2_GS_MELEE_RANGE_N);
    bool swapped = false;
    for (int i = 0; i < static_cast<int>(melee_n) && i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (i == slot || !slots_[i].alive) {
            continue;
        }
        loadMonsterCombatGlobals(gs, i);
        if (mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_AC_BIT6) != 0) {
            continue;
        }
        /* Swap parallel battle arrays + slots_[] between `slot` and `i`. */
        const CombatMonster tmp = slots_[slot];
        slots_[slot] = slots_[i];
        slots_[i] = tmp;
        const bool acted = monster_acted_[slot];
        monster_acted_[slot] = monster_acted_[i];
        monster_acted_[i] = acted;

        const uint16_t hp_a = mm2_gs_u16(gs.a4(), MM2_GS_MONSTER_HP + slot * 2);
        const uint16_t hp_b = mm2_gs_u16(gs.a4(), MM2_GS_MONSTER_HP + i * 2);
        mm2_gs_set_u16(gs.a4(), MM2_GS_MONSTER_HP + slot * 2, hp_b);
        mm2_gs_set_u16(gs.a4(), MM2_GS_MONSTER_HP + i * 2, hp_a);
        const uint8_t st_a = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_STATUS + slot);
        const uint8_t st_b = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_STATUS + i);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_STATUS + slot, st_b);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_STATUS + i, st_a);
        const uint8_t sp_a = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SPEED + slot);
        const uint8_t sp_b = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SPEED + i);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SPEED + slot, sp_b);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SPEED + i, sp_a);
        const uint8_t ch_a = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + slot);
        const uint8_t ch_b = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + i);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + slot, ch_b);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + i, ch_a);
        const uint8_t ty_a = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + slot);
        const uint8_t ty_b = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + i);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + slot, ty_b);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + i, ty_a);
        if (slot < 10 && i < 10) {
            const uint8_t ac_a = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_ACTED + slot);
            const uint8_t ac_b = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_ACTED + i);
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + slot, ac_b);
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_ACTED + i, ac_a);
        }

        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_TURN_SLOT, static_cast<uint8_t>(i));
        mm2_gs_set_u8(gs.a4(), MM2_GS_CUR_MON_SLOT, static_cast<uint8_t>(i));
        active_monster_slot_ = i;
        char name[MM2_MONSTER_NAME_SIZE + 1];
        mm2_monster_name_to_cstr(&monsters_->records[slots_[i].type], name, sizeof(name));
        std::snprintf(status_line_, sizeof(status_line_), "%s advances!", name);
        enqueueCombatMessage(status_line_);
        beginActionAck(gs);
        swapped = true;
        break;
    }
    loadMonsterCombatGlobals(gs, active_monster_slot_ >= 0 ? active_monster_slot_ : slot);
    if (!swapped) {
        /* 0x1031A: jsr 0x10082 then 0x132E6. */
        monsterWaitsOrAddsFriends10082(gs, slot);
        beginActionAck(gs);
    }
}

void CombatSession::monsterWaitsOrAddsFriends10082(GameStateView &gs, int slot)
{
    /* 0x10082: load globals; maybe grow live via multiply; print verb. */
    loadMonsterCombatGlobals(gs, slot);
    uint8_t added = 0;
    if (mm2_gs_u8(gs.a4(), MM2_GS_ADDS_FRIENDS_LATCH) == 0 &&
        mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_MULTIPLIES) != 0) {
        const uint8_t live = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT);
        const uint8_t mon_type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + slot);
        const uint8_t overflow = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_OVERFLOW_TYPE);
        if (live > 0x0A && live < 0x6E && mon_type == overflow) {
            const uint8_t grown = static_cast<uint8_t>(live + (live - 0x0A));
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_COUNT, grown);
            encounter_live_total_ = grown;
            mm2_gs_set_u8(gs.a4(), MM2_GS_ADDS_FRIENDS_LATCH, 1);
            added = 1;
        }
    }
    char name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], name, sizeof(name));
    if (added) {
        std::snprintf(status_line_, sizeof(status_line_), "%s adds friends!", name);
    } else {
        std::snprintf(status_line_, sizeof(status_line_), "%s waits for opening!", name);
    }
    enqueueCombatMessage(status_line_);
}

bool CombatSession::monsterSpecialGate105B6(GameStateView &gs, int slot)
{
    /* 0x105B6: rng(1,100); status bit6 blocks; tst -$503[slot]; -$11BB >= roll; subq charge. */
    const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
    if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS) {
        return false;
    }
    if ((slots_[slot].status & 0x40) != 0) {
        return false;
    }
    const uint8_t charges = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + slot);
    if (charges == 0) {
        return false;
    }
    const uint8_t chance = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_PABIL_CHANCE);
    if (chance < static_cast<uint8_t>(roll)) {
        return false;
    }
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SPECIAL_CHARGES + slot,
                  static_cast<uint8_t>(charges - 1));
    return true;
}

void CombatSession::deliverMonsterHit10478(GameStateView &gs, int mon_slot, int target_slot,
                                           const char *mon_name)
{
    uint16_t dmg = monsterMeleeDamage10478(gs, mon_slot, target_slot);
    if (party_blocking_[target_slot]) {
        dmg = static_cast<uint16_t>(dmg / 2); /* 0x11B0A / 0x49BA block half */
    }
    if (mm2_gs_u8(gs.a4(), MM2_GS_POWER_SHIELD_CTR) != 0) {
        dmg = static_cast<uint16_t>(dmg / 2);
    }
    if (mm2_gs_u8(gs.a4(), -0x5E32) == 0 && mm2_gs_u8(gs.a4(), MM2_GS_SHIELD_COUNTER) != 0) {
        dmg = static_cast<uint16_t>(dmg / 2);
    }
    applyPartyDamage4AAA(gs, target_slot, dmg, mon_name);
    if (dmg > 0) {
        const int ri = rosterIndexForPartySlot(target_slot);
        if (ri >= 0) {
            char vname[MM2_ROSTER_NAME_SIZE + 1];
            mm2_roster_name_to_cstr(&roster_->records[ri], vname, sizeof(vname));
            appendSingleEffectFeea(gs, vname);
        }
    }
}

int CombatSession::pickMonsterRangedTarget10332(GameStateView &gs)
{
    /* 0x10332: rng(1,party)−1; skip cond>=$80; wrap; store -$4F8. */
    const int pc = launch_ ? party_count_ : 0;
    if (pc <= 0) {
        return -1;
    }
    mm2_gs_set_u16(gs.a4(), MM2_GS_PARTY_COUNT, static_cast<uint16_t>(pc));
    int cur = rng_ ? rng_->range(1, pc) - 1 : 0;
    for (int tries = 0; tries < pc + 2; ++tries) {
        if (cur < 0 || cur >= pc) {
            cur = 0;
        }
        const int ri = rosterIndexForPartySlot(cur);
        if (ri >= 0 && roster_->records[ri].condition < 0x80) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, static_cast<uint8_t>(cur));
            return cur;
        }
        ++cur;
        if (cur >= pc) {
            cur = 0;
        }
    }
    return -1;
}

void CombatSession::monsterRangedAttack10584(GameStateView &gs, int slot)
{
    /* 0x10584: move.b #1,-$5E32; jsr 0x10332; clr -$53B; jsr 0x10478. */
    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], mon_name, sizeof(mon_name));
    mm2_gs_set_u8(gs.a4(), -0x5E32, 1);
    mm2_gs_set_u8(gs.a4(), -0x53B, 0);
    const int target_slot = pickMonsterRangedTarget10332(gs);
    if (target_slot < 0) {
        return;
    }
    deliverMonsterHit10478(gs, slot, target_slot, mon_name);
}

void CombatSession::rollPabilDamage1F6DC(GameStateView &gs, int mon_slot, uint16_t sides)
{
    /* 0x1F6DC: rolls = type>>4; sum rng(1,sides); -$F0C = sum*2+1. */
    const uint8_t mon_type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + mon_slot);
    const int rolls = static_cast<int>(mon_type >> 4);
    uint16_t sum = 0;
    const int hi = sides > 0 ? static_cast<int>(sides) : 1;
    for (int i = 0; i < rolls; ++i) {
        sum = static_cast<uint16_t>(sum + (rng_ ? rng_->range(1, hi) : 1));
    }
    mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, static_cast<uint16_t>(sum * 2u + 1u));
}

static uint16_t shieldHalvePartyDmg(uint8_t *a4, uint16_t dmg)
{
    if (mm2_gs_u8(a4, MM2_GS_POWER_SHIELD_CTR) != 0) {
        dmg = static_cast<uint16_t>(dmg / 2);
    }
    if (mm2_gs_u8(a4, -0x5E32) == 0 && mm2_gs_u8(a4, MM2_GS_SHIELD_COUNTER) != 0) {
        dmg = static_cast<uint16_t>(dmg / 2);
    }
    return dmg;
}

void CombatSession::applyPabilDamageSingle1F632(GameStateView &gs, const char *mon_name)
{
    /* 0x1F632: save -$4F8; pick living; 0x1F55E; restore. */
    const uint8_t saved = mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT);
    if (pickMonsterRangedTarget10332(gs) < 0) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, saved);
        return;
    }
    applyPabilDamage1F55E(gs, mon_name);
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, saved);
}

int CombatSession::luckBonus4442(GameStateView &gs, uint8_t luck) const
{
    /* 0x4442: result=$FD; while luck > -$7486[i] addq. */
    int result = 0xFD;
    for (int i = 0; i < 23; ++i) {
        const uint8_t thr = mm2_gs_u8(gs.a4(), MM2_GS_LUCK_THRESH_TBL + i);
        if (luck <= thr) {
            break;
        }
        ++result;
    }
    return result;
}

int CombatSession::partyResistCheck7F0E(GameStateView &gs, int party_slot)
{
    /* 0x48BA / -$7F0E: rng(1,100); ≤5→0; ≥$5F→1; else luck+level vs roll. */
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        return 0;
    }
    const Mm2RosterRecord &rec = roster_->records[ri];
    const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
    if (roll <= 5) {
        return 0;
    }
    if (roll >= 0x5F) {
        return 1;
    }
    const uint8_t level = rec.level > 0 ? rec.level : 1;
    int thr = luckBonus4442(gs, rec.luck_current) + static_cast<int>(level);
    if (thr < static_cast<int>(level)) {
        thr = 2;
    }
    return thr >= roll ? 1 : 0;
}

void CombatSession::partyResistFlags7D94(GameStateView &gs, int party_slot)
{
    /* 0x4952 resist prologue (flags at -$5630/2F/2E); may halve -$F0C. */
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        return;
    }
    const Mm2RosterRecord &rec = roster_->records[ri];
    if (rec.condition >= 0x80) {
        return;
    }

    uint8_t flag_b = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_B);
    if (flag_b != 0) {
        const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
        const int thr = static_cast<int>(rec.thievery_percent) +
                        static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_RESIST_BUFF_A));
        if (thr >= roll) {
            return; /* full immune path keeps flag_b */
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_B, 0);
    }

    uint8_t flag_a = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_A);
    if (flag_a != 0) {
        if (partyResistCheck7F0E(gs, party_slot) != 0) {
            uint16_t dmg = mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY);
            mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, static_cast<uint16_t>(dmg / 2));
        } else {
            mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_A, 0);
        }
    }

    uint8_t flag_c = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_C);
    if (flag_c == 3 || flag_c == 0xFF) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_C, 0);
    } else if (flag_c != 0) {
        int thr = static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_RESIST_BUFF_C));
        if (flag_c == 1) {
            thr += rec.secondary_skills[0]; /* +$17 */
        } else if (flag_c == 2) {
            thr += rec.unknown_1a_20[2]; /* +$1C */
        } else {
            /* 0x4A44: *(party + flag_c + $11) */
            const auto *raw = reinterpret_cast<const uint8_t *>(&rec);
            const int off = static_cast<int>(flag_c) + 0x11;
            if (off >= 0 && off < static_cast<int>(sizeof(Mm2RosterRecord))) {
                thr += raw[off];
            }
        }
        const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;
        if (thr >= roll) {
            uint16_t dmg = mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY);
            mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, static_cast<uint16_t>(dmg / 4));
        } else {
            mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_C, 0);
        }
    }
}

void CombatSession::applyPabilDamage1F55E(GameStateView &gs, const char *mon_name)
{
    /* 0x1F55E: -$7D94 resist; skip if -$562F; else apply (msg if A/C). */
    const int target = static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT));
    const uint16_t saved = mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY);
    partyResistFlags7D94(gs, target);
    if (mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_B) != 0) {
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, saved);
        return; /* "is not affected" */
    }
    uint16_t dmg = mm2_gs_u16(gs.a4(), MM2_GS_HP_APPLY);
    dmg = shieldHalvePartyDmg(gs.a4(), dmg);
    applyPartyDamage4AAA(gs, target, dmg, mon_name);
    mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, saved);
}

void CombatSession::applyPabilDamageMulti1F414(GameStateView &gs, const char *mon_name)
{
    /* 0x1F414: hit_count = rng(1,4)+3 − dead; walk -$65FE order; 0x1F55E each. */
    const int pc = launch_ ? party_count_ : 0;
    if (pc <= 0) {
        return;
    }
    int dead = 0;
    for (int i = 0; i < pc; ++i) {
        const int ri = rosterIndexForPartySlot(i);
        if (ri >= 0 && roster_->records[ri].condition >= 0x80) {
            ++dead;
        }
    }
    int hits = (rng_ ? rng_->range(1, 4) : 1) + 3 - dead;
    if (hits > pc) {
        hits = pc;
    }
    if (hits < 1) {
        hits = 1;
    }
    const int roll = rng_ ? rng_->range(1, 0x64) : 1;
    int base = 0;
    if (roll >= 0x28) {
        ++base;
    }
    if (roll >= 0x3C) {
        ++base;
    }
    if (roll >= 0x50) {
        ++base;
    }
    base *= 8;
    if (base > 0x18) {
        base = 0;
    }
    const uint8_t saved_ra = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_A);
    const uint8_t saved_rb = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_B);
    const uint8_t saved_rc = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_C);
    for (int n = 0; n < hits; ++n) {
        const uint8_t t = mm2_gs_u8(gs.a4(), MM2_GS_PABIL_TARGET_ORD + base + n);
        if (t >= static_cast<uint8_t>(pc)) {
            continue;
        }
        const int ri = rosterIndexForPartySlot(static_cast<int>(t));
        if (ri < 0 || roster_->records[ri].condition >= 0x80) {
            continue;
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, t);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_A, saved_ra);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_B, saved_rb);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_C, saved_rc);
        applyPabilDamage1F55E(gs, mon_name);
    }
}

bool CombatSession::partyConditionResistPass1F64E(GameStateView &gs, int party_slot)
{
    /* 0x1F64E: return true if effect should apply (all resist flags cleared). */
    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        return false;
    }
    const Mm2RosterRecord &rec = roster_->records[ri];
    const int roll = rng_ ? rng_->range(1, 0x64) : 0x64;

    if (mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_B) != 0) {
        const int thr = static_cast<int>(rec.thievery_percent) +
                        static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_RESIST_BUFF_A));
        if (thr >= roll) {
            return false; /* resisted — keep flag */
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_B, 0);
    }
    if (mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_A) != 0) {
        const int r = partyResistCheck7F0E(gs, party_slot);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_A, static_cast<uint8_t>(r));
        if (r != 0) {
            return false;
        }
    }
    const uint8_t rc = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_C);
    if (rc != 0) {
        int thr = static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_RESIST_BUFF_C));
        if (rc == 0x10) {
            thr += rec.might_current;
        } else if (rc == 0x15) {
            thr += rec.luck_current;
        } else if (rc == 0x16) {
            thr += rec.thievery_percent;
        } else if (rc == 0x24) {
            thr += rec.armor_class;
        } else {
            thr += static_cast<int>(rc);
        }
        if (thr >= roll) {
            return false;
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_C, 0);
    }
    return true;
}

void CombatSession::applyPabilCondition1F72E(GameStateView &gs, uint8_t mask)
{
    /* 0x1F72E: for each party slot, 0x1F796 OR mask unless resisted/dead. */
    const int pc = launch_ ? party_count_ : 0;
    const uint8_t saved_ra = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_A);
    const uint8_t saved_rb = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_B);
    const uint8_t saved_rc = mm2_gs_u8(gs.a4(), MM2_GS_RESIST_FLAG_C);
    for (int i = 0; i < pc; ++i) {
        const int ri = rosterIndexForPartySlot(i);
        if (ri < 0) {
            continue;
        }
        Mm2RosterRecord &rec = roster_->records[ri];
        if (rec.condition >= 0x80) {
            continue;
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, static_cast<uint8_t>(i));
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_A, saved_ra);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_B, saved_rb);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_C, saved_rc);
        if (partyConditionResistPass1F64E(gs, i)) {
            rec.condition = static_cast<uint8_t>(rec.condition | mask);
        }
    }
}

void CombatSession::applyPabilConditionRandom1F896(GameStateView &gs, uint8_t mask)
{
    /* 0x1F896: random living (cond<$80) then 0x1F796. */
    const int target = pickMonsterRangedTarget10332(gs);
    if (target < 0) {
        return;
    }
    const int ri = rosterIndexForPartySlot(target);
    if (ri < 0) {
        return;
    }
    if (partyConditionResistPass1F64E(gs, target)) {
        roster_->records[ri].condition =
            static_cast<uint8_t>(roster_->records[ri].condition | mask);
    }
}

void CombatSession::monsterGroupAttack10002(GameStateView &gs, int slot)
{
    /* 0x10006: print verb from Pabil; clr -$F0C; jsr -$7CDA → 0x1FB4E. */
    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], mon_name, sizeof(mon_name));
    const uint8_t pabil = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_PABIL);
    const char *verb = mm2_party_verb_name(pabil);
    std::snprintf(status_line_, sizeof(status_line_), "%s %s!", mon_name, verb);
    mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, 0);
    applyPabilEffect1FB4E(gs, slot, pabil);
}

void CombatSession::applyPabilEffect1FB4E(GameStateView &gs, int slot, uint8_t pabil)
{
    /* 0x1FB4E: load resist params; damage-flag or jump-table leaves. */
    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], mon_name, sizeof(mon_name));
    const uint8_t idx = static_cast<uint8_t>(pabil & 0x1F);
    mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_A, mm2_gs_u8(gs.a4(), MM2_GS_PABIL_RESIST_A + idx));
    mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_B, mm2_gs_u8(gs.a4(), MM2_GS_PABIL_RESIST_B + idx));
    {
        int c = static_cast<int>(mm2_gs_u8(gs.a4(), MM2_GS_PABIL_RESIST_C + idx)) - 0x11;
        mm2_gs_set_u8(gs.a4(), MM2_GS_RESIST_FLAG_C, static_cast<uint8_t>(c & 0xFF));
    }

    bool dmg_flag = false;
    if (pabil < 2) {
        dmg_flag = true;
    } else if (pabil >= 3 && pabil <= 8) {
        dmg_flag = true;
    } else if (pabil == 0x18 || pabil == 0x1F) {
        dmg_flag = true;
    }
    if (dmg_flag) {
        uint16_t dmg = static_cast<uint16_t>(slots_[slot].hp > 0 ? slots_[slot].hp : 1);
        if (slots_[slot].type < 0xB3) {
            dmg = static_cast<uint16_t>((dmg / 2) > 0 ? (dmg / 2) : 1);
        }
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, dmg);
        applyPabilDamageMulti1F414(gs, mon_name);
        return;
    }

    switch (pabil) {
    case 2: { /* 0x1FC1E curse */
        uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_BUFF_79A5);
        if (v < 0xFF) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_BUFF_79A5, static_cast<uint8_t>(v + 1));
        }
        break;
    }
    case 9: { /* 0x1F912 explodes */
        uint16_t dmg = static_cast<uint16_t>((slots_[slot].hp / 2) + 1);
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, dmg);
        applyPabilDamageMulti1F414(gs, mon_name);
        mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 1);
        {
            const uint32_t xp = mm2_monster_decode_xp(&monsters_->records[slots_[slot].type]);
            onMonsterKilled(gs, slot, xp);
        }
        break;
    }
    case 10: /* gazes */
        applyPabilCondition1F72E(gs, 0x82);
        break;
    case 11: { /* drains magic — clr.w +$58 (sp_max) */
        const int pc = launch_ ? party_count_ : 0;
        for (int i = 0; i < pc; ++i) {
            const int ri = rosterIndexForPartySlot(i);
            if (ri >= 0) {
                roster_->records[ri].sp_max = 0;
            }
        }
        break;
    }
    case 12: { /* drains spell level — subq +$72 */
        const int pc = launch_ ? party_count_ : 0;
        for (int i = 0; i < pc; ++i) {
            const int ri = rosterIndexForPartySlot(i);
            if (ri >= 0 && roster_->records[ri].spell_level > 0) {
                roster_->records[ri].spell_level =
                    static_cast<uint8_t>(roster_->records[ri].spell_level - 1);
            }
        }
        break;
    }
    case 13: { /* vaporizes valuables */
        const int roll = rng_ ? rng_->range(1, 0x64) : 1;
        const int pc = launch_ ? party_count_ : 0;
        for (int i = 0; i < pc; ++i) {
            const int ri = rosterIndexForPartySlot(i);
            if (ri < 0 || roll <= 0x41) {
                continue;
            }
            const uint16_t ridx = mm2_gs_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + i * 2);
            if (ridx < 0x18) {
                roster_->records[ri].gold = 0;
            } else {
                roster_->records[ri].gems = 0;
            }
        }
        break;
    }
    case 14: { /* juggles party */
        rollPabilDamage1F6DC(gs, slot, 3);
        applyPabilDamageMulti1F414(gs, mon_name);
        const int pc = launch_ ? party_count_ : 0;
        if (pc >= 3 && launch_) {
            const int swaps = rng_ ? rng_->range(1, 8) : 1;
            auto *launch = const_cast<Mm2PartyLaunch *>(launch_);
            for (int s = 0; s < swaps; ++s) {
                const int a = (rng_ ? rng_->range(1, pc) : 1) - 1;
                const int b = (rng_ ? rng_->range(1, pc) : 1) - 1;
                if (a == b || a < 0 || b < 0 || a >= pc || b >= pc) {
                    continue;
                }
                const int tmp = launch->roster_slots[a];
                launch->roster_slots[a] = launch->roster_slots[b];
                launch->roster_slots[b] = tmp;
                const uint16_t wa = mm2_gs_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + a * 2);
                const uint16_t wb = mm2_gs_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + b * 2);
                mm2_gs_set_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + a * 2, wb);
                mm2_gs_set_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL + b * 2, wa);
            }
        }
        break;
    }
    case 15: /* blast */
        rollPabilDamage1F6DC(gs, slot, 6);
        applyPabilDamageSingle1F632(gs, mon_name);
        break;
    case 16: /* sleep */
        applyPabilCondition1F72E(gs, 0x10);
        break;
    case 17:
    case 18:
        rollPabilDamage1F6DC(gs, slot, 6);
        applyPabilDamageMulti1F414(gs, mon_name);
        break;
    case 19: /* death */
        applyPabilConditionRandom1F896(gs, 0x81);
        break;
    case 20: /* disintegrate */
        applyPabilConditionRandom1F896(gs, 0xFF);
        break;
    case 21:
        rollPabilDamage1F6DC(gs, slot, 0x28);
        applyPabilDamageSingle1F632(gs, mon_name);
        break;
    case 22:
        rollPabilDamage1F6DC(gs, slot, 0x0C);
        applyPabilDamageMulti1F414(gs, mon_name);
        break;
    case 23:
        rollPabilDamage1F6DC(gs, slot, 0x32);
        applyPabilDamageSingle1F632(gs, mon_name);
        break;
    case 24: /* jump-table FAIL */
        break;
    case 25: /* implosion */
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, 0x3E8);
        applyPabilDamageSingle1F632(gs, mon_name);
        break;
    case 26:
        rollPabilDamage1F6DC(gs, slot, 0x14);
        applyPabilDamageMulti1F414(gs, mon_name);
        break;
    case 27: {
        const int r = rng_ ? rng_->range(1, 0x0F) : 1;
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, static_cast<uint16_t>(r + 1));
        applyPabilDamageSingle1F632(gs, mon_name);
        break;
    }
    case 28: /* silence-ish */
        applyPabilCondition1F72E(gs, 0x02);
        break;
    case 29: { /* frenzies */
        const uint8_t swings = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SWINGS);
        const uint8_t sides = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_DMG_SIDES);
        const int hi = sides > 0 ? static_cast<int>(sides) : 1;
        uint16_t sum = 0;
        for (uint8_t s = 0; s < swings; ++s) {
            sum = static_cast<uint16_t>(sum + (rng_ ? rng_->range(1, hi) : 1));
        }
        mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, sum);
        applyPabilDamageMulti1F414(gs, mon_name);
        mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 1);
        {
            const uint32_t xp = mm2_monster_decode_xp(&monsters_->records[slots_[slot].type]);
            onMonsterKilled(gs, slot, xp);
        }
        break;
    }
    case 30:
        applyPabilCondition1F72E(gs, 0x20);
        break;
    default:
        break;
    }
}

void CombatSession::appendSingleEffectFeea(GameStateView &gs, const char *victim_name)
{
    /* 0xFEEA: (Sabil&$1F)-1 indexes victim strings; skip if Sabil==0. */
    const uint8_t sabil = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SABIL);
    if (sabil == 0 || sabil > MM2_SINGLE_EFFECT_COUNT) {
        return;
    }
    const char *eff = mm2_single_effect_name(static_cast<uint8_t>(sabil));
    if (!eff || eff[0] == '\0' || std::strcmp(eff, "nothing") == 0) {
        return;
    }
    char line[160];
    std::snprintf(line, sizeof(line), "%s %s!", victim_name, eff);
    enqueueCombatMessage(line);
}

int CombatSession::pickMonsterMeleeTarget103BA(GameStateView &gs)
{
    /* 0x103BA, ported instruction-for-instruction. It runs on the same three
     * game-state bytes as the ROM, with the ROM's unbounded loop (no cap, no
     * early-out):
     *   -$4F8  MM2_GS_COMBAT_TARGET_SLOT  target party slot (persists per call)
     *   -$5E4D MM2_GS_FRONT_RANK_N        hand-to-hand front-rank cutoff
     *   -$7959 low byte of the -$795A party-count word = active party size
     *
     * The stack-local counter t (-$1(a5)) drives the front-rank growth: only
     * once t has walked the whole current rank without a live target
     * (t >= -$5E4D) is -$5E4D widened by one, clamped to party size, so a
     * back-rank member steps into hand-to-hand — exactly what happens as front
     * members die/go unconscious. Dead/eradicated/stoned members (roster +$26
     * & $C0) are skipped. Termination matches the ROM: monster turns only run
     * while checkOutcome() has confirmed a member with condition <= $10 (hence
     * & $C0 == 0) still fights, so a live target always exists in the window. */
    uint8_t t = 0; /* 0x103BE clr.b -$1(a5) */
    for (;;) {
        if (t < mm2_gs_u8(gs.a4(), MM2_GS_FRONT_RANK_N)) { /* 0x103C6 t < -$5E4D */
            /* 0x103E2: addq.b #1,-$4F8. */
            mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT,
                          static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT) + 1));
        } else {
            /* 0x103CC..0x103DA: addq.b #1,-$5E4D; clamp to party size -$7959. */
            uint8_t front = static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_FRONT_RANK_N) + 1);
            const uint8_t size = mm2_gs_u8(gs.a4(), -0x7959);
            if (front > size) {
                front = size;
            }
            mm2_gs_set_u8(gs.a4(), MM2_GS_FRONT_RANK_N, front);
            front_rank_count_ = front; /* keep the C++ mirror equal to -$5E4D */
        }
        /* 0x103E6..0x103F0: if -$4F8 >= -$5E4D → 0. */
        if (mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT) >= mm2_gs_u8(gs.a4(), MM2_GS_FRONT_RANK_N)) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, 0);
        }
        /* 0x103F4..0x103FE: if -$4F8 >= party size -$7959 → 0. */
        if (mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT) >= mm2_gs_u8(gs.a4(), -0x7959)) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT, 0);
        }
        t = static_cast<uint8_t>(t + 1); /* 0x10402 addq.b #1,-$1(a5) */
        /* 0x10406..0x10422: roster record for -$4F8; test +$26 & $C0. */
        const uint8_t cur = mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_TARGET_SLOT);
        const int ri = rosterIndexForPartySlot(static_cast<int>(cur));
        const uint8_t cond = (ri >= 0) ? roster_->records[ri].condition : 0;
        if ((cond & 0xC0) == 0) {
            return static_cast<int>(cur); /* 0x10428 living hand-to-hand target */
        }
        /* 0x10426: & $C0 nonzero → loop back to 0x103C2. */
    }
}

uint16_t CombatSession::monsterMeleeDamage10478(GameStateView &gs, int mon_slot, int party_slot)
{
    /* 0x10478: hit_chance from type>>4 via -$6F16; ceiling vs AC+$24; swings. */
    mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, 0);
    mm2_gs_set_u8(gs.a4(), -0x5E31, 0);
    const uint8_t swings = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SWINGS);
    mm2_gs_set_u8(gs.a4(), -0x5E2D, swings);
    const uint8_t mon_type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + mon_slot);
    const uint8_t hit_idx = static_cast<uint8_t>(mon_type >> 4);
    uint8_t hit_chance = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_HIT_TBL + hit_idx);

    const int ri = rosterIndexForPartySlot(party_slot);
    if (ri < 0) {
        return 0;
    }
    const uint8_t ac = roster_->records[ri].armor_class;
    uint8_t ceiling = 5;
    if (ac <= hit_chance) {
        ceiling = static_cast<uint8_t>(hit_chance - ac);
    }
    if ((slots_[mon_slot].status & 0x08) != 0) {
        ceiling = static_cast<uint8_t>(ceiling / 2);
    }

    uint16_t total = 0;
    const uint8_t sides = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_DMG_SIDES);
    const int side_hi = sides > 0 ? static_cast<int>(sides) : 1;
    for (uint8_t s = 0; s < swings; ++s) {
        const int raw = rng_ ? rng_->range(0x0A, 0x3F1) : 0x0A;
        const int roll = raw / 10;
        if (ceiling > static_cast<uint8_t>(roll)) {
            mm2_gs_set_u8(gs.a4(), -0x5E31, static_cast<uint8_t>(mm2_gs_u8(gs.a4(), -0x5E31) + 1));
            const int hit_dmg = rng_ ? rng_->range(1, side_hi) : 1;
            total = static_cast<uint16_t>(total + hit_dmg);
        }
    }
    if ((slots_[mon_slot].status & 0x04) != 0) {
        total = static_cast<uint16_t>(total / 2);
    }
    mm2_gs_set_u16(gs.a4(), MM2_GS_HP_APPLY, total);
    return total;
}

void CombatSession::applyPartyDamage4AAA(GameStateView &gs, int party_slot, uint16_t dmg,
                                         const char *mon_name)
{
    const int idx = rosterIndexForPartySlot(party_slot);
    if (idx < 0) {
        return;
    }
    Mm2RosterRecord &victim = roster_->records[idx];
    char victim_name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&victim, victim_name, sizeof(victim_name));
    /* 0xFF7C: -$5E32==1 (set by 0x10584 ranged) → "shoots"; else melee verb.
     * Port uses "attacks" for the melee table pick at -$6EF6 until that table
     * is wired; latch still gates hand-to-hand vs out-of-range archers. */
    const char *verb = (mm2_gs_u8(gs.a4(), -0x5E32) == 1) ? "shoots" : "attacks";
    if (dmg == 0) {
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s and misses.", mon_name, verb,
                      victim_name);
        return;
    }
    /* 0x10478 → 0xFF42: play_sound_seq id 9 on a connecting hit. */
    audio::playSoundSeq(9, gs.soundsEnabled(), gs.walkBeepEnabled());
    /* 0x4AA0..0x4AF8: andi #$EF on +$26; subtract from +$5E (working HP /
     * codec hp_max), NOT +$74 (HP ceiling / codec hp_current). Same path as
     * eventVmApplyOp31Damage. */
    victim.condition = static_cast<uint8_t>(victim.condition & 0xEF);
    if ((victim.condition & 0x40) != 0) {
        victim.condition = 0x81;
        victim.hp_max = 0;
        audio::playSoundSeq(6, gs.soundsEnabled(), gs.walkBeepEnabled());
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s - %s falls!", mon_name, verb,
                      victim_name, victim_name);
    } else if (victim.hp_max > dmg) {
        victim.hp_max = static_cast<uint16_t>(victim.hp_max - dmg);
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s for %u damage.", mon_name, verb,
                      victim_name, static_cast<unsigned>(dmg));
    } else {
        victim.condition = static_cast<uint8_t>(victim.condition | 0x40);
        victim.hp_max = 0;
        /* 0xFD8C / 0xFE0C: play_sound_seq id 6 on party KO ("goes down" / "is killed"). */
        audio::playSoundSeq(6, gs.soundsEnabled(), gs.walkBeepEnabled());
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s - %s falls!", mon_name, verb,
                      victim_name, victim_name);
    }
}

void CombatSession::finishVictory(GameStateView &gs)
{
    /* 0x12430: victory latch, optional 0x123B0 item fill, then XP split. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 1); /* 0x12438 */
    /* 0x1243e: clr.b -$794C */
    mm2_gs_set_u8(gs.a4(), MM2_GS_FOUND_SENTINEL, 0);
    generateVictoryItems123B0(gs);

    int reward_count = 0;
    for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
        if (partySlotEligibleForRewards(i)) {
            ++reward_count;
        }
    }
    /* 0x10B74 accrues kill XP into the fight pool; 0x12430 divides -$6FC6 by
     * the count of members with roster+$26 < $80 and adds the share to each
     * (+$62). Unconscious (0 HP, condition still < $80) receive a full share. */
    if (reward_count > 0 && xp_pool_ > 0) {
        const uint32_t share = xp_pool_ / static_cast<uint32_t>(reward_count);
        for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
            if (!partySlotEligibleForRewards(i)) {
                continue;
            }
            roster_->records[rosterIndexForPartySlot(i)].experience += share;
        }
    }

    uint32_t gold = 0;
    if (arena_reward_.active) {
        /* asm 0x9F04-0x9F2C: arena ticket gold to the first eligible member. */
        gold = events::eventVmArenaGoldReward(arena_reward_.color, arena_reward_.screen);
        for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
            if (!partySlotEligibleForRewards(i)) {
                continue;
            }
            roster_->records[rosterIndexForPartySlot(i)].gold += gold;
            break;
        }
    }

    gs.setRightPanelMode(saved_panel_mode_); /* 0x131B8 */
    outcome_ = CombatOutcome::Victory;
    if (arena_reward_.active) {
        std::snprintf(status_line_, sizeof(status_line_),
                      "You are victorious! %lu XP earned. Winner, you receive %lu gold.",
                      static_cast<unsigned long>(xp_pool_), static_cast<unsigned long>(gold));
    } else {
        std::snprintf(status_line_, sizeof(status_line_), "You are victorious! %lu experience points earned.",
                      static_cast<unsigned long>(xp_pool_));
    }
    arena_reward_ = ArenaReward{};
    /* 0x125FC: play_sound_seq id 3 (victory sting). */
    audio::playSoundSeq(3, gs.soundsEnabled(), gs.walkBeepEnabled());
    round_layout_active_ = false;
    state_ = CombatState::Inactive;
}

void CombatSession::finishLeave(GameStateView &gs, bool fled)
{
    gs.setRightPanelMode(saved_panel_mode_); /* 0x131B8 */
    round_layout_active_ = false;
    outcome_ = fled ? CombatOutcome::Fled : CombatOutcome::Defeated;
    if (!fled && roster_ && launch_) {
        /* combat_defeat_retreat @ 0x11646 (party wipe path via 0x12C66):
         *   0x1164A: unpack -$560C (attrib entry_coord 0x0E via loader 0x923E)
         *     lo nibble → -$79F1 (X), hi nibble → -$79F0 (Y).
         *   jsr -$7E96 (funeral audio) — presentation.
         *   addq.w #1,-$796E — music transpose; skip (audio).
         *   move.w #1,-$4F4E (ENCOUNTER_REDRAW).
         *   if -$523==0: members with cond>=$10 → cond=$81 (0x1168C–0x116AA).
         * Flee leaves coords and conditions alone. */
        const uint8_t packed = mm2_gs_u8(gs.a4(), MM2_GS_ENTRY_COORD);
        gs.setCoordX(static_cast<uint8_t>(packed & 0x0F));
        gs.setCoordY(static_cast<uint8_t>((packed >> 4) & 0x0F));
        mm2_gs_set_u16(gs.a4(), MM2_GS_ENCOUNTER_REDRAW, 1);
        for (int i = 0; i < party_count_ && i < MM2_GS_PARTY_SIZE; ++i) {
            const int idx = rosterIndexForPartySlot(i);
            if (idx < 0) {
                continue;
            }
            Mm2RosterRecord &rec = roster_->records[idx];
            if (rec.condition >= 0x10) {
                rec.condition = 0x81;
            }
        }
    }
    setStatus(fled ? "The party flees!" : "The party has been defeated...");
    arena_reward_ = ArenaReward{};
    state_ = CombatState::Inactive;
}

namespace {

int monsterPictureDiskIndex(const Mm2MonstersFile *monsters, uint8_t type)
{
    if (!monsters || type >= MM2_MONSTER_RECORD_COUNT) {
        return -1;
    }
    const Mm2MonsterRecord &rec = monsters->records[type];
    const uint8_t pic = rec.fields[MM2_MON_OFF_PICTURE - MM2_MONSTER_NAME_SIZE];
    return static_cast<int>(pic & 0x7F);
}

}  // namespace

int CombatSession::leaderSpriteDiskIndex() const
{
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (!slots_[i].alive) {
            continue;
        }
        return monsterPictureDiskIndex(monsters_, slots_[i].type);
    }
    return -1;
}

gfx::CombatPanelView CombatSession::panelView() const
{
    gfx::CombatPanelView view{};
    if (!monsters_) {
        return view;
    }

    std::snprintf(view.message, sizeof(view.message), "%s", status_line_);
    view.show_party_options = state_ == CombatState::AwaitingPartyOptions;
    view.show_bribe_kind = state_ == CombatState::AwaitingBribeKind;
    view.show_bribe_amount = state_ == CombatState::AwaitingBribeAmount;
    view.show_command_options = state_ == CombatState::AwaitingCommand;
    view.show_cast_level = state_ == CombatState::AwaitingCastLevel;
    view.show_cast_number = state_ == CombatState::AwaitingCastNumber;
    view.show_cast_target = state_ == CombatState::AwaitingCastTarget ||
                            state_ == CombatState::AwaitingAttackTarget;
    view.show_party_pick = state_ == CombatState::AwaitingPartyPick;
    view.show_item_pick = state_ == CombatState::AwaitingItemPick;
    view.cast_level = cast_level_;
    /* Sticky after 0x12A22 — not derived from ephemeral picker states. */
    view.label_monster_slots = round_layout_active_;
    if (view.show_command_options && active_party_slot_ >= 0) {
        const int idx = rosterIndexForPartySlot(active_party_slot_);
        if (idx >= 0) {
            mm2_roster_name_to_cstr(&roster_->records[idx], view.options_for, sizeof(view.options_for));
        }
        commandFlagsForActiveSlot(view.opt_melee, view.opt_shoot, view.opt_cast);
    }

    int alive_total = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (slots_[i].alive) {
            ++alive_total;
        }
    }

    if (view.label_monster_slots) {
        /* 0x129CC: fixed rows slot+3 for battle slots 0..9; dead → clear. */
        view.monster_line_count = 10;
        for (int i = 0; i < 10; ++i) {
            gfx::CombatMonsterLine &line = view.monster_lines[i];
            line.letter = static_cast<char>('A' + i);
            if (!slots_[i].alive) {
                continue;
            }
            line.occupied = true;
            /* 0x12742: check when battle slot index < A4-$524 melee-range count. */
            line.show_checkmark = i < melee_range_count_;
            line.highlighted = (active_monster_slot_ >= 0 && i == active_monster_slot_);
            if (slots_[i].hp > 0 && slots_[i].hp < slots_[i].max_hp) {
                std::snprintf(line.status_suffix, sizeof(line.status_suffix), "Hurt");
            }
            mm2_monster_name_to_cstr(&monsters_->records[slots_[i].type], line.name, sizeof(line.name));
        }
    } else {
        /* Pre-combat name box @ 0x12D80: compact live names (no A–J). */
        int listed = 0;
        for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS && listed < 10; ++i) {
            if (!slots_[i].alive) {
                continue;
            }
            gfx::CombatMonsterLine &line = view.monster_lines[listed];
            line.occupied = true;
            mm2_monster_name_to_cstr(&monsters_->records[slots_[i].type], line.name, sizeof(line.name));
            ++listed;
        }
        view.monster_line_count = listed;
    }
    view.highlight_slot = active_monster_slot_;

    const int live_total = encounter_live_total_ > alive_total ? encounter_live_total_ : alive_total;
    if (live_total > 10) {
        view.overflow_more = live_total - 10;
        const uint8_t type = overflow_type_ != 0 ? overflow_type_ : slots_[0].type;
        mm2_monster_name_to_cstr(&monsters_->records[type], view.overflow_name, sizeof(view.overflow_name));
    }

    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (!slots_[i].alive) {
            continue;
        }
        const int disk = monsterPictureDiskIndex(monsters_, slots_[i].type);
        if (disk < 0) {
            continue;
        }
        if (view.sprite_disk_index < 0) {
            view.sprite_disk_index = disk;
        }
        int found = -1;
        for (int s = 0; s < view.sprite_slot_count; ++s) {
            if (view.sprite_slots[s].disk_index == disk) {
                found = s;
                break;
            }
        }
        if (found >= 0) {
            ++view.sprite_slots[found].stack_count;
            continue;
        }
        if (view.sprite_slot_count >= gfx::kAgaCombatSpriteCap) {
            continue;
        }
        gfx::CombatSpriteSlot &slot = view.sprite_slots[view.sprite_slot_count++];
        slot.disk_index = disk;
        slot.stack_count = 1;
        slot.battle_slot = i;
    }

    return view;
}

}  // namespace mm2::combat
