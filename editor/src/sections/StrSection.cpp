#include "sections/StrSection.h"

#include <cstdio>
#include <string>

#include "imgui.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace mm2 {

bool StrSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    linesBuilt_ = false;
    return loaded;
}

bool StrSection::save(const std::string& dataDir) {
    syncTextFromLines();
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

void StrSection::rebuildLines() {
    lines_.clear();
    const std::string& t = file_.text;
    size_t start = 0;
    for (size_t i = 0; i <= t.size(); ++i) {
        if (i == t.size() || t[i] == '\n') {
            lines_.emplace_back(t.substr(start, i - start));
            start = i + 1;
        }
    }
    linesBuilt_ = true;
}

void StrSection::syncTextFromLines() {
    std::string joined;
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (i) joined.push_back('\n');
        joined += lines_[i];
    }
    file_.text = std::move(joined);
}

void StrSection::drawWorkspace(App& app, EditorSelection& sel) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("str.dat not loaded", "Open a folder containing str.dat");
        return;
    }
    if (!linesBuilt_) rebuildLines();

    ImGui::TextDisabled("%zu bytes · %zu lines · transform (byte + 0x1C) & 0xFF", file_.rawSize,
                        lines_.size());
    ImGui::Spacing();

    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto* str = static_cast<std::string*>(data->UserData);
            str->resize(data->BufTextLen);
            data->Buf = str->data();
        }
        return 0;
    };

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("str_lines", 2, flags, ImVec2(0, 0))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ui::Em(3.5f));
        ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(lines_.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                const bool rowSel = selectedLine_ == i;
                if (ImGui::Selectable(std::to_string(i).c_str(), rowSel,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                          ImGuiSelectableFlags_AllowOverlap)) {
                    selectedLine_ = i;
                    sel.Select(DocKind::Str, EditorSelection::Kind::StringLine, i);
                }

                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ui::SetFieldStretch();
                if (ImGui::InputText("##line", lines_[i].data(), lines_[i].size() + 1,
                                     ImGuiInputTextFlags_CallbackResize, callback, &lines_[i])) {
                    syncTextFromLines();
                    dirty = true;
                    selectedLine_ = i;
                    sel.Select(DocKind::Str, EditorSelection::Kind::StringLine, i);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}

void StrSection::drawProperties(App& app, EditorSelection& sel) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("Not loaded", "str.dat missing from the data folder");
        return;
    }
    if (!linesBuilt_) rebuildLines();
    if (sel.doc == DocKind::Str && sel.kind == EditorSelection::Kind::StringLine) selectedLine_ = sel.index;

    ui::SectionBlock("String pool");
    ImGui::Text("%zu lines", lines_.size());
    ImGui::TextDisabled("%zu encoded bytes on disk", file_.rawSize);
    ImGui::Spacing();
    if (selectedLine_ >= 0 && selectedLine_ < static_cast<int>(lines_.size())) {
        ui::SectionBlock("Selected line");
        ImGui::Text("Line %d", selectedLine_);
        ImGui::TextWrapped("%s", lines_[selectedLine_].empty() ? "(empty)" : lines_[selectedLine_].c_str());
        ImGui::TextDisabled("%zu chars", lines_[selectedLine_].size());
    } else {
        ui::EmptyState("No line selected", "Click a row number in the Workspace");
    }
}

}  // namespace mm2
