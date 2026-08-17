#include "mm2/gameplay/InGameCharacterSheet.h"

#include "mm2/CppStdCompat.h"

#include "mm2/gameplay/RosterSkills.h"
#include "mm2/gameplay/SpellBook.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PartyStatusFormat.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"
#include "mm2/ui/AguiPaperDollLayout.h"
#include "mm2/ui/RosterSkillDisplay.h"

#include <cstddef>
#include <cstdint>

namespace mm2::gameplay {

namespace {

using namespace mm2::ui::amiga_layout;
using namespace mm2::gfx::play_layout;

/* Food trade ($E3C6 @ 0xE444): a transfer is rejected if the target's food would
   exceed 0x28 (40), the per-character food maximum. */
constexpr int kSheetMaxFood = 0x28;

static const char *kClassNames[] = {
    "Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian",
};

static const char *kAlignHeaderNames[] = {"Good", "Neut", "Evil"};

const char *className(uint8_t id) { return id < 8 ? kClassNames[id] : "?"; }

const char *alignHeaderName(uint8_t id) { return id < 3 ? kAlignHeaderNames[id] : "?"; }

const char *raceHeaderName(uint8_t id)
{
    static const char *kRaceNames[] = {"Human", "Elf", "Dwarf", "Gnome", "Half-Orc"};
    return id < 5 ? kRaceNames[id] : "?";
}

const char *conditionName(uint8_t c)
{
    if (c >= 0x80) {
        return "Dead";
    }
    switch (c) {
    case 0:
        return "Good";
    case 1:
        return "Cursed";
    case 2:
    case 3:
        return "Silenced";
    default:
        return (c >= 4) ? "Poisoned" : "?";
    }
}

void drawCellText(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255, uint8_t g = 255,
                  uint8_t b = 255)
{
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

int overlayBottomRow() { return kPlayOverlayBorderRow + kPlayOverlayBorderH - 1; }

void drawBorderIntegratedText(gfx::ScreenCompositor &c, int row, int border_col, int border_w_cells,
                              const char *text, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255)
{
    const int len = static_cast<int>(std::strlen(text));
    if (len <= 0 || len > border_w_cells - 2) {
        return;
    }
    const int text_col = border_col + (border_w_cells - len) / 2;
    gfx::fillCellRect(c, text_col, row, len, 1);
    drawCellText(c, row, text_col, text, r, g, b);
}

void drawBorderIntegratedTextAt(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255,
                                uint8_t g = 255, uint8_t b = 255)
{
    const int len = static_cast<int>(std::strlen(text));
    if (len <= 0) {
        return;
    }
    gfx::fillCellRect(c, col, row, len, 1);
    drawCellText(c, row, col, text, r, g, b);
}

void copyRosterNameRaw(const Mm2RosterRecord &rec, char *out, size_t cap)
{
    if (!out || cap == 0) {
        return;
    }
    size_t n = 0;
    while (n + 1 < cap && n < static_cast<size_t>(MM2_ROSTER_NAME_SIZE) && rec.name[n] != '\0') {
        out[n] = rec.name[n];
        ++n;
    }
    out[n] = '\0';
}

/** print_number @ 0x22480 with width 1 (sheet / Quick Ref field printer). */
void drawPrintNumber(gfx::ScreenCompositor &c, int row, int col, uint32_t value, uint8_t r = 255, uint8_t g = 255,
                     uint8_t b = 255)
{
    char buf[16];
    gfx::formatPrintNumber(value, buf, sizeof(buf), 1);
    drawCellText(c, row, col, buf, r, g, b);
}

void drawLabeledNumber(gfx::ScreenCompositor &c, int row, int col, const char *label, uint32_t value)
{
    drawCellText(c, row, col, label);
    drawPrintNumber(c, row, col + static_cast<int>(std::strlen(label)), value);
}

int rosterIndexForPartySlot(const Mm2PartyLaunch &launch, int party_slot)
{
    if (party_slot < 0 || party_slot >= launch.party_count || party_slot >= MM2_PARTY_LAUNCH_SLOTS) {
        return -1;
    }
    return launch.roster_slots[party_slot];
}

void itemLabel(char *out, size_t cap, const Mm2ItemsFile *items, uint8_t item_id)
{
    if (!out || cap == 0) {
        return;
    }
    if (item_id == 0 || !items) {
        out[0] = '\0';
        return;
    }
    const Mm2ItemRecord *rec = mm2_items_lookup(items, item_id);
    if (rec) {
        mm2_item_name_to_cstr(rec, out, cap);
    } else {
        std::snprintf(out, cap, "#%u", item_id);
    }
}

void itemCaption(char *out, size_t cap, char prefix, const Mm2ItemsFile *items, uint8_t item_id, uint8_t flags)
{
    if (!out || cap == 0) {
        return;
    }
    char name[16];
    itemLabel(name, sizeof(name), items, item_id);
    const int plus = static_cast<int>(flags & 0x3F);
    if (plus && name[0]) {
        std::snprintf(out, cap, "%c) %s +%d", prefix, name, plus);
    } else if (name[0]) {
        std::snprintf(out, cap, "%c) %s", prefix, name);
    } else {
        out[0] = '\0';
    }
}

int firstEmptyEquip(const Mm2RosterRecord &rec)
{
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.equipped_id[i] == 0) {
            return i;
        }
    }
    return -1;
}

int firstEmptyBackpack(const Mm2RosterRecord &rec)
{
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.backpack_id[i] == 0) {
            return i;
        }
    }
    return -1;
}

uint8_t *recByte(Mm2RosterRecord &rec, int offset)
{
    return reinterpret_cast<uint8_t *>(&rec) + offset;
}

void satAddByte(uint8_t *field, uint8_t amount)
{
    if (!field) {
        return;
    }
    const int sum = static_cast<int>(*field) + static_cast<int>(amount);
    *field = static_cast<uint8_t>(sum > 0xFF ? 0xFF : sum);
}

void satSubByte(uint8_t *field, uint8_t amount)
{
    if (!field) {
        return;
    }
    if (*field < amount) {
        *field = 0;
    } else {
        *field = static_cast<uint8_t>(*field - amount);
    }
}

/* -$7F56 / 0x4442 threshold walk (same table as Rest SP / training). */
uint8_t statBonus7f56(uint8_t attr)
{
    static const uint8_t kThresh[] = {4,  6,  9,  13, 15, 17, 19, 22, 26, 30, 45,
                                      60, 75, 90, 105, 120, 135, 150, 175, 200, 225, 250, 255};
    uint8_t bonus = 0xFD; /* −3 */
    for (size_t i = 0; i < sizeof(kThresh); ++i) {
        if (attr <= kThresh[i]) {
            break;
        }
        ++bonus;
    }
    return bonus;
}

