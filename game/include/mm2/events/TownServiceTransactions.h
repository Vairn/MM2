#pragma once
// OP_0E town-service transaction primitives — faithful leaf operations traced
// byte-exact from EXTRACTED/mm2.capstone.annotated.asm. NO presentation here:
// these are the pure state mutations the interactive shop/temple/training engine
// performs once the player has selected an option + character. The interactive
// menu loop (A4 vtable UI thunks -$7be4/-$7bfc/-$7ddc, RNG caption tables) is the
// swappable UI backend (see TownServiceMenu.h); the LOGIC below is ASM-canonical.

#include "mm2_gamestate.h"
#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

#include <cstdint>

namespace mm2::events {

/* gold_check_and_deduct @ 0x1C9C0 (== 0x1D90C). Per-CHARACTER gold (record+0x66):
 * if rec.gold >= amount, subtract and return true; otherwise leave untouched and
 * return false. Town services charge the *selected character's own* gold, not a
 * pooled party total. */
bool townSvcCharGoldDeduct(Mm2RosterRecord &rec, uint32_t amount);

/* hp_restore @ 0x1DD48. The permanent max is record+0x60 (hp_aux); the working
 * max is record+0x5E (hp_max) and current HP is record+0x74 (hp_current). When
 * the working max is below the permanent max, both the working max and current
 * HP are restored to the permanent max. Returns true if anything changed. */
bool townSvcRestoreHp(Mm2RosterRecord &rec);

/* Condition clear @ 0x1D736: clr.b $26(a0). Sets the condition byte to 0 (Good). */
void townSvcRestoreCondition(Mm2RosterRecord &rec);

struct TownSvcHealResult {
    bool paid = false;     /* gold was sufficient and deducted */
    uint32_t cost = 0;     /* gold charged (0 if not paid) */
    bool hp_restored = false;
};

/* Temple heal flow @ 0x1D716: deduct `cost` from the character's own gold
 * (0x1D90C); on success restore HP (0x1DD48) and clear the condition byte
 * (clr.b $26). `cost` is the per-character healing charge — for a LIVING
 * character that is level*training_index*10 (doc 34 §13.2 / townSvcHealingCost).
 * NOTE: the dead (x10) / eradicated (x100) multipliers are a documented gap
 * (roster $26 only groups $80+; the dead-vs-eradicated threshold is not yet
 * ASM-confirmed) and must be folded into `cost` by the caller when known. */
TownSvcHealResult townSvcHeal(Mm2RosterRecord &rec, uint32_t cost);

/* training_stat_apply @ 0x1C898. NOTE: this is NOT the Training Hall. It is the
 * separate "stat shrine" leaf (the Atlantium beautify / olympic stat add). It
 * raises a base-stat byte selected by `stat_id` 0..6 (jump table @ 0x1C86C /
 * mm2_town_stat_field_offset) by the per-map add (A4-$6720[map_id]); written
 * ONLY when the rolled value is >= add (8-bit overflow guard @ 0x1C8C2) AND <
 * cap (A4-$671A[map_id] @ 0x1C8A8). Retained for that shrine path; the Training
 * Hall uses townSvcTrainLevelUp() below. Returns true when the byte was written. */
bool townSvcTrainStat(Mm2RosterRecord &rec, int stat_id, int map_id);

/* Training-hall LEVEL UP (OP_0E 0x04 -> -$7D16). The Training Hall does NOT raise
 * a stat: it advances the character one LEVEL when they have the experience
 * (record+0x62, threshold = mm2_class_xp_for_level for the next level on the
 * class's XP curve — TWO curves, Group A/B, see mm2_town_tables.h) AND can pay
 * the fee (level*training_town_index*50). */
struct TownSvcTrainResult {
    bool eligible = false;     /* had >= the XP threshold for the next level */
    bool paid = false;         /* fee affordable and deducted */
    bool leveled = false;      /* level field actually incremented */
    uint32_t cost = 0;         /* fee charged (0 if not paid) */
    uint32_t required_xp = 0;  /* threshold for next_level */
    uint8_t old_level = 0;
    uint8_t new_level = 0;
    uint16_t hp_gain = 0;      /* HP added (doc-32 model; RNG roll deferred) */
    uint16_t sp_gain = 0;      /* SP added for casters (0 otherwise) */
    uint8_t spell_level = 0;   /* spell level after the level-up */
};

/* Faithful Training Hall transaction:
 *   1) next = level+1; threshold = mm2_class_xp_for_level(class, next).
 *   2) if experience < threshold -> not eligible, NO charge (return early).
 *   3) cost = level*training_town_index*50; deduct from the char's own gold
 *      (0x1C9C0); insufficient -> paid=false, no level change.
 *   4) on success: level++ (record+0x71), then recompute HP/SP/spell level.
 * HP/SP recompute uses the documented doc-32 per-level model (class base HP +
 * END bonus; caster SP = stat bonus + 3) because the exact per-level RANDOM roll
 * (HP path @ 0x9BCA) is a documented gap — see mm2_town_tables.h. spell_level is
 * raised to mm2_class_spell_level_for(class, new_level). */
TownSvcTrainResult townSvcTrainLevelUp(Mm2RosterRecord &rec, int map_id);

struct TownSvcDonateResult {
    bool paid = false;
    uint32_t cost = 0;
    bool all_temples_donated = false; /* A4-$799E reached 0x1F this donation */
};

/* Temple donation (doc 28 §5.2). cost = donation_gold[map_id] (A4-$6742); paid
 * from the selected character's own gold (0x1C9C0). On success the per-town quest
 * bit (A4-$66B1[map_id]) is OR'd into the global temple-donation bitfield
 * (A4-$799E, MM2_GS_TEMPLE_DONATION); all_temples_donated is set when the field
 * reaches 0x1F. The 0x1F reward sequence itself (found-item buffer A4-$794C /
 * A4-$3F1C, stat bump A4-$5770, Nordon farthing payoff) is engine/presentation
 * and remains DEFERRED — only the gold + bitfield state are applied here. */
TownSvcDonateResult townSvcTempleDonate(uint8_t *a4, Mm2RosterRecord &rec, int map_id);

/* Cost helpers (FAQ §3-6, doc 34 §13.2), thin wrappers over mm2_town_tables. */
uint32_t townSvcTrainingCost(int level, int map_id);
uint32_t townSvcHealingCost(int level, int map_id);

/* Why a smith purchase was rejected (matches the engine's error captions @
 * 0x1C432 with the indices shown). */
enum class TownSvcBuyReject : uint8_t {
    None = 0,
    Condition,    /* 0x1BE4C: char condition byte $26 != 0 -> caption 8 */
    BackpackFull, /* 0x1BE82: all six backpack slots used -> caption 2 */
    NotEnoughGold /* 0x1BDD6 scc(gold<price) -> caption 4 */
};

struct TownSvcBuyResult {
    bool bought = false;
    TownSvcBuyReject reject = TownSvcBuyReject::None;
    uint32_t price = 0;
    int slot = -1; /* backpack slot 0..5 the item landed in */
};

/* Blacksmith buy leaf (0x1BE44). Faithful ordering of the ASM guards:
 *   1) reject if rec.condition ($26) != 0 (0x1BE4C tst.b $26).
 *   2) scan the backpack id-run (+$3A) for the first empty slot; reject as
 *      BackpackFull when all six are used (0x1BE82 cmpi #6).
 *   3) gold check + deduct from the char's own gold (record+$66) via the
 *      scc(gold>=price)+subtract primitive (0x1BDD6 == 0x1C9C0). No partial spend.
 *   4) write item_id -> backpack +$3A, charges -> +$40, flags -> +$46 (0x1BEBC),
 *      via the SoA roster accessors (matches the OP_19 give-item path).
 * `price` is the precomputed shop price (mm2_smith_price over items.dat gold);
 * charges/flags are the per-category buy fields (mm2_smith_buy_fields). The
 * Merchant-skill half-price discount (-$7F32 @ 0x1BFB4) is a documented gap. */
TownSvcBuyResult townSvcSmithBuy(Mm2RosterRecord &rec, uint8_t item_id, uint8_t charges,
                                 uint8_t flags, uint32_t price);

enum class TownSvcSellReject : uint8_t {
    None = 0,
    Condition, /* 0x1BC2E: char condition $26 != 0 -> caption 8 */
    NoItem,    /* 0x1BC48: empty backpack slot -> caption 6 */
};

struct TownSvcSellResult {
    bool sold = false;
    TownSvcSellReject reject = TownSvcSellReject::None;
    uint32_t price = 0;
    int slot = -1;
};

/* Blacksmith sell leaf (0x1BC26). Guards: condition==0, slot occupied; then
 * add `price` to record+$66 (0x1B62A) and clear backpack id/charges/flags. */
TownSvcSellResult townSvcSmithSell(Mm2RosterRecord &rec, int backpack_slot, uint32_t price);

enum class TownSvcIdentifyReject : uint8_t {
    None = 0,
    Condition,
    NoItem,
    NotEnoughGold,
};

struct TownSvcIdentifyResult {
    bool identified = false;
    TownSvcIdentifyReject reject = TownSvcIdentifyReject::None;
    uint32_t cost = 0;
};

/* Blacksmith identify leaf (0x1B6E0). Deducts identify cost (0x1BDD6), then
 * fills `summary` with item details (presentation-only; item bytes unchanged). */
TownSvcIdentifyResult townSvcSmithIdentify(Mm2RosterRecord &rec, int backpack_slot,
                                           const Mm2ItemsFile *items, uint32_t cost,
                                           char *summary, size_t summary_cap);

/* Pub feeding frenzy (tavern menu A, leaf 0x1CA2E @ 0x1D58E). The active member
 * pays A4-$6742[map] (same BE u16 table as temple donations), then every living
 * party member with food (record+$25) below 0x28 is topped up to 40. */
struct TownSvcFeedingResult {
    bool fed = false;
    bool paid = false;
    uint32_t cost = 0;
    int members_topped = 0;
};

TownSvcFeedingResult townSvcFeedingFrenzy(Mm2RosterRecord &payer, const Mm2PartyLaunch *launch,
                                         Mm2RosterFile *roster, int map_id);

/* Mage-guild shop-open membership gate (0x1E410, ASM-confirmed): the guild only
 * opens for a party with >=1 member whose record+0x79 (class_quest_guild_mask)
 * has the per-town bit set (mm2_mage_guild_member_mask). See
 * EXTRACTED/decomp/mm2_town_tables.h for the two confirmed write paths (the
 * ALREADY-PORTED generic event-script field selector 0x74, and the unported,
 * buggy 0x9D76 class-quest reward loop, doc 36-class-quest-hp-bug.md) — so a
 * party that has triggered the right event.dat script IS a real member; a
 * fresh roster that hasn't is correctly denied. Do not confuse with OP_0E
 * 0x0D "enroll" (writes record+0x0B town_flags, a different field with no
 * confirmed read site). */
bool townSvcMageGuildMember(const Mm2RosterRecord &rec, int map_id);

/* True if ANY member of the party has the map_id membership bit (0x1E410's
 * actual per-member OR loop before the shop opens). */
bool townSvcPartyHasMageGuildMember(const Mm2RosterRecord *const *members, int count,
                                    int map_id);

enum class TownSvcSpellReject : uint8_t {
    None = 0,
    NotForSale,   /* 0x1D882: decoded slot cost == 0 -> caption 0x18 (empty slot) */
    Condition,    /* 0x1D898: char condition $26 != 0 -> caption 8 */
    NotEnoughGold /* 0x1D91C scc(gold<cost) -> caption 0xC */
};

struct TownSvcSpellResult {
    bool learned = false;
    TownSvcSpellReject reject = TownSvcSpellReject::None;
    uint32_t cost = 0;
};

/* Mage guild / temple spell-purchase leaf (0x1D872, shared by both shops). The
 * ASM gate order is: (1) decoded slot cost != 0 (0x1D882 tst.l — an unpopulated
 * slot, not an "already known" check), (2) char condition $26 == 0 (0x1D898),
 * (3) gold check+deduct (0x1D90C == 0x1C9C0), (4) grant: raw record+0x51+(n>>3)
 * bit OR keyed only on the flat spell index `n` (0x1D8D4/0x1D8FA) — idempotent
 * if already known, and NO class-id check was found in the traced ASM, so none
 * is enforced here (a per-class restriction, if any, lives in the menu-open /
 * character-select UI, which is presentation and out of scope). */
TownSvcSpellResult townSvcBuySpell(Mm2RosterRecord &rec, int spell_index, uint32_t cost);

}  // namespace mm2::events
