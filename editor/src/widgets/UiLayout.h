#pragma once

#include <cfloat>
#include <functional>

#include "imgui.h"

namespace mm2::ui {

// All widths are expressed in "em" (multiples of the current, DPI-scaled font
// size). Hardcoded pixel widths do NOT scale with the font, so on a HiDPI
// display labels get truncated and the numeric +/- fields collapse until the
// value can't render. Deriving every width from GetFontSize() keeps labels,
// list panels and value bars proportional at any DPI.
inline float Em(float n) { return ImGui::GetFontSize() * n; }

inline float ListWidth() { return Em(15.0f); }
inline float LabelWidth() { return Em(9.5f); }
inline float FieldByte() { return Em(4.0f); }
inline float FieldShort() { return Em(9.0f); }   // value + +/- buttons, comfortably
inline float FieldMed() { return Em(11.0f); }
inline float FieldWide() { return Em(16.0f); }
inline float FieldCombo() { return Em(22.0f); }
inline float PanelGap() { return Em(0.7f); }

inline void SetFieldWidth(float w) { ImGui::SetNextItemWidth(w); }
inline void SetFieldStretch() { ImGui::SetNextItemWidth(-FLT_MIN); }
inline void SetFieldByte() { SetFieldWidth(FieldByte()); }
inline void SetFieldShort() { SetFieldWidth(FieldShort()); }
inline void SetFieldMed() { SetFieldWidth(FieldMed()); }
inline void SetFieldWide() { SetFieldWidth(FieldWide()); }
inline void SetFieldCombo() { SetFieldWidth(FieldCombo()); }

// Right-align the next group of widgets to the content region's right edge.
// `width` is the total pixel width of the controls that will follow.
inline void SameLineRightAlign(float width) {
    float x = ImGui::GetContentRegionMax().x - width;
    if (x < ImGui::GetCursorPosX()) x = ImGui::GetCursorPosX();
    ImGui::SameLine(x);
}

// Left list panel (bordered) + right detail panel. width<=0 => default (scaled).
inline bool BeginListPanel(const char* id, float width = -1.f) {
    if (width <= 0.f) width = ListWidth();
    return ImGui::BeginChild(id, ImVec2(width, 0), true);
}
inline void ListPanelNextDetail(const char* id) {
    ImGui::EndChild();
    ImGui::SameLine(0, PanelGap());
    ImGui::BeginChild(id, ImVec2(0, 0), false);
}
inline void EndDetailPanel() { ImGui::EndChild(); }

// Label | field rows (single column of inputs). labelW<=0 => default (scaled).
class FormTable {
public:
    explicit FormTable(const char* id, float labelW = -1.f) : id_(id), labelW_(labelW) {}

    bool begin() {
        open_ = ImGui::BeginTable(id_, 2,
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX);
        if (!open_) return false;
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed,
                                labelW_ > 0.f ? labelW_ : LabelWidth());
        ImGui::TableSetupColumn("field", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    void row(const char* label, const std::function<void()>& field) {
        if (!open_) return;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        field();
    }

    void end() {
        if (open_) ImGui::EndTable();
        open_ = false;
    }

    ~FormTable() { end(); }

private:
    const char* id_;
    float labelW_;
    bool open_ = false;
};

// Two fields per row: label | field | label | field. labelW/fieldW<=0 => scaled.
class FormGrid {
public:
    explicit FormGrid(const char* id, float labelW = -1.f, float fieldW = -1.f)
        : id_(id), labelW_(labelW), fieldW_(fieldW) {}

    bool begin() {
        open_ = ImGui::BeginTable(id_, 4,
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX);
        if (!open_) return false;
        float lw = labelW_ > 0.f ? labelW_ : LabelWidth();
        float fw = fieldW_ > 0.f ? fieldW_ : FieldMed();
        ImGui::TableSetupColumn("label0", ImGuiTableColumnFlags_WidthFixed, lw);
        ImGui::TableSetupColumn("field0", ImGuiTableColumnFlags_WidthFixed, fw);
        ImGui::TableSetupColumn("label1", ImGuiTableColumnFlags_WidthFixed, lw);
        ImGui::TableSetupColumn("field1", ImGuiTableColumnFlags_WidthFixed, fw);
        return true;
    }

    void row2(const char* label0, const std::function<void()>& field0, const char* label1,
              const std::function<void()>& field1) {
        if (!open_) return;
        ImGui::TableNextRow();
        cellLabel(label0);
        cellField(field0);
        cellLabel(label1);
        cellField(field1);
    }

    void row1(const char* label, const std::function<void()>& field) {
        if (!open_) return;
        ImGui::TableNextRow();
        cellLabel(label);
        cellField(field);
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
    }

    void end() {
        if (open_) ImGui::EndTable();
        open_ = false;
    }

    ~FormGrid() { end(); }

private:
    void cellLabel(const char* text) {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
    }
    void cellField(const std::function<void()>& field) {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        field();
    }

    const char* id_;
    float labelW_;
    float fieldW_;
    bool open_ = false;
};

// Evenly spaced checkbox columns.
template <typename Fn>
inline void CheckboxGrid(const char* id, int columns, int count, Fn&& drawCell) {
    if (!ImGui::BeginTable(id, columns, ImGuiTableFlags_SizingStretchSame)) return;
    for (int i = 0; i < count; ++i) {
        if (i % columns == 0) ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawCell(i);
    }
    ImGui::EndTable();
}

// Toolbar row: primary control on the left, secondary actions on the right.
inline void BeginToolbarRow() { ImGui::BeginGroup(); }
inline void ToolbarSpacer() {
    ImGui::EndGroup();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x, 0);
    ImGui::BeginGroup();
}
inline void EndToolbarRow() { ImGui::EndGroup(); }

// Compact panel chrome: title + optional muted subtitle, then a hairline.
inline void PanelHeader(const char* title, const char* subtitle = nullptr) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.f));
    ImGui::TextUnformatted(title);
    if (subtitle && subtitle[0]) {
        ImGui::SameLine(0, Em(0.5f));
        ImGui::TextDisabled("%s", subtitle);
    }
    ImGui::Separator();
    ImGui::PopStyleVar();
}

// Drag a vertical strip to resize *width. `rightPanel` inverts the delta so the
// strip can sit to the left of a right-hand panel.
inline bool VSplitter(const char* id, float* width, float minW, float maxW,
                      bool rightPanel = false) {
    ImGui::PushID(id);
    ImGui::SameLine(0, 0);
    const float thick = Em(0.35f);
    ImGui::InvisibleButton("##vs", ImVec2(thick, -1.f));
    const bool active = ImGui::IsItemActive();
    if (ImGui::IsItemHovered() || active) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (active && width) {
        *width += rightPanel ? -ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.x;
        if (*width < minW) *width = minW;
        if (*width > maxW) *width = maxW;
    }
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    const float mid = (a.x + b.x) * 0.5f;
    ImU32 col = active ? IM_COL32(180, 50, 50, 220) : IM_COL32(120, 35, 35, 160);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(mid, a.y + 4.f), ImVec2(mid, b.y - 4.f), col, 1.5f);
    ImGui::PopID();
    return active;
}

}  // namespace mm2::ui
