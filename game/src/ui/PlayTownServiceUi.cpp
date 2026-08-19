#include "mm2/ui/PlayTownServiceUi.h"

#include "mm2/CppStdCompat.h"

#include "mm2/gameplay/ExploreActions.h"
#include "mm2/gameplay/SpellBook.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"

#include "mm2_gamestate.h"

namespace mm2::ui {

namespace {

using namespace mm2::gfx::play_layout;
using mm2::ui::amiga_layout::cellX;
using mm2::ui::amiga_layout::cellY;

/* Bartman m68k: uint32_t is unsigned long; %u wants unsigned int. */
template <typename T>
constexpr unsigned u32(T v)
{
    return static_cast<unsigned>(v);
}

/* Town shop chrome @ asm 0x1C494 (left col 2) + option captions @ 0x1C6BE.
 * Clear preset 2: cells (1,17)-(38,22). ESC prompt @ 0x6DA6 row 23 col 11. */
constexpr int kLeftCol = 0x02;
constexpr int kOptCol = 0x10;        /* generic option band (smith/tavern) */
constexpr int kTempleOptCol = 0x13;  /* temple captions @ 0x1DE4E */
constexpr int kGuildOptCol = 0x14;   /* guild captions @ 0x1E5E8 */
constexpr int kSpellIdCol = 0x1F;    /* L-N id print @ 0x1DAC6 / 0x1D97A */
constexpr int kCostCol = 0x23;       /* cost / "---" @ 0x1DF30 / 0x1E660 */
constexpr int kBandRowFirst = 0x11;
constexpr int kBandRowLast = 0x16;
constexpr int kEscPromptCol = 0x0B;
constexpr int kEscPromptRow = 0x17;

/* A4-$56E6: 7 str.dat ptrs loaded @ 0x1E962 (after -$56F6 chrome).
 * Temple paints [0..5] @ 0x1DE60 after 0x1E2AE patches [3..5] to D/E/F + "Spell C".
 * Guild paints [3..6] @ 0x1E5FA after 0x1E43C patches [3..5] first-byte to A/B/C
 * and byte 9 to 'S' ("Spell S"); [6] is already "D) Spell S". */
constexpr const char *kTownSpellMenuCaptions[7] = {
    "A) Restore Cond", "B) Restore Algn", "C) Donations", "D) Spell C",
    "E) Spell C",      "F) Spell C",      "D) Spell S",
};

void clearShopMenuBand(gfx::ScreenCompositor &c)
{
    gfx::fillCellRect(c, 1, kBandRowFirst, 38, kBandRowLast - kBandRowFirst + 1);
}

void drawCell(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255,
              uint8_t g = 255, uint8_t b = 255)
{
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

void drawEscFooter(gfx::ScreenCompositor &c)
{
    /* prompt_esc @ 0x6DA6 — "( 'ESC' to go back )" at (11,23), pen 1. */
    drawCell(c, kEscPromptRow, kEscPromptCol, "( 'ESC' to go back )");
}

void fillSpellBuyStatus(char *status, size_t cap, const char *name, const char *spell_name,
                        const mm2::events::TownSvcSpellResult &r, uint32_t offer)
{
    if (r.learned) {
        std::snprintf(status, cap, "%s learned %s for %u gp.", name, spell_name, u32(r.cost));
    } else if (r.reject == mm2::events::TownSvcSpellReject::NotEnoughGold) {
        std::snprintf(status, cap, "%s: not enough gold (%u gp).", name, u32(offer));
    } else if (r.reject == mm2::events::TownSvcSpellReject::Condition) {
        std::snprintf(status, cap, "%s is afflicted - cannot buy.", name);
    } else {
        std::snprintf(status, cap, "Not for sale.");
    }
}

void formatPubPair(char *status, size_t cap, const char *a, const char *c, const char *empty)
{
    if (a && c && a[0] && a[0] != '(' && c[0] && c[0] != '(') {
        std::snprintf(status, cap, "%s\n%s", a, c);
    } else if (a && a[0] && a[0] != '(') {
        std::snprintf(status, cap, "%s", a);
    } else if (empty) {
        std::snprintf(status, cap, "%s", empty);
    } else {
        status[0] = '\0';
    }
}

/* XP gate that arms 'T' @ 0x20BD0 (tst -$8(a5) from the 0x20AD2 scc). */
bool trainingXpEligible(const mm2::events::TownServiceContext &ctx, int slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx, slot);
    if (!rec) {
        return false;
    }
    const int next_level = static_cast<int>(rec->level) + 1;
    const uint32_t threshold = mm2_class_xp_for_level(rec->class_id, next_level);
    return threshold != 0 && threshold != 0xffffffffu && rec->experience >= threshold;
}

/* Offer-gate id print (0x1DAC6 / 0x1D97A): ASCII level, '-', number at col $1F. */
void formatSpellIdLn(char *out, size_t out_n, mm2::gameplay::SpellSchool school, int spell_index)
{
    const mm2::gameplay::SpellMeta *table = mm2::gameplay::schoolSpellTable(school);
    if (!table || spell_index < 0 || spell_index >= mm2::gameplay::kSpellsPerSchool) {
        out[0] = '\0';
        return;
    }
    const mm2::gameplay::SpellMeta &meta = table[spell_index];
    std::snprintf(out, out_n, "%u-%u", static_cast<unsigned>(meta.level),
                  static_cast<unsigned>(meta.number));
}

void formatCostColumn(char *out, size_t out_n, uint32_t offer_gold)
{
    /* Zero offer → literal "---" @ 0x1E111; else print gold via -$7E5A. */
    if (offer_gold == 0) {
        std::snprintf(out, out_n, "---");
    } else {
        std::snprintf(out, out_n, "%u", u32(offer_gold));
    }
}

/** Inverse-video service title (0x1C494: -$7C08(1) around "Blacksmith"). */
void drawInverseTitle(gfx::ScreenCompositor &c, int row, int col, const char *title)
{
    if (!title || title[0] == '\0') {
        return;
    }
    const int px = cellX(col);
    const int py = cellY(row);
    const int w = static_cast<int>(std::strlen(title)) * 8;
    c.fillRect(px, py, w, 8, 255, 255, 255, 255);
    c.drawText(px, py, title, 0, 0, 0, 255);
}

void drawShopLeftChrome(gfx::ScreenCompositor &c, const char *title, const char *select_line,
                        const mm2::events::TownServiceContext &ctx, int active_member,
                        bool show_gather_gold, bool hireling_training_cost_label)
{
    drawInverseTitle(c, kBandRowFirst, kLeftCol, title);

    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx, active_member);
    if (rec) {
        char name[20];
        mm2_roster_name_to_cstr(rec, name, sizeof(name));
        char member_line[32];
        std::snprintf(member_line, sizeof(member_line), "%d) %s", active_member + 1, name);
        drawCell(c, kBandRowFirst + 1, kLeftCol, member_line);
        char gold_line[24];
        /* Training hireling chrome @ 0x206D4 prints "Cost=" (daily fee in +$66). */
        std::snprintf(gold_line, sizeof(gold_line), "%s%u",
                      hireling_training_cost_label ? "Cost=" : "Gold=",
                      static_cast<unsigned>(rec->gold));
        drawCell(c, kBandRowFirst + 2, kLeftCol, gold_line);
    }

