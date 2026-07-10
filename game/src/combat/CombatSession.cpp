#include "mm2/combat/CombatSession.h"

#include "mm2/CppStdCompat.h"
#include "mm2/combat/EncounterPicker.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/TownServiceTransactions.h"
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
    mm2_gs_set_u32(gs.a4(), MM2_GS_COMBAT_XP_POOL, 0); /* 0x12CB4 clr.l -$6FC6 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TYPE_HI, 0); /* 0x12C80 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_LOOT_ITEM_TIER, 0);    /* 0x12C84 */
    mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_FLEE_PRINT, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERADICATE_SKIP_PRINT, 0);
    msg_queue_len_ = 0;
    msg_queue_idx_ = 0;
    outcome_ = CombatOutcome::None;
    active_party_slot_ = -1;
    active_monster_slot_ = -1;
    melee_range_count_ = 0; /* 0x12C90 clears -$524 */
    front_rank_count_ = 0;  /* 0x12C8C clears -$5E4D */
    surprise_mode_ = 0;
    seedCombatStaticTables(gs);
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
    /* 0x13104: rng(1,100) from party-options entry vs A4-$5E4E (0x12D42 ← -$7EFC→0x4B60).
     * 0x4B60: mean of party +$1E thievery, divu by -$795A, cap $FF. Success → leave. */
    (void)world;
    const int roll = rng_ ? rng_->range(1, 100) : 1;
    int sum = 0;
    int n = 0;
    for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
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

