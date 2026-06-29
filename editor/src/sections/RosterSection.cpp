#include "sections/RosterSection.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <array>

#include "app/App.h"
#include "core/RosterGlobalTail.h"
#include "core/Spells.h"
#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/UiLayout.h"

namespace mm2 {

namespace {

const char* kSexNames[] = {"Male", "Female"};
const char* kAlignNames[] = {"Good", "Neutral", "Evil"};
const char* kRaceNames[] = {"Human", "Elf", "Dwarf", "Gnome", "Half-Orc"};
const char* kClassNames[] = {"Knight", "Paladin", "Archer", "Cleric",
                             "Sorcerer", "Robber", "Ninja", "Barbarian"};
const char* kTownNames[] = {"(none)", "Middlegate", "Atlantium", "Tundara",
                            "Vulcania", "Sandsobar"};

const char* labelFor(const char* const* names, int count, uint8_t value) {
    return (value < count) ? names[value] : "?";
}

SpellSchool spellSchoolForClass(uint8_t classId) {
    // Paladin/Cleric use the cleric list; everyone else maps to sorcerer list.
    if (classId == 1 || classId == 3) return SpellSchool::Cleric;
    return SpellSchool::Sorcerer;
}

bool editByteField(const char* id, uint8_t* value) {
    int v = *value;
    if (!ImGui::InputInt(id, &v, 0, 0)) return false;
    if (v < 0) v = 0;
    if (v > 0xFF) v = 0xFF;
    *value = static_cast<uint8_t>(v);
    return true;
}

bool editWordField(const char* id, uint16_t* value) {
    int v = *value;
    if (!ImGui::InputInt(id, &v, 0, 0)) return false;
    if (v < 0) v = 0;
    if (v > 0xFFFF) v = 0xFFFF;
    *value = static_cast<uint16_t>(v);
    return true;
}

bool editLongField(const char* id, uint32_t* value) {
    int v = static_cast<int>(*value);
    if (!ImGui::InputInt(id, &v, 0, 0)) return false;
    if (v < 0) v = 0;
    *value = static_cast<uint32_t>(v);
    return true;
}

}  // namespace

bool RosterSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    if (selected_ < 0 || selected_ >= kRosterCharacterCount) selected_ = 0;
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
                    if (ImGui::InputInt(("##" + std::to_string(f0.off)).c_str(), &v, 0, 0)) {
                        r.setU8(f0.off, static_cast<uint8_t>(v & 0xFF));
                        dirty = true;
                    }
                },
                f1.label,
                [&] {
                    ui::SetFieldStretch();
                    int v = r.u8(f1.off);
                    if (ImGui::InputInt(("##" + std::to_string(f1.off)).c_str(), &v, 0, 0)) {
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
                    if (ImGui::InputInt(("##" + std::to_string(f.off)).c_str(), &v, 0, 0)) {
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
                if (ImGui::InputInt("##gold", &gold, 0, 0)) {
                    if (gold < 0) gold = 0;
                    r.setU32(kGold, static_cast<uint32_t>(gold));
                    dirty = true;
                }
            });
            form.row("Experience", [&] {
                ui::SetFieldMed();
                int exp = static_cast<int>(r.u32(kExperience));
                if (ImGui::InputInt("##exp", &exp, 0, 0)) {
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
                        if (ImGui::InputInt(("##" + std::to_string(f0.off)).c_str(), &v, 0, 0)) {
                            r.setU8(f0.off, static_cast<uint8_t>(v & 0xFF));
                            dirty = true;
                        }
                    },
                    f1.label,
                    [&] {
                        ui::SetFieldStretch();
                        int v = r.u8(f1.off);
                        if (ImGui::InputInt(("##" + std::to_string(f1.off)).c_str(), &v, 0, 0)) {
                            r.setU8(f1.off, static_cast<uint8_t>(v & 0xFF));
                            dirty = true;
                        }
                    });
            }
        }
    }
}