    int foot_row = kBandRowFirst + 3;
    if (show_gather_gold) {
        drawCell(c, foot_row++, kLeftCol, "G-Gather Gold");
    }
    drawCell(c, foot_row++, kLeftCol, "#-Other Char");
    drawCell(c, foot_row, kLeftCol, select_line);
}

void cycleActiveMember(int &slot, const mm2::events::TownServiceContext &ctx, bool skip_hirelings)
{
    if (!ctx.launch) {
        return;
    }
    const int n = ctx.launch->party_count;
    if (n <= 0) {
        return;
    }
    for (int step = 1; step <= n; ++step) {
        const int next = (slot + step) % n;
        if (!mm2::events::townSvcMemberRecord(ctx, next)) {
            continue;
        }
        if (skip_hirelings && mm2::events::townSvcPartySlotIsHireling(ctx, next)) {
            continue;
        }
        slot = next;
        return;
    }
}

/** ASM shop member index @ A4-$5A3A: digits 1..8 pick the displayed party slot. */
bool trySelectMemberByDigit(int &slot, const mm2::events::TownServiceContext &ctx, char ch,
                            bool reject_hirelings)
{
    if (ch < '1' || ch > '8') {
        return false;
    }
    const int party_slot = ch - '1';
    if (!mm2::events::townSvcMemberRecord(ctx, party_slot)) {
        return false;
    }
    if (reject_hirelings && mm2::events::townSvcPartySlotIsHireling(ctx, party_slot)) {
        return false; /* smith 0x1C39C / guild 0x1E840 / tavern 0x1D636 */
    }
    slot = party_slot;
    return true;
}

/* Word-wrap at col 39 (320/8). Returns the row after the last line drawn. */
int drawWrappedLine(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r, uint8_t g,
                    uint8_t b)
{
    constexpr int kScreenCols = gfx::ScreenCompositor::kWidth / 8;
    const int max_w = kScreenCols - 1 - col;
    if (!text || max_w <= 0) {
        return row;
    }
    if (*text == '\0') {
        drawCell(c, row++, col, "", r, g, b);
        return row;
    }
    while (*text) {
        int take = static_cast<int>(std::strlen(text));
        if (take > max_w) {
            take = max_w;
            while (take > 0 && text[take] != ' ') {
                --take;
            }
            if (take == 0) {
                take = max_w; /* single word longer than the line: hard-break it */
            }
        }
        char buf[80];
        const std::size_t copy =
            static_cast<std::size_t>(take) < sizeof(buf) - 1 ? static_cast<std::size_t>(take) : sizeof(buf) - 1;
        __builtin_memcpy(buf, text, copy);
        buf[copy] = '\0';
        drawCell(c, row++, col, buf, r, g, b);
        text += take;
        while (*text == ' ') {
            ++text;
        }
    }
    return row;
}

/* str.dat line with embedded `\n` (pub food names); each segment word-wrapped. */
int drawMultiline(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255,
                    uint8_t g = 255, uint8_t b = 255)
{
    if (!text) {
        return row;
    }
    const char *p = text;
    while (*p) {
        const char *nl = ::strchr(p, '\n');
        char buf[200];
        const std::size_t n = nl ? static_cast<std::size_t>(nl - p) : std::strlen(p);
        const std::size_t copy = (n < sizeof(buf) - 1) ? n : sizeof(buf) - 1;
        __builtin_memcpy(buf, p, copy);
        buf[copy] = '\0';
        row = drawWrappedLine(c, row, col, buf, r, g, b);
        if (!nl) {
            break;
        }
        p = nl + 1;
    }
    return row;
}

}  // namespace

bool PlayTownServiceUi::chooseTempleOption(const mm2::events::TownServiceContext &ctx,
                                           mm2::events::TempleOption &, int &)
{
    ctx_ = ctx;
    kind_ = Kind::Temple;
    pending_ = true;
    mm2_temple_spell_stock(ctx_.map_id, temple_spell_stock_);
    return false; /* defer to the multi-frame overlay; exit the blocking driver */
}

bool PlayTownServiceUi::chooseMageGuildSpell(const mm2::events::TownServiceContext &ctx,
                                             const Mm2SpellShopSlot[MM2_MAGE_GUILD_SLOTS], int &)
{
    ctx_ = ctx;
    kind_ = Kind::MageGuild;
    pending_ = true;
    guild_denied_ = false;
    buildGuildStock();
    return false;
}

void PlayTownServiceUi::reportGuildNotMember(const mm2::events::TownServiceContext &ctx)
{
    ctx_ = ctx;
    kind_ = Kind::MageGuild;
    pending_ = true;
    guild_denied_ = true;
}

bool PlayTownServiceUi::chooseTrainingOption(const mm2::events::TownServiceContext &ctx,
                                             mm2::events::TrainingOption &, int &)
{
    ctx_ = ctx;
    kind_ = Kind::Training;
    pending_ = true;
    return false;
}

bool PlayTownServiceUi::chooseSmithCategory(const mm2::events::TownServiceContext &ctx, int &)
{
    ctx_ = ctx;
    kind_ = Kind::Smith;
    pending_ = true;
    return false;
}

bool PlayTownServiceUi::chooseTavernOption(const mm2::events::TownServiceContext &ctx,
                                           const mm2::events::TavernMenuData &data,
                                           mm2::events::TavernOption &)
{
    ctx_ = ctx;
    kind_ = Kind::Tavern;
    tavern_data_ = data;
    pending_ = true;
    return false; /* defer to the multi-frame overlay; exit the blocking driver */
}

void PlayTownServiceUi::begin()
{
    if (!pending_) {
        return;
    }
    pending_ = false;
    active_ = true;
    /* Training has no option menu (level-up only) -> open directly on the trainee
     * prompt; the mage guild's 0x1E410 gate may deny the whole party up front
     * (Denied); temple/smith/guild otherwise open on their option menu.
     * Tavern opens on the A-E top menu. */
    phase_ = (kind_ == Kind::MageGuild && guild_denied_) ? Phase::Denied : Phase::Menu;
    smith_slot_ = -1;
    smith_mode_ = SmithMode::Buy;
    smith_identify_pending_ = false;
    result_return_phase_ = Phase::Menu;
    temple_spell_slot_ = -1;
    guild_slot_ = -1;
    /* ASM open skip: first non-hireling for temple/smith/guild/tavern; training
     * keeps slot 0 (hirelings may train). */
    active_member_ = 0;
    if (kind_ != Kind::Training) {
        const int first = mm2::events::townSvcFirstNonHirelingSlot(ctx_);
        if (first >= 0) {
            active_member_ = first;
        }
    }
    status_[0] = '\0';
    hireling_heal_msg_[0] = '\0';
    training_feedback_ = TrainingFeedback::None;
    training_hp_gain_ = 0;
    training_gained_spells_ = false;
    training_max_summary_ = false;
    training_levels_gained_ = 0;
    training_spell_levels_gained_ = 0;
    if (kind_ == Kind::Temple) {
        mm2_temple_spell_stock(ctx_.map_id, temple_spell_stock_);
        /* Outer temple loop @ 0x1E396: hireling slot opens free leaf 0x1E116. */
        if (mm2::events::townSvcPartySlotIsHireling(ctx_, active_member_)) {
            applyHirelingTempleAutoHeal(active_member_);
        }
    } else if (kind_ == Kind::MageGuild && !guild_denied_) {
        buildGuildStock();
    }
}

void PlayTownServiceUi::close()
{
    active_ = false;
    pending_ = false;
    kind_ = Kind::None;
    phase_ = Phase::Menu;
    guild_denied_ = false;
    tavern_opt_ = mm2::events::TavernOption::Exit;
    tavern_sub_sel_ = -1;
    tavern_tipped_ = false;
    tavern_rumor_idx_ = 0;
    tavern_tip_idx_ = 0;
    active_member_ = 0;
    smith_mode_ = SmithMode::Buy;
    smith_identify_pending_ = false;
    result_return_phase_ = Phase::Menu;
    status_[0] = '\0';
    hireling_heal_msg_[0] = '\0';
    training_feedback_ = TrainingFeedback::None;
    training_hp_gain_ = 0;
    training_gained_spells_ = false;
    training_max_summary_ = false;
    training_levels_gained_ = 0;
    training_spell_levels_gained_ = 0;
}

const char *PlayTownServiceUi::serviceTitle() const
{
    switch (kind_) {
    case Kind::Smith:
        return "Blacksmith";
    case Kind::Temple:
        return "Temple";
    case Kind::Tavern:
        return "Tavern";
    case Kind::MageGuild:
        return "Mage Guild";
    case Kind::Training:
        return "Training";
    default:
        return "";
    }
}

const char *PlayTownServiceUi::selectPromptText() const
{
    switch (kind_) {
    case Kind::Temple:
        return "Select (A-G)";
    case Kind::Tavern:
        return "Select (A-E)";
    case Kind::MageGuild:
        return "Learn (A-D)";
    case Kind::Training:
        /* Training hall @ 0x020988: left chrome is member/gold + "#-Other Char"
         * only; the level-up prompt lives in the right column (0x20c4c+). */
        return "";
    default:
        return "Select (A-F)";
    }
}

