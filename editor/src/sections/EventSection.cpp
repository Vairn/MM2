#include "sections/EventSection.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "app/App.h"
#include "core/AreaNames.h"
#include "core/EventOps.h"
#include "imgui.h"
#include "imnodes.h"
#include "portable-file-dialogs.h"
#include "widgets/UiLayout.h"

namespace mm2 {

namespace {

struct NodeColors {
    ImU32 title;
    ImU32 bg;
    ImU32 outline;
};

void pushNodeStyle(const NodeColors& c) {
    ImNodes::PushColorStyle(ImNodesCol_TitleBar, c.title);
    ImNodes::PushColorStyle(ImNodesCol_NodeBackground, c.bg);
    ImNodes::PushColorStyle(ImNodesCol_NodeOutline, c.outline);
}

void popNodeStyle() {
    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

void applyEditorMouseWheelZoom() {
    if (!ImNodes::IsEditorHovered()) return;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel == 0.0f) return;

    constexpr float kStep = 0.1f;
    constexpr float kMinZoom = 0.25f;
    constexpr float kMaxZoom = 4.0f;
    const float zoom =
        std::clamp(ImNodes::EditorContextGetZoom() + wheel * kStep, kMinZoom, kMaxZoom);
    ImNodes::EditorContextSetZoom(zoom, ImGui::GetMousePos());
}

constexpr NodeColors kTrigColors{
    IM_COL32(220, 160, 64, 255), IM_COL32(52, 42, 28, 255), IM_COL32(190, 130, 50, 255)};
constexpr NodeColors kScriptColors{
    IM_COL32(80, 150, 220, 255), IM_COL32(28, 40, 56, 255), IM_COL32(60, 120, 190, 255)};
constexpr NodeColors kTextScriptColors{
    IM_COL32(170, 120, 220, 255), IM_COL32(40, 32, 52, 255), IM_COL32(140, 90, 190, 255)};
constexpr NodeColors kEmptyScriptColors{
    IM_COL32(120, 120, 120, 255), IM_COL32(36, 36, 36, 255), IM_COL32(90, 90, 90, 255)};
constexpr NodeColors kStringColors{
    IM_COL32(80, 190, 120, 255), IM_COL32(28, 48, 36, 255), IM_COL32(60, 160, 100, 255)};

enum class ScriptKind { Missing, Empty, Text, Bytecode };

ScriptKind scriptKind(int evt, const EventLocation& loc) {
    if (evt >= static_cast<int>(loc.segments.size())) return ScriptKind::Missing;
    const EventSegment& seg = loc.segments[evt];
    if (seg.isText) return ScriptKind::Text;
    if (seg.ops.empty()) return ScriptKind::Empty;
    return ScriptKind::Bytecode;
}

const NodeColors& scriptColors(ScriptKind kind) {
    switch (kind) {
        case ScriptKind::Text: return kTextScriptColors;
        case ScriptKind::Empty:
        case ScriptKind::Missing: return kEmptyScriptColors;
        default: return kScriptColors;
    }
}

}  // namespace

// ---- unique id spaces (node / pin / static attribute) ----------------------
static inline int kTrigNode(int e)   { return 0x10000 + e; }
static inline int kEvtNode(int e)    { return 0x20000 + e; }
static inline int kStrNode(int s)    { return 0x30000 + s; }
static inline int kTrigOut(int e)    { return 0x40000 + e; }
static inline int kEvtIn(int e)      { return 0x50000 + e; }
static inline int kOpOut(int e, int i)  { return 0x60000 + e * 256 + i; }
static inline int kStrIn(int s)      { return 0x70000 + s; }
static inline int kStaticAttr(int e, int i) { return 0x80000 + e * 256 + i; }

static std::string blockColorForLine(const std::string& line) {
    if (line.find("if ") == 0 || line.find("else") == 0) return "branch";
    if (line.find("say") == 0) return "say";
    if (line.find("quest ") == 0 || line.find("shop ") == 0) return "action";
    if (line == "abort" || line == "end") return "flow";
    return "other";
}

void EventSection::refreshDslCache(App& app) {
    dslScriptBlocks_.clear();
    dslCacheLoc_ = selectedLoc_;
    if (app.state().dataDir.empty()) return;

    const std::string tmpPath = app.state().dataDir + "/.mm2ed_dsl_cache.mm2evt";
    const std::string toolsDir = app.state().dataDir + "/tools";
    const std::string eventPath = app.state().dataDir + "/event.dat";

    std::ostringstream cmd;
#ifdef _WIN32
    cmd << "cd /d \"" << toolsDir << "\" && python -m mm2_event_lang decompile -l "
        << selectedLoc_ << " --output \"" << tmpPath << "\" \"" << eventPath << "\"";
#else
    cmd << "cd \"" << toolsDir << "\" && python -m mm2_event_lang decompile -l "
        << selectedLoc_ << " --output \"" << tmpPath << "\" \"" << eventPath << "\"";
#endif
    if (std::system(cmd.str().c_str()) != 0) return;

    std::ifstream in(tmpPath);
    if (!in) return;
    std::string line;
    int currentEvt = -1;
    while (std::getline(in, line)) {
        if (line.rfind("script ", 0) == 0) {
            auto at = line.find("@event ");
            if (at != std::string::npos)
                currentEvt = std::stoi(line.substr(at + 7));
            else
                currentEvt = -1;
            continue;
        }
        if (currentEvt < 0) continue;
        if (line.empty()) continue;
        if (line[0] == ' ') {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(' '));
            if (!trimmed.empty()) dslScriptBlocks_[currentEvt].push_back(trimmed);
        }
    }
}

