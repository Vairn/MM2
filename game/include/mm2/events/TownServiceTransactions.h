#pragma once
// OP_0E town-service transaction primitives — faithful leaf operations traced
// byte-exact from EXTRACTED/mm2.capstone.annotated.asm. NO presentation here:
// these are the pure state mutations the interactive shop/temple/training engine
// performs once the player has selected an option + character. The interactive
// menu loop (A4 vtable UI thunks -$7be4/-$7bfc/-$7ddc, RNG caption tables) is the
// swappable UI backend (see TownServiceMenu.h); the LOGIC below is ASM-canonical.

#include "mm2/gameplay/ExploreActions.h"
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
 * (clr.b $26). Prefer townSvcTempleHealCost() for the ASM cost builder. */
TownSvcHealResult townSvcHeal(Mm2RosterRecord &rec, uint32_t cost);

struct TownSvcAlignResult {
    bool paid = false;
    uint32_t cost = 0;
    bool restored = false; /* wrote +$0D → +$6A */
};

/* Temple B @ 0x1D758: gold vs A4-$56BA (from 0x1DC3A), then move.b $d,$6a —
 * alignment_current → alignment_base. Cost 0 when already matched. */
TownSvcAlignResult townSvcRestoreAlignment(Mm2RosterRecord &rec, uint32_t cost);

/* Temple heal cost builder 0x1DCA2: base by condition ($FF→1000, ≥$80→100,
 * else 10 if cond!=0 or HP needs restore, else 0), then ×level ×A4-$6714. */
uint32_t townSvcTempleHealCost(const Mm2RosterRecord &rec, int map_id);

/* Temple align cost builder 0x1DC3A: 0 if +$0D==+$6A; else 100×level×A4-$6714. */
uint32_t townSvcTempleAlignCost(const Mm2RosterRecord &rec, int map_id);

/* training_stat_apply @ 0x1C898. NOTE: this is NOT the Training Hall. It is the
 * separate "stat shrine" leaf (the Atlantium beautify / olympic stat add). It
 * raises a base-stat byte selected by `stat_id` 0..6 (jump table @ 0x1C86C /
 * mm2_town_stat_field_offset) by the per-map add (A4-$6720[map_id]); written
 * ONLY when the rolled value is >= add (8-bit overflow guard @ 0x1C8C2) AND <
 * cap (A4-$671A[map_id] @ 0x1C8A8). Retained for that shrine path; the Training
 * Hall uses townSvcTrainLevelUp() below. Returns true when the byte was written. */
bool townSvcTrainStat(Mm2RosterRecord &rec, int stat_id, int map_id);

/* Training-hall LEVEL UP (OP_0E 0x02 -> -$7CD4). Advances one LEVEL when XP
 * threshold is met and fee (level*training_town_index*50) is paid. */
struct TownSvcTrainResult {
    bool eligible = false;
    bool paid = false;
    bool leveled = false;
    uint32_t cost = 0;
    uint32_t required_xp = 0;
    uint8_t old_level = 0;
    uint8_t new_level = 0;
    uint16_t hp_gain = 0;
    uint16_t sp_gain = 0;
    uint8_t spell_level = 0;
};

/* Level-up HP @ 0x20390: ($64DA[class]*$64EE[map])/$64E4[map] + -$7F56(+$27).
 * 0x9BCA is bash-door — do not use it here. */
TownSvcTrainResult townSvcTrainLevelUp(Mm2RosterRecord &rec, int map_id,
                                       gameplay::Rng *rng = nullptr);

struct TownSvcDonateResult {
    bool paid = false;
    uint32_t cost = 0;
    bool all_temples_donated = false; /* A4-$799E reached 0x1F this donation */
    bool reward_queued = false; /* 0x1D7E8: sentinel $FE + item 0xD4 in found buffer */
    bool blessed = false; /* RNG(1,100)>0x5A → buff writes @ 0x1D7FE */
};

/* Temple C @ 0x1D796: cost = A4-$6714[map]×100 (0x1DC1A); OR quest bit into
 * A4-$799E. When bits==0x1F: move.b #$FE,-$794C; move.b #$D4,-$3F1C; clr -$799E.
 * Independent RNG -$7BB4(100,1)>0x5A → blessed buff writes 0x1D7FE..0x1D852
 * (A4-$79AB..-$799F + addq.w #1,A4-$5770). */
TownSvcDonateResult townSvcTempleDonate(uint8_t *a4, Mm2RosterRecord &rec, int map_id,
                                        gameplay::Rng *rng = nullptr);