bool PlayTownServiceUi::showGatherGoldLine() const
{
    /* Training skips G for hirelings @ 0x20BBA; temple/smith always show it. */
    if (kind_ == Kind::Training) {
        return !mm2::events::townSvcPartySlotIsHireling(ctx_, active_member_);
    }
    return kind_ == Kind::Temple || kind_ == Kind::Smith;
}

void PlayTownServiceUi::showActiveMemberGold()
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec) {
        return;
    }
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    std::snprintf(status_, sizeof(status_), "%s: %u gold", name,
                  static_cast<unsigned>(rec->gold));
    holdShopResult(phase_);
}

void PlayTownServiceUi::holdShopResult(Phase return_to)
{
    result_return_phase_ = return_to;
    phase_ = Phase::ShopResult;
}

void PlayTownServiceUi::applyHirelingTempleAutoHeal(int party_slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec) {
        return;
    }
    const mm2::events::TownSvcHirelingTempleResult r =
        mm2::events::townSvcHirelingTempleAutoHeal(*rec);
    /* Exact exe strings @ 0x1E26A / 0x1E27B (pea targets from 0x1E1E4 / 0x1E1F0). */
    std::snprintf(hireling_heal_msg_, sizeof(hireling_heal_msg_), "%s",
                  r.any_change ? "has been healed." : "  is healthy.");
    status_[0] = '\0';
    phase_ = Phase::HirelingTemple;
}

void PlayTownServiceUi::enterTempleMemberView(int party_slot)
{
    if (mm2::events::townSvcPartySlotIsHireling(ctx_, party_slot)) {
        applyHirelingTempleAutoHeal(party_slot);
    } else {
        hireling_heal_msg_[0] = '\0';
        phase_ = Phase::Menu;
    }
}

const char *PlayTownServiceUi::templeHirelingHealMessage() const
{
    return templeHirelingHealView() ? hireling_heal_msg_ : "";
}

void PlayTownServiceUi::drawHirelingTempleHeal(gfx::ScreenCompositor &c) const
{
    /* Hireling free leaf paint @ 0x1E116 — no left chrome, no A–F captions/costs.
     * Cursor convention matches paid temple: -$7bfc(row, col). */
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (rec) {
        char name[20];
        mm2_roster_name_to_cstr(rec, name, sizeof(name));
        char member_line[32];
        /* 0x1E14E..0x1E176: digit+$31, ')', ' ', name @ (row $12, col $0D). */
        std::snprintf(member_line, sizeof(member_line), "%d) %s", active_member_ + 1, name);
        drawCell(c, 0x12, 0x0D, member_line);
    }

    /* 0x1E1C4: cursor (row $13, col $5); heal/healthy then "  Select another" (0x1E289). */
    char line[48];
    std::snprintf(line, sizeof(line), "%s  Select another", hireling_heal_msg_);
    drawCell(c, 0x13, 0x05, line);
    /* 0x1E204: cursor (row $14, col $11) + "character." @ 0x1E29A. */
    drawCell(c, 0x14, 0x11, "character.");
}

void PlayTownServiceUi::drawLeftChrome(gfx::ScreenCompositor &c) const
{
    const char *prompt = selectPromptText();
    if (phase_ == Phase::SmithItems) {
        prompt = "Select (A-F)";
    }
    const bool hireling_cost =
        kind_ == Kind::Training && mm2::events::townSvcPartySlotIsHireling(ctx_, active_member_);
    drawShopLeftChrome(c, serviceTitle(), prompt, ctx_, active_member_, showGatherGoldLine(),
                       hireling_cost);
}

/** Training hall trainee panel (ASM 0x020988 / 0x20612, strings @ 0x20c4c+). */
void PlayTownServiceUi::drawTrainingPrompt(gfx::ScreenCompositor &c) const
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec) {
        return;
    }

    const int next_level = static_cast<int>(rec->level) + 1;
    const uint32_t threshold = mm2_class_xp_for_level(rec->class_id, next_level);
    const bool eligible = trainingXpEligible(ctx_, active_member_);
    const int ridx = mm2::events::townSvcRosterIndex(ctx_, active_member_);
    const uint32_t fee = mm2::events::townSvcTrainingCost(rec->level, ctx_.map_id, ridx);

    char line[48];
    std::snprintf(line, sizeof(line), "Train for level %d", next_level);
    drawCell(c, kBandRowFirst, kOptCol, line);

    if (eligible) {
        if (fee == 0) {
            drawCell(c, kBandRowFirst + 1, kOptCol, "Cost in gold = free");
        } else {
            std::snprintf(line, sizeof(line), "Cost in gold = %u", u32(fee));
            drawCell(c, kBandRowFirst + 1, kOptCol, line);
        }
        drawCell(c, kBandRowFirst + 2, kOptCol, "Press 'T' to train");
        drawCell(c, kBandRowFirst + 3, kOptCol, "Press 'M' to max");
    } else if (threshold == 0 || threshold == 0xffffffffu) {
        drawCell(c, kBandRowFirst + 1, kOptCol, " to level up.");
        drawCell(c, kBandRowFirst + 2, kOptCol, "You need more experience.");
    } else {
        /* Remaining XP = next-level threshold − current (not the absolute threshold). */
        const uint32_t remaining =
            (rec->experience < threshold) ? (threshold - rec->experience) : 0u;
        drawCell(c, kBandRowFirst + 1, kOptCol, " to level up.");
        std::snprintf(line, sizeof(line), "Need %u XP", static_cast<unsigned>(remaining));
        drawCell(c, kBandRowFirst + 2, kOptCol, line);
    }
}

/** Train result panel @ 0x204BA..0x20518 (and the 0x20278 / 0x202EE rejects).
 *  0x2023A clear_rect_preset(7) has already wiped (15,17)-(38,22), so this
 *  replaces the trainee prompt rather than overlaying it. */
void PlayTownServiceUi::drawTrainingResult(gfx::ScreenCompositor &c) const
{
    switch (training_feedback_) {
    case TrainingFeedback::Gained: {
        char line[32];
        if (training_max_summary_) {
            /* Remake 'M': "You gained N levels / X spell levels / and Y hit points". */
            std::snprintf(line, sizeof(line), "You gained %u level%s",
                          static_cast<unsigned>(training_levels_gained_),
                          training_levels_gained_ == 1 ? "" : "s");
            drawCell(c, 0x13, 0x12, line);
            int row = 0x14;
            if (training_spell_levels_gained_ > 0) {
                std::snprintf(line, sizeof(line), "%u spell level%s",
                              static_cast<unsigned>(training_spell_levels_gained_),
                              training_spell_levels_gained_ == 1 ? "" : "s");
                drawCell(c, row++, 0x12, line);
            }
            std::snprintf(line, sizeof(line), "and %u hit points",
                          static_cast<unsigned>(training_hp_gain_));
            drawCell(c, row, 0x12, line);
            break;
        }
        /* Cursor (col $14, row $13) "You gained " + N; (col $14, row $14)
         * " hit points"; optional (col $14, row $15) "and new spells". */
        std::snprintf(line, sizeof(line), "You gained %u",
                      static_cast<unsigned>(training_hp_gain_));
        drawCell(c, 0x13, 0x14, line);
        drawCell(c, 0x14, 0x14, " hit points");
        if (training_gained_spells_) {
            drawCell(c, 0x15, 0x14, "and new spells");
        }
        break;
    }
    case TrainingFeedback::NeedGold:
        /* 0x202EE: (col $12, row $13) / (col $12, row $14). */
        drawCell(c, 0x13, 0x12, "Sorry - you need");
        drawCell(c, 0x14, 0x12, "more gold.");
        break;
    case TrainingFeedback::NotWell:
        /* 0x20278: condition $26 != 0. */
        drawCell(c, 0x13, 0x12, "You have to be well");
        drawCell(c, 0x14, 0x12, "to train here.");
        break;
    case TrainingFeedback::None:
        break;
    }
}

