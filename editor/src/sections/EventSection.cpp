#include "sections/EventSection.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "app/App.h"
#include "core/AreaNames.h"
#include "core/EventOps.h"
#include "imgui.h"
#include "imnodes.h"
#include "widgets/UiLayout.h"

namespace mm2 {

// ---- unique id spaces (node / pin / static attribute) ----------------------
static inline int kTrigNode(int e)   { return 0x10000 + e; }
static inline int kEvtNode(int e)    { return 0x20000 + e; }
static inline int kStrNode(int s)    { return 0x30000 + s; }
static inline int kTrigOut(int e)    { return 0x40000 + e; }
static inline int kEvtIn(int e)      { return 0x50000 + e; }
static inline int kOpOut(int e, int i)  { return 0x60000 + e * 256 + i; }
static inline int kStrIn(int s)      { return 0x70000 + s; }
static inline int kStaticAttr(int e, int i) { return 0x80000 + e * 256 + i; }

bool EventSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    layoutForLoc_ = -1;
    return loaded;
}

bool EventSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

static std::string locLabel(int id) {
    char buf[96];
    if (id < kAreaCount)
        snprintf(buf, sizeof(buf), "%02d  %s", id, areaLabel(id).c_str());
    else
        snprintf(buf, sizeof(buf), "%02d  (extra area)", id);
    return buf;
}

void EventSection::draw(App& app) {
    if (!loaded) {
        ImGui::TextDisabled("event.dat not loaded.");
        return;
    }
    if (file_.locations.empty()) {
        ImGui::TextDisabled("No locations decoded (file too small?).");
        return;
    }

    selectedLoc_ = std::clamp(selectedLoc_, 0, static_cast<int>(file_.locations.size()) - 1);

    ui::SetFieldCombo();
    if (ImGui::BeginCombo("Location", locLabel(selectedLoc_).c_str())) {
        for (int i = 0; i < static_cast<int>(file_.locations.size()); ++i) {
            bool sel = (i == selectedLoc_);
            if (ImGui::Selectable(locLabel(i).c_str(), sel)) selectedLoc_ = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    EventLocation& loc = file_.locations[selectedLoc_];

    // Right-align the toolbar actions (String-nodes toggle + Relayout).
    const ImGuiStyle& st = ImGui::GetStyle();
    float cbW = ImGui::GetFrameHeight() + st.ItemInnerSpacing.x +
                ImGui::CalcTextSize("String nodes").x;
    float btnW = ImGui::CalcTextSize("Relayout").x + st.FramePadding.x * 2.f;
    ui::SameLineRightAlign(cbW + st.ItemSpacing.x + btnW);
    ImGui::Checkbox("String nodes", &showStrings_);
    ImGui::SameLine();
    if (ImGui::SmallButton("Relayout")) layoutForLoc_ = -1;

    ImGui::TextDisabled(
        "offset=0x%06X  len=%u  triggers=%zu  segments=%zu  strings=%zu",
        loc.offset, loc.length, loc.triplets.size(), loc.segments.size(),
        loc.strings.size());
    if (!loc.terminated) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 180, 80, 255));
        ImGui::TextWrapped(
            "WARNING: no 00 00 00 triplet terminator (castle / Hall of Spells "
            "style record). Triggers/segments below are unreliable.");
        ImGui::PopStyleColor();
    }
    ImGui::TextDisabled(
        "Drag nodes to arrange. Edit the hex byte fields to patch opcode "
        "arguments / trigger flags in place; File > Save writes event.dat.");
    ImGui::Separator();

    drawGraph(app, loc);
}

