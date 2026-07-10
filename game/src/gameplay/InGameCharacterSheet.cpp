#include "mm2/gameplay/InGameCharacterSheet.h"

#include "mm2/CppStdCompat.h"

#include "mm2/DataPath.h"

#include "mm2/gameplay/SpellBook.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PartyStatusFormat.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"
#include "mm2/ui/RosterSkillDisplay.h"

#include "mm2_image32_codec.h"

#include "mm2_gfx_sheet.h"

#include "mm2/platform/Platform.h"

namespace mm2::gameplay {

namespace {

constexpr int kMinBookFramesForSheet = 12; /* LAB_4252 @ 0x422A blits book.32 frame index 11. */

/** PC ``BOOK.16`` is title-menu ornament only (doc 54); sheet composite needs Amiga ``book.32``. */
bool sheetBookCompositeReady(bool has_book, const mm2_image32_file *book)
{
    return has_book && book && book->frame_count >= kMinBookFramesForSheet;
}

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

void blitBookFramePc(gfx::ScreenCompositor &c, const mm2_gfx_sheet &book, int frame, int dst_x, int dst_y)
{
    const mm2_gfx_frame *f = mm2_gfx_sheet_frame(&book, frame);
    if (!f) {
        return;
    }
#if MM2_HOST_AMIGA
    if (!f->bitmap) {
        return;
    }
    platform::blitImage32(&book.img, static_cast<uint16_t>(frame), dst_x, dst_y, 0);
#else
    if (!f->rgba) {
        return;
    }
    c.blitRgba(f->rgba, f->width, f->height, dst_x, dst_y, true, 255);
#endif
}

/** LAB_4252 @ 0x4252: composite book.32 frames 11,11,5,5,4,3,2,1 @ kPartyPanelBlit*. */
void blitCharacterSheetBook(gfx::ScreenCompositor &c, bool pc_mode, const mm2_gfx_sheet *book_pc,
                            const mm2_image32_file *book)
{
    static const int kFrames[] = {11, 11, 5, 5, 4, 3, 2, 1};
    int x = kPartyPanelBlitX;
    const int y = kPartyPanelBlitY;

    for (int i = 0; i < 8; ++i) {
        int fi = kFrames[i];
        int fw = 96;
        int fh = 60;
        if (pc_mode && book_pc) {
            const mm2_gfx_frame *f = mm2_gfx_sheet_frame(book_pc, fi);
            if (!f && book_pc->img.frame_count > 0) {
                fi = fi % static_cast<int>(book_pc->img.frame_count);
                f = mm2_gfx_sheet_frame(book_pc, fi);
            }
            if (!f) {
                continue;
            }
            fw = static_cast<int>(f->width);
            fh = static_cast<int>(f->height);
            blitBookFramePc(c, *book_pc, fi, x, y);
        } else if (book && book->frame_count > 0) {
            if (fi >= book->frame_count) {
                fi = fi % book->frame_count;
            }
            const mm2_image32_frame &f = book->frames[fi];
            fw = static_cast<int>(f.width);
            fh = static_cast<int>(f.height);
            blitBookFrame(c, *book, fi, x, y);
        } else {
            continue;
        }
        x -= fw;
        if (x < 0) {
            x = kPartyPanelBlitX;
        }
        (void)fh;
    }
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
    book_pc_mode_ = false;
    if (mm2_image32_load_file(path, &book_) == MM2_IMAGE32_OK && sheetBookCompositeReady(true, &book_)) {
        has_book_ = true;
    } else {
        mm2_image32_free(&book_);
    }
    return has_book_;
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
    const bool hireling = roster_idx >= kRosterHirelingPageOffset;
    const int hireling_index = hireling ? (roster_idx - kRosterHirelingPageOffset) : -1;

    gfx::drawPlayModalBackdrop(c);

    if (sheetBookCompositeReady(has_book_, &book_)) {
        blitCharacterSheetBook(c, false, nullptr, &book_);
    }

    char name[16];
    mm2_roster_name_to_cstr(&rec, name, sizeof(name));
    char header[80];
    std::snprintf(header, sizeof(header), "%c) %s: %s %s %s %s%s", disp_char, name, rec.sex ? "F" : "M",
                  alignHeaderName(rec.alignment_base), raceHeaderName(rec.race), className(rec.class_id),
                  (rec.class_quest_guild_mask & 0x80) ? "+" : "");
    drawBorderIntegratedTextAt(c, kPlayOverlayBorderRow, kSheetHeaderCol, header);

    const char *skill_names[6] = {};
    const int skill_count = hireling ? mm2::ui::collectHirelingSkillNames(hireling_index, skill_names, 6)
                                     : mm2::ui::collectRosterSkillNames(rec, skill_names, 6);

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

    std::snprintf(buf, sizeof(buf), "Thievery %u%%", mm2::ui::rosterDisplayThievery(rec));
    drawCellText(c, r0 + 4, kSheetStatColMid, buf);
    if (skill_count >= 1) {
        drawCellText(c, r0 + 5, kSheetStatColMid, skill_names[0]);
    } else {
        drawCellText(c, r0 + 5, kSheetStatColMid, mm2::ui::kRosterEmptySkillSlot);
    }
    if (skill_count >= 2) {
        drawCellText(c, r0 + 6, kSheetStatColMid, skill_names[1]);
    } else {
        drawCellText(c, r0 + 6, kSheetStatColMid, mm2::ui::kRosterEmptySkillSlot);
    }

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
        if (rec.equipped_id[i]) {
            itemLabel(iname, sizeof(iname), items, rec.equipped_id[i]);
            std::snprintf(eline, sizeof(eline), "%d) %-10s", i + 1, iname);
        } else {
            std::snprintf(eline, sizeof(eline), "%d)", i + 1);
        }
        drawCellText(c, row, kSheetEquipCol, eline, 220, 220, 220);

        if (rec.backpack_id[i]) {
            itemLabel(iname, sizeof(iname), items, rec.backpack_id[i]);
            std::snprintf(eline, sizeof(eline), "%c) %-10s", kPackLetters[i], iname);
        } else {
            std::snprintf(eline, sizeof(eline), "%c)", kPackLetters[i]);
        }
        drawCellText(c, row, kSheetBackpackCol, eline, 220, 220, 220);
    }

