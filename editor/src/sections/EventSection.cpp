#include "sections/EventSection.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
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
#include "widgets/UiTheme.h"

namespace mm2 {
namespace {

ImU32 stmtColor(const std::string& kind) {
    if (kind == "say" || kind == "service_title" || kind == "plain_text" || kind == "ask_yes_no")
        return ui::ToU32(ImVec4(0.42f, 0.78f, 0.55f, 1.00f));
    if (kind == "if" || kind == "set_cond" || kind == "skip_if_true" || kind == "skip_if_false" ||
        kind == "skip_if_victory")
        return ui::ToU32(ui::Warn());
    if (kind == "abort" || kind == "end" || kind == "wait" || kind == "clear_tile_event")
        return ui::ToU32(ui::Danger());
    if (kind == "selector" || kind == "shop" || kind == "quest" || kind == "go_to" ||
        kind == "overlay")
        return IM_COL32(130, 165, 230, 255);
    if (kind == "give_item" || kind == "set_party_bits" || kind == "apply_party" ||
        kind == "party_effect" || kind == "party_damage" || kind == "treasure" ||
        kind == "or_member_field")
        return IM_COL32(175, 145, 230, 255);
    if (kind == "fight" || kind == "fight_b" || kind == "play_sound" || kind == "engine_call")
        return IM_COL32(235, 145, 100, 255);
    if (kind == "raw_op" || kind == "unlifted" || kind == "unknown_line")
        return ui::ToU32(ui::Muted());
    return IM_COL32(190, 190, 200, 255);
}

std::string formatExprShort(const eventlang::Expr& c) {
    if (c.kind == "gold_at_least") return "gold >= " + std::to_string(c.getNum("amount"));
    if (c.kind == "gems_at_least" || c.kind == "code16")
        return "gems >= " + std::to_string(c.getNum("amount", c.getNum("value")));
    if (c.kind == "yes_no") return c.getNum("mode") ? "yes_no mode=1" : "yes_no";
    if (c.kind == "class_field" || c.kind == "member_attr") {
        char b[48];
        std::snprintf(b, sizeof(b), "member_attr 0x%02X", c.getNum("arg1", c.getNum("field")));
        return b;
    }
    if (c.kind == "answer_eq") return "answer == \"" + c.getStr("text") + "\"";
    if (c.kind == "combat_victory") return "combat_victory";
    if (c.kind == "prior_cond") return "prior_cond";
    if (c.kind == "consume_item" || c.kind == "has_item_id") {
        char b[40];
        std::snprintf(b, sizeof(b), "consume_item 0x%02X", c.getNum("item"));
        return b;
    }
    if (c.kind == "party_has_item") {
        char b[40];
        std::snprintf(b, sizeof(b), "party_has_item 0x%02X", c.getNum("item", c.getNum("b")));
        return b;
    }
    if (c.kind == "party_bits") {
        const char* fname = eventlang::partyFieldName(c.getNum("field"));
        char b[72];
        if (fname)
            std::snprintf(b, sizeof(b), "party_bits %s mask=0x%02X", fname, c.getNum("mask"));
        else
            std::snprintf(b, sizeof(b), "party_bits field=0x%02X mask=0x%02X", c.getNum("field"),
                          c.getNum("mask"));
        return b;
    }
    if (c.kind == "give_item_ok") {
        char b[48];
        std::snprintf(b, sizeof(b), "give_item_ok 0x%02X", c.getNum("item"));
        return b;
    }
    if (c.kind.empty()) return "?";
    return c.kind;
}

std::string stmtLabel(const eventlang::Stmt& s) {
    if (s.kind == "say") {
        const std::string v = s.getStr("variant");
        std::string verb = "say";
        if (v == "door") verb = "say_door";
        else if (v == "block") verb = "say_block";
        else if (v == "popup_a") verb = "say_popup_a";
        else if (v == "popup_b") verb = "say_popup_b";
        else if (v == "basic") verb = "say_basic";
        return verb + " " + s.getStr("string");
    }
    if (s.kind == "service_title") {
        char b[48];
        std::snprintf(b, sizeof(b), "service_title sign=0x%02X", s.getNum("sign", s.getNum("string")));
        return b;
    }
    if (s.kind == "read_answer" || s.kind == "clear_input") return "read_answer";
    if (s.kind == "wait") {
        if (s.getNum("mode") || s.getStr("kind") == "key") return "wait space mode=1";
        return "wait " + s.getStr("kind");
    }
    if (s.kind == "ask_yes_no") return s.getNum("mode") ? "ask yes_no mode=1" : "ask yes_no";
    if (s.kind == "set_cond") return "set_cond " + formatExprShort(s.cond);
    if (s.kind == "skip_if_true") return "skip_if_true " + std::to_string(s.getNum("n"));
    if (s.kind == "skip_if_false") return "skip_if_false " + std::to_string(s.getNum("n"));
    if (s.kind == "skip_if_victory") return "skip_if_victory " + std::to_string(s.getNum("n"));
    if (s.kind == "if") return "if " + formatExprShort(s.cond);
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
    if (s.kind == "give_item") {
        char b[48];
        std::snprintf(b, sizeof(b), "give_item 0x%02X", s.getNum("item"));
        return b;
    }
    if (s.kind == "set_party_bits" || s.kind == "apply_party_masked") {
        const int field = s.getNum("field", s.getNum("set"));
        const char* fname = eventlang::partyFieldName(field);
        if (fname) return std::string("set_party_bits ") + fname;
        char b[40];
        std::snprintf(b, sizeof(b), "set_party_bits field=0x%02X", field);
        return b;
    }
    if (s.kind == "apply_party") {
        const int field = s.getNum("op", s.getNum("field"));
        const char* fname = eventlang::partyFieldName(field);
        if (fname) return std::string("party_bits ") + fname;
        return "party_bits";
    }
    if (s.kind == "party_effect") {
        char b[40];
        std::snprintf(b, sizeof(b), "party_effect sel=0x%02X", s.getNum("sel"));
        return b;
    }
    if (s.kind == "fight") return "fight";
    if (s.kind == "fight_b") return "fight_b";
    if (s.kind == "play_sound" || s.kind == "engine_call") {
        char b[32];
        std::snprintf(b, sizeof(b), "play_sound %d", s.getNum("id", s.getNum("code")));
        return b;
    }
    if (s.kind == "set_tile") {
        char b[40];
        std::snprintf(b, sizeof(b), "set_tile (%d,%d)", s.getNum("y"), s.getNum("x"));
        return b;
    }
    if (s.kind == "delay") return "delay " + std::to_string(s.getNum("ticks"));
    if (s.kind == "select_member") return "select_member";
    if (s.kind == "set_quest_complete") return "set quest_complete";
    if (s.kind == "set_quest_flag") return "set quest_flag " + s.getStr("name");
    if (s.kind == "clear_tile_event") return "clear_tile_event";
    if (s.kind == "raw_op") {
        char b[32];
        std::snprintf(b, sizeof(b), "@op 0x%02X", s.getNum("op"));
        return b;
    }
    if (s.kind == "unknown_line") return s.getStr("text");
    if (s.kind == "plain_text") return "plain_text";
    return s.kind.empty() ? "?" : s.kind;
}

/** Short outline hint from the first interesting statement. */
std::string scriptOutlineHint(const eventlang::Script& sc) {
    if (sc.isPlainText) return "text";
    for (const auto& st : sc.body) {
        if (st.kind == "say") {
            const std::string ref = st.getStr("string");
            return ref.empty() ? "say" : ref;
        }
        if (st.kind == "shop" || st.kind == "quest" || st.kind == "selector") {
            int sel = st.getNum("value");
            if (!sel) sel = st.getNum("selector");
            if (!sel && st.kind != "selector")
                sel = eventlang::selectorByShopOrQuest(st.kind, st.getStr("name"));
            return eventlang::formatSelectorSummary(sel);
        }
        if (st.kind == "service_title") return "signboard";
        if (st.kind == "go_to") {
            char b[32];
            std::snprintf(b, sizeof(b), "→ screen %d", st.getNum("screen"));
            return b;
        }
        if (st.kind == "if" || st.kind == "set_cond" || st.kind == "skip_if_true" ||
            st.kind == "skip_if_false")
            return stmtLabel(st);
        if (st.kind == "fight" || st.kind == "fight_b") return "combat";
    }
    if (sc.body.empty()) return {};
    return stmtLabel(sc.body[0]);
}

const eventlang::Script* findScript(const eventlang::Location& loc, int eventId) {
    for (const auto& sc : loc.scripts)
        if (sc.eventId == eventId) return &sc;
    return nullptr;
}

/** Resolve OP_0E overlay bank target from a selector/shop/quest stmt. */
std::optional<std::pair<int, int>> overlayTargetFromStmt(const eventlang::Stmt& s) {
    if (s.kind != "selector" && s.kind != "shop" && s.kind != "quest" && s.kind != "overlay")
        return std::nullopt;
    int sel = s.getNum("value");
    if (!sel) sel = s.getNum("selector");
    if (!sel && s.kind != "selector" && s.kind != "overlay")
        sel = eventlang::selectorByShopOrQuest(s.kind, s.getStr("name"));
    if (!sel && s.kind == "overlay") {
        const int loc = s.getNum("loc", s.getNum("overlay"));
        const int ev = s.getNum("event");
        if (loc > 0 && ev >= 0) return std::make_pair(loc, ev);
    }
    return eventlang::binExecSelector(sel);
}

/** First overlay target in a script body (walks nested ifs). */
std::optional<std::pair<int, int>> overlayTargetFromScript(const eventlang::Script& sc) {
    std::function<std::optional<std::pair<int, int>>(const std::vector<eventlang::Stmt>&)> walk;
    walk = [&](const std::vector<eventlang::Stmt>& stmts) -> std::optional<std::pair<int, int>> {
        for (const auto& st : stmts) {
            if (auto t = overlayTargetFromStmt(st)) return t;
            if (st.kind == "if") {
                if (auto t = walk(st.thenBody)) return t;
                if (auto t = walk(st.elseBody)) return t;
            }
        }
        return std::nullopt;
    };
    return walk(sc.body);
}

/** Parse `overlay N event_MM` from a .mm2evt line (ignores leading space / trailing comment). */
std::optional<std::pair<int, int>> parseOverlayLine(std::string_view line) {
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.remove_prefix(1);
    if (line.size() < 10 || line.substr(0, 8) != "overlay ") return std::nullopt;
    line.remove_prefix(8);
    int loc = 0, ev = 0;
    size_t i = 0;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
        loc = loc * 10 + (line[i] - '0');
        ++i;
    }
    if (i == 0) return std::nullopt;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    constexpr std::string_view kEv = "event_";
    if (line.substr(i, kEv.size()) != kEv) return std::nullopt;
    i += kEv.size();
    size_t start = i;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
    if (i == start) return std::nullopt;
    for (size_t j = start; j < i; ++j) ev = ev * 10 + (line[j] - '0');
    return std::make_pair(loc, ev);
}

/** True when the script is essentially a single overlay call (+ optional clear_tile_event). */
bool scriptIsOverlayOnly(const eventlang::Script& sc) {
    int overlayN = 0;
    for (const auto& st : sc.body) {
        if (st.kind == "clear_tile_event" || st.kind == "end" || st.kind == "abort") continue;
        if (overlayTargetFromStmt(st)) {
            ++overlayN;
            continue;
        }
        return false;
    }
    return overlayN == 1;
}

}  // namespace

