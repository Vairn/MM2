#include "mm2/ui/ICharacterUi.h"

#include "mm2/CppStdCompat.h"
#include "mm2/ui/CharacterUiFactory.h"
#include "mm2_party_launch.h"

namespace mm2::ui {

namespace {

constexpr int kPlayableSlots = 48;

static const char *kClassNames[] = {
    "Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian",
};
static const char *kRaceNames[] = {
    "Human", "Elf", "Dwarf", "Gnome", "Half-Orc",
};
static const char *kAlignNames[] = {
    "Good", "Neutral", "Evil",
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

const char *className(uint8_t id)
{
    if (id < sizeof(kClassNames) / sizeof(kClassNames[0])) {
        return kClassNames[id];
    }
    return "?";
}

const char *raceName(uint8_t id)
{
    if (id < sizeof(kRaceNames) / sizeof(kRaceNames[0])) {
        return kRaceNames[id];
    }
    return "?";
}

const char *alignName(uint8_t id)
{
    if (id < 3) {
        return kAlignNames[id];
    }
    return "?";
}

class StubCharacterUi final : public ICharacterUi {
public:
    bool init(const char *data_dir) override
    {
        data_dir_ = data_dir;
        return true;
    }

    void shutdown() override {}

    void beginViewParty(Mm2RosterFile &roster) override
    {
        roster_ = &roster;
        rebuildIndices();
        cursor_ = 0;
        mode_ = ViewMode::List;
    }

    UiResult tickViewParty(const platform::KeyState &keys) override
    {
        if (keys.escape) {
            if (mode_ == ViewMode::Detail) {
                mode_ = ViewMode::List;
                return UiResult::Continue;
            }
            return UiResult::Cancel;
        }
        if (mode_ == ViewMode::List) {
            if (keys.up && count_ > 0) {
                cursor_ = (cursor_ + count_ - 1) % count_;
            }
            if (keys.down && count_ > 0) {
                cursor_ = (cursor_ + 1) % count_;
            }
            if (keys.enter && count_ > 0) {
                view_slot_ = slots_[cursor_];
                mode_ = ViewMode::Detail;
            }
        } else if (keys.enter) {
            mode_ = ViewMode::List;
        }
        return UiResult::Continue;
    }

    void renderViewParty(gfx::ScreenCompositor &compositor) override
    {
        compositor.clear(16, 16, 32, 255);
        if (mode_ == ViewMode::List) {
            drawList(compositor);
        } else {
            drawDetail(compositor);
        }
    }

    void beginCreateCharacter(Mm2RosterFile &roster, int slot) override
    {
        roster_ = &roster;
        create_slot_ = slot >= 0 ? slot : 0;
        message_ = "Create — slot picker (stub UI)";
    }

    UiResult tickCreateCharacter(const platform::KeyState &keys) override
    {
        if (keys.escape) {
            return UiResult::Cancel;
        }
        if (keys.up) {
            create_slot_ = (create_slot_ + kPlayableSlots - 1) % kPlayableSlots;
        }
        if (keys.down) {
            create_slot_ = (create_slot_ + 1) % kPlayableSlots;
        }
        if (keys.enter) {
            message_ = "Create confirm — ASM @ $944C pending (stub)";
            return UiResult::Cancel;
        }
        return UiResult::Continue;
    }

    void renderCreateCharacter(gfx::ScreenCompositor &compositor) override
    {
        compositor.clear(16, 16, 32, 255);
        compositor.drawTextShadow(8, 8, "CREATE CHARACTER (stub UI)");
        char line[64];
        std::snprintf(line, sizeof(line), "Slot %d — Up/Down Enter Esc", create_slot_);
        compositor.drawText(8, 28, line, 200, 200, 200, 255);
        compositor.drawText(8, 44, message_, 180, 180, 180, 255);
    }

    void beginChooseParty(Mm2RosterFile &roster) override
    {
        roster_ = &roster;
        party_town_ = 1;
        party_page_hirelings_ = false;
        party_count_ = 0;
        has_party_launch_ = false;
    }

    UiResult tickChooseParty(const platform::KeyState &keys) override
    {
        // ESC on the choose-party screen exits the game (controller maps Quit).
        if (keys.escape) {
            return UiResult::Quit;
        }
        if (keys.last_ascii == 'Z') {
            if (party_count_ <= 0) {
                return UiResult::Continue;
            }
            mm2_party_launch_build(&party_launch_, static_cast<uint8_t>(party_town_), party_members_,
                                   party_count_);
            has_party_launch_ = true;
            return UiResult::Done;
        }
        if (keys.space) {
            party_page_hirelings_ = !party_page_hirelings_;
            return UiResult::Continue;
        }
        if (keys.last_ascii >= '1' && keys.last_ascii <= '5') {
            party_town_ = keys.last_ascii - '0';
            return UiResult::Continue;
        }
        if (keys.ctrl && keys.last_ascii >= 'A' && keys.last_ascii <= 'X') {
            int display[24];
            const int count = buildPartyDisplayList(display);
            const int idx = keys.last_ascii - 'A';
            if (idx < count) {
                togglePartyMember(display[idx]);
            }
        }
        return UiResult::Continue;
    }

    void renderChooseParty(gfx::ScreenCompositor &compositor) override
    {
        compositor.clear(16, 16, 32, 255);
        char title[48];
        std::snprintf(title, sizeof(title), "CHOOSE PARTY  Town %d  %s", party_town_,
                      party_page_hirelings_ ? "(Hirelings)" : "(Characters)");
        compositor.drawTextShadow(8, 8, title);

        int char_count = 0;
        for (int i = 0; i < party_count_; ++i) {
            if (party_members_[i] < 24) {
                ++char_count;
            }
        }
        char counts[48];
        std::snprintf(counts, sizeof(counts), "PARTY C=%d / H=%d%s", char_count, party_count_ - char_count,
                      party_count_ >= 8 ? "  *** FULL ***" : "");
        compositor.drawText(8, 22, counts, 220, 220, 180, 255);

        int display[24];
        const int count = buildPartyDisplayList(display);
        for (int i = 0; i < count && i < 18; ++i) {
            const int slot = display[i];
            const Mm2RosterRecord &rec = roster_->records[slot];
            char name[16];
            mm2_roster_name_to_cstr(&rec, name, sizeof(name));
            char line[64];
            std::snprintf(line, sizeof(line), "%c%c- %-11s %s", isPartyMember(slot) ? '*' : ' ',
                          static_cast<char>('A' + i), name, className(rec.class_id));
            compositor.drawText(8, 38 + i * 9, line, 200, 200, 200, 255);
        }
        compositor.drawText(8, 38 + 19 * 9, "1-5 town  Space pg  Ctrl+A-X add  Z begin  Esc quit", 160, 160, 160, 255);
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
    enum class ViewMode { List, Detail };

    int buildPartyDisplayList(int *out) const
    {
        const int start = party_page_hirelings_ ? 24 : 0;
        int count = 0;
        for (int slot = start; slot < start + 24 && slot < kPlayableSlots && count < 24; ++slot) {
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
        if (party_count_ >= 8) {
            return;
        }
        int char_count = 0;
        for (int i = 0; i < party_count_; ++i) {
            if (party_members_[i] < 24) {
                ++char_count;
            }
        }
        if (slot < 24 && char_count >= 6) {
            return;
        }
        party_members_[party_count_++] = slot;
    }

    void rebuildIndices()
    {
        count_ = 0;
        for (int i = 0; i < kPlayableSlots; ++i) {
            if (!mm2_roster_slot_is_empty(&roster_->records[i])) {
                slots_[count_++] = i;
            }
        }
        if (cursor_ >= count_) {
            cursor_ = count_ > 0 ? count_ - 1 : 0;
        }
    }

    void drawList(gfx::ScreenCompositor &c)
    {
        c.drawTextShadow(8, 8, "PARTY ROSTER (stub UI)");
        c.drawText(8, 22, "Up/Down Enter=sheet Esc=menu", 180, 180, 180, 255);
        if (count_ == 0) {
            c.drawTextShadow(8, 48, "No characters in roster.");
            return;
        }
        const int line_h = 10;
        for (int row = 0; row < count_ && row < 18; ++row) {
            const int slot = slots_[row];
            const Mm2RosterRecord &rec = roster_->records[slot];
            char name[16];
            mm2_roster_name_to_cstr(&rec, name, sizeof(name));
            char line[96];
            std::snprintf(line, sizeof(line), "%2d %-11s Lv%2u HP%4u/%4u", slot, name, rec.level,
                          rec.hp_current, rec.hp_max);
            const int y = 36 + row * line_h;
            if (row == cursor_) {
                c.fillRect(6, y - 1, 308, line_h, 48, 64, 96, 255);
                c.drawText(8, y, line, 255, 255, 128, 255);
            } else {
                c.drawText(8, y, line, 200, 200, 200, 255);
            }
        }
    }

    void drawDetail(gfx::ScreenCompositor &c)
    {
        const Mm2RosterRecord &rec = roster_->records[view_slot_];
        char name[16];
        mm2_roster_name_to_cstr(&rec, name, sizeof(name));
        c.drawTextShadow(8, 8, name);
        char buf[64];
        int y = 28;
        std::snprintf(buf, sizeof(buf), "%s / %s", className(rec.class_id), raceName(rec.race));
        c.drawText(8, y, buf, 220, 220, 220, 255);
        y += 12;
        std::snprintf(buf, sizeof(buf), "Lv%2u  HP %u/%u  SP %u/%u", rec.level, rec.hp_current, rec.hp_max,
                      rec.sp_current, rec.sp_max);
        c.drawText(8, y, buf, 220, 220, 220, 255);
    }

    const char *data_dir_ = nullptr;
    Mm2RosterFile *roster_ = nullptr;
    ViewMode mode_ = ViewMode::List;
    int slots_[kPlayableSlots]{};
    int count_ = 0;
    int cursor_ = 0;
    int view_slot_ = 0;
    int create_slot_ = 0;
    const char *message_ = "";

    int party_town_ = 1;
    bool party_page_hirelings_ = false;
    int party_members_[8] = {0};
    int party_count_ = 0;
    Mm2PartyLaunch party_launch_{};
    bool has_party_launch_ = false;
};

}  // namespace

std::unique_ptr<ICharacterUi> makeStubCharacterUi()
{
    return std::make_unique<StubCharacterUi>();
}

}  // namespace mm2::ui