void EventSection::exportDsl(App& app) {
    if (app.state().dataDir.empty()) return;
    auto path = pfd::save_file("Export location as .mm2evt", "loc_" + std::to_string(selectedLoc_) + ".mm2evt",
                               {"MM2 Event Script", "*.mm2evt", "All", "*"})
                      .result();
    if (path.empty()) return;

    const std::string toolsDir = app.state().dataDir + "/tools";
    const std::string eventPath = app.state().dataDir + "/event.dat";
    std::ostringstream cmd;
#ifdef _WIN32
    cmd << "cd /d \"" << toolsDir << "\" && python -m mm2_event_lang decompile -l "
        << selectedLoc_ << " --output \"" << path << "\" \"" << eventPath << "\"";
#else
    cmd << "cd \"" << toolsDir << "\" && python -m mm2_event_lang decompile -l "
        << selectedLoc_ << " --output \"" << path << "\" \"" << eventPath << "\"";
#endif
    if (std::system(cmd.str().c_str()) == 0)
        app.state().status = "Exported " + path;
    else
        app.state().status = "DSL export failed (is Python on PATH?)";
}

void EventSection::importDsl(App& app) {
    if (app.state().dataDir.empty()) return;
    auto paths = pfd::open_file("Import .mm2evt location", ".",
                                {"MM2 Event Script", "*.mm2evt", "All Files", "*"})
                       .result();
    if (paths.empty()) return;
    const std::string path = paths[0];

    const std::string toolsDir = app.state().dataDir + "/tools";
    const std::string eventPath = app.state().dataDir + "/event.dat";
    const std::string outPath = app.state().dataDir + "/event.dat";
    std::ostringstream cmd;
#ifdef _WIN32
    cmd << "cd /d \"" << toolsDir << "\" && python -m mm2_event_lang patch-loc -l "
        << selectedLoc_ << " \"" << path << "\" --original \"" << eventPath << "\" -o \""
        << outPath << "\"";
#else
    cmd << "cd \"" << toolsDir << "\" && python -m mm2_event_lang patch-loc -l "
        << selectedLoc_ << " \"" << path << "\" --original \"" << eventPath << "\" -o \""
        << outPath << "\"";
#endif
    if (std::system(cmd.str().c_str()) == 0) {
        load(app.state().dataDir);
        dslCacheLoc_ = -1;
        layoutForLoc_ = -1;
        app.state().status = "Imported " + path + " -> event.dat";
    } else {
        app.state().status = "DSL import failed (is Python on PATH?)";
    }
}