bool EventSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    astLoc_ = -1;
    scriptDirty_ = false;
    compileError_.clear();
    compileOkMsg_.clear();
    compileErrorLine_ = -1;
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

void EventSection::openOverlay(int locId, int eventId) {
    if (locId < 0 || locId >= static_cast<int>(file_.locations.size())) return;
    if (scriptDirty_) scriptDirty_ = false;
    selectedLoc_ = locId;
    selectedEvent_ = eventId;
    refreshAst();
    selectEvent(eventId);
}

void EventSection::tryOpenOverlayFromEditorClick() {
    // Ctrl+click on an `overlay N event_MM` line → jump to that bank script.
    if (!ImGui::IsItemHovered()) return;
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) return;
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        return;
    int line = 0, col = 0;
    scriptEditor_.raw().GetCurrentCursor(line, col);
    const std::string row = scriptEditor_.raw().GetLineText(line);
    if (auto t = parseOverlayLine(row)) openOverlay(t->first, t->second);
}

void EventSection::jumpToError() {
    if (compileErrorLine_ >= 0) scriptEditor_.goToLine(compileErrorLine_);
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
    compileErrorLine_ = -1;
    scriptEditor_.clearErrorMarkers();
}

void EventSection::syncScriptBuffer() {
    // Full-file AST so overlay OP_0E lines can show a short target-string hint.
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
        compileErrorLine_ = -1;
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
        compileErrorLine_ = parsed.errorLine;
        compileOkMsg_.clear();
        scriptEditor_.markErrorLine(compileErrorLine_, compileError_.c_str());
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
    compileErrorLine_ = -1;
    scriptEditor_.clearErrorMarkers();
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
    const ImGuiStyle& st = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(st.FramePadding.x, st.FramePadding.y));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(st.ItemSpacing.x * 0.85f, st.ItemSpacing.y * 0.85f));

    ui::BeginToolbarRow();

    // Location picker
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Loc");
    ImGui::SameLine(0, ui::Em(0.25f));
    ImGui::SetNextItemWidth(ui::Em(18.f));
    const int prevLoc = selectedLoc_;
    if (ImGui::BeginCombo("##loc", eventLocationLabel(selectedLoc_).c_str())) {
        for (int i = 0; i < static_cast<int>(file_.locations.size()); ++i) {
            bool sel = (i == selectedLoc_);
            if (ImGui::Selectable(eventLocationLabel(i).c_str(), sel)) selectedLoc_ = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (selectedLoc_ != prevLoc && scriptDirty_) {
        // Switching location discards unsaved buffer; refreshAst handles it via draw().
        scriptDirty_ = false;
    }

    ImGui::SameLine(0, ui::Em(0.6f));
    if (ImGui::Button("Reload")) {
        refreshAst();
        compileOkMsg_.clear();
        app.state().status = "Reloaded location " + std::to_string(selectedLoc_);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-decompile this location from event.dat");

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
        compileErrorLine_ = -1;
        compileOkMsg_.clear();
        scriptEditor_.clearErrorMarkers();
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0, ui::Em(0.8f));
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, ui::Em(0.8f));
    if (ImGui::Button("Export")) exportDsl(app);
    ImGui::SameLine();
    if (ImGui::Button("Import")) importDsl(app);

    ImGui::SameLine();
    const float chipW = ui::Em(14.f);
    ui::SameLineRightAlign(chipW);
    if (!compileError_.empty()) {
        ui::StatusChip("Compile error", ui::Danger());
    } else if (scriptDirty_) {
        ui::StatusChip("Unsaved edits", ui::Warn());
    } else if (!compileOkMsg_.empty()) {
        ui::StatusChip("Compile OK", ui::Success());
    } else {
        ImGui::TextDisabled("%s", eventlang::recordKindName(ast_.recordKind));
    }
    ui::EndToolbarRow();

    ImGui::PopStyleVar(2);

    // Record-kind notes sit under the toolbar, not in the action row.
    if (ast_.recordKind == eventlang::RecordKind::CastleBlob) {
        ui::TextWarn("Castle-style record (no triplet terminator) — edit carefully.");
    } else if (ast_.recordKind == eventlang::RecordKind::OverlayBank) {
        ImGui::TextDisabled(
            "Overlay bank (locs 60..70): LE string anchor @ [0..1], scripts @ [2..] — matches ASM "
            "0x176B6.");
    }
}

