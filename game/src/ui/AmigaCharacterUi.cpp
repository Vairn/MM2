#include "mm2/ui/ICharacterUi.h"
#include "mm2/Config.h"
#include "mm2/CppStdCompat.h"
#include "mm2/DataPath.h"
#include "mm2/gfx/Mm2FontGlyphs.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"
#include "mm2/ui/CharacterUiFactory.h"
#include "mm2/Mm2Dbg.h"
#include "mm2/platform/Platform.h"
#include "mm2/runtime/PathScratch.h"
#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/mm2_amiga_file.h"
#endif
#include "mm2_create_character.h"
#include "mm2_image32_codec.h"
#include "mm2_party_launch.h"

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

static const char *kRaceNames[] = {"Human", "Elf", "Dwarf", "Gnome", "Half-Orc"};
static const char *kAlignNames[] = {"Good", "Neutral", "Evil"};
static const char *kSexNames[] = {"Male", "Female"};
static const char *kStatKeys[] = {"A", "B", "C", "D", "E", "F", "G"};
static const char *kStatLongNames[] = {"Might", "Intellect", "Personality", "Endurance", "Speed", "Accuracy",
                                       "Luck"};
int statIndexFromKey(char key)
{
    key = static_cast<char>(std::toupper(static_cast<unsigned char>(key)));
    if (key >= 'A' && key <= 'G') {
        return key - 'A';
    }
    return -1;
}

void statValues(const Mm2CreateStats &stats, uint8_t out[MM2_CREATE_STAT_COUNT])
{
    out[0] = stats.might;
    out[1] = stats.intelligence;
    out[2] = stats.personality;
    out[3] = stats.endurance;
    out[4] = stats.speed;
    out[5] = stats.accuracy;
    out[6] = stats.luck;
}

static const char *kClassNames[] = {
    "Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian",
};
static const char *kClassAbbrevs[] = {"K", "P", "A", "C", "S", "R", "N", "B"};
// 3-letter class abbreviations used by the inn party-assembly screen (matches the
// original "Kni/Pal/Arc/Cle/Sor/Rob/Nin/Bar" column in the WinUAE screenshot).
static const char *kClassAbbrev3[] = {"Kni", "Pal", "Arc", "Cle", "Sor", "Rob", "Nin", "Bar"};
// Home-town byte ($0B & 0x7F): 1..5. See EXTRACTED/docs/06-roster-format.md.
static const char *kTownNames[] = {"?", "Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};
static const char *kAlignHeaderNames[] = {
    "Good", "Neut", "Evil",
};

const char *raceHeaderName(uint8_t id) { return id < MM2_CREATE_RACE_COUNT ? kRaceNames[id] : "?"; }
const char *classAbbrev(uint8_t id) { return id < 8 ? kClassAbbrevs[id] : "?"; }
const char *classAbbrev3(uint8_t id) { return id < 8 ? kClassAbbrev3[id] : "?"; }
const char *className(uint8_t id) { return id < 8 ? kClassNames[id] : "?"; }
const char *townName(uint8_t id) { return id >= 1 && id <= 5 ? kTownNames[id] : "?"; }
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

// FAQ save-state note: skill nibbles 1..15 map to these names (alphabetical).
static const char *kSkillNames[] = {
    "",
    "Arms Master",
    "Athlete",
    "Cartographer",
    "Crusader",
    "Diplomat",
    "Gambler",
    "Gladiator",
    "Hero",
    "Linguist",
    "Merchant",
    "Mountaineering",
    "Navigator",
    "Pathfinder",
    "Pickpocket",
    "Soldier",
};

struct HirelingMeta {
    uint32_t base_cost;
    uint8_t start_level;
    const char *skills; // space-separated FAQ abbreviations, or nullptr
};

// FAQ §3-4-1: hireling base cost, starting level, and preset skills (A–X).
static const HirelingMeta kHirelingMeta[] = {
    {2, 1, nullptr},       {2, 1, nullptr},       {2, 1, nullptr},
    {10, 3, "Cru"},        {12, 3, "Car"},        {35, 5, "Lin"},
    {35, 5, "Ath"},        {25, 6, "Arm"},        {55, 6, "Cru"},
    {55, 7, "Mou Pat"},    {45, 7, "Gam Pic"},    {200, 9, "Dip Nav"},
    {250, 9, "Lin Mer"},   {500, 11, "Cru Sol"},  {600, 11, "Arm Pat"},
    {700, 13, "Dip Her"},  {1200, 13, "Cru Mer"},  {2000, 14, "Gla Sol"},
    {2000, 16, "Ath Gam"}, {4000, 15, "Arm Gla"}, {6000, 15, "Arm Mou"},
    {15000, 19, "Dip Gam"}, {25000, 25, "Gam Nav"}, {50000, 21, "Lin Mer"},
};

static const char *skillNameFromId(uint8_t id)
{
    if (id >= 1 && id <= 15) {
        return kSkillNames[id];
    }
    return nullptr;
}

static const char *skillAbbrevToName(const char *abbrev)
{
    if (!abbrev || !abbrev[0]) {
        return nullptr;
    }
    if (std::strncmp(abbrev, "Arm", 3) == 0) {
        return "Arms Master";
    }
    if (std::strncmp(abbrev, "Ath", 3) == 0) {
        return "Athlete";
    }
    if (std::strncmp(abbrev, "Car", 3) == 0) {
        return "Cartographer";
    }
    if (std::strncmp(abbrev, "Cru", 3) == 0) {
        return "Crusader";
    }
    if (std::strncmp(abbrev, "Dip", 3) == 0) {
        return "Diplomat";
    }
    if (std::strncmp(abbrev, "Gam", 3) == 0) {
        return "Gambler";
    }
    if (std::strncmp(abbrev, "Gla", 3) == 0) {
        return "Gladiator";
    }
    if (std::strncmp(abbrev, "Her", 3) == 0) {
        return "Hero";
    }
    if (std::strncmp(abbrev, "Lin", 3) == 0) {
        return "Linguist";
    }
    if (std::strncmp(abbrev, "Mer", 3) == 0) {
        return "Merchant";
    }
    if (std::strncmp(abbrev, "Mou", 3) == 0) {
        return "Mountaineering";
    }
    if (std::strncmp(abbrev, "Nav", 3) == 0) {
        return "Navigator";
    }
    if (std::strncmp(abbrev, "Pat", 3) == 0) {
        return "Pathfinder";
    }
    if (std::strncmp(abbrev, "Pic", 3) == 0) {
        return "Pickpocket";
    }
    if (std::strncmp(abbrev, "Sol", 3) == 0) {
        return "Soldier";
    }
    return nullptr;
}

static bool isRosterSkillTemplate(const Mm2RosterRecord &rec)
{
    const uint8_t a = rec.secondary_skills[0];
    const uint8_t b = rec.secondary_skills[1];
    const uint8_t c = rec.secondary_skills[2];
    if (a != b || b != c) {
        return false;
    }
    return a == 0x05 || a == 0x0A;
}

static int collectRosterSkillNames(const Mm2RosterRecord &rec, const char **names, int max_names)
{
    if (isRosterSkillTemplate(rec)) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < 3 && count < max_names; ++i) {
        const uint8_t byte = rec.secondary_skills[i];
        const uint8_t lo = byte & 0x0F;
        const uint8_t hi = (byte >> 4) & 0x0F;
        for (int n = 0; n < 2; ++n) {
            const uint8_t id = (n == 0) ? lo : hi;
            const char *name = skillNameFromId(id);
            if (!name) {
                continue;
            }
            bool dup = false;
            for (int j = 0; j < count; ++j) {
                if (std::strcmp(names[j], name) == 0) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                names[count++] = name;
            }
        }
    }
    return count;
}

