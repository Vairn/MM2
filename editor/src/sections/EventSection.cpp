#include "sections/EventSection.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include "app/App.h"
#include "core/AreaNames.h"
#include "eventlang/Decompile.h"
#include "eventlang/DslEmit.h"
#include "eventlang/DslParse.h"
#include "eventlang/Encode.h"
#include "eventlang/Semantics.h"
#include "imgui.h"
#include "portable-file-dialogs.h"
#include "widgets/UiLayout.h"

namespace mm2 {
namespace {

ImU32 stmtColor(const std::string& kind) {
    if (kind == "say" || kind == "service_title" || kind == "plain_text")
        return IM_COL32(120, 210, 150, 255);
    if (kind == "if") return IM_COL32(240, 180, 90, 255);
    if (kind == "abort" || kind == "end" || kind == "wait") return IM_COL32(255, 130, 130, 255);
    if (kind == "selector" || kind == "shop" || kind == "quest" || kind == "go_to")
        return IM_COL32(140, 180, 255, 255);
    if (kind == "raw_op" || kind == "unlifted") return IM_COL32(170, 170, 170, 255);
    return IM_COL32(200, 200, 210, 255);
}

std::string stmtLabel(const eventlang::Stmt& s) {
    if (s.kind == "say") return "say " + s.getStr("string");
    if (s.kind == "service_title") {
        char b[48];
        std::snprintf(b, sizeof(b), "service_title sign=0x%02X", s.getNum("sign", s.getNum("string")));
        return b;
    }
    if (s.kind == "read_answer" || s.kind == "clear_input") return "read_answer";
    if (s.kind == "wait") return "wait " + s.getStr("kind");
    if (s.kind == "if") {
        const auto& c = s.cond;
        if (c.kind == "gold_at_least") return "if gold >= " + std::to_string(c.getNum("amount"));
        if (c.kind == "yes_no") return "if yes_no";
        if (c.kind == "class_field") {
            char b[48];
            std::snprintf(b, sizeof(b), "if class_field 0x%02X", c.getNum("field"));
            return b;
        }
        if (c.kind == "answer_eq") return "if answer == \"" + c.getStr("text") + "\"";
        return "if " + c.kind;
    }
    if (s.kind == "selector" || s.kind == "shop" || s.kind == "quest") {
        int sel = s.getNum("value");
        if (!sel) sel = s.getNum("selector");
        if (!sel && s.kind != "selector")
            sel = eventlang::selectorByShopOrQuest(s.kind, s.getStr("name"));
        return eventlang::formatSelectorSummary(sel);
    }
    if (s.kind == "go_to") {
        char b[48];
        std::snprintf(b, sizeof(b), "go_to screen %d", s.getNum("screen"));
        return b;
    }
    if (s.kind == "raw_op") {
        char b[32];
        std::snprintf(b, sizeof(b), "@op 0x%02X", s.getNum("op"));
        return b;
    }
    return s.kind.empty() ? "?" : s.kind;
}

const eventlang::Script* findScript(const eventlang::Location& loc, int eventId) {
    for (const auto& sc : loc.scripts)
        if (sc.eventId == eventId) return &sc;
    return nullptr;
}

}  // namespace

bool EventSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    astLoc_ = -1;
    scriptDirty_ = false;
    compileError_.clear();
    compileOkMsg_.clear();
    return loaded;
}

bool EventSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

void EventSection::ensureSelectedEvent() {
    std::set<int> ids;
    for (const auto& t : ast_.triggers) ids.insert(t.eventId);
    for (const auto& sc : ast_.scripts)
        if (!sc.body.empty() || sc.isPlainText) ids.insert(sc.eventId);
    if (ids.empty()) {
        selectedEvent_ = -1;
        return;
    }
    if (selectedEvent_ < 0 || !ids.count(selectedEvent_)) selectedEvent_ = *ids.begin();
}

void EventSection::selectEvent(int eventId) {
    selectedEvent_ = eventId;
    // Prefer the script header over trigger lines that also carry @event N.
    char needle[64];
    std::snprintf(needle, sizeof(needle), "script event_%02d", eventId);
    int line = scriptEditor_.findLine(needle);
    if (line < 0) {
        // Custom script names still end with:  @event N
        const std::string text = scriptEditor_.getText();
        size_t pos = 0;
        int ln = 0;
        const std::string tag = "@event " + std::to_string(eventId);
        while (pos <= text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string_view row(text.data() + pos, end - pos);
            if (row.find("script ") == 0 && row.find(tag) != std::string_view::npos) {
                line = ln;
                break;
            }
            if (end == text.size()) break;
            pos = end + 1;
            ++ln;
        }
    }
    if (line >= 0) scriptEditor_.goToLine(line);
}