    /* book.32 @ row 18 can overlap footer rows from stale viewport — clear before commands. */
    gfx::fillCellRect(c, kSheetFooterCol, kSheetFooterRow1 - 1, kPlayOverlayBorderW - 2, 4);

    if (combat_mode) {
        drawCellText(c, kSheetFooterRow1, kSheetFooterCol, "'V' View spell book", 180, 180, 180);
    } else {
        drawCellText(c, kSheetFooterRow1, kSheetFooterCol, "'Q' Quick Ref  'C' Cast    'D' Drop", 180, 180, 180);
        drawCellText(c, kSheetFooterRow2, kSheetFooterCol, "'E' Equip      'G' Gather  'R' Remove", 180, 180, 180);
        drawCellText(c, kSheetFooterCmdRow3, kSheetFooterCol, "'S' Share  'T' Trade  'U' Use", 180, 180, 180);
    }

    if (session && session->status_line[0]) {
        drawCellText(c, kSheetFooterRow1 - 1, kSheetFooterCol, session->status_line, 255, 255, 128);
    }

    drawSheetEscFooter(c);

    if (session && session->sub_mode == SheetSubMode::SpellBook) {
        renderSpellBook(c, roster, launch, party_slot);
    } else if (session && session->sub_mode == SheetSubMode::CastPicker) {
        renderCastPicker(c, roster, launch, party_slot, *session);
    }
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

