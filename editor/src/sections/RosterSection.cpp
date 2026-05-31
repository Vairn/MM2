#include "sections/RosterSection.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "app/App.h"
#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/UiLayout.h"

namespace mm2 {

bool RosterSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    return loaded;
}

bool RosterSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

void RosterSection::drawStats(RosterRecord& r) {
    using namespace roster_off;
    struct U8Field {
        const char* label;
        int off;
    };
    const U8Field stats[] = {
        {"Might", kMight},       {"Intellect", kIntellect}, {"Personality", kPersonality},
        {"Speed", kSpeed},       {"Accuracy", kAccuracy},   {"Luck", kLuck},
        {"Level", kLevel},       {"Age", kAge},             {"Armor class", kArmorClass},
        {"Food", kFood},         {"Condition", kCondition}, {"Thievery %", kThievery},
        {"Sex", kSex},           {"Race", kRace},           {"Class", kClass},
        {"Alignment", kAlignCur},
    };

    ui::FormGrid grid("roster_stats");
    if (grid.begin()) {
        for (int i = 0; i < static_cast<int>(sizeof(stats) / sizeof(stats[0])); i += 2) {
            const U8Field& f0 = stats[i];
            const U8Field& f1 = stats[i + 1];
            grid.row2(
                f0.label,
                [&] {
                    ui::SetFieldStretch();
                    int v = r.u8(f0.off);
                    if (ImGui::InputInt(("##" + std::to_string(f0.off)).c_str(), &v)) {
                        r.setU8(f0.off, static_cast<uint8_t>(v & 0xFF));
                        dirty = true;
                    }
                },
                f1.label,
                [&] {
                    ui::SetFieldStretch();
                    int v = r.u8(f1.off);
                    if (ImGui::InputInt(("##" + std::to_string(f1.off)).c_str(), &v)) {
                        r.setU8(f1.off, static_cast<uint8_t>(v & 0xFF));
                        dirty = true;
                    }
                });
        }
    }

    ImGui::SeparatorText("Pools & wealth");
    {
        ui::FormTable form("roster_pools");
        if (form.begin()) {
            struct U16Field {
                const char* label;
                int off;
            };
            const U16Field pools[] = {
                {"HP current", kHpCur}, {"HP max", kHpMax}, {"SP current", kSpCur},
                {"SP max", kSpMax},     {"Gems", kGems},
            };
            for (const U16Field& f : pools) {
                form.row(f.label, [&] {
                    ui::SetFieldMed();
                    int v = r.u16(f.off);
                    if (ImGui::InputInt(("##" + std::to_string(f.off)).c_str(), &v)) {
                        if (v < 0) v = 0;
                        if (v > 0xFFFF) v = 0xFFFF;
                        r.setU16(f.off, static_cast<uint16_t>(v));
                        dirty = true;
                    }
                });
            }
            form.row("Gold", [&] {
                ui::SetFieldMed();
                int gold = static_cast<int>(r.u32(kGold));
                if (ImGui::InputInt("##gold", &gold)) {
                    if (gold < 0) gold = 0;
                    r.setU32(kGold, static_cast<uint32_t>(gold));
                    dirty = true;
                }
            });
            form.row("Experience", [&] {
                ui::SetFieldMed();
                int exp = static_cast<int>(r.u32(kExperience));
                if (ImGui::InputInt("##exp", &exp)) {
                    if (exp < 0) exp = 0;
                    r.setU32(kExperience, static_cast<uint32_t>(exp));
                    dirty = true;
                }
            });
        }
    }

    ImGui::SeparatorText("Base stats (+0x6A..0x73)");
    {
        struct U8Field {
            const char* label;
            int off;
        };
        const U8Field base[] = {
            {"Alignment base", kAlignBase},   {"Might base", kMightBase},
            {"Intellect base", kIntellectBase}, {"Personality base", kPersonalityBase},
            {"Speed base", kSpeedBase},       {"Accuracy base", kAccuracyBase},
            {"Luck base", kLuckBase},         {"Endurance base", kEnduranceBase},
            {"Level", kLevel},                {"Spell level", kSpellLevel},
        };
        ui::FormGrid grid("roster_base");
        if (grid.begin()) {
            for (int i = 0; i < static_cast<int>(sizeof(base) / sizeof(base[0])); i += 2) {
                const U8Field& f0 = base[i];
                const U8Field& f1 = base[i + 1];
                grid.row2(
                    f0.label,
                    [&] {
                        ui::SetFieldStretch();
                        int v = r.u8(f0.off);
                        if (ImGui::InputInt(("##" + std::to_string(f0.off)).c_str(), &v)) {
                            r.setU8(f0.off, static_cast<uint8_t>(v & 0xFF));
                            dirty = true;
                        }
                    },
                    f1.label,
                    [&] {
                        ui::SetFieldStretch();
                        int v = r.u8(f1.off);
                        if (ImGui::InputInt(("##" + std::to_string(f1.off)).c_str(), &v)) {
                            r.setU8(f1.off, static_cast<uint8_t>(v & 0xFF));
                            dirty = true;
                        }
                    });
            }
        }
    }
}