void PlayTownServiceUi::dismissTrainingResult()
{
    phase_ = Phase::Menu;
    training_feedback_ = TrainingFeedback::None;
    training_hp_gain_ = 0;
    training_gained_spells_ = false;
    training_max_summary_ = false;
    training_levels_gained_ = 0;
    training_spell_levels_gained_ = 0;
    status_[0] = '\0';
}

void PlayTownServiceUi::buildGuildStock()
{
    for (int i = 0; i < MM2_MAGE_GUILD_SLOTS; ++i) {
        guild_stock_[i] = Mm2SpellShopSlot{};
    }
    mm2_mage_guild_stock(ctx_.map_id, guild_stock_);
}

void PlayTownServiceUi::buildSmithView()
{
    for (int i = 0; i < MM2_SMITH_SLOTS; ++i) {
        smith_view_[i] = mm2::events::SmithItemView{};
    }
    Mm2SmithSlot slots[MM2_SMITH_SLOTS];
    if (!mm2_smith_inventory(ctx_.map_id, smith_category_, slots)) {
        return;
    }
    /* Category 1 = Today's Specials (ASM mode 2 @ 0x1C13E): date-roll bonus. */
    const uint8_t specials_bonus =
        (smith_category_ == 1) ? mm2::events::townSvcSmithSpecialsDateBonus(ctx_.a4) : 0;
    for (int i = 0; i < MM2_SMITH_SLOTS; ++i) {
        uint8_t price_meta = 0, charges = 0, flags = 0;
        mm2_smith_buy_fields(smith_category_, slots[i].meta, &price_meta, &charges, &flags);
        if (smith_category_ == 1) {
            price_meta = specials_bonus;
            flags = specials_bonus;
        }
        smith_view_[i].item_id = slots[i].item_id;
        smith_view_[i].bonus = flags;
        smith_view_[i].charges = charges;
        if (slots[i].item_id != 0 && ctx_.items) {
            const Mm2ItemRecord *rec = mm2_items_lookup(ctx_.items, slots[i].item_id);
            const uint32_t base = rec ? rec->gold : 0u;
            smith_view_[i].price = mm2_smith_price(base, price_meta);
        }
    }
}

void PlayTownServiceUi::buildSmithBackpackView()
{
    for (int i = 0; i < MM2_SMITH_SLOTS; ++i) {
        smith_view_[i] = mm2::events::SmithItemView{};
    }
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec) {
        return;
    }
    for (int i = 0; i < MM2_SMITH_SLOTS; ++i) {
        const Mm2RosterItemSlot slot = mm2_roster_backpack(rec, i);
        smith_view_[i].item_id = slot.item_id;
        smith_view_[i].bonus = slot.flags;
        smith_view_[i].charges = slot.charges;
        if (slot.item_id == 0 || !ctx_.items) {
            continue;
        }
        const Mm2ItemRecord *irec = mm2_items_lookup(ctx_.items, slot.item_id);
        const uint32_t base = irec ? irec->gold : 0u;
        const uint32_t buy = mm2_smith_price(base, static_cast<uint8_t>(slot.flags & 0x3Fu));
        if (smith_mode_ == SmithMode::Sell) {
            /* Store buy/2; townSvcSmithSell applies Merchant second /2. */
            smith_view_[i].price = mm2_smith_sell_price(buy);
        } else {
            smith_view_[i].price = mm2_smith_identify_cost(slot.flags);
        }
    }
}

void PlayTownServiceUi::applyTempleAndReturn(int party_slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec) {
        return;
    }
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));

    switch (temple_opt_) {
    case mm2::events::TempleOption::Heal: {
        const uint32_t cost = mm2::events::townSvcTempleHealCost(*rec, ctx_.map_id);
        const mm2::events::TownSvcHealResult r = mm2::events::townSvcHeal(*rec, cost);
        if (cost > 0 && !r.paid) {
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, u32(cost));
        } else if (cost == 0) {
            std::snprintf(status_, sizeof(status_), "%s needs no healing.", name);
        } else {
            std::snprintf(status_, sizeof(status_), "%s healed for %u gp.", name, u32(r.cost));
        }
        break;
    }
    case mm2::events::TempleOption::RestoreAlignment: {
        const uint32_t cost = mm2::events::townSvcTempleAlignCost(*rec, ctx_.map_id);
        const mm2::events::TownSvcAlignResult r =
            mm2::events::townSvcRestoreAlignment(*rec, cost);
        if (cost > 0 && !r.paid) {
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, u32(cost));
        } else if (!r.restored && cost == 0) {
            std::snprintf(status_, sizeof(status_), "%s's alignment is already true.", name);
        } else {
            std::snprintf(status_, sizeof(status_), "%s's alignment restored (%u gp).", name,
                          u32(r.cost));
        }
        break;
    }
    case mm2::events::TempleOption::Donate: {
        const uint32_t cost = mm2_town_temple_donate_cost(ctx_.map_id);
        const mm2::events::TownSvcDonateResult r =
            mm2::events::townSvcTempleDonate(ctx_.a4, *rec, ctx_.map_id, ctx_.rng);
        if (!r.paid) {
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, u32(cost));
        } else if (r.all_temples_donated && r.blessed) {
            std::snprintf(status_, sizeof(status_),
                          "%s donated %u gp. Fe Farthing queued — Today you are blessed!", name,
                          u32(r.cost));
        } else if (r.all_temples_donated) {
            std::snprintf(status_, sizeof(status_),
                          "%s donated %u gp. All temples — Fe Farthing queued!", name, u32(r.cost));
        } else if (r.blessed) {
            std::snprintf(status_, sizeof(status_),
                          "%s donated %u gp.\nToday you are blessed!", name, u32(r.cost));
        } else {
            std::snprintf(status_, sizeof(status_), "%s donated %u gp.", name, u32(r.cost));
        }
        break;
    }
    case mm2::events::TempleOption::BuySpell: {
        if (temple_spell_slot_ < 0 || temple_spell_slot_ >= MM2_TEMPLE_SPELL_SLOTS) {
            break;
        }
        const Mm2SpellShopSlot &slot = temple_spell_stock_[temple_spell_slot_];
        const char *spell_name =
            (slot.spell_index < mm2::gameplay::kSpellsPerSchool)
                ? mm2::gameplay::kClericSpells[slot.spell_index].name
                : "spell";
        /* Menu offer gate 0x1DAC6 — zero cost if wrong class / SL / already known. */
        const uint32_t offer = mm2::events::townSvcSpellOfferGold(
            *rec, mm2::gameplay::SpellSchool::Cleric, slot.spell_index, slot.gold);
        const mm2::events::TownSvcSpellResult r =
            mm2::events::townSvcBuySpell(*rec, slot.spell_index, offer);
        fillSpellBuyStatus(status_, sizeof(status_), name, spell_name, r, offer);
        break;
    }
    default:
        break;
    }
    holdShopResult(Phase::Menu); /* 0x1D6C2: hold until key_read, then A-F redraws */
}

void PlayTownServiceUi::applyGuildBuyAndReturn(int party_slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec || guild_slot_ < 0 || guild_slot_ >= MM2_MAGE_GUILD_SLOTS) {
        return;
    }
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    const Mm2SpellShopSlot &slot = guild_stock_[guild_slot_];
    const char *spell_name = (slot.spell_index < mm2::gameplay::kSpellsPerSchool)
                                 ? mm2::gameplay::kSorcererSpells[slot.spell_index].name
                                 : "spell";
    /* Menu offer gate 0x1D97A — zero cost if wrong class / SL / already known. */
    const uint32_t offer = mm2::events::townSvcSpellOfferGold(
        *rec, mm2::gameplay::SpellSchool::Sorcerer, slot.spell_index, slot.gold);
    const mm2::events::TownSvcSpellResult r =
        mm2::events::townSvcBuySpell(*rec, slot.spell_index, offer);
    fillSpellBuyStatus(status_, sizeof(status_), name, spell_name, r, offer);
    guild_slot_ = -1;
    holdShopResult(Phase::Menu); /* 0x1D6C2 result hold — same as temple spell buy */
}

