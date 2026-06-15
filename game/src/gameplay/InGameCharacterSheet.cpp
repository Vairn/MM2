#include "mm2/gameplay/InGameCharacterSheet.h"

#include "mm2/CppStdCompat.h"

#include "mm2/DataPath.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PartyStatusFormat.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"

#include "mm2_image32_codec.h"

namespace mm2::gameplay {

namespace {

using namespace mm2::ui::amiga_layout;
using namespace mm2::gfx::play_layout;

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

void drawCellHorizRule(gfx::ScreenCompositor &c, int row, int col, int len, uint8_t r = 200, uint8_t g = 200,
                       uint8_t b = 200)
{
    for (int i = 0; i < len; ++i) {
        c.drawGlyph(cellX(col + i), cellY(row), '-', r, g, b, 255);
    }
}

void drawDoubleSectionHeader(gfx::ScreenCompositor &c, int row, int col, int width_cells, const char *label1,
                             const char *label2, uint8_t r = 200, uint8_t g = 200, uint8_t b = 200)
{
    const int l1 = static_cast<int>(std::strlen(label1));
    const int l2 = static_cast<int>(std::strlen(label2));
    if (width_cells <= l1 + l2) {
        drawCellText(c, row, col, label1, r, g, b);
        return;
    }
    const int pad = width_cells - l1 - l2;
    const int left_rule = pad / 4;
    const int right_rule = pad / 4;
    const int mid_rule = pad - left_rule - right_rule;
    int cx = col;
    drawCellHorizRule(c, row, cx, left_rule, r, g, b);
    cx += left_rule;
    drawCellText(c, row, cx, label1, r, g, b);
    cx += l1;
    drawCellHorizRule(c, row, cx, mid_rule, r, g, b);
    cx += mid_rule;
    drawCellText(c, row, cx, label2, r, g, b);
    cx += l2;
    drawCellHorizRule(c, row, cx, right_rule, r, g, b);
}

/** Draw label + current (right-aligned before slash) + '/' + max — sheet / Quick Ref style. */
void drawSlashStat(gfx::ScreenCompositor &c, int row, int label_col, int slash_col, const char *label,
                   uint16_t current, uint16_t max, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255)
{
    const int label_len = static_cast<int>(std::strlen(label));
    const int field_width = slash_col - (label_col + label_len);
    if (field_width <= 0) {
        return;
    }

    char buf[24];
    drawCellText(c, row, label_col, label, r, g, b);
    gfx::formatSlashStatCurrent(current, buf, sizeof(buf), field_width);
    drawCellText(c, row, label_col + label_len, buf, r, g, b);
    drawCellText(c, row, slash_col, "/", r, g, b);
    std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(max));
    drawCellText(c, row, slash_col + 1, buf, r, g, b);
}

/** Quick Ref HP/SP pair: current right-aligned into field ending at slash_col, then '/' + max. */
void drawQuickRefSlashPair(gfx::ScreenCompositor &c, int row, int slash_col, uint16_t current, uint16_t max)
{
    constexpr int kValWidth = 5;
    char buf[16];
    gfx::formatSlashStatCurrent(current, buf, sizeof(buf), kValWidth);
    drawCellText(c, row, slash_col - kValWidth, buf);
    drawCellText(c, row, slash_col, "/");
    std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(max));
    drawCellText(c, row, slash_col + 1, buf);
}

void blitBookFrame(gfx::ScreenCompositor &c, const mm2_image32_file &book, int frame, int dst_x, int dst_y)
{
    if (frame < 0 || frame >= book.frame_count) {
        return;
    }
    const mm2_image32_frame &f = book.frames[frame];
    if (!f.rgba) {
        return;
    }
    c.blitRgba(f.rgba, f.width, f.height, dst_x, dst_y);
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

int firstEmptyEquip(const Mm2RosterRecord &rec)
{
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.equipped[i].item_id == 0) {
            return i;
        }
    }
    return -1;
}

int firstEmptyBackpack(const Mm2RosterRecord &rec)
{
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (rec.backpack[i].item_id == 0) {
            return i;
        }
    }
    return -1;
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
    // Retail prompt_esc @ 0x6DA6 uses row $17; sheet commands print at col $01 — keep ESC
    // left-aligned with the three command rows above (not centered in the red frame).
    drawBorderIntegratedTextAt(c, overlayBottomRow(), kSheetFooterCol, "( 'ESC' to go back )", 180, 180, 180);
}

}  // namespace

bool InGameCharacterSheet::loadAssets(const char *data_dir)
{
    if (has_book_) {
        return true;
    }
    if (!data_dir) {
        return false;
    }

    char *path = mm2_path_scratch_a();
    if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir, "book.32")) {
        return false;
    }

    mm2_image32_set_preview_opaque(0);
    if (mm2_image32_load_file(path, &book_) == MM2_IMAGE32_OK && book_.frame_count > 0) {
        has_book_ = true;
    }
    return has_book_;
}