void RosterSection::drawCharacterSheet(RosterRecord& r) {
    using namespace roster_off;

    char nameBuf[kRosterNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kRosterNameSize] = '\0';

    uint8_t townFlags = r.u8(kTownFlags);
    bool inParty = (townFlags & 0x80) != 0;
    int townId = townFlags & 0x7F;
    const bool classQuestPlus = (r.u8(kClassQuestGuildMask) & 0x80) != 0;

    ImGui::Text("A>");
    ImGui::SameLine();
    ui::SetFieldWide();
    if (ImGui::InputText("##char_name", nameBuf, sizeof(nameBuf))) {
        r.setName(nameBuf);
        dirty = true;
    }
    ImGui::SameLine();
    ImGui::Text(": %s %s %s %s%s",
                labelFor(kSexNames, 2, r.u8(kSex)),
                labelFor(kAlignNames, 3, r.u8(kAlignBase)),
                labelFor(kRaceNames, 5, r.u8(kRace)),
                labelFor(kClassNames, 8, r.u8(kClass)),
                classQuestPlus ? "+" : "");

    ImGui::Separator();
    if (ImGui::BeginTable("roster_sheet", 3,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        uint8_t might = r.u8(kMight);
        uint8_t intellect = r.u8(kIntellect);
        uint8_t personality = r.u8(kPersonality);
        uint8_t endurance = r.u8(kEnduranceCur);
        uint8_t speed = r.u8(kSpeed);
        uint8_t accuracy = r.u8(kAccuracy);
        uint8_t luck = r.u8(kLuck);

        ui::FormTable left("roster_sheet_left");
        if (left.begin()) {
            left.row("Mgt", [&] {
                ui::SetFieldShort();
                if (editByteField("##mgt", &might)) {
                    r.setU8(kMight, might);
                    dirty = true;
                }
            });
            left.row("Int", [&] {
                ui::SetFieldShort();
                if (editByteField("##int", &intellect)) {
                    r.setU8(kIntellect, intellect);
                    dirty = true;
                }
            });
            left.row("Per", [&] {
                ui::SetFieldShort();
                if (editByteField("##per", &personality)) {
                    r.setU8(kPersonality, personality);
                    dirty = true;
                }
            });
            left.row("End", [&] {
                ui::SetFieldShort();
                if (editByteField("##end", &endurance)) {
                    r.setU8(kEnduranceCur, endurance);
                    dirty = true;
                }
            });
            left.row("Spd", [&] {
                ui::SetFieldShort();
                if (editByteField("##spd", &speed)) {
                    r.setU8(kSpeed, speed);
                    dirty = true;
                }
            });
            left.row("Acy", [&] {
                ui::SetFieldShort();
                if (editByteField("##acy", &accuracy)) {
                    r.setU8(kAccuracy, accuracy);
                    dirty = true;
                }
            });
            left.row("Lck", [&] {
                ui::SetFieldShort();
                if (editByteField("##lck", &luck)) {
                    r.setU8(kLuck, luck);
                    dirty = true;
                }
            });
        }

        ImGui::TableNextColumn();
        uint16_t hpCur = r.u16(kHpCur);
        uint16_t hpMax = r.u16(kHpMax);
        uint16_t spCur = r.u16(kSpCur);
        uint16_t spMax = r.u16(kSpMax);
        uint8_t ac = r.u8(kArmorClass);
        uint8_t thievery = r.u8(kThievery);
        uint8_t spellLevel = r.u8(kSpellLevel);

        ui::FormTable mid("roster_sheet_mid");
        if (mid.begin()) {
            mid.row("HP", [&] {
                ui::SetFieldShort();
                if (editWordField("##hp_cur", &hpCur)) {
                    r.setU16(kHpCur, hpCur);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::Text("/");
                ImGui::SameLine();
                ui::SetFieldShort();
                if (editWordField("##hp_max", &hpMax)) {
                    r.setU16(kHpMax, hpMax);
                    dirty = true;
                }
            });
            mid.row("SP", [&] {
                ui::SetFieldShort();
                if (editWordField("##sp_cur", &spCur)) {
                    r.setU16(kSpCur, spCur);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::Text("/");
                ImGui::SameLine();
                ui::SetFieldShort();
                if (editWordField("##sp_max", &spMax)) {
                    r.setU16(kSpMax, spMax);
                    dirty = true;
                }
            });
            mid.row("AC", [&] {
                ui::SetFieldShort();
                if (editByteField("##ac", &ac)) {
                    r.setU8(kArmorClass, ac);
                    dirty = true;
                }
            });
            mid.row("Thievery %%", [&] {
                ui::SetFieldShort();
                if (editByteField("##thievery", &thievery)) {
                    r.setU8(kThievery, thievery);
                    dirty = true;
                }
            });
            mid.row("SL", [&] {
                ui::SetFieldShort();
                if (editByteField("##spell_level", &spellLevel)) {
                    r.setU8(kSpellLevel, spellLevel);
                    dirty = true;
                }
            });
        }

        ImGui::TableNextColumn();
        uint8_t level = r.u8(kLevel);
        uint8_t age = r.u8(kAge);
        uint32_t experience = r.u32(kExperience);
        uint32_t gold = r.u32(kGold);
        uint16_t gems = r.u16(kGems);
        uint8_t food = r.u8(kFood);
        uint8_t cond = r.u8(kCondition);

        ui::FormTable right("roster_sheet_right");
        if (right.begin()) {
            right.row("Lvl", [&] {
                ui::SetFieldShort();
                if (editByteField("##lvl", &level)) {
                    r.setU8(kLevel, level);
                    dirty = true;
                }
            });
            right.row("Age", [&] {
                ui::SetFieldShort();
                if (editByteField("##age", &age)) {
                    r.setU8(kAge, age);
                    dirty = true;
                }
            });
            right.row("Exp", [&] {
                ui::SetFieldMed();
                if (editLongField("##exp", &experience)) {
                    r.setU32(kExperience, experience);
                    dirty = true;
                }
            });
            right.row("Gold", [&] {
                ui::SetFieldMed();
                if (editLongField("##gold", &gold)) {
                    r.setU32(kGold, gold);
                    dirty = true;
                }
            });
            right.row("Gems", [&] {
                ui::SetFieldShort();
                if (editWordField("##gems", &gems)) {
                    r.setU16(kGems, gems);
                    dirty = true;
                }
            });
            right.row("Food", [&] {
                ui::SetFieldShort();
                if (editByteField("##food", &food)) {
                    r.setU8(kFood, food);
                    dirty = true;
                }
            });
            right.row("Condition", [&] {
                ui::SetFieldShort();
                if (editByteField("##cond", &cond)) {
                    r.setU8(kCondition, cond);
                    dirty = true;
                }
            });
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Identity and location flags");
    ui::FormGrid flagsGrid("roster_identity", ui::LabelWidth(), ui::FieldMed());
    if (flagsGrid.begin()) {
        flagsGrid.row2(
            "Sex", [&] {
                ui::SetFieldShort();
                uint8_t v = r.u8(kSex);
                if (editByteField("##sex", &v)) {
                    r.setU8(kSex, v);
                    dirty = true;
                }
            },
            "Race", [&] {
                ui::SetFieldShort();
                uint8_t v = r.u8(kRace);
                if (editByteField("##race", &v)) {
                    r.setU8(kRace, v);
                    dirty = true;
                }
            });
        flagsGrid.row2(
            "Class", [&] {
                ui::SetFieldShort();
                uint8_t v = r.u8(kClass);
                if (editByteField("##class", &v)) {
                    r.setU8(kClass, v);
                    dirty = true;
                }
            },
            "Align cur/base", [&] {
                ui::SetFieldShort();
                uint8_t cur = r.u8(kAlignCur);
                if (editByteField("##align_cur", &cur)) {
                    r.setU8(kAlignCur, cur);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::Text("/");
                ImGui::SameLine();
                ui::SetFieldShort();
                uint8_t base = r.u8(kAlignBase);
                if (editByteField("##align_base", &base)) {
                    r.setU8(kAlignBase, base);
                    dirty = true;
                }
            });
        flagsGrid.row2(
            "Town ID (low7)", [&] {
                ui::SetFieldShort();
                if (ImGui::InputInt("##town", &townId, 0, 0)) {
                    if (townId < 0) townId = 0;
                    if (townId > 0x7F) townId = 0x7F;
                    uint8_t next = static_cast<uint8_t>((inParty ? 0x80 : 0x00) | townId);
                    r.setU8(kTownFlags, next);
                    dirty = true;
                }
            },
            "In active party (bit7)", [&] {
                if (ImGui::Checkbox("##in_party", &inParty)) {
                    uint8_t next = static_cast<uint8_t>((inParty ? 0x80 : 0x00) | (townId & 0x7F));
                    r.setU8(kTownFlags, next);
                    dirty = true;
                }
            });
        flagsGrid.row2(
            "Class '+' display (bit7 @ +0x79)", [&] {
                bool hasPlus = (r.u8(kClassQuestGuildMask) & 0x80) != 0;
                if (ImGui::Checkbox("##class_plus", &hasPlus)) {
                    const uint8_t raw = r.u8(kClassQuestGuildMask);
                    const uint8_t next = static_cast<uint8_t>(hasPlus ? (raw | 0x80) : (raw & 0x7F));
                    r.setU8(kClassQuestGuildMask, next);
                    dirty = true;
                }
            },
            "Guild/quest mask (+0x79)", [&] {
                ui::SetFieldShort();
                uint8_t v = r.u8(kClassQuestGuildMask);
                if (editByteField("##class_quest_mask", &v)) {
                    r.setU8(kClassQuestGuildMask, v);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(class-quest/guild bits; 0x40 = seen Pegasus, OP_18 sel 0x74)");
            });
        flagsGrid.row2(
            "Script work flag (+0x78)", [&] {
                ui::SetFieldShort();
                uint8_t v = r.u8(kScriptWorkFlag);
                if (editByteField("##script_work", &v)) {
                    r.setU8(kScriptWorkFlag, v);
                    dirty = true;
                }
            },
            "Temp/score word (+0x76)", [&] {
                ui::SetFieldShort();
                uint16_t v = r.u16(kTempScoreWord);
                if (editWordField("##temp_score", &v)) {
                    r.setU16(kTempScoreWord, v);
                    dirty = true;
                }
            });
    }
    ImGui::TextDisabled("Town label: %s", labelFor(kTownNames, 6, static_cast<uint8_t>(townId)));
}

void RosterSection::drawEquipment(App& app, RosterRecord& r) {
    auto drawSlots = [&](const char* title, int base) {
        ImGui::SeparatorText(title);
        ImGui::TextDisabled("6 slots");
        if (!ImGui::BeginTable(title, 5,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                   ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
            return;
        }
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.f);
        ImGui::TableSetupColumn("Item ID", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Charges", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < kRosterItemSlots; ++i) {
            RosterItemSlot s = r.slot(base, i);
            ImGui::TableNextRow();
            ImGui::PushID(base * 16 + i);

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%d", i + 1);

            ImGui::TableNextColumn();
            int id = s.itemId;
            ui::SetFieldStretch();
            if (ImGui::InputInt("##id", &id, 0, 0)) {
                s.itemId = static_cast<uint8_t>(id & 0xFF);
                r.setSlot(base, i, s);
                dirty = true;
            }

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(app.itemName(s.itemId).c_str());

            ImGui::TableNextColumn();
            int charges = s.charges;
            ui::SetFieldStretch();
            if (ImGui::InputInt("##charges", &charges, 0, 0)) {
                s.charges = static_cast<uint8_t>(charges & 0xFF);
                r.setSlot(base, i, s);
                dirty = true;
            }

            ImGui::TableNextColumn();
            int flags = s.flags;
            ui::SetFieldStretch();
            if (ImGui::InputInt("##flags", &flags, 0, 0)) {
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
    using namespace roster_off;

    uint8_t cls = r.u8(kClass);
    SpellSchool school = spellSchoolForClass(cls);
    const char* schoolName = (school == SpellSchool::Cleric) ? "Cleric" : "Sorcerer";
    constexpr int kSpellCols = 7;

    ImGui::SeparatorText("Spell Book");
    ImGui::TextDisabled("Game-style %s spell grid.", schoolName);
    ImGui::TextDisabled(
        "NOTE: the +0x4C..0x57 \"spells\" bitset is UNVERIFIED (MM2 gates spells by "
        "spell-level, not per-spell flags). Byte +0x50 is NOT a spell field -- it is a "
        "packed alignment/profession-title nibble pair (event VM OP_32).");

    // roster.dat stores the 48-spell bitset in reverse byte order:
    // low-level spells live at +0x51, while high-level spells are at +0x4C.
    auto spellKnown = [&](int flat) -> bool {
        const int spellBit = flat - 1;  // 0..47
        const int byte = 5 - (spellBit / 8);
        const int bit = spellBit % 8;
        return ((r.raw[roster_off::kSpells + byte] >> bit) & 1) != 0;
    };
    auto setSpellKnown = [&](int flat, bool known) {
        const int spellBit = flat - 1;  // 0..47
        const int byte = 5 - (spellBit / 8);
        const int bit = spellBit % 8;
        if (known)
            r.raw[roster_off::kSpells + byte] |= static_cast<uint8_t>(1 << bit);
        else
            r.raw[roster_off::kSpells + byte] &= static_cast<uint8_t>(~(1 << bit));
    };

    ImGui::BeginChild("spell_book_box", ImVec2(ui::Em(26.0f), ui::Em(14.0f)), true);
    if (ImGui::BeginTable("spell_book_grid", 1 + kSpellCols,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Lvl", ImGuiTableColumnFlags_WidthFixed, ui::Em(3.0f));
        for (int col = 1; col <= kSpellCols; ++col) {
            char colLabel[8];
            snprintf(colLabel, sizeof(colLabel), "%d", col);
            ImGui::TableSetupColumn(colLabel, ImGuiTableColumnFlags_WidthFixed, ui::Em(2.6f));
        }
        ImGui::TableHeadersRow();

        int flat = 1;  // 1-based for spellName API
        for (int level = 1; level <= kSpellLevels; ++level) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", level);

            for (int slot = 1; slot <= kSpellCols; ++slot) {
                ImGui::TableNextColumn();
                if (slot > kSpellsPerLevel[level - 1]) {
                    ImGui::TextDisabled(" ");
                    continue;
                }

                bool known = spellKnown(flat);
                ImGui::PushID(flat);
                if (ImGui::Checkbox("##known", &known)) {
                    setSpellKnown(flat, known);
                    dirty = true;
                }
                ImGui::PopID();
                ++flat;
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Names");
    int flat = 1;  // 1-based for spellName API
    for (int level = 1; level <= kSpellLevels; ++level) {
        char levelTitle[32];
        snprintf(levelTitle, sizeof(levelTitle), "Level %d", level);
        ImGui::SeparatorText(levelTitle);
        char tableId[32];
        snprintf(tableId, sizeof(tableId), "roster_spells_names_l%d", level);
        if (ImGui::BeginTable(tableId, 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                           ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Known", ImGuiTableColumnFlags_WidthFixed, ui::Em(4.0f));
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ui::Em(3.0f));
            ImGui::TableSetupColumn("Spell", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int n = 0; n < kSpellsPerLevel[level - 1]; ++n, ++flat) {
                bool known = spellKnown(flat);
                const char* name = spellName(school, flat);

                ImGui::TableNextRow();
                ImGui::PushID(flat);

                ImGui::TableNextColumn();
                if (ImGui::Checkbox("##known", &known)) {
                    setSpellKnown(flat, known);
                    dirty = true;
                }

                ImGui::TableNextColumn();
                ImGui::Text("%d", n + 1);

                ImGui::TableNextColumn();
                ImGui::Text("L%d-%d  %s", level, n + 1, name ? name : "(unknown)");

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
}

void RosterSection::drawGlobalOverlay() {
    static int selectedGlobal = kRosterGlobalStart;
    if (selectedGlobal < kRosterGlobalStart || selectedGlobal >= kRosterCount) {
        selectedGlobal = kRosterGlobalStart;
    }

    ImGui::TextWrapped(
        "Slots %d..%d are runtime globals overlaid on the roster buffer after the last recruitable "
        "hireling (Mr. Wizard in slot 47).",
        kRosterGlobalStart, kRosterCount - 1);
    ImGui::TextDisabled(
        "ASM cross-check: roster_base = A4-$2A3E, so slot 48 starts at A4-$11DE (combat/global arrays).");
    ImGui::TextDisabled(
        "Capstone references include LEA -$11DE(A4) at 0x114A / 0x118A / 0x4C9C / 0x9ECE / 0xB7A4.");
    ImGui::Separator();

    // Rebuild the serialized post-roster tail (slots 48..63) so we can decode known globals.
    std::array<uint8_t, kRosterGlobalCount * kRosterRecordSize> saveTail{};
    for (int i = 0; i < kRosterGlobalCount; ++i) {
        const RosterRecord& rec = file_.records[kRosterGlobalStart + i];
        std::memcpy(saveTail.data() + (i * kRosterRecordSize), rec.raw.data(), kRosterRecordSize);
    }
    auto tailU16 = [&](int off) -> uint16_t {
        if (off < 0 || off + 1 >= static_cast<int>(saveTail.size())) return 0;
        return readU16LE(saveTail.data() + off);
    };
    auto tailU8 = [&](int off) -> uint8_t {
        if (off < 0 || off >= static_cast<int>(saveTail.size())) return 0;
        return saveTail[off];
    };

    namespace G = roster_global::tail_off;

    const uint16_t era = tailU16(G::kEra);
    uint16_t curDay = 0;
    uint16_t curYear = 0;
    if (era < 10) {
        curDay = tailU16(G::kDayByEra + static_cast<int>(era) * 2);
        curYear = tailU16(G::kYearByEra + static_cast<int>(era) * 2);
    }
    static const char* kDispositionNames[] = {
        "Inconspicuous", "Average", "Aggressive", "Thrill Seeker"};

    char dayRow[160];
    char yearRow[160];
    std::snprintf(dayRow, sizeof(dayRow), "%u %u %u %u %u %u %u %u %u %u",
                  tailU16(G::kDayByEra + 0), tailU16(G::kDayByEra + 2),
                  tailU16(G::kDayByEra + 4), tailU16(G::kDayByEra + 6),
                  tailU16(G::kDayByEra + 8), tailU16(G::kDayByEra + 10),
                  tailU16(G::kDayByEra + 12), tailU16(G::kDayByEra + 14),
                  tailU16(G::kDayByEra + 16), tailU16(G::kDayByEra + 18));
    std::snprintf(yearRow, sizeof(yearRow), "%u %u %u %u %u %u %u %u %u %u",
                  tailU16(G::kYearByEra + 0), tailU16(G::kYearByEra + 2),
                  tailU16(G::kYearByEra + 4), tailU16(G::kYearByEra + 6),
                  tailU16(G::kYearByEra + 8), tailU16(G::kYearByEra + 10),
                  tailU16(G::kYearByEra + 12), tailU16(G::kYearByEra + 14),
                  tailU16(G::kYearByEra + 16), tailU16(G::kYearByEra + 18));

    ImGui::SeparatorText("Known decoded globals");
    ui::FormTable known("roster_globals_known");
    if (known.begin()) {
        known.row("Era index", [&] { ImGui::Text("%u", era); });
        known.row("Current day/year", [&] {
            if (era < 10) ImGui::Text("%u / %u", curDay, curYear);
            else ImGui::Text("unknown (era out of range)");
        });
        known.row("day[0..9]", [&] { ImGui::TextUnformatted(dayRow); });
        known.row("year[0..9]", [&] { ImGui::TextUnformatted(yearRow); });
        known.row("Party size", [&] { ImGui::Text("%u", tailU16(G::kPartySize)); });
        known.row("Subday ticks", [&] { ImGui::Text("%u", tailU16(G::kSubday)); });
        known.row("g=0x84 era low (A4-$79B5)", [&] {
            ImGui::Text("0x%02X (%u)", static_cast<uint8_t>(era & 0xFF), static_cast<uint8_t>(era & 0xFF));
        });
        known.row("Light factor (A4-$79AB)", [&] { ImGui::Text("%u", tailU8(G::kLightFactor)); });
        known.row("Endgame score A (A4-$79AA)", [&] { ImGui::Text("%u", tailU8(G::kEndgameScoreA)); });
        known.row("Endgame score B (A4-$79A9)", [&] { ImGui::Text("%u", tailU8(G::kEndgameScoreB)); });
        known.row("Achievement A / B (A4-$79A7/$79A6)", [&] {
            ImGui::Text("%02X %02X", tailU8(G::kAchievementA), tailU8(G::kAchievementB));
        });
        known.row("Encounter mod (A4-$79A5)", [&] { ImGui::Text("0x%02X", tailU8(G::kEncounterMod)); });
        known.row("Sounds (A4-$79B0 bit0)", [&] {
            ImGui::TextUnformatted((tailU8(G::kSounds) & 1) ? "ON" : "OFF");
        });
        known.row("Walk beep (A4-$79AF bit0)", [&] {
            ImGui::TextUnformatted((tailU8(G::kWalkBeep) & 1) ? "ON" : "OFF");
        });
        known.row("Disposition (A4-$79AE)", [&] {
            uint8_t d = tailU8(G::kDisposition);
            const char* nm = (d <= 3) ? kDispositionNames[d] : "Unknown";
            ImGui::Text("%u (%s)", d, nm);
        });
        known.row("Delay (A4-$79AD)", [&] { ImGui::Text("%u", tailU8(G::kDelay)); });
        known.row("new_game_flag (A4-$79B2)", [&] { ImGui::Text("%u", tailU8(G::kNewGameFlag)); });
        known.row("Elemental talismans A4-$79A4..-$79A1", [&] {
            ImGui::Text("%02X %02X %02X %02X", tailU8(G::kTalismans + 0), tailU8(G::kTalismans + 1),
                        tailU8(G::kTalismans + 2), tailU8(G::kTalismans + 3));
        });
        known.row("g=0x27 A4-$79A4 (Acwalandar)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kTalismans + 0));
        });
        known.row("g=0x28 A4-$79A3 (Shalwend)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kTalismans + 1));
        });
        known.row("g=0x29 A4-$79A2 (Pyrannaste)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kTalismans + 2));
        });
        known.row("g=0x2A A4-$79A1 (Gralkor)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kTalismans + 3));
        });
        known.row("g=0x3B A4-$798F (Castle Xabran gate)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kCastleXabran));
        });
        known.row("g=0x3C A4-$798E (Dawn's Mist / resort gate)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kDawnsMistGate));
        });
        known.row("g=0x3D A4-$798D (calendar period flag B)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kPeriodFlagB));
        });
        known.row("g=0x3E A4-$798C (calendar period flag A)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kPeriodFlagA));
        });
        known.row("g=0x33 A4-$7990 (Tundara cavern lever)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kTundaraLever));
        });
        known.row("g=0x32 A4-$7996 (Guardian cave gate)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kGuardianGate));
        });
        known.row("g=0x13 A4-$799E (temple donation bits)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kTempleDonation));
        });
        known.row("g=0x23 special / 0x2B Eagle Eye / 0x2C Wizard Eye step timer", [&] {
            ImGui::Text("0x23@-$79A8=%02X  0x2B@-$79A0=%02X  0x2C@-$799F=%02X",
                        tailU8(G::kSpecialQuest), tailU8(G::kEagleEyeStepTimer),
                        tailU8(G::kWizardEyeStepTimer));
        });
        known.row("g=0x80..0x83 gate bank A4-$7995..-$7992", [&] {
            ImGui::Text("%02X %02X %02X %02X", tailU8(G::kGateG80), tailU8(G::kGateG81),
                        tailU8(G::kGateG82), tailU8(G::kGateG83));
        });
        known.row("g=0x00..0x17 event bank A4-$798B", [&] {
            ImGui::Text("%02X %02X %02X %02X %02X %02X %02X %02X ...",
                        tailU8(G::kEventBank + 0), tailU8(G::kEventBank + 1),
                        tailU8(G::kEventBank + 2), tailU8(G::kEventBank + 3),
                        tailU8(G::kEventBank + 4), tailU8(G::kEventBank + 5),
                        tailU8(G::kEventBank + 6), tailU8(G::kEventBank + 7));
        });
        known.row("input_state A4-$799D..-$7999", [&] {
            ImGui::Text("%02X %02X %02X %02X %02X", tailU8(G::kInputState0 + 0),
                        tailU8(G::kInputState0 + 1), tailU8(G::kInputState0 + 2),
                        tailU8(G::kInputState0 + 3), tailU8(G::kInputState4));
        });
        known.row("move counter (A4-$796C)", [&] { ImGui::Text("%u", tailU8(G::kMoveCounter)); });
        known.row("encounter mode (A4-$796B)", [&] {
            ImGui::Text("0x%02X", tailU8(G::kEncounterMode));
        });
    }
    ImGui::TextDisabled(
        "Tail layout from save_game_state @ 0x823C (reload timer path @ 0x86F6). "
        "Event g-vars resolved @ 0x15620 (OP_17/OP_1A/OP_32).");
    ImGui::Separator();

    if (ImGui::BeginTable("roster_global_blocks", 5,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                              ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit,
                          ImVec2(0, ui::Em(13.0f)))) {
        ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, ui::Em(4.0f));
        ImGui::TableSetupColumn("A4 range", ImGuiTableColumnFlags_WidthFixed, ui::Em(12.0f));
        ImGui::TableSetupColumn("Non-zero", ImGuiTableColumnFlags_WidthFixed, ui::Em(6.0f));
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Bytes (first 8)", ImGuiTableColumnFlags_WidthFixed, ui::Em(14.0f));
        ImGui::TableHeadersRow();

        for (int i = kRosterGlobalStart; i < kRosterCount; ++i) {
            const RosterRecord& rec = file_.records[i];
            int nonZero = 0;
            for (uint8_t b : rec.raw) {
                if (b != 0) ++nonZero;
            }
            int start = -0x2A3E + (i * kRosterRecordSize);
            int end = start + kRosterRecordSize - 1;
            bool sel = (selectedGlobal == i);

            char range[32];
            snprintf(range, sizeof(range), "-$%04X..-$%04X", -start, -end);

            char sample[24];
            snprintf(sample, sizeof(sample), "%02X %02X %02X %02X %02X %02X %02X %02X",
                     rec.raw[0], rec.raw[1], rec.raw[2], rec.raw[3],
                     rec.raw[4], rec.raw[5], rec.raw[6], rec.raw[7]);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(i).c_str(), sel, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedGlobal = i;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(range);
            ImGui::TableNextColumn();
            ImGui::Text("%d", nonZero);
            ImGui::TableNextColumn();
            if (i == kRosterGlobalStart) {
                ImGui::TextUnformatted("Combat/global arrays start");
            } else if (i == kRosterCount - 1) {
                ImGui::TextUnformatted("Global flags near A4-$009C");
            } else {
                ImGui::TextUnformatted("Global workspace block");
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(sample);
        }
        ImGui::EndTable();
    }

    RosterRecord& g = file_.records[selectedGlobal];
    ImGui::SeparatorText("Selected global block (hex)");
    ImGui::Text("Slot %d @ file 0x%04X, A4 offset start -$%04X",
                selectedGlobal,
                selectedGlobal * kRosterRecordSize,
                0x2A3E - (selectedGlobal * kRosterRecordSize));
    DrawHexView("roster_globals_hex", g.raw.data(), kRosterRecordSize,
                selectedGlobal * kRosterRecordSize);
}

void RosterSection::draw(App& app) {
    if (!loaded) {
        ImGui::TextDisabled("roster.dat not loaded.");
        return;
    }
    if (selected_ < 0 || selected_ >= kRosterCharacterCount) selected_ = 0;

    ui::BeginListPanel("roster_list");
    for (int i = 0; i < kRosterCharacterCount; ++i) {
        char label[64];
        std::string nm = file_.records[i].nameStr();
        snprintf(label, sizeof(label), "%2d  %s", i, nm.empty() ? "(empty)" : nm.c_str());
        if (ImGui::Selectable(label, selected_ == i)) selected_ = i;
    }
    ui::ListPanelNextDetail("roster_detail");

    RosterRecord& r = file_.records[selected_];

    if (ImGui::BeginTabBar("roster_tabs")) {
        if (ImGui::BeginTabItem("Character")) {
            drawCharacterSheet(r);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Raw Stats")) {
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
        if (ImGui::BeginTabItem("Globals 48..63")) {
            drawGlobalOverlay();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ui::EndDetailPanel();
}

}  // namespace mm2