void PlayTownServiceUi::applyTrainingAndReturn(int party_slot, bool to_max)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec) {
        return;
    }
    status_[0] = '\0';
    /* 0x2026E tst.b $26: afflicted chars never reach the gold/level-up path. */
    if (rec->condition != 0) {
        training_feedback_ = TrainingFeedback::NotWell;
        phase_ = Phase::TrainingResult;
        return;
    }
    const int ridx = mm2::events::townSvcRosterIndex(ctx_, party_slot);
    const uint8_t old_spell_level = rec->spell_level;
    uint16_t hp_total = 0;
    int levels = 0;
    bool gained_spells = false;
    /* 'T' is one ASM level-up. 'M' repeats the same leaf until XP or gold stops. */
    const int max_steps = to_max ? 80 : 1;
    for (int step = 0; step < max_steps; ++step) {
        const mm2::events::TownSvcTrainResult r =
            mm2::events::townSvcTrainLevelUp(*rec, ctx_.map_id, ctx_.rng, ridx);
        if (!r.eligible) {
            /* First key ignored unless XP-eligible (0x20BD0); keep the trainee prompt. */
            if (step == 0) {
                return;
            }
            break;
        }
        if (!r.paid) {
            if (step == 0) {
                training_feedback_ = TrainingFeedback::NeedGold;
                phase_ = Phase::TrainingResult;
                return;
            }
            break;
        }
        ++levels;
        hp_total = static_cast<uint16_t>(hp_total + r.hp_gain);
        if (r.gained_spells) {
            gained_spells = true;
        }
    }
    if (levels == 0) {
        return;
    }
    /* 0x204C8..0x20518: "You gained " + N, then " hit points", then optionally
     * "and new spells" when the 0x20064 leaf returned 1. 'M' uses a cumulative
     * summary (levels / spell levels / HP) instead. */
    training_feedback_ = TrainingFeedback::Gained;
    training_hp_gain_ = hp_total;
    training_gained_spells_ = gained_spells;
    training_max_summary_ = to_max;
    training_levels_gained_ = static_cast<uint8_t>(levels);
    training_spell_levels_gained_ = (rec->spell_level > old_spell_level)
                                        ? static_cast<uint8_t>(rec->spell_level - old_spell_level)
                                        : 0;
    phase_ = Phase::TrainingResult;
}

void PlayTownServiceUi::applySmithBuyAndReturn(int party_slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec || smith_slot_ < 0 || smith_slot_ >= MM2_SMITH_SLOTS) {
        return;
    }
    const mm2::events::SmithItemView &v = smith_view_[smith_slot_];
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    char item[20] = "item";
    if (ctx_.items) {
        const Mm2ItemRecord *irec = mm2_items_lookup(ctx_.items, v.item_id);
        if (irec) {
            mm2_item_name_to_cstr(irec, item, sizeof(item));
        }
    }
    const mm2::events::TownSvcBuyResult r =
        mm2::events::townSvcSmithBuy(*rec, v.item_id, v.charges, v.bonus, v.price);
    if (r.bought) {
        std::snprintf(status_, sizeof(status_), "%s bought %s for %u gp.", name, item, u32(r.price));
    } else {
        switch (r.reject) {
        case mm2::events::TownSvcBuyReject::Condition:
            std::snprintf(status_, sizeof(status_), "%s is afflicted - cannot buy.", name);
            break;
        case mm2::events::TownSvcBuyReject::BackpackFull:
            std::snprintf(status_, sizeof(status_), "%s's pack is full.", name);
            break;
        case mm2::events::TownSvcBuyReject::NotEnoughGold:
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name,
                          u32(mm2::events::townSvcSmithMerchantBuyPrice(v.price, *rec)));
            break;
        default:
            std::snprintf(status_, sizeof(status_), "%s could not buy %s.", name, item);
            break;
        }
    }
    smith_slot_ = -1;
    holdShopResult(Phase::SmithItems); /* 0x1C432: preset 7 + pair text + key_read */
}

void PlayTownServiceUi::applySmithSellAndReturn(int party_slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec || smith_slot_ < 0 || smith_slot_ >= MM2_SMITH_SLOTS) {
        return;
    }
    const mm2::events::SmithItemView &v = smith_view_[smith_slot_];
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    char item[20] = "item";
    if (ctx_.items) {
        const Mm2ItemRecord *irec = mm2_items_lookup(ctx_.items, v.item_id);
        if (irec) {
            mm2_item_name_to_cstr(irec, item, sizeof(item));
        }
    }
    const mm2::events::TownSvcSellResult r =
        mm2::events::townSvcSmithSell(*rec, smith_slot_, v.price);
    if (r.sold) {
        std::snprintf(status_, sizeof(status_), "%s sold %s for %u gp.", name, item, u32(r.price));
        buildSmithBackpackView();
    } else {
        switch (r.reject) {
        case mm2::events::TownSvcSellReject::Condition:
            std::snprintf(status_, sizeof(status_), "%s is afflicted - cannot sell.", name);
            break;
        case mm2::events::TownSvcSellReject::NoItem:
            std::snprintf(status_, sizeof(status_), "No item in that slot.");
            break;
        default:
            std::snprintf(status_, sizeof(status_), "%s could not sell %s.", name, item);
            break;
        }
    }
    smith_slot_ = -1;
    holdShopResult(Phase::SmithItems); /* 0x1C432 result hold — same as buy */
}

void PlayTownServiceUi::applySmithIdentifyAndReturn(int party_slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec || smith_slot_ < 0 || smith_slot_ >= MM2_SMITH_SLOTS) {
        return;
    }
    const mm2::events::SmithItemView &v = smith_view_[smith_slot_];
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    char summary[sizeof(status_)];
    const mm2::events::TownSvcIdentifyResult r = mm2::events::townSvcSmithIdentify(
        *rec, smith_slot_, ctx_.items, v.price, summary, sizeof(summary));
    if (r.identified) {
        /* Real screen shows only the report (name/gp aren't part of it — see
         * the WinUAE reference capture); Gold= in the left chrome already
         * reflects the cost. */
        std::snprintf(status_, sizeof(status_), "%s", summary);
    } else {
        switch (r.reject) {
        case mm2::events::TownSvcIdentifyReject::Condition:
            std::snprintf(status_, sizeof(status_), "%s is afflicted - cannot identify.", name);
            break;
        case mm2::events::TownSvcIdentifyReject::NoItem:
            std::snprintf(status_, sizeof(status_), "No item in that slot.");
            break;
        case mm2::events::TownSvcIdentifyReject::NotEnoughGold:
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, u32(v.price));
            break;
        default:
            std::snprintf(status_, sizeof(status_), "%s could not identify item.", name);
            break;
        }
    }
    /* 0x1BBD6: hold on backpack list until the next key dismisses it — applies
     * to rejects too, else "not enough gold" etc. never reaches the screen. */
    smith_identify_pending_ = true;
    phase_ = Phase::SmithItems;
    smith_slot_ = -1;
}

void PlayTownServiceUi::applyTavernFeedingFrenzy()
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec) {
        return;
    }
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    const mm2::events::TownSvcFeedingResult r =
        mm2::events::townSvcFeedingFrenzy(*rec, ctx_.launch, ctx_.roster, ctx_.map_id);
    if (r.fed) {
        std::snprintf(status_, sizeof(status_), "%s: party fed to 40 food (%u gp).", name,
                      u32(r.cost));
    } else {
        std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, u32(r.cost));
    }
    /* 0x1C902: clear_rect_preset(7) + pair text + key_read, then the A-E menu
     * redraws. Holding here (not Menu) so status cannot paint over captions. */
    tavern_tipped_ = false;
    phase_ = Phase::TavernRumor;
}

void PlayTownServiceUi::applyTavernStatBoost(int slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec || slot < 0 || slot >= mm2::events::kPubStatBoostCount) {
        return;
    }
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    const mm2::events::TownSvcStatBoostResult r =
        mm2::events::townSvcTavernStatBoost(ctx_.a4, *rec, slot, ctx_.map_id, ctx_.rng);
    const char *label = tavern_data_.boosts[slot].label;
    if (!r.paid) {
        std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name,
                      u32(tavern_data_.boosts[slot].cost));
    } else if (r.sick) {
        std::snprintf(status_, sizeof(status_), "%s bought %s — you feel sick!", name,
                      label ? label : "boost");
    } else if (!r.applied) {
        std::snprintf(status_, sizeof(status_), "%s paid %u gp for %s (building up).", name, u32(r.cost),
                      label ? label : "boost");
    } else {
        std::snprintf(status_, sizeof(status_), "%s bought %s (%u gp).", name,
                      label ? label : "boost", u32(r.cost));
    }
    tavern_tipped_ = false;
    phase_ = Phase::TavernRumor; /* 0x1C902 result hold — see feeding frenzy */
}

