#include "sections/SpellsSection.h"

#include <cstdio>

#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/UiLayout.h"

namespace mm2 {

bool SpellsSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    return loaded;
}

bool SpellsSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

static const char* castName(SpellCast c) {
    switch (c) {
        case SpellCast::Combat: return "Combat only";
        case SpellCast::NonCombat: return "Non-combat only";
        default: return "Anytime";
    }
}

void SpellsSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ImGui::TextDisabled("spells.dat not loaded.");
        return;
    }

    ui::BeginListPanel("spell_list");
    int prevSchool = -1;
    for (int i = 0; i < kSpellsRecordCount; ++i) {
        const SpellInfo& info = SpellsFile::info(i);
        int sch = static_cast<int>(info.school);
        if (sch != prevSchool) {
            ImGui::SeparatorText(info.school == SpellSchool::Sorcerer ? "Sorcerer" : "Cleric");
            prevSchool = sch;
        }
        char label[64];
        char prefix = info.school == SpellSchool::Sorcerer ? 'S' : 'C';
        snprintf(label, sizeof(label), "%c%d/%d  %s", prefix, info.level, info.number, info.name);
        if (ImGui::Selectable(label, selected_ == i)) selected_ = i;
    }
    ui::ListPanelNextDetail("spell_detail");

    const SpellInfo& info = SpellsFile::info(selected_);
    SpellRecord& r = file_.records[selected_];
    char prefix = info.school == SpellSchool::Sorcerer ? 'S' : 'C';

    ImGui::Text("%c%d/%d  %s", prefix, info.level, info.number, info.name);
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", info.school == SpellSchool::Sorcerer ? "Sorcerer" : "Cleric");

    ImGui::SeparatorText("File record (spells.dat)");
    {
        ui::FormTable form("spell_file");
        if (form.begin()) {
            form.row("Cast", [&] {
                int cur = static_cast<int>(r.cast());
                const char* items[] = {"Anytime", "Combat only", "Non-combat only"};
                ui::SetFieldWide();
                if (ImGui::Combo("##cast", &cur, items, 3)) {
                    r.setCast(static_cast<SpellCast>(cur));
                    dirty = true;
                }
                ImGui::SameLine();
                bool outdoor = r.outdoor();
                if (ImGui::Checkbox("Outdoor only", &outdoor)) {
                    r.setOutdoor(outdoor);
                    dirty = true;
                }
            });
            form.row("Spell points", [&] {
                bool pl = r.perLevel();
                int sp = r.sp();
                ui::SetFieldShort();
                if (ImGui::InputInt("##sp", &sp)) {
                    if (sp < 0) sp = 0;
                    if (sp > 15) sp = 15;
                    r.setCost(sp, pl);
                    dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("per caster level", &pl)) {
                    r.setCost(r.sp(), pl);
                    dirty = true;
                }
            });
            form.row("Gems", [&] {
                int g = r.gems();
                ui::SetFieldShort();
                if (ImGui::InputInt("##gems", &g)) {
                    if (g < 0) g = 0;
                    if (g > 15) g = 15;
                    r.setGems(g);
                    dirty = true;
                }
                if (r.specialCost()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "special (hard-coded)");
                }
            });
            form.row("Cost", [&] {
                char buf[48];
                if (r.perLevel())
                    snprintf(buf, sizeof(buf), "%d/L", r.sp());
                else
                    snprintf(buf, sizeof(buf), "%d SP", r.sp());
                std::string s = buf;
                if (r.gems() > 0 || r.specialCost()) {
                    snprintf(buf, sizeof(buf), " + %d Gem%s%s", r.gems(),
                             r.gems() == 1 ? "" : "s", r.specialCost() ? "+" : "");
                    s += buf;
                }
                ImGui::TextUnformatted(s.c_str());
            });
        }
    }

    ImGui::SeparatorText("Manual reference");
    {
        ui::FormTable form("spell_ref");
        if (form.begin()) {
            form.row("Cost", [&] { ImGui::TextUnformatted(formatSpellCost(info).c_str()); });
            form.row("Type", [&] {
                if (info.where[0])
                    ImGui::Text("%s, %s", castName(info.cast), info.where);
                else
                    ImGui::TextUnformatted(castName(info.cast));
            });
            form.row("Object", [&] { ImGui::TextUnformatted(info.object); });
        }
    }
    ImGui::TextWrapped("%s", info.desc);

    ImGui::SeparatorText("Raw record");
    int off = selected_ * kSpellRecordSize;
    ImGui::Text("Record %d @ 0x%02X  bytes %02X %02X", selected_, off, r.b0, r.b1);
    uint8_t bytes[2] = {r.b0, r.b1};
    DrawHexView("spell_hex", bytes, 2, off);

    ui::EndDetailPanel();
}

}  // namespace mm2
