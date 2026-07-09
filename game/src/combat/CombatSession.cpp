#include "mm2/combat/CombatSession.h"

#include "mm2/CppStdCompat.h"
#include "mm2/combat/EncounterPicker.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/gameplay/SpellBook.h"
#include "mm2_create_character.h"

namespace mm2::combat {

namespace {

/* doc 16: byte 0x14 "speed" = {low nibble+1, high nibble+1} (two paired
 * stats). Doc 17's round-loop trace (-$50E initiative) is not fully reduced
 * to a single confirmed source byte, so this MVP uses the low-nibble value
 * (the byte doc 16 names "speed") as the initiative stat. */
int decodeMonsterSpeed(const Mm2MonsterRecord &rec)
{
    const uint8_t b = rec.fields[MM2_MON_OFF_SPEED - MM2_MONSTER_NAME_SIZE];
    return (b & 0x0F) + 1;
}

/* doc 16: byte 0x17 damage = low5+1, x10 if bit5, capped 250. Used as the
 * ceiling of a simplified rng(1,max) melee roll (0xCD90/0x10118 exact
 * formula not traced — doc 17 open item). */
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
    if (!launch_ || party_slot < 0 || party_slot >= launch_->party_count ||
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
    return roster_->records[idx].hp_current > 0;
}

bool CombatSession::partySlotInFight(int party_slot) const
{
    const int idx = rosterIndexForPartySlot(party_slot);
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT || !roster_) {
        return false;
    }
    /* combat_round_end_check @ 0x13282: cmpi.b #$10, $26(a0); bhi skips. */
    return roster_->records[idx].condition <= 0x10;
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
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_STATUS + i, slots_[i].alive ? 1 : 0);
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
     * random picker; 0x1213E runs only when mode lacks 0x80 and leaves -$77BE
     * as-is (no second recompute). */
    const uint8_t mode = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE);
    encounterRecomputeLiveCount(gs);
    if (mode != 0x80) {
        encounterInitXpBudget(gs, *roster_, *launch_);
        encounterRunRandomPicker(gs, world.attrib(), *rng_, &friendCountFromMonsters, monsters_);
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
    }
    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
        const int idx = rosterIndexForPartySlot(i);
        if (idx >= 0) {
            mm2_roster_refresh_spell_points(&roster_->records[idx]);
        }
    }
    for (int i = 0; i < MM2_GS_PARTY_SIZE; ++i) {
        party_acted_[i] = false;
        party_blocking_[i] = false;
    }

    xp_pool_ = 0;
    outcome_ = CombatOutcome::None;
    active_party_slot_ = -1;
    active_monster_slot_ = -1;
    melee_range_count_ = 0; /* 0x12C90 clears -$524 */
    front_rank_count_ = 0;  /* 0x12C8C clears -$5E4D */
    surprise_mode_ = 0;
    syncMonsterSlotsToGs(gs);

    beginEncounterUi(gs, world);
    return true;
}

void CombatSession::applySurpriseRoll(GameStateView &gs)
{
    /* Encounter-setup surprise @ 0x12EE2: rng(1,100) vs front-rank speed and
     * levitate; mode 2 = party surprised, 3 = monsters surprised. */
    const int roll = rng_->range(1, 100);
    int front_speed = 10;
    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
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
    const int party_count = launch_ ? launch_->party_count : 0;

    /* A4-$524: monsters within melee reach. Indoor rng(10,39)/10+3 = 4..6;
     * outdoor party/2 + rng(10,69)/10 (0x11D1C / 0x11D38). */
    int melee;
    if (view_mode == 0) {
        melee = rng_->range(10, 39) / 10 + 3;
    } else {
        melee = party_count / 2 + rng_->range(10, 69) / 10;
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
    /* not silenced (+$26 bit1), caster (+$72), current SP word +$58 (0x118F0..0x1190C). */
    cast = (rec.condition & 0x02) == 0 && rec.spell_level != 0 && rec.sp_current > 0;
}

void CombatSession::startRoundLoop(GameStateView &gs, const world::MapWorld &world)
{
    recomputeRangeCounts(gs);
    runUntilDecisionOrEnd(gs, world);
}

void CombatSession::resolvePartyAttack(GameStateView &gs, const world::MapWorld &world)
{
    startRoundLoop(gs, world);
}

void CombatSession::resolvePartyRun(GameStateView &gs, const world::MapWorld &world)
{
    const uint8_t difficulty = world.attrib().raw[0x0D];
    const int success_chance = 100 - (difficulty > 90 ? 90 : difficulty);
    const int roll = rng_->range(1, 100);
    if (roll <= success_chance) {
        finishLeave(gs, true);
        return;
    }
    setStatus("The party fails to run!");
    state_ = CombatState::AwaitingPartyOptions;
    (void)world;
}

void CombatSession::resolvePartyHide(GameStateView &gs, const world::MapWorld &world)
{
    int best_hide = 0;
    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
        if (!partySlotAlive(i)) {
            continue;
        }
        const int idx = rosterIndexForPartySlot(i);
        const int skill = roster_->records[idx].thievery_percent;
        if (skill > best_hide) {
            best_hide = skill;
        }
    }
    /* GAP: exact hide formula not traced; thievery% gates the flee roll. */
    const int roll = rng_->range(1, 100);
    if (roll <= best_hide) {
        finishLeave(gs, true);
        setStatus("The party hides!");
        return;
    }
    setStatus("You failed to hide!");
    state_ = CombatState::AwaitingPartyOptions;
}