static int collectHirelingSkillNames(int hireling_index, const char **names, int max_names)
{
    if (hireling_index < 0 || hireling_index >= static_cast<int>(sizeof(kHirelingMeta) / sizeof(kHirelingMeta[0]))) {
        return 0;
    }
    const char *skills = kHirelingMeta[hireling_index].skills;
    if (!skills || !skills[0]) {
        return 0;
    }
    int count = 0;
    const char *p = skills;
    while (*p && count < max_names) {
        while (*p == ' ') {
            ++p;
        }
        if (!*p) {
            break;
        }
        const char *start = p;
        while (*p && *p != ' ') {
            ++p;
        }
        char abbrev[8];
        const std::size_t len = static_cast<std::size_t>(p - start);
        if (len >= sizeof(abbrev)) {
            continue;
        }
        ::memcpy(abbrev, start, len);
        abbrev[len] = '\0';
        const char *name = skillAbbrevToName(abbrev);
        if (name) {
            names[count++] = name;
        }
    }
    return count;
}

static uint32_t hirelingDailyCost(int hireling_index, uint8_t level)
{
    if (hireling_index < 0 || hireling_index >= static_cast<int>(sizeof(kHirelingMeta) / sizeof(kHirelingMeta[0]))) {
        return 0;
    }
    const HirelingMeta &meta = kHirelingMeta[hireling_index];
    uint32_t cost = meta.base_cost;
    int gained = static_cast<int>(level) - static_cast<int>(meta.start_level);
    if (gained < 0) {
        gained = 0;
    }
    for (int i = 0; i < gained; ++i) {
        cost += cost / 2;
        if (cost >= 50000u) {
            return 50000u;
        }
    }
    return cost;
}

