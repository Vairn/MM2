#include "sections/ItemsSection.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "app/App.h"
#include "core/Spells.h"
#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace mm2 {

namespace {

struct ClassBit {
    const char* name;
    uint8_t bit;
};
const ClassBit kClasses[] = {
    {"Knight", ITEM_CLASS_KNIGHT},     {"Paladin", ITEM_CLASS_PALADIN},
    {"Archer", ITEM_CLASS_ARCHER},     {"Cleric", ITEM_CLASS_CLERIC},
    {"Sorcerer", ITEM_CLASS_SORCERER}, {"Robber", ITEM_CLASS_ROBBER},
    {"Ninja", ITEM_CLASS_NINJA},       {"Barbarian", ITEM_CLASS_BARBARIAN},
};

// Non-spell use-power kinds (hi nibble of effect byte when < 0x80). Kind 4 unused.
const char* kBoostKindNames[] = {"Max HP", "Might", "Speed", "Accuracy",
                                 "(unused)", "Level", "Spell Level"};
constexpr int kBoostKindCount = 7;

void setEffectByte(ItemRecord& r, uint8_t b) {
    r.effectType = static_cast<uint8_t>((b >> 4) & 0x0F);
    r.effectAmount = static_cast<uint8_t>(b & 0x0F);
}

int effectMode(uint8_t b) {
    if (b == 0) return 0;           // None
    if (b < 0x80) return 1;         // Boost
    if (b <= 0xB0) return 2;        // Sorcerer spell
    return 3;                       // Cleric spell
}

int spellFlatFromEffect(uint8_t b) {
    if (b >= 0x81 && b <= 0xB0) return b - 0x80;
    if (b >= 0xB1 && b <= 0xE0) return b - 0xB0;
    return 1;
}

uint8_t encodeSpellEffect(SpellSchool school, int flat) {
    if (flat < 1) flat = 1;
    if (flat > kSpellsPerSchool) flat = kSpellsPerSchool;
    return static_cast<uint8_t>((school == SpellSchool::Sorcerer ? 0x80 : 0xB0) + flat);
}

bool spellPicker(const char* id, SpellSchool school, int* flat) {
    const SpellInfo* cur = spellInfo(school, *flat);
    char preview[64];
    if (cur)
        snprintf(preview, sizeof(preview), "%d/%d %s", cur->level, cur->number, cur->name);
    else
        snprintf(preview, sizeof(preview), "#%d", *flat);

    bool changed = false;
    if (ImGui::BeginCombo(id, preview)) {
        for (int f = 1; f <= kSpellsPerSchool; ++f) {
            const SpellInfo* s = spellInfo(school, f);
            if (!s) continue;
            char label[72];
            snprintf(label, sizeof(label), "%d/%d  %s", s->level, s->number, s->name);
            const bool sel = (*flat == f);
            if (ImGui::Selectable(label, sel)) {
                *flat = f;
                changed = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

}  // namespace

bool ItemsSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    return loaded;
}

bool ItemsSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

void ItemsSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("items.dat not loaded.");
        return;
    }
    if (selected_ < 0 || selected_ >= kItemsCount) selected_ = 0;

    if (!ui::BeginMasterList(layout_, "item_list", "Items")) {
        return;
    }
    for (int i = 0; i < kItemsCount; ++i) {
        std::string nm = file_.records[i].nameStr();
        char hay[48];
        snprintf(hay, sizeof(hay), "%d %s", i, nm.c_str());
        if (!ui::ListFilterPass(layout_, hay)) continue;

        char label[48];
        snprintf(label, sizeof(label), "%3d  %s", i, nm.empty() ? "(blank)" : nm.c_str());
        if (ImGui::Selectable(label, selected_ == i)) selected_ = i;
    }
    ui::EndMasterListBeginDetail(layout_, "item_detail");

    ItemRecord& r = file_.records[selected_];

    char nameBuf[kItemNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kItemNameSize] = '\0';

    char sub[96];
    snprintf(sub, sizeof(sub), "#%d · %s · dmg %u · %u gp", selected_,
             describeItemEffect(r.effectByte()).c_str(), r.damage, r.gold);
    ui::PanelHeader(nm.empty() ? "(blank item)" : nm.c_str(), sub);

    // —— Identity ——
    ui::SectionBlock("Identity");
    {
        const float labelW = ui::Em(4.5f);
        if (ImGui::BeginTable("item_id", 6,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn("l0", ImGuiTableColumnFlags_WidthFixed, labelW);
            ImGui::TableSetupColumn("f0", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("l1", ImGuiTableColumnFlags_WidthFixed, labelW);
            ImGui::TableSetupColumn("f1", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("l2", ImGuiTableColumnFlags_WidthFixed, labelW);
            ImGui::TableSetupColumn("f2", ImGuiTableColumnFlags_WidthStretch);

            auto lab = [](const char* t) {
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(t);
            };
            auto fld = [](const std::function<void()>& fn) {
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                fn();
            };

            ImGui::TableNextRow();
            lab("Name");
            fld([&] {
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                    r.setName(nameBuf);
                    dirty = true;
                }
            });
            lab("Gold");
            fld([&] {
                ImGui::SetNextItemWidth(-FLT_MIN);
                int gold = r.gold;
                if (ImGui::InputInt("##gold", &gold, 0, 0)) {
                    if (gold < 0) gold = 0;
                    if (gold > 0xFFFF) gold = 0xFFFF;
                    r.gold = static_cast<uint16_t>(gold);
                    dirty = true;
                }
            });
            lab("Damage");
            fld([&] {
                ImGui::SetNextItemWidth(-FLT_MIN);
                int dmg = r.damage;
                if (ImGui::InputInt("##damage", &dmg, 0, 0)) {
                    if (dmg < 0) dmg = 0;
                    if (dmg > 0xFF) dmg = 0xFF;
                    r.damage = static_cast<uint8_t>(dmg);
                    dirty = true;
                }
            });
            ImGui::EndTable();
        }
        ImGui::TextDisabled("name 12 chars · gold u16 LE on disk");
    }

    // —— Class restriction ——
    ui::SectionBlock("Usable by class", "checked = allowed  (bit clear in +0x0D)");
    {
        if (ImGui::Button("Allow all")) {
            r.forbiddenClassMask = 0;
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Forbid all")) {
            r.forbiddenClassMask = 0xFF;
            dirty = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("mask 0x%02X", r.forbiddenClassMask);

        ui::CheckboxGrid("item_classes", 4, 8, [&](int c) {
            bool canUse = (r.forbiddenClassMask & kClasses[c].bit) == 0;
            if (ImGui::Checkbox(kClasses[c].name, &canUse)) {
                r.setUsableBy(static_cast<ItemClassBit>(kClasses[c].bit), canUse);
                dirty = true;
            }
        });
    }

    // —— Equipped bonus ——
    ui::SectionBlock("Equipped bonus", "+0x0E special power while worn");
    {
        const float labelW = ui::Em(5.5f);
        if (ImGui::BeginTable("item_bonus", 4,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn("l0", ImGuiTableColumnFlags_WidthFixed, labelW);
            ImGui::TableSetupColumn("f0", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("l1", ImGuiTableColumnFlags_WidthFixed, labelW);
            ImGui::TableSetupColumn("f1", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Type");
            ImGui::TableNextColumn();
            {
                ImGui::SetNextItemWidth(-FLT_MIN);
                int bType = r.bonusType & 0x0F;
                if (ImGui::Combo("##btype", &bType, kItemBonusTypeNames, kItemBonusTypeCount)) {
                    r.bonusType = static_cast<uint8_t>(bType & 0x0F);
                    dirty = true;
                }
            }
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Amount");
            ImGui::TableNextColumn();
            {
                ImGui::SetNextItemWidth(-FLT_MIN);
                int bAmt = r.bonusAmount;
                if (ImGui::InputInt("##bamt", &bAmt, 0, 0)) {
                    if (bAmt < 0) bAmt = 0;
                    if (bAmt > 0x0F) bAmt = 0x0F;
                    r.bonusAmount = static_cast<uint8_t>(bAmt);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("0 = none  (byte 0x%02X)", r.bonusByte());
            }
            ImGui::EndTable();
        }
    }

    // —— Use power ——
    ui::SectionBlock("Use power", "+0x0F flat spell index / boost");
    {
        uint8_t eb = r.effectByte();
        int mode = effectMode(eb);
        const char* modeNames = "None\0Stat boost\0Sorcerer spell\0Cleric spell\0";

        const float labelW = ui::Em(5.5f);
        if (ImGui::BeginTable("item_effect", 2,
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelW);
            ImGui::TableSetupColumn("field", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Kind");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ui::Em(14.f));
            if (ImGui::Combo("##eff_mode", &mode, modeNames)) {
                switch (mode) {
                    case 0:
                        setEffectByte(r, 0);
                        break;
                    case 1:
                        setEffectByte(r, 0x11);  // Might +1 default
                        break;
                    case 2:
                        setEffectByte(r, encodeSpellEffect(SpellSchool::Sorcerer, 1));
                        break;
                    case 3:
                        setEffectByte(r, encodeSpellEffect(SpellSchool::Cleric, 1));
                        break;
                }
                dirty = true;
                eb = r.effectByte();
            }

            if (mode == 1) {
                int kind = (eb >> 4) & 0x0F;
                int amt = eb & 0x0F;
                if (kind >= kBoostKindCount) kind = 0;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Boost");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ui::Em(12.f));
                if (ImGui::Combo("##boost_kind", &kind, kBoostKindNames, kBoostKindCount)) {
                    setEffectByte(r, static_cast<uint8_t>(((kind & 0x0F) << 4) | (amt & 0x0F)));
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ui::Em(4.f));
                if (ImGui::InputInt("##boost_amt", &amt, 0, 0)) {
                    if (amt < 0) amt = 0;
                    if (amt > 0x0F) amt = 0x0F;
                    setEffectByte(r, static_cast<uint8_t>(((kind & 0x0F) << 4) | (amt & 0x0F)));
                    dirty = true;
                }
            } else if (mode == 2 || mode == 3) {
                SpellSchool school =
                    (mode == 2) ? SpellSchool::Sorcerer : SpellSchool::Cleric;
                int flat = spellFlatFromEffect(eb);
                if (flat < 1) flat = 1;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Spell");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (spellPicker("##spell", school, &flat)) {
                    setEffectByte(r, encodeSpellEffect(school, flat));
                    dirty = true;
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Summary");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(describeItemEffect(r.effectByte()).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(byte 0x%02X)", r.effectByte());

            ImGui::EndTable();
        }
    }

    if (ui::BeginHexBlock("Raw record")) {
        ImGui::TextDisabled("Record %d @ 0x%04X (%d bytes)", selected_,
                            selected_ * kItemRecordSize, kItemRecordSize);
        Bytes rec = file_.encode();
        DrawHexView("item_hex", rec.data() + selected_ * kItemRecordSize, kItemRecordSize,
                    selected_ * kItemRecordSize);
        ui::EndHexBlock();
    }

    ui::EndMasterDetail();
}

}  // namespace mm2