void CombatSession::resolvePartyBribe(GameStateView &gs)
{
    uint32_t gold = 0;
    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
        if (!partySlotAlive(i)) {
            continue;
        }
        gold += roster_->records[rosterIndexForPartySlot(i)].gold;
    }
    constexpr uint32_t kBribeCost = 100;
    if (gold < kBribeCost) {
        setStatus("You don't have enough gold!");
        state_ = CombatState::AwaitingPartyOptions;
        return;
    }
    /* GAP: bribe submenu (gold/gems/food @ 0x12FB8) not ported; flat gold try. */
    const int roll = rng_->range(1, 100);
    if (roll <= 50) {
        for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
            if (!partySlotAlive(i)) {
                continue;
            }
            Mm2RosterRecord &rec = roster_->records[rosterIndexForPartySlot(i)];
            if (rec.gold >= kBribeCost) {
                rec.gold -= kBribeCost;
                break;
            }
        }
        gs.setRightPanelMode(saved_panel_mode_);
        outcome_ = CombatOutcome::None;
        state_ = CombatState::Inactive;
        setStatus("The monsters accept your bribe.");
        arena_reward_ = ArenaReward{};
        return;
    }
    setStatus("Your bribe is refused!");
    state_ = CombatState::AwaitingPartyOptions;
}

bool CombatSession::tick(GameStateView &gs, const world::MapWorld &world, const platform::KeyState &keys)
{
    if (state_ == CombatState::AwaitingActionAck) {
        if (keys.last_ascii == 0) {
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
            resolvePartyBribe(gs);
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

    if (state_ == CombatState::AwaitingCastLevel || state_ == CombatState::AwaitingCastNumber) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (keys.escape || ch == 0x1B) {
            cast_level_ = 0;
            state_ = CombatState::AwaitingCommand;
            setStatus("");
            return false;
        }
        if (!tickCastPicker(ch)) {
            return false;
        }
        /* Cast resolved (or cancelled back to command) — if still casting, wait. */
        if (state_ == CombatState::AwaitingCastLevel || state_ == CombatState::AwaitingCastNumber) {
            return false;
        }
        if (state_ == CombatState::AwaitingActionAck) {
            party_acted_[active_party_slot_] = true;
            active_party_slot_ = -1;
        }
        return false;
    }

    if (state_ != CombatState::AwaitingCommand) {
        return false;
    }

    /* combat_read_command_key @ 0x1175C: B/R always; A/F gated on melee,
     * S on shoot; C gated on cast. V is handled by GameSession (sheet view). */
    bool can_melee = false, can_shoot = false, can_cast = false;
    commandFlagsForActiveSlot(can_melee, can_shoot, can_cast);

    const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
    switch (ch) {
    case 'A':
    case 'F':
        if (!can_melee) {
            return false;
        }
        resolvePlayerAttack(gs, false);
        break;
    case 'S':
        if (!can_shoot) {
            return false;
        }
        resolvePlayerAttack(gs, true);
        break;
    case 'C':
        if (!can_cast) {
            return false;
        }
        beginCastPicker();
        return false;
    case 'B':
        resolvePlayerBlock();
        break;
    case 'R':
        resolvePlayerRun(gs, world);
        break;
    default:
        return false;
    }

    if (state_ == CombatState::Inactive) {
        return true; /* Run succeeded -> combat ended this tick. */
    }

    party_acted_[active_party_slot_] = true;
    active_party_slot_ = -1;
    state_ = CombatState::AwaitingActionAck;
    return false;
}

void CombatSession::beginRound(GameStateView &gs)
{
    active_monster_slot_ = -1;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        monster_acted_[i] = false;
    }
    for (int i = 0; i < MM2_GS_PARTY_SIZE; ++i) {
        party_acted_[i] = false;
        party_blocking_[i] = false;
    }
    syncMonsterSlotsToGs(gs);
}