void EventSection::refreshAst() {
    if (selectedLoc_ < 0 || selectedLoc_ >= static_cast<int>(file_.locations.size())) return;
    const auto& el = file_.locations[selectedLoc_];
    if (el.offset + el.length > file_.raw.size()) return;
    ast_ = eventlang::decompileLocation(file_.raw.data() + el.offset, el.length, selectedLoc_);
    astLoc_ = selectedLoc_;
    ensureSelectedEvent();
    syncScriptBuffer();
    compileError_.clear();
}

void EventSection::syncScriptBuffer() {
    // Full-file AST so overlay OP_0E calls can expand target scripts as comments.
    const eventlang::EventFileAst full =
        eventlang::decompileEventDat(file_.raw.data(), file_.raw.size());
    scriptBuf_ = eventlang::emitLocation(ast_, eventLocationLabel(selectedLoc_), &full);
    scriptEditor_.setText(scriptBuf_, selectedLoc_);
    scriptDirty_ = false;
}

void EventSection::applyAstToFile(App& app) {
    ast_.modified = true;
    auto rec = eventlang::encodeLocation(ast_);
    if (!file_.replaceLocationRecord(selectedLoc_, rec)) {
        compileError_ = "encode/patch failed";
        compileOkMsg_.clear();
        app.state().status = compileError_;
        return;
    }
    dirty = true;
    refreshAst();
    compileOkMsg_ = "Compiled OK — location " + std::to_string(selectedLoc_) + " written to event.dat";
    app.state().status = compileOkMsg_;
}

bool EventSection::compileFromScript(App& app) {
    scriptBuf_ = scriptEditor_.getText();
    auto parsed = eventlang::parseLocationText(scriptBuf_);
    if (!parsed.ok) {
        compileError_ = parsed.error.empty() ? "parse failed" : parsed.error;
        compileOkMsg_.clear();
        app.state().status = compileError_;
        return false;
    }
    parsed.loc.id = selectedLoc_;
    parsed.loc.modified = true;
    if (parsed.loc.recordKind == eventlang::RecordKind::CastleBlob && parsed.loc.rawBlob.empty() &&
        !ast_.rawBlob.empty())
        parsed.loc.rawBlob = ast_.rawBlob;
    ast_ = std::move(parsed.loc);
    applyAstToFile(app);
    compileError_.clear();
    scriptDirty_ = false;
    return true;
}

void EventSection::exportDsl(App& app) {
    if (astLoc_ != selectedLoc_) refreshAst();
    scriptBuf_ = scriptEditor_.getText();
    auto path = pfd::save_file("Export .mm2evt", "loc_" + std::to_string(selectedLoc_) + ".mm2evt",
                               {"MM2 Event Script", "*.mm2evt", "All", "*"})
                    .result();
    if (path.empty()) return;
    std::ofstream out(path, std::ios::binary);
    out << scriptBuf_;
    app.state().status = "Exported " + path;
}

void EventSection::importDsl(App& app) {
    auto paths =
        pfd::open_file("Import .mm2evt", ".", {"MM2 Event Script", "*.mm2evt", "All", "*"}).result();
    if (paths.empty()) return;
    std::ifstream in(paths[0]);
    if (!in) {
        app.state().status = "Cannot open " + paths[0];
        return;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    scriptBuf_ = ss.str();
    scriptEditor_.setText(scriptBuf_, selectedLoc_);
    scriptDirty_ = true;
    if (compileFromScript(app)) app.state().status = "Imported " + paths[0];
}

void EventSection::drawToolbar(App& app) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));

    ImGui::SetNextItemWidth(ui::Em(16.f));
    if (ImGui::BeginCombo("##loc", eventLocationLabel(selectedLoc_).c_str())) {
        for (int i = 0; i < static_cast<int>(file_.locations.size()); ++i) {
            bool sel = (i == selectedLoc_);
            if (ImGui::Selectable(eventLocationLabel(i).c_str(), sel)) selectedLoc_ = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!scriptDirty_ && compileError_.empty());
    if (ImGui::Button("Compile")) compileFromScript(app);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Parse .mm2evt and write this location back into event.dat");

    ImGui::SameLine();
    ImGui::BeginDisabled(!scriptDirty_);
    if (ImGui::Button("Revert")) {
        syncScriptBuffer();
        compileError_.clear();
        compileOkMsg_.clear();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(ui::Em(0.8f), 0));
    ImGui::SameLine();
    if (ImGui::Button("Export")) exportDsl(app);
    ImGui::SameLine();
    if (ImGui::Button("Import")) importDsl(app);

    ImGui::PopStyleVar();

    ImGui::SameLine();
    const float rightMeta = ui::Em(18.f);
    ui::SameLineRightAlign(rightMeta);
    if (scriptDirty_)
        ImGui::TextColored(ImVec4(1.f, 0.75f, 0.35f, 1.f), "unsaved edits");
    else
        ImGui::TextDisabled("%s", eventlang::recordKindName(ast_.recordKind));
}