void EventSection::drawBlockScript(int evt, const std::vector<std::string>& lines) {
    ImNodes::BeginStaticAttribute(kStaticAttr(evt, 9000));
    if (lines.empty()) {
        ImGui::TextDisabled("(no lifted script — toggle off Block view for hex)");
    } else {
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string& ln = lines[i];
            const char* cat = blockColorForLine(ln).c_str();
            ImU32 col = IM_COL32(200, 200, 200, 255);
            if (std::string(cat) == "say") col = IM_COL32(120, 220, 160, 255);
            else if (std::string(cat) == "branch") col = IM_COL32(240, 180, 90, 255);
            else if (std::string(cat) == "action") col = IM_COL32(140, 180, 255, 255);
            else if (std::string(cat) == "flow") col = IM_COL32(255, 120, 120, 255);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
            ImGui::TextUnformatted(ln.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
    }
    ImNodes::EndStaticAttribute();
}

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
    return eventLocationLabel(id);
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
    float btnExportW = ImGui::CalcTextSize("Export .mm2evt").x + st.FramePadding.x * 2.f;
    float btnImportW = ImGui::CalcTextSize("Import .mm2evt").x + st.FramePadding.x * 2.f;
    float btnRelayoutW = ImGui::CalcTextSize("Relayout").x + st.FramePadding.x * 2.f;
    float blockW = ImGui::GetFrameHeight() + st.ItemInnerSpacing.x +
                   ImGui::CalcTextSize("Block view").x;
    ui::SameLineRightAlign(cbW + blockW + st.ItemSpacing.x * 4 + btnExportW + btnImportW +
                           btnRelayoutW);
    ImGui::Checkbox("Block view", &blockView_);
    if (blockView_ && dslCacheLoc_ != selectedLoc_) refreshDslCache(app);
    ImGui::SameLine();
    ImGui::Checkbox("String nodes", &showStrings_);
    ImGui::SameLine();
    if (ImGui::SmallButton("Export .mm2evt")) exportDsl(app);
    ImGui::SameLine();
    if (ImGui::SmallButton("Import .mm2evt")) importDsl(app);
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
        "Drag nodes to arrange. Hex fields patch in place; use Export/Import .mm2evt "
        "for structural edits. Block view shows readable script statements.");
    ImGui::TextDisabled("Colors:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kTrigColors.title);
    ImGui::TextUnformatted("triggers");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kScriptColors.title);
    ImGui::TextUnformatted("bytecode");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextScriptColors.title);
    ImGui::TextUnformatted("text");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kEmptyScriptColors.title);
    ImGui::TextUnformatted("empty");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kStringColors.title);
    ImGui::TextUnformatted("strings");
    ImGui::PopStyleColor();
    ImGui::Separator();

    drawGraph(app, loc);
}

