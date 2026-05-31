#include "sections/MonstersSection.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/Texture.h"
#include "widgets/UiLayout.h"

namespace mm2 {

MonstersSection::~MonstersSection() { releaseTextures(); }

void MonstersSection::releaseTextures() {
    for (unsigned int t : textures_) freeTexture(t);
    textures_.clear();
}

bool MonstersSection::load(const std::string& dataDir) {
    dir_ = dataDir;
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    releaseTextures();
    sprite_ = GfxImage{};
    spritePic_ = -1;
    return loaded;
}

bool MonstersSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

void MonstersSection::loadSprite(uint8_t picture) {
    releaseTextures();
    sprite_ = GfxImage{};
    spriteFrame_ = 0;
    spritePic_ = picture;

    int idx = picture & 0x7F;
    char name[16];
    snprintf(name, sizeof(name), "%02d.anm", idx);
    spriteFile_ = name;

    if (idx <= 0) {
        sprite_.error = "no sprite (picture & 0x7F == 0)";
        return;
    }
    gfxLoad(dir_ + "/" + spriteFile_, /*isAnm=*/true, sprite_);
    if (sprite_.ok || !sprite_.frames.empty()) {
        textures_.reserve(sprite_.frames.size());
        for (auto& fr : sprite_.frames)
            textures_.push_back(makeTextureRGBA(fr.rgba.data(), fr.width, fr.height));
    }
}

void MonstersSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ImGui::TextDisabled("monsters.dat not loaded.");
        return;
    }

    ui::BeginListPanel("mon_list");
    for (int i = 0; i < kMonstersCount; ++i) {
        char label[64];
        std::string nm = file_.records[i].nameStr();
        snprintf(label, sizeof(label), "%3d  %s", i, nm.empty() ? "(blank)" : nm.c_str());
        if (ImGui::Selectable(label, selected_ == i)) selected_ = i;
    }
    ui::ListPanelNextDetail("mon_detail");

    MonsterRecord& r = file_.records[selected_];

    char nameBuf[kMonsterNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kMonsterNameSize] = '\0';

    {
        ui::FormTable form("mon_name");
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

    uint8_t pic = r.raw[0x15];
    if (spritePic_ != pic) loadSprite(pic);

    if (ImGui::BeginTable("mon_body", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("sprite", ImGuiTableColumnFlags_WidthFixed, ui::Em(20.f));
        ImGui::TableSetupColumn("props", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::SeparatorText("Sprite");
        ImGui::Text("%s", spriteFile_.c_str());
        ImGui::TextDisabled("picture=%u  (file %u, flag 0x80=%s)", pic, pic & 0x7F,
                            (pic & 0x80) ? "set" : "clear");
        if (sprite_.ok || !sprite_.frames.empty()) {
            int n = static_cast<int>(sprite_.frames.size());
            ImGui::TextDisabled("%d frames  depth=%d", n, sprite_.depth);
            ui::SetFieldMed();
            ImGui::SliderInt("Frame##sprite", &spriteFrame_, 0, n > 0 ? n - 1 : 0);
            ui::SetFieldShort();
            ImGui::SliderFloat("Zoom##sprite", &spriteZoom_, 1.0f, 6.0f, "%.0fx");
            if (spriteFrame_ >= 0 && spriteFrame_ < n) {
                const GfxFrame& fr = sprite_.frames[spriteFrame_];
                unsigned int tex = textures_[spriteFrame_];
                if (tex) {
                    ImVec2 sz(fr.width * spriteZoom_, fr.height * spriteZoom_);
                    ImGui::Image(static_cast<ImTextureID>(tex), sz);
                }
                ImGui::TextDisabled("%dx%d  flags=0x%X", fr.width, fr.height, fr.flags);
            }
        } else {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No sprite: %s",
                               sprite_.error.empty() ? "decode failed" : sprite_.error.c_str());
        }

        ImGui::TableNextColumn();
        auto effectCombo = [&](const char* label, uint8_t cur, const char* const* names,
                               int count, void (MonsterRecord::*setter)(uint8_t)) {
            ui::SetFieldWide();
            const char* curName = cur < count ? names[cur] : "?";
            if (ImGui::BeginCombo(label, curName)) {
                for (int e = 0; e < count; ++e) {
                    bool sel = (e == cur);
                    char item[48];
                    snprintf(item, sizeof(item), "%2d  %s", e, names[e]);
                    if (ImGui::Selectable(item, sel)) {
                        (r.*setter)(static_cast<uint8_t>(e));
                        dirty = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        };

        ImGui::SeparatorText("Abilities (ASM-confirmed)");
        ImGui::TextDisabled("Single-target attack (Sabil 0x%02X)", r.sabil());
        effectCombo("Effect##single", r.singleEffect(), kAbilityNames, kAbilityCount,
                    &MonsterRecord::setSingleEffect);
        bool undead = r.isUndead();
        if (ImGui::Checkbox("Undead", &undead)) {
            r.setUndead(undead);
            dirty = true;
        }
        ImGui::SameLine();
        bool archer = r.isArcher();
        if (ImGui::Checkbox("Archer", &archer)) {
            r.setArcher(archer);
            dirty = true;
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Group attack (Pabil 0x%02X)", r.pabil());
        effectCombo("Verb##party", r.partyVerb(), kPartyVerbNames, kPartyVerbCount,
                    &MonsterRecord::setPartyVerb);
        {
            ui::SetFieldShort();
            int chance = r.partyChance();
            if (ImGui::SliderInt("Use-chance tier##party", &chance, 0, 7)) {
                r.setPartyChance(static_cast<uint8_t>(chance));
                dirty = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Pabil bits 5-7. Indexes a frequency table at\n"
                                  "asm 0x10002 - higher = used more often.");
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Other behavior (Oabil 0x%02X)", r.oabil());
        ImGui::Text("Adds friends: %u reinforcements", r.addFriends());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Oabil low nibble +1 (x10 if bit4). Count of monsters\n"
                              "summoned by the \"adds friends!\" action (asm 0x11F0A).");
        ImGui::Text("Flee tier: %u", r.fleeTier());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Oabil bits 5-6. Chance the monster \"runs\" / flees\n"
                              "(asm 0x10DFC).");
        bool mult = r.multiplies();
        if (ImGui::Checkbox("Multiplies / breeds", &mult)) {
            r.setMultiplies(mult);
            dirty = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Oabil bit7. Monster duplicates itself in combat\n"
                              "(asm 0x100B0).");

        ImGui::SeparatorText("Named fields (semantics partial)");
        struct Field {
            const char* label;
            int off;
        };
        const Field fields[] = {
            {"HP code", 0x0E},   {"XP code", 0x0F},  {"Treasure", 0x10},
            {"Pabil", 0x11},     {"Sabil", 0x12},    {"Oabil", 0x13},
            {"Speed", 0x14},     {"Picture", 0x15},  {"Armor class", 0x16},
            {"Damage", 0x17},    {"Speed2", 0x18},   {"Magic resist", 0x19},
        };
        {
            ui::FormGrid grid("mon_fields");
            if (grid.begin()) {
                for (int i = 0; i < static_cast<int>(sizeof(fields) / sizeof(fields[0])); i += 2) {
                    const Field& f0 = fields[i];
                    const Field& f1 = fields[i + 1];
                    grid.row2(
                        f0.label,
                        [&] {
                            ui::SetFieldStretch();
                            int v = r.raw[f0.off];
                            if (ImGui::InputInt(("##" + std::to_string(f0.off)).c_str(), &v)) {
                                r.raw[f0.off] = static_cast<uint8_t>(v & 0xFF);
                                dirty = true;
                            }
                        },
                        f1.label,
                        [&] {
                            ui::SetFieldStretch();
                            int v = r.raw[f1.off];
                            if (ImGui::InputInt(("##" + std::to_string(f1.off)).c_str(), &v)) {
                                r.raw[f1.off] = static_cast<uint8_t>(v & 0xFF);
                                dirty = true;
                            }
                        });
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Raw record");
    ImGui::Text("Record %d @ 0x%04X (%d bytes)", selected_, selected_ * kMonsterRecordSize,
                kMonsterRecordSize);
    DrawHexView("mon_hex", r.raw.data(), kMonsterRecordSize, selected_ * kMonsterRecordSize);

    ui::EndDetailPanel();
}

}  // namespace mm2
