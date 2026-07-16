#ifndef MM2_TOWN_TABLES_H
#define MM2_TOWN_TABLES_H

#include <stddef.h>
#include <stdint.h>

#include "mm2_roster_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MM2 per-town commerce constant tables (OP_0E town/building services).
 *
 * These five-entry tables are loaded into the A4 data block from the game's
 * data hunk and indexed by the current map screen id (A4-$79F2, 0..4 =
 * Middlegate, Atlantium, Tundara, Vulcania, Sandsobar). They are *constants*
 * (the engine never writes them at runtime), so the remake mirrors them as a
 * fixed C table verified against the 68k disassembly + EXTRACTED/docs/28/34.
 *
 * Confirmed ASM sources (EXTRACTED/mm2.capstone.annotated.asm):
 *   - A4-$6720  training stat add per map  @ training_stat_apply 0x1C898
 *   - A4-$671A  training stat cap per map  @ 0x1C8A8 (cmp before write)
 *   - A4-$6714  temple cost scale (BE u16) @ 0x1DC26 / 0x1DD2A / 0x1DC84
 *               donate = scale*100; heal/align multiply by scale (and level)
 *   - A4-$6742  tavern feeding-frenzy gold (BE u16) @ 0x1CA3A (NOT temple donate)
 *   - A4-$66B1  temple donation quest bit    @ 0x1D7B8 (OR into A4-$799E)
 *   - training "town index" multiplier (FAQ §3-6, doc 34 §13): map->[1,5,2,4,2]
 *     (Vulcania training=4 but temple scale A4-$6714=3 — do not conflate)
 *
 * Stat-id -> roster record byte offset, from the jump table @ 0x1C86C feeding
 * 0x1C898 (caller passes char ptr in $a(a5), map index in $8(a5)):
 *   id0 0x6B might_base   id1 0x6F accuracy_base  id2 0x6D personality_base
 *   id3 0x6C intelligence id4 0x71 level          id5 0x72 spell_level
 *   id6 0x6E speed_base
 */

enum {
    MM2_TOWN_COUNT = 5,
    MM2_TOWN_STAT_COUNT = 7
};

/* Map screen id 0..4 (== A4-$79F2). */
typedef enum Mm2TownId {
    MM2_TOWN_MIDDLEGATE = 0,
    MM2_TOWN_ATLANTIUM = 1,
    MM2_TOWN_TUNDARA = 2,
    MM2_TOWN_VULCANIA = 3,
    MM2_TOWN_SANDSOBAR = 4
} Mm2TownId;

typedef struct Mm2TownCommerce {
    uint8_t training_town_index; /* FAQ §3-6 training fee multiplier (NOT the map index) */
    uint8_t stat_train_add;      /* A4-$6720[map]: amount added to trained stat */
    uint8_t stat_train_cap;      /* A4-$671A[map]: max value the trained stat may reach */
    uint8_t temple_cost_index;   /* A4-$6714[map]: temple heal/align/donate scale */
    uint16_t feeding_frenzy_gold; /* A4-$6742[map]: tavern A feeding-frenzy debit */
    uint8_t donation_quest_bit;  /* A4-$66B1[map]: OR'd into A4-$799E on donation */
} Mm2TownCommerce;

/* All five towns are bit 0x1F when every temple has been donated to (reward gate
 * @ doc 28 §5.2: A4-$799E == 0x1F triggers the Nordon farthing reward). */
#define MM2_TEMPLE_DONATION_ALL_BITS 0x1F

/* Returns 1 and fills *out for a valid map id (0..4); else 0. */
int mm2_town_commerce(int map_id, Mm2TownCommerce *out);

/* Roster record byte offset for trainable stat id 0..6 (jump table @ 0x1C86C),
 * or -1 if out of range. */
int mm2_town_stat_field_offset(int stat_id);

/* Cost formulas. Training uses FAQ training_town_index; temple heal uses
 * A4-$6714 (temple_cost_index) — Vulcania differs (4 vs 3). */
uint32_t mm2_town_training_cost(int level, int town_index); /* level*index*50 */
uint32_t mm2_town_healing_cost(int level, int temple_cost_index); /* level*index*10 (healthy) */
uint32_t mm2_town_temple_donate_cost(int map_id); /* A4-$6714[map]*100 @ 0x1DC1A */

/* ------------------------------------------------------------------------- *
 * Training-hall LEVEL progression (OP_0E 0x04, handler -$7D16). The Training
 * Hall does NOT raise a stat: it levels the character up when they have enough
 * experience (record+0x62) for the next level AND can pay the fee
 * (level*index*50). There are TWO experience-to-level curves keyed on class
 * (doc 32 §"Experience to next level"):
 *   Group A (~25% less XP): Knight(0), Cleric(3), Robber(5), Barbarian(7).
 *   Group B:               Paladin(1), Archer(2), Sorcerer(4), Ninja(6).
 * class_id is the roster record byte +0x0F (doc 06: 0 Knight .. 7 Barbarian).
 *
 * NOTE (ASM grounding): the XP comparison long lives at A4-$6FC6 (party XP);
 * the per-class threshold array's data-hunk address is not yet pinned (doc 32),
 * so the thresholds below are transcribed from the documented FAQ tables (the
 * authoritative source the workspace tracks for this curve). */

