#include "sections/ItemsSection.h"

#include <cstdio>
#include <cstring>
#include <functional>
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

const char* kBoostKindNames[] = {"Max HP", "Might", "Speed", "Accuracy",
                                 "(unused)", "Level", "Spell Level"};
constexpr int kBoostKindCount = 7;

void setEffectByte(ItemRecord& r, uint8_t b) {
    r.effectType = static_cast<uint8_t>((b >> 4) & 0x0F);
    r.effectAmount = static_cast<uint8_t>(b & 0x0F);
}

int effectMode(uint8_t b) {
    if (b == 0) return 0;
    if (b < 0x80) return 1;
    if (b <= 0xB0) return 2;
    return 3;
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

std::string bonusSummary(const ItemRecord& r) {
    if (r.bonusAmount == 0) return "none";
    const int t = r.bonusType & 0x0F;
    const char* nm = (t >= 0 && t < kItemBonusTypeCount) ? kItemBonusTypeNames[t] : "?";
    char buf[48];
    snprintf(buf, sizeof(buf), "+%u %s", r.bonusAmount, nm);
    return buf;
}

std::string usableClassesSummary(const ItemRecord& r) {
    int n = 0;
    std::string out;
    for (const auto& c : kClasses) {
        if ((r.forbiddenClassMask & c.bit) == 0) {
            if (n++) out += ", ";
            out += c.name;
        }
    }
    if (n == 0) return "nobody";
    if (n == 8) return "all classes";
    return out;
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

void ItemsSection::drawWorkspace(App& app, EditorSelection& sel) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("items.dat not loaded", "Open a folder containing items.dat");
        return;
    }
    if (sel.doc == DocKind::Items && sel.kind == EditorSelection::Kind::Item && sel.index >= 0 &&
        sel.index < kItemsCount)
        selected_ = sel.index;
    if (selected_ < 0 || selected_ >= kItemsCount) selected_ = 0;

    if (!ui::BeginMasterList(layout_, "item_list", "Items")) return;
    for (int i = 0; i < kItemsCount; ++i) {
        std::string nm = file_.records[i].nameStr();
        char hay[48];
        snprintf(hay, sizeof(hay), "%d %s", i, nm.c_str());
        if (!ui::ListFilterPass(layout_, hay)) continue;
        if (ui::ListRow(i, nm.c_str(), selected_ == i)) {
            selected_ = i;
            sel.Select(DocKind::Items, EditorSelection::Kind::Item, i);
        }
    }
    ui::EndMasterListBeginDetail(layout_, "item_detail");
    drawItemDetail();
    ui::EndMasterDetail();
}