bool itemIsOneHandedMelee(uint8_t id)
{
    return id >= 0x01 && id <= 0x41; /* 0xF5F4 */
}

bool itemIsTwoHandedMelee(uint8_t id)
{
    return id >= 0x42 && id <= 0x5B; /* 0xF612 */
}

bool itemIsMeleeWeapon(uint8_t id)
{
    /* 0xF6B4 = 0xF5F4 | 0xF612. */
    return itemIsOneHandedMelee(id) || itemIsTwoHandedMelee(id);
}

bool itemIsMissileWeapon(uint8_t id)
{
    return id >= 0x5C && id <= 0x72; /* 0xF630 (includes keys 0x6F..0x72) */
}

bool itemIsShield(uint8_t id)
{
    return id >= 0x73 && id <= 0x7E; /* 0xF64E */
}

bool itemIsBodyArmor(uint8_t id)
{
    return id >= 0x7F && id <= 0x9A; /* 0xF66C */
}

bool itemIsHelm(uint8_t id)
{
    return id >= 0x9B && id <= 0x9F; /* 0xF68E */
}

bool itemAddsArmorClass(uint8_t id)
{
    return itemIsShield(id) || itemIsBodyArmor(id) || itemIsHelm(id);
}

bool equippedHas(const Mm2RosterRecord &rec, bool (*pred)(uint8_t))
{
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (pred(rec.equipped_id[i])) {
            return true;
        }
    }
    return false;
}

/* 0xEC02 occupancy / 2H-shield gate. Strings @ 0xE158..0xE1E8 via 0xE238. */
const char *equipOccupancyError(const Mm2RosterRecord &rec, uint8_t id)
{
    if (itemIsOneHandedMelee(id)) {
        if (equippedHas(rec, itemIsMeleeWeapon)) {
            return "Already have weapon";
        }
        return nullptr;
    }
    if (itemIsTwoHandedMelee(id)) {
        if (equippedHas(rec, itemIsMeleeWeapon)) {
            return "Already have weapon";
        }
        if (equippedHas(rec, itemIsShield)) {
            return "Not with shield";
        }
        return nullptr;
    }
    if (itemIsMissileWeapon(id)) {
        if (equippedHas(rec, itemIsMissileWeapon)) {
            return "Already have missile weapon";
        }
        return nullptr;
    }
    if (itemIsShield(id)) {
        if (equippedHas(rec, itemIsShield)) {
            return "Already have shield";
        }
        if (equippedHas(rec, itemIsTwoHandedMelee)) {
            return "Not with 2 handed weapon";
        }
        return nullptr;
    }
    if (itemIsBodyArmor(id)) {
        if (equippedHas(rec, itemIsBodyArmor)) {
            return "Already wearing armor";
        }
        return nullptr;
    }
    if (itemIsHelm(id)) {
        if (equippedHas(rec, itemIsHelm)) {
            return "Already wearing helm";
        }
        return nullptr;
    }
    return nullptr;
}

/* 0xF1C0 (add) / 0xF110 (sub): items.dat byte 0x0E special power.
 * Type nibble indexes rec+$10; types 0..5 also hit rec+$6B (base stats).
 * Amount nibble 0 is a no-op; otherwise amount += flags&0x3F. */
void applyItemSpecialPower(Mm2RosterRecord &rec, uint8_t item_id, uint8_t flags_lo, bool adding,
                           const Mm2ItemsFile *items)
{
    const uint8_t packed = items->records[item_id].bonus_byte;
    uint8_t amount = static_cast<uint8_t>(packed & 0x0F);
    if (amount == 0) {
        return;
    }
    const uint8_t type = static_cast<uint8_t>(packed >> 4);
    if (type == 14) {
        /* Type 14 (Thievery) lands on +$1E. Create/training persist the skill
         * there; equipped bonuses are added by rosterLiveThievery instead of
         * mutating +$1E, so Remove cannot subtract gear the load path never
         * added. */
        return;
    }
    amount = static_cast<uint8_t>(amount + flags_lo);
    uint8_t *cur = recByte(rec, 0x10 + static_cast<int>(type));
    uint8_t *base = (type <= 5) ? recByte(rec, 0x6B + static_cast<int>(type)) : nullptr;
    if (adding) {
        satAddByte(cur, amount);
        satAddByte(base, amount);
    } else {
        satSubByte(cur, amount);
        satSubByte(base, amount);
    }
}

/* 0xF36C / 0xF270 armor band: add/sub flags&0x3F then items.dat byte 0x10 to +$1F. */
void applyArmorAcAccumulator(Mm2RosterRecord &rec, uint8_t item_id, uint8_t flags_lo, bool adding,
                             const Mm2ItemsFile *items)
{
    if (!itemAddsArmorClass(item_id)) {
        return;
    }
    uint8_t *acc = recByte(rec, 0x1F);
    const uint8_t die = items->records[item_id].damage;
    if (adding) {
        satAddByte(acc, flags_lo);
        satAddByte(acc, die);
    } else {
        satSubByte(acc, flags_lo);
        satSubByte(acc, die);
    }
}

/* 0x67E6 per member: displayed AC +$24 = -$7F56(+$13) + equipment AC +$1F. */
void syncDisplayedArmorClass(Mm2RosterRecord &rec)
{
    uint8_t spd = statBonus7f56(rec.speed_current);
    if (spd >= 0xF0) {
        spd = 0;
    }
    const int sum = static_cast<int>(spd) + static_cast<int>(*recByte(rec, 0x1F));
    if (sum > 0xFF) {
        rec.armor_class = 0xFF;
    } else if (sum <= 0) {
        rec.armor_class = 0;
    } else {
        rec.armor_class = static_cast<uint8_t>(sum);
    }
}

/* Rebuild the equip-derived weapon combat fields the combat code reads for
 * player melee/missile damage: roster +$4C..+$4F (aliased as spells[0..3]).
 *   +$4C melee die   / +$4D melee bonus   ← item id 0x01..0x5B  (0xF6B4 band)
 *   +$4E missile die / +$4F missile bonus ← item id 0x5C..0x72  (0xF630 band)
 * Die = items.dat byte 0x10 (raw); bonus = equipped_flags & 0x3F (per-instance,
 * 0xF36C reads $34(a0)&0x3F). Shield/armor/helm ids 0x73..0x9F drive AC (+$1F).
 *
 * The retail engine mutates these per-slot on each equip/unequip (0xF36C set /
 * 0xF270 clear). We instead rebuild from all six equipped slots so combat always
 * reflects current gear regardless of the mutation path; net result matches for
 * the normal one-weapon loadout. Combat previously never ran this rebuild, so a
 * weapon equipped in-game had no effect on damage. */
