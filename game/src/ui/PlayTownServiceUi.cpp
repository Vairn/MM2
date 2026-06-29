#include "mm2/ui/PlayTownServiceUi.h"

#include "mm2/CppStdCompat.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"

namespace mm2::ui {

namespace {

using namespace mm2::gfx::play_layout;
using mm2::ui::amiga_layout::cellX;
using mm2::ui::amiga_layout::cellY;

const char *townName(int map_id)
{
    static const char *kTowns[] = {"Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};
    return (map_id >= 0 && map_id < 5) ? kTowns[map_id] : "Town";
}

void drawCell(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = 255,
              uint8_t g = 255, uint8_t b = 255)
{
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

}  // namespace

bool PlayTownServiceUi::chooseTempleOption(const mm2::events::TownServiceContext &ctx,
                                           mm2::events::TempleOption &)
{
    ctx_ = ctx;
    kind_ = Kind::Temple;
    pending_ = true;
    return false; /* defer to the multi-frame overlay; exit the blocking driver */
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

void PlayTownServiceUi::begin()
{
    if (!pending_) {
        return;
    }
    pending_ = false;
    active_ = true;
    /* Training has no option menu (level-up only) -> open directly on the trainee
     * prompt; temple/smith open on their option/category menu. */
    phase_ = (kind_ == Kind::Training) ? Phase::Member : Phase::Menu;
    smith_slot_ = -1;
    status_[0] = '\0';
}

void PlayTownServiceUi::close()
{
    active_ = false;
    pending_ = false;
    kind_ = Kind::None;
    phase_ = Phase::Menu;
    status_[0] = '\0';
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
    for (int i = 0; i < MM2_SMITH_SLOTS; ++i) {
        uint8_t price_meta = 0, charges = 0, flags = 0;
        mm2_smith_buy_fields(smith_category_, slots[i].meta, &price_meta, &charges, &flags);
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
        const uint32_t cost = mm2::events::townSvcHealingCost(rec->level, ctx_.map_id);
        const mm2::events::TownSvcHealResult r = mm2::events::townSvcHeal(*rec, cost);
        if (!r.paid) {
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, cost);
        } else {
            std::snprintf(status_, sizeof(status_), "%s healed for %u gp.", name, r.cost);
        }
        break;
    }
    case mm2::events::TempleOption::RestoreCondition:
        mm2::events::townSvcRestoreCondition(*rec);
        std::snprintf(status_, sizeof(status_), "%s's condition restored.", name);
        break;
    case mm2::events::TempleOption::Donate: {
        Mm2TownCommerce town{};
        const uint32_t cost = mm2_town_commerce(ctx_.map_id, &town) ? town.donation_gold : 0u;
        const mm2::events::TownSvcDonateResult r =
            mm2::events::townSvcTempleDonate(ctx_.a4, *rec, ctx_.map_id);
        if (!r.paid) {
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, cost);
        } else if (r.all_temples_donated) {
            std::snprintf(status_, sizeof(status_), "%s donated %u gp. All temples blessed!", name,
                          r.cost);
        } else {
            std::snprintf(status_, sizeof(status_), "%s donated %u gp.", name, r.cost);
        }
        break;
    }
    default:
        break;
    }
    phase_ = Phase::Menu;
}

void PlayTownServiceUi::applyTrainingAndReturn(int party_slot)
{
    Mm2RosterRecord *rec = mm2::events::townSvcMemberRecord(ctx_, party_slot);
    if (!rec) {
        return;
    }
    char name[20];
    mm2_roster_name_to_cstr(rec, name, sizeof(name));
    const mm2::events::TownSvcTrainResult r = mm2::events::townSvcTrainLevelUp(*rec, ctx_.map_id);
    if (!r.eligible) {
        /* XP gate (no charge): faithful to the engine charging nothing when the
         * character lacks the experience for the next level. */
        std::snprintf(status_, sizeof(status_), "%s lacks experience to advance.", name);
    } else if (!r.paid) {
        const uint32_t fee = mm2::events::townSvcTrainingCost(rec->level, ctx_.map_id);
        std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, fee);
    } else {
        std::snprintf(status_, sizeof(status_), "%s trained to level %u! (+%u HP) %u gp.", name,
                      static_cast<unsigned>(r.new_level), static_cast<unsigned>(r.hp_gain), r.cost);
    }
    /* Training has no sub-menu: stay on the member prompt for the next trainee. */
    phase_ = Phase::Member;
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
        std::snprintf(status_, sizeof(status_), "%s bought %s for %u gp.", name, item, v.price);
    } else {
        switch (r.reject) {
        case mm2::events::TownSvcBuyReject::Condition:
            std::snprintf(status_, sizeof(status_), "%s is afflicted - cannot buy.", name);
            break;
        case mm2::events::TownSvcBuyReject::BackpackFull:
            std::snprintf(status_, sizeof(status_), "%s's pack is full.", name);
            break;
        case mm2::events::TownSvcBuyReject::NotEnoughGold:
            std::snprintf(status_, sizeof(status_), "%s: not enough gold (%u gp).", name, v.price);
            break;
        default:
            std::snprintf(status_, sizeof(status_), "%s could not buy %s.", name, item);
            break;
        }
    }
    phase_ = Phase::Menu;
    smith_slot_ = -1;
}

