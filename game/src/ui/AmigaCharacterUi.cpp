#include "mm2/ui/ICharacterUi.h"

#include "mm2/gfx/Mm2FontGlyphs.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"
#include "mm2/ui/CharacterUiFactory.h"

#include "mm2/platform/Platform.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace mm2::ui {

namespace {

constexpr int kPlayableSlots = 48;
constexpr int kItemRecordSize = 0x14;
constexpr int kItemCount = 256;
constexpr int kItemNameLen = 12;

// Party limits (per the original inn party-assembly screen): up to 8 members
// total, of which at most 6 may be normal characters; the rest may be hirelings
// (so a party may hold up to 8 hirelings when it has 0 characters).
constexpr int kMaxParty = 8;
constexpr int kMaxCharacters = 6;

static const char *kClassNames[] = {
    "Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian",
};
static const char *kClassAbbrevs[] = {"K", "P", "A", "C", "S", "R", "N", "B"};
// 3-letter class abbreviations used by the inn party-assembly screen (matches the
// original "Kni/Pal/Arc/Cle/Sor/Rob/Nin/Bar" column in the WinUAE screenshot).
static const char *kClassAbbrev3[] = {"Kni", "Pal", "Arc", "Cle", "Sor", "Rob", "Nin", "Bar"};
// Home-town byte ($0B & 0x7F): 1..5. See EXTRACTED/docs/06-roster-format.md.
static const char *kTownNames[] = {"?", "Middlegate", "Sandsobar", "Vulcania", "Atlantium", "Tundara"};
static const char *kAlignHeaderNames[] = {
    "Good", "Neut", "Evil",
};
static const char *kRaceHeaderNames[] = {
    "Human", "Elf", "Dwarf", "Gnome", "H-Orc",
};

bool joinPath(char *out, std::size_t out_cap, const char *dir, const char *name)
{
    const std::size_t dir_len = std::strlen(dir);
    const std::size_t name_len = std::strlen(name);
    const bool need_sep = dir_len > 0 && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\';
    if (dir_len + name_len + (need_sep ? 1u : 0u) + 1u > out_cap) {
        return false;
    }
    std::snprintf(out, out_cap, "%s%s%s", dir, need_sep ? "/" : "", name);
    return true;
}

const char *classAbbrev(uint8_t id) { return id < 8 ? kClassAbbrevs[id] : "?"; }
const char *classAbbrev3(uint8_t id) { return id < 8 ? kClassAbbrev3[id] : "?"; }
const char *className(uint8_t id) { return id < 8 ? kClassNames[id] : "?"; }
const char *townName(uint8_t id) { return id >= 1 && id <= 5 ? kTownNames[id] : "?"; }
const char *raceHeaderName(uint8_t id) { return id < 5 ? kRaceHeaderNames[id] : "?"; }
const char *alignHeaderName(uint8_t id) { return id < 3 ? kAlignHeaderNames[id] : "?"; }

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
        if (c >= 4) {
            return "Poisoned";
        }
        return "?";
    }
}