bool CombatSession::tick(GameStateView &gs, const world::MapWorld &world, const platform::KeyState &keys)
{
    if (state_ == CombatState::AwaitingActionAck) {
        if (keys.last_ascii == 0) {
            return false;
        }
        /* 0x132E6 host: pace multi-target spell lines one ack each. */
        if (advanceCombatMessageQueue()) {
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
            state_ == CombatState::AwaitingCastTarget || state_ == CombatState::AwaitingPartyPick) {
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

    if (state_ == CombatState::AwaitingPartyPick) {
        const char ch = static_cast<char>(keys.last_ascii);
        if (ch == 0 && !keys.escape) {
            return false;
        }
        if (keys.escape || ch == 0x1B) {
            pending_cast_flat_ = -1;
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
    /* resolvePlayerCast may enter AwaitingCastTarget / AwaitingPartyPick; otherwise ack. */
    if (state_ != CombatState::AwaitingCastTarget && state_ != CombatState::AwaitingPartyPick) {
        state_ = CombatState::AwaitingActionAck;
    }
    return true;
}

void CombatSession::resolvePlayerCast(GameStateView &gs, int flat0)
{
    const int idx = rosterIndexForPartySlot(active_party_slot_);
    Mm2RosterRecord &caster = roster_->records[idx];
    char name[MM2_ROSTER_NAME_SIZE + 1];
    mm2_roster_name_to_cstr(&caster, name, sizeof(name));

    const gameplay::SpellSchool school = gameplay::spellSchoolForClass(caster.class_id);
    const gameplay::SpellMeta *table = gameplay::schoolSpellTable(school);
    const gameplay::SpellMeta *meta =
        (table && flat0 >= 0 && flat0 < gameplay::kSpellsPerSchool) ? &table[flat0] : nullptr;
    const char *spell_name = meta ? meta->name : "?";

    /* Cost staging → 0x6DC6: A4-$3F0A SP, A4-$3F08 gems; clr both; may set -$79E8. */
    uint16_t sp_cost = 0;
    uint16_t gem_cost = 0;
    if (meta) {
        sp_cost = meta->sp;
        if (meta->per_level) {
            sp_cost = static_cast<uint16_t>(sp_cost * (caster.level > 0 ? caster.level : 1));
        }
        gem_cost = meta->gems;
    }
    mm2_gs_set_u16(gs.a4(), -0x3F0A, sp_cost);
    mm2_gs_set_u16(gs.a4(), -0x3F08, gem_cost);
    /* 0x6DC6: always sub gems from +$5C; SP from +$58 only if enough. */
    if (caster.gems >= gem_cost) {
        caster.gems = static_cast<uint16_t>(caster.gems - gem_cost);
    } else {
        caster.gems = 0;
    }
    if (caster.sp_current >= sp_cost) {
        caster.sp_current = static_cast<uint16_t>(caster.sp_current - sp_cost);
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
        if (flat0 == 4) { /* S1/5 Light @ 0xA8D8 */
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
        } else if (flat0 == 19) { /* S3/6 Wizard Eye @ 0xAD1C */
            add_eye(MM2_GS_WIZARD_EYE_TIMER);
            effect = true;
        } else if (flat0 == 23) { /* S4/4 Guard Dog @ 0xAD7A */
            addq_flag(MM2_GS_GUARD_DOG_FLAG);
            effect = true;
        } else if (flat0 == 24) { /* S4/5 Shield @ 0xBB84 */
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_SHIELD_COUNTER);
            if (v < 0xFF) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_SHIELD_COUNTER, static_cast<uint8_t>(v + 1));
            }
            effect = true;
        } else if (flat0 == 25) { /* S4/6 Time Distortion @ 0xBBAE */
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x08) == 0) {
                uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_TIME_DISTORT);
                mm2_gs_set_u8(gs.a4(), MM2_GS_TIME_DISTORT, static_cast<uint8_t>(v + 1));
                effect = true;
            }
        } else if (flat0 == 29) { /* S5/4 Shelter @ 0xADA8 */
            addq_flag(MM2_GS_SHELTER_FLAG);
            effect = true;
        } else if (flat0 == 32) { /* S6/2 Entrapment @ 0xBCE0 */
            if ((mm2_gs_u8(gs.a4(), -0x5600) & 0x01) == 0) {
                uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_ENTRAPMENT);
                mm2_gs_set_u8(gs.a4(), MM2_GS_ENTRAPMENT, static_cast<uint8_t>(v + 1));
                effect = true;
            }
        } else if (flat0 == 43) { /* S8/4 Power Shield @ 0xBEE2 */
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_POWER_SHIELD_CTR);
            if (v < 0xFF) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_POWER_SHIELD_CTR, static_cast<uint8_t>(v + 1));
            }
            effect = true;
        }
    } else if (meta && school == gameplay::SpellSchool::Cleric) {
        if (flat0 == 2) { /* C1/3 Bless @ 0xBFEC */
            uint8_t v = mm2_gs_u8(gs.a4(), MM2_GS_BLESS_COUNTER);
            if (v < 0xFF) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_BLESS_COUNTER, static_cast<uint8_t>(v + 1));
            }
            effect = true;
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
            const int pc = launch_->party_count;
            if (pc <= 1) {
                applyHeroismToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            } else {
                pending_cast_flat_ = flat0;
                state_ = CombatState::AwaitingPartyPick;
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0);
                std::snprintf(status_line_, sizeof(status_line_), "On whom (1-%d)?", pc);
                return;
            }
        } else if (flat0 == 28) { /* C5/3 Frenzy @ 0xC3AE → 0xD2EA */
            if (mm2_gs_u8(gs.a4(), MM2_GS_FRENZY_LATCH) != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "* Spell Failed *");
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            const int pc = launch_->party_count;
            if (pc <= 1) {
                applyFrenzyToPartySlot(gs, 0);
                mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
                return;
            }
            pending_cast_flat_ = flat0;
            state_ = CombatState::AwaitingPartyPick;
            mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 0);
            std::snprintf(status_line_, sizeof(status_line_), "On whom (1-%d)?", pc);
            return;
        } else if (flat0 == 44) { /* C9/1 Divine Intervention @ 0xC6D6 */
            if (mm2_gs_u8(gs.a4(), -0x51A) == 0) {
                mm2_gs_set_u8(gs.a4(), -0x51A, 1);
                for (int p = 0; p < launch_->party_count && p < MM2_GS_PARTY_SIZE; ++p) {
                    const int ri = rosterIndexForPartySlot(p);
                    if (ri < 0) {
                        continue;
                    }
                    Mm2RosterRecord &r = roster_->records[ri];
                    if (r.condition != 0xFF) {
                        r.condition = 0;
                    }
                    r.hp_current = r.hp_max;
                }
                effect = true;
            }
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

    /* Combat-only stubs needing 0xD464 monster letter (CAST='C' damage/status). */
    static const uint8_t kSorcCombat[] = {2,  3,  6,  8,  9,  14, 17, 18, 20, 21, 22,
                                          26, 27, 28, 31, 33, 35, 36, 39, 40, 41, 42, 44, 45, 46};
    static const uint8_t kClerCombat[] = {0,  10, 12, 13, 14, 17, 20, 26, 27, 29, 36, 37, 38, 40, 42};
    bool combat_target = false;
    if (school == gameplay::SpellSchool::Sorcerer) {
        for (uint8_t f : kSorcCombat) {
            if (f == static_cast<uint8_t>(flat0)) {
                combat_target = true;
                break;
            }
        }
    } else if (school == gameplay::SpellSchool::Cleric) {
        for (uint8_t f : kClerCombat) {
            if (f == static_cast<uint8_t>(flat0)) {
                combat_target = true;
                break;
            }
        }
    }
    if (combat_target) {
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
    state_ = CombatState::AwaitingActionAck;
    return true;
}

bool CombatSession::tickPartyPick(GameStateView &gs, char key)
{
    /* 0xD2EA / 0xD312: '1'..party_count → party slot index. */
    const int pc = launch_ ? launch_->party_count : 0;
    if (key < '1' || key > static_cast<char>('0' + pc)) {
        return false;
    }
    const int party_slot = key - '1';
    const int flat = pending_cast_flat_;
    pending_cast_flat_ = -1;
    mm2_gs_set_u8(gs.a4(), MM2_GS_SPELL_ACTED, 1);
    if (flat == 8) {
        applyHeroismToPartySlot(gs, party_slot);
    } else if (flat == 28) {
        applyFrenzyToPartySlot(gs, party_slot);
    }
    state_ = CombatState::AwaitingActionAck;
    return true;
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
    /* A4-$6F3E / -$6F36: class → to-hit level divisor / swing-count divisor. */
    static const uint8_t kHitDiv[8] = {1, 1, 1, 3, 4, 2, 2, 1};
    static const uint8_t kSwingDiv[8] = {4, 4, 5, 7, 10, 5, 5, 4};
    for (int i = 0; i < 8; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_STATUS_BIT_TBL + i, kStatusBits[i]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_MRES_CHANCE_TBL + i, kMresChance[i]);
        mm2_gs_set_u8(gs.a4(), -0x6F3E + i, kHitDiv[i]);
        mm2_gs_set_u8(gs.a4(), -0x6F36 + i, kSwingDiv[i]);
    }
    for (int i = 0; i < 4; ++i) {
        mm2_gs_set_u8(gs.a4(), -0x6F1A + i, kFleeChance[i]);
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
    if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS || !monsters_ || !slots_[slot].alive) {
        return;
    }
    const Mm2MonsterRecord &rec = monsters_->records[slots_[slot].type];
    Mm2MonsterAbilities ab{};
    mm2_monster_decode_abilities(&rec, &ab);
    const uint8_t ac = rec.fields[MM2_MON_OFF_AC - MM2_MONSTER_NAME_SIZE];
    const uint8_t dmg = rec.fields[MM2_MON_OFF_DAMAGE - MM2_MONSTER_NAME_SIZE];
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
    uint8_t melee = mm2_gs_u8(gs.a4(), -0x524);
    if (melee > live) {
        mm2_gs_set_u8(gs.a4(), -0x524, live);
        melee_range_count_ = live;
    }
    /* Overflow-only kill (slot >= 10): no visible-row shift. */
    if (slot >= 0x0A) {
        if (slot < MM2_GS_MONSTER_BATTLE_SLOTS) {
            slots_[slot] = CombatMonster{};
            monster_acted_[slot] = false;
        }
        syncMonsterSlotsToGs(gs);
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
    const uint8_t mon_type = slots_[slot].type;
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

    /* Identify Monster @ 0xB788: print type/HP/undead into status line (UI host). */
    if (school == gameplay::SpellSchool::Sorcerer && flat0 == 9) {
        if (slot < 0 || slot >= MM2_GS_MONSTER_BATTLE_SLOTS || !slots_[slot].alive || !monsters_) {
            std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
            return;
        }
        loadMonsterCombatGlobals(gs, slot);
        char mon_name[MM2_MONSTER_NAME_SIZE + 1];
        mm2_monster_name_to_cstr(&monsters_->records[slots_[slot].type], mon_name, sizeof(mon_name));
        const char *und =
            mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_UNDEAD) ? "undead" : "living";
        std::snprintf(status_line_, sizeof(status_line_), "%s: %s HP %d (%s).", spell_name, mon_name,
                      slots_[slot].hp, und);
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
        {2, 0, 5, 1, 0, 1, 0, 1},
        {3, 1, 5, 1, 3, 1, 0, 1},
        {6, 2, 0, 0, 4, 0xFF, 1, 5},  /* Sleep: hit=1333A, $d=5 */
        {8, 1, 9, 1, 7, 2, 0, 1},
        {14, 0, 5, 3, 0, 4, 0, 0},
        {17, 0, 5, 1, 0, 2, 0, 0},
        {18, 2, 0, 0, 5, 0xFF, 1, 6}, /* Web: hit=1333A, $d=6 */
        {20, 0, 0, 6, 0, 3, 0, 1},
        {21, 2, 0, 0, 6, 1, 1, 0},
        {22, 0, 5, 1, 0, 1, 0, 0},
        {26, 3, 0x64, 0, 0, 1, 0, 0},
        {27, 2, 0, 0, 8, 6, 1, 0},
        {28, 0, 7, 1, 0, 10, 0, 0},
        {31, 2, 0, 0, 9, 3, 1, 0},
        {33, 0, 0, 10, 0, 3, 0, 0},
        {35, 0, 0, 20, 0, 2, 0, 0},
        {36, 0, 11, 1, 0, 10, 0, 0},
        {39, 1, 4, 1, 4, 10, 0, 0},
        {40, 0, 21, 19, 0, 1, 0, 0},
        {41, 0, 9, 7, 0, 10, 0, 0},
        {42, 1, 21, 1, 4, 1, 0, 0},
        {44, 3, 0x3E8, 0, 0, 1, 0, 0},
        {45, 0, 16, 4, 0, 10, 0, 0},
        {46, 1, 161, 1, 4, 1, 0, 0},
    };
    static const SpellFx kClerFx[] = {
        {0, 2, 0, 0, 3, 6, 1, 0},
        {10, 1, 12, 1, 3, 1, 0, 0},
        {12, 2, 0, 0, 1, 0xFF, 1, 6}, /* Silence: hit=1333A, $d=6 */
        {13, 2, 0, 0, 2, 10, 1, 0},
        {14, 3, 0x19, 0, 0, 3, 0, 0},
        {17, 3, 0x19, 0, 0, 5, 0, 3}, /* Immobilize: hit=5, $d=3, dmg=25 */
        {20, 1, 49, 1, 4, 4, 0, 0},
        {26, 2, 0, 0, 7, 1, 1, 0},
        {27, 1, 33, 1, 7, 10, 0, 0},
        {29, 2, 0, 0, 5, 6, 1, 0},
        {36, 2, 0, 0, 7, 4, 1, 0},
        {37, 1, 400, 255, 4, 1, 0, 0},
        {38, 1, 91, 1, 4, 10, 0, 0},
        {40, 2, 0, 0, 7, 1, 1, 0},
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

    uint8_t hits = fx->hit_count;
    if (hits == 0xFF) {
        hits = spellTargetPower13362(gs, level);
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
    }

    applySpell108BC(gs, slot, hits > 0 ? hits : 1, mode_d);
    if (fx->kind == 2 && status_line_[0] == '\0') {
        std::snprintf(status_line_, sizeof(status_line_), "%s casts %s.", name, spell_name);
    }
}

void CombatSession::resolvePlayerAttack(GameStateView &gs, bool shooting)
{
    /* Player attack resolve inside 0x111DA @ 0x112DE..0x115F8 (not 0xCD90 —
     * that is the skill/spell dispatcher; 0x10118 is monster-slot swap). */
    const int target = firstAliveMonster();
    if (target < 0) {
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

    /* 0x1070C–0x10776 flee: status bit6 / Entrapment block; else tier vs -$6FC2 + rng. */
    if ((m.status & 0x40) == 0 && mm2_gs_u8(gs.a4(), MM2_GS_ENTRAPMENT) == 0) {
        Mm2MonsterAbilities ab{};
        mm2_monster_decode_abilities(&monsters_->records[m.type], &ab);
        const uint8_t tier = static_cast<uint8_t>(ab.flee_tier & 3);
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
    /* 0x12430: victory latch, optional 0x123B0 item fill, then XP split. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 1); /* 0x12438 */
    /* 0x1243e: clr.b -$794C */
    mm2_gs_set_u8(gs.a4(), MM2_GS_FOUND_SENTINEL, 0);
    generateVictoryItems123B0(gs);

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
    state_ = CombatState::Inactive;
}

void CombatSession::finishLeave(GameStateView &gs, bool fled)
{
    gs.setRightPanelMode(saved_panel_mode_); /* 0x131B8 */
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
        for (int i = 0; i < launch_->party_count && i < MM2_GS_PARTY_SIZE; ++i) {
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
    view.show_cast_target = state_ == CombatState::AwaitingCastTarget;
    view.show_party_pick = state_ == CombatState::AwaitingPartyPick;
    view.cast_level = cast_level_;
    view.label_monster_slots =
        active() && state_ != CombatState::AwaitingPartyOptions &&
        state_ != CombatState::AwaitingBribeKind && state_ != CombatState::AwaitingBribeAmount &&
        state_ != CombatState::AwaitingSurpriseDismiss && state_ != CombatState::AwaitingPartyPick;
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
