#include "sections/StrSection.h"

#include <cstdio>

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

void StrSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("str.dat not loaded.");
        return;
    }
    if (!linesBuilt_) rebuildLines();

    ui::PanelHeader("String pool", "str.dat");
    ImGui::TextDisabled("%zu bytes · %zu lines · transform (byte + 0x1C) & 0xFF", file_.rawSize,
                        lines_.size());
    ImGui::Spacing();

    // Resizable per-row buffer backed by the std::string.
    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto* str = static_cast<std::string*>(data->UserData);
            str->resize(data->BufTextLen);
            data->Buf = str->data();
        }
        return 0;
    };

    // A ListClipper-driven table keeps the draw list small: only the visible
    // rows are emitted, so the whole pool never overflows the 16-bit index
    // limit the way a single giant InputTextMultiline did.
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
                ImGui::TextDisabled("%d", i);

                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ui::SetFieldStretch();
                if (ImGui::InputText("##line", lines_[i].data(), lines_[i].size() + 1,
                                     ImGuiInputTextFlags_CallbackResize, callback, &lines_[i])) {
                    syncTextFromLines();
                    dirty = true;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}

}  // namespace mm2