void recomputeWeaponFields(Mm2RosterRecord &rec, const Mm2ItemsFile *items)
{
    if (!items) {
        return;
    }
    rec.spells[0] = 0;
    rec.spells[1] = 0;
    rec.spells[2] = 0;
    rec.spells[3] = 0;
    for (int slot = 0; slot < MM2_ROSTER_ITEM_SLOTS; ++slot) {
        const uint8_t id = rec.equipped_id[slot];
        if (itemIsMeleeWeapon(id)) {
            rec.spells[0] = items->records[id].damage;
            rec.spells[1] = static_cast<uint8_t>(rec.equipped_flags[slot] & 0x3F);
        } else if (itemIsMissileWeapon(id)) {
            rec.spells[2] = items->records[id].damage;
            rec.spells[3] = static_cast<uint8_t>(rec.equipped_flags[slot] & 0x3F);
        }
    }
}

/* 0xF36C after the backpack→equip copy: armor AC, special power, weapons, +$24. */
void applyEquippedSlotEffects(Mm2RosterRecord &rec, int slot, const Mm2ItemsFile *items)
{
    if (!items || slot < 0 || slot >= MM2_ROSTER_ITEM_SLOTS) {
        return;
    }
    const uint8_t id = rec.equipped_id[slot];
    if (id == 0) {
        recomputeWeaponFields(rec, items);
        syncDisplayedArmorClass(rec);
        return;
    }
    const uint8_t flags_lo = static_cast<uint8_t>(rec.equipped_flags[slot] & 0x3F);
    applyArmorAcAccumulator(rec, id, flags_lo, true, items);
    applyItemSpecialPower(rec, id, flags_lo, true, items);
    recomputeWeaponFields(rec, items);
    syncDisplayedArmorClass(rec);
}

/* 0xF270 before the equip→backpack/drop copy. Slot must still hold the item.
 * Weapons are rebuilt by the caller after the slot is emptied. */
void reverseEquippedSlotEffects(Mm2RosterRecord &rec, int slot, const Mm2ItemsFile *items)
{
    if (!items || slot < 0 || slot >= MM2_ROSTER_ITEM_SLOTS) {
        return;
    }
    const uint8_t id = rec.equipped_id[slot];
    if (id == 0) {
        return;
    }
    const uint8_t flags_lo = static_cast<uint8_t>(rec.equipped_flags[slot] & 0x3F);
    applyArmorAcAccumulator(rec, id, flags_lo, false, items);
    applyItemSpecialPower(rec, id, flags_lo, false, items);
    syncDisplayedArmorClass(rec);
}

void setStatus(SheetSession &session, const char *msg)
{
    if (!msg) {
        session.status_line[0] = '\0';
        return;
    }
    std::snprintf(session.status_line, sizeof(session.status_line), "%s", msg);
}

Mm2RosterRecord *rosterMut(Mm2RosterFile &roster, const Mm2PartyLaunch &launch, int party_slot)
{
    const int idx = rosterIndexForPartySlot(launch, party_slot);
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
        return nullptr;
    }
    return &roster.records[idx];
}

void drawModalEscFooter(gfx::ScreenCompositor &c)
{
    drawBorderIntegratedText(c, overlayBottomRow(), kPlayOverlayBorderCol, kPlayOverlayBorderW,
                             "( 'ESC' to go back )", 180, 180, 180);
}

void drawSheetEscFooter(gfx::ScreenCompositor &c)
{
    /* $6DA6 prints at ($B,$17); outer-frame bottom row is $17. */
    drawModalEscFooter(c);
}

}  // namespace

bool InGameCharacterSheet::loadAssets(const char *data_dir)
{
    /* Sheet backdrop is -$7F7A outer frame only (no book.32). Agui atlas is
     * optional — paper-doll falls back to the text lists if it is missing. */
    (void)atlas_.load(data_dir);
    return true;
}

void InGameCharacterSheet::blitItemIcon(gfx::ScreenCompositor &c, uint8_t item_id, int x, int y,
                                        bool highlight) const
{
    using namespace mm2::ui::agui_doll;
    atlas_.blitNamed(c, "doll/slot", x, y);
    if (item_id != 0) {
        char name[16];
        std::snprintf(name, sizeof(name), "items/i%02x", item_id);
        atlas_.blitNamed(c, name, x, y);
    }
    if (highlight) {
        c.drawBoxBorder(x - 1, y - 1, kIcon + 2, kIcon + 2, 224, 224, 192, 255);
    }
}

void InGameCharacterSheet::renderPaperDoll(gfx::ScreenCompositor &c, const Mm2RosterRecord &rec,
                                           const Mm2ItemsFile *items, const SheetSession *session) const
{
    using namespace mm2::ui::agui_doll;
    const SheetSubMode sub = session ? session->sub_mode : SheetSubMode::Normal;
    const bool hi_pack = sub == SheetSubMode::EquipPickBackpack || sub == SheetSubMode::DropPickSlot ||
                         sub == SheetSubMode::UsePick || sub == SheetSubMode::TradePickItemSlot;
    const bool hi_eq = sub == SheetSubMode::RemovePickEquip || sub == SheetSubMode::DropPickSlot ||
                       sub == SheetSubMode::UsePick;

    drawCellText(c, kSheetDividerRow, kSheetEquipCol, "Equipped", 200, 200, 200);
    drawCellText(c, kSheetDividerRow, kSheetBackpackCol, "Backpack", 200, 200, 200);
    (void)items;

    atlas_.blitNamed(c, "doll/body", kBodyX, kBodyY);

    const EquipView view = assignEquip(rec.equipped_id);
    for (int i = 0; i < 5; ++i) {
        const int x = kBodySlots[i].x;
        const int y = kBodySlots[i].y;
        const uint8_t id = view.body_id[i];
        blitItemIcon(c, id, x, y, hi_eq && id != 0);
        if (view.body_slot[i] != 0xFF) {
            char d[2] = {static_cast<char>('1' + view.body_slot[i]), '\0'};
            c.drawText(x - 8, y + 2, d, 224, 224, 192, 255);
        }
    }
    for (int i = 0; i < view.trinket_count; ++i) {
        const int x = kTrinketX0 + i * kTrinketStep;
        const int y = kTrinketY;
        blitItemIcon(c, view.trinket_id[i], x, y, hi_eq);
        if (view.trinket_slot[i] != 0xFF) {
            char d[2] = {static_cast<char>('1' + view.trinket_slot[i]), '\0'};
            c.drawText(x - 8, y + 2, d, 224, 224, 192, 255);
        }
    }

    static const char kPackLetters[] = "ABCDEF";
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        const SlotPx p = packSlot(i);
        const uint8_t id = rec.backpack_id[i];
        char lab[3] = {kPackLetters[i], ')', '\0'};
        c.drawText(kPackLabelX + (i % 2) * kPackColW, p.y + 2, lab, 200, 200, 200, 255);
        blitItemIcon(c, id, p.x, p.y, hi_pack);
    }
}