/* 0 = Group A, 1 = Group B. Returns -1 for an out-of-range class id. */
int mm2_class_xp_group(uint8_t class_id);

/* Total experience required to BE `level` for this class (doc 32). level <= 1
 * returns 0; levels 2..77 return the table value; beyond 77 returns UINT32_MAX
 * (the +16384k cleric/sorcerer extension @ FAQ line 1957 is a documented gap). */
uint32_t mm2_class_xp_for_level(uint8_t class_id, int level);

/* Deterministic per-level HP/SP gain model (doc 32 "HP per level by class" +
 * "Attribute -> bonuses per level"). The original adds a per-level RANDOM roll
 * (HP path @ 0x9BCA); the exact RNG is a documented gap, so the remake applies
 * the documented average (class base HP + END bonus; caster SP = stat bonus+3).
 *   mm2_class_hp_per_level: A4-$64DA class base (training 0x2039C).
 *   mm2_train_hp_gain: full Training Hall HP leaf @ 0x20390.
 *   mm2_attr_bonus: FAQ/doc-32 column (not the -$7F56 table walk).
 *   mm2_class_caster_stat: 0 none, 1 INT (Sorcerer/Archer), 2 PER (Cleric/Paladin).
 *   mm2_class_spell_level_for: spell level reachable at `char_level`
 *     (pure casters 2X-1; fighter-mages 2X+5, natural cap 7). */
int mm2_class_hp_per_level(uint8_t class_id);
/* Training Hall HP gain @ 0x20390: ($64DA[class]*$64EE[map])/$64E4[map]
 * (+1 if rem!=0 and class not Cleric/Robber/Ninja) + -$7F56(+$27). */
int mm2_train_hp_gain(uint8_t class_id, int map_id, uint8_t endurance_current);
int mm2_attr_bonus(int attribute_value);
int mm2_class_caster_stat(uint8_t class_id);
int mm2_class_spell_level_for(uint8_t class_id, int char_level);

/* Training Hall spell leaf @ 0x20064 (called from level-up @ 0x204AC when class
 * is Cleric/Sorcerer/Archer/Paladin). Recomputes spell level from +$20/+class,
 * grants up to 4 auto-spells from A4-$64A2 (cleric/paladin) or A4-$64C2
 * (sorcerer/archer), and sets SP current/max = (-$7F56(INT|PER)+3)*new_sl.
 * Returns 1 when spell level increased (UI prints "and new spells"). */
int mm2_train_spell_on_levelup(Mm2RosterRecord *rec);

/* ------------------------------------------------------------------------- *
 * Blacksmith static inventories (OP_0E 0x06, handler 0x1C54A; inventory
 * builder 0x1C00E walking `town*6 + slot` into the data-hunk tables selected
 * by the category jump @ 0x1C09E). Six slots per town per category, indexed by
 * the map screen id A4-$79F2 (0..4). Decoded byte-exact from the data hunk
 * (EXTRACTED/ghidra/mm2_data_00.bin via tools/dump_shop_tables.py +
 * tools/mm2_town_tables.py) — see EXTRACTED/docs/28-town-services.md §4.1:
 *   Weapons  ids A4-$68EE  meta A4-$68D0
 *   Specials ids A4-$683A  (bonus is DATE-ROLLED @ 0x1C146, not static — see note)
 *   Armor    ids A4-$68B2  meta A4-$6894
 *   Misc     ids A4-$6876  meta A4-$6858
 */
typedef enum Mm2SmithCategory {
    MM2_SMITH_WEAPONS = 0,  /* menu key A, builder category 1 */
    MM2_SMITH_SPECIALS = 1, /* menu key B, builder category 2 (Today's Specials) */
    MM2_SMITH_ARMOR = 2,    /* menu key C, builder category 3 */
    MM2_SMITH_MISC = 3,     /* menu key D, builder category 4 */
    MM2_SMITH_CATEGORY_COUNT = 4
} Mm2SmithCategory;

enum { MM2_SMITH_SLOTS = 6 };

typedef struct Mm2SmithSlot {
    uint8_t item_id; /* items.dat id; 0 means the slot is not stocked. */
    uint8_t meta;    /* raw meta byte: weapons/armor = magic '+' bonus (-$68D0/$6894);
                      * misc = item charges (-$6858); specials = N/A (date-rolled). */
} Mm2SmithSlot;

/* Fill the 6 blacksmith shop slots for (map_id 0..4, category). Returns 1 on
 * success (valid map+category), else 0. */
int mm2_smith_inventory(int map_id, int category, Mm2SmithSlot out[MM2_SMITH_SLOTS]);