void EventSection::drawOutline() {
    char sub[48];
    std::snprintf(sub, sizeof(sub), "%zu scripts", ast_.scripts.size());
    ui::PanelHeader("Outline", sub);

    ImGui::Checkbox("Hide empty", &outlineFilterScripts_);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hide scripts with no body");
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 3.f));

    if (ImGui::TreeNodeEx("Scripts", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::map<int, int> triggerCount;
        for (const auto& t : ast_.triggers) triggerCount[t.eventId]++;

        std::set<int> shown;
        for (const auto& sc : ast_.scripts) {
            if (outlineFilterScripts_ && sc.body.empty() && !sc.isPlainText) continue;
            shown.insert(sc.eventId);
            const std::string hint = scriptOutlineHint(sc);
            char label[128];
            if (!hint.empty())
                std::snprintf(label, sizeof(label), "event_%02d  %s", sc.eventId, hint.c_str());
            else
                std::snprintf(label, sizeof(label), "event_%02d", sc.eventId);

            const bool sel = (selectedEvent_ == sc.eventId);
            if (ImGui::Selectable(label, sel)) selectEvent(sc.eventId);
            const int nTrig = triggerCount[sc.eventId];
            if (nTrig > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("%d tile%s", nTrig, nTrig == 1 ? "" : "s");
            }
        }
        for (const auto& [eid, n] : triggerCount) {
            if (shown.count(eid)) continue;
            char label[64];
            std::snprintf(label, sizeof(label), "event_%02d  (empty)", eid);
            if (ImGui::Selectable(label, selectedEvent_ == eid)) selectEvent(eid);
            ImGui::SameLine();
            ImGui::TextDisabled("%d tile%s", n, n == 1 ? "" : "s");
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
}

void EventSection::drawEditor() {
    char sub[96];
    std::snprintf(sub, sizeof(sub), "%s · %zu scripts · %zu strings",
                  eventLocationLabel(selectedLoc_).c_str(), ast_.scripts.size(),
                  ast_.strings.size());
    ui::PanelHeader(".mm2evt", sub);
    ImGui::TextDisabled("Ctrl+click overlay N event_MM to open target");
    const ImVec2 size = ImGui::GetContentRegionAvail();
    if (scriptEditor_.draw("##mm2evt_editor", size)) {
        scriptBuf_ = scriptEditor_.getText();
        scriptDirty_ = true;
        compileOkMsg_.clear();
        if (!compileError_.empty()) {
            // Edits invalidate the previous error marker until next compile.
            compileError_.clear();
            compileErrorLine_ = -1;
            scriptEditor_.clearErrorMarkers();
        }
    }
    tryOpenOverlayFromEditorClick();
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
        } else if (auto ov = overlayTargetFromStmt(st)) {
            char btn[64];
            std::snprintf(btn, sizeof(btn), "%s##ov", label.c_str());
            if (ImGui::Selectable(btn)) openOverlay(ov->first, ov->second);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Open overlay %d event_%02d", ov->first, ov->second);
        } else {
            ImGui::BulletText("%s", label.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
}

void EventSection::drawInspector() {
    if (selectedEvent_ < 0) {
        ui::PanelHeader("Inspector");
        ImGui::TextDisabled("Select a script in the outline.");
        return;
    }

    char sub[32];
    std::snprintf(sub, sizeof(sub), "event_%02d", selectedEvent_);
    ui::PanelHeader("Inspector", sub);

    if (ImGui::SmallButton("Jump to script")) selectEvent(selectedEvent_);

    const eventlang::Script* sc = findScript(ast_, selectedEvent_);
    if (sc && scriptIsOverlayOnly(*sc)) {
        if (auto ov = overlayTargetFromScript(*sc)) {
            ImGui::SameLine();
            char btn[48];
            std::snprintf(btn, sizeof(btn), "Open overlay %d.%02d", ov->first, ov->second);
            if (ImGui::SmallButton(btn)) openOverlay(ov->first, ov->second);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Switch to location %d and select event_%02d", ov->first,
                                  ov->second);
        }
    }

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
    ImGui::Separator();
    ImGui::TextDisabled("Flow");
    if (!sc) {
        ImGui::TextDisabled("  (no script body)");
    } else if (sc->isPlainText) {
        ImGui::TextWrapped("%s", sc->body.empty() ? "(empty text)" : sc->body[0].getStr("text").c_str());
    } else if (sc->body.empty()) {
        ImGui::TextDisabled("  (empty)");
    } else {
        if (scriptIsOverlayOnly(*sc)) {
            if (auto ov = overlayTargetFromScript(*sc)) {
                char tip[80];
                std::snprintf(tip, sizeof(tip),
                              "This script only calls overlay %d event_%02d.", ov->first,
                              ov->second);
                ImGui::TextWrapped("%s", tip);
            }
        }
        ImGui::BeginChild("flow_tree", ImVec2(0, 0), ImGuiChildFlags_None);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 2.f));
        drawStmtTree(sc->body, 0);
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
}

void EventSection::drawProblems() {
    const bool hasErr = !compileError_.empty();
    const bool hasOk = !hasErr && !compileOkMsg_.empty();
    if (!hasErr && !hasOk) return;

    if (hasErr) {
        ui::PushDangerBanner("problems");
        ui::StatusChip("Problems", ui::Danger());
        ImGui::SameLine(0, ui::Em(0.8f));
        if (compileErrorLine_ >= 0) {
            char btn[40];
            std::snprintf(btn, sizeof(btn), "Go to line %d", compileErrorLine_ + 1);
            if (ImGui::SmallButton(btn)) jumpToError();
            ImGui::SameLine();
        }
        ImGui::TextWrapped("%s", compileError_.c_str());
        if (compileErrorLine_ >= 0 && ImGui::IsItemClicked()) jumpToError();
        ui::EndBanner();
    } else {
        ui::PushSuccessBanner("problems");
        ui::TextSuccess("%s", compileOkMsg_.c_str());
        ui::EndBanner();
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

    const bool showProblems = !compileError_.empty() || !compileOkMsg_.empty();
    const float gap = ui::PanelGap();
    const float availY = ImGui::GetContentRegionAvail().y;
    float problemsH = 0.f;
    if (showProblems) {
        if (problemsH_ <= 0.f) problemsH_ = compileError_.empty() ? ui::Em(2.2f) : ui::Em(3.6f);
        problemsH = std::clamp(problemsH_, ui::Em(1.8f), availY * 0.35f);
    }
    const float bodyH = std::max(ui::Em(12.f), availY - (showProblems ? problemsH + gap : 0.f));

    // Toolkit body: outline | editor | inspector
    if (outlineW_ <= 0.f) outlineW_ = ui::Em(15.5f);
    if (inspectorW_ <= 0.f) inspectorW_ = ui::Em(17.f);
    const float availX = ImGui::GetContentRegionAvail().x;
    const float minOutline = ui::Em(11.f);
    const float minInspector = ui::Em(12.f);
    const float minEditor = ui::Em(18.f);
    const float maxOutline = std::max(minOutline, availX - minEditor - minInspector - gap * 3.f);
    const float maxInspector = std::max(minInspector, availX - minEditor - minOutline - gap * 3.f);
    outlineW_ = std::clamp(outlineW_, minOutline, maxOutline);
    inspectorW_ = std::clamp(inspectorW_, minInspector, maxInspector);

    ImGui::BeginChild("evt_toolkit_body", ImVec2(0, bodyH), ImGuiChildFlags_None);

    ImGui::BeginChild("evt_outline", ImVec2(outlineW_, 0), ImGuiChildFlags_Borders);
    drawOutline();
    ImGui::EndChild();

    ui::VSplitter("##outline_split", &outlineW_, minOutline, maxOutline);

    ImGui::SameLine(0, 0);
    const float splitThick = ui::Em(0.35f);
    const float editorW =
        std::max(minEditor, ImGui::GetContentRegionAvail().x - inspectorW_ - splitThick);
    ImGui::BeginChild("evt_editor_host", ImVec2(editorW, 0), ImGuiChildFlags_Borders);
    drawEditor();
    ImGui::EndChild();

    ui::VSplitter("##inspector_split", &inspectorW_, minInspector, maxInspector, /*rightPanel=*/true);

    ImGui::SameLine(0, 0);
    ImGui::BeginChild("evt_inspector", ImVec2(inspectorW_, 0), ImGuiChildFlags_Borders);
    drawInspector();
    ImGui::EndChild();

    ImGui::EndChild();  // evt_toolkit_body

    if (showProblems) {
        ImGui::Dummy(ImVec2(0, gap * 0.5f));
        ImGui::InvisibleButton("##problems_split", ImVec2(-1.f, ui::Em(0.3f)));
        if (ImGui::IsItemActive()) {
            problemsH_ -= ImGui::GetIO().MouseDelta.y;
            problemsH_ = std::clamp(problemsH_, ui::Em(1.8f), availY * 0.45f);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ui::Em(0.05f));
        ImGui::BeginChild("##problems_host", ImVec2(0, problemsH_), ImGuiChildFlags_None);
        drawProblems();
        ImGui::EndChild();
    }
}

}  // namespace mm2