static uint8_t rosterDisplayThievery(const Mm2RosterRecord &rec)
{
    // Character sheet draw reads roster+$1E (see character_sheet_draw @ $3C42).
    return rec.unknown_1a_20[4];
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

void drawCellDotLeader(gfx::ScreenCompositor &c, int row, int col, int len, uint8_t r = 255, uint8_t g = 255,
                       uint8_t b = 255)
{
    // create stat rows @ $01BD9E: -$7BFC fills with '.' (0x2E) via -$7C62 — no spaces.
    for (int i = 0; i < len; ++i) {
        drawCellGlyph(c, row, col + i, static_cast<uint8_t>('.'), r, g, b);
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

// Section label flanked by font-8 horizontal rules (0x7B), matching character UI red frame (-$809E).
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

int borderBottomRow(int border_row, int border_h) { return border_row + border_h - 1; }
// Label embedded in a console-box border row: black-out the red bar segment, then draw text.
void drawBorderIntegratedText(gfx::ScreenCompositor &c, int row, int border_col, int border_w_cells,
                              const char *text, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255)
{
    using namespace amiga_layout;
    const int len = static_cast<int>(std::strlen(text));
    if (len <= 0 || len > border_w_cells - 2) {
        return;
    }
    const int text_col = border_col + (border_w_cells - len) / 2;
    c.fillRect(cellX(text_col), cellY(row), len * kCellW, kCellH, 0, 0, 0);
    drawCellText(c, row, text_col, text, r, g, b);
}

void drawBorderIntegratedTextAt(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255,
                                uint8_t g = 255, uint8_t b = 255)
{
    using namespace amiga_layout;
    const int len = static_cast<int>(std::strlen(text));
    if (len <= 0) {
        return;
    }
    c.fillRect(cellX(col), cellY(row), len * kCellW, kCellH, 0, 0, 0);
    drawCellText(c, row, col, text, r, g, b);
}

// Two section labels on one horizontal rule row, e.g. "-----( Equipped )-------( Backpack )-----".
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

class AmigaCharacterUi final : public ICharacterUi {
public:
    bool init(const char *data_dir) override
    {
        data_dir_ = data_dir;
        MM2_DBG("MM2 DBG: CharacterUi init\n");
        return true;
    }

    bool loadDataFiles() override
    {
        loadItems();
        return has_items_;
    }

    bool adoptItemNames(uint8_t *data, std::size_t size) override
    {
        if (item_names_) {
            platform::freeFileBuffer(item_names_);
            item_names_ = nullptr;
        }
        item_names_ = data;
        has_items_ = false;
        if (!data) {
            return false;
        }
        if (size < static_cast<std::size_t>(kItemRecordSize * kItemCount)) {
            MM2_DBG("MM2 DBG: items.dat adopt FAIL (size %lu)\n", (unsigned long)size);
            return false;
        }
        has_items_ = true;
        MM2_DBG("MM2 DBG: items.dat adopt ok (%lu bytes)\n", (unsigned long)size);
        return true;
    }

    void prepareCreateCharacterAssets() override { loadThrowAsset(); }

    void shutdown() override
    {
        if (item_names_) {
            platform::freeFileBuffer(item_names_);
            item_names_ = nullptr;
        }
        mm2_image32_free(&throw_);
        has_throw_ = false;
    }

    void requestRedraw() override { frame_dirty_ = true; }

    bool needsRedraw() const override { return frame_dirty_; }

    void ackRedraw() override { frame_dirty_ = false; }

    void beginViewParty(Mm2RosterFile &roster) override
    {
        MM2_DBG("MM2 DBG: CharacterUi ViewParty\n");
        roster_ = &roster;
        view_mode_ = ViewMode::RosterList;
        roster_page_offset_ = 0;
        sheet_roster_index_ = -1;
        sheet_slot_letter_ = 'A';
        sheet_mode_ = SheetMode::View;
        markDirty();
    }

    UiResult tickViewParty(const platform::KeyState &keys) override
    {
        if (view_mode_ == ViewMode::CharacterSheet) {
            return tickCharacterSheet(keys, false);
        }

        if (keys.escape) {
            return UiResult::Cancel;
        }

        if (view_mode_ == ViewMode::RosterList) {
            if (keys.space) {
                roster_page_offset_ = (roster_page_offset_ == 0) ? amiga_layout::kRosterHirelingPageOffset : 0;
                markDirty();
                return UiResult::Continue;
            }
            const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
            if (ch >= 'A' && ch <= 'X') {
                const int slot = (ch - 'A') + roster_page_offset_;
                if (slot >= 0 && slot < kPlayableSlots && isRosterListEntryVisible(slot)) {
                    sheet_roster_index_ = slot;
                    sheet_slot_letter_ = ch;
                    sheet_mode_ = SheetMode::View;
                    view_mode_ = ViewMode::CharacterSheet;
                    markDirty();
                }
                return UiResult::Continue;
            }
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
        MM2_DBG("MM2 DBG: CharacterUi CreateCharacter\n");
        roster_ = &roster;
        if (slot >= 0 && slot < kPlayableSlots && mm2_roster_slot_is_empty(&roster_->records[slot])) {
            create_slot_ = slot;
        } else {
            create_slot_ = findFirstEmptySlot();
        }
        if (create_slot_ < 0 || !mm2_roster_slot_is_empty(&roster_->records[create_slot_])) {
            create_active_ = false;
            return;
        }
        create_active_ = true;
        create_step_ = CreateStep::StatRoll;
        create_exchange_first_ = -1;
        create_rng_ = 0xC0FFEE01u;
        mm2_create_pending_init(&pending_);
        pending_.class_id = -1;
        startThrowAnimation(true);
        markDirty();
    }

    UiResult tickCreateCharacter(const platform::KeyState &keys) override
    {
        if (!create_active_) {
            return UiResult::Cancel;
        }

        if (throw_anim_playing_) {
            tickThrowAnimation();
        } else if (create_step_ == CreateStep::Name) {
            tickCreateNameCursor();
        }

        if (keys.escape) {
            if (create_step_ == CreateStep::StatRoll) {
                return UiResult::Cancel;
            }
            rewindCreateStep(create_step_, pending_);
            create_step_ = previousCreateStep(create_step_);
            create_exchange_first_ = -1;
            markDirty();
            return UiResult::Continue;
        }

        switch (create_step_) {
        case CreateStep::StatRoll:
            return tickCreateStatRoll(keys);
        case CreateStep::Race:
            return tickCreateRace(keys);
        case CreateStep::Alignment:
            return tickCreateAlignment(keys);
        case CreateStep::Sex:
            return tickCreateSex(keys);
        case CreateStep::Name:
            return tickCreateName(keys);
        }
        return UiResult::Continue;
    }

    void renderCreateCharacter(gfx::ScreenCompositor &compositor) override
    {
        compositor.clear(0, 0, 0, 255);
        if (!create_active_) {
            drawCellText(compositor, 4, 4, "No empty roster slots.");
            return;
        }
        renderCreateBase(compositor);
        renderCreateRightPanel(compositor);
    }

    void beginChooseParty(Mm2RosterFile &roster) override
    {
        roster_ = &roster;
        party_town_ = 1;
        party_page_ = PartyPage::Characters;
        party_sub_ = PartySub::List;
        party_count_ = 0;
        has_party_launch_ = false;
        markDirty();
    }

    UiResult tickChooseParty(const platform::KeyState &keys) override
    {
        using namespace amiga_layout;
        if (party_sub_ == PartySub::Sheet) {
            return tickCharacterSheet(keys, true);
        }

        if (keys.escape) {
            return UiResult::Quit;  // "( 'ESC' to exit game )"
        }
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 'Z') {
            // ASM @ 0x0E06: Z with party → spawn at town inn (requires members).
            if (party_count_ <= 0) {
                return UiResult::Continue;
            }
            mm2_party_launch_build(&party_launch_, static_cast<uint8_t>(party_town_), party_members_,
                                   party_count_);
            has_party_launch_ = true;
            return UiResult::Done;
        }
        if (keys.space) {
            party_page_ = (party_page_ == PartyPage::Characters) ? PartyPage::Hirelings : PartyPage::Characters;
            markDirty();
            return UiResult::Continue;
        }
        if (keys.last_ascii >= '1' && keys.last_ascii <= '5') {
            party_town_ = keys.last_ascii - '0';
            markDirty();
            return UiResult::Continue;
        }
        if (ch >= 'A' && ch <= 'X') {
            using namespace amiga_layout;
            const int page_offset =
                (party_page_ == PartyPage::Hirelings) ? kRosterHirelingPageOffset : 0;
            const int slot = rosterSlotForLetter(ch - 'A', page_offset);
            if (!isPartyListEntryVisible(slot)) {
                return UiResult::Continue;
            }
            if (keys.ctrl) {
                togglePartyMember(slot);
                markDirty();
            } else {
                sheet_roster_index_ = slot;
                sheet_slot_letter_ = ch;
                party_sub_ = PartySub::Sheet;
                markDirty();
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

    bool takePartyLaunch(Mm2PartyLaunch *out) override
    {
        if (!has_party_launch_ || !out) {
            return false;
        }
        *out = party_launch_;
        has_party_launch_ = false;
        return true;
    }

private:
    enum class ViewMode { RosterList, CharacterSheet };
    enum class SheetMode { View, Rename };
    enum class CreateStep { StatRoll, Race, Alignment, Sex, Name };
    enum class PartyPage { Characters, Hirelings };
    enum class PartySub { List, Sheet };

    void markDirty() { frame_dirty_ = true; }

    void loadItems()
    {
        has_items_ = false;
        if (!data_dir_) {
            return;
        }
        char *const path = mm2_path_scratch_a();
        if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "items.dat")) {
            return;
        }
        std::size_t size = 0;
#if MM2_HOST_AMIGA
        if (!mm2_amiga_read_file(path, &item_names_, &size)) {
            return;
        }
#else
        if (!platform::readFile(path, &item_names_, &size)) {
            return;
        }
#endif
        if (size < static_cast<std::size_t>(kItemRecordSize * kItemCount)) {
            platform::freeFileBuffer(item_names_);
            item_names_ = nullptr;
            MM2_DBG("MM2 DBG: Loading items.dat FAIL (size %lu)\n", (unsigned long)size);
            return;
        }
        has_items_ = true;
        MM2_DBG("MM2 DBG: Loading items.dat ok (%lu bytes)\n", (unsigned long)size);
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
        return -1;
    }

    static CreateStep previousCreateStep(CreateStep step)
    {
        switch (step) {
        case CreateStep::Race:
            return CreateStep::StatRoll;
        case CreateStep::Alignment:
            return CreateStep::Race;
        case CreateStep::Sex:
            return CreateStep::Alignment;
        case CreateStep::Name:
            return CreateStep::Sex;
        default:
            return CreateStep::StatRoll;
        }
    }

    static void rewindCreateStep(CreateStep step, Mm2PendingCharacter &pending)
    {
        switch (step) {
        case CreateStep::Name:
            pending.name[0] = '\0';
            pending.sex = -1;
            break;
        case CreateStep::Sex:
            pending.sex = -1;
            break;
        case CreateStep::Alignment:
            pending.alignment = -1;
            break;
        case CreateStep::Race:
            pending.race = -1;
            break;
        default:
            break;
        }
    }

    void loadThrowAsset()
    {
        has_throw_ = false;
        mm2_image32_free(&throw_);
        if (!data_dir_) {
            return;
        }
        char *const path = mm2_path_scratch_a();
        if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "throw.32")) {
            MM2_DBG("MM2 DBG: Loading throw.32 FAIL (path)\n");
            return;
        }
        MM2_DBG("MM2 DBG: Loading throw.32 from '%s'\n", path);
        mm2_image32_set_preview_opaque(0);
        if (mm2_image32_load_file(path, &throw_) != MM2_IMAGE32_OK || throw_.frame_count <= 0) {
            mm2_image32_free(&throw_);
            MM2_DBG("MM2 DBG: Loading throw.32 FAIL (decode)\n");
            return;
        }
#if MM2_HOST_AMIGA
        if (!throw_.frames[0].bitmap) {
            mm2_image32_free(&throw_);
            MM2_DBG("MM2 DBG: Loading throw.32 FAIL (no bitmap)\n");
            return;
        }
#else
        if (!throw_.frames[0].rgba) {
            mm2_image32_free(&throw_);
            MM2_DBG("MM2 DBG: Loading throw.32 FAIL (no rgba)\n");
            return;
        }
#endif
        has_throw_ = true;
        MM2_DBG("MM2 DBG: Loading throw.32 ok (%u frames)\n", (unsigned)throw_.frame_count);
    }

    UiResult saveRoster()
    {
        if (!roster_ || !data_dir_) {
            return UiResult::Cancel;
        }
        char *const path = mm2_path_scratch_a();
        if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "roster.dat")) {
            return UiResult::Cancel;
        }
        return (mm2_roster_save_file(path, roster_) == MM2_ROSTER_OK) ? UiResult::Continue : UiResult::Cancel;
    }

    void removeFromParty(int roster_idx)
    {
        for (int i = 0; i < party_count_; ++i) {
            if (party_members_[i] == roster_idx) {
                togglePartyMember(roster_idx);
                return;
            }
        }
    }

    void startSheetRename()
    {
        if (sheet_roster_index_ < 0 || isHirelingSlot(sheet_roster_index_)) {
            return;
        }
        mm2_roster_name_to_cstr(&roster_->records[sheet_roster_index_], rename_buf_, sizeof(rename_buf_));
        sheet_mode_ = SheetMode::Rename;
        markDirty();
    }

    UiResult tickSheetRename(const platform::KeyState &keys)
    {
        if (keys.escape) {
            sheet_mode_ = SheetMode::View;
            markDirty();
            return UiResult::Continue;
        }
        if (keys.backspace) {
            const std::size_t len = std::strlen(rename_buf_);
            if (len > 0) {
                rename_buf_[len - 1] = '\0';
                markDirty();
            }
            return UiResult::Continue;
        }
        if (keys.enter) {
            if (rename_buf_[0] != '\0') {
                mm2_roster_set_name(&roster_->records[sheet_roster_index_], rename_buf_);
                saveRoster();
            }
            sheet_mode_ = SheetMode::View;
            markDirty();
            return UiResult::Continue;
        }
        if ((keys.last_ascii >= 'A' && keys.last_ascii <= 'Z') ||
            (keys.last_ascii >= 'a' && keys.last_ascii <= 'z')) {
            const std::size_t len = std::strlen(rename_buf_);
            if (len < MM2_ROSTER_NAME_SIZE) {
                rename_buf_[len] = keys.last_ascii;
                rename_buf_[len + 1] = '\0';
                markDirty();
            }
        } else if (keys.last_ascii >= '0' && keys.last_ascii <= '9') {
            const std::size_t len = std::strlen(rename_buf_);
            if (len < MM2_ROSTER_NAME_SIZE) {
                rename_buf_[len] = keys.last_ascii;
                rename_buf_[len + 1] = '\0';
                markDirty();
            }
        } else if (keys.space) {
            const std::size_t len = std::strlen(rename_buf_);
            if (len > 0 && len < MM2_ROSTER_NAME_SIZE) {
                rename_buf_[len] = ' ';
                rename_buf_[len + 1] = '\0';
                markDirty();
            }
        }
        return UiResult::Continue;
    }

    void deleteSheetCharacter(bool from_party)
    {
        if (sheet_roster_index_ < 0 || isHirelingSlot(sheet_roster_index_)) {
            return;
        }
        if (mm2_roster_slot_is_empty(&roster_->records[sheet_roster_index_])) {
            return;
        }
        removeFromParty(sheet_roster_index_);
        mm2_roster_clear_record(&roster_->records[sheet_roster_index_]);
        saveRoster();
        sheet_mode_ = SheetMode::View;
        sheet_roster_index_ = -1;
        if (from_party) {
            party_sub_ = PartySub::List;
        } else {
            view_mode_ = ViewMode::RosterList;
        }
    }

    UiResult tickCharacterSheet(const platform::KeyState &keys, bool from_party)
    {
        if (sheet_mode_ == SheetMode::Rename) {
            return tickSheetRename(keys);
        }

        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (keys.escape || (from_party && ch == 'Z')) {
            sheet_mode_ = SheetMode::View;
            if (from_party) {
                party_sub_ = PartySub::List;
            } else {
                view_mode_ = ViewMode::RosterList;
            }
            markDirty();
            return UiResult::Continue;
        }

        if (keys.ctrl && keys.key_n) {
            startSheetRename();
            return UiResult::Continue;
        }
        if (keys.ctrl && keys.key_d) {
            deleteSheetCharacter(from_party);
            return UiResult::Continue;
        }
        return UiResult::Continue;
    }

    UiResult saveCreatedCharacter()
    {
        if (!roster_ || create_slot_ < 0) {
            return UiResult::Cancel;
        }
        mm2_create_build_record(&pending_, &roster_->records[create_slot_]);
        char *const path = mm2_path_scratch_a();
        if (!data_dir_ || !joinDataPath(path, MM2_PATH_SCRATCH_CAP, data_dir_, "roster.dat")) {
            return UiResult::Cancel;
        }
        if (mm2_roster_save_file(path, roster_) != MM2_ROSTER_OK) {
            return UiResult::Cancel;
        }
        create_active_ = false;
        return UiResult::Done;
    }

    UiResult tickCreateStatRoll(const platform::KeyState &keys)
    {
        if (throw_anim_playing_) {
            return UiResult::Continue;
        }
        if (keys.enter) {
            startThrowAnimation(true);
            return UiResult::Continue;
        }
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
        if (ch == 'G') {
            if (create_exchange_first_ < 0) {
                create_exchange_first_ = 6;
            } else {
                mm2_create_swap_stats(&pending_.rolled, create_exchange_first_, 6);
                create_exchange_first_ = -1;
            }
            markDirty();
            return UiResult::Continue;
        }
        const int stat_key = statIndexFromKey(ch);
        if (stat_key >= 0 && stat_key < 6) {
            if (create_exchange_first_ < 0) {
                create_exchange_first_ = stat_key;
            } else {
                mm2_create_swap_stats(&pending_.rolled, create_exchange_first_, stat_key);
                create_exchange_first_ = -1;
            }
            markDirty();
            return UiResult::Continue;
        }
        if (keys.last_ascii >= '1' && keys.last_ascii <= '8') {
            const int class_id = keys.last_ascii - '1';
            if (mm2_create_class_eligible(class_id, &pending_.rolled)) {
                pending_.class_id = static_cast<int8_t>(class_id);
                pending_.race = -1;
                pending_.alignment = -1;
                pending_.sex = -1;
                create_exchange_first_ = -1;
                create_step_ = CreateStep::Race;
                markDirty();
            }
            return UiResult::Continue;
        }
        return UiResult::Continue;
    }

    void startThrowAnimation(bool roll_when_done)
    {
        if (!has_throw_ || throw_.frame_count <= 1) {
            if (roll_when_done) {
                mm2_create_roll_stats(&pending_.rolled, &create_rng_);
                pending_.class_id = -1;
                create_exchange_first_ = -1;
            }
            throw_anim_playing_ = false;
            throw_anim_frame_ = 0;
            throw_anim_step_ = 0;
            return;
        }
        throw_anim_playing_ = true;
        throw_anim_frame_ = amiga_layout::kCreateThrowAnimFrameFirst;
        throw_anim_step_ = 0;
        throw_anim_gate_ = 0;
        throw_roll_when_done_ = roll_when_done;
        markDirty();
    }

    void tickThrowAnimation()
    {
        if (!throw_anim_playing_ || !has_throw_) {
            return;
        }
        if (throw_anim_gate_ < amiga_layout::kCreateThrowAnimStepTicks) {
            ++throw_anim_gate_;
            return;
        }
        throw_anim_gate_ = 0;
        ++throw_anim_step_;
        const int anim_frames = amiga_layout::kCreateThrowAnimFrameCount;
        if (anim_frames <= 0) {
            throw_anim_playing_ = false;
            throw_anim_frame_ = 0;
            return;
        }
        throw_anim_frame_ =
            amiga_layout::kCreateThrowAnimFrameFirst + ((throw_anim_step_ - 1) % anim_frames);
        markDirty();
        if (throw_anim_step_ < amiga_layout::kCreateThrowAnimSteps) {
            return;
        }
        throw_anim_playing_ = false;
        throw_anim_frame_ = amiga_layout::kCreateThrowRestFrame;
        throw_anim_step_ = 0;
        if (throw_roll_when_done_) {
            mm2_create_roll_stats(&pending_.rolled, &create_rng_);
            pending_.class_id = -1;
            create_exchange_first_ = -1;
            throw_roll_when_done_ = false;
        }
        markDirty();
    }

    void tickCreateNameCursor()
    {
        using namespace amiga_layout;
        if (create_name_cursor_gate_ < kCreateNameCursorStepTicks) {
            ++create_name_cursor_gate_;
            return;
        }
        create_name_cursor_gate_ = 0;
        create_name_cursor_frame_ =
            (create_name_cursor_frame_ + 1) % kCreateNameCursorFrameCount;
        markDirty();
    }

    // ---- throw.32 tableau (rewritten from asset + ASM LAB_551A / LAB_5632 / LAB_60DE) ----
    int throwBlitX(int frame_index, int frame_width) const
    {
        using namespace amiga_layout;
        if (frame_index == kCreateThrowRestFrame ||
            frame_index < kCreateThrowAnimRightAnchorFrameFirst) {
            return kCreateThrowTableauX;
        }
        return kCreateThrowBlitCol * kCellW - frame_width;
    }

    void blitThrowFrame(gfx::ScreenCompositor &c, int frame_index) const
    {
        if (!has_throw_ || frame_index < 0 || frame_index >= static_cast<int>(throw_.frame_count)) {
            return;
        }
        const mm2_image32_frame &frame = throw_.frames[frame_index];
        if (!frame.rgba) {
            return;
        }
        using namespace amiga_layout;
        const int x = throwBlitX(frame_index, static_cast<int>(frame.width));
        c.blitRgba(frame.rgba, frame.width, frame.height, x, kCreateThrowBlitY);
    }

    void paintThrowTable(gfx::ScreenCompositor &c) const
    {
        using namespace amiga_layout;
        c.fillRect(kCreateThrowTableauX, kCreateThrowOrangeY, kCreateThrowTableauW, kCreateThrowOrangeH,
                   kCreateThrowOrangeR, kCreateThrowOrangeG, kCreateThrowOrangeB);
    }

    void eraseThrowHandLayer(gfx::ScreenCompositor &c) const
    {
        using namespace amiga_layout;
        c.clearRect(kCreateThrowTableauX, kCreateThrowBlitY, kCreateThrowTableauW, kCreateThrowOrangeRow, 0,
                    0, 0, 255);
    }

    void drawThrowAnimHighlights(gfx::ScreenCompositor &c) const
    {
        using namespace amiga_layout;
        const int cols[3] = {kCreateThrowDieCol0, kCreateThrowDieCol1, kCreateThrowDieCol2};
        const int widths[3] = {kCreateThrowHighlightW0, kCreateThrowHighlightW1, kCreateThrowHighlightW2};
        for (int i = 0; i < 3; ++i) {
            c.fillRect(cols[i] * kCellW, kCreateThrowHighlightY, widths[i], kCreateThrowHighlightH,
                       kCreateThrowHighlightR, kCreateThrowHighlightG, kCreateThrowHighlightB);
        }
    }

    void drawThrowTableauDieValues(gfx::ScreenCompositor &c, const uint8_t vals[6]) const
    {
        using namespace amiga_layout;
        const int cols[3] = {kCreateThrowDieCol0, kCreateThrowDieCol1, kCreateThrowDieCol2};
        char buf[8];
        for (int i = 0; i < 3; ++i) {
            std::snprintf(buf, sizeof(buf), "%u", vals[i]);
            c.drawText(cols[i] * kCellW, kCreateThrowDieRowTop, buf);
        }
        for (int i = 0; i < 3; ++i) {
            std::snprintf(buf, sizeof(buf), "%u", vals[3 + i]);
            c.drawText(cols[i] * kCellW, kCreateThrowDieRowBot, buf);
        }
    }

    void drawThrowAnimDieValues(gfx::ScreenCompositor &c) const
    {
        uint8_t flicker[6];
        for (int i = 0; i < 6; ++i) {
            const uint32_t mix = create_rng_ ^ static_cast<uint32_t>(throw_anim_step_ * 17 + i * 3);
            flicker[i] = static_cast<uint8_t>((mix % 20u) + 1u);
        }
        drawThrowTableauDieValues(c, flicker);
    }

    void renderThrowTableau(gfx::ScreenCompositor &c) const
    {
        if (!has_throw_) {
            return;
        }
        using namespace amiga_layout;
        if (!throw_anim_playing_) {
            // LAB_551A rest path: blit frame 0, print rolled stats on tableau.
            blitThrowFrame(c, kCreateThrowRestFrame);
            uint8_t vals[MM2_CREATE_STAT_COUNT];
            statValues(createDisplayStats(), vals);
            drawThrowTableauDieValues(c, vals);
            return;
        }

        // LAB_5632 anim path: full-width orange table, then right-anchored anim slice only.
        // WinUAE refs (174..205) show frame-0 left fist must NOT stay under anim slices.
        paintThrowTable(c);
        blitThrowFrame(c, throw_anim_frame_);
        drawThrowAnimHighlights(c);
        drawThrowAnimDieValues(c);
    }

    int createProgressLineCount() const
    {
        int n = 0;
        if (create_step_ >= CreateStep::Race && pending_.class_id >= 0) {
            ++n;
        }
        if (create_step_ >= CreateStep::Alignment && pending_.race >= 0) {
            ++n;
        }
        if (create_step_ >= CreateStep::Sex && pending_.alignment >= 0) {
            ++n;
        }
        if (create_step_ >= CreateStep::Name && pending_.sex >= 0) {
            ++n;
        }
        return n;
    }

    void drawCreateProgressLine(gfx::ScreenCompositor &c, int cell_row, const char *label, const char *value)
    {
        using namespace amiga_layout;
        drawCellText(c, cell_row, kCreateProgressLabelCol, label);
        const int value_col = kCreateProgressLabelCol + static_cast<int>(std::strlen(label)) + 1;
        drawCellText(c, cell_row, value_col, value);
    }

    void drawCreateNameLine(gfx::ScreenCompositor &c, int cell_row)
    {
        using namespace amiga_layout;
        constexpr const char *kNameLabel = " Name:";
        drawCellText(c, cell_row, kCreateProgressLabelCol, kNameLabel);
        const int value_col = kCreateProgressLabelCol + static_cast<int>(std::strlen(kNameLabel));
        const std::size_t name_len = std::strlen(pending_.name);
        const int scroll =
            name_len > static_cast<std::size_t>(kCreateNameMaxLen)
                ? static_cast<int>(name_len - static_cast<std::size_t>(kCreateNameMaxLen))
                : 0;
        drawCellText(c, cell_row, value_col, pending_.name + scroll);
        const int cursor_col = value_col + static_cast<int>(name_len - static_cast<std::size_t>(scroll));
        char cursor[2] = {kCreateNameCursorChars[create_name_cursor_frame_], '\0'};
        drawCellText(c, cell_row, cursor_col, cursor);
    }

    void renderCreateProgress(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;
        int row = kCreateStatRowBase;
        if (create_step_ >= CreateStep::Race && pending_.class_id >= 0) {
            drawCreateProgressLine(c, row++, " Class=", kClassNames[pending_.class_id]);
        }
        if (create_step_ >= CreateStep::Alignment && pending_.race >= 0) {
            drawCreateProgressLine(c, row++, " Race=", kRaceNames[pending_.race]);
        }
        if (create_step_ >= CreateStep::Sex && pending_.alignment >= 0) {
            drawCreateProgressLine(c, row++, " Align=", kAlignNames[pending_.alignment]);
        }
        if (create_step_ >= CreateStep::Name && pending_.sex >= 0) {
            drawCreateProgressLine(c, row++, "  Sex=", kSexNames[pending_.sex]);
        }
        if (create_step_ == CreateStep::Name) {
            drawCreateNameLine(c, row);
        }
    }

    void renderCreateFooter(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;
        switch (create_step_) {
        case CreateStep::StatRoll:
            drawCellText(c, kCreatePromptRow1, kCreatePromptCol, "Select Class (1-8)", 180, 180, 180);
            drawCellText(c, kCreatePromptRow2, kCreatePromptCol, "Exchange Stat (A-G) (ENT) to Reroll", 180, 180,
                         180);
            break;
        case CreateStep::Race:
            drawCellText(c, kCreatePromptRow1, kCreatePromptCol, "Select Race (1-5)", 180, 180, 180);
            break;
        case CreateStep::Alignment:
            drawCellText(c, kCreatePromptRow1, kCreatePromptCol, "Select Alignment (1-3)", 180, 180, 180);
            break;
        case CreateStep::Sex:
            drawCellText(c, kCreatePromptRow1, kCreatePromptCol, "Select Sex (1-2)", 180, 180, 180);
            break;
        case CreateStep::Name:
            drawCenteredCellText(c, kCreatePromptRow1, kCreateTextCols,
                                 "Type Name of Character and", 180, 180, 180);
            drawCenteredCellText(c, kCreatePromptRow2, kCreateTextCols, "Press 'Return' to Save", 180, 180,
                                 180);
            break;
        }
        drawBorderIntegratedText(c, borderBottomRow(kCreateBorderRow, kCreateBorderH), kCreateBorderCol, kCreateBorderW,
                                 "( 'ESC' to go back )", 180, 180, 180);
    }

    UiResult tickCreateRace(const platform::KeyState &keys)
    {
        if (keys.last_ascii >= '1' && keys.last_ascii <= '5') {
            pending_.race = static_cast<int8_t>(keys.last_ascii - '1');
            mm2_create_apply_race(pending_.race, &pending_.rolled, &pending_.modified);
            create_step_ = CreateStep::Alignment;
            markDirty();
        }
        return UiResult::Continue;
    }

    UiResult tickCreateAlignment(const platform::KeyState &keys)
    {
        if (keys.last_ascii >= '1' && keys.last_ascii <= '3') {
            pending_.alignment = static_cast<int8_t>(keys.last_ascii - '1');
            create_step_ = CreateStep::Sex;
            markDirty();
        }
        return UiResult::Continue;
    }

    UiResult tickCreateSex(const platform::KeyState &keys)
    {
        if (keys.last_ascii >= '1' && keys.last_ascii <= '2') {
            pending_.sex = static_cast<int8_t>(keys.last_ascii - '1');
            pending_.name[0] = '\0';
            create_name_cursor_frame_ = 0;
            create_name_cursor_gate_ = 0;
            create_step_ = CreateStep::Name;
            markDirty();
        }
        return UiResult::Continue;
    }

    UiResult tickCreateName(const platform::KeyState &keys)
    {
        if (keys.backspace) {
            const std::size_t len = std::strlen(pending_.name);
            if (len > 0) {
                pending_.name[len - 1] = '\0';
            }
            return UiResult::Continue;
        }
        if (keys.enter) {
            if (pending_.name[0] != '\0') {
                return saveCreatedCharacter();
            }
            return UiResult::Continue;
        }
        if ((keys.last_ascii >= 'A' && keys.last_ascii <= 'Z') ||
            (keys.last_ascii >= 'a' && keys.last_ascii <= 'z')) {
            const std::size_t len = std::strlen(pending_.name);
            if (len < static_cast<std::size_t>(amiga_layout::kCreateNameMaxLen)) {
                pending_.name[len] = keys.last_ascii;
                pending_.name[len + 1] = '\0';
            }
        } else if (keys.last_ascii >= '0' && keys.last_ascii <= '9') {
            const std::size_t len = std::strlen(pending_.name);
            if (len < static_cast<std::size_t>(amiga_layout::kCreateNameMaxLen)) {
                pending_.name[len] = keys.last_ascii;
                pending_.name[len + 1] = '\0';
            }
        } else if (keys.space) {
            const std::size_t len = std::strlen(pending_.name);
            if (len > 0 && len < static_cast<std::size_t>(amiga_layout::kCreateNameMaxLen)) {
                pending_.name[len] = ' ';
                pending_.name[len + 1] = '\0';
            }
        }
        return UiResult::Continue;
    }

    const Mm2CreateStats &createDisplayStats() const
    {
        if (create_step_ >= CreateStep::Alignment && pending_.race >= 0) {
            return pending_.modified;
        }
        return pending_.rolled;
    }

    void renderCreateBase(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;
        drawRedBorder(c, kCreateBorderRow, kCreateBorderCol, kCreateBorderW, kCreateBorderH);
        drawBorderIntegratedText(c, kCreateBorderRow, kCreateBorderCol, kCreateBorderW, "( Create New Characters )");
        renderThrowTableau(c);
        uint8_t vals[MM2_CREATE_STAT_COUNT];
        statValues(createDisplayStats(), vals);
        for (int row = 0; row < MM2_CREATE_STAT_COUNT; ++row) {
            const int cell_row = kCreateStatRowBase + row;
            char letter_line[8];
            std::snprintf(letter_line, sizeof(letter_line), "%c -", kStatKeys[row][0]);
            drawCellText(c, cell_row, kCreateStatColLetter, letter_line);
            const bool exchange_pick = create_step_ == CreateStep::StatRoll && create_exchange_first_ == row;
            if (exchange_pick) {
                drawCellText(c, cell_row, kCreateStatColName, kStatLongNames[row], 255, 255, 128);
            } else {
                drawCellText(c, cell_row, kCreateStatColName, kStatLongNames[row]);
            }

            const int dots_col =
                kCreateStatColName + static_cast<int>(std::strlen(kStatLongNames[row]));
            // Last '.' @ col (equals-1); '=' @ kCreateStatColEquals — no blank between them.
            const int dots_len = kCreateStatColEquals - dots_col;
            if (dots_len > 0) {
                drawCellDotLeader(c, cell_row, dots_col, dots_len);
            }
            drawCellGlyph(c, cell_row, kCreateStatColEquals, static_cast<uint8_t>('='));
            drawCellGlyph(c, cell_row, kCreateStatColValueSpace, static_cast<uint8_t>(' '));
            char val_buf[8];
            std::snprintf(val_buf, sizeof(val_buf), "%2u", vals[row]);
            drawCellText(c, cell_row, kCreateStatColValue, val_buf);
        }

        renderCreateFooter(c);
    }

    void drawCreateClassRow(gfx::ScreenCompositor &c, int cell_row, int cls)
    {
        using namespace amiga_layout;
        const bool picked = pending_.class_id == cls;
        const bool eligible = mm2_create_class_eligible(cls, &pending_.rolled);
        const uint8_t r = picked ? 255 : 220;
        const uint8_t g = picked ? 220 : 220;
        const uint8_t b = picked ? 128 : 220;
        if (eligible) {
            char digit[4];
            std::snprintf(digit, sizeof(digit), "%d", cls + 1);
            drawCellText(c, cell_row, kCreateClassDigitCol, digit, r, g, b);
        }
        // Fixed " - " separator on every row; digit only when eligible (WinUAE stat-roll layout).
        drawCellText(c, cell_row, kCreateClassSepCol, " - ", r, g, b);
        drawCellText(c, cell_row, kCreateClassNameCol, kClassNames[cls], r, g, b);
    }

    void drawCreateChoiceList(gfx::ScreenCompositor &c, int first_row, int count, const char *const *labels)
    {
        using namespace amiga_layout;
        char line[24];
        int max_len = 0;
        for (int i = 0; i < count; ++i) {
            std::snprintf(line, sizeof(line), "%d) %s", i + 1, labels[i]);
            const int len = static_cast<int>(std::strlen(line));
            if (len > max_len) {
                max_len = len;
            }
        }
        int col = kCreateRightPanelCol + (kCreateRightPanelWidth - max_len) / 2;
        if (col < kCreateRightPanelCol) {
            col = kCreateRightPanelCol;
        }
        for (int i = 0; i < count; ++i) {
            std::snprintf(line, sizeof(line), "%d) %s", i + 1, labels[i]);
            drawCellText(c, first_row + i, col, line);
        }
    }

    void renderCreateRightPanel(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;
        if (create_step_ == CreateStep::StatRoll) {
            for (int cls = 0; cls < MM2_CREATE_CLASS_COUNT; ++cls) {
                drawCreateClassRow(c, kCreateStatRowBase + cls, cls);
            }
            return;
        }

        renderCreateProgress(c);
        const int choice_base = kCreateStatRowBase + createProgressLineCount() + 1;
        if (create_step_ == CreateStep::Name) {
            return;
        }

        switch (create_step_) {
        case CreateStep::Race:
            drawCreateChoiceList(c, choice_base, MM2_CREATE_RACE_COUNT, kRaceNames);
            break;
        case CreateStep::Alignment:
            drawCreateChoiceList(c, choice_base, MM2_CREATE_ALIGN_COUNT, kAlignNames);
            break;
        case CreateStep::Sex:
            drawCreateChoiceList(c, choice_base, MM2_CREATE_SEX_COUNT, kSexNames);
            break;
        default:
            break;
        }
    }

    void renderRosterList(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;
        drawRedBorder(c, kRosterBorderRow, kRosterBorderCol, kRosterBorderW, kRosterBorderH);
        const char *header = (roster_page_offset_ == 0) ? "(View All)" : "(Hirelings)";
        drawBorderIntegratedText(c, kRosterBorderRow, kRosterBorderCol, kRosterBorderW, header);
        drawCenteredCellText(c, kRosterTitleRow, kRosterTextCols, "Characters");
        drawCenteredHorizRule(c, kRosterUnderlineRow, kRosterTextCols, 12);
        for (int slot = 0; slot < kRosterSlotCount; ++slot) {
            const int roster_idx = slot + roster_page_offset_;
            const int col = (slot >= kRosterSlotsPerColumn) ? kRosterListColRight : kRosterListColLeft;
            const int row = (slot % kRosterSlotsPerColumn) + kRosterListRowBase;
            const char letter = static_cast<char>('A' + slot);
            char line[40];
            if (!isRosterListEntryVisible(roster_idx)) {
                std::snprintf(line, sizeof(line), "%c-", letter);
            } else {
                const Mm2RosterRecord &rec = roster_->records[roster_idx];
                char name[16];
                mm2_roster_name_to_cstr(&rec, name, sizeof(name));
                // Original LAB_470: "A- " + 11-byte space-padded name + " K/level".
                std::snprintf(line, sizeof(line), "%c- %-*s%s/%u", letter, MM2_ROSTER_NAME_SIZE, name,
                              classAbbrev(rec.class_id), rec.level);
            }
            drawCellText(c, row, col, line);
        }

        drawCenteredCellText(c, kRosterFooterRow, kRosterTextCols, "'A' - 'X' to View", 180, 180, 180);
        drawCenteredCellText(c, kRosterFooterRow + 1, kRosterTextCols,
                             roster_page_offset_ == 0 ? "'Space' for Hirelings" : "'Space' for Characters", 180,
                             180, 180);
        drawBorderIntegratedText(c, borderBottomRow(kRosterBorderRow, kRosterBorderH), kRosterBorderCol,
                                 kRosterBorderW, "( 'ESC' to go back )", 180, 180, 180);
    }

    void renderCharacterSheet(gfx::ScreenCompositor &c)
    {
        using namespace amiga_layout;
        if (sheet_roster_index_ < 0 || sheet_roster_index_ >= kPlayableSlots) {
            drawCellText(c, 2, 2, "No character selected.");
            return;
        }

        const Mm2RosterRecord &rec = roster_->records[sheet_roster_index_];
        const bool hireling = isHirelingSlot(sheet_roster_index_);
        const int hireling_index = hireling ? (sheet_roster_index_ - kRosterHirelingPageOffset) : -1;
        drawRedBorder(c, kSheetBorderRow, kSheetBorderCol, kSheetBorderW, kSheetBorderH);
        char name[16];
        mm2_roster_name_to_cstr(&rec, name, sizeof(name));
        char header[80];
        std::snprintf(header, sizeof(header), "%c) %s: %s %s %s %s%s", sheet_slot_letter_, name,
                      rec.sex ? "F" : "M", alignHeaderName(rec.alignment_base), raceHeaderName(rec.race),
                      className(rec.class_id), (rec.class_quest_guild_mask & 0x80) ? "+" : "");
        drawBorderIntegratedTextAt(c, kSheetBorderRow, kSheetHeaderCol, header);
        const char *skill_names[6] = {};
        const int skill_count =
            hireling ? collectHirelingSkillNames(hireling_index, skill_names, 6)
                     : collectRosterSkillNames(rec, skill_names, 6);
        char buf[48];
        const int r0 = kSheetStatRowBase;
        // Left column
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
        // Middle column — HP/SP/AC+SL/thievery/skills/dotted rule (WinUAE layout).
        // Each row: label+current at kSheetStatColMid, companion value at kSheetStatColSlash.
        // Row 2 (Int= row) has no mid/right content — natural gap.
        std::snprintf(buf, sizeof(buf), "HP=%u", rec.hp_current);
        drawCellText(c, r0 + 0, kSheetStatColMid, buf);
        std::snprintf(buf, sizeof(buf), "/%u", rec.hp_max);
        drawCellText(c, r0 + 0, kSheetStatColSlash, buf);
        std::snprintf(buf, sizeof(buf), "SP=%u", rec.sp_current);
        drawCellText(c, r0 + 1, kSheetStatColMid, buf);
        std::snprintf(buf, sizeof(buf), "/%u", rec.sp_max);
        drawCellText(c, r0 + 1, kSheetStatColSlash, buf);
        // r0+2 intentionally empty (aligns with Int= left-only row)
        std::snprintf(buf, sizeof(buf), "AC=%u", rec.armor_class);
        drawCellText(c, r0 + 3, kSheetStatColMid, buf);
        std::snprintf(buf, sizeof(buf), "SL=%u", rec.spell_level);
        drawCellText(c, r0 + 3, kSheetStatColSlash, buf);
        std::snprintf(buf, sizeof(buf), "Thievery %u%%", rosterDisplayThievery(rec));
        drawCellText(c, r0 + 4, kSheetStatColMid, buf);
        static const char kEmptySkillSlot[] = ". . . . . . . . . . . . ";
        if (skill_count >= 1) {
            drawCellText(c, r0 + 5, kSheetStatColMid, skill_names[0]);
        } else {
            drawCellText(c, r0 + 5, kSheetStatColMid, kEmptySkillSlot);
        }

        if (skill_count >= 2) {
            drawCellText(c, r0 + 6, kSheetStatColMid, skill_names[1]);
        } else {
            drawCellText(c, r0 + 6, kSheetStatColMid, kEmptySkillSlot);
        }
        std::snprintf(buf, sizeof(buf), "Cond= %s", conditionName(rec.condition));
        drawCellText(c, r0 + 6, kSheetStatColRight, buf);
        // Right column — Age/Exp stay at kSheetStatColRight (col 26).
        // Cost/Gems/Food sit at kSheetStatColCost (col 24), 2 left of Age/Exp.
        std::snprintf(buf, sizeof(buf), "Age=%u", rec.age);
        drawCellText(c, r0 + 0, kSheetStatColRight, buf);
        std::snprintf(buf, sizeof(buf), "Exp=%lu", static_cast<unsigned long>(rec.experience));
        drawCellText(c, r0 + 1, kSheetStatColRight, buf);
        if (hireling) {
            const uint32_t cost = hirelingDailyCost(hireling_index, rec.level);
            std::snprintf(buf, sizeof(buf), "Cost=%u /Day", cost);
        } else {
            std::snprintf(buf, sizeof(buf), "Gold=%lu", static_cast<unsigned long>(rec.gold));
        }
        drawCellText(c, r0 + 3, kSheetStatColCost, buf);
        std::snprintf(buf, sizeof(buf), "Gems=%u", rec.gems);
        drawCellText(c, r0 + 4, kSheetStatColCost, buf);
        std::snprintf(buf, sizeof(buf), "Food=%u", rec.food);
        drawCellText(c, r0 + 5, kSheetStatColCost, buf);
        const int divider_w = kSheetBorderCol + kSheetBorderW - 2 - kSheetEquipCol + 1;
        drawDoubleSectionHeader(c, kSheetDividerRow, kSheetEquipCol, divider_w, " Equipped ", " Backpack ");
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

        if (sheet_mode_ == SheetMode::Rename) {
            char line[32];
            std::snprintf(line, sizeof(line), "Name: %s_", rename_buf_);
            drawCellText(c, kSheetFooterRow1, kSheetFooterCol, line, 255, 255, 128);
            drawCellText(c, kSheetFooterRow2, kSheetFooterCol,
                           "Type name   BACKSPACE erase   ENTER save   ESC cancel", 180, 180, 180);
        } else if (!hireling) {
            drawCellText(c, kSheetFooterRow1, kSheetFooterCol, "( Ctrl )-'N' Re-Name Character", 180, 180, 180);
            drawCellText(c, kSheetFooterRow2, kSheetFooterCol, "( Ctrl )-'D' Delete Character", 180, 180, 180);
        }
        drawBorderIntegratedText(c, borderBottomRow(kSheetBorderRow, kSheetBorderH), kSheetBorderCol, kSheetBorderW,
                                 "( 'ESC' to go back )", 180, 180, 180);
    }

    // ---- Choose-party helpers ------------------------------------------------
    // A roster slot belongs to the hireling pool when it sits in the second
    // roster page (offset 0x18). Party totals: max 6 characters, max 8 total
    // (so 0 characters allows up to 8 hirelings).
    static bool isHirelingSlot(int slot) { return slot >= amiga_layout::kRosterHirelingPageOffset; }
    // Letter A..X maps 1:1 to roster slots on the current page (0..23 or 24..47).
    static int rosterSlotForLetter(int letter_index, int page_offset)
    {
        if (letter_index < 0 || letter_index >= amiga_layout::kPartySlotCount) {
            return -1;
        }
        return page_offset + letter_index;
    }

    // Hirelings are pre-seeded in roster slots 24..47 but stay hidden until
    // discovered in-game; an unset home-town byte means "not found yet".
    static bool isHirelingDiscovered(const Mm2RosterRecord &rec)
    {
        return (rec.town_flags & 0x7F) != 0;
    }

    // Choose-party list: fixed A..X positions; entry visible when occupied,
    // home town matches the current filter, and (for hirelings) discovered.
    bool isPartyListEntryVisible(int slot) const
    {
        if (slot < 0 || slot >= kPlayableSlots) {
            return false;
        }
        const Mm2RosterRecord &rec = roster_->records[slot];
        if (mm2_roster_slot_is_empty(&rec)) {
            return false;
        }
        if (isHirelingSlot(slot) && !isHirelingDiscovered(rec)) {
            return false;
        }
        return (rec.town_flags & 0x7F) == static_cast<uint8_t>(party_town_);
    }

    bool isRosterListEntryVisible(int roster_idx) const
    {
        if (roster_idx < 0 || roster_idx >= kPlayableSlots) {
            return false;
        }
        const Mm2RosterRecord &rec = roster_->records[roster_idx];
        if (mm2_roster_slot_is_empty(&rec)) {
            return false;
        }
        if (roster_idx >= amiga_layout::kRosterHirelingPageOffset && !isHirelingDiscovered(rec)) {
            return false;
        }
        return true;
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
        drawBorderIntegratedText(c, kRosterBorderRow, kRosterBorderCol, kRosterBorderW, title);
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

        const int page_offset =
            (party_page_ == PartyPage::Hirelings) ? kRosterHirelingPageOffset : 0;
        for (int letter = 0; letter < kPartySlotCount; ++letter) {
            const bool right = letter >= kPartySlotsPerColumn;
            const int row = (letter % kPartySlotsPerColumn) + kPartyListRowBase;
            const int check_col = right ? kPartyListColRightCheck : kPartyListColLeftCheck;
            const int text_col = right ? kPartyListColRight : kPartyListColLeft;
            const char glyph = static_cast<char>('A' + letter);
            const int slot = rosterSlotForLetter(letter, page_offset);
            if (!isPartyListEntryVisible(slot)) {
                char line[8];
                std::snprintf(line, sizeof(line), "%c-", glyph);
                drawCellText(c, row, text_col, line, 160, 160, 160);
                continue;
            }

            const Mm2RosterRecord &rec = roster_->records[slot];
            char name[16];
            mm2_roster_name_to_cstr(&rec, name, sizeof(name));
            char line[48];
            // Inn party list: same 11-char name field, then 3-letter class abbrev.
            std::snprintf(line, sizeof(line), "%c- %-*s%s", glyph, MM2_ROSTER_NAME_SIZE, name,
                          classAbbrev3(rec.class_id));
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
        drawCellText(c, kPartyFooterHireRow, kPartyFooterExitCol, "'Z' to begin", 200, 200, 200);
        drawBorderIntegratedText(c, borderBottomRow(kRosterBorderRow, kRosterBorderH), kRosterBorderCol, kRosterBorderW,
                                 "( 'ESC' to exit game )", 180, 180, 180);
    }

    const char *data_dir_ = nullptr;
    Mm2RosterFile *roster_ = nullptr;
    uint8_t *item_names_ = nullptr;
    bool has_items_ = false;
    ViewMode view_mode_ = ViewMode::RosterList;
    int roster_page_offset_ = 0;
    int sheet_roster_index_ = -1;
    char sheet_slot_letter_ = 'A';
    SheetMode sheet_mode_ = SheetMode::View;
    char rename_buf_[MM2_ROSTER_NAME_SIZE + 1] = {};
    CreateStep create_step_ = CreateStep::StatRoll;
    int create_slot_ = 0;
    bool create_active_ = false;
    int create_exchange_first_ = -1;
    uint32_t create_rng_ = 1;
    Mm2PendingCharacter pending_{};
    mm2_image32_file throw_{};
    bool has_throw_ = false;
    bool throw_anim_playing_ = false;
    bool throw_roll_when_done_ = false;
    int throw_anim_frame_ = 0;
    int throw_anim_step_ = 0;
    int throw_anim_gate_ = 0;
    int create_name_cursor_frame_ = 0;
    int create_name_cursor_gate_ = 0;
    PartyPage party_page_ = PartyPage::Characters;
    PartySub party_sub_ = PartySub::List;
    int party_town_ = 1;                  // current home-town filter (1..5)
    int party_members_[kMaxParty] = {0};  // roster indices, in party order (A4-$8696)
    int party_count_ = 0;
    Mm2PartyLaunch party_launch_{};
    bool has_party_launch_ = false;
    bool frame_dirty_ = true;
};

}  // namespace
std::unique_ptr<ICharacterUi> makeAmigaCharacterUi()
{
    return std::make_unique<AmigaCharacterUi>();
}

#if MM2_HOST_AMIGA
ICharacterUi *acquireAmigaCharacterUi()
{
    static AmigaCharacterUi *s_ui = nullptr;
    if (!s_ui) {
        void *mem = mm2_malloc(sizeof(AmigaCharacterUi));
        if (!mem) {
            return nullptr;
        }
        s_ui = new (mem) AmigaCharacterUi();
    }
    return s_ui;
}
#endif

}  // namespace mm2::ui