void EventSection::drawOutline() {
    ImGui::BeginChild("evt_outline", ImVec2(ui::Em(14.5f), 0), ImGuiChildFlags_Borders);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 4.f));

    ImGui::TextUnformatted("Outline");
    ImGui::Separator();

    // Scripts / events
    if (ImGui::TreeNodeEx("Scripts", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::map<int, int> triggerCount;
        for (const auto& t : ast_.triggers) triggerCount[t.eventId]++;

        std::set<int> shown;
        for (const auto& sc : ast_.scripts) {
            if (outlineFilterScripts_ && sc.body.empty() && !sc.isPlainText) continue;
            shown.insert(sc.eventId);
            char label[80];
            const int nTrig = triggerCount[sc.eventId];
            if (sc.isPlainText)
                std::snprintf(label, sizeof(label), "event_%02d  text", sc.eventId);
            else
                std::snprintf(label, sizeof(label), "event_%02d", sc.eventId);

            const bool sel = (selectedEvent_ == sc.eventId);
            if (ImGui::Selectable(label, sel)) selectEvent(sc.eventId);
            if (nTrig > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("%d tile%s", nTrig, nTrig == 1 ? "" : "s");
            }
        }
        // Triggers pointing at missing scripts
        for (const auto& [eid, n] : triggerCount) {
            if (shown.count(eid)) continue;
            char label[64];
            std::snprintf(label, sizeof(label), "event_%02d  (empty)", eid);
            if (ImGui::Selectable(label, selectedEvent_ == eid)) selectEvent(eid);
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Triggers", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& t : ast_.triggers) {
            char label[96];
            std::snprintf(label, sizeof(label), "(%d,%d) → %02d  %s", t.y, t.x, t.eventId,
                          eventlang::triggerCondName(t.cond));
            if (ImGui::Selectable(label, selectedEvent_ == t.eventId,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                selectEvent(t.eventId);
            }
        }
        if (ast_.triggers.empty()) ImGui::TextDisabled("(none)");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Strings")) {
        for (const auto& s : ast_.strings) {
            char label[64];
            std::snprintf(label, sizeof(label), "%s", s.name.c_str());
            if (ImGui::Selectable(label)) {
                int line = scriptEditor_.findLine(s.name + ":");
                if (line >= 0) scriptEditor_.goToLine(line);
            }
            if (ImGui::IsItemHovered() && !s.text.empty()) {
                ImGui::BeginTooltip();
                std::string preview = s.text;
                if (preview.size() > 120) preview = preview.substr(0, 117) + "...";
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.f);
                ImGui::TextUnformatted(preview.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        if (ast_.strings.empty()) ImGui::TextDisabled("(none)");
        ImGui::TreePop();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void EventSection::drawEditor() {
    ImGui::BeginChild("evt_editor", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::TextDisabled(".mm2evt");
    ImGui::SameLine();
    ImGui::TextDisabled("· %s · %zu scripts · %zu strings",
                        eventLocationLabel(selectedLoc_).c_str(), ast_.scripts.size(),
                        ast_.strings.size());
    ImGui::Separator();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    if (scriptEditor_.draw("##mm2evt_editor", size)) {
        scriptBuf_ = scriptEditor_.getText();
        scriptDirty_ = true;
        compileOkMsg_.clear();
    }
    ImGui::EndChild();
}

void EventSection::drawStmtTree(const std::vector<eventlang::Stmt>& stmts, int depth) {
    for (size_t i = 0; i < stmts.size(); ++i) {
        const auto& st = stmts[i];
        ImGui::PushID(static_cast<int>(i) + depth * 1000);
        const std::string label = stmtLabel(st);
        ImGui::PushStyleColor(ImGuiCol_Text, stmtColor(st.kind));

        if (st.kind == "if") {
            const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor();
            if (open) {
                if (!st.thenBody.empty()) {
                    ImGui::TextDisabled("then");
                    drawStmtTree(st.thenBody, depth + 1);
                }
                if (!st.elseBody.empty()) {
                    ImGui::TextDisabled("else");
                    drawStmtTree(st.elseBody, depth + 1);
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::BulletText("%s", label.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
}

void EventSection::drawInspector() {
    ImGui::BeginChild("evt_inspector", ImVec2(ui::Em(16.f), 0), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();

    if (selectedEvent_ < 0) {
        ImGui::TextDisabled("Select a script in the outline.");
        ImGui::EndChild();
        return;
    }

    ImGui::Text("event_%02d", selectedEvent_);
    if (ImGui::SmallButton("Jump to script")) selectEvent(selectedEvent_);

    ImGui::Spacing();
    ImGui::TextDisabled("Triggers");
    int n = 0;
    for (const auto& t : ast_.triggers) {
        if (t.eventId != selectedEvent_) continue;
        ImGui::BulletText("(%d,%d)  %s", t.y, t.x, eventlang::triggerCondName(t.cond));
        ++n;
    }
    if (n == 0) ImGui::TextDisabled("  (no tiles)");

    ImGui::Spacing();
    ImGui::TextDisabled("Flow");
    const eventlang::Script* sc = findScript(ast_, selectedEvent_);
    if (!sc) {
        ImGui::TextDisabled("  (no script body)");
    } else if (sc->isPlainText) {
        ImGui::TextWrapped("%s", sc->body.empty() ? "(empty text)" : sc->body[0].getStr("text").c_str());
    } else if (sc->body.empty()) {
        ImGui::TextDisabled("  (empty)");
    } else {
        ImGui::BeginChild("flow_tree", ImVec2(0, 0), ImGuiChildFlags_None);
        drawStmtTree(sc->body, 0);
        ImGui::EndChild();
    }

    ImGui::EndChild();
}

void EventSection::drawProblems() {
    if (compileError_.empty() && compileOkMsg_.empty()) return;
    ImGui::Separator();
    if (!compileError_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 16, 16, 255));
        ImGui::BeginChild("problems", ImVec2(0, ui::Em(3.2f)), ImGuiChildFlags_Borders);
        ImGui::TextColored(ImVec4(1.f, 0.45f, 0.45f, 1.f), "Compile error");
        ImGui::TextWrapped("%s", compileError_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else if (!compileOkMsg_.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.55f, 1.f), "%s", compileOkMsg_.c_str());
    }
}

void EventSection::draw(App& app) {
    if (!loaded) {
        ImGui::TextDisabled("event.dat not loaded.");
        return;
    }
    if (file_.locations.empty()) {
        ImGui::TextDisabled("No locations decoded.");
        return;
    }

    selectedLoc_ = std::clamp(selectedLoc_, 0, static_cast<int>(file_.locations.size()) - 1);
    if (astLoc_ != selectedLoc_) refreshAst();

    drawToolbar(app);
    if (ast_.recordKind == eventlang::RecordKind::CastleBlob) {
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f),
                           "Castle-style record (no triplet terminator) — edit carefully.");
    } else if (ast_.recordKind == eventlang::RecordKind::OverlayBank) {
        ImGui::TextDisabled(
            "Overlay bank (locs 60..70): LE string anchor @ [0..1], scripts @ [2..] — matches ASM 0x176B6.");
    }
    drawProblems();
    ImGui::Separator();

    // Toolkit body: outline | editor | inspector
    const float problemsH =
        (!compileError_.empty()) ? ui::Em(3.6f) : (!compileOkMsg_.empty() ? ui::Em(1.6f) : 0.f);
    (void)problemsH;

    drawOutline();
    ImGui::SameLine(0, ui::PanelGap());

    // Center editor takes remaining width minus inspector
    const float inspectorW = ui::Em(16.f);
    const float gap = ui::PanelGap();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float editorW = std::max(ui::Em(20.f), avail - inspectorW - gap);

    ImGui::BeginChild("evt_editor_host", ImVec2(editorW, 0), ImGuiChildFlags_None);
    drawEditor();
    ImGui::EndChild();

    ImGui::SameLine(0, gap);
    drawInspector();
}

}  // namespace mm2