uint32_t townSvcTrainingCost(int level, int map_id);
uint32_t townSvcHealingCost(int level, int map_id); /* healthy: level×$6714×10 */

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
 *   3) Merchant skill id 0x0A @ +$50 (-$7F32→0x45C4 @ 0x1BFB4): halve price.
 *   4) gold check + deduct from the char's own gold (record+$66) via the
 *      scc(gold>=price)+subtract primitive (0x1BDD6 == 0x1C9C0). No partial spend.
 *   5) write item_id -> backpack +$3A, charges -> +$40, flags -> +$46 (0x1BEBC),
 *      via the SoA roster accessors (matches the OP_19 give-item path).
 * `price` is the precomputed shop price (mm2_smith_price over items.dat gold)
 * before the Merchant half; charges/flags are the per-category buy fields. */
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
 * credit `price` (already mm2_smith_sell_price = buy/2) and if no Merchant
 * skill, halve again (0x1BFDC). Clear backpack id/charges/flags. */
TownSvcSellResult townSvcSmithSell(Mm2RosterRecord &rec, int backpack_slot, uint32_t price);

/* 0x1BFB4 Merchant adjust: buy → /2 if skill 0x0A; sell-halved → /2 if no skill. */
uint32_t townSvcSmithMerchantBuyPrice(uint32_t price, const Mm2RosterRecord &rec);
uint32_t townSvcSmithMerchantSellPrice(uint32_t sell_half_price, const Mm2RosterRecord &rec);

/* Today's Specials date-roll bonus @ 0x1C146 (category/mode 2): day%30==$1D →
 * A4-$681C[day/30], else A4-$6816[day%30]. Overwrites price_meta/flags. */
uint8_t townSvcSmithSpecialsDateBonus(const uint8_t *a4);

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
 * pays A4-$6742[map] (feeding_frenzy_gold), then every living party member with
 * food (record+$25) below 0x28 is topped up to 40. */
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

/* General store OP_0E 0x07 → 0xA62C: 100gp gate (0xA75E), then 0xA3AE table
 * keyed on each +$50 skill nibble; clears +$50 on success. Middlegate/Vulcania. */
struct TownSvcGeneralStoreResult {
    bool converted = false;
    bool paid = false;
    const char *message = nullptr; /* ASM inline @ 0xA7D2 / 0xA7F1 */
};

TownSvcGeneralStoreResult townSvcGeneralStoreConvert(Mm2RosterRecord &rec);

/* Circus OP_0E 0x64 → 0xDF04 win leaf 0xDD18. Menu keys 1..6 (0-based 0..5)
 * map to CURRENT attr offsets (ASM adda.l):
 *   0→+$10 might, 1→+$12 personality, 2→+$14 accuracy,
 *   3→+$27 endurance, 4→+$13 speed, 5→+$15 luck.
 * FAQ lists 7 games (incl. Shell Game / intellect) — ASM only has 6; default
 * branch uses +$11 intelligence. Cap: >0x5A → 0x64 else +0x0A. */
void townSvcCircusWinBoost(Mm2RosterRecord &rec, int attr_choice /*0..5*/);

/* Circus lose leaf 0xDE2C: 50% (rng 1..0xFE > 0x7F) places Cupie Doll 0xDA
 * in first empty backpack. Returns true when a doll was placed. */
bool townSvcCircusGiveCupieDoll(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                gameplay::Rng *rng);

/* Tavern B @ 0x1CAC4: A–F buy from A4-$6738 costs; apply via 0x1C7EC (base
 * stats +$6B..+$72). Purchase-count gate A4-$577E[slot] vs A4-$672C limits
 * (0x1CBDE): count < limit → pay + increment only; count >= limit → apply.
 * Sick roll RNG(1, end+10)==2 → bset #$3,$26. */
struct TownSvcStatBoostResult {
    bool ok = false;
    bool paid = false;
    bool applied = false; /* false when only the -$577E counter advanced */
    bool sick = false;
    uint32_t cost = 0;
    int slot = -1; /* 0..5 = A..F */
};

TownSvcStatBoostResult townSvcTavernStatBoost(uint8_t *a4, Mm2RosterRecord &rec, int slot,
                                              int map_id, gameplay::Rng *rng);

/* Tavern C specialties @ 0x1CD2E: pay A4-$6760[town*3+menu] (0x1CEA4), then
 * sick RNG(1, -$7F56(+$73)+5)==1 → bset #$2,$26 and SKIP mask; else
 * 0x1C8D4 OR A4-$786C into +$76. NOT the 0x18EC0 / +$78 encoder (0xC9/0xCA). */
struct TownSvcSpecialtyResult {
    bool ok = false;
    bool paid = false;
    bool sick = false;
    uint32_t cost = 0;
    int menu = -1; /* 0..2 = A..C */
};

