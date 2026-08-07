#include "sections/EventSection.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
#include "sections/eventwizard/EventWizard.h"
#include "sections/eventgraph/EventGraph.h"
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

eventlang::Script* findScriptMut(eventlang::Location& loc, int eventId) {
    for (auto& sc : loc.scripts)
        if (sc.eventId == eventId) return &sc;
    return nullptr;
}

bool itemCombo(const char* id, App& app, int* itemId) {
    char preview[64];
    std::string nm = app.itemName(*itemId);
    std::snprintf(preview, sizeof(preview), "0x%02X  %s", *itemId, nm.c_str());
    bool changed = false;
    if (ImGui::BeginCombo(id, preview)) {
        for (int i = 0; i < 256; ++i) {
            std::string n = app.itemName(i);
            char label[72];
            std::snprintf(label, sizeof(label), "0x%02X  %s", i, n.c_str());
            if (ImGui::Selectable(label, i == *itemId)) {
                *itemId = i;
                changed = true;
            }
            if (i == *itemId) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool stringCombo(const char* id, eventlang::Location& loc, eventlang::Stmt& st) {
    std::string cur = st.getStr("string");
    if (cur.empty() && st.num.count("string"))
        cur = "s" + std::to_string(st.getNum("string"));
    bool changed = false;
    if (ImGui::BeginCombo(id, cur.empty() ? "(none)" : cur.c_str())) {
        for (const auto& sd : loc.strings) {
            const char* name = sd.name.empty() ? "?" : sd.name.c_str();
            char label[160];
            std::snprintf(label, sizeof(label), "%s  \"%.40s%s\"", name, sd.text.c_str(),
                          sd.text.size() > 40 ? "…" : "");
            if (ImGui::Selectable(label, sd.name == cur)) {
                st.str["string"] = sd.name;
                st.num.erase("string");
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
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

std::string hex2sh(int v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02X", v & 0xFF);
    return buf;
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
    graph_.lastFocusedEvent = eventId;
    graph_.selectedNodeId = -1;
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

bool EventSection::confirmDiscardBuffer(const char* action) {
    if (!scriptDirty_) return true;
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "Location %d has unsaved script edits.\n\nDiscard buffer and %s?", selectedLoc_,
                  action);
    auto r = pfd::message("Unsaved script", msg, pfd::choice::yes_no, pfd::icon::warning).result();
    return r == pfd::button::yes;
}

void EventSection::openOverlay(int locId, int eventId) {
    if (locId < 0 || locId >= static_cast<int>(file_.locations.size())) return;
    if (!confirmDiscardBuffer("open overlay")) return;
    scriptDirty_ = false;
    selectedLoc_ = locId;
    selectedEvent_ = eventId;
    refreshAst();
    selectEvent(eventId);
}

void EventSection::focusIndex(int index) {
    if (index < 0 || index >= static_cast<int>(file_.locations.size())) return;
    if (index == selectedLoc_) return;
    if (!confirmDiscardBuffer("switch location")) return;
    scriptDirty_ = false;
    selectedLoc_ = index;
    refreshAst();
}

const char* EventSection::bufferStatus() const {
    if (scriptDirty_) return "script dirty (Compile before Save)";
    if (dirty) return "event.dat modified";
    return nullptr;
}

void EventSection::tryOpenOverlayFromEditorClick() {
    // Ctrl+click on an `overlay N event_MM` line → jump to that bank script.
    if (!ImGui::IsItemHovered()) return;
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) return;
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        return;
    const TextEditor::DocPos cursor = scriptEditor_.raw().GetCurrentCursorPosition();
    const std::string row = scriptEditor_.raw().GetLineText(cursor.line);
    if (auto t = parseOverlayLine(row)) openOverlay(t->first, t->second);
}

void EventSection::jumpToError() {
    if (compileErrorLine_ >= 0) scriptEditor_.goToLine(compileErrorLine_);
}

bool EventSection::structuredEditsAllowed() const {
    if (ast_.recordKind == eventlang::RecordKind::CastleBlob ||
        ast_.recordKind == eventlang::RecordKind::OverlayBank)
        return false;
    return true;
}

void EventSection::regenerateFromAst() {
    const eventlang::EventFileAst full =
        eventlang::decompileEventDat(file_.raw.data(), file_.raw.size());
    ast_.modified = true;
    eventlang::EmitLookups lookups;
    if (app_) {
        lookups.itemName = [this](int id) { return app_->itemName(id); };
        lookups.monsterName = [this](int id) { return app_->monsterName(id); };
    }
    scriptBuf_ = eventlang::emitLocation(ast_, eventLocationLabel(selectedLoc_), &full, &lookups);
    scriptEditor_.setText(scriptBuf_, selectedLoc_);
    scriptDirty_ = true;
    compileOkMsg_.clear();
    compileError_.clear();
    compileErrorLine_ = -1;
    scriptEditor_.clearErrorMarkers();
}

eventlang::Script* EventSection::selectedScript() {
    return findScriptMut(ast_, selectedEvent_);
}

eventlang::Script* EventSection::scriptByEvent(int eventId) {
    return findScriptMut(ast_, eventId);
}

void EventSection::jumpToEvent(int eventId) {
    selectedEvent_ = eventId;
    graph_.lastFocusedEvent = eventId;
}

void EventSection::rebuildGraph(App& app) {
    (void)app;
    // Order scripts deterministically; focused event gets shown but order by
    // event id for a stable layout.
    std::vector<int> order;
    for (const auto& sc : ast_.scripts) order.push_back(sc.eventId);
    std::sort(order.begin(), order.end());
    graph_.rebuild(ast_, selectedLoc_, selectedEvent_, order, app_);
}

void EventSection::commitAstEdit(App& app) {
    applyAstToFile(app);
    rebuildGraph(app);
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
    graph_.layoutDirty = true;
    graph_.builtForLoc = -1;  // force rebuild on next drawGraph
}

void EventSection::syncScriptBuffer() {
    // Full-file AST so overlay OP_0E lines can show a short target-string hint.
    const eventlang::EventFileAst full =
        eventlang::decompileEventDat(file_.raw.data(), file_.raw.size());
    eventlang::EmitLookups lookups;
    if (app_) {
        lookups.itemName = [this](int id) { return app_->itemName(id); };
        lookups.monsterName = [this](int id) { return app_->monsterName(id); };
    }
    scriptBuf_ = eventlang::emitLocation(ast_, eventLocationLabel(selectedLoc_), &full, &lookups);
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
    if (ImGui::BeginCombo("##loc", eventLocationLabel(selectedLoc_).c_str())) {
        for (int i = 0; i < static_cast<int>(file_.locations.size()); ++i) {
            bool sel = (i == selectedLoc_);
            if (ImGui::Selectable(eventLocationLabel(i).c_str(), sel) && i != selectedLoc_) {
                if (confirmDiscardBuffer("switch location")) {
                    scriptDirty_ = false;
                    selectedLoc_ = i;
                }
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
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

    ImGui::SameLine(0, ui::Em(0.8f));
    ImGui::TextDisabled("|");
    if (ImGui::BeginMenu("New Event…")) {
        bool openWizard = false;
        if (ImGui::MenuItem("New Event Wizard…", "Ctrl+N")) openWizard = true;
        ImGui::Separator();
        for (const auto& t : wizardTemplates()) {
            char id[64];
            std::snprintf(id, sizeof(id), "##prefab_%d", static_cast<int>(t.kind));
            if (ImGui::MenuItem(t.name)) {
                wizard_.templateIdx = static_cast<int>(t.kind);
                openWizard = true;
            }
        }
        ImGui::EndMenu();
        if (openWizard) openWizardModal();
    }

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

void EventSection::drawOutline(EditorSelection& sel) {
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

            const bool selRow = (selectedEvent_ == sc.eventId);
            if (ImGui::Selectable(label, selRow)) {
                selectEvent(sc.eventId);
                sel.Select(DocKind::Events, EditorSelection::Kind::EventNode, selectedLoc_,
                           sc.eventId);
                sel.RequestTab("Graph");
            }
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
            if (ImGui::Selectable(label, selectedEvent_ == eid)) {
                selectEvent(eid);
                sel.Select(DocKind::Events, EditorSelection::Kind::EventNode, selectedLoc_, eid);
                sel.RequestTab("Graph");
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%d tile%s", n, n == 1 ? "" : "s");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Triggers", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < static_cast<int>(ast_.triggers.size()); ++i) {
            const auto& t = ast_.triggers[i];
            char label[96];
            std::snprintf(label, sizeof(label), "(%d,%d) → %02d  %s", t.y, t.x, t.eventId,
                          eventlang::triggerCondName(t.cond));
            const bool selRow = sel.kind == EditorSelection::Kind::EventTrigger && sel.index == i;
            if (ImGui::Selectable(label, selRow, ImGuiSelectableFlags_AllowDoubleClick)) {
                selectedEvent_ = t.eventId;
                sel.Select(DocKind::Events, EditorSelection::Kind::EventTrigger, i);
                // Double-click jumps into the script body; single click edits in Properties.
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) sel.RequestTab("Graph");
            }
        }
        if (ast_.triggers.empty()) ImGui::TextDisabled("(none)");
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Strings", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < static_cast<int>(ast_.strings.size()); ++i) {
            const auto& s = ast_.strings[i];
            char label[64];
            std::snprintf(label, sizeof(label), "%s", s.name.c_str());
            const bool selRow = sel.kind == EditorSelection::Kind::EventString && sel.index == i;
            if (ImGui::Selectable(label, selRow)) {
                sel.Select(DocKind::Events, EditorSelection::Kind::EventString, i);
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

void EventSection::drawGraph(App& app, EditorSelection& sel) {
    ensureSelectedEvent();
    if (selectedEvent_ < 0) {
        ui::EmptyState("No script selected", "Pick a script in the Outline");
        return;
    }

    char sub[64];
    std::snprintf(sub, sizeof(sub), "loc %d · %zu scripts", selectedLoc_, ast_.scripts.size());
    ui::PanelHeader("Opcode graph", sub);
    ImGui::TextDisabled("Select a node to edit on it · right-click to add / navigate");

    if (graph_.builtForLoc != selectedLoc_)
        rebuildGraph(app);

    eventlang::Script* sc = selectedScript();
    const bool readOnly = scriptDirty_;
    const bool allowMutate = structuredEditsAllowed() && !readOnly;

    bool selChanged = false;
    bool astMutated = false;
    int focusedEvent = selectedEvent_;
    graph_.pendingAction = EventGraph::PendingAction::None;
    graph_.draw(app, ast_, sc, readOnly, allowMutate, &focusedEvent, &selChanged, &astMutated);
    if (astMutated) commitAstEdit(app);
    if (focusedEvent != selectedEvent_) {
        selectedEvent_ = focusedEvent;
        sel.Select(DocKind::Events, EditorSelection::Kind::EventNode, selectedLoc_,
                   selectedEvent_);
    } else if (selChanged || graph_.selectedNodeId >= 0) {
        sel.Select(DocKind::Events, EditorSelection::Kind::EventNode, selectedLoc_,
                   selectedEvent_);
    }

    // Consume pending navigation from the graph context menu.
    switch (graph_.pendingAction) {
        case EventGraph::PendingAction::ShowOnMap:
            app.openMapTile(selectedLoc_, graph_.pendingTileY, graph_.pendingTileX);
            break;
        case EventGraph::PendingAction::NavigateOverlay:
            if (graph_.pendingLoc >= 0 && graph_.pendingEvent >= 0)
                openOverlay(graph_.pendingLoc, graph_.pendingEvent);
            break;
        case EventGraph::PendingAction::JumpEvent:
            if (graph_.pendingEvent >= 0) jumpToEvent(graph_.pendingEvent);
            break;
        default:
            break;
    }
    graph_.pendingAction = EventGraph::PendingAction::None;
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

void EventSection::drawScriptSummary(App& app) {
    if (selectedEvent_ < 0) {
        ui::PanelHeader("Properties");
        ui::EmptyState("Nothing selected", "Select a script in Outline or a node in Graph");
        return;
    }

    char sub[32];
    std::snprintf(sub, sizeof(sub), "event_%02d", selectedEvent_);
    ui::PanelHeader("Script", sub);

    // Chain controls.
    if (ImGui::SmallButton("Jump to script text")) {
        selectEvent(selectedEvent_);
    }
    ImGui::SameLine();
    ImGui::SmallButton("Focus graph");

    const eventlang::Script* sc = findScript(ast_, selectedEvent_);
    int trigCount = 0;
    std::vector<int> triggeredBy;  // scripts in this loc that overlay into this event
    for (const auto& t : ast_.triggers)
        if (t.eventId == selectedEvent_) ++trigCount;
    // Who calls into this event (in-location selectors/overlays)?
    for (const auto& s : ast_.scripts) {
        if (s.body.empty()) continue;
        std::function<void(const std::vector<eventlang::Stmt>&)> walk;
        walk = [&](const std::vector<eventlang::Stmt>& list) {
            for (const auto& st : list) {
                if (st.kind == "if") {
                    walk(st.thenBody);
                    walk(st.elseBody);
                    continue;
                }
                if (st.kind == "selector" || st.kind == "shop" || st.kind == "quest") {
                    int selv = st.getNum("value");
                    if (!selv) selv = st.getNum("selector");
                    if (!selv && st.kind != "selector")
                        selv = eventlang::selectorByShopOrQuest(st.kind, st.getStr("name"));
                    auto bin = eventlang::binExecSelector(selv);
                    if (bin && bin->first == selectedLoc_ && bin->second == selectedEvent_)
                        triggeredBy.push_back(s.eventId);
                }
            }
        };
        walk(s.body);
    }
    // Who this event calls out to (in-location).
    std::vector<int> callsTo;
    if (sc) {
        std::function<void(const std::vector<eventlang::Stmt>&)> walk;
        walk = [&](const std::vector<eventlang::Stmt>& list) {
            for (const auto& st : list) {
                if (st.kind == "if") {
                    walk(st.thenBody);
                    walk(st.elseBody);
                    continue;
                }
                if (st.kind == "selector" || st.kind == "shop" || st.kind == "quest") {
                    int selv = st.getNum("value");
                    if (!selv) selv = st.getNum("selector");
                    if (!selv && st.kind != "selector")
                        selv = eventlang::selectorByShopOrQuest(st.kind, st.getStr("name"));
                    auto bin = eventlang::binExecSelector(selv);
                    if (bin && bin->first == selectedLoc_ && bin->second != selectedEvent_)
                        callsTo.push_back(bin->second);
                }
            }
        };
        walk(sc->body);
    }

    ui::SectionBlock("Summary");
    {
        ui::FormTable form("evt_sum");
        if (form.begin()) {
            form.row("Opcodes", [&] {
                ImGui::TextUnformatted(sc ? std::to_string(sc->body.size()).c_str() : "—");
            });
            form.row("Triggers", [&] { ImGui::TextUnformatted(std::to_string(trigCount).c_str()); });
            form.row("Strings",
                     [&] { ImGui::TextUnformatted(std::to_string(ast_.strings.size()).c_str()); });
        }
    }

    ImGui::Spacing();

    ImGui::TextDisabled("Triggers on this script");
    if (trigCount == 0) {
        ImGui::TextDisabled("  (none)");
    } else {
        for (int i = 0; i < static_cast<int>(ast_.triggers.size()); ++i) {
            const auto& t = ast_.triggers[i];
            if (t.eventId != selectedEvent_) continue;
            char lbl[72];
            std::snprintf(lbl, sizeof(lbl), "(%d,%d)  %s##t%d", t.y, t.x,
                          eventlang::triggerCondName(t.cond), i);
            if (ImGui::Selectable(lbl)) app.openMapTile(selectedLoc_, t.y, t.x);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to show on map");
        }
    }

    // Chain: who calls in / who it calls out to — with navigation.
    ImGui::Spacing();
    ImGui::TextDisabled("Calls into this script (overlay/selector)");
    if (triggeredBy.empty()) {
        ImGui::TextDisabled("  (none in this location)");
    } else {
        for (int eid : triggeredBy) {
            char lbl[48];
            std::snprintf(lbl, sizeof(lbl), "← event_%02d##in%d", eid, eid);
            if (ImGui::Selectable(lbl)) {
                jumpToEvent(eid);
            }
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("This script calls out to");
    if (callsTo.empty()) {
        ImGui::TextDisabled("  (none in this location)");
    } else {
        for (int eid : callsTo) {
            char lbl[48];
            std::snprintf(lbl, sizeof(lbl), "→ event_%02d##out%d", eid, eid);
            if (ImGui::Selectable(lbl)) {
                jumpToEvent(eid);
            }
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Select a node in the Graph tab to edit opcode fields.");
}

void EventSection::drawOpcodeInspector(App& app, EditorSelection& sel) {
    (void)sel;
    const GraphNode* node = graph_.findNode(graph_.selectedNodeId);
    if (!node || node->eventId < 0) {
        drawScriptSummary(app);
        return;
    }

    // Header / trigger nodes: navigation + script summary, no opcode fields.
    if (node->category == GraphNodeCategory::Header ||
        node->category == GraphNodeCategory::Trigger) {
        ui::PanelHeader(node->title.c_str(),
                        node->subtitle.empty() ? nullptr : node->subtitle.c_str());
        ui::SectionBlock("Navigate");
        bool any = false;
        for (int i = 0; i < static_cast<int>(ast_.triggers.size()); ++i) {
            const auto& t = ast_.triggers[i];
            if (t.eventId != node->eventId) continue;
            any = true;
            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "Show on map (%d,%d)##sm%d", t.y, t.x, i);
            if (ImGui::Button(lbl)) app.openMapTile(selectedLoc_, t.y, t.x);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", eventlang::triggerCondName(t.cond));
        }
        if (!any) ImGui::TextDisabled("No tile triggers for this script");
        if (ImGui::Button("Jump to script text")) selectEvent(node->eventId);
        return;
    }

    eventlang::Script* sc = scriptByEvent(node->eventId);
    if (!sc) {
        drawScriptSummary(app);
        return;
    }

    eventlang::Stmt* st = EventGraph::resolveStmt(*sc, node->path);
    if (!st) {
        drawScriptSummary(app);
        return;
    }

    std::string title, subtitle;
    describeStmt(*st, ast_, &app, &title, &subtitle);
    ui::PanelHeader(title.c_str(), subtitle.empty() ? nullptr : subtitle.c_str());

    // Always-visible navigation strip.
    {
        bool anyNav = false;
        for (const auto& t : ast_.triggers) {
            if (t.eventId != node->eventId) continue;
            char lbl[48];
            std::snprintf(lbl, sizeof(lbl), "Map (%d,%d)##nm%d", t.y, t.x, t.y * 16 + t.x);
            if (ImGui::SmallButton(lbl)) app.openMapTile(selectedLoc_, t.y, t.x);
            ImGui::SameLine();
            anyNav = true;
        }
        if (node->overlayLoc >= 0 && node->overlayEvent >= 0) {
            char jl[64];
            std::snprintf(jl, sizeof(jl), "→ loc %d event_%02d", node->overlayLoc,
                          node->overlayEvent);
            if (ImGui::SmallButton(jl)) openOverlay(node->overlayLoc, node->overlayEvent);
            anyNav = true;
        }
        if (st->kind == "go_to") {
            if (ImGui::SmallButton("Open map##goto"))
                app.openDocumentFocused(DocKind::Map, st->getNum("screen"));
            anyNav = true;
        }
        if (anyNav) ImGui::Spacing();
    }

    const bool canEdit = structuredEditsAllowed() && !scriptDirty_;
    if (scriptDirty_) {
        ui::TextWarn("Compile script before editing opcode fields");
        ImGui::Spacing();
    } else if (!structuredEditsAllowed()) {
        ui::TextWarn("Structured edits disabled for this record kind");
        ImGui::Spacing();
    }

    bool mutated = false;

    if (st->kind == "say") {
        ui::SectionBlock("Dialogue");
        {
            ui::FormTable form("op_say");
            if (form.begin()) {
                form.row("Variant", [&] {
                    const char* variants[] = {"", "door", "block", "popup_a", "popup_b", "basic"};
                    const char* labels[] = {"say", "say_door", "say_block", "say_popup_a",
                                            "say_popup_b", "say_basic"};
                    int cur = 0;
                    const std::string v = st->getStr("variant");
                    for (int i = 0; i < 6; ++i)
                        if (v == variants[i]) cur = i;
                    if (canEdit && ImGui::BeginCombo("##var", labels[cur])) {
                        for (int i = 0; i < 6; ++i)
                            if (ImGui::Selectable(labels[i], i == cur)) {
                                st->str["variant"] = variants[i];
                                mutated = true;
                            }
                        ImGui::EndCombo();
                    } else if (!canEdit) {
                        ImGui::TextUnformatted(labels[cur]);
                    }
                });
                form.row("String", [&] {
                    if (canEdit) {
                        if (stringCombo("##str", ast_, *st)) mutated = true;
                    } else {
                        ImGui::TextUnformatted(st->getStr("string").c_str());
                    }
                });
            }
        }
        std::string preview;
        {
            std::string ref = st->getStr("string");
            if (ref.empty() && st->num.count("string"))
                ref = "s" + std::to_string(st->getNum("string"));
            for (const auto& sd : ast_.strings) {
                if (sd.name == ref) {
                    preview = sd.text;
                    break;
                }
                if (ref.size() >= 2 && (ref[0] == 's' || ref[0] == 'S') && sd.index == std::atoi(ref.c_str() + 1)) {
                    preview = sd.text;
                    break;
                }
            }
        }
        if (!preview.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Preview");
            ImGui::TextWrapped("%s", preview.c_str());
        }
    } else if (st->kind == "give_item") {
        ui::SectionBlock("Item");
        int item = st->getNum("item");
        if (canEdit) {
            if (itemCombo("##gi", app, &item)) {
                st->set("item", item);
                mutated = true;
            }
        } else {
            ImGui::Text("0x%02X  %s", item, app.itemName(item).c_str());
        }
        if (ImGui::SmallButton("Open in Items")) app.openDocumentFocused(DocKind::Items, item);
    } else if (st->kind == "set_cond") {
        ui::SectionBlock("Condition");
        ImGui::TextDisabled("kind: %s", st->cond.kind.c_str());
        if (st->cond.kind == "gold_at_least" || st->cond.kind == "gems_at_least") {
            int amt = st->cond.getNum("amount", st->cond.getNum("value"));
            if (canEdit) {
                ImGui::SetNextItemWidth(ui::Em(8.f));
                if (ImGui::InputInt("Amount", &amt)) {
                    if (amt < 0) amt = 0;
                    st->cond.set("amount", amt);
                    mutated = true;
                }
            } else {
                ImGui::Text("Amount: %d", amt);
            }
        } else if (st->cond.kind == "consume_item" || st->cond.kind == "has_item_id" ||
                   st->cond.kind == "give_item_ok" || st->cond.kind == "party_has_item") {
            int item = st->cond.getNum("item", st->cond.getNum("b"));
            if (canEdit) {
                if (itemCombo("##ci", app, &item)) {
                    st->cond.set("item", item);
                    mutated = true;
                }
            } else {
                ImGui::Text("0x%02X  %s", item, app.itemName(item).c_str());
            }
            if (ImGui::SmallButton("Open in Items"))
                app.openDocumentFocused(DocKind::Items, item);
        } else if (st->cond.kind == "answer_eq") {
            std::string ans = st->cond.getStr("text");
            if (canEdit) {
                if (ui::TextInput("Answer", ans)) {
                    st->cond.set("text", ans);
                    mutated = true;
                }
            } else {
                ImGui::Text("\"%s\"", ans.c_str());
            }
        }
    } else if (st->kind == "if") {
        ui::SectionBlock("Branch condition");
        ImGui::TextWrapped("%s", formatExprShort(st->cond).c_str());
        ImGui::TextDisabled("then: %d stmts · else: %d stmts",
                            static_cast<int>(st->thenBody.size()),
                            static_cast<int>(st->elseBody.size()));
        if (st->cond.kind == "gold_at_least") {
            int amt = st->cond.getNum("amount");
            if (canEdit && ImGui::InputInt("Gold >=", &amt)) {
                st->cond.set("amount", amt);
                mutated = true;
            }
        }
    } else if (st->kind == "go_to") {
        ui::SectionBlock("Transition");
        int screen = st->getNum("screen");
        int pos = st->getNum("pos");
        if (canEdit) {
            ImGui::SetNextItemWidth(ui::Em(6.f));
            if (ImGui::InputInt("Screen", &screen)) {
                st->set("screen", screen);
                mutated = true;
            }
            ImGui::SetNextItemWidth(ui::Em(6.f));
            if (ImGui::InputInt("Pos", &pos)) {
                st->set("pos", pos);
                mutated = true;
            }
        } else {
            ImGui::Text("Screen %d  pos 0x%02X", screen, pos);
        }
        const char* an = areaNameRaw(screen);
        if (an && an[0]) ImGui::TextDisabled("%s", an);
        if (ImGui::SmallButton("Open Map")) app.openDocumentFocused(DocKind::Map, screen);
    } else if (st->kind == "fight" || st->kind == "fight_b") {
        ui::SectionBlock("Encounter");
        std::string title, detail;
        describeFightEncounter(*st, &app, &title, &detail);
        ImGui::TextUnformatted(title.c_str());
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextWrapped("%s", detail.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();

        const bool randomPool = st->kind == "fight_b";
        auto mit = st->lists.find(randomPool ? "data" : "monsters");
        if (mit == st->lists.end() && randomPool) mit = st->lists.find("monsters");
        if (mit != st->lists.end()) {
            ui::FormTable form("fight_slots", ui::Em(5.f));
            if (form.begin()) {
                for (size_t i = 0; i < mit->second.size(); ++i) {
                    int mid = mit->second[i];
                    char lab[16];
                    std::snprintf(lab, sizeof(lab), "Slot %zu", i + 1);
                    form.row(lab, [&] {
                        if (canEdit) {
                            ui::SetFieldShort();
                            if (ImGui::InputInt(("##fs" + std::to_string(i)).c_str(), &mid, 1, 5)) {
                                if (mid < 0) mid = 0;
                                if (mid > 255) mid = 255;
                                mit->second[i] = mid;
                                mutated = true;
                            }
                            ImGui::SameLine();
                        }
                        if (mid != 0) {
                            ImGui::TextDisabled("%s", app.monsterName(mid).c_str());
                            ImGui::SameLine();
                            char btn[32];
                            std::snprintf(btn, sizeof(btn), "Open##m%zu", i);
                            if (ImGui::SmallButton(btn))
                                app.openDocumentFocused(DocKind::Monsters, mid);
                        } else {
                            ImGui::TextDisabled("(empty)");
                        }
                    });
                }
            }
        } else {
            ImGui::TextDisabled("(no monster slots)");
        }

        if (!randomPool) {
            auto fit = st->lists.find("flags");
            if (fit != st->lists.end() && fit->second.size() >= 2) {
                ImGui::Spacing();
                int ov = fit->second[0];
                int lc = fit->second[1];
                if (canEdit) {
                    ui::SetFieldShort();
                    if (ImGui::InputInt("Overflow type", &ov, 1, 5)) {
                        fit->second[0] = ov & 0xFF;
                        mutated = true;
                    }
                    ui::SetFieldShort();
                    if (ImGui::InputInt("Overflow count", &lc, 1, 5)) {
                        fit->second[1] = lc & 0xFF;
                        mutated = true;
                    }
                } else if (ov != 0) {
                    ImGui::TextDisabled("Overflow: %d× %s  (type 0x%02X)", lc,
                                        app.monsterName(ov).c_str(), ov);
                }
            }
        } else {
            ImGui::Spacing();
            ImGui::TextDisabled("OP_13 — seeded random fight from this pool");
        }
    } else if (st->kind == "selector" || st->kind == "shop" || st->kind == "quest" ||
               st->kind == "overlay") {
        ui::SectionBlock("Jump / service");
        if (st->kind == "overlay") {
            int ol = st->getNum("loc", st->getNum("overlay"));
            int oe = st->getNum("event");
            if (canEdit) {
                if (ImGui::InputInt("Location", &ol)) {
                    st->set("loc", ol);
                    mutated = true;
                }
                if (ImGui::InputInt("Event", &oe)) {
                    st->set("event", oe);
                    mutated = true;
                }
            } else {
                ImGui::Text("loc %d · event_%02d", ol, oe);
            }
            if (ImGui::Button("Go to target")) openOverlay(ol, oe);
        } else {
            int sel = st->getNum("value");
            if (!sel) sel = st->getNum("selector");
            if (!sel && st->kind != "selector")
                sel = eventlang::selectorByShopOrQuest(st->kind, st->getStr("name"));
            if (canEdit) {
                if (ImGui::InputInt("Selector", &sel)) {
                    st->set("value", sel & 0xFF);
                    mutated = true;
                }
                if (st->kind == "shop" || st->kind == "quest") {
                    char nameBuf[32];
                    std::string nm = st->getStr("name");
                    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf) - 1);
                    nameBuf[sizeof(nameBuf) - 1] = '\0';
                    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                        st->set("name", std::string(nameBuf));
                        mutated = true;
                    }
                }
            }
            ImGui::TextDisabled("%s", eventlang::formatSelectorSummary(sel).c_str());
            if (auto bin = eventlang::binExecSelector(sel)) {
                char jl[64];
                std::snprintf(jl, sizeof(jl), "Go to loc %d event_%02d", bin->first, bin->second);
                if (ImGui::Button(jl)) openOverlay(bin->first, bin->second);
            }
        }
    } else if (st->kind == "skip_if_true" || st->kind == "skip_if_false" ||
               st->kind == "skip_if_victory") {
        ui::SectionBlock("Skip");
        int n = st->getNum("n");
        if (canEdit) {
            if (ImGui::InputInt("Tokens", &n)) {
                st->set("n", n);
                mutated = true;
            }
        } else {
            ImGui::Text("n = %d", n);
        }
    } else if (st->kind == "ask_yes_no") {
        ui::SectionBlock("Prompt");
        bool m = st->getNum("mode") != 0;
        if (canEdit) {
            if (ImGui::Checkbox("Alternate mode (mode=1)", &m)) {
                st->set("mode", m ? 1 : 0);
                mutated = true;
            }
        } else {
            ImGui::TextUnformatted(m ? "ask yes_no mode=1" : "ask yes_no");
        }
    } else if (st->kind == "wait") {
        ui::SectionBlock("Wait");
        bool m = st->getNum("mode") != 0;
        if (canEdit) {
            if (ImGui::Checkbox("Mode 1", &m)) {
                st->set("mode", m ? 1 : 0);
                mutated = true;
            }
        } else {
            ImGui::TextUnformatted(m ? "wait space mode=1" : "wait space");
        }
    } else if (st->kind == "end" || st->kind == "abort" || st->kind == "clear_tile_event") {
        ui::SectionBlock("Control");
        ImGui::TextDisabled("No fields — terminal / side-effect opcode");
    } else {
        ui::SectionBlock("Opcode");
        ImGui::TextWrapped("%s", stmtLabel(*st).c_str());
        ImGui::TextDisabled("Edit remaining fields in the Script tab for now.");
    }

    if (mutated && canEdit) commitAstEdit(app);
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

void EventSection::drawWorkspace(App& app, EditorSelection& sel) {
    if (!loaded) {
        ui::EmptyState("event.dat not loaded", "Open a folder containing event.dat");
        return;
    }
    if (file_.locations.empty()) {
        ui::EmptyState("No locations", "event.dat decoded with zero location records");
        return;
    }

    selectedLoc_ = std::clamp(selectedLoc_, 0, static_cast<int>(file_.locations.size()) - 1);
    if (astLoc_ != selectedLoc_) refreshAst();

    if (wizardTargetEvent_ >= 0) {
        const int target = wizardTargetEvent_;
        wizardTargetEvent_ = -1;
        selectedEvent_ = target;
        sel.Select(DocKind::Events, EditorSelection::Kind::EventNode, selectedLoc_, target);
        sel.RequestTab("Graph");
        selectEvent(target);
    }

    if (sel.doc != DocKind::Events || sel.kind == EditorSelection::Kind::None)
        sel.Select(DocKind::Events, EditorSelection::Kind::EventLoc, selectedLoc_, selectedEvent_);
    else if (sel.kind == EditorSelection::Kind::EventLoc && sel.index >= 0 &&
             sel.index < static_cast<int>(file_.locations.size()) && sel.index != selectedLoc_) {
        if (confirmDiscardBuffer("switch location")) {
            scriptDirty_ = false;
            selectedLoc_ = sel.index;
            refreshAst();
        } else {
            sel.index = selectedLoc_;
        }
    }

    drawToolbar(app);

    // Dirty legend under toolbar
    {
        ImGui::TextDisabled("Dirty:");
        ImGui::SameLine();
        if (scriptDirty_)
            ui::StatusChip("script", ui::Warn());
        else
            ImGui::TextDisabled("script");
        ImGui::SameLine();
        ImGui::TextDisabled("/");
        ImGui::SameLine();
        if (dirty)
            ui::StatusChip("file", ui::Warn());
        else
            ImGui::TextDisabled("file");
        ImGui::SameLine(0, ui::Em(0.8f));
        ImGui::TextDisabled("Compile writes location → memory; Save writes event.dat to disk");
    }
    ImGui::Spacing();

    const bool showProblems = !compileError_.empty() || !compileOkMsg_.empty();
    const float gap = ui::PanelGap();
    const float availY = ImGui::GetContentRegionAvail().y;
    float problemsH = 0.f;
    if (showProblems) {
        if (problemsH_ <= 0.f) problemsH_ = compileError_.empty() ? ui::Em(2.2f) : ui::Em(3.6f);
        problemsH = std::clamp(problemsH_, ui::Em(1.8f), availY * 0.35f);
    }
    const float bodyH = std::max(ui::Em(12.f), availY - (showProblems ? problemsH + gap : 0.f));

    bool forceScript = false;
    bool forceOutline = false;
    bool forceGraph = false;
    if (!sel.requestInnerTab.empty() && sel.doc == DocKind::Events) {
        if (sel.requestInnerTab == "Script") forceScript = true;
        if (sel.requestInnerTab == "Outline") forceOutline = true;
        if (sel.requestInnerTab == "Graph") forceGraph = true;
        sel.requestInnerTab.clear();
    }

    ImGui::BeginChild("evt_toolkit_body", ImVec2(0, bodyH), ImGuiChildFlags_None);

    if (ImGui::BeginTabBar("evt_inner", ImGuiTabBarFlags_None)) {
        ImGuiTabItemFlags outlineFlags = forceOutline ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags graphFlags = forceGraph ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags scriptFlags = forceScript ? ImGuiTabItemFlags_SetSelected : 0;

        if (ImGui::BeginTabItem("Outline", nullptr, outlineFlags)) {
            drawOutline(sel);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Graph", nullptr, graphFlags)) {
            drawGraph(app, sel);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Script", nullptr, scriptFlags)) {
            drawEditor();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    if (showProblems) {
        ImGui::Dummy(ImVec2(0, gap * 0.5f));
        ImGui::BeginChild("##problems_host", ImVec2(0, problemsH_), ImGuiChildFlags_None);
        drawProblems();
        ImGui::EndChild();
    }

    drawWizard(app);

    if (sel.kind != EditorSelection::Kind::EventTrigger &&
        sel.kind != EditorSelection::Kind::EventString)
        sel.Select(DocKind::Events, EditorSelection::Kind::EventNode, selectedLoc_, selectedEvent_);
}

void EventSection::openWizardModal() {
    wizardOpen_ = true;
    wizard_.templateIdx = std::clamp(wizard_.templateIdx, 0,
                                     static_cast<int>(WizardTemplateKind::Count) - 1);
    // Default event id = first free.
    wizard_.eventId = 0;
    for (const auto& t : ast_.triggers) wizard_.eventId = std::max(wizard_.eventId, t.eventId + 1);
    for (const auto& sc : ast_.scripts) wizard_.eventId = std::max(wizard_.eventId, sc.eventId + 1);
    std::snprintf(wizard_.scriptName, sizeof(wizard_.scriptName), "event_%02d", wizard_.eventId);
}

void EventSection::drawWizard(App& app) {
    if (!wizardOpen_) return;

    const auto& templates = wizardTemplates();
    const WizardTemplate& tmpl = templates[wizard_.templateIdx];

    ImGui::OpenPopup("New Event Wizard");
    ImVec2 sz(ui::Em(46.f), ui::Em(40.f));
    ImGui::SetNextWindowSize(sz, ImGuiCond_Appearing);
    bool open = true;
    if (ImGui::BeginPopupModal("New Event Wizard", &open,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextDisabled("%s — %s", tmpl.name, tmpl.blurb);
        ImGui::Separator();

        // Template picker
        {
            const char* cur = templates[wizard_.templateIdx].name;
            ImGui::Text("Template");
            ImGui::SameLine(0, ui::Em(0.6f));
            ImGui::SetNextItemWidth(ui::Em(22.f));
            if (ImGui::BeginCombo("##wz_tmpl", cur)) {
                for (const auto& t : templates) {
                    if (ImGui::Selectable(t.name, t.kind == tmpl.kind))
                        wizard_.templateIdx = static_cast<int>(t.kind);
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();
        }

        // Shared fields: event id, script name, trigger tile + condition.
        {
            ImGui::SetNextItemWidth(ui::Em(6.f));
            ImGui::InputInt("Event id", &wizard_.eventId);
            char sn[40];
            std::snprintf(sn, sizeof(sn), "event_%02d", wizard_.eventId);
            ImGui::Text("Script name");
            ImGui::SameLine(0, ui::Em(0.6f));
            ImGui::SetNextItemWidth(ui::Em(16.f));
            ImGui::InputText("##wz_sn", wizard_.scriptName, sizeof(wizard_.scriptName));
            std::string want = "event_%02d";
            (void)sn;
            ImGui::InputInt("Tile Y", &wizard_.tileY);
            ImGui::SetNextItemWidth(ui::Em(9.f));
            ImGui::InputInt("Tile X", &wizard_.tileX);

            int nCond = 0;
            const char* const* conds = wizardCondNames(&nCond);
            ImGui::Text("Condition");
            ImGui::SameLine(0, ui::Em(0.6f));
            ImGui::SetNextItemWidth(ui::Em(14.f));
            if (ImGui::BeginCombo("##wz_cond", conds[wizard_.condIndex])) {
                for (int i = 0; i < nCond; ++i)
                    if (ImGui::Selectable(conds[i], i == wizard_.condIndex))
                        wizard_.condIndex = i;
                ImGui::EndCombo();
            }
            ImGui::Spacing();
            ImGui::Separator();
        }

        // Template-specific form.
        auto& f = wizard_.form;
        switch (tmpl.kind) {
            case WizardTemplateKind::DoorSign: {
                static const char* vars[] = {"Door", "Block", "Popup A", "Basic"};
                ImGui::Combo("Say type", &f.sayVariant, vars, 4);
                ui::TextInput("Message", f.message, ImVec2(ui::Em(38.f), ui::Em(4.f)));
                break;
            }
            case WizardTemplateKind::DialogueReward: {
                static const char* vars[] = {"Door", "Block", "Popup A", "Basic"};
                ImGui::Combo("Say type", &f.sayVariant, vars, 4);
                ui::TextInput("Message", f.message, ImVec2(ui::Em(38.f), ui::Em(4.f)));
                ImGui::Checkbox("End with quest handler", &f.withQuest);
                ImGui::BeginDisabled(f.withQuest);
                ImGui::InputInt("Reward item id", &f.itemId);
                if (f.itemId >= 0 && f.itemId < 256) {
                    std::string n = app.itemName(f.itemId);
                    if (!n.empty()) ImGui::TextDisabled("%s", n.c_str());
                }
                ImGui::EndDisabled();
                break;
            }
            case WizardTemplateKind::GoldToll:
            case WizardTemplateKind::GemToll:
            case WizardTemplateKind::Trap: {
                ImGui::InputInt(tmpl.kind == WizardTemplateKind::Trap ? "Damage value" : "Amount",
                                &f.amount);
                ui::TextInput("Pay message", f.payMsg, ImVec2(ui::Em(38.f), ui::Em(2.f)));
                if (tmpl.kind != WizardTemplateKind::Trap)
                    ui::TextInput("Refuse message", f.refuseMsg, ImVec2(ui::Em(38.f), ui::Em(2.f)));
                if (tmpl.kind == WizardTemplateKind::Trap)
                    ImGui::InputInt("Member (0=all)", &f.trapMember);
                else
                    ImGui::Checkbox("Clear tile after pass", &f.clearTile);
                break;
            }
            case WizardTemplateKind::ItemGate: {
                ImGui::InputInt("Required item id", &f.itemId);
                if (f.itemId >= 0 && f.itemId < 256) {
                    std::string n = app.itemName(f.itemId);
                    if (!n.empty()) ImGui::TextDisabled("%s", n.c_str());
                }
                ui::TextInput("Open message", f.payMsg, ImVec2(ui::Em(38.f), ui::Em(2.f)));
                ui::TextInput("Hint message", f.refuseMsg, ImVec2(ui::Em(38.f), ui::Em(2.f)));
                ImGui::Checkbox("Clear tile after pass", &f.clearTile);
                break;
            }
            case WizardTemplateKind::ServiceShop: {
                ImGui::InputInt("Sign index", &f.signIndex);
                ImGui::InputInt("Mode", &f.serviceMode);
                ImGui::InputInt("Selector byte (0=none)", &f.shopSelector);
                break;
            }
            case WizardTemplateKind::Encounter: {
                ImGui::Text("Monster ids (hex), space separated:");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("e.g. 0x01 0x14");
                static char mbuf[128] = "0x01";
                ImGui::InputText("##wz_monsters", mbuf, sizeof(mbuf));
                f.monsters.clear();
                std::istringstream iss(mbuf);
                std::string tok;
                while (iss >> tok) {
                    int v = 0;
                    try {
                        if (tok.size() > 2 && (tok[1] == 'x' || tok[1] == 'X'))
                            v = static_cast<int>(std::stoul(tok.substr(2), nullptr, 16));
                        else
                            v = std::stoi(tok);
                    } catch (...) {
                    }
                    f.monsters.push_back(v);
                }
                break;
            }
            case WizardTemplateKind::Transition: {
                ImGui::InputInt("Screen", &f.screen);
                if (f.screen >= 0 && f.screen < 256) {
                    const char* an = areaNameRaw(f.screen);
                    if (an && an[0]) ImGui::TextDisabled("%s", an);
                }
                ImGui::InputInt("Pos (hex)", &f.pos);
                break;
            }
            case WizardTemplateKind::Riddle: {
                ui::TextInput("Question", f.question, ImVec2(ui::Em(38.f), ui::Em(3.f)));
                ui::TextInput("Answer", f.answer);
                break;
            }
            default:
                break;
        }

        ImGui::Spacing();
        ImGui::Separator();

        const bool nameOk = std::string(wizard_.scriptName).find(' ') == std::string::npos &&
                            wizard_.scriptName[0] != '\0';
        if (ImGui::Button("Insert", ImVec2(ui::Em(8.f), 0)) && nameOk) {
            auto itemNameOf = [&app](int id) { return app.itemName(id); };
            auto monsterNameOf = [&app](int id) { return app.monsterName(id); };
            WizardSnippet snip = buildWizardSnippet(
                tmpl.kind, wizard_.eventId, wizard_.scriptName, wizard_.tileY, wizard_.tileX,
                wizard_.condIndex, itemNameOf, monsterNameOf, f);
        // Merge into buffer + rebuild editor.
        scriptBuf_ = mergeWizardSnippet(scriptEditor_.getText(), snip);
        scriptEditor_.setText(scriptBuf_, selectedLoc_);
        scriptDirty_ = true;
        compileError_.clear();
        compileErrorLine_ = -1;
        compileOkMsg_.clear();
        wizardOpen_ = false;
        wizardTargetEvent_ = wizard_.eventId;
        ImGui::CloseCurrentPopup();
        }
        if (!nameOk)
            ImGui::SetTooltip("Script name must be a single token with no spaces");
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(ui::Em(8.f), 0))) {
            wizardOpen_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EventSection::drawTriggerEditor(EditorSelection& sel) {
    ui::PanelHeader("Trigger", "editing");
    if (sel.index < 0 || sel.index >= static_cast<int>(ast_.triggers.size())) {
        ImGui::TextDisabled("(no trigger selected)");
        return;
    }
    eventlang::Trigger& t = ast_.triggers[sel.index];

    // 16x16 tile picker (mini-map sense check; editing located at selected tile).
    ImGui::TextDisabled("Tile");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ui::Em(4.f));
    ImGui::InputInt("##trig_y", &t.y, 1, 1);
    ImGui::SameLine();
    ImGui::TextDisabled("x");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ui::Em(4.f));
    ImGui::InputInt("##trig_x", &t.x, 1, 1);

    int nCond = 0;
    const char* const* conds = wizardCondNames(&nCond);
    // Map TriggerCond enum to combo index (keep order consistent with wizard).
    static const eventlang::TriggerCond kOrder[] = {
        eventlang::TriggerCond::Always,      eventlang::TriggerCond::Enter,
        eventlang::TriggerCond::FromNorth,   eventlang::TriggerCond::DirSpecial,
        eventlang::TriggerCond::AnyDirection, eventlang::TriggerCond::FacingNs,
        eventlang::TriggerCond::EnterSpecial};
    int combo = 0;
    for (int i = 0; i < nCond; ++i)
        if (kOrder[i] == t.cond) combo = i;
    ImGui::Text("Condition");
    if (ImGui::BeginCombo("##trig_cond", conds[combo])) {
        for (int i = 0; i < nCond; ++i)
            if (ImGui::Selectable(conds[i], i == combo)) {
                t.cond = kOrder[i];
                t.condRaw = static_cast<uint8_t>(0);
            }
        ImGui::EndCombo();
    }

    // Target script combo.
    ImGui::Text("Target");
    char cur[32];
    std::snprintf(cur, sizeof(cur), "event_%02d", t.eventId);
    if (ImGui::BeginCombo("##trig_target", cur)) {
        std::set<int> ids;
        for (const auto& tt : ast_.triggers) ids.insert(tt.eventId);
        for (const auto& sc : ast_.scripts) ids.insert(sc.eventId);
        for (int id : ids) {
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "event_%02d", id);
            if (ImGui::Selectable(lbl, id == t.eventId)) t.eventId = id;
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    if (ImGui::Button("Apply trigger")) {
        regenerateFromAst();
        sel.Select(DocKind::Events, EditorSelection::Kind::EventNode, selectedLoc_, t.eventId);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete trigger")) {
        ast_.triggers.erase(ast_.triggers.begin() + sel.index);
        regenerateFromAst();
        sel.Select(DocKind::Events, EditorSelection::Kind::EventLoc, selectedLoc_, selectedEvent_);
    }
}

void EventSection::drawStringEditor(EditorSelection& sel) {
    ui::PanelHeader("String", "editing");
    if (sel.index < 0 || sel.index >= static_cast<int>(ast_.strings.size())) {
        ImGui::TextDisabled("(no string selected)");
        return;
    }
    eventlang::StringDef& sd = ast_.strings[sel.index];
    ImGui::TextDisabled("name: %s", sd.name.c_str());
    ImGui::Spacing();

    if (propsStringEditIdx_ != sel.index) {
        propsStringEditIdx_ = sel.index;
        propsStringBuf_ = sd.text;
    }
    const bool edited = ui::TextInput("Text", propsStringBuf_, ImVec2(-FLT_MIN, ui::Em(8.f)));
    if (edited) {
        sd.text = propsStringBuf_;
        sd.rawBytes.clear();
    }

    // Raw-bytes hex peek.
    if (!sd.rawBytes.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Raw bytes:");
        std::string hex;
        for (uint8_t b : sd.rawBytes) {
            char h[4];
            std::snprintf(h, sizeof(h), "%02x ", b);
            hex += h;
        }
        ImGui::TextWrapped("%s", hex.c_str());
    }
}

void EventSection::drawProperties(App& app, EditorSelection& sel) {
    if (!loaded) {
        ui::EmptyState("Not loaded", "event.dat missing from the data folder");
        return;
    }
    if (sel.kind == EditorSelection::Kind::EventTrigger) {
        if (structuredEditsAllowed()) {
            drawTriggerEditor(sel);
        } else {
            ImGui::TextWrapped("Structured trigger editing is disabled for %s records.",
                               eventlang::recordKindName(ast_.recordKind));
            ImGui::Spacing();
            ui::EmptyState("Read-only", "Edit this record's text in the Script tab instead.");
        }
        return;
    }
    if (sel.kind == EditorSelection::Kind::EventString) {
        if (structuredEditsAllowed()) {
            drawStringEditor(sel);
        } else {
            ImGui::TextWrapped("Structured string editing is disabled for %s records.",
                               eventlang::recordKindName(ast_.recordKind));
            ImGui::Spacing();
            ui::EmptyState("Read-only", "Edit this record's text in the Script tab instead.");
        }
        return;
    }

    if (graph_.selectedNodeId >= 0 && graph_.findNode(graph_.selectedNodeId))
        drawOpcodeInspector(app, sel);
    else
        drawScriptSummary(app);
}

}  // namespace mm2