void PlayTownServiceUi::applyTavernTip()
{
    /* D @ 0x1CFCA: 1gp + RNG + day-pair tips (0x1C962 → A4-$58AE). */
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec) {
        return;
    }
    const uint16_t era = ctx_.a4 ? mm2_gs_u16(ctx_.a4, MM2_GS_ERA) : 0;
    const uint16_t day = ctx_.a4 ? mm2_gs_day(ctx_.a4, era) : 1;
    const mm2::events::TownSvcTipResult r =
        mm2::events::townSvcTavernTip(*rec, day, ctx_.rng);
    tavern_tipped_ = true;
    if (!r.ok) {
        if (r.reject == mm2::events::TownSvcTipReject::NotEnoughGold) {
            std::snprintf(status_, sizeof(status_), "Not enough gold!");
        } else if (r.reject == mm2::events::TownSvcTipReject::Condition) {
            std::snprintf(status_, sizeof(status_), "Cannot tip while afflicted.");
        } else {
            std::snprintf(status_, sizeof(status_), "Thank you -\nPlease come again");
        }
        phase_ = Phase::TavernRumor;
        return;
    }
    const int b = r.pair_base;
    const char *a = (b >= 0 && b < mm2::events::kPubTipCount) ? tavern_data_.tips[b] : nullptr;
    const char *c =
        (b + 1 < mm2::events::kPubTipCount) ? tavern_data_.tips[b + 1] : nullptr;
    formatPubPair(status_, sizeof(status_), a, c, "Thank you -\nPlease come again");
    phase_ = Phase::TavernRumor;
}

void PlayTownServiceUi::applyTavernRumor()
{
    /* E @ 0x1D0B4: day-pair rumors (A4-$594E), no gold. */
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec) {
        return;
    }
    const uint16_t era = ctx_.a4 ? mm2_gs_u16(ctx_.a4, MM2_GS_ERA) : 0;
    const uint16_t day = ctx_.a4 ? mm2_gs_day(ctx_.a4, era) : 1;
    const mm2::events::TownSvcTipResult r = mm2::events::townSvcTavernRumor(*rec, day);
    tavern_tipped_ = false;
    if (!r.ok) {
        std::snprintf(status_, sizeof(status_), "Cannot listen while afflicted.");
        phase_ = Phase::TavernRumor;
        return;
    }
    const int b = r.pair_base;
    const char *a =
        (b >= 0 && b < mm2::events::kPubRumorCount) ? tavern_data_.rumors[b] : nullptr;
    const char *c =
        (b + 1 < mm2::events::kPubRumorCount) ? tavern_data_.rumors[b + 1] : nullptr;
    formatPubPair(status_, sizeof(status_), a, c, nullptr);
    phase_ = Phase::TavernRumor;
}

void PlayTownServiceUi::applyTavernSpecialty(int food_idx)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
    if (!rec || food_idx < 0 || food_idx >= mm2::events::kPubFoodOptions) {
        return;
    }
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    /* 0x1CD2E only — no 0x18EC0 encode (that is selector 0xC9). */
    const mm2::events::TownSvcSpecialtyResult r =
        mm2::events::townSvcTavernSpecialty(ctx_.a4, *rec, ctx_.map_id, food_idx, ctx_.rng);
    const char *food = tavern_data_.food.options[food_idx];
    if (!r.paid) {
        std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, u32(r.cost));
    } else if (r.sick) {
        std::snprintf(status_, sizeof(status_), "%s: %s — you feel sick!", name,
                      food ? food : "meal");
    } else {
        std::snprintf(status_, sizeof(status_), "%s ordered %s (%u gp).", name,
                      food ? food : "meal", u32(r.cost));
    }
    tavern_tipped_ = false;
    phase_ = Phase::TavernRumor; /* 0x1C902 result hold — see feeding frenzy */
}

