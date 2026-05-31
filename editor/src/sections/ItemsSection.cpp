#include "sections/ItemsSection.h"

#include <cstdio>
#include <cstring>

#include "app/App.h"
#include "core/Spells.h"
#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/UiLayout.h"

namespace mm2 {

namespace {
struct ClassBit {
    const char* name;
    uint8_t bit;
};
const ClassBit kClasses[] = {
    {"Knight", ITEM_CLASS_KNIGHT},   {"Paladin", ITEM_CLASS_PALADIN},
    {"Archer", ITEM_CLASS_ARCHER},   {"Cleric", ITEM_CLASS_CLERIC},
    {"Sorcerer", ITEM_CLASS_SORCERER}, {"Robber", ITEM_CLASS_ROBBER},
    {"Ninja", ITEM_CLASS_NINJA},     {"Barbarian", ITEM_CLASS_BARBARIAN},
};
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
        ImGui::TextDisabled("items.dat not loaded.");
        return;
    }

    ui::BeginListPanel("item_list");
    for (int i = 0; i < kItemsCount; ++i) {
        char label[64];
        std::string nm = file_.records[i].nameStr();
        snprintf(label, sizeof(label), "%3d  %s", i, nm.empty() ? "(blank)" : nm.c_str());
        if (ImGui::Selectable(label, selected_ == i)) selected_ = i;
    }
    ui::ListPanelNextDetail("item_detail");

    ItemRecord& r = file_.records[selected_];

    char nameBuf[kItemNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kItemNameSize] = '\0';

    {
        ui::FormTable form("item_identity");
        if (form.begin()) {
            form.row("Name", [&] {
                ui::SetFieldWide();
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                    r.setName(nameBuf);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(max 12 chars)");
            });
            form.row("Gold", [&] {
                ui::SetFieldMed();
                int gold = r.gold;
                if (ImGui::InputInt("##gold", &gold)) {
                    if (gold < 0) gold = 0;
                    if (gold > 0xFFFF) gold = 0xFFFF;
                    r.gold = static_cast<uint16_t>(gold);
                    dirty = true;
                }
            });
            form.row("Damage", [&] {
                ui::SetFieldMed();
                int dmg = r.damage;
                if (ImGui::InputInt("##damage", &dmg)) {
                    r.damage = static_cast<uint8_t>(dmg & 0xFF);
                    dirty = true;
                }
            });
        }
    }

    // Byte 0x0D is a restriction mask (set bit = class CANNOT use). Present it
    // as intuitive "usable by" checkboxes: checked = the class CAN use the item
    // = its bit is clear in the underlying mask.
    ImGui::SeparatorText("Usable by class");
    ui::CheckboxGrid("item_classes", 4, 8, [&](int c) {
        bool canUse = (r.forbiddenClassMask & kClasses[c].bit) == 0;
        if (ImGui::Checkbox(kClasses[c].name, &canUse)) {
            if (canUse) r.forbiddenClassMask &= static_cast<uint8_t>(~kClasses[c].bit);
            else r.forbiddenClassMask |= kClasses[c].bit;
            dirty = true;
        }
    });

    ImGui::SeparatorText("Bonus (equipped, while worn)");
    {
        ui::FormGrid grid("item_bonus");
        if (grid.begin()) {
            grid.row2(
                "Bonus type",
                [&] {
                    ui::SetFieldStretch();
                    int bType = r.bonusType & 0x0F;
                    if (ImGui::Combo("##btype", &bType, kItemBonusTypeNames,
                                     kItemBonusTypeCount)) {
                        r.bonusType = static_cast<uint8_t>(bType & 0x0F);
                        dirty = true;
                    }
                },
                "Bonus amt",
                [&] {
                    ui::SetFieldStretch();
                    int bAmt = r.bonusAmount;
                    if (ImGui::InputInt("##bamt", &bAmt)) {
                        if (bAmt < 0) bAmt = 0;
                        if (bAmt > 0x0F) bAmt = 0x0F;
                        r.bonusAmount = static_cast<uint8_t>(bAmt & 0x0F);
                        dirty = true;
                    }
                });
        }
        ImGui::TextDisabled("(amount 0 = no bonus)");
    }

    // The effect byte is a flat spell index, not a (type,amount) pair: 0x80+ =
    // Sorcerer spell, 0xB0+ = Cleric spell, <0x80 = stat boost. Edit the raw
    // byte and show the decoded spell/effect.
    ImGui::SeparatorText("Effect (use power / charges)");
    {
        uint8_t eb = r.effectByte();
        int val = eb;
        ui::FormTable form("item_effect");
        if (form.begin()) {
            form.row("Effect byte", [&] {
                ui::SetFieldMed();
                if (ImGui::InputInt("##effbyte", &val)) {
                    if (val < 0) val = 0;
                    if (val > 0xFF) val = 0xFF;
                    r.effectType = static_cast<uint8_t>((val >> 4) & 0x0F);
                    r.effectAmount = static_cast<uint8_t>(val & 0x0F);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("0x%02X", static_cast<unsigned>(val & 0xFF));
            });
            form.row("Decoded", [&] {
                ImGui::TextUnformatted(describeItemEffect(r.effectByte()).c_str());
            });
        }
    }

    ImGui::SeparatorText("Raw record");
    ImGui::Text("Record %d @ file offset 0x%04X (%d bytes)", selected_, selected_ * kItemRecordSize,
                kItemRecordSize);
    Bytes rec = file_.encode();
    DrawHexView("item_hex", rec.data() + selected_ * kItemRecordSize, kItemRecordSize,
                selected_ * kItemRecordSize);

    ui::EndDetailPanel();
}

}  // namespace mm2