    /* Spell-book popup @ 0x675A: window -$7C74(row $A, col $7, w $1D, h $13).
       Grid renderer @ 0x6622: title $673C, header $6747, rows = level 1..9,
       known marks = font glyph $17, columns spaced every 2 cells from col $5.
       Combat/sheet 'V' is view-only — no cast prompts (doc 43 §6.1). */
    constexpr int kWinRow = 10;
    constexpr int kWinCol = 7;
    constexpr int kWinW = 29;
    constexpr int kWinH = 14;
    constexpr int kTitleRow = kWinRow + 1;
    constexpr int kHeaderRow = kWinRow + 3;
    constexpr int kGridRowBase = kWinRow + 4;
    constexpr int kLvlCol = kWinCol + 2;
    constexpr int kMarkColBase = kWinCol + 5;
    constexpr uint8_t kKnownMark = 0x17;

    c.fillRect(kWinCol * 8, kWinRow * 8, kWinW * 8, kWinH * 8, 0, 0, 128, 255);
    c.drawConsoleBox(kWinRow, kWinCol, kWinW, kWinH, 255, 255, 0);

    drawBorderIntegratedText(c, kTitleRow, kWinCol, kWinW, "Spell Book", 255, 255, 128);
    drawCellText(c, kHeaderRow, kLvlCol, "Lvl 1 2 3 4 5 6 7", 255, 255, 255);

    int flat = 0;
    for (int level = 1; level <= kSpellLevels; ++level) {
        const int row = kGridRowBase + (level - 1);
        char lvl[4];
        std::snprintf(lvl, sizeof(lvl), "%d", level);
        drawCellText(c, row, kLvlCol, lvl, 255, 255, 255);

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

    /* Prompt row: combat mode uses row $0F; exploration uses $15 (0x79F2/0x7A04). */
    constexpr int kPromptRow = 0x16;
    if (session.cast_phase == CastPromptPhase::Level) {
        drawCellText(c, kPromptRow, 0x02, " Spell Level: ", 255, 255, 255);
    } else {
        char buf[24];
        std::snprintf(buf, sizeof(buf), " Spell Level: %d", session.cast_level);
        drawCellText(c, kPromptRow, 0x02, buf, 255, 255, 255);
        drawCellText(c, kPromptRow, 0x0C, "Number: ", 255, 255, 255);
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
                setStatus(session, "Class cannot use.");
                return SheetKeyOutcome::None;
            }
            const int eq = firstEmptyEquip(*rec);
            if (eq < 0) {
                setStatus(session, "No empty equip slot.");
                return SheetKeyOutcome::None;
            }
            mm2_roster_set_equipped(rec, eq, src);
            mm2_roster_set_backpack(rec, bp, Mm2RosterItemSlot{});
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
            mm2_roster_set_backpack(rec, bp, src);
            mm2_roster_set_equipped(rec, eq, Mm2RosterItemSlot{});
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
                mm2_roster_set_equipped(rec, eq, Mm2RosterItemSlot{});
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
            /* GameSession dispatches via CombatSession::castSpellFromSheet. */
            {
                const SpellSchool school = spellSchoolForClass(rec->class_id);
                const SpellMeta *table = schoolSpellTable(school);
                if (table) {
                    char buf[48];
                    std::snprintf(buf, sizeof(buf), "Cast %s.", table[flat].name);
                    setStatus(session, buf);
                } else {
                    setStatus(session, "Cast.");
                }
            }
            return SheetKeyOutcome::None;
        }
        return SheetKeyOutcome::None;
    }

    /* Gather ($8050): '1' pools all party gold ($7F68), '2' pools gems ($7FF8) into
       the current character. Each member's amount is summed then zeroed, total -> rec. */
    if (session.sub_mode == SheetSubMode::GatherPick) {
        session.sub_mode = SheetSubMode::Normal;
        if (key == '1') {
            uint32_t total = 0;
            for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
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

