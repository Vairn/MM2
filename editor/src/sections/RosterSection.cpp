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
#include "widgets/UiTheme.h"

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

bool editEnumCombo(const char* id, uint8_t* value, const char* const* names, int count) {
    int v = *value;
    if (v < 0 || v >= count) {
        // Out-of-range: fall back to integer so the byte is still editable.
        if (editByteField(id, value)) return true;
        ImGui::SameLine();
        ImGui::TextDisabled("?");
        return false;
    }
    if (!ImGui::Combo(id, &v, names, count)) return false;
    *value = static_cast<uint8_t>(v);
    return true;
}

// Roster +$26 condition — ASM-confirmed bitfield + fatal values.
// Living bits (OR up to 0x7F): bset #0/#2/#3/#4 @ 0xEEA6/0x1CEF8/0x1CC66/0x19DD4,
//   btst #1 silence @ 0x118F0, or #$20 paralyz @ 0x1EE94, #$40 uncon @ 0xC3FC.
// Fatal whole-byte: #$81 dead @ 0x1EEC8, #$82 stone @ 0x1EEDA, #$FF erad @ 0x1EEEC.
constexpr uint8_t kCondCursed = 0x01;
constexpr uint8_t kCondSilenced = 0x02;
constexpr uint8_t kCondDiseased = 0x04;
constexpr uint8_t kCondPoisoned = 0x08;
constexpr uint8_t kCondAsleep = 0x10;
constexpr uint8_t kCondParalyzed = 0x20;
constexpr uint8_t kCondUnconscious = 0x40;
constexpr uint8_t kCondDead = 0x81;
constexpr uint8_t kCondStoned = 0x82;
constexpr uint8_t kCondEradicated = 0xFF;

const char* conditionLabel(uint8_t c) {
    if (c == kCondDead) return "Dead";
    if (c == kCondStoned) return "Stoned";
    if (c >= 0x80) return "Eradicated";  // FAQ: any other ≥0x80
    if (c == 0) return "Good";
    return "Afflicted";
}

int conditionMajorIndex(uint8_t c) {
    if (c == kCondDead) return 1;
    if (c == kCondStoned) return 2;
    if (c >= 0x80) return 3;  // eradicated (0xFF and other ≥0x80)
    return 0;                 // living / good
}

bool editConditionMajor(const char* id, uint8_t* value) {
    int sel = conditionMajorIndex(*value);
    const char* names = "Living\0Dead\0Stoned\0Eradicated\0";
    if (!ImGui::Combo(id, &sel, names)) return false;
    switch (sel) {
        case 1: *value = kCondDead; break;
        case 2: *value = kCondStoned; break;
        case 3: *value = kCondEradicated; break;
        default:
            if (*value >= 0x80) *value = 0;  // clear fatal → Good
            break;
    }
    return true;
}

