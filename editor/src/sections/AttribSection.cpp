#include "sections/AttribSection.h"

#include <cstdio>
#include <string>

#include "core/AreaNames.h"
#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace mm2 {

bool AttribSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    return loaded;
}

bool AttribSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

void AttribSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("attrib.dat not loaded.");
        return;
    }

    ui::BeginToolbarRow();
    ui::SetFieldWide();
    std::string cur = std::to_string(screen_) + ": " + areaLabel(screen_);
    if (ImGui::BeginCombo("##screen", cur.c_str())) {
        for (int i = 0; i < kAttribScreens; ++i) {
            std::string lbl = std::to_string(i) + ": " + areaLabel(i);
            if (ImGui::Selectable(lbl.c_str(), screen_ == i)) screen_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("env · neighbors · transition · era · roof");
    ui::EndToolbarRow();
    ImGui::Spacing();

    AttribScreen& s = file_.screens[screen_];

    bool outside = s.isOutside();
    std::string area = areaLabel(screen_);
    ui::PanelHeader(area.c_str(),
                    outside ? "overland" : envTypeName(s.envType()));
    if (outside)
        ImGui::TextDisabled("[overland +0x15 = 0x%02X]", s.raw[attrib_off::kOutsideLabel]);
    else
        ImGui::TextDisabled("[%s  level %d  complex %04X]", envTypeName(s.envType()), s.level(),
                            s.complexId());

    ui::SectionBlock("Identity");
    {
        ui::FormGrid grid("attrib_identity");
        if (grid.begin()) {
            grid.row2(
                "Env type (+0x03)",
                [&] {
                    ui::SetFieldShort();
                    int v = s.raw[attrib_off::kEnvType];
                    if (ImGui::InputInt("##env", &v)) {
                        s.raw[attrib_off::kEnvType] = static_cast<uint8_t>(v & 0xFF);
                        dirty = true;
                    }
                },
                "Map category (+0x01)",
                [&] {
                    ui::SetFieldShort();
                    int v = s.raw[attrib_off::kMapCategory];
                    if (ImGui::InputInt("##mapcat", &v)) {
                        s.raw[attrib_off::kMapCategory] = static_cast<uint8_t>(v & 0xFF);
                        dirty = true;
                    }
                });
            grid.row2(
                "Tileset (+0x02)",
                [&] {
                    ui::SetFieldShort();
                    int v = s.raw[attrib_off::kTileset];
                    if (ImGui::InputInt("##tileset", &v)) {
                        s.raw[attrib_off::kTileset] = static_cast<uint8_t>(v & 0xFF);
                        dirty = true;
                    }
                },
                "Surface flag (+0x04)",
                [&] {
                    ui::SetFieldShort();
                    int v = s.raw[attrib_off::kOutsideFlag];
                    if (ImGui::InputInt("##surface", &v)) {
                        s.raw[attrib_off::kOutsideFlag] = static_cast<uint8_t>(v & 0xFF);
                        dirty = true;
                    }
                });
        }
    }

    ui::SectionBlock("World adjacency", "neighbour screens +0x05..0x08");
    ImGui::TextDisabled("Slots (0,2) and (1,3) are opposite directions; self = no link.");
    {
        ui::FormTable form("attrib_neighbors");
        if (form.begin()) {
            auto neighborCombo = [&](const char* label, int off) {
                form.row(label, [&] {
                    int v = s.raw[off];
                    std::string curN = std::to_string(v) + ": " + areaLabel(v);
                    ui::SetFieldWide();
                    if (ImGui::BeginCombo(("##" + std::to_string(off)).c_str(), curN.c_str())) {
                        for (int i = 0; i < kAttribScreens; ++i) {
                            std::string lbl = std::to_string(i) + ": " + areaLabel(i);
                            if (ImGui::Selectable(lbl.c_str(), v == i)) {
                                s.raw[off] = static_cast<uint8_t>(i);
                                dirty = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                });
            };
            neighborCombo("Neighbour 0 / N (+0x05)", attrib_off::kNeighbor0 + 0);
            neighborCombo("Neighbour 1 / E (+0x06)", attrib_off::kNeighbor0 + 1);
            neighborCombo("Neighbour 2 / S (+0x07)", attrib_off::kNeighbor0 + 2);
            neighborCombo("Neighbour 3 / W (+0x08)", attrib_off::kNeighbor0 + 3);
        }
    }

    ui::SectionBlock("Entry / Transition / Era");
    ImGui::Text("Entry pos (+0x0E): (%d, %d)", s.entryX(), s.entryY());
    {
        ui::FormGrid grid("attrib_transition");
        if (grid.begin()) {
            grid.row1("Entry coord byte (+0x0E)", [&] {
                ui::SetFieldShort();
                int v = s.raw[attrib_off::kEntryCoord];
                if (ImGui::InputInt("##entry", &v)) {
                    s.raw[attrib_off::kEntryCoord] = static_cast<uint8_t>(v & 0xFF);
                    dirty = true;
                }
            });
            grid.row2(
                "Transition coord (+0x16)",
                [&] {
                    ui::SetFieldShort();
                    int v = s.raw[attrib_off::kTransitionCoord];
                    if (ImGui::InputInt("##trcoord", &v)) {
                        s.raw[attrib_off::kTransitionCoord] = static_cast<uint8_t>(v & 0xFF);
                        dirty = true;
                    }
                },
                "Transition screen (+0x18)",
                [&] {
                    ui::SetFieldShort();
                    int v = s.raw[attrib_off::kTransitionScreen];
                    if (ImGui::InputInt("##trscreen", &v)) {
                        s.raw[attrib_off::kTransitionScreen] = static_cast<uint8_t>(v & 0xFF);
                        dirty = true;
                    }
                });
            grid.row1("Era gate (+0x0F)", [&] {
                ui::SetFieldShort();
                int v = s.raw[attrib_off::kEraGate];
                if (ImGui::InputInt("##era", &v)) {
                    s.raw[attrib_off::kEraGate] = static_cast<uint8_t>(v & 0xFF);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(events run when this == current era index; 0x09 = default)");
            });
        }
    }
    ImGui::Text("Transition -> screen %d (%s) at (%d, %d)", s.transitionScreen(),
                areaLabel(s.transitionScreen()).c_str(), s.transitionX(), s.transitionY());

    ui::SectionBlock("Flags", "+0x1A");
    static const char* kFlagNames[8] = {
        "bit0 (asm 0xBCCA)", "bit1", "bit2", "bit3 (asm 0xBB98)",
        "bit4 (asm 0xADE8)", "bit5 (asm 0xB006)",
        "bit6 transition gate", "bit7 underground/cavern"};
    ui::CheckboxGrid("attrib_flags", 2, 8, [&](int b) {
        bool on = (s.raw[attrib_off::kFlags] >> b) & 1;
        ImGui::PushID(b);
        if (ImGui::Checkbox(kFlagNames[b], &on)) {
            if (on) s.raw[attrib_off::kFlags] |= (1 << b);
            else s.raw[attrib_off::kFlags] &= ~(1 << b);
            dirty = true;
        }
        ImGui::PopID();
    });

    ui::SectionBlock("Roof bits", "16x16 +0x20..0x3F");
    if (ImGui::BeginTable("attrib_roof", 16,
                          ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoPadOuterX)) {
        for (int y = 0; y < 16; ++y) {
            ImGui::TableNextRow();
            for (int x = 0; x < 16; ++x) {
                int tile = attribRoofTileAt(x, y);
                int byte = attrib_off::kRoofBits + (tile >> 3);
                int bit = tile & 7;
                bool on = (s.raw[byte] >> bit) & 1;
                ImGui::TableNextColumn();
                ImGui::PushID(tile);
                if (ImGui::Checkbox("##roof", &on)) {
                    if (on) s.raw[byte] |= (1 << bit);
                    else s.raw[byte] &= ~(1 << bit);
                    dirty = true;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    if (ui::BeginHexBlock("Raw record")) {
        DrawHexView("attrib_hex", s.raw.data(), kAttribRecordSize, screen_ * kAttribRecordSize);
        ui::EndHexBlock();
    }
}

}  // namespace mm2