void InGameCharacterSheet::renderSheet(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                                       const Mm2PartyLaunch &launch, int party_slot, const Mm2ItemsFile *items,
                                       const SheetSession *session, bool combat_mode) const
{
    const int roster_idx = rosterIndexForPartySlot(launch, party_slot);
    if (roster_idx < 0 || roster_idx >= MM2_ROSTER_RECORD_COUNT) {
        drawCellText(c, 2, 2, "No character selected.");
        return;
    }

    const Mm2RosterRecord &rec = roster.records[roster_idx];
    const char disp_char = static_cast<char>('1' + party_slot);
    const SheetSubMode sub = session ? session->sub_mode : SheetSubMode::Normal;

    gfx::drawPlayModalBackdrop(c);

    /* Header @ $39D4: Locate(1,1) slot+") "+name+sex+align; then Locate(1,$16)
     * space + race + space + class (+ optional '+'). Row 1 is inside the outer
     * frame (top border is row 0 via -$7F7A @ 0x422A). */
    char name[16];
    copyRosterNameRaw(rec, name, sizeof(name));
    char left[48];
    std::snprintf(left, sizeof(left), "%c) %s%s%s", disp_char, name, rec.sex ? ": F " : ": M ",
                  alignHeaderName(rec.alignment_base));
    char right[40];
    std::snprintf(right, sizeof(right), " %s %s%s", raceHeaderName(rec.race), className(rec.class_id),
                  (rec.class_quest_guild_mask & 0x80) ? "+" : "");
    const int header_end = kSheetHeaderRaceCol + static_cast<int>(std::strlen(right));
    gfx::fillCellRect(c, kSheetHeaderCol, kSheetHeaderRow, header_end - kSheetHeaderCol, 1);
    drawCellText(c, kSheetHeaderRow, kSheetHeaderCol, left);
    drawCellText(c, kSheetHeaderRow, kSheetHeaderRaceCol, right);

    const uint8_t packed = rosterSkillPackedByte(rec);

    /* LAB_38EA field table — width-1 numbers, no slash-column padding. */
    drawLabeledNumber(c, 3, kSheetStatColLeft, "Lvl=", rec.level);
    drawLabeledNumber(c, 3, kSheetStatColMid, "HP=", rec.hp_max);
    drawLabeledNumber(c, 3, kSheetStatColSlash, "/", rec.hp_current);
    drawLabeledNumber(c, 3, kSheetStatColRight, "Age=", rec.age);

    drawLabeledNumber(c, 4, kSheetStatColLeft, "Mgt=", rec.might_base);
    drawLabeledNumber(c, 4, kSheetStatColMid, "SP=", rec.sp_current);
    drawLabeledNumber(c, 4, kSheetStatColSlash, "/", rec.sp_max);
    drawLabeledNumber(c, 4, kSheetStatColRight, "Exp=", rec.experience);

    drawLabeledNumber(c, 5, kSheetStatColLeft, "Int=", rec.intelligence_base);

    drawLabeledNumber(c, 6, kSheetStatColLeft, "Per=", rec.personality_base);
    drawLabeledNumber(c, 6, kSheetStatColMid, "AC=", rec.armor_class);
    drawLabeledNumber(c, 6, kSheetStatColSlash, "SL=", rec.spell_level);
    drawLabeledNumber(c, 6, kSheetStatColCost, "Gold=", rec.gold);

    drawLabeledNumber(c, 7, kSheetStatColLeft, "End=", rec.endurance_base);
    drawLabeledNumber(c, 7, kSheetStatColCost, "Gems=", rec.gems);

    drawLabeledNumber(c, 8, kSheetStatColLeft, "Spd=", rec.speed_base);
    {
        char th[24];
        std::snprintf(th, sizeof(th), "Thievery %u%%", mm2::ui::rosterDisplayThievery(rec, items));
        drawCellText(c, 8, kSheetStatColMid, th);
    }
    drawLabeledNumber(c, 8, kSheetStatColCost, "Food=", rec.food);

    drawLabeledNumber(c, 9, kSheetStatColLeft, "Acy=", rec.accuracy_base);
    drawCellText(c, 9, kSheetStatColMid, mm2::ui::rosterSheetSkillName(static_cast<uint8_t>(packed & 0x0F)));

    drawLabeledNumber(c, 10, kSheetStatColLeft, "Lck=", rec.luck_base);
    drawCellText(c, 10, kSheetStatColMid, mm2::ui::rosterSheetSkillName(static_cast<uint8_t>((packed >> 4) & 0x0F)));
    char cond[24];
    std::snprintf(cond, sizeof(cond), "Cond= %s", conditionName(rec.condition));
    drawCellText(c, 10, kSheetStatColCond, cond);

    if (paper_doll_ && atlas_.ready()) {
        renderPaperDoll(c, rec, items, session);
    } else {
        drawCellText(c, kSheetDividerRow, kSheetEquipCol, "-----(Equipped)-------(Backpack)-----", 200, 200, 200);

        static const char kPackLetters[] = "ABCDEF";
        for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
            const int row = kSheetEquipRowBase + i;
            char eline[24];
            if (rec.equipped_id[i]) {
                itemCaption(eline, sizeof(eline), static_cast<char>('1' + i), items, rec.equipped_id[i],
                            rec.equipped_flags[i]);
            } else {
                std::snprintf(eline, sizeof(eline), "%d)", i + 1);
            }
            drawCellText(c, row, kSheetEquipCol, eline, 220, 220, 220);

            if (rec.backpack_id[i]) {
                itemCaption(eline, sizeof(eline), kPackLetters[i], items, rec.backpack_id[i], rec.backpack_flags[i]);
            } else {
                std::snprintf(eline, sizeof(eline), "%c)", kPackLetters[i]);
            }
            drawCellText(c, row, kSheetBackpackCol, eline, 220, 220, 220);
        }
    }

    gfx::fillCellRect(c, kSheetFooterCol, kSheetFooterRow1 - 1, kPlayOverlayBorderW - 2, 4);

    const bool aux_pending = session && session->cast_aux_pending;
    const bool hide_commands =
        sub == SheetSubMode::SpellBook || sub == SheetSubMode::CastPicker || aux_pending;
    if (session && session->status_line[0] && sub != SheetSubMode::SpellBook &&
        sub != SheetSubMode::CastPicker) {
        /* 0xCCE8 locate(col $16, row $15) "'Return' to cast"; other aux on the footer. */
        if (std::strcmp(session->status_line, "'Return' to cast") == 0) {
            drawCellText(c, 0x15, 0x16, session->status_line, 255, 255, 128);
        } else {
            drawCellText(c, kSheetFooterRow1 - 1, kSheetFooterCol, session->status_line, 255, 255, 128);
        }
    } else if (paper_doll_ && atlas_.ready() && sub != SheetSubMode::SpellBook &&
               sub != SheetSubMode::CastPicker) {
        char cap[48];
        cap[0] = '\0';
        static const char kPackLetters[] = "ABCDEF";
        for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS && !cap[0]; ++i) {
            if (rec.equipped_id[i]) {
                itemCaption(cap, sizeof(cap), static_cast<char>('1' + i), items, rec.equipped_id[i],
                            rec.equipped_flags[i]);
            }
        }
        for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS && !cap[0]; ++i) {
            if (rec.backpack_id[i]) {
                itemCaption(cap, sizeof(cap), kPackLetters[i], items, rec.backpack_id[i], rec.backpack_flags[i]);
            }
        }
        if (cap[0]) {
            drawCellText(c, kSheetFooterRow1 - 1, kSheetFooterCol, cap, 255, 255, 128);
        }
    }
    if (!hide_commands) {
        if (combat_mode) {
            drawCellText(c, kSheetFooterRow1, kSheetFooterCol, "'V' View spell book", 180, 180, 180);
        } else {
            drawCellText(c, kSheetFooterRow1, kSheetFooterCol, "'Q' Quick Ref  'C' Cast    'D' Drop   ", 180, 180,
                         180);
            drawCellText(c, kSheetFooterRow2, kSheetFooterCol, "'E' Equip      'G' Gather  'R' Remove ", 180, 180,
                         180);
            drawCellText(c, kSheetFooterCmdRow3, kSheetFooterCol, "'S' Share      'T' Trade   'U' Use    ", 180, 180,
                         180);
        }
    }

    drawSheetEscFooter(c);

    if (sub == SheetSubMode::SpellBook) {
        renderSpellBook(c, roster, launch, party_slot);
    } else if (sub == SheetSubMode::CastPicker) {
        renderCastPicker(c, roster, launch, party_slot, *session);
    }
}