void PlayTownServiceUi::handleKey(char ch, bool escape)
{
    if (!active_) {
        return;
    }

    /* 0x20554 / 0x1D6C2 key_read: any key (including Esc) dismisses the result
     * and returns to the shop prompt — it does not leave the hall. */
    if (phase_ == Phase::TrainingResult) {
        if (escape || ch != 0) {
            dismissTrainingResult();
        }
        return;
    }
    if (phase_ == Phase::ShopResult) {
        if (escape || ch != 0) {
            status_[0] = '\0';
            phase_ = result_return_phase_;
            result_return_phase_ = Phase::Menu;
        }
        return;
    }

    if (escape) {
        switch (phase_) {
        case Phase::Menu:
        case Phase::HirelingTemple:
            close();
            break;
        case Phase::SmithItems:
            if (smith_identify_pending_) {
                smith_identify_pending_ = false;
                status_[0] = '\0';
                break;
            }
            phase_ = Phase::Menu;
            smith_mode_ = SmithMode::Buy;
            status_[0] = '\0';
            break;
        case Phase::Denied:
            close();
            break;
        case Phase::TavernFood:
        case Phase::TavernBoost:
        case Phase::TavernRumor:
            phase_ = Phase::Menu;
            status_[0] = '\0';
            tavern_tipped_ = false;
            break;
        default:
            break;
        }
        return;
    }

    if (ch == 0) {
        return;
    }

    /* Digits 1..8 and # switch the shop member (A4-$5A3A / left chrome). */
    const bool reject_hireling_digit =
        kind_ == Kind::Smith || kind_ == Kind::MageGuild || kind_ == Kind::Tavern;
    if (trySelectMemberByDigit(active_member_, ctx_, ch, reject_hireling_digit)) {
        status_[0] = '\0';
        /* Temple outer loop @ 0x1E396: hireling → 0x1E116; hero → paid 0x1DD8E. */
        if (kind_ == Kind::Temple) {
            enterTempleMemberView(active_member_);
        }
        if (phase_ == Phase::SmithItems && smith_mode_ != SmithMode::Buy) {
            buildSmithBackpackView();
        }
        return;
    }
    if (ch == '#') {
        switch (phase_) {
        case Phase::Menu:
        case Phase::HirelingTemple:
        case Phase::SmithItems:
        case Phase::TavernFood:
        case Phase::TavernBoost:
        case Phase::TavernRumor: {
            const bool skip_hirelings =
                kind_ == Kind::Smith || kind_ == Kind::MageGuild || kind_ == Kind::Tavern;
            cycleActiveMember(active_member_, ctx_, skip_hirelings);
            status_[0] = '\0';
            if (kind_ == Kind::Temple) {
                enterTempleMemberView(active_member_);
            }
            if (phase_ == Phase::SmithItems && smith_mode_ != SmithMode::Buy) {
                buildSmithBackpackView();
            }
            return;
        }
        default:
            break;
        }
    }

    switch (phase_) {
    case Phase::Menu:
        if (kind_ == Kind::Training && (ch == 'T' || ch == 'M')) {
            /* 0x20BD0: T is accepted only when the XP scc armed -$8(a5).
             * M is remake QoL: same gate, then train until XP or gold runs out. */
            if (trainingXpEligible(ctx_, active_member_)) {
                applyTrainingAndReturn(active_member_, ch == 'M');
            }
            break;
        }
        if (kind_ == Kind::Temple) {
            /* Hirelings never enter the paid A–F menu (outer loop @ 0x1E396). */
            if (mm2::events::townSvcPartySlotIsHireling(ctx_, active_member_)) {
                break;
            }
            if (ch == 'A') {
                temple_opt_ = mm2::events::TempleOption::Heal;
                applyTempleAndReturn(active_member_);
            } else if (ch == 'B') {
                temple_opt_ = mm2::events::TempleOption::RestoreAlignment;
                applyTempleAndReturn(active_member_);
            } else if (ch == 'C') {
                temple_opt_ = mm2::events::TempleOption::Donate;
                applyTempleAndReturn(active_member_);
            } else if (ch >= 'D' && ch <= 'F') {
                const int slot = ch - 'D';
                Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
                const uint32_t offer =
                    rec ? mm2::events::townSvcSpellOfferGold(
                              *rec, mm2::gameplay::SpellSchool::Cleric,
                              temple_spell_stock_[slot].spell_index, temple_spell_stock_[slot].gold)
                        : 0u;
                if (offer != 0) {
                    temple_opt_ = mm2::events::TempleOption::BuySpell;
                    temple_spell_slot_ = slot;
                    applyTempleAndReturn(active_member_);
                }
            } else if (ch == 'G') {
                showActiveMemberGold();
            }
        } else if (kind_ == Kind::Smith) {
            if (ch == 'G') {
                showActiveMemberGold();
            } else if (ch >= 'A' && ch <= 'D') {
                smith_mode_ = SmithMode::Buy;
                smith_category_ = ch - 'A';
                buildSmithView();
                phase_ = Phase::SmithItems;
            } else if (ch == 'E' || ch == 'F') {
                smith_mode_ = (ch == 'E') ? SmithMode::Sell : SmithMode::Identify;
                buildSmithBackpackView();
                phase_ = Phase::SmithItems;
                status_[0] = '\0';
            }
        } else if (kind_ == Kind::MageGuild) {
            if (ch >= 'A' && ch <= 'D') {
                const int slot = ch - 'A';
                Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
                const uint32_t offer =
                    rec ? mm2::events::townSvcSpellOfferGold(
                              *rec, mm2::gameplay::SpellSchool::Sorcerer,
                              guild_stock_[slot].spell_index, guild_stock_[slot].gold)
                        : 0u;
                if (offer != 0) {
                    guild_slot_ = slot;
                    applyGuildBuyAndReturn(active_member_);
                }
            }
        } else if (kind_ == Kind::Tavern) {
            status_[0] = '\0';
            if (ch == 'A') {
                applyTavernFeedingFrenzy();
            } else if (ch == 'B') {
                /* ASM 0x1CAC4 — stat-boost submenu, NOT drinks. */
                tavern_opt_ = mm2::events::TavernOption::StatBoost;
                phase_ = Phase::TavernBoost;
            } else if (ch == 'C') {
                tavern_opt_ = mm2::events::TavernOption::Specialties;
                phase_ = Phase::TavernFood;
            } else if (ch == 'D') {
                applyTavernTip();
            } else if (ch == 'E') {
                applyTavernRumor();
            }
        }
        break;

    case Phase::TavernFood:
        if (ch >= 'A' && ch < char('A' + mm2::events::kPubFoodOptions)) {
            applyTavernSpecialty(ch - 'A');
        }
        break;

    case Phase::TavernBoost:
        if (ch >= 'A' && ch < char('A' + mm2::events::kPubStatBoostCount)) {
            applyTavernStatBoost(ch - 'A');
        }
        break;

    case Phase::TavernRumor:
        /* Any key clears the rumor and returns to the menu. */
        phase_ = Phase::Menu;
        status_[0] = '\0';
        tavern_tipped_ = false;
        break;

    case Phase::SmithItems:
        if (smith_identify_pending_) {
            smith_identify_pending_ = false;
            status_[0] = '\0';
            break;
        }
        if (ch >= 'A' && ch <= 'F') {
            const int slot = ch - 'A';
            if (slot < MM2_SMITH_SLOTS && smith_view_[slot].item_id != 0) {
                smith_slot_ = slot;
                if (smith_mode_ == SmithMode::Sell) {
                    applySmithSellAndReturn(active_member_);
                } else if (smith_mode_ == SmithMode::Identify) {
                    applySmithIdentifyAndReturn(active_member_);
                } else {
                    applySmithBuyAndReturn(active_member_);
                }
            }
        } else if (ch == 'G') {
            showActiveMemberGold();
        }
        break;

    case Phase::Denied:
        close();
        break;

    case Phase::HirelingTemple:
        /* Digits / # / Esc handled above; A–F / G are not accepted (0x1E21C). */
        break;

    case Phase::ShopResult:
    case Phase::TrainingResult:
        break;
    }
}