/* How a category's raw meta byte maps to the buy fields (0x1C00E / 0x1BE44):
 *   - Weapons/Armor: -$5A1E (flags/+bonus) = meta, -$5A18 (charges) = 0.
 *   - Misc: the 0x1C1CC swap moves meta -> -$5A18 (charges) and clears -$5A1E,
 *     so flags = 0 and the price meta is 0 (base gold, no '+' multiplier).
 *   - Specials: -$5A1E is overwritten by the date-roll @ 0x1C146 (deferred), so
 *     the static meta is unused; price_meta/flags/charges are 0 here. */
void mm2_smith_buy_fields(int category, uint8_t meta, uint8_t *out_price_meta,
                          uint8_t *out_charges, uint8_t *out_flags);

/* Blacksmith price (0x1BF16, buy categories): base_gold = items.dat gold field
 * (record offset 0x12). `meta` is the &0x3F bonus byte (0 for misc/specials per
 * mm2_smith_buy_fields). meta==0 -> price == base_gold; otherwise
 * price = base_gold*2 + (meta-1)*1000 (each magic '+' point). The Merchant-skill
 * half-price discount (engine thunk -$7F32 @ 0x1BFB4) is NOT applied here and is
 * a documented gap. */
uint32_t mm2_smith_price(uint32_t base_gold, uint8_t meta);

/* Sell proceeds (0x1BF16 category 5 @ 0x1BFCC): half the buy-style price computed
 * from items.dat gold + backpack flags (+$46 low 6 bits). Merchant skill (-$7F32)
 * may halve again — deferred. */
uint32_t mm2_smith_sell_price(uint32_t buy_style_price);

/* Identify fee (0x1BF16 category 6 @ 0x1BF48): flags & 0x3F == 0 -> 10 gp; else
 * meta*100 via mul -$7B54 (0x24C74). Not shop RNG. */
uint32_t mm2_smith_identify_cost(uint8_t backpack_flags);

/* ------------------------------------------------------------------------- *
 * Mage guild sorcerer-spell shop (OP_0E 0x05, handler A4 thunk -$7D10 ->
 * 0x1E3E6; stock loop 0x1E618..0x1E650; purchase keys A-D @ 0x1E864, grant
 * 0x1D872). 4 fixed slots/town; `spell_index` is the per-school flat index
 * 0..47 used by mm2/gameplay/SpellBook.h (spellKnownInBook/spellLearnInBook),
 * NOT the 0..95 spells.dat index. Decoded byte-exact from the data hunk via
 * tools/dump_shop_tables.py -> EXTRACTED/shop_tables.json
 * (static_by_town.mage_guild_spells) — see EXTRACTED/docs/28-town-services.md.
 *
 * MEMBERSHIP GATE (0x1E410, ASM-confirmed, doc 36): the shop only opens for a
 * party that has >=1 member with `(record+0x79 class_quest_guild_mask) &
 * kMageGuildMemberMask[map] != 0`. Two confirmed write paths for that byte:
 *  1. The generic event-script field-selector dispatcher (OP_15/18/1F/20,
 *     `game/include/mm2/events/EventFieldMap.h` selector 0x74 -> offset 0x79,
 *     ASM-derived from `event_member_field_ptr` 0x17766 + jump table 0x17FEA)
 *     — already ported; any event.dat script that sets this selector grants
 *     real membership through normal gameplay.
 *  2. The Mount-Farview/Arena class-quest reward loop (`or.b d1,$79(a0)` @
 *     0x9FE0 inside 0x9D76) — a routine with confirmed pointer/offset bugs
 *     (doc 36-class-quest-hp-bug.md) that is NOT yet ported.
 * No location script has been confirmed (this pass) to actually exercise
 * path 1 for these specific bits, so mm2_mage_guild_member() may still return
 * false for a roster that hasn't triggered such a script — an accurate
 * reflection of the current port, not an invented restriction. Do NOT gate on
 * the OP_0E 0x0D "enroll" write (record+0x0B town_flags) — that is a
 * different field (0x1A1CE) with no confirmed read site. */
enum { MM2_MAGE_GUILD_SLOTS = 4, MM2_TEMPLE_SPELL_SLOTS = 3 };

typedef struct Mm2SpellShopSlot {
    uint8_t spell_index; /* 0..47, per-school flat index (SpellBook.h). */
    uint32_t gold;       /* gp cost (static; ASM-confirmed byte-exact). */
} Mm2SpellShopSlot;

/* Fill the 4 mage-guild (sorcerer) slots for map_id 0..4. Returns 1 on success. */
int mm2_mage_guild_stock(int map_id, Mm2SpellShopSlot out[MM2_MAGE_GUILD_SLOTS]);

/* Fill the 3 temple (cleric) spell-purchase slots (menu D/E/F) for map_id 0..4.
 * Returns 1 on success. */
int mm2_temple_spell_stock(int map_id, Mm2SpellShopSlot out[MM2_TEMPLE_SPELL_SLOTS]);

/* Per-town class_quest_guild_mask bit (record+0x79), data hunk A4-$66A9,
 * byte-exact: {0x02, 0x04, 0x08, 0x10, 0x20}. 0 for an out-of-range map_id. */
uint8_t mm2_mage_guild_member_mask(int map_id);

#ifdef __cplusplus
}
#endif

#endif /* MM2_TOWN_TABLES_H */