void InGameCharacterSheet::renderQuickRef(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                                          const Mm2PartyLaunch &launch) const
{
    gfx::drawPlayModalBackdrop(c);

    drawCellText(c, kQuickRefHeaderRow1, kQuickRefColIndex, "#     Name    Hit Points  Spell Points", 255, 255, 128);
    drawCellText(c, kQuickRefHeaderRow2, kQuickRefColIndex, "# Lvl SL AC Age Gems  Food Condition  ", 255, 255, 128);

    for (int i = 0; i < launch.party_count && i < 8; ++i) {
        const int roster_idx = rosterIndexForPartySlot(launch, i);
        if (roster_idx < 0) {
            continue;
        }
        const Mm2RosterRecord &rec = roster.records[roster_idx];
        char name[16];
        copyRosterNameRaw(rec, name, sizeof(name));

        const int row1 = kQuickRefDataRow1Base + i;
        char prefix[20];
        std::snprintf(prefix, sizeof(prefix), "%d) %s ", i + 1, name);
        drawCellText(c, row1, kQuickRefColIndex, prefix);
        /* hp_max = live +$5E, hp_current = +$74 ceiling (codec names inverted). */
        drawPrintNumber(c, row1, kQuickRefColIndex + static_cast<int>(std::strlen(prefix)), rec.hp_max);
        drawLabeledNumber(c, row1, kQuickRefColHpSlash, "/", rec.hp_current);
        drawPrintNumber(c, row1, kQuickRefColSpCurrent, rec.sp_current);
        drawLabeledNumber(c, row1, kQuickRefColSpSlash, "/", rec.sp_max);

        const int row2 = kQuickRefDataRow2Base + i;
        std::snprintf(prefix, sizeof(prefix), "%d) ", i + 1);
        drawCellText(c, row2, kQuickRefColIndex, prefix, 200, 200, 200);
        drawPrintNumber(c, row2, kQuickRefColIndex + static_cast<int>(std::strlen(prefix)), rec.level, 200, 200, 200);
        drawPrintNumber(c, row2, kQuickRefColSL, rec.spell_level, 200, 200, 200);
        drawPrintNumber(c, row2, kQuickRefColAC, rec.armor_class, 200, 200, 200);
        drawPrintNumber(c, row2, kQuickRefColAge, rec.age, 200, 200, 200);
        drawPrintNumber(c, row2, kQuickRefColGems, rec.gems, 200, 200, 200);
        drawPrintNumber(c, row2, kQuickRefColFood, rec.food, 200, 200, 200);
        drawCellText(c, row2, kQuickRefColCond, conditionName(rec.condition), 200, 200, 200);
    }

    drawModalEscFooter(c);
}

void InGameCharacterSheet::renderSpellBook(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                                           const Mm2PartyLaunch &launch, int party_slot) const
{
    const int roster_idx = rosterIndexForPartySlot(launch, party_slot);
    if (roster_idx < 0 || roster_idx >= MM2_ROSTER_RECORD_COUNT) {
        return;
    }

    const Mm2RosterRecord &rec = roster.records[roster_idx];
    const SpellSchool school = spellSchoolForClass(rec.class_id);
    if (school == SpellSchool::None) {
        return;
    }

    /* Spell-book popup @ 0x6732 / cast @ 0x6E08: -$7C74 is win_define(x1,y1,x2,y2)
       = ($A,7,$1D,$13) → cells (10,7)-(29,19), 20×13. Grid @ 0x65FA is window-
       relative: title (5,1), header (1,2), level digit col 2 / row level+2,
       marks row index+3 col slot*2+5, glyph $17. */
    constexpr int kWinX1 = 0x0A;
    constexpr int kWinY1 = 7;
    constexpr int kWinX2 = 0x1D;
    constexpr int kWinY2 = 0x13;
    constexpr int kWinW = kWinX2 - kWinX1 + 1;
    constexpr int kWinH = kWinY2 - kWinY1 + 1;
    constexpr int kTitleRow = kWinY1 + 1;
    constexpr int kHeaderRow = kWinY1 + 2;
    constexpr int kGridRowBase = kWinY1 + 3;
    constexpr int kTitleCol = kWinX1 + 5;
    constexpr int kHeaderCol = kWinX1 + 1;
    constexpr int kDigitCol = kWinX1 + 2;
    constexpr int kMarkColBase = kWinX1 + 5;
    constexpr uint8_t kKnownMark = 0x17;

    c.fillRect(kWinX1 * 8, kWinY1 * 8, kWinW * 8, kWinH * 8, 0, 0, 128, 255);
    c.drawConsoleBox(kWinY1, kWinX1, kWinW, kWinH, 255, 255, 0);

    drawCellText(c, kTitleRow, kTitleCol, "Spell Book", 255, 255, 128);
    drawCellText(c, kHeaderRow, kHeaderCol, "Lvl 1 2 3 4 5 6 7", 255, 255, 255);

    int flat = 0;
    for (int level = 1; level <= kSpellLevels; ++level) {
        const int row = kGridRowBase + (level - 1);
        char lvl[4];
        std::snprintf(lvl, sizeof(lvl), "%d", level);
        drawCellText(c, row, kDigitCol, lvl, 255, 255, 255);

        const int slots = kSpellsPerLevel[level - 1];
        for (int slot = 0; slot < slots; ++slot) {
            if (spellKnownInBook(rec, flat)) {
                const int col = kMarkColBase + slot * 2;
                c.drawGlyph(cellX(col), cellY(row), kKnownMark, 255, 255, 255, 255);
            }
            ++flat;
        }
    }
}

