#include "mm2/combat/CombatSession.h"

#include "mm2/CppStdCompat.h"
#include "mm2/combat/EncounterPicker.h"
#include "mm2/events/EventVmHelpers.h"

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

    const uint8_t mode = mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE);
    if (mode != 0x80) {
        encounterInitXpBudget(gs, *roster_, *launch_);
        encounterRunRandomPicker(gs, world.attrib(), *rng_, &friendCountFromMonsters, monsters_);
    }
    const uint8_t live = encounterRecomputeLiveCount(gs);
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
    for (int i = 0; i < MM2_GS_PARTY_SIZE; ++i) {
        party_acted_[i] = false;
        party_blocking_[i] = false;
    }

    xp_pool_ = 0;
    outcome_ = CombatOutcome::None;
    active_party_slot_ = -1;
    active_monster_slot_ = -1;
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

void CombatSession::startRoundLoop(GameStateView &gs, const world::MapWorld &world)
{
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

    if (state_ != CombatState::AwaitingCommand) {
        return false;
    }

    const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
    if (ch != 'A' && ch != 'B' && ch != 'R') {
        return false;
    }

    switch (ch) {
    case 'A':
        resolvePlayerAttack(gs);
        break;
    case 'B':
        resolvePlayerBlock();
        break;
    case 'R':
        resolvePlayerRun(gs, world);
        break;
    default:
        break;
    }

    if (state_ == CombatState::Inactive) {
        return true; /* Run succeeded -> combat ended this tick. */
    }

    party_acted_[active_party_slot_] = true;
    active_party_slot_ = -1;
    runUntilDecisionOrEnd(gs, world);
    return state_ == CombatState::Inactive;
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
            const int idx = rosterIndexForPartySlot(best_party);
            char name[MM2_ROSTER_NAME_SIZE + 1];
            mm2_roster_name_to_cstr(&roster_->records[idx], name, sizeof(name));
            std::snprintf(status_line_, sizeof(status_line_), "%s's turn - (A)ttack (B)lock (R)un?", name);
            return;
        }

        resolveMonsterTurn(gs, best_monster);
        monster_acted_[best_monster] = true;
    }
}

void CombatSession::resolvePlayerAttack(GameStateView &gs)
{
    const int target = firstAliveMonster();
    if (target < 0) {
        return;
    }
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    const Mm2RosterRecord &attacker = roster_->records[idx];
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&attacker, name, sizeof(name));

    /* Simplified melee roll (0xCD90 exact to-hit/damage formula not traced —
     * doc 17 open item): rng(1, might_current). */
    const int might = attacker.might_current > 0 ? attacker.might_current : 1;
    const int dmg = rng_->range(1, might);

    CombatMonster &m = slots_[target];
    m.hp -= dmg;
    char mon_name[MM2_MONSTER_NAME_SIZE + 1];
    mm2_monster_name_to_cstr(&monsters_->records[m.type], mon_name, sizeof(mon_name));

    if (m.hp <= 0) {
        m.hp = 0;
        m.alive = false;
        xp_pool_ += mm2_monster_decode_xp(&monsters_->records[m.type]); /* 0x10B74 XP accrual */
        std::snprintf(status_line_, sizeof(status_line_), "%s attacks %s for %d - %s is destroyed!", name,
                      mon_name, dmg, mon_name);
    } else {
        std::snprintf(status_line_, sizeof(status_line_), "%s attacks %s for %d damage.", name, mon_name, dmg);
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
    view.label_monster_slots =
        active() && state_ != CombatState::AwaitingPartyOptions && state_ != CombatState::AwaitingSurpriseDismiss;
    if (view.show_command_options && active_party_slot_ >= 0) {
        const int idx = rosterIndexForPartySlot(active_party_slot_);
        if (idx >= 0) {
            mm2_roster_name_to_cstr(&roster_->records[idx], view.options_for, sizeof(view.options_for));
        }
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
        line.show_checkmark = view.label_monster_slots && i == active_monster_slot_;
        if (slots_[i].hp > 0 && slots_[i].hp < slots_[i].max_hp) {
            std::snprintf(line.status_suffix, sizeof(line.status_suffix), "Hurt");
        }
        mm2_monster_name_to_cstr(&monsters_->records[slots_[i].type], line.name, sizeof(line.name));
        ++listed;
    }
    view.monster_line_count = listed;

    if (alive_total > 10) {
        view.overflow_more = alive_total - 10;
        const uint8_t type = overflow_type_ != 0 ? overflow_type_ : slots_[0].type;
        mm2_monster_name_to_cstr(&monsters_->records[type], view.overflow_name, sizeof(view.overflow_name));
    }

    for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {
        if (slots_[i].alive) {
            view.sprite_disk_index = monsterPictureDiskIndex(monsters_, slots_[i].type);
            break;
        }
    }

    return view;
}

}  // namespace mm2::combat