TownSvcSpecialtyResult townSvcTavernSpecialty(Mm2RosterRecord &rec, int map_id, int menu /*0..2*/,
                                              gameplay::Rng *rng);

/* Food encoder 0x18EC0 + party write 0x019030 (selector 0xC9 A–C). No A4-$6760
 * gold deduct. Writes encoding to every party +$78; +$7C bit0 cleared (food).
 * Do NOT call from tavern C specialties (0x1CD2E) — separate OP_0E path. */
struct TownSvcFoodEncodeResult {
    bool ok = false;
    uint8_t encoding = 0;
    int members_written = 0;
};

TownSvcFoodEncodeResult townSvcFoodEncodePurchase(Mm2RosterFile *roster,
                                                  const Mm2PartyLaunch *launch, int menu /*0..2*/,
                                                  gameplay::Rng *rng);

/* Drink encoder 0x18F78 + 0x019030 with drink mode (+$7C bit0 set). Selector 0xCA.
 * Encoding only — base-stat bonuses are FAQ fiction; apply leaf is gold @ 0x18D3A. */
TownSvcFoodEncodeResult townSvcDrinkEncodePurchase(Mm2RosterFile *roster,
                                                   const Mm2PartyLaunch *launch, int drink /*0..5*/,
                                                   gameplay::Rng *rng);

/* Combat attack phase @ 0x10C66: if +$78 == monster type and +$7C bit0 (drink),
 * bset +$7C bit1 (armed for gold apply). */
void townSvcArmDrinkMatchOnKill(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                uint8_t monster_type);

/* Drink gold apply @ 0x18D3A: +$7C bit1 + matching +$78 → A4-$6AE0[tier] added
 * to experience +$62, then clr +$78 and clear bit1. Called from tavern check
 * 0x19516, not mid-combat. */
struct TownSvcDrinkApplyResult {
    bool applied = false;
    uint32_t gold = 0;
};

TownSvcDrinkApplyResult townSvcApplyDrinkEncoding(Mm2RosterRecord &rec);

/* Food EXP apply @ 0x18DE0: +$78 encoding as items.dat id; XP = gold(+$12)×8
 * via A4-$3EEC (= items base -$3EFE + $12). Clr +$78. Cond ≥$80 skips. */
struct TownSvcFoodApplyResult {
    bool applied = false;
    uint32_t xp = 0;
};

TownSvcFoodApplyResult townSvcApplyFoodEncoding(const Mm2ItemsFile *items, Mm2RosterRecord &rec);

/* Party backpack scan @ 0x18C74 / 0x18CE6: first slot whose +$3A == item_id.
 * Returns 1-based slot count (ASM addq #1), or 0 if not found. */
int townSvcPartyFindBackpackItem(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                 uint8_t item_id);

/* Consume first matching backpack id @ 0x18D06 → -$7F26 clear slot. */
bool townSvcPartyConsumeBackpackItem(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                     uint8_t item_id);

/* FAQ-era drink helper — no base-stat mutate. Prefer encode + apply leaves. */
struct TownSvcDrinkResult {
    bool ok = false;
    bool sick = false;
    const char *name = nullptr;
};

TownSvcDrinkResult townSvcPubDrink(Mm2RosterRecord &rec, int drink_idx, gameplay::Rng *rng);

/* Tip/rumor day-pair index @ 0x1C962: returns 0, 1, or 3 (then ×2 → tip pair
 * base into A4-$58AE / A4-$594E). Day word from -$79DE[era]. */
int townSvcTipDayPairBase(uint16_t day_of_year);

enum class TownSvcTipReject : uint8_t {
    None = 0,
    Condition,    /* 0x1CFD2: +$26 != 0 → caption 4 */
    NotEnoughGold, /* 0x1C9C0 fail → caption 2 */
    NoTip,        /* RNG miss @ 0x1D02A → caption 0 */
};

struct TownSvcTipResult {
    bool ok = false;
    TownSvcTipReject reject = TownSvcTipReject::None;
    int pair_base = 0; /* 0, 2, or 6 — index of first line of the tip pair */
};

/* Tavern D @ 0x1CFCA: cond==0, deduct 1gp, RNG(1, -$7F56(+$73)+5)==1, then
 * day-pair tips. `pair_base` indexes tips[pair_base] + tips[pair_base+1]. */
TownSvcTipResult townSvcTavernTip(Mm2RosterRecord &rec, uint16_t day_of_year,
                                  gameplay::Rng *rng);

/* Tavern E @ 0x1D0B4: cond==0 then day-pair rumors (no gold / no RNG). */
TownSvcTipResult townSvcTavernRumor(Mm2RosterRecord &rec, uint16_t day_of_year);

}  // namespace mm2::events