void PlayTownServiceUi::render(gfx::ScreenCompositor &c) const
{
    if (!active_) {
        return;
    }

    clearShopMenuBand(c);

    if (phase_ == Phase::Denied) {
        drawLeftChrome(c);
        drawCell(c, kBandRowFirst, kOptCol, "Sorry, you must be a");
        drawCell(c, kBandRowFirst + 1, kOptCol, "member of this guild");
        drawCell(c, kBandRowFirst + 2, kOptCol, " to purchase spells.");
        drawEscFooter(c);
        return;
    }

    /* Hireling temple leaf @ 0x1E116: heal/healthy text only — no A–F paid menu. */
    if (phase_ == Phase::HirelingTemple) {
        drawHirelingTempleHeal(c);
        drawEscFooter(c);
        return;
    }

    /* 0x2023A / 0x1D6C2 preset 7: right panel only. Left chrome (name/gold) stays. */
    if (phase_ == Phase::TrainingResult) {
        drawLeftChrome(c);
        drawTrainingResult(c);
        drawEscFooter(c);
        return;
    }
    if (phase_ == Phase::ShopResult) {
        drawLeftChrome(c);
        drawMultiline(c, kBandRowFirst, kOptCol, status_, 160, 255, 160);
        drawEscFooter(c);
        return;
    }

    if (phase_ == Phase::TavernRumor) {
        drawLeftChrome(c);
        if (tavern_tipped_ && status_[0] == '\0') {
            drawCell(c, kBandRowFirst, kOptCol, "    Thank you -");
            drawCell(c, kBandRowFirst + 1, kOptCol, "Please come again");
        } else if (tavern_tipped_ && ::strchr(status_, '\n') == nullptr &&
                   status_[0] != '\0') {
            drawCell(c, kBandRowFirst, kOptCol, "    Thank you -");
            drawCell(c, kBandRowFirst + 1, kOptCol, "Please come again");
            drawMultiline(c, kBandRowFirst + 3, kOptCol, status_, 180, 255, 180);
        } else {
            drawMultiline(c, kBandRowFirst, kOptCol, status_, 180, 255, 180);
        }
        drawEscFooter(c);
        return;
    }

    if (phase_ == Phase::TavernFood) {
        drawLeftChrome(c);
        int row = kBandRowFirst;
        for (int i = 0; i < mm2::events::kPubFoodOptions; ++i) {
            char line[48];
            const char *food = tavern_data_.food.options[i];
            std::snprintf(line, sizeof(line), "%c) %s (%u gp)", char('A' + i),
                          food ? food : "---", u32(tavern_data_.food.costs[i]));
            row = drawMultiline(c, row, kOptCol, line);
        }
        drawEscFooter(c);
        return;
    }

    if (phase_ == Phase::TavernBoost) {
        drawLeftChrome(c);
        int row = kBandRowFirst;
        for (int i = 0; i < mm2::events::kPubStatBoostCount; ++i) {
            char line[48];
            const char *lab = tavern_data_.boosts[i].label;
            std::snprintf(line, sizeof(line), "%c) %-14s %u gp", char('A' + i),
                          lab ? lab : "---", u32(tavern_data_.boosts[i].cost));
            drawCell(c, row++, kOptCol, line);
        }
        drawEscFooter(c);
        return;
    }

    if (phase_ == Phase::SmithItems) {
        /* 0x1BBD6: the identify result holds over the backpack list until the
         * next keypress dismisses it. The list fills all six option rows, and
         * the report lines (e.g. "Class rating(s) - ( K P A C S R N B )") are
         * wider than the option column leaves room for, so this drops the
         * left member/gold chrome and uses the full band width — otherwise
         * (as it was) the identify summary is computed and the gold is spent
         * but nothing ever reaches the screen, or the longer lines clip. */
        if (smith_identify_pending_ && status_[0]) {
            drawMultiline(c, kBandRowFirst, kLeftCol, status_, 255, 255, 128);
            drawEscFooter(c);
            return;
        }
        drawLeftChrome(c);
        int row = kBandRowFirst;
        Mm2RosterRecord *buyer = mm2::events::townSvcMemberRecord(ctx_, active_member_);
        for (int i = 0; i < MM2_SMITH_SLOTS; ++i) {
            char line[48];
            if (smith_view_[i].item_id != 0) {
                char item[20] = "?";
                if (ctx_.items) {
                    const Mm2ItemRecord *irec = mm2_items_lookup(ctx_.items, smith_view_[i].item_id);
                    if (irec) {
                        mm2_item_name_to_cstr(irec, item, sizeof(item));
                    }
                }
                uint32_t shown = smith_view_[i].price;
                if (buyer) {
                    if (smith_mode_ == SmithMode::Buy) {
                        shown = mm2::events::townSvcSmithMerchantBuyPrice(shown, *buyer);
                    } else if (smith_mode_ == SmithMode::Sell) {
                        shown = mm2::events::townSvcSmithMerchantSellPrice(shown, *buyer);
                    }
                }
                /* Weapons/armor carry a magic "+" bonus (0x1BF16 drives the price
                 * from it); show it in the name like the identify path does, else
                 * the inflated price looks like a bug. Misc charges are not a '+'. */
                char name_bonus[28];
                const uint8_t plus = static_cast<uint8_t>(smith_view_[i].bonus & 0x3Fu);
                if (plus != 0) {
                    std::snprintf(name_bonus, sizeof(name_bonus), "%s +%u", item,
                                  static_cast<unsigned>(plus));
                } else {
                    std::snprintf(name_bonus, sizeof(name_bonus), "%s", item);
                }
                char price_str[16];
                std::snprintf(price_str, sizeof(price_str), "%u gp", u32(shown));
                /* 24-col option band (cols 0x10..0x27): name left, price right-aligned. */
                char body[40];
                const int body_len =
                    std::snprintf(body, sizeof(body), "%c) %s", char('A' + i), name_bonus);
                int gap = 24 - body_len - static_cast<int>(std::strlen(price_str));
                if (gap < 1) {
                    gap = 1;
                }
                std::snprintf(line, sizeof(line), "%s%*s%s", body, gap, "", price_str);
                drawCell(c, row++, kOptCol, line);
            } else {
                std::snprintf(line, sizeof(line), "%c) ---", char('A' + i));
                drawCell(c, row++, kOptCol, line, 130, 130, 130);
            }
        }
        drawEscFooter(c);
        return;
    }

    /* Top-level menus — left chrome @ 0x1C494, A–F captions @ col 0x10 (0x1C6BE). */
    if (phase_ == Phase::Menu) {
        drawLeftChrome(c);
        if (kind_ == Kind::Training) {
            drawTrainingPrompt(c);
        } else if ((kind_ == Kind::Temple || kind_ == Kind::Tavern || kind_ == Kind::Smith) &&
                   status_[0]) {
            /* Temple A-F, tavern A-E, and smith A-F all fill the six option rows
             * (0x11..0x16), the same row the common status_ footer below
             * normally uses — drawing both garbled the last caption together
             * with the feedback text. Show the result in place of the menu
             * instead, same as the hireling-heal leaf already does. */
            drawMultiline(c, kBandRowFirst, kOptCol, status_, 160, 255, 160);
        } else if (kind_ == Kind::Temple) {
            int row = kBandRowFirst;
            Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
            /* Captions A4-$56E6[0..5] @ col $13; L-N @ $1F; cost/"---" @ $23. */
            uint32_t costs[6] = {0, 0, 0, 0, 0, 0};
            if (rec) {
                costs[0] = mm2::events::townSvcTempleHealCost(*rec, ctx_.map_id);
                costs[1] = mm2::events::townSvcTempleAlignCost(*rec, ctx_.map_id);
                costs[2] = mm2_town_temple_donate_cost(ctx_.map_id);
            }
            for (int i = 0; i < 3; ++i) {
                drawCell(c, row + i, kTempleOptCol, kTownSpellMenuCaptions[i]);
                char cost_txt[16];
                formatCostColumn(cost_txt, sizeof(cost_txt), costs[i]);
                drawCell(c, row + i, kCostCol, cost_txt);
            }
            for (int i = 0; i < MM2_TEMPLE_SPELL_SLOTS; ++i) {
                const Mm2SpellShopSlot &s = temple_spell_stock_[i];
                const uint32_t offer =
                    rec ? mm2::events::townSvcSpellOfferGold(*rec, mm2::gameplay::SpellSchool::Cleric,
                                                             s.spell_index, s.gold)
                        : 0u;
                drawCell(c, row + 3 + i, kTempleOptCol, kTownSpellMenuCaptions[3 + i]);
                char id_txt[8];
                formatSpellIdLn(id_txt, sizeof(id_txt), mm2::gameplay::SpellSchool::Cleric,
                                s.spell_index);
                if (id_txt[0]) {
                    drawCell(c, row + 3 + i, kSpellIdCol, id_txt);
                }
                char cost_txt[16];
                formatCostColumn(cost_txt, sizeof(cost_txt), offer);
                drawCell(c, row + 3 + i, kCostCol, cost_txt);
            }
        } else if (kind_ == Kind::Smith) {
            drawCell(c, kBandRowFirst, kOptCol, "A) Weapons");
            drawCell(c, kBandRowFirst + 1, kOptCol, "B) Today's Specials");
            drawCell(c, kBandRowFirst + 2, kOptCol, "C) Armor");
            drawCell(c, kBandRowFirst + 3, kOptCol, "D) Misc Items");
            drawCell(c, kBandRowFirst + 4, kOptCol, "E) Sell Items");
            drawCell(c, kBandRowFirst + 5, kOptCol, "F) Identify Items");
        } else if (kind_ == Kind::MageGuild) {
            /* Captions A4-$56E6[3..6] @ col $14, rows $12..$15 (index+0xF @ 0x1E5E2)
             * after 0x1E43C patches [3..5] to "A/B/C) Spell S"; [6] is "D) Spell S".
             * L-N @ $1F; cost @ $23 (0x1E5FA+). */
            constexpr int kGuildRow0 = 0x12;
            Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, active_member_);
            for (int i = 0; i < MM2_MAGE_GUILD_SLOTS; ++i) {
                const Mm2SpellShopSlot &s = guild_stock_[i];
                const uint32_t offer =
                    rec ? mm2::events::townSvcSpellOfferGold(
                              *rec, mm2::gameplay::SpellSchool::Sorcerer, s.spell_index, s.gold)
                        : 0u;
                char cap[16];
                std::snprintf(cap, sizeof(cap), "%c) Spell S", char('A' + i));
                drawCell(c, kGuildRow0 + i, kGuildOptCol, cap);
                char id_txt[8];
                formatSpellIdLn(id_txt, sizeof(id_txt), mm2::gameplay::SpellSchool::Sorcerer,
                                s.spell_index);
                if (id_txt[0]) {
                    drawCell(c, kGuildRow0 + i, kSpellIdCol, id_txt);
                }
                char cost_txt[16];
                formatCostColumn(cost_txt, sizeof(cost_txt), offer);
                drawCell(c, kGuildRow0 + i, kCostCol, cost_txt);
            }
        } else if (kind_ == Kind::Tavern) {
            drawCell(c, kBandRowFirst, kOptCol, "A) Feeding frenzy (all");
            drawCell(c, kBandRowFirst + 1, kOptCol, "   you can carry");
            drawCell(c, kBandRowFirst + 2, kOptCol, "B) Have a drink");
            drawCell(c, kBandRowFirst + 3, kOptCol, "C) Specialties");
            drawCell(c, kBandRowFirst + 4, kOptCol, "D) Tip the bartender");
            drawCell(c, kBandRowFirst + 5, kOptCol, "E) Listen for rumors");
        }
        drawEscFooter(c);
    }

    /* Temple / tavern / smith already drew status_ in place of their six-row
     * menus — the common footer here would redraw it a second time at
     * kBandRowLast. */
    if (status_[0] && phase_ != Phase::TavernRumor && kind_ != Kind::Temple &&
        kind_ != Kind::Tavern && kind_ != Kind::Smith) {
        drawMultiline(c, kBandRowLast, kOptCol, status_, 160, 255, 160);
    }
}

}  // namespace mm2::ui