void EventSection::drawGraph(App& app, EventLocation& loc) {
    const bool doLayout = (layoutForLoc_ != selectedLoc_);

    auto strAt = [&](int idx) -> std::string {
        if (idx >= 0 && idx < static_cast<int>(loc.strings.size())) return loc.strings[idx];
        return std::string();
    };
    auto itemAt = [&](int id) -> std::string { return app.itemName(id); };

    // Group triggers by event id (matches decode_event dump_decompiled order).
    std::map<int, std::vector<const EventTriplet*>> byEvt;
    for (const auto& t : loc.triplets) byEvt[t.event].push_back(&t);

    ImNodes::BeginNodeEditor();

    struct StrLink { int outPin; int strIdx; };
    std::vector<StrLink> stringLinks;
    std::set<int> usedStrings;

    float yCursor = 16.0f;

    for (auto& [evt, tiles] : byEvt) {
        const float rowY = yCursor;

        // ---- trigger node -------------------------------------------------
        if (doLayout) ImNodes::SetNodeGridSpacePos(kTrigNode(evt), ImVec2(16, rowY));
        ImNodes::BeginNode(kTrigNode(evt));
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("Triggers -> EVT %02d", evt);
        ImNodes::EndNodeTitleBar();
        for (size_t ti = 0; ti < tiles.size(); ++ti) {
            const EventTriplet* t = tiles[ti];
            ImGui::PushID(static_cast<int>(t->absOff));
            uint8_t posLive = (t->absOff < file_.raw.size()) ? file_.raw[t->absOff] : t->pos;
            ImGui::Text("tile (y=%d,x=%d)", (posLive >> 4) & 0xF, posLive & 0xF);
            ImGui::SameLine();
            ui::SetFieldByte();
            if (file_.raw.size() > t->absOff &&
                ImGui::InputScalar("pos", ImGuiDataType_U8, &file_.raw[t->absOff], nullptr,
                                   nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal))
                dirty = true;
            ImGui::SameLine();
            ui::SetFieldByte();
            if (file_.raw.size() > t->absOff + 2 &&
                ImGui::InputScalar("cond", ImGuiDataType_U8, &file_.raw[t->absOff + 2], nullptr,
                                   nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal))
                dirty = true;
            uint8_t condLive = (t->absOff + 2 < file_.raw.size()) ? file_.raw[t->absOff + 2] : t->cond;
            if (const char* cn = eventCondName(condLive)) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", cn);
            }
            ImGui::PopID();
        }
        ImNodes::BeginOutputAttribute(kTrigOut(evt));
        ImGui::TextDisabled("fires");
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();

        // ---- event-script node -------------------------------------------
        if (doLayout) ImNodes::SetNodeGridSpacePos(kEvtNode(evt), ImVec2(340, rowY));
        ImNodes::BeginNode(kEvtNode(evt));
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("EVT %02d  script", evt);
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(kEvtIn(evt));
        ImGui::TextDisabled("trigger");
        ImNodes::EndInputAttribute();

        int opCount = 0;
        if (evt >= static_cast<int>(loc.segments.size())) {
            ImNodes::BeginStaticAttribute(kStaticAttr(evt, 0));
            ImGui::TextDisabled("<missing script segment>");
            ImNodes::EndStaticAttribute();
        } else {
            EventSegment& seg = loc.segments[evt];
            if (seg.isText) {
                ImNodes::BeginStaticAttribute(kStaticAttr(evt, 0));
                ImGui::TextDisabled("(plain-text record)");
                std::string preview = seg.text;
                std::replace(preview.begin(), preview.end(), '\n', ' ');
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 300);
                ImGui::TextWrapped("\"%s\"", preview.c_str());
                ImGui::PopTextWrapPos();
                ImNodes::EndStaticAttribute();
            } else if (seg.ops.empty()) {
                ImNodes::BeginStaticAttribute(kStaticAttr(evt, 0));
                ImGui::TextDisabled("(empty)");
                ImNodes::EndStaticAttribute();
            } else {
                opCount = static_cast<int>(seg.ops.size());
                for (int oi = 0; oi < opCount; ++oi) {
                    EventOp& op = seg.ops[oi];

                    // Live argument bytes (reflect in-place edits this frame).
                    std::vector<uint8_t> live;
                    for (size_t k = 0; k < op.args.size(); ++k)
                        live.push_back(file_.argByte(op, static_cast<int>(k)));

                    int strIdx = opcodeStringArg(op.op, live);
                    bool linksString = showStrings_ && strIdx >= 0 &&
                                       strIdx < static_cast<int>(loc.strings.size());

                    if (linksString)
                        ImNodes::BeginOutputAttribute(kOpOut(evt, oi));
                    else
                        ImNodes::BeginStaticAttribute(kStaticAttr(evt, oi));

                    ImGui::PushID(oi);
                    std::string desc = describeOp(op.op, live, strAt, itemAt);
                    ImGui::Text("%02d: %s", oi, desc.c_str());
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("opcode 0x%02X @ file 0x%06zX%s%s", op.op,
                                          op.absOff, op.variable ? "  (token-skip)" : "",
                                          op.truncated ? "  (truncated)" : "");

                    // Editable raw argument bytes.
                    for (size_t k = 0; k < op.args.size(); ++k) {
                        size_t abs = op.absOff + 1 + k;
                        if (abs >= file_.raw.size()) break;
                        ImGui::SameLine();
                        ImGui::PushID(static_cast<int>(k));
                        ui::SetFieldByte();
                        if (ImGui::InputScalar("##b", ImGuiDataType_U8, &file_.raw[abs], nullptr,
                                               nullptr, "%02X",
                                               ImGuiInputTextFlags_CharsHexadecimal))
                            dirty = true;
                        ImGui::PopID();
                    }
                    ImGui::PopID();

                    if (linksString) {
                        stringLinks.push_back({kOpOut(evt, oi), strIdx});
                        usedStrings.insert(strIdx);
                        ImNodes::EndOutputAttribute();
                    } else {
                        ImNodes::EndStaticAttribute();
                    }
                }
            }
        }
        ImNodes::EndNode();

        // Vertical spacing roughly proportional to script size.
        float h = 90.0f + static_cast<float>(std::max<int>(opCount, static_cast<int>(tiles.size()))) * 24.0f;
        yCursor += std::min(h, 340.0f) + 24.0f;
    }

    // ---- string nodes -----------------------------------------------------
    if (showStrings_) {
        for (int s : usedStrings) {
            if (doLayout) ImNodes::SetNodeGridSpacePos(kStrNode(s), ImVec2(760, 16 + s * 78.0f));
            ImNodes::BeginNode(kStrNode(s));
            ImNodes::BeginNodeTitleBar();
            ImGui::Text("str[%d]", s);
            ImNodes::EndNodeTitleBar();
            ImNodes::BeginInputAttribute(kStrIn(s));
            std::string preview = loc.strings[s];
            std::replace(preview.begin(), preview.end(), '\n', ' ');
            if (preview.empty()) preview = "<empty>";
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260);
            ImGui::TextWrapped("%s", preview.c_str());
            ImGui::PopTextWrapPos();
            ImNodes::EndInputAttribute();
            ImNodes::EndNode();
        }
    }

    // ---- links ------------------------------------------------------------
    int linkId = 1;
    for (auto& [evt, tiles] : byEvt) {
        (void)tiles;
        ImNodes::Link(linkId++, kTrigOut(evt), kEvtIn(evt));
    }
    for (const auto& l : stringLinks)
        ImNodes::Link(linkId++, l.outPin, kStrIn(l.strIdx));

    ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
    ImNodes::EndNodeEditor();

    if (doLayout) layoutForLoc_ = selectedLoc_;
}

}  // namespace mm2
