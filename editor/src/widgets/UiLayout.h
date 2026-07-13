#pragma once

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>

#include "imgui.h"
#include "widgets/UiTheme.h"

namespace mm2::ui {

inline float Em(float n) { return ImGui::GetFontSize() * n; }

inline float ListWidth() { return Em(16.0f); }
inline float LabelWidth() { return Em(8.0f); }
inline float FieldByte() { return Em(4.0f); }
inline float FieldShort() { return Em(8.0f); }
inline float FieldMed() { return Em(10.0f); }
inline float FieldWide() { return Em(14.0f); }
inline float FieldCombo() { return Em(18.0f); }
inline float PanelGap() { return Em(0.6f); }
inline float HexPaneHeight() { return Em(14.0f); }

inline void SetFieldWidth(float w) { ImGui::SetNextItemWidth(w); }
inline void SetFieldStretch() { ImGui::SetNextItemWidth(-FLT_MIN); }
inline void SetFieldByte() { SetFieldWidth(FieldByte()); }
inline void SetFieldShort() { SetFieldWidth(FieldShort()); }
inline void SetFieldMed() { SetFieldWidth(FieldMed()); }
inline void SetFieldWide() { SetFieldWidth(FieldWide()); }
inline void SetFieldCombo() { SetFieldWidth(FieldCombo()); }

inline void SameLineRightAlign(float width) {
    float x = ImGui::GetContentRegionMax().x - width;
    if (x < ImGui::GetCursorPosX()) x = ImGui::GetCursorPosX();
    ImGui::SameLine(x);
}

inline void SectionBlock(const char* title, const char* subtitle = nullptr) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Accent());
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (subtitle && subtitle[0]) {
        ImGui::SameLine(0, Em(0.5f));
        ImGui::TextDisabled("%s", subtitle);
    }
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.40f, 0.14f, 0.14f, 0.35f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

inline void EmptyState(const char* message) {
    ImGui::Spacing();
    ImGui::TextDisabled("%s", message);
}

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
    ImU32 col = active ? AccentU32(220) : AccentU32(120);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(mid, a.y + 4.f), ImVec2(mid, b.y - 4.f), col, 1.5f);
    ImGui::PopID();
    return active;
}

struct MasterDetail {
    float listWidth = 0.f;
    char filter[64] = {};
    bool showFilter = true;
};

inline bool ListFilterPass(const MasterDetail& md, const char* text) {
    if (!md.filter[0]) return true;
    if (!text) return false;
    auto lower = [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    for (const char* h = text; *h; ++h) {
        const char* a = h;
        const char* b = md.filter;
        while (*a && *b && lower(*a) == lower(*b)) {
            ++a;
            ++b;
        }
        if (!*b) return true;
    }
    return false;
}

inline bool BeginMasterList(MasterDetail& md, const char* listId, const char* listTitle = nullptr) {
    if (md.listWidth <= 0.f) md.listWidth = ListWidth();
    const float minW = Em(11.0f);
    const float maxW = (std::max)(minW, ImGui::GetContentRegionAvail().x * 0.45f);
    md.listWidth = (std::clamp)(md.listWidth, minW, maxW);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(0.55f), Em(0.45f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.03f, 0.03f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.32f, 0.12f, 0.12f, 0.40f));

    if (!ImGui::BeginChild(listId, ImVec2(md.listWidth, 0), ImGuiChildFlags_Borders)) {
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return false;
    }

    if (listTitle && listTitle[0]) {
        ImGui::TextDisabled("%s", listTitle);
        ImGui::Spacing();
    }
    if (md.showFilter) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "Filter…", md.filter, sizeof(md.filter));
        ImGui::Spacing();
    }
    return true;
}

inline bool ListRow(int index, const char* name, bool selected, const char* emptyLabel = "(blank)") {
    char label[96];
    const char* shown = (name && name[0]) ? name : emptyLabel;
    std::snprintf(label, sizeof(label), "%3d  %s", index, shown);
    return ImGui::Selectable(label, selected);
}

inline void EndMasterListBeginDetail(MasterDetail& md, const char* detailId) {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    const float minW = Em(11.0f);
    const float maxW = (std::max)(minW, ImGui::GetContentRegionAvail().x * 0.55f);
    VSplitter("##md_split", &md.listWidth, minW, maxW);

    ImGui::SameLine(0, PanelGap());
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(0.7f), Em(0.55f)));
    ImGui::BeginChild(detailId, ImVec2(0, 0), ImGuiChildFlags_None);
}

inline void EndMasterDetail() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

// Legacy wrappers — per-list-id state so sections don't share one width.
inline MasterDetail& LegacyMasterDetail(const char* listId = nullptr) {
    static std::unordered_map<std::string, MasterDetail> map;
    static std::string last = "legacy";
    if (listId && listId[0]) last = listId;
    return map[last];
}
inline bool BeginListPanel(const char* id, float width = -1.f) {
    auto& md = LegacyMasterDetail(id);
    md.showFilter = true;
    if (width > 0.f) md.listWidth = width;
    return BeginMasterList(md, id);
}
inline void ListPanelNextDetail(const char* id) {
    EndMasterListBeginDetail(LegacyMasterDetail(nullptr), id);
}
inline void EndDetailPanel() { EndMasterDetail(); }

inline bool BeginHexBlock(const char* title, bool defaultOpen = false) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    const bool open = ImGui::TreeNodeEx(title, flags);
    if (open) {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("##hex_body", ImVec2(0, HexPaneHeight()), ImGuiChildFlags_Borders);
    }
    return open;
}
inline void EndHexBlock() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::TreePop();
}

class FormTable {
public:
    explicit FormTable(const char* id, float labelW = -1.f) : id_(id), labelW_(labelW) {}

    bool begin() {
        open_ = ImGui::BeginTable(id_, 2,
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX);
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

class FormGrid {
public:
    explicit FormGrid(const char* id, float labelW = -1.f, float fieldW = -1.f)
        : id_(id), labelW_(labelW), fieldW_(fieldW) {}

    bool begin() {
        open_ = ImGui::BeginTable(id_, 4,
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX);
        if (!open_) return false;
        float lw = labelW_ > 0.f ? labelW_ : LabelWidth();
        ImGui::TableSetupColumn("label0", ImGuiTableColumnFlags_WidthFixed, lw);
        ImGui::TableSetupColumn("field0", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("label1", ImGuiTableColumnFlags_WidthFixed, lw);
        ImGui::TableSetupColumn("field1", ImGuiTableColumnFlags_WidthStretch);
        (void)fieldW_;
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

inline void BeginToolbarRow() { ImGui::BeginGroup(); }
inline void ToolbarSpacer() {
    ImGui::EndGroup();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x, 0);
    ImGui::BeginGroup();
}
inline void EndToolbarRow() { ImGui::EndGroup(); }

inline void PanelHeader(const char* title, const char* subtitle = nullptr,
                        const char* chip = nullptr, const ImVec4* chipColor = nullptr) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 4.f));
    ImGui::TextUnformatted(title);
    if (subtitle && subtitle[0]) {
        ImGui::SameLine(0, Em(0.5f));
        ImGui::TextDisabled("%s", subtitle);
    }
    if (chip && chip[0]) {
        ImGui::SameLine(0, Em(0.6f));
        StatusChip(chip, chipColor ? *chipColor : Warn());
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PopStyleVar();
}

}  // namespace mm2::ui