void drawCellText(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255, uint8_t g = 255,
                  uint8_t b = 255)
{
    using namespace amiga_layout;
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

void drawCenteredCellText(gfx::ScreenCompositor &c, int row, int total_cols, const char *text, uint8_t r = 255,
                          uint8_t g = 255, uint8_t b = 255)
{
    const int len = static_cast<int>(std::strlen(text));
    int col = (total_cols - len) / 2;
    if (col < 0) {
        col = 0;
    }
    drawCellText(c, row, col, text, r, g, b);
}

// Small check-mark glyph (the font is ASCII-only, no U+221A). Drawn as a short
// up-right stroke inside one 8x8 text cell to mirror the original "√" prefix.
void drawCheckMark(gfx::ScreenCompositor &c, int row, int col, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255)
{
    using namespace amiga_layout;
    const int x = cellX(col);
    const int y = cellY(row);
    static const int kPts[][2] = {{1, 4}, {2, 5}, {3, 6}, {4, 4}, {5, 3}, {6, 2}, {7, 1}};
    for (const auto &p : kPts) {
        c.fillRect(x + p[0], y + p[1], 2, 2, r, g, b);
    }
}

void drawCellGlyph(gfx::ScreenCompositor &c, int row, int col, uint8_t codepoint, uint8_t r = 255, uint8_t g = 255,
                   uint8_t b = 255)
{
    using namespace amiga_layout;
    c.drawGlyph(cellX(col), cellY(row), codepoint, r, g, b, 255);
}

void drawCellHorizRule(gfx::ScreenCompositor &c, int row, int col, int len, uint8_t r = 255, uint8_t g = 255,
                       uint8_t b = 255)
{
    for (int i = 0; i < len; ++i) {
        drawCellGlyph(c, row, col + i, gfx::font_glyphs::kHorizRule, r, g, b);
    }
}

void drawCenteredHorizRule(gfx::ScreenCompositor &c, int row, int total_cols, int len, uint8_t r = 255,
                           uint8_t g = 255, uint8_t b = 255)
{
    int col = (total_cols - len) / 2;
    if (col < 0) {
        col = 0;
    }
    drawCellHorizRule(c, row, col, len, r, g, b);
}

// Section label flanked by font-8 horizontal rules (0x7B), matching OP_06 sign styling.
void drawSectionHeader(gfx::ScreenCompositor &c, int row, int col, int width_cells, const char *label,
                       uint8_t r = 200, uint8_t g = 200, uint8_t b = 200)
{
    const int label_len = static_cast<int>(std::strlen(label));
    if (width_cells <= label_len) {
        drawCellText(c, row, col, label, r, g, b);
        return;
    }
    const int pad = width_cells - label_len;
    const int left_rule = pad / 2;
    const int right_rule = pad - left_rule;
    int cx = col;
    drawCellHorizRule(c, row, cx, left_rule, r, g, b);
    cx += left_rule;
    drawCellText(c, row, cx, label, r, g, b);
    cx += label_len;
    drawCellHorizRule(c, row, cx, right_rule, r, g, b);
}

void drawRedBorder(gfx::ScreenCompositor &c, int row, int col, int width_cells, int height_cells)
{
    using namespace amiga_layout;
    c.drawConsoleBox(row, col, width_cells, height_cells, kBorderR, kBorderG, kBorderB);
}

class AmigaCharacterUi final : public ICharacterUi {
public:
    bool init(const char *data_dir) override
    {
        data_dir_ = data_dir;
        loadItems();
        return true;
    }

    void shutdown() override
    {
        if (item_names_) {
            platform::freeFileBuffer(item_names_);
            item_names_ = nullptr;
        }
    }

    void beginViewParty(Mm2RosterFile &roster) override
    {
        roster_ = &roster;
        view_mode_ = ViewMode::RosterList;
        roster_page_offset_ = 0;
        sheet_roster_index_ = -1;
        sheet_slot_letter_ = 'A';
    }

    UiResult tickViewParty(const platform::KeyState &keys) override
    {
        if (keys.escape) {
            if (view_mode_ == ViewMode::CharacterSheet) {
                view_mode_ = ViewMode::RosterList;
                return UiResult::Continue;
            }
            return UiResult::Cancel;
        }

        if (view_mode_ == ViewMode::RosterList) {
            if (keys.space) {
                roster_page_offset_ = (roster_page_offset_ == 0) ? amiga_layout::kRosterHirelingPageOffset : 0;
                return UiResult::Continue;
            }
            if (keys.last_ascii >= 'A' && keys.last_ascii <= 'X') {
                const int slot = (keys.last_ascii - 'A') + roster_page_offset_;
                if (slot >= 0 && slot < kPlayableSlots && !mm2_roster_slot_is_empty(&roster_->records[slot])) {
                    sheet_roster_index_ = slot;
                    sheet_slot_letter_ = keys.last_ascii;
                    view_mode_ = ViewMode::CharacterSheet;
                }
                return UiResult::Continue;
            }
            return UiResult::Continue;
        }

        if (keys.ctrl && keys.key_n) {
            return UiResult::Continue;
        }
        if (keys.ctrl && keys.key_d) {
            return UiResult::Continue;
        }
        return UiResult::Continue;
    }

    void renderViewParty(gfx::ScreenCompositor &compositor) override
    {
        compositor.clear(0, 0, 0, 255);
        if (view_mode_ == ViewMode::RosterList) {
            renderRosterList(compositor);
        } else {
            renderCharacterSheet(compositor);
        }
    }

    void beginCreateCharacter(Mm2RosterFile &roster, int slot) override
    {
        roster_ = &roster;
        create_mode_ = CreateMode::SlotPicker;
        if (slot >= 0 && slot < kPlayableSlots) {
            create_slot_ = slot;
        } else {
            create_slot_ = findFirstEmptySlot();
        }
        create_pick_index_ = 0;
    }

    UiResult tickCreateCharacter(const platform::KeyState &keys) override
    {
        if (keys.escape) {
            if (create_mode_ == CreateMode::PendingSteps) {
                create_mode_ = CreateMode::SlotPicker;
                return UiResult::Continue;
            }
            return UiResult::Cancel;
        }

        if (create_mode_ == CreateMode::SlotPicker) {
            if (keys.key_c || keys.up) {
                advanceCreateSlot(-1);
            }
            if (keys.down) {
                advanceCreateSlot(1);
            }
            if (keys.left) {
                create_pick_index_ = (create_pick_index_ + 3) % 4;
            }
            if (keys.right) {
                create_pick_index_ = (create_pick_index_ + 1) % 4;
            }
            if (keys.enter && mm2_roster_slot_is_empty(&roster_->records[create_slot_])) {
                create_mode_ = CreateMode::PendingSteps;
            }
        }
        return UiResult::Continue;
    }

    void renderCreateCharacter(gfx::ScreenCompositor &compositor) override
    {
        using namespace amiga_layout;
        compositor.clear(0, 0, 0, 255);
        drawRedBorder(compositor, kRosterBorderRow, kRosterBorderCol, kRosterBorderW, kRosterBorderH);
        drawCellText(compositor, 4, 4, "CREATE CHARACTER (ASM $944C)");
        char line[48];
        std::snprintf(line, sizeof(line), "Slot %2d [empty=%d]", create_slot_,
                      mm2_roster_slot_is_empty(&roster_->records[create_slot_]) ? 1 : 0);
        drawCellText(compositor, 6, 4, line, 255, 255, 128);
        if (create_mode_ == CreateMode::PendingSteps) {
            drawCellText(compositor, 8, 4, "CREATE — steps not implemented", 255, 200, 128);
        }
        drawCellText(compositor, kRosterFooterRow, kRosterFooterCol, "( 'ESC' to go back )", 180, 180, 180);
    }

    void beginChooseParty(Mm2RosterFile &roster) override
    {
        roster_ = &roster;
        party_town_ = 1;
        party_page_ = PartyPage::Characters;
        party_sub_ = PartySub::List;
        party_count_ = 0;
    }

    UiResult tickChooseParty(const platform::KeyState &keys) override
    {
        using namespace amiga_layout;
        if (party_sub_ == PartySub::Sheet) {
            if (keys.escape || keys.last_ascii == 'Z') {
                party_sub_ = PartySub::List;
            }
            return UiResult::Continue;
        }

        if (keys.escape) {
            return UiResult::Quit;  // "( 'ESC' to exit game )"
        }
        if (keys.last_ascii == 'Z') {
            return UiResult::Cancel;  // "'Z' to exit" → back to title menu
        }
        if (keys.space) {
            party_page_ = (party_page_ == PartyPage::Characters) ? PartyPage::Hirelings : PartyPage::Characters;
            return UiResult::Continue;
        }
        if (keys.last_ascii >= '1' && keys.last_ascii <= '5') {
            party_town_ = keys.last_ascii - '0';
            return UiResult::Continue;
        }
        if (keys.last_ascii >= 'A' && keys.last_ascii <= 'X') {
            int display[kPartySlotCount];
            const int count = buildPartyDisplayList(display);
            const int idx = keys.last_ascii - 'A';
            if (idx < count) {
                const int slot = display[idx];
                if (keys.ctrl) {
                    togglePartyMember(slot);
                } else {
                    sheet_roster_index_ = slot;
                    sheet_slot_letter_ = keys.last_ascii;
                    party_sub_ = PartySub::Sheet;
                }
            }
            return UiResult::Continue;
        }
        return UiResult::Continue;
    }

    void renderChooseParty(gfx::ScreenCompositor &compositor) override
    {
        compositor.clear(0, 0, 0, 255);
        if (party_sub_ == PartySub::Sheet) {
            renderCharacterSheet(compositor);
            return;
        }
        renderPartyChooser(compositor);
    }

private:
    enum class ViewMode { RosterList, CharacterSheet };
    enum class CreateMode { SlotPicker, PendingSteps };
    enum class PartyPage { Characters, Hirelings };
    enum class PartySub { List, Sheet };

    void loadItems()
    {
        has_items_ = false;
        if (!data_dir_) {
            return;
        }
        char path[512];
        if (!joinPath(path, sizeof(path), data_dir_, "items.dat")) {
            return;
        }
        std::size_t size = 0;
        if (!platform::readFile(path, &item_names_, &size)) {
            return;
        }
        if (size < static_cast<std::size_t>(kItemRecordSize * kItemCount)) {
            platform::freeFileBuffer(item_names_);
            item_names_ = nullptr;
            return;
        }
        has_items_ = true;
    }

    void itemName(uint8_t id, char *out, std::size_t out_cap) const
    {
        if (id == 0) {
            out[0] = '\0';
            return;
        }
        if (!has_items_ || id >= kItemCount) {
            std::snprintf(out, out_cap, "#%u", id);
            return;
        }
        const char *src = reinterpret_cast<const char *>(item_names_ + id * kItemRecordSize);
        std::size_t n = 0;
        while (n < kItemNameLen && src[n] != '\0') {
            out[n] = src[n];
            ++n;
        }
        while (n > 0 && out[n - 1] == ' ') {
            --n;
        }
        out[n] = '\0';
        if (n == 0) {
            std::snprintf(out, out_cap, "#%u", id);
        }
    }

    int findFirstEmptySlot() const
    {
        for (int i = 0; i < kPlayableSlots; ++i) {
            if (mm2_roster_slot_is_empty(&roster_->records[i])) {
                return i;
            }
        }
        return 0;
    }

    void advanceCreateSlot(int delta)
    {
        int slot = create_slot_;
        for (int step = 0; step < kPlayableSlots; ++step) {
            slot = (slot + delta + kPlayableSlots) % kPlayableSlots;
            if (mm2_roster_slot_is_empty(&roster_->records[slot])) {
                create_slot_ = slot;
                return;
            }
        }
    }

    void renderRosterList(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;

        drawRedBorder(c, kRosterBorderRow, kRosterBorderCol, kRosterBorderW, kRosterBorderH);

        const char *header = (roster_page_offset_ == 0) ? "(View All)" : "(Hirelings)";
        drawCenteredCellText(c, kRosterViewAllRow, kRosterTextCols, header);
        drawCenteredCellText(c, kRosterTitleRow, kRosterTextCols, "Characters");
        drawCenteredHorizRule(c, kRosterUnderlineRow, kRosterTextCols, 12);

        for (int slot = 0; slot < kRosterSlotCount; ++slot) {
            const int roster_idx = slot + roster_page_offset_;
            const int col = (slot >= kRosterSlotsPerColumn) ? kRosterListColRight : kRosterListColLeft;
            const int row = (slot % kRosterSlotsPerColumn) + kRosterListRowBase;
            const char letter = static_cast<char>('A' + slot);

            char line[40];
            if (roster_idx < 0 || roster_idx >= kPlayableSlots ||
                mm2_roster_slot_is_empty(&roster_->records[roster_idx])) {
                std::snprintf(line, sizeof(line), "%c-", letter);
            } else {
                const Mm2RosterRecord &rec = roster_->records[roster_idx];
                char name[16];
                mm2_roster_name_to_cstr(&rec, name, sizeof(name));
                std::snprintf(line, sizeof(line), "%c- %-10.10s %s/%u", letter, name, classAbbrev(rec.class_id), rec.level);
            }
            drawCellText(c, row, col, line);
        }

        drawCenteredCellText(c, kRosterFooterRow, kRosterTextCols, "'A' - 'X' to View", 180, 180, 180);
        drawCenteredCellText(c, kRosterFooterRow + 1, kRosterTextCols,
                             roster_page_offset_ == 0 ? "'Space' for Hirelings" : "'Space' for Characters", 180,
                             180, 180);
        drawCenteredCellText(c, kRosterFooterRow + 2, kRosterTextCols, "( 'ESC' to go back )", 180, 180, 180);
    }

    void renderCharacterSheet(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;
        if (sheet_roster_index_ < 0 || sheet_roster_index_ >= kPlayableSlots) {
            drawCellText(c, 2, 2, "No character selected.");
            return;
        }

        const Mm2RosterRecord &rec = roster_->records[sheet_roster_index_];
        drawRedBorder(c, kSheetBorderRow, kSheetBorderCol, kSheetBorderW, kSheetBorderH);

        char name[16];
        mm2_roster_name_to_cstr(&rec, name, sizeof(name));

        char header[80];
        std::snprintf(header, sizeof(header), "%c) %s: %s %s %s %s%s", sheet_slot_letter_, name,
                      rec.sex ? "F" : "M", alignHeaderName(rec.alignment_base), raceHeaderName(rec.race),
                      className(rec.class_id), (rec.class_quest_guild_mask & 0x80) ? "+" : "");
                      
        // Clear the background behind the header text so it breaks the top border
        c.fillRect(cellX(kSheetHeaderCol), cellY(kSheetHeaderRow), std::strlen(header) * kCellW, kCellH, 0, 0, 0);
        drawCellText(c, kSheetHeaderRow, kSheetHeaderCol, header);

        char buf[48];
        const int r0 = kSheetStatRowBase;

        std::snprintf(buf, sizeof(buf), "Lvl=%u", rec.level);
        drawCellText(c, r0 + 0, kSheetStatColLeft, buf);
        std::snprintf(buf, sizeof(buf), "HP=%u /%u", rec.hp_current, rec.hp_max);
        drawCellText(c, r0 + 0, kSheetStatColMid, buf);
        std::snprintf(buf, sizeof(buf), "Age=%u", rec.age);
        drawCellText(c, r0 + 0, kSheetStatColRight, buf);

        std::snprintf(buf, sizeof(buf), "Mgt=%u", rec.might_base);
        drawCellText(c, r0 + 1, kSheetStatColLeft, buf);
        std::snprintf(buf, sizeof(buf), "SP=%u /%u", rec.sp_current, rec.sp_max);
        drawCellText(c, r0 + 1, kSheetStatColMid, buf);
        std::snprintf(buf, sizeof(buf), "Exp=%lu", static_cast<unsigned long>(rec.experience));
        drawCellText(c, r0 + 1, kSheetStatColRight, buf);

        std::snprintf(buf, sizeof(buf), "Int=%u", rec.intelligence_base);
        drawCellText(c, r0 + 2, kSheetStatColLeft, buf);
        std::snprintf(buf, sizeof(buf), "AC=%u SL=%u", rec.armor_class, rec.spell_level);
        drawCellText(c, r0 + 2, kSheetStatColMid, buf);
        std::snprintf(buf, sizeof(buf), "Gold=%lu", static_cast<unsigned long>(rec.gold));
        drawCellText(c, r0 + 2, kSheetStatColRight, buf);

        std::snprintf(buf, sizeof(buf), "Per=%u", rec.personality_base);
        drawCellText(c, r0 + 3, kSheetStatColLeft, buf);
        std::snprintf(buf, sizeof(buf), "Thievery %u%%", rec.thievery_percent);
        drawCellText(c, r0 + 3, kSheetStatColMid, buf);
        std::snprintf(buf, sizeof(buf), "Gems=%u", rec.gems);
        drawCellText(c, r0 + 3, kSheetStatColRight, buf);

        std::snprintf(buf, sizeof(buf), "End=%u", rec.endurance_base);
        drawCellText(c, r0 + 4, kSheetStatColLeft, buf);
        std::snprintf(buf, sizeof(buf), "Food=%u", rec.food);
        drawCellText(c, r0 + 4, kSheetStatColRight, buf);

        std::snprintf(buf, sizeof(buf), "Spd=%u", rec.speed_base);
        drawCellText(c, r0 + 5, kSheetStatColLeft, buf);
        std::snprintf(buf, sizeof(buf), "Cond= %s", conditionName(rec.condition));
        drawCellText(c, r0 + 5, kSheetStatColRight, buf);

        std::snprintf(buf, sizeof(buf), "Acy=%u", rec.accuracy_base);
        drawCellText(c, r0 + 6, kSheetStatColLeft, buf);

        std::snprintf(buf, sizeof(buf), "Lck=%u", rec.luck_base);
        drawCellText(c, r0 + 7, kSheetStatColLeft, buf);

        drawSectionHeader(c, kSheetDividerRow, kSheetEquipCol, kSheetBackpackCol - kSheetEquipCol, " Equipped ",
                          200, 200, 200);
        drawSectionHeader(c, kSheetDividerRow, kSheetBackpackCol,
                          kSheetBorderCol + kSheetBorderW - 2 - kSheetBackpackCol + 1, " Backpack ", 200, 200, 200);

        static const char kPackLetters[] = "ABCDEF";
        for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
            const int row = kSheetEquipRowBase + i;
            char iname[20];
            char eline[24];
            itemName(rec.equipped[i].item_id, iname, sizeof(iname));
            if (iname[0]) {
                std::snprintf(eline, sizeof(eline), "%d) %s", i + 1, iname);
            } else {
                std::snprintf(eline, sizeof(eline), "%d)", i + 1);
            }
            drawCellText(c, row, kSheetEquipCol, eline, 220, 220, 220);

            itemName(rec.backpack[i].item_id, iname, sizeof(iname));
            if (iname[0]) {
                std::snprintf(eline, sizeof(eline), "%c) %s", kPackLetters[i], iname);
            } else {
                std::snprintf(eline, sizeof(eline), "%c)", kPackLetters[i]);
            }
            drawCellText(c, row, kSheetBackpackCol, eline, 220, 220, 220);
        }

        drawCellText(c, kSheetFooterRow1, kSheetFooterCol, "( Ctrl )-'N' Re-Name Character", 180, 180, 180);
        drawCellText(c, kSheetFooterRow2, kSheetFooterCol, "( Ctrl )-'D' Delete Character", 180, 180, 180);
        drawCellText(c, kSheetFooterRow3, kSheetFooterCol, "( 'ESC' to go back )", 180, 180, 180);
    }

    // ---- Choose-party helpers ------------------------------------------------

    // A roster slot belongs to the hireling pool when it sits in the second
    // roster page (offset 0x18). Party totals: max 6 characters, max 8 total
    // (so 0 characters allows up to 8 hirelings).
    static bool isHirelingSlot(int slot) { return slot >= amiga_layout::kRosterHirelingPageOffset; }

    // Build the visible A..X list for the current page filtered by home town.
    int buildPartyDisplayList(int *out) const
    {
        using namespace amiga_layout;
        const int start = (party_page_ == PartyPage::Hirelings) ? kRosterHirelingPageOffset : 0;
        const int end = start + kRosterHirelingPageOffset;  // 24 slots per page
        int count = 0;
        for (int slot = start; slot < end && slot < kPlayableSlots && count < kPartySlotCount; ++slot) {
            const Mm2RosterRecord &rec = roster_->records[slot];
            if (mm2_roster_slot_is_empty(&rec)) {
                continue;
            }
            if ((rec.town_flags & 0x7F) != party_town_) {
                continue;
            }
            out[count++] = slot;
        }
        return count;
    }

    bool isPartyMember(int slot) const
    {
        for (int i = 0; i < party_count_; ++i) {
            if (party_members_[i] == slot) {
                return true;
            }
        }
        return false;
    }

    int partyCharacterCount() const
    {
        int n = 0;
        for (int i = 0; i < party_count_; ++i) {
            if (!isHirelingSlot(party_members_[i])) {
                ++n;
            }
        }
        return n;
    }

    void togglePartyMember(int slot)
    {
        for (int i = 0; i < party_count_; ++i) {
            if (party_members_[i] == slot) {
                for (int j = i; j + 1 < party_count_; ++j) {
                    party_members_[j] = party_members_[j + 1];
                }
                --party_count_;
                return;
            }
        }
        if (party_count_ >= kMaxParty) {
            return;  // "Party is Full"
        }
        if (!isHirelingSlot(slot) && partyCharacterCount() >= kMaxCharacters) {
            return;  // max 6 normal characters
        }
        party_members_[party_count_++] = slot;
    }

    void renderPartyChooser(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;

        drawRedBorder(c, kRosterBorderRow, kRosterBorderCol, kRosterBorderW, kRosterBorderH);

        char title[32];
        std::snprintf(title, sizeof(title), "( %d-%s )", party_town_, townName(static_cast<uint8_t>(party_town_)));
        drawCenteredCellText(c, kPartyTitleRow, kPartyTextCols, title);

        drawCellText(c, kPartyOtherTownsRow, kPartyOtherTownsCol, "Other Towns");
        drawCellText(c, kPartyTownKeysRow, kPartyTownKeysCol, "'1' - '5'");

        const char *header = (party_page_ == PartyPage::Hirelings) ? "Hirelings" : "Characters";
        drawCenteredCellText(c, kPartyHeaderRow, kPartyTextCols, header);
        drawCenteredHorizRule(c, kPartyUnderlineRow, kPartyTextCols, 12);

        const int char_count = partyCharacterCount();
        const int hire_count = party_count_ - char_count;
        drawCellText(c, kPartyPartyLabelRow, kPartyRightCol + 2, "PARTY");
        char counts[24];
        std::snprintf(counts, sizeof(counts), "C=%d / H=%d", char_count, hire_count);
        drawCellText(c, kPartyCountRow, kPartyRightCol, counts);

        if (party_count_ >= kMaxParty) {
            const char *full = "*** Party is Full ***";
            const int len = static_cast<int>(std::strlen(full));
            const int col = (kPartyTextCols - len) / 2;
            c.fillRect(cellX(col), cellY(kPartyFullRow), len * kCellW, kCellH, kPartyHiliteR, kPartyHiliteG,
                       kPartyHiliteB);
            drawCellText(c, kPartyFullRow, col, full, 0, 0, 0);
        }

        int display[kPartySlotCount];
        const int count = buildPartyDisplayList(display);

        for (int letter = 0; letter < kPartySlotCount; ++letter) {
            const bool right = letter >= kPartySlotsPerColumn;
            const int row = (letter % kPartySlotsPerColumn) + kPartyListRowBase;
            const int check_col = right ? kPartyListColRightCheck : kPartyListColLeftCheck;
            const int text_col = right ? kPartyListColRight : kPartyListColLeft;
            const char glyph = static_cast<char>('A' + letter);

            if (letter >= count) {
                char line[8];
                std::snprintf(line, sizeof(line), "%c-", glyph);
                drawCellText(c, row, text_col, line, 160, 160, 160);
                continue;
            }

            const int slot = display[letter];
            const Mm2RosterRecord &rec = roster_->records[slot];
            char name[16];
            mm2_roster_name_to_cstr(&rec, name, sizeof(name));
            char line[40];
            std::snprintf(line, sizeof(line), "%c- %-10.10s %s", glyph, name, classAbbrev3(rec.class_id));
            drawCellText(c, row, text_col, line);
            if (isPartyMember(slot)) {
                drawCheckMark(c, row, check_col);
            }
        }

        drawCenteredCellText(c, kPartyFooterViewRow, kPartyTextCols, "'A' - 'X' to View", 200, 200, 200);
        drawCenteredCellText(c, kPartyFooterAddRow, kPartyTextCols, "(Ctrl) 'A' - 'X' to Add/Remove", 200, 200, 200);
        drawCellText(c, kPartyFooterHireRow, kPartyFooterHireCol,
                     (party_page_ == PartyPage::Hirelings) ? "'Space' for Characters" : "'Space' for Hirelings", 200,
                     200, 200);
        drawCellText(c, kPartyFooterHireRow, kPartyFooterExitCol, "'Z' to exit", 200, 200, 200);
        drawCenteredCellText(c, kPartyFooterEscRow, kPartyTextCols, "( 'ESC' to exit game )", 180, 180, 180);
    }

    const char *data_dir_ = nullptr;
    Mm2RosterFile *roster_ = nullptr;

    uint8_t *item_names_ = nullptr;
    bool has_items_ = false;

    ViewMode view_mode_ = ViewMode::RosterList;
    int roster_page_offset_ = 0;
    int sheet_roster_index_ = -1;
    char sheet_slot_letter_ = 'A';

    CreateMode create_mode_ = CreateMode::SlotPicker;
    int create_slot_ = 0;
    int create_pick_index_ = 0;

    PartyPage party_page_ = PartyPage::Characters;
    PartySub party_sub_ = PartySub::List;
    int party_town_ = 1;                  // current home-town filter (1..5)
    int party_members_[kMaxParty] = {0};  // roster indices, in party order (A4-$8696)
    int party_count_ = 0;
};

}  // namespace

std::unique_ptr<ICharacterUi> makeAmigaCharacterUi()
{
    return std::make_unique<AmigaCharacterUi>();
}

}  // namespace mm2::ui