bool CombatSession::checkOutcome(GameStateView &gs, const world::MapWorld & /*world*/)
{
    if (firstAliveMonster() < 0) {
        finishVictory(gs);
        return true;
    }

    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
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
        for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
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
            beginRound(gs);
            continue;
        }

        if (best_party >= 0 && (best_monster < 0 || best_party_speed > best_monster_speed)) {
            active_monster_slot_ = -1;
            active_party_slot_ = best_party;
            state_ = CombatState::AwaitingCommand;
            setStatus(""); /* the command grid (0x11866) is the whole prompt */
            return;
        }

        resolveMonsterTurn(gs, best_monster);
        monster_acted_[best_monster] = true;
        if (checkOutcome(gs, world)) {
            return;
        }
        state_ = CombatState::AwaitingActionAck;
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

bool CombatSession::tickCastPicker(char key)
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
    resolvePlayerCast(flat);
    cast_level_ = 0;
    state_ = CombatState::AwaitingActionAck;
    return true;
}

void CombatSession::resolvePlayerCast(int flat0)
{
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    const Mm2RosterRecord &caster = roster_->records[idx];
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&caster, name, sizeof(name));

    const gameplay::SpellSchool school = gameplay::spellSchoolForClass(caster.class_id);
    const gameplay::SpellMeta *table = gameplay::schoolSpellTable(school);
    const char *spell_name = (table && flat0 >= 0 && flat0 < gameplay::kSpellsPerSchool) ? table[flat0].name : "?";

    /* Effect stubs ($CDB8/$CFF8) + SP deduct ($6DEE) are still a GAP — acknowledge. */
    std::snprintf(status_line_, sizeof(status_line_), "%s casts %s (stub).", name, spell_name);
}

void CombatSession::resolvePlayerAttack(GameStateView &gs, bool shooting)
{
    const int target = firstAliveMonster();
    if (target < 0) {
        return;
    }
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    const Mm2RosterRecord &attacker = roster_->records[idx];
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&attacker, name, sizeof(name));

    /* Simplified roll (0xCD90 melee / 0x11610 shoot exact formulas not traced —
     * doc 17 open item): rng(1, might_current) for both paths. */
    const int might = attacker.might_current > 0 ? attacker.might_current : 1;
    const int dmg = rng_->range(1, might);
    const char *verb = shooting ? "shoots" : "attacks";

    CombatMonster &m = slots_[target];
    m.hp -= dmg;
    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[m.type], mon_name, sizeof(mon_name));

    if (m.hp <= 0) {
        m.hp = 0;
        m.alive = false;
        xp_pool_ += mm2_monster_decode_xp(&monsters_->records[m.type]); /* 0x10B74 XP accrual */
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s for %d - %s is destroyed!", name, verb,
                      mon_name, dmg, mon_name);
    } else {
        std::snprintf(status_line_, sizeof(status_line_), "%s %s %s for %d damage.", name, verb, mon_name, dmg);
    }
    syncMonsterSlotsToGs(gs);
}

void CombatSession::resolvePlayerBlock()
{
    party_blocking_[active_party_slot_] = true; /* 0x11B0A: halves incoming melee damage this round. */
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&roster_->records[idx], name, sizeof(name));
    std::snprintf(status_line_, sizeof(status_line_), "%s blocks.", name);
}

void CombatSession::resolvePlayerRun(GameStateView &gs, const world::MapWorld &world)
{
    /* Simplified whole-party retreat (0x116CA/0x13148 exact difficulty
     * formula not traced — doc 17/35 open item): harder attrib byte 0x0D
     * lowers the flee chance, floored at 10%. */
    const uint8_t difficulty = world.attrib().raw[0x0D];
    const int success_chance = 100 - (difficulty > 90 ? 90 : difficulty);
    const int roll = rng_->range(1, 100);
    if (roll <= success_chance) {
        finishLeave(gs, true);
        return;
    }
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&roster_->records[idx], name, sizeof(name));
    std::snprintf(status_line_, sizeof(status_line_), "%s fails to flee!", name);
}