void ItemsSection::drawItemDetail() {
    ItemRecord& r = file_.records[selected_];

    char nameBuf[kItemNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kItemNameSize] = '\0';

    char sub[96];
    snprintf(sub, sizeof(sub), "#%d · %u gp · dmg %u", selected_, r.gold, r.damage);
    ui::PanelHeader(nm.empty() ? "(blank item)" : nm.c_str(), sub);

    // —— Identity ——
    ui::SectionBlock("Identity");
    {
        ui::FormGrid grid("item_id", ui::Em(5.f));
        if (grid.begin()) {
            grid.row2(
                "Name",
                [&] {
                    ui::SetFieldStretch();
                    if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                        r.setName(nameBuf);
                        dirty = true;
                    }
                },
                "Gold",
                [&] {
                    ui::SetFieldStretch();
                    int gold = r.gold;
                    if (ImGui::InputInt("##gold", &gold, 1, 10)) {
                        if (gold < 0) gold = 0;
                        if (gold > 0xFFFF) gold = 0xFFFF;
                        r.gold = static_cast<uint16_t>(gold);
                        dirty = true;
                    }
                });
            grid.row1("Damage", [&] {
                ui::SetFieldShort();
                int dmg = r.damage;
                if (ImGui::InputInt("##damage", &dmg, 1, 5)) {
                    if (dmg < 0) dmg = 0;
                    if (dmg > 0xFF) dmg = 0xFF;
                    r.damage = static_cast<uint8_t>(dmg);
                    dirty = true;
                }
            });
        }
    }

    // —— Class restriction ——
    ui::SectionBlock("Usable by");
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
        ImGui::SameLine(0, ui::Em(1.f));
        ImGui::TextDisabled("%s", usableClassesSummary(r).c_str());

        ui::CheckboxGrid("item_classes", 4, 8, [&](int c) {
            bool canUse = (r.forbiddenClassMask & kClasses[c].bit) == 0;
            if (ImGui::Checkbox(kClasses[c].name, &canUse)) {
                r.setUsableBy(static_cast<ItemClassBit>(kClasses[c].bit), canUse);
                dirty = true;
            }
        });
    }

    // —— Equipped bonus ——
    ui::SectionBlock("Equipped bonus");
    {
        ui::FormGrid grid("item_bonus", ui::Em(5.5f));
        if (grid.begin()) {
            grid.row2(
                "Type",
                [&] {
                    ui::SetFieldStretch();
                    int bType = r.bonusType & 0x0F;
                    if (ImGui::Combo("##btype", &bType, kItemBonusTypeNames, kItemBonusTypeCount)) {
                        r.bonusType = static_cast<uint8_t>(bType & 0x0F);
                        dirty = true;
                    }
                },
                "Amount",
                [&] {
                    ui::SetFieldShort();
                    int bAmt = r.bonusAmount;
                    if (ImGui::InputInt("##bamt", &bAmt, 1, 1)) {
                        if (bAmt < 0) bAmt = 0;
                        if (bAmt > 0x0F) bAmt = 0x0F;
                        r.bonusAmount = static_cast<uint8_t>(bAmt);
                        dirty = true;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", bonusSummary(r).c_str());
                });
        }
    }

    // —— Use power ——
    ui::SectionBlock("Use power");
    {
        uint8_t eb = r.effectByte();
        int mode = effectMode(eb);
        const char* modeNames = "None\0Stat boost\0Sorcerer spell\0Cleric spell\0";

        ui::FormTable form("item_effect", ui::Em(5.5f));
        if (form.begin()) {
            form.row("Kind", [&] {
                ui::SetFieldMed();
                if (ImGui::Combo("##eff_mode", &mode, modeNames)) {
                    switch (mode) {
                        case 0:
                            setEffectByte(r, 0);
                            break;
                        case 1:
                            setEffectByte(r, 0x11);
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
            });

            if (mode == 1) {
                int kind = (eb >> 4) & 0x0F;
                int amt = eb & 0x0F;
                if (kind >= kBoostKindCount) kind = 0;
                form.row("Boost", [&] {
                    ui::SetFieldMed();
                    if (ImGui::Combo("##boost_kind", &kind, kBoostKindNames, kBoostKindCount)) {
                        setEffectByte(r, static_cast<uint8_t>(((kind & 0x0F) << 4) | (amt & 0x0F)));
                        dirty = true;
                    }
                    ImGui::SameLine();
                    ui::SetFieldByte();
                    if (ImGui::InputInt("##boost_amt", &amt, 1, 1)) {
                        if (amt < 0) amt = 0;
                        if (amt > 0x0F) amt = 0x0F;
                        setEffectByte(r, static_cast<uint8_t>(((kind & 0x0F) << 4) | (amt & 0x0F)));
                        dirty = true;
                    }
                });
            } else if (mode == 2 || mode == 3) {
                SpellSchool school = (mode == 2) ? SpellSchool::Sorcerer : SpellSchool::Cleric;
                int flat = spellFlatFromEffect(eb);
                if (flat < 1) flat = 1;
                form.row("Spell", [&] {
                    ui::SetFieldStretch();
                    if (spellPicker("##spell", school, &flat)) {
                        setEffectByte(r, encodeSpellEffect(school, flat));
                        dirty = true;
                    }
                });
            }

            form.row("Result", [&] {
                ImGui::TextUnformatted(describeItemEffect(r.effectByte()).c_str());
            });
        }
    }

    if (ui::BeginHexBlock("Raw record")) {
        ImGui::TextDisabled("Record %d · %d bytes", selected_, kItemRecordSize);
        Bytes rec = file_.encode();
        DrawHexView("item_hex", rec.data() + selected_ * kItemRecordSize, kItemRecordSize,
                    selected_ * kItemRecordSize);
        ui::EndHexBlock();
    }
}

void ItemsSection::drawProperties(App& app, EditorSelection& sel) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("Not loaded", "items.dat missing from the data folder");
        return;
    }
    if (sel.doc == DocKind::Items && sel.kind == EditorSelection::Kind::Item && sel.index >= 0 &&
        sel.index < kItemsCount)
        selected_ = sel.index;
    if (selected_ < 0 || selected_ >= kItemsCount) selected_ = 0;
    if (sel.kind == EditorSelection::Kind::None || sel.doc != DocKind::Items)
        sel.Select(DocKind::Items, EditorSelection::Kind::Item, selected_);

    const ItemRecord& r = file_.records[selected_];
    std::string nmStr = r.nameStr();
    const char* nm = nmStr.empty() ? "(blank)" : nmStr.c_str();
    char sub[64];
    snprintf(sub, sizeof(sub), "#%d", selected_);
    ui::PanelHeader(nm, sub);

    ui::SectionBlock("Summary");
    {
        ui::FormTable form("item_summary", ui::Em(6.f));
        if (form.begin()) {
            form.row("Gold", [&] { ImGui::Text("%u gp", r.gold); });
            form.row("Damage", [&] { ImGui::Text("%u", r.damage); });
            form.row("Bonus", [&] { ImGui::TextUnformatted(bonusSummary(r).c_str()); });
            form.row("Use power",
                     [&] { ImGui::TextUnformatted(describeItemEffect(r.effectByte()).c_str()); });
            form.row("Usable by", [&] {
                ImGui::PushTextWrapPos(0.f);
                ImGui::TextUnformatted(usableClassesSummary(r).c_str());
                ImGui::PopTextWrapPos();
            });
        }
    }
}

}  // namespace mm2