void InGameCharacterSheet::renderSheet(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                                       const Mm2PartyLaunch &launch, int party_slot, const Mm2ItemsFile *items,
                                       const SheetSession *session) const
{
    const int roster_idx = rosterIndexForPartySlot(launch, party_slot);
    if (roster_idx < 0 || roster_idx >= MM2_ROSTER_RECORD_COUNT) {
        drawCellText(c, 2, 2, "No character selected.");
        return;
    }

    const Mm2RosterRecord &rec = roster.records[roster_idx];
    const char disp_char = static_cast<char>('1' + party_slot);

    gfx::drawPlayModalBackdrop(c);

    char name[16];
    mm2_roster_name_to_cstr(&rec, name, sizeof(name));
    char header[80];
    std::snprintf(header, sizeof(header), "%c) %s: %s %s %s %s%s", disp_char, name, rec.sex ? "F" : "M",
                  alignHeaderName(rec.alignment_base), raceHeaderName(rec.race), className(rec.class_id),
                  (rec.class_quest_guild_mask & 0x80) ? "+" : "");
    drawBorderIntegratedTextAt(c, kPlayOverlayBorderRow, kSheetHeaderCol, header);

    char buf[48];
    const int r0 = kSheetStatRowBase;
    std::snprintf(buf, sizeof(buf), "Lvl=%u", rec.level);
    drawCellText(c, r0 + 0, kSheetStatColLeft, buf);
    std::snprintf(buf, sizeof(buf), "Mgt=%u", rec.might_base);
    drawCellText(c, r0 + 1, kSheetStatColLeft, buf);
    std::snprintf(buf, sizeof(buf), "Int=%u", rec.intelligence_base);
    drawCellText(c, r0 + 2, kSheetStatColLeft, buf);
    std::snprintf(buf, sizeof(buf), "Per=%u", rec.personality_base);
    drawCellText(c, r0 + 3, kSheetStatColLeft, buf);
    std::snprintf(buf, sizeof(buf), "End=%u", rec.endurance_base);
    drawCellText(c, r0 + 4, kSheetStatColLeft, buf);
    std::snprintf(buf, sizeof(buf), "Spd=%u", rec.speed_base);
    drawCellText(c, r0 + 5, kSheetStatColLeft, buf);
    std::snprintf(buf, sizeof(buf), "Acy=%u", rec.accuracy_base);
    drawCellText(c, r0 + 6, kSheetStatColLeft, buf);
    std::snprintf(buf, sizeof(buf), "Lck=%u", rec.luck_base);
    drawCellText(c, r0 + 7, kSheetStatColLeft, buf);

    drawSlashStat(c, r0 + 0, kSheetStatColMid, kInGameSheetSlashCol, "HP=", rec.hp_current, rec.hp_max);
    drawSlashStat(c, r0 + 1, kSheetStatColMid, kInGameSheetSlashCol, "SP=", rec.sp_current, rec.sp_max);
    std::snprintf(buf, sizeof(buf), "AC=%u", rec.armor_class);
    drawCellText(c, r0 + 3, kSheetStatColMid, buf);
    std::snprintf(buf, sizeof(buf), "SL=%u", rec.spell_level);
    drawCellText(c, r0 + 3, kInGameSheetSlashCol + 1, buf);

    std::snprintf(buf, sizeof(buf), "Age=%u", rec.age);
    drawCellText(c, r0 + 0, kSheetStatColRight, buf);
    std::snprintf(buf, sizeof(buf), "Exp=%lu", static_cast<unsigned long>(rec.experience));
    drawCellText(c, r0 + 1, kSheetStatColRight, buf);
    std::snprintf(buf, sizeof(buf), "Gold=%lu", static_cast<unsigned long>(rec.gold));
    drawCellText(c, r0 + 3, kSheetStatColCost, buf);
    std::snprintf(buf, sizeof(buf), "Gems=%u", rec.gems);
    drawCellText(c, r0 + 4, kSheetStatColCost, buf);
    std::snprintf(buf, sizeof(buf), "Food=%u", rec.food);
    drawCellText(c, r0 + 5, kSheetStatColCost, buf);
    std::snprintf(buf, sizeof(buf), "Cond= %s", conditionName(rec.condition));
    drawCellText(c, r0 + 6, kSheetStatColRight, buf);

    const int divider_w = kPlayOverlayBorderCol + kPlayOverlayBorderW - 2 - kSheetEquipCol + 1;
    drawDoubleSectionHeader(c, kSheetDividerRow, kSheetEquipCol, divider_w, " Equipped ", " Backpack ");

    static const char kPackLetters[] = "ABCDEF";
    char iname[16];
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        const int row = kSheetEquipRowBase + i;
        char eline[24];
        if (rec.equipped[i].item_id) {
            itemLabel(iname, sizeof(iname), items, rec.equipped[i].item_id);
            std::snprintf(eline, sizeof(eline), "%d) %-10s", i + 1, iname);
        } else {
            std::snprintf(eline, sizeof(eline), "%d)", i + 1);
        }
        drawCellText(c, row, kSheetEquipCol, eline, 220, 220, 220);

        if (rec.backpack[i].item_id) {
            itemLabel(iname, sizeof(iname), items, rec.backpack[i].item_id);
            std::snprintf(eline, sizeof(eline), "%c) %-10s", kPackLetters[i], iname);
        } else {
            std::snprintf(eline, sizeof(eline), "%c)", kPackLetters[i]);
        }
        drawCellText(c, row, kSheetBackpackCol, eline, 220, 220, 220);
    }

    /* book.32 @ row 18 can overlap footer rows from stale viewport — clear before commands. */
    gfx::fillCellRect(c, kSheetFooterCol, kSheetFooterRow1 - 1, kPlayOverlayBorderW - 2, 4);

    drawCellText(c, kSheetFooterRow1, kSheetFooterCol, "'Q' Quick Ref  'C' Cast    'D' Drop", 180, 180, 180);
    drawCellText(c, kSheetFooterRow2, kSheetFooterCol, "'E' Equip      'G' Gather  'R' Remove", 180, 180, 180);
    drawCellText(c, kSheetFooterCmdRow3, kSheetFooterCol, "'S' Share  'T' Trade  'U' Use", 180, 180, 180);

    if (session && session->status_line[0]) {
        drawCellText(c, kSheetFooterRow1 - 1, kSheetFooterCol, session->status_line, 255, 255, 128);
    }

    drawSheetEscFooter(c);
}