void InGameCharacterSheet::renderCastPicker(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                                            const Mm2PartyLaunch &launch, int party_slot,
                                            const SheetSession &session) const
{
    /* Exploration cast @ 0x6E30: LAB_6622 grid then -$7E12 / 0x79EE prompts. */
    renderSpellBook(c, roster, launch, party_slot);

    /* Prompt is on the full-screen window (0x6E68 swaps -$6198 before -$7E12).
       Explore row $15 / combat $0F (0x79CA/0x79DC); Number at row+1 col $C. */
    constexpr int kPromptRow = 0x15;
    gfx::fillCellRect(c, 1, kPromptRow, 38, 2);
    drawCellText(c, kPromptRow, 0x02, " Spell Level: ", 255, 255, 255);
    if (session.cast_phase == CastPromptPhase::Number) {
        char digit[4];
        std::snprintf(digit, sizeof(digit), "%d", session.cast_level);
        drawCellText(c, kPromptRow, 0x02 + 14, digit, 255, 255, 255);
        drawCellText(c, kPromptRow + 1, 0x0C, "Number: ", 255, 255, 255);
    }
}

SheetKeyOutcome InGameCharacterSheet::handleKey(char key, SheetSession &session, Mm2RosterFile &roster,
                                                const Mm2PartyLaunch &launch, const Mm2ItemsFile *items,
                                                bool combat_mode)
{
    Mm2RosterRecord *rec = rosterMut(roster, launch, session.party_slot);
    if (!rec) {
        return SheetKeyOutcome::Close;
    }

    if (session.sub_mode == SheetSubMode::EquipPickBackpack) {
        session.sub_mode = SheetSubMode::Normal;
        if (key >= 'A' && key <= 'F') {
            const int bp = key - 'A';
            const Mm2RosterItemSlot src = mm2_roster_backpack(rec, bp);
            if (src.item_id == 0) {
                setStatus(session, "Empty slot.");
                return SheetKeyOutcome::None;
            }
            const Mm2ItemRecord *item = items ? mm2_items_lookup(items, src.item_id) : nullptr;
            if (item && !mm2_item_class_can_use(item, rec->class_id)) {
                setStatus(session, "Wrong class");
                return SheetKeyOutcome::None;
            }
            if (const char *occ = equipOccupancyError(*rec, src.item_id)) {
                setStatus(session, occ);
                return SheetKeyOutcome::None;
            }
            const int eq = firstEmptyEquip(*rec);
            if (eq < 0) {
                setStatus(session, "No empty equip slot.");
                return SheetKeyOutcome::None;
            }
            mm2_roster_set_equipped(rec, eq, src);
            mm2_roster_set_backpack(rec, bp, Mm2RosterItemSlot{});
            applyEquippedSlotEffects(*rec, eq, items);
            setStatus(session, "Equipped.");
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    /* Use @ 0xE94A: '1'..'6' equip / 'A'..'F' backpack → GameSession applyItemUse. */
    if (session.sub_mode == SheetSubMode::UsePick) {
        session.sub_mode = SheetSubMode::Normal;
        if (key >= '1' && key <= '6') {
            session.pending_use_slot = key - '1';
            setStatus(session, "Using...");
            return SheetKeyOutcome::None;
        }
        if (key >= 'A' && key <= 'F') {
            session.pending_use_slot = 6 + (key - 'A');
            setStatus(session, "Using...");
            return SheetKeyOutcome::None;
        }
        session.pending_use_slot = -1;
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    if (session.sub_mode == SheetSubMode::RemovePickEquip) {
        session.sub_mode = SheetSubMode::Normal;
        if (key >= '1' && key <= '6') {
            const int eq = key - '1';
            const Mm2RosterItemSlot src = mm2_roster_equipped(rec, eq);
            if (src.item_id == 0) {
                setStatus(session, "Empty slot.");
                return SheetKeyOutcome::None;
            }
            const int bp = firstEmptyBackpack(*rec);
            if (bp < 0) {
                setStatus(session, "Backpack full.");
                return SheetKeyOutcome::None;
            }
            reverseEquippedSlotEffects(*rec, eq, items);
            mm2_roster_set_backpack(rec, bp, src);
            mm2_roster_set_equipped(rec, eq, Mm2RosterItemSlot{});
            recomputeWeaponFields(*rec, items);
            setStatus(session, "Removed.");
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    if (session.sub_mode == SheetSubMode::DropPickSlot) {
        session.sub_mode = SheetSubMode::Normal;
        if (key >= '1' && key <= '6') {
            const int eq = key - '1';
            if (rec->equipped_id[eq] == 0) {
                setStatus(session, "Empty slot.");
            } else {
                reverseEquippedSlotEffects(*rec, eq, items);
                mm2_roster_set_equipped(rec, eq, Mm2RosterItemSlot{});
                recomputeWeaponFields(*rec, items);
                setStatus(session, "Dropped.");
            }
            return SheetKeyOutcome::None;
        }
        if (key >= 'A' && key <= 'F') {
            const int bp = key - 'A';
            if (rec->backpack_id[bp] == 0) {
                setStatus(session, "Empty slot.");
            } else {
                mm2_roster_set_backpack(rec, bp, Mm2RosterItemSlot{});
                setStatus(session, "Dropped.");
            }
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    /* View-only spell book (combat/sheet 'V' → 0x675A). ESC closes upstream. */
    if (session.sub_mode == SheetSubMode::SpellBook) {
        return SheetKeyOutcome::None;
    }

    /* Exploration cast picker @ 0x6E30 / 0x79EE: level then number; ESC aborts. */
    if (session.sub_mode == SheetSubMode::CastPicker) {
        if (key >= '1' && key <= '9') {
            const int digit = key - '0';
            if (session.cast_phase == CastPromptPhase::Level) {
                const int max_sl = rec->spell_level > 0 ? static_cast<int>(rec->spell_level) : 0;
                if (digit < 1 || digit > max_sl || digit > kSpellLevels) {
                    return SheetKeyOutcome::None;
                }
                session.cast_level = digit;
                session.cast_phase = CastPromptPhase::Number;
                return SheetKeyOutcome::None;
            }
            const int flat = spellFlatFromLevelNumber(session.cast_level, digit);
            if (flat < 0 || !spellKnownInBook(*rec, flat)) {
                return SheetKeyOutcome::None;
            }
            session.cast_spell_flat = flat;
            session.sub_mode = SheetSubMode::Normal;
            session.cast_phase = CastPromptPhase::Level;
            /* GameSession::castSpellFromSheet fills status (fail / prompt / effect). */
            setStatus(session, "");
            return SheetKeyOutcome::None;
        }
        return SheetKeyOutcome::None;
    }

    /* Gather ($8050): '1' → $7F68 gold, '2' → $7FF8 gems into the current character.
       Gold: only roster indices < $18 (heroes); hireling +$66 is their daily fee.
       Initiator must also be a hero ($7F68 slt vs #$18) or the gather is a no-op. */
    if (session.sub_mode == SheetSubMode::GatherPick) {
        session.sub_mode = SheetSubMode::Normal;
        if (key == '1') {
            if (rosterIndexForPartySlot(launch, session.party_slot) >= 0x18) {
                setStatus(session, "Cannot gather gold.");
                return SheetKeyOutcome::None;
            }
            uint32_t total = 0;
            for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                const int ri = rosterIndexForPartySlot(launch, i);
                if (ri < 0 || ri >= 0x18) {
                    continue;
                }
                Mm2RosterRecord *m = rosterMut(roster, launch, i);
                if (!m) {
                    continue;
                }
                total += m->gold;
                m->gold = 0;
            }
            rec->gold = total;
            setStatus(session, "Gold gathered.");
            return SheetKeyOutcome::None;
        }
        if (key == '2') {
            /* $7FF8: gems pool every party slot (no $18 gate). */
            uint32_t total = 0;
            for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                Mm2RosterRecord *m = rosterMut(roster, launch, i);
                if (!m) {
                    continue;
                }
                total += m->gems;
                m->gems = 0;
            }
            rec->gems = static_cast<uint16_t>(total);
            setStatus(session, "Gems gathered.");
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    /* Share ($7DCC): '1' gold ($7BBE), '2' gems ($7CB0), '3' food ($7D3E).
       Equal split via -$7B4E-style truncating divide; remainder to initiator. */
    if (session.sub_mode == SheetSubMode::SharePick) {
        session.sub_mode = SheetSubMode::Normal;
        if (key == '1') {
            /* $7BBE: only roster indices < $18; need >=2 eligible. */
            if (rosterIndexForPartySlot(launch, session.party_slot) >= 0x18) {
                setStatus(session, "Cannot share gold.");
                return SheetKeyOutcome::None;
            }
            int eligible[MM2_PARTY_LAUNCH_SLOTS];
            int n = 0;
            uint32_t total = 0;
            for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                const int ri = rosterIndexForPartySlot(launch, i);
                if (ri < 0 || ri >= 0x18) {
                    continue;
                }
                Mm2RosterRecord *m = rosterMut(roster, launch, i);
                if (!m) {
                    continue;
                }
                eligible[n++] = i;
                total += m->gold;
            }
            if (n <= 1) {
                setStatus(session, "Cannot share gold.");
                return SheetKeyOutcome::None;
            }
            const uint32_t share = total / static_cast<uint32_t>(n);
            for (int k = 0; k < n; ++k) {
                Mm2RosterRecord *m = rosterMut(roster, launch, eligible[k]);
                if (m) {
                    m->gold = share;
                    total -= share;
                }
            }
            rec->gold += total;
            setStatus(session, "Gold shared.");
            return SheetKeyOutcome::None;
        }
        if (key == '2') {
            /* $7CB0: all party members via -$795a count. */
            const int n = launch.party_count;
            if (n <= 0) {
                setStatus(session, "");
                return SheetKeyOutcome::None;
            }
            uint32_t total = 0;
            for (int i = 0; i < n && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                Mm2RosterRecord *m = rosterMut(roster, launch, i);
                if (m) {
                    total += m->gems;
                }
            }
            const uint16_t share = static_cast<uint16_t>(total / static_cast<uint32_t>(n));
            for (int i = 0; i < n && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                Mm2RosterRecord *m = rosterMut(roster, launch, i);
                if (m) {
                    m->gems = share;
                    total -= share;
                }
            }
            rec->gems = static_cast<uint16_t>(rec->gems + total);
            setStatus(session, "Gems shared.");
            return SheetKeyOutcome::None;
        }
        if (key == '3') {
            /* $7D3E: all party; divu by party count; remainder byte to initiator. */
            const int n = launch.party_count;
            if (n <= 0) {
                setStatus(session, "");
                return SheetKeyOutcome::None;
            }
            uint16_t total = 0;
            for (int i = 0; i < n && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                Mm2RosterRecord *m = rosterMut(roster, launch, i);
                if (m) {
                    total = static_cast<uint16_t>(total + m->food);
                }
            }
            const uint8_t share = static_cast<uint8_t>(total / static_cast<uint16_t>(n));
            for (int i = 0; i < n && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
                Mm2RosterRecord *m = rosterMut(roster, launch, i);
                if (m) {
                    m->food = share;
                    total = static_cast<uint16_t>(total - share);
                }
            }
            rec->food = static_cast<uint8_t>(rec->food + static_cast<uint8_t>(total & 0xFF));
            setStatus(session, "Food shared.");
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    /* Trade ($E61C): pick type then a target member. The retail menu prompts keys
       '1'..'4' (prompt range $31..$34 @ 0xE678): '1' gold ($E2D0, rec +$66 u32),
       '2' gems ($E35A, +$5C u16), '3' food ($E3C6, +$25 u8), '4' item ($E492, moves
       one backpack slot id/charges/flags @ +$3A/+$40/+$46). Retail prompts for an
       amount on gold/gems/food; the single-key port moves the full balance. */
    if (session.sub_mode == SheetSubMode::TradePickType) {
        if (key == '1') {
            session.trade_kind = SheetTradeKind::Gold;
            session.sub_mode = SheetSubMode::TradePickTarget;
            setStatus(session, "Trade gold to which? (1-8)");
            return SheetKeyOutcome::None;
        }
        if (key == '2') {
            session.trade_kind = SheetTradeKind::Gems;
            session.sub_mode = SheetSubMode::TradePickTarget;
            setStatus(session, "Trade gems to which? (1-8)");
            return SheetKeyOutcome::None;
        }
        if (key == '3') {
            session.trade_kind = SheetTradeKind::Food;
            session.sub_mode = SheetSubMode::TradePickTarget;
            setStatus(session, "Trade food to which? (1-8)");
            return SheetKeyOutcome::None;
        }
        if (key == '4') {
            session.trade_kind = SheetTradeKind::Items;
            session.sub_mode = SheetSubMode::TradePickTarget;
            setStatus(session, "Trade item to which? (1-8)");
            return SheetKeyOutcome::None;
        }
        session.sub_mode = SheetSubMode::Normal;
        session.trade_kind = SheetTradeKind::None;
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    if (session.sub_mode == SheetSubMode::TradePickTarget) {
        const SheetTradeKind kind = session.trade_kind;
        if (key >= '1' && key <= '8') {
            const int target_slot = key - '1';
            if (target_slot == session.party_slot) {
                session.sub_mode = SheetSubMode::Normal;
                session.trade_kind = SheetTradeKind::None;
                setStatus(session, "Same character.");
                return SheetKeyOutcome::None;
            }
            Mm2RosterRecord *tgt = rosterMut(roster, launch, target_slot);
            if (!tgt) {
                session.sub_mode = SheetSubMode::Normal;
                session.trade_kind = SheetTradeKind::None;
                setStatus(session, "No such member.");
                return SheetKeyOutcome::None;
            }
            if (kind == SheetTradeKind::Gold) {
                tgt->gold += rec->gold;
                rec->gold = 0;
                setStatus(session, "Traded gold.");
            } else if (kind == SheetTradeKind::Gems) {
                tgt->gems = static_cast<uint16_t>(tgt->gems + rec->gems);
                rec->gems = 0;
                setStatus(session, "Traded gems.");
            } else if (kind == SheetTradeKind::Food) {
                /* $E3C6 rejects the transfer if the target would exceed kMaxFood. */
                if (static_cast<int>(tgt->food) + static_cast<int>(rec->food) > kSheetMaxFood) {
                    setStatus(session, "Too much food.");
                } else {
                    tgt->food = static_cast<uint8_t>(tgt->food + rec->food);
                    rec->food = 0;
                    setStatus(session, "Traded food.");
                }
            } else if (kind == SheetTradeKind::Items) {
                /* $E492 prompts for the source backpack letter AFTER the target is
                   chosen; defer to the backpack-letter sub-mode. */
                session.trade_target_slot = target_slot;
                session.sub_mode = SheetSubMode::TradePickItemSlot;
                setStatus(session, "Trade which pack item? (A-F)");
                return SheetKeyOutcome::None;
            }
            session.sub_mode = SheetSubMode::Normal;
            session.trade_kind = SheetTradeKind::None;
            return SheetKeyOutcome::None;
        }
        session.sub_mode = SheetSubMode::Normal;
        session.trade_kind = SheetTradeKind::None;
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    /* Item trade backpack pick ($E492): A-F selects the source's backpack slot; the
       item (id/charges/flags @ +$3A/+$40/+$46) moves to the target's first empty
       backpack slot. Rejected if the source slot is empty or the target pack is full. */
    if (session.sub_mode == SheetSubMode::TradePickItemSlot) {
        const int target_slot = session.trade_target_slot;
        session.sub_mode = SheetSubMode::Normal;
        session.trade_kind = SheetTradeKind::None;
        session.trade_target_slot = -1;
        if (key >= 'A' && key <= 'F') {
            const int bp = key - 'A';
            Mm2RosterRecord *tgt = rosterMut(roster, launch, target_slot);
            if (!tgt) {
                setStatus(session, "No such member.");
                return SheetKeyOutcome::None;
            }
            const Mm2RosterItemSlot src = mm2_roster_backpack(rec, bp);
            if (src.item_id == 0) {
                setStatus(session, "Empty slot.");
                return SheetKeyOutcome::None;
            }
            const int dst = firstEmptyBackpack(*tgt);
            if (dst < 0) {
                setStatus(session, "Pack full.");
                return SheetKeyOutcome::None;
            }
            mm2_roster_set_backpack(tgt, dst, src);
            mm2_roster_set_backpack(rec, bp, Mm2RosterItemSlot{});
            setStatus(session, "Traded item.");
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    switch (key) {
    case 'V':
        if (combat_mode) {
            const SpellSchool school = spellSchoolForClass(rec->class_id);
            if (school == SpellSchool::None) {
                setStatus(session, "No spell book.");
            } else {
                session.sub_mode = SheetSubMode::SpellBook;
                setStatus(session, "");
            }
        }
        break;
    case 'C': {
        /* Sheet 'C' → exploration cast @ 0x6E30 (grid + 0x79EE). Combat command
           'C' never opens the sheet — GameSession routes it to CombatSession. */
        if (combat_mode) {
            break;
        }
        const SpellSchool school = spellSchoolForClass(rec->class_id);
        if (school == SpellSchool::None || rec->spell_level == 0) {
            setStatus(session, "No spell book.");
            break;
        }
        session.sub_mode = SheetSubMode::CastPicker;
        session.cast_phase = CastPromptPhase::Level;
        session.cast_level = 0;
        session.cast_spell_flat = -1;
        setStatus(session, "");
        break;
    }
    case 'D':
        if (combat_mode) {
            break;
        }
        session.sub_mode = SheetSubMode::DropPickSlot;
        setStatus(session, "Drop equip 1-6 or pack A-F:");
        break;
    case 'E':
        if (combat_mode) {
            break;
        }
        session.sub_mode = SheetSubMode::EquipPickBackpack;
        setStatus(session, "Equip which? (A-F)");
        break;
    case 'G':
        if (combat_mode) {
            break;
        }
        session.sub_mode = SheetSubMode::GatherPick;
        setStatus(session, "Gather: 1) Gold  2) Gems");
        break;
    case 'R':
        if (combat_mode) {
            break;
        }
        session.sub_mode = SheetSubMode::RemovePickEquip;
        setStatus(session, "Remove which? (1-6)");
        break;
    case 'S':
        if (combat_mode) {
            break;
        }
        session.sub_mode = SheetSubMode::SharePick;
        setStatus(session, "Share: 1) Gold  2) Gems  3) Food");
        break;
    case 'T':
        if (combat_mode) {
            break;
        }
        session.sub_mode = SheetSubMode::TradePickType;
        session.trade_kind = SheetTradeKind::None;
        session.trade_target_slot = -1;
        setStatus(session, "Trade: 1)Gold 2)Gems 3)Food 4)Item");
        break;
    case 'U':
        if (combat_mode) {
            break;
        }
        session.sub_mode = SheetSubMode::UsePick;
        session.pending_use_slot = -1;
        setStatus(session, "Use which? (1-6/A-F)");
        break;
    default:
        break;
    }

    return SheetKeyOutcome::None;
}

}  // namespace mm2::gameplay