void PlayTownServiceUi::handleKey(char ch, bool escape)
{
    if (!active_) {
        return;
    }

    if (escape) {
        switch (phase_) {
        case Phase::Menu:
            close();
            break;
        case Phase::SmithItems:
            phase_ = Phase::Menu;
            break;
        case Phase::Member:
            /* Training opens on Member with no menu behind it -> Esc leaves. */
            if (kind_ == Kind::Training) {
                close();
            } else {
                phase_ = (kind_ == Kind::Smith) ? Phase::SmithItems : Phase::Menu;
            }
            break;
        }
        return;
    }

    if (ch == 0) {
        return;
    }

    switch (phase_) {
    case Phase::Menu:
        if (kind_ == Kind::Temple) {
            if (ch == 'A') {
                temple_opt_ = mm2::events::TempleOption::Heal;
                phase_ = Phase::Member;
            } else if (ch == 'B') {
                temple_opt_ = mm2::events::TempleOption::RestoreCondition;
                phase_ = Phase::Member;
            } else if (ch == 'C') {
                temple_opt_ = mm2::events::TempleOption::Donate;
                phase_ = Phase::Member;
            }
        } else if (kind_ == Kind::Smith) {
            if (ch >= 'A' && ch <= 'D') {
                smith_category_ = ch - 'A';
                buildSmithView();
                phase_ = Phase::SmithItems;
            }
        }
        break;

    case Phase::SmithItems:
        if (ch >= '1' && ch <= '6') {
            const int slot = ch - '1';
            if (smith_view_[slot].item_id != 0) {
                smith_slot_ = slot;
                phase_ = Phase::Member;
            }
        }
        break;

    case Phase::Member:
        if (ch >= '1' && ch <= '8') {
            const int slot = ch - '1';
            if (!mm2::events::townSvcMemberRecord(ctx_, slot)) {
                break; /* empty / out-of-range party slot — ignore */
            }
            if (kind_ == Kind::Temple) {
                applyTempleAndReturn(slot);
            } else if (kind_ == Kind::Training) {
                applyTrainingAndReturn(slot);
            } else if (kind_ == Kind::Smith) {
                applySmithBuyAndReturn(slot);
            }
        }
        break;
    }
}

void PlayTownServiceUi::render(gfx::ScreenCompositor &c) const
{
    if (!active_) {
        return;
    }

    /* FAITHFUL PRESENTATION: the original town services are NOT a full-screen
     * modal. The handler menus render in the lower text/console band (the same
     * region the event-script text uses, EventTextView rows ~16..23, per doc 15
     * §4 "Game screen layout") while the 3D viewport + party panel stay on screen.
     * So we draw
     * ONLY in that band (clearing it to the console black, JAM2 bg pen 0) instead
     * of the old fabricated fullscreen backdrop. */
    constexpr int kBandTopRow = 16; /* below the 3D viewport frame (row 16, px 132) */
    constexpr int kBandBotRow = 23; /* status/prompt row used by the event text view */
    constexpr int kLeftCol = 1;
    constexpr int kRightCol = 38;
    c.clearRect(kLeftCol * 8, kBandTopRow * 8, (kRightCol - kLeftCol + 1) * 8,
                (kBandBotRow - kBandTopRow + 1) * 8, 0, 0, 0, 255);

    const int col = kLeftCol + 1; /* 2 */
    int row = kBandTopRow;        /* 16 */

    char title[48];
    const char *suffix = (kind_ == Kind::Temple)     ? "Temple"
                         : (kind_ == Kind::Training)  ? "Training Hall"
                                                      : "Blacksmith";
    std::snprintf(title, sizeof(title), "%s %s", townName(ctx_.map_id), suffix);
    drawCell(c, row, col, title, 255, 230, 120);
    row += 2; /* 18 */

    if (phase_ == Phase::Menu) {
        if (kind_ == Kind::Temple) {
            drawCell(c, row++, col, "A) Heal (HP + condition)");
            drawCell(c, row++, col, "B) Restore Condition");
            drawCell(c, row++, col, "C) Donate");
        } else if (kind_ == Kind::Smith) {
            drawCell(c, row++, col, "A) Weapons  B) Specials");
            drawCell(c, row++, col, "C) Armor    D) Misc");
        }
    } else if (phase_ == Phase::SmithItems) {
        /* Six priced slots fit rows 17..22 (title 16, prompt 23). */
        row = kBandTopRow + 1; /* 17 */
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
                std::snprintf(line, sizeof(line), "%d) %-13s %u gp", i + 1, item,
                              smith_view_[i].price);
                drawCell(c, row++, col, line);
            } else {
                std::snprintf(line, sizeof(line), "%d) ---", i + 1);
                drawCell(c, row++, col, line, 130, 130, 130);
            }
        }
    } else if (phase_ == Phase::Member) {
        /* The party roster is already on the right panel (slot numbers); the
         * console just prompts which one — matching the engine's member pick. */
        const char *q = (kind_ == Kind::Training) ? "Train which character? (1-8)"
                        : (kind_ == Kind::Smith)   ? "Buy for which character? (1-8)"
                                                   : "Which character? (1-8)";
        drawCell(c, row, col, q);
    }

    /* Feedback + prompt on the lower rows of the console band. */
    if (status_[0]) {
        drawCell(c, kBandBotRow - 1, col, status_, 160, 255, 160);
    }

    const char *prompt = nullptr;
    switch (phase_) {
    case Phase::Menu:
        prompt = (kind_ == Kind::Temple) ? "Select A-C, Esc to leave"
                                         : "Select A-D, Esc to leave";
        break;
    case Phase::SmithItems:
        prompt = "Select 1-6, Esc to go back";
        break;
    case Phase::Member:
        prompt = (kind_ == Kind::Training) ? "Press 1-8, Esc to leave"
                                           : "Press 1-8, Esc to go back";
        break;
    }
    if (prompt) {
        drawCell(c, kBandBotRow, col, prompt, 200, 200, 255);
    }
}

}  // namespace mm2::ui
