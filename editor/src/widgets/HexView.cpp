#include "widgets/HexView.h"

#include <cstdio>

#include "core/Mm2BitmapFont.h"
#include "imgui.h"
#include "widgets/UiTheme.h"

namespace mm2 {

void DrawHexView(const char* id, const uint8_t* data, size_t size, size_t baseAddr) {
    const int bytesPerRow = 16;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.03f, 0.03f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.12f, 0.12f, 0.40f));
    if (ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        if (ImFont* code = mm2CodeFont()) ImGui::PushFont(code);
        ImGuiListClipper clipper;
        int totalRows = static_cast<int>((size + bytesPerRow - 1) / bytesPerRow);
        clipper.Begin(totalRows);
        char hexPart[96];
        char asciiPart[24];
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                size_t off = static_cast<size_t>(row) * bytesPerRow;
                int n = snprintf(hexPart, sizeof(hexPart), "%06zX  ", baseAddr + off);
                for (int i = 0; i < bytesPerRow; ++i) {
                    if (off + i < size)
                        n += snprintf(hexPart + n, sizeof(hexPart) - n, "%02X ", data[off + i]);
                    else
                        n += snprintf(hexPart + n, sizeof(hexPart) - n, "   ");
                }
                int a = 0;
                for (int i = 0; i < bytesPerRow && off + i < size; ++i) {
                    uint8_t c = data[off + i];
                    asciiPart[a++] = (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
                }
                asciiPart[a] = '\0';

                ImGui::TextUnformatted(hexPart);
                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Text, ui::Muted());
                ImGui::TextUnformatted(asciiPart);
                ImGui::PopStyleColor();
            }
        }
        if (mm2CodeFont()) ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

}  // namespace mm2