void RosterSection::drawEquipment(App& app, RosterRecord& r) {
    auto drawSlots = [&](const char* title, int base) {
        ImGui::SeparatorText(title);
        if (!ImGui::BeginTable(title, 5,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                   ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit |
                                   ImGuiTableFlags_ScrollY, ImVec2(0, 220))) {
            return;
        }
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.f);
        ImGui::TableSetupColumn("Item ID", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Bonus", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < kRosterItemSlots; ++i) {
            RosterItemSlot s = r.slot(base, i);
            ImGui::TableNextRow();
            ImGui::PushID(base * 16 + i);

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%d", i);

            ImGui::TableNextColumn();
            int id = s.itemId;
            ui::SetFieldStretch();
            if (ImGui::InputInt("##id", &id)) {
                s.itemId = static_cast<uint8_t>(id & 0xFF);
                r.setSlot(base, i, s);
                dirty = true;
            }

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(app.itemName(s.itemId).c_str());

            ImGui::TableNextColumn();
            int bonus = s.bonus;
            ui::SetFieldStretch();
            if (ImGui::InputInt("##bonus", &bonus)) {
                s.bonus = static_cast<uint8_t>(bonus & 0xFF);
                r.setSlot(base, i, s);
                dirty = true;
            }

            ImGui::TableNextColumn();
            int flags = s.flags;
            ui::SetFieldStretch();
            if (ImGui::InputInt("##flags", &flags)) {
                s.flags = static_cast<uint8_t>(flags & 0xFF);
                r.setSlot(base, i, s);
                dirty = true;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    };
    drawSlots("Equipped", roster_off::kEquipped);
    drawSlots("Backpack", roster_off::kBackpack);
}

void RosterSection::drawSpells(RosterRecord& r) {
    ImGui::SeparatorText("Spell bitmask (48 bits)");
    ui::CheckboxGrid("roster_spells", 12, 48, [&](int spell) {
        int byte = spell / 8;
        int bit = spell % 8;
        uint8_t v = r.raw[roster_off::kSpells + byte];
        bool on = (v >> bit) & 1;
        ImGui::PushID(spell);
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%d", spell);
        if (ImGui::Checkbox(lbl, &on)) {
            if (on) r.raw[roster_off::kSpells + byte] |= (1 << bit);
            else r.raw[roster_off::kSpells + byte] &= ~(1 << bit);
            dirty = true;
        }
        ImGui::PopID();
    });
}

void RosterSection::draw(App& app) {
    if (!loaded) {
        ImGui::TextDisabled("roster.dat not loaded.");
        return;
    }

    ui::BeginListPanel("roster_list");
    for (int i = 0; i < kRosterCount; ++i) {
        char label[64];
        std::string nm = file_.records[i].nameStr();
        snprintf(label, sizeof(label), "%2d  %s", i, nm.empty() ? "(empty)" : nm.c_str());
        if (ImGui::Selectable(label, selected_ == i)) selected_ = i;
    }
    ui::ListPanelNextDetail("roster_detail");

    RosterRecord& r = file_.records[selected_];

    char nameBuf[kRosterNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kRosterNameSize] = '\0';

    {
        ui::FormTable form("roster_name");
        if (form.begin()) {
            form.row("Name", [&] {
                ui::SetFieldWide();
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                    r.setName(nameBuf);
                    dirty = true;
                }
            });
        }
    }

    if (ImGui::BeginTabBar("roster_tabs")) {
        if (ImGui::BeginTabItem("Stats")) {
            drawStats(r);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Equipment")) {
            drawEquipment(app, r);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Spells")) {
            drawSpells(r);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hex")) {
            ImGui::Text("Record %d @ 0x%04X (%d bytes)", selected_, selected_ * kRosterRecordSize,
                        kRosterRecordSize);
            DrawHexView("roster_hex", r.raw.data(), kRosterRecordSize, selected_ * kRosterRecordSize);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ui::EndDetailPanel();
}

}  // namespace mm2