void InGameCharacterSheet::renderQuickRef(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                                          const Mm2PartyLaunch &launch) const
{
    gfx::drawPlayModalBackdrop(c);

    drawCellText(c, kQuickRefHeaderRow1, kQuickRefColIndex, "#     Name    Hit Points  Spell Points", 255, 255, 128);
    drawCellText(c, kQuickRefHeaderRow2, kQuickRefColIndex, "# Lvl SL AC Age Gems  Food Condition", 255, 255, 128);

    for (int i = 0; i < launch.party_count && i < 8; ++i) {
        const int roster_idx = rosterIndexForPartySlot(launch, i);
        if (roster_idx < 0) {
            continue;
        }
        const Mm2RosterRecord &rec = roster.records[roster_idx];
        char name[16];
        mm2_roster_name_to_cstr(&rec, name, sizeof(name));

        const int row1 = kQuickRefDataRow1Base + i;
        char line1[32];
        std::snprintf(line1, sizeof(line1), "%d)  %-11s", i + 1, name);
        drawCellText(c, row1, kQuickRefColIndex, line1);
        drawQuickRefSlashPair(c, row1, kQuickRefColHpSlash, rec.hp_current, rec.hp_max);
        drawQuickRefSlashPair(c, row1, kQuickRefColSpSlash, rec.sp_current, rec.sp_max);

        const int row2 = kQuickRefDataRow2Base + i;
        char line2[8];
        std::snprintf(line2, sizeof(line2), "%d", i + 1);
        drawCellText(c, row2, kQuickRefColIndex, line2, 200, 200, 200);
        std::snprintf(line2, sizeof(line2), "%u", rec.level);
        drawCellText(c, row2, kQuickRefColLvl, line2, 200, 200, 200);
        std::snprintf(line2, sizeof(line2), "%u", rec.spell_level);
        drawCellText(c, row2, kQuickRefColSL, line2, 200, 200, 200);
        std::snprintf(line2, sizeof(line2), "%u", rec.armor_class);
        drawCellText(c, row2, kQuickRefColAC, line2, 200, 200, 200);
        std::snprintf(line2, sizeof(line2), "%u", rec.age);
        drawCellText(c, row2, kQuickRefColAge, line2, 200, 200, 200);
        std::snprintf(line2, sizeof(line2), "%u", rec.gems);
        drawCellText(c, row2, kQuickRefColGems, line2, 200, 200, 200);
        std::snprintf(line2, sizeof(line2), "%u", rec.food);
        drawCellText(c, row2, kQuickRefColFood, line2, 200, 200, 200);
        drawCellText(c, row2, kQuickRefColCond, conditionName(rec.condition), 200, 200, 200);
    }

    drawModalEscFooter(c);
}

