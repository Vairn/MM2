#include "widgets/HexView.h"

#include <cstdio>

#include "imgui.h"

namespace mm2 {

void DrawHexView(const char* id, const uint8_t* data, size_t size, size_t baseAddr) {
    const int bytesPerRow = 16;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
    if (ImGui::BeginChild(id, ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::PushFont(nullptr);  // default font (monospace-ish enough)
        ImGuiListClipper clipper;
        int totalRows = static_cast<int>((size + bytesPerRow - 1) / bytesPerRow);
        clipper.Begin(totalRows);
        char line[128];
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                size_t off = static_cast<size_t>(row) * bytesPerRow;
                int n = snprintf(line, sizeof(line), "%06zX  ", baseAddr + off);
                for (int i = 0; i < bytesPerRow; ++i) {
                    if (off + i < size)
                        n += snprintf(line + n, sizeof(line) - n, "%02X ", data[off + i]);
                    else
                        n += snprintf(line + n, sizeof(line) - n, "   ");
                }
                n += snprintf(line + n, sizeof(line) - n, " ");
                for (int i = 0; i < bytesPerRow && off + i < size; ++i) {
                    uint8_t c = data[off + i];
                    line[n++] = (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
                }
                line[n] = '\0';
                ImGui::TextUnformatted(line);
            }
        }
        ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

}  // namespace mm2