bool editConditionBit(const char* id, uint8_t* value, uint8_t bit) {
    if (*value >= 0x80) return false;
    bool on = (*value & bit) != 0;
    if (!ImGui::Checkbox(id, &on)) return false;
    if (on)
        *value = static_cast<uint8_t>(*value | bit);
    else
        *value = static_cast<uint8_t>(*value & static_cast<uint8_t>(~bit));
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

    ui::SectionBlock("Pools & wealth");
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

    ui::SectionBlock("Base stats", "+0x6A..0x73");
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

    // Name + live summary
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("A>");
    ImGui::SameLine();
    ui::SetFieldWide();
    if (ImGui::InputText("##char_name", nameBuf, sizeof(nameBuf))) {
        r.setName(nameBuf);
        dirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(": %s %s %s %s%s", labelFor(kSexNames, 2, r.u8(kSex)),
                        labelFor(kAlignNames, 3, r.u8(kAlignBase)),
                        labelFor(kRaceNames, 5, r.u8(kRace)),
                        labelFor(kClassNames, 8, r.u8(kClass)), classQuestPlus ? "+" : "");

    // Identity — combos first (these were raw IDs)
    ui::SectionBlock("Identity");
    {
        ui::FormGrid id("roster_id", ui::Em(5.5f));
        if (id.begin()) {
            id.row2(
                "Sex",
                [&] {
                    ui::SetFieldStretch();
                    uint8_t v = r.u8(kSex);
                    if (editEnumCombo("##sex", &v, kSexNames, 2)) {
                        r.setU8(kSex, v);
                        dirty = true;
                    }
                },
                "Race",
                [&] {
                    ui::SetFieldStretch();
                    uint8_t v = r.u8(kRace);
                    if (editEnumCombo("##race", &v, kRaceNames, 5)) {
                        r.setU8(kRace, v);
                        dirty = true;
                    }
                });
            id.row2(
                "Class",
                [&] {
                    ui::SetFieldStretch();
                    uint8_t v = r.u8(kClass);
                    if (editEnumCombo("##class", &v, kClassNames, 8)) {
                        r.setU8(kClass, v);
                        dirty = true;
                    }
                },
                "Align cur",
                [&] {
                    ui::SetFieldStretch();
                    uint8_t cur = r.u8(kAlignCur);
                    if (editEnumCombo("##align_cur", &cur, kAlignNames, 3)) {
                        r.setU8(kAlignCur, cur);
                        dirty = true;
                    }
                });
            id.row2(
                "Align base",
                [&] {
                    ui::SetFieldStretch();
                    uint8_t base = r.u8(kAlignBase);
                    if (editEnumCombo("##align_base", &base, kAlignNames, 3)) {
                        r.setU8(kAlignBase, base);
                        dirty = true;
                    }
                },
                "Town",
                [&] {
                    ui::SetFieldStretch();
                    if (townId >= 0 && townId < 6) {
                        if (ImGui::Combo("##town", &townId, kTownNames, 6)) {
                            uint8_t next =
                                static_cast<uint8_t>((inParty ? 0x80 : 0x00) | (townId & 0x7F));
                            r.setU8(kTownFlags, next);
                            dirty = true;
                        }
                    } else {
                        if (ImGui::InputInt("##town_raw", &townId, 0, 0)) {
                            if (townId < 0) townId = 0;
                            if (townId > 0x7F) townId = 0x7F;
                            uint8_t next =
                                static_cast<uint8_t>((inParty ? 0x80 : 0x00) | (townId & 0x7F));
                            r.setU8(kTownFlags, next);
                            dirty = true;
                        }
                        ImGui::SameLine();
                        ImGui::TextDisabled("(id %d)", townId);
                    }
                });
            id.row2(
                "In party",
                [&] {
                    if (ImGui::Checkbox("##in_party", &inParty)) {
                        uint8_t next =
                            static_cast<uint8_t>((inParty ? 0x80 : 0x00) | (townId & 0x7F));
                        r.setU8(kTownFlags, next);
                        dirty = true;
                    }
                },
                "Class +",
                [&] {
                    bool hasPlus = (r.u8(kClassQuestGuildMask) & 0x80) != 0;
                    if (ImGui::Checkbox("##class_plus", &hasPlus)) {
                        const uint8_t raw = r.u8(kClassQuestGuildMask);
                        const uint8_t next =
                            static_cast<uint8_t>(hasPlus ? (raw | 0x80) : (raw & 0x7F));
                        r.setU8(kClassQuestGuildMask, next);
                        dirty = true;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(bit7 @ +0x79)");
                });
        }
    }

    // Stats — 3 equal columns; label + field aligned within each cell
    ui::SectionBlock("Stats");
    const float statsLabelW = ui::Em(3.2f);

    auto byteCell = [&](const char* id, int off) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        uint8_t v = r.u8(off);
        if (editByteField(id, &v)) {
            r.setU8(off, v);
            dirty = true;
        }
    };
    auto wordCell = [&](const char* id, int off) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        uint16_t v = r.u16(off);
        if (editWordField(id, &v)) {
            r.setU16(off, v);
            dirty = true;
        }
    };
    auto longCell = [&](const char* id, int off) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        uint32_t v = r.u32(off);
        if (editLongField(id, &v)) {
            r.setU32(off, v);
            dirty = true;
        }
    };
    auto pair = [&](const char* label, const std::function<void()>& field) {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(statsLabelW);
        field();
    };
    auto pairCurMax = [&](const char* label, const char* idCur, int offCur, const char* idMax,
                          int offMax) {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(statsLabelW);
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float slashW = ImGui::CalcTextSize("/").x;
        const float avail = ImGui::GetContentRegionAvail().x;
        const float half = (avail - slashW - gap * 2.f) * 0.5f;
        ImGui::SetNextItemWidth(half);
        uint16_t cur = r.u16(offCur);
        if (editWordField(idCur, &cur)) {
            r.setU16(offCur, cur);
            dirty = true;
        }
        ImGui::SameLine(0, gap);
        ImGui::TextUnformatted("/");
        ImGui::SameLine(0, gap);
        ImGui::SetNextItemWidth(half);
        uint16_t mx = r.u16(offMax);
        if (editWordField(idMax, &mx)) {
            r.setU16(offMax, mx);
            dirty = true;
        }
    };

    if (ImGui::BeginTable("roster_stats_grid", 3,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_PadOuterX |
                              ImGuiTableFlags_NoHostExtendX)) {
        ImGui::TableSetupColumn("c0", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupColumn("c1", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupColumn("c2", ImGuiTableColumnFlags_WidthStretch, 1.f);

        ImGui::TableNextRow();
        pair("Mgt", [&] { byteCell("##mgt", kMight); });
        pairCurMax("HP", "##hp_cur", kHpCur, "##hp_max", kHpMax);
        pair("Lvl", [&] { byteCell("##lvl", kLevel); });

        ImGui::TableNextRow();
        pair("Int", [&] { byteCell("##int", kIntellect); });
        pairCurMax("SP", "##sp_cur", kSpCur, "##sp_max", kSpMax);
        pair("Age", [&] { byteCell("##age", kAge); });

        ImGui::TableNextRow();
        pair("Per", [&] { byteCell("##per", kPersonality); });
        pair("AC", [&] { byteCell("##ac", kArmorClass); });
        pair("Exp", [&] { longCell("##exp", kExperience); });

        ImGui::TableNextRow();
        pair("End", [&] { byteCell("##end", kEnduranceCur); });
        pair("Thv", [&] { byteCell("##thievery", kThievery); });
        pair("Gold", [&] { longCell("##gold", kGold); });

        ImGui::TableNextRow();
        pair("Spd", [&] { byteCell("##spd", kSpeed); });
        pair("SL", [&] { byteCell("##spell_level", kSpellLevel); });
        pair("Gems", [&] { wordCell("##gems", kGems); });

        ImGui::TableNextRow();
        pair("Acy", [&] { byteCell("##acy", kAccuracy); });
        pair("Food", [&] { byteCell("##food", kFood); });
        pair("Cond", [&] {
            uint8_t cond = r.u8(kCondition);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (editConditionMajor("##cond_major", &cond)) {
                r.setU8(kCondition, cond);
                dirty = true;
            }
        });

        ImGui::TableNextRow();
        pair("Lck", [&] { byteCell("##lck", kLuck); });
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        ImGui::EndTable();
    }

    // Living afflictions are OR-able bits on +$26 (ASM); hidden when Dead/Stone/Erad.
    {
        uint8_t cond = r.u8(kCondition);
        if (cond < 0x80) {
            ImGui::Spacing();
            auto bit = [&](const char* label, uint8_t mask) {
                ImGui::PushID(mask);
                if (editConditionBit(label, &cond, mask)) {
                    r.setU8(kCondition, cond);
                    dirty = true;
                }
                ImGui::PopID();
            };
            if (ImGui::BeginTable("cond_bits", 4, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                bit("Cursed", kCondCursed);
                ImGui::TableNextColumn();
                bit("Silenced", kCondSilenced);
                ImGui::TableNextColumn();
                bit("Diseased", kCondDiseased);
                ImGui::TableNextColumn();
                bit("Poisoned", kCondPoisoned);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                bit("Asleep", kCondAsleep);
                ImGui::TableNextColumn();
                bit("Paralyzed", kCondParalyzed);
                ImGui::TableNextColumn();
                bit("Unconscious", kCondUnconscious);
                ImGui::TableNextColumn();
                ImGui::TextDisabled("0x%02X", cond);
                ImGui::EndTable();
            }
        } else {
            ImGui::TextDisabled("Fatal condition 0x%02X (%s)", cond, conditionLabel(cond));
        }
    }

    ui::SectionBlock("Flags", "class-quest / script scratch");
    {
        ui::FormGrid flags("roster_flags", ui::Em(7.f));
        if (flags.begin()) {
            flags.row2(
                "Seen Pegasus",
                [&] {
                    uint8_t raw = r.u8(kClassQuestGuildMask);
                    bool seen = (raw & 0x40) != 0;
                    if (ImGui::Checkbox("##pegasus", &seen)) {
                        r.setU8(kClassQuestGuildMask,
                                static_cast<uint8_t>(seen ? (raw | 0x40) : (raw & ~0x40)));
                        dirty = true;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(bit6 / OP_18 sel 0x74)");
                },
                "Guild mask",
                [&] {
                    ui::SetFieldShort();
                    uint8_t v = r.u8(kClassQuestGuildMask);
                    if (editByteField("##class_quest_mask", &v)) {
                        r.setU8(kClassQuestGuildMask, v);
                        dirty = true;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("0x%02X", v);
                });
            flags.row2(
                "Script work",
                [&] {
                    ui::SetFieldShort();
                    uint8_t v = r.u8(kScriptWorkFlag);
                    if (editByteField("##script_work", &v)) {
                        r.setU8(kScriptWorkFlag, v);
                        dirty = true;
                    }
                },
                "Temp/score",
                [&] {
                    ui::SetFieldShort();
                    uint16_t v = r.u16(kTempScoreWord);
                    if (editWordField("##temp_score", &v)) {
                        r.setU16(kTempScoreWord, v);
                        dirty = true;
                    }
                });
        }
    }
}

void RosterSection::drawEquipment(App& app, RosterRecord& r) {
    auto drawSlots = [&](const char* title, int base) {
        ui::SectionBlock(title);
        ImGui::TextDisabled("6 slots");
        if (!ImGui::BeginTable(title, 5,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                   ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
            return;
        }
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ui::Em(1.75f));
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

    ui::SectionBlock("Spell Book");
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

    ui::SectionBlock("Names");
    int flat = 1;  // 1-based for spellName API
    for (int level = 1; level <= kSpellLevels; ++level) {
        char levelTitle[32];
        snprintf(levelTitle, sizeof(levelTitle), "Level %d", level);
        ui::SectionBlock(levelTitle);
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

uint8_t RosterSection::hirelingUnlockByte(int letterIndex) const {
    if (letterIndex < 0 || letterIndex >= 24) return 0;
    using namespace roster_global::tail_off;
    const int off = kEventBank + letterIndex;
    const int slot = kRosterGlobalStart + off / kRosterRecordSize;
    const int byte = off % kRosterRecordSize;
    if (slot < 0 || slot >= kRosterCount) return 0;
    return file_.records[slot].raw[static_cast<size_t>(byte)];
}

void RosterSection::setHirelingUnlock(int letterIndex, bool found) {
    if (letterIndex < 0 || letterIndex >= 24) return;
    using namespace roster_global::tail_off;
    const int off = kEventBank + letterIndex;
    const int slot = kRosterGlobalStart + off / kRosterRecordSize;
    const int byte = off % kRosterRecordSize;
    if (slot < 0 || slot >= kRosterCount) return;
    file_.records[slot].raw[static_cast<size_t>(byte)] = found ? 1 : 0;
    dirty = true;
}

void RosterSection::drawHirelingUnlocks() {
    ui::PanelHeader("Hireling unlocks", "g=0x00..0x17  (found = hireable at inn)");
    ImGui::TextWrapped(
        "These 24 bytes live in the roster.dat global tail (A4-$798B). "
        "The party UI hides a hireling letter until its byte is nonzero.");
    ImGui::Spacing();

    if (ImGui::Button("Unlock all")) {
        for (int i = 0; i < 24; ++i) setHirelingUnlock(i, true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Lock all")) {
        for (int i = 0; i < 24; ++i) setHirelingUnlock(i, false);
    }
    ImGui::Spacing();

    if (!ImGui::BeginTable("hire_unlock_panel", 3,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Found", ImGuiTableColumnFlags_WidthFixed, ui::Em(5.f));
    ImGui::TableSetupColumn("Hireling", ImGuiTableColumnFlags_WidthFixed, ui::Em(4.f));
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (int i = 0; i < 24; ++i) {
        const int slot = 24 + i;
        std::string nm = file_.records[slot].nameStr();
        bool found = hirelingUnlockByte(i) != 0;
        ImGui::PushID(i);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Checkbox("##found", &found)) setHirelingUnlock(i, found);
        ImGui::TableNextColumn();
        char letter[4];
        std::snprintf(letter, sizeof(letter), "%c", 'A' + i);
        if (ImGui::Selectable(letter, selected_ == slot, ImGuiSelectableFlags_SpanAllColumns))
            selected_ = slot;
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(nm.empty() ? "(empty)" : nm.c_str());
        if (found) {
            ImGui::SameLine();
            ui::StatusChip("found", ui::Success());
        } else {
            ImGui::SameLine();
            ui::StatusChip("locked", ui::Muted());
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
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

    ui::SectionBlock("Known decoded globals");
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

    ui::SectionBlock("Hireling unlocks A–X", "g=0x00..0x17 / A4-$798B — nonzero = found");
    ImGui::TextDisabled("ASM party UI hides hireling letters when bank[letter] is zero (0x574).");
    auto writeTailU8 = [&](int off, uint8_t v) {
        const int slot = kRosterGlobalStart + off / kRosterRecordSize;
        const int byte = off % kRosterRecordSize;
        if (slot < kRosterCount && byte >= 0 && byte < kRosterRecordSize) {
            file_.records[slot].raw[static_cast<size_t>(byte)] = v;
            dirty = true;
        }
    };
    if (ImGui::BeginTable("hireling_unlocks", 8, ImGuiTableFlags_SizingStretchSame)) {
        for (int i = 0; i < 24; ++i) {
            if (i % 8 == 0) ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char label[8];
            std::snprintf(label, sizeof(label), "%c", 'A' + i);
            bool found = tailU8(G::kEventBank + i) != 0;
            ImGui::PushID(i);
            if (ImGui::Checkbox(label, &found)) {
                writeTailU8(G::kEventBank + i, found ? 1 : 0);
                saveTail[static_cast<size_t>(G::kEventBank + i)] = found ? 1 : 0;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
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
    ui::SectionBlock("Selected global block");
    ImGui::Text("Slot %d @ file 0x%04X, A4 offset start -$%04X",
                selectedGlobal,
                selectedGlobal * kRosterRecordSize,
                0x2A3E - (selectedGlobal * kRosterRecordSize));
    DrawHexView("roster_globals_hex", g.raw.data(), kRosterRecordSize,
                selectedGlobal * kRosterRecordSize);
}

void RosterSection::draw(App& app) {
    if (!loaded) {
        ui::EmptyState("roster.dat not loaded.");
        return;
    }
    if (selected_ < 0 || selected_ >= kRosterCharacterCount) selected_ = 0;

    if (!ui::BeginMasterList(layout_, "roster_list", "Roster")) {
        return;
    }

    ImGui::TextDisabled("Characters");
    for (int i = 0; i < 24; ++i) {
        std::string nm = file_.records[i].nameStr();
        char hay[96];
        snprintf(hay, sizeof(hay), "%d %s", i, nm.c_str());
        if (!ui::ListFilterPass(layout_, hay)) continue;
        char label[64];
        snprintf(label, sizeof(label), "%2d  %s", i, nm.empty() ? "(empty)" : nm.c_str());
        if (ImGui::Selectable(label, selected_ == i)) selected_ = i;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Hirelings  (checkbox = found)");
    for (int i = 0; i < 24; ++i) {
        const int slot = 24 + i;
        std::string nm = file_.records[slot].nameStr();
        char letter = static_cast<char>('A' + i);
        char hay[96];
        snprintf(hay, sizeof(hay), "%d %c %s", slot, letter, nm.c_str());
        if (!ui::ListFilterPass(layout_, hay)) continue;

        bool found = hirelingUnlockByte(i) != 0;
        ImGui::PushID(1000 + i);
        if (ImGui::Checkbox("##u", &found)) setHirelingUnlock(i, found);
        ImGui::SameLine();
        char label[64];
        snprintf(label, sizeof(label), "%c  %s", letter, nm.empty() ? "(empty)" : nm.c_str());
        if (!found) ImGui::PushStyleColor(ImGuiCol_Text, ui::Muted());
        if (ImGui::Selectable(label, selected_ == slot)) selected_ = slot;
        if (!found) ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ui::EndMasterListBeginDetail(layout_, "roster_detail");

    RosterRecord& r = file_.records[selected_];
    std::string nm = r.nameStr();
    char sub[64];
    if (selected_ >= 24) {
        const int letter = selected_ - 24;
        const bool found = hirelingUnlockByte(letter) != 0;
        snprintf(sub, sizeof(sub), "Hireling %c · slot %d · %s", 'A' + letter, selected_,
                 found ? "FOUND" : "locked");
    } else {
        snprintf(sub, sizeof(sub), "Character · slot %d", selected_);
    }
    ui::PanelHeader(nm.empty() ? "(empty)" : nm.c_str(), sub);

    if (selected_ >= 24) {
        const int letter = selected_ - 24;
        bool found = hirelingUnlockByte(letter) != 0;
        if (ImGui::Checkbox("Found / hireable at inn", &found)) setHirelingUnlock(letter, found);
        ImGui::SameLine();
        ImGui::TextDisabled("(writes g=0x%02X in global event bank)", letter);
        ImGui::Spacing();
    }

    if (ImGui::BeginTabBar("roster_tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem("Hirelings")) {
            drawHirelingUnlocks();
            ImGui::EndTabItem();
        }
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
            ImGui::TextDisabled("Record %d @ 0x%04X (%d bytes)", selected_,
                                selected_ * kRosterRecordSize, kRosterRecordSize);
            DrawHexView("roster_hex", r.raw.data(), kRosterRecordSize,
                        selected_ * kRosterRecordSize);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Globals")) {
            drawGlobalOverlay();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ui::EndMasterDetail();
}

}  // namespace mm2