SheetKeyOutcome InGameCharacterSheet::handleKey(char key, SheetSession &session, Mm2RosterFile &roster,
                                                const Mm2PartyLaunch &launch, const Mm2ItemsFile *items)
{
    Mm2RosterRecord *rec = rosterMut(roster, launch, session.party_slot);
    if (!rec) {
        return SheetKeyOutcome::Close;
    }

    if (session.sub_mode == SheetSubMode::EquipPickBackpack) {
        session.sub_mode = SheetSubMode::Normal;
        if (key >= 'A' && key <= 'F') {
            const int bp = key - 'A';
            const Mm2RosterItemSlot &src = rec->backpack[bp];
            if (src.item_id == 0) {
                setStatus(session, "Empty slot.");
                return SheetKeyOutcome::None;
            }
            const Mm2ItemRecord *item = items ? mm2_items_lookup(items, src.item_id) : nullptr;
            if (item && !mm2_item_class_can_use(item, rec->class_id)) {
                setStatus(session, "Class cannot use.");
                return SheetKeyOutcome::None;
            }
            const int eq = firstEmptyEquip(*rec);
            if (eq < 0) {
                setStatus(session, "No empty equip slot.");
                return SheetKeyOutcome::None;
            }
            rec->equipped[eq] = src;
            rec->backpack[bp].item_id = 0;
            rec->backpack[bp].bonus = 0;
            rec->backpack[bp].flags = 0;
            setStatus(session, "Equipped.");
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    if (session.sub_mode == SheetSubMode::RemovePickEquip) {
        session.sub_mode = SheetSubMode::Normal;
        if (key >= '1' && key <= '6') {
            const int eq = key - '1';
            Mm2RosterItemSlot &src = rec->equipped[eq];
            if (src.item_id == 0) {
                setStatus(session, "Empty slot.");
                return SheetKeyOutcome::None;
            }
            const int bp = firstEmptyBackpack(*rec);
            if (bp < 0) {
                setStatus(session, "Backpack full.");
                return SheetKeyOutcome::None;
            }
            rec->backpack[bp] = src;
            src.item_id = 0;
            src.bonus = 0;
            src.flags = 0;
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
            if (rec->equipped[eq].item_id == 0) {
                setStatus(session, "Empty slot.");
            } else {
                rec->equipped[eq].item_id = 0;
                rec->equipped[eq].bonus = 0;
                rec->equipped[eq].flags = 0;
                setStatus(session, "Dropped.");
            }
            return SheetKeyOutcome::None;
        }
        if (key >= 'A' && key <= 'F') {
            const int bp = key - 'A';
            if (rec->backpack[bp].item_id == 0) {
                setStatus(session, "Empty slot.");
            } else {
                rec->backpack[bp].item_id = 0;
                rec->backpack[bp].bonus = 0;
                rec->backpack[bp].flags = 0;
                setStatus(session, "Dropped.");
            }
            return SheetKeyOutcome::None;
        }
        setStatus(session, "");
        return SheetKeyOutcome::None;
    }

    if (session.sub_mode == SheetSubMode::CastPicker) {
        session.sub_mode = SheetSubMode::Normal;
        /* GAP: spell execution via $CD90 / LAB_CDB8 after 0x79C6 picker. */
        setStatus(session, "Cast execution not yet wired.");
        return SheetKeyOutcome::None;
    }

    switch (key) {
    case 'C':
        if (rec->condition >= 0x80 || (rec->condition & 0x70) != 0) {
            setStatus(session, "Cannot cast now.");
            break;
        }
        if (rec->spell_level == 0) {
            setStatus(session, "Not a spell caster.");
            break;
        }
        session.sub_mode = SheetSubMode::CastPicker;
        setStatus(session, "Spell book (GAP $6E08) — ESC cancels.");
        break;
    case 'D':
        session.sub_mode = SheetSubMode::DropPickSlot;
        setStatus(session, "Drop equip 1-6 or pack A-F:");
        break;
    case 'E':
        session.sub_mode = SheetSubMode::EquipPickBackpack;
        setStatus(session, "Equip which? (A-F)");
        break;
    case 'G':
        /* GAP: gather gold via $8050. */
        setStatus(session, "Gather not yet wired ($8050).");
        break;
    case 'R':
        session.sub_mode = SheetSubMode::RemovePickEquip;
        setStatus(session, "Remove which? (1-6)");
        break;
    case 'S':
        /* GAP: share via $7DCC / 0x6ACE. */
        setStatus(session, "Share not yet wired ($7DCC).");
        break;
    case 'T':
        /* GAP: trade via $E61C. */
        setStatus(session, "Trade not yet wired ($E61C).");
        break;
    case 'U':
        /* GAP: item use via $E94A / combat_use_item_handler 0x133EC. */
        setStatus(session, "Use not yet wired ($E94A).");
        break;
    default:
        break;
    }

    return SheetKeyOutcome::None;
}

}  // namespace mm2::gameplay