void CombatSession::resolveMonsterTurn(GameStateView &gs, int slot)
{
    active_monster_slot_ = slot;
    active_party_slot_ = -1;
    CombatMonster &m = slots_[slot];
    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[m.type], mon_name, sizeof(mon_name));

    int candidates[MM2_GS_PARTY_SIZE];
    int n = 0;
    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
        if (partySlotAlive(i)) {
            candidates[n++] = i;
        }
    }
    if (n == 0) {
        return; /* checkOutcome() ends combat on the next loop iteration. */
    }
    const int target_slot = candidates[rng_->range(0, n - 1)];
    const int idx = rosterIndexForPartySlot(target_slot);
    Mm2RosterRecord &victim = roster_->records[idx];
    char victim_name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&victim, victim_name, sizeof(victim_name));

    /* Simplified melee roll (0x10118 exact formula not traced): rng(1,maxDamage). */
    const int max_dmg = decodeMonsterMaxDamage(monsters_->records[m.type]);
    int dmg = rng_->range(1, max_dmg > 0 ? max_dmg : 1);
    if (party_blocking_[target_slot]) {
        dmg /= 2; /* 0x11B0A block halves incoming damage. */
    }

    if (victim.hp_current > static_cast<uint16_t>(dmg)) {
        victim.hp_current = static_cast<uint16_t>(victim.hp_current - dmg);
        std::snprintf(status_line_, sizeof(status_line_), "%s attacks %s for %d damage.", mon_name, victim_name,
                      dmg);
    } else {
        /* Lethal hit @ 0x4AAA-0x4AF8: first KO sets bit6 on $26; repeat -> $81. */
        victim.hp_current = 0;
        if ((victim.condition & 0x40) != 0) {
            victim.condition = 0x81;
        } else {
            victim.condition = static_cast<uint8_t>((victim.condition & 0xEF) | 0x40);
        }
        std::snprintf(status_line_, sizeof(status_line_), "%s attacks %s - %s falls!", mon_name, victim_name,
                      victim_name);
    }
}

void CombatSession::finishVictory(GameStateView &gs)
{
    int reward_count = 0;
    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
        if (partySlotEligibleForRewards(i)) {
            ++reward_count;
        }
    }
    /* 0x10B74 accrues kill XP into the fight pool; 0x12430 divides -$6FC6 by
     * the count of members with roster+$26 < $80 and adds the share to each
     * (+$62). Unconscious (0 HP, condition still < $80) receive a full share. */
    if (reward_count > 0 && xp_pool_ > 0) {
        const uint32_t share = xp_pool_ / static_cast<uint32_t>(reward_count);
        for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
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
        for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
            if (!partySlotEligibleForRewards(i)) {
                continue;
            }
            roster_->records[rosterIndexForPartySlot(i)].gold += gold;
            break;
        }
    }

    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 1); /* 0x12438 */
    gs.setRightPanelMode(saved_panel_mode_);                /* 0x131B8 */
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
    state_ = CombatState::Inactive;
}

void CombatSession::finishLeave(GameStateView &gs, bool fled)
{
    gs.setRightPanelMode(saved_panel_mode_); /* 0x131B8 */
    outcome_ = fled ? CombatOutcome::Fled : CombatOutcome::Defeated;
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

gfx::CombatPanelView CombatSession::panelView() const
{
    gfx::CombatPanelView view{};
    if (!monsters_) {
        return view;
    }

    std::snprintf(view.message, sizeof(view.message), "%s", status_line_);
    view.show_party_options = state_ == CombatState::AwaitingPartyOptions;
    view.show_command_options = state_ == CombatState::AwaitingCommand;
    view.show_cast_level = state_ == CombatState::AwaitingCastLevel;
    view.show_cast_number = state_ == CombatState::AwaitingCastNumber;
    view.cast_level = cast_level_;
    view.label_monster_slots =
        active() && state_ != CombatState::AwaitingPartyOptions && state_ != CombatState::AwaitingSurpriseDismiss;
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

    int listed = 0;
    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS && listed < 10; ++i) {
        if (!slots_[i].alive) {
            continue;
        }
        gfx::CombatMonsterLine &line = view.monster_lines[listed];
        if (view.label_monster_slots) {
            line.letter = static_cast<char>('A' + listed);
        }
        /* 0x12742: check glyph when the display slot is inside the melee-range
         * count A4-$524 (rolled per fight by 0x11D0C) — not an acted flag. */
        line.show_checkmark = view.label_monster_slots && listed < melee_range_count_;
        if (slots_[i].hp > 0 && slots_[i].hp < slots_[i].max_hp) {
            /* -$519 bit0 → abbrev table A4-$6FBC entry 7 "Hurt" (0x127FE). */
            std::snprintf(line.status_suffix, sizeof(line.status_suffix), "Hurt");
        }
        mm2_monster_name_to_cstr(&monsters_->records[slots_[i].type], line.name, sizeof(line.name));
        ++listed;
    }
    view.monster_line_count = listed;

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