void EventSection::drawGraph(App& app, EventLocation& loc) {
    const bool doLayout = (layoutForLoc_ != selectedLoc_);

    constexpr float kMargin = 32.0f;
    constexpr float kColGap = 96.0f;
    constexpr float kRowPad = 72.0f;
    constexpr float kStrGap = 36.0f;

    auto strAt = [&](int idx) -> std::string {
        if (idx >= 0 && idx < static_cast<int>(loc.strings.size())) return loc.strings[idx];
        return std::string();
    };
    auto itemAt = [&](int id) -> std::string { return app.itemName(id); };

    auto drawStringNode = [&](int s) {
        pushNodeStyle(kStringColors);
        ImNodes::BeginNode(kStrNode(s));
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("str[%d]", s);
        ImNodes::EndNodeTitleBar();
        ImNodes::BeginInputAttribute(kStrIn(s));
        std::string preview = loc.strings[s];
        std::replace(preview.begin(), preview.end(), '\n', ' ');
        if (preview.empty()) preview = "<empty>";
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 300.0f);
        ImGui::TextWrapped("%s", preview.c_str());
        ImGui::PopTextWrapPos();
        ImNodes::EndInputAttribute();
        ImNodes::EndNode();
        popNodeStyle();
    };

    // Group triggers by event id (matches decode_event dump_decompiled order).
    std::map<int, std::vector<const EventTriplet*>> byEvt;
    for (const auto& t : loc.triplets) byEvt[t.event].push_back(&t);

    ImNodes::BeginNodeEditor();

    struct StrLink { int outPin; int strIdx; };
    std::vector<StrLink> stringLinks;
    std::set<int> placedStrings;

    float yCursor = kMargin;

    for (auto& [evt, tiles] : byEvt) {
        const float rowY = yCursor;
        std::vector<int> rowStrings;
        const ScriptKind kind = scriptKind(evt, loc);

        // ---- trigger node -------------------------------------------------
        if (doLayout) ImNodes::SetNodeGridSpacePos(kTrigNode(evt), ImVec2(kMargin, rowY));
        pushNodeStyle(kTrigColors);
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
        popNodeStyle();
        const ImVec2 trigDim =
            doLayout ? ImNodes::GetNodeDimensions(kTrigNode(evt)) : ImVec2(0, 0);

        // ---- event-script node -------------------------------------------
        const float xScript = doLayout ? (kMargin + trigDim.x + kColGap) : 0.0f;
        if (doLayout) ImNodes::SetNodeGridSpacePos(kEvtNode(evt), ImVec2(xScript, rowY));
        pushNodeStyle(scriptColors(kind));
        ImNodes::BeginNode(kEvtNode(evt));
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("EVT %02d  script", evt);
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(kEvtIn(evt));
        ImGui::TextDisabled("trigger");
        ImNodes::EndInputAttribute();

        if (evt >= static_cast<int>(loc.segments.size())) {
            ImNodes::BeginStaticAttribute(kStaticAttr(evt, 0));
            ImGui::TextDisabled("<missing script segment>");
            ImNodes::EndStaticAttribute();
        } else {
            EventSegment& seg = loc.segments[evt];
            if (kind == ScriptKind::Text) {
                ImNodes::BeginStaticAttribute(kStaticAttr(evt, 0));
                ImGui::TextDisabled("(plain-text record)");
                std::string preview = seg.text;
                std::replace(preview.begin(), preview.end(), '\n', ' ');
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 300);
                ImGui::TextWrapped("\"%s\"", preview.c_str());
                ImGui::PopTextWrapPos();
                ImNodes::EndStaticAttribute();
            } else if (kind == ScriptKind::Empty) {
                ImNodes::BeginStaticAttribute(kStaticAttr(evt, 0));
                ImGui::TextDisabled("(empty)");
                ImNodes::EndStaticAttribute();
            } else if (blockView_) {
                auto it = dslScriptBlocks_.find(evt);
                drawBlockScript(evt, it != dslScriptBlocks_.end() ? it->second : std::vector<std::string>{});
            } else {
                const int opCount = static_cast<int>(seg.ops.size());
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
                    std::string desc = describeOp(op.op, live, strAt, itemAt, loc.id);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
                    ImGui::Text("%02d: %s", oi, desc.c_str());
                    ImGui::PopTextWrapPos();
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
                        if (!placedStrings.count(strIdx) &&
                            std::find(rowStrings.begin(), rowStrings.end(), strIdx) == rowStrings.end())
                            rowStrings.push_back(strIdx);
                        ImNodes::EndOutputAttribute();
                    } else {
                        ImNodes::EndStaticAttribute();
                    }
                }
            }
        }
        ImNodes::EndNode();
        popNodeStyle();
        const ImVec2 scriptDim =
            doLayout ? ImNodes::GetNodeDimensions(kEvtNode(evt)) : ImVec2(0, 0);

        // ---- string nodes for this row (first use only) -------------------
        const float xString = doLayout ? (xScript + scriptDim.x + kColGap) : 0.0f;
        float strBandH = 0.0f;
        if (showStrings_ && !rowStrings.empty()) {
            float strY = rowY;
            for (int s : rowStrings) {
                if (doLayout)
                    ImNodes::SetNodeGridSpacePos(kStrNode(s), ImVec2(xString, strY));
                drawStringNode(s);
                placedStrings.insert(s);
                if (doLayout) {
                    strY += ImNodes::GetNodeDimensions(kStrNode(s)).y + kStrGap;
                    strBandH = strY - rowY;
                }
            }
        }

        if (doLayout) {
            const float rowH =
                std::max(trigDim.y, std::max(scriptDim.y, strBandH)) + kRowPad;
            yCursor += rowH;
        }
    }

    // Render string nodes referenced only after the last event row (rare).
    if (showStrings_) {
        for (const auto& l : stringLinks) {
            if (placedStrings.count(l.strIdx)) continue;
            if (doLayout)
                ImNodes::SetNodeGridSpacePos(kStrNode(l.strIdx), ImVec2(kMargin, yCursor));
            drawStringNode(l.strIdx);
            placedStrings.insert(l.strIdx);
            if (doLayout)
                yCursor += ImNodes::GetNodeDimensions(kStrNode(l.strIdx)).y + kRowPad;
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
    applyEditorMouseWheelZoom();

    if (doLayout) layoutForLoc_ = selectedLoc_;
}

}  // namespace mm2
