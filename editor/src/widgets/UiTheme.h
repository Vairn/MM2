#pragma once
// Shared semantic colors / chrome helpers for MM2ED.
// Keep the crimson-on-near-black identity; use these instead of ad-hoc IM_COL32.

#include <cstdarg>

#include "imgui.h"

namespace mm2::ui {

inline ImVec4 Accent() { return ImVec4(0.86f, 0.28f, 0.28f, 1.00f); }
inline ImVec4 AccentSoft() { return ImVec4(0.55f, 0.14f, 0.14f, 1.00f); }
inline ImVec4 Danger() { return ImVec4(0.95f, 0.42f, 0.42f, 1.00f); }
inline ImVec4 DangerBg() { return ImVec4(0.18f, 0.04f, 0.04f, 1.00f); }
inline ImVec4 Warn() { return ImVec4(0.92f, 0.72f, 0.38f, 1.00f); }
inline ImVec4 Success() { return ImVec4(0.45f, 0.82f, 0.52f, 1.00f); }
inline ImVec4 SuccessBg() { return ImVec4(0.05f, 0.14f, 0.07f, 1.00f); }
inline ImVec4 Muted() { return ImVec4(0.55f, 0.48f, 0.48f, 1.00f); }
inline ImVec4 CanvasBg() { return ImVec4(0.04f, 0.03f, 0.04f, 1.00f); }
inline ImVec4 Selection() { return ImVec4(0.95f, 0.78f, 0.35f, 1.00f); }
inline ImVec4 MapEvent() { return ImVec4(0.92f, 0.38f, 0.38f, 1.00f); }
inline ImVec4 MapPlayer() { return ImVec4(0.95f, 0.82f, 0.28f, 1.00f); }

inline ImU32 ToU32(const ImVec4& c) {
    return ImGui::ColorConvertFloat4ToU32(c);
}

inline ImU32 AccentU32(int alpha = 255) {
    ImVec4 c = Accent();
    c.w = alpha / 255.0f;
    return ToU32(c);
}

inline void TextDanger(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, Danger());
    ImGui::TextV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
}

inline void TextWarn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, Warn());
    ImGui::TextV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
}

inline void TextSuccess(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, Success());
    ImGui::TextV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
}

inline void TextMuted(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, Muted());
    ImGui::TextV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
}

// Compact status chip (compile / dirty / ok).
inline void StatusChip(const char* label, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
}

// Soft filled banner for error / success strips.
inline void BeginBanner(const char* id, const ImVec4& bg, float height = 0.f) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::BeginChild(id, ImVec2(0, height), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
}

inline void EndBanner() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

inline void PushDangerBanner(const char* id) { BeginBanner(id, DangerBg()); }
inline void PushSuccessBanner(const char* id) { BeginBanner(id, SuccessBg()); }

// Yellow selection ring for thumbnails / frame grids.
struct SelectedFrameScope {
    bool active = false;
    SelectedFrameScope(bool selected) : active(selected) {
        if (!active) return;
        ImGui::PushStyleColor(ImGuiCol_Border, Selection());
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    }
    ~SelectedFrameScope() {
        if (!active) return;
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
};

}  // namespace mm2::ui
