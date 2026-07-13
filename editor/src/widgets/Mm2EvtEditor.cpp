#include "widgets/Mm2EvtEditor.h"

#include <string_view>

#include "core/Mm2BitmapFont.h"
#include "imgui.h"

namespace mm2 {

const TextEditor::Language* Mm2EvtEditor::language() {
    static TextEditor::Language lang = [] {
        TextEditor::Language l = *TextEditor::Language::Python();
        l.name = "mm2evt";
        l.caseSensitive = true;
        l.preprocess = '@';
        l.singleLineComment = "#";
        l.singleLineCommentAlt.clear();
        l.commentStart.clear();
        l.commentEnd.clear();
        l.hasDoubleQuotedStrings = true;
        l.hasSingleQuotedStrings = false;
        l.stringEscape = '\\';
        l.keywords.clear();
        l.declarations.clear();
        l.identifiers.clear();
        const char* kws[] = {
            "location",     "record",         "strings",         "script",
            "on",           "tile",           "facing",          "from",
            "north",        "enter",          "always",          "any_direction",
            "when",         "if",             "else",            "say",
            "say_door",     "say_block",      "say_popup",       "service_title",
            "wait",         "ask",            "yes_no",          "space",
            "key",          "abort",          "end",             "selector",
            "shop",         "quest",          "go_to",           "screen",
            "pos",          "fight",          "fight_b",         "set_tile",
            "clear_tile_event", "clear_input", "read_answer",    "delay",
            "load_var8",    "store_var8",     "set",             "quest_complete",
            "quest_flag",   "gold",           "class",           "class_field",
            "has_item",     "answer",         "code16",          "era",
            "day",          "odd",            "even",            "party_has_item",
            "count_title_nibble", "monster_present", "in",       "mask",
            "mode",         "group",          "index",           "value",
            "monsters",     "flags",          "raw_record",      "standard",
            "string_bank",  "mixed_pool",     "castle_blob",     "ns",
            "special",      "dir",            "probe",           "count",
            "op",           "and",            "or",              "event",
            "apply_party",  "engine_call",    "sign",            "overlay",
            "engine",       "shop",           "quest",
        };
        for (const char* k : kws) l.keywords.insert(k);
        return l;
    }();
    return &lang;
}

Mm2EvtEditor::Mm2EvtEditor() {
    editor_.SetLanguage(language());
    editor_.SetPalette(TextEditor::GetDarkPalette());
    editor_.SetShowWhitespacesEnabled(false);
    // TextEditor multiplies GetTextLineHeightWithSpacing(); keep at 1.0 and
    // shrink ItemSpacing around Render so UI chrome spacing doesn't double lines.
    editor_.SetLineSpacing(1.0f);
}

void Mm2EvtEditor::setText(const std::string& text, int locId) {
    if (syncedLoc_ == locId && editor_.GetText() == text) return;
    editor_.SetText(text);
    editor_.SetLanguage(language());
    syncedLoc_ = locId;
}

void Mm2EvtEditor::goToLine(int line0) {
    if (line0 < 0) return;
    pendingGoToLine_ = line0;
}

int Mm2EvtEditor::findLine(const std::string& needle) const {
    if (needle.empty()) return -1;
    const std::string text = editor_.GetText();
    size_t pos = 0;
    int line = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string_view row(text.data() + pos, end - pos);
        if (row.find(needle) != std::string_view::npos) return line;
        if (end == text.size()) break;
        pos = end + 1;
        ++line;
    }
    return -1;
}

bool Mm2EvtEditor::draw(const char* title, const ImVec2& size) {
    if (pendingGoToLine_ >= 0) {
        editor_.SetCursor(pendingGoToLine_, 0);
        editor_.ScrollToLine(pendingGoToLine_, TextEditor::Scroll::alignMiddle);
        pendingGoToLine_ = -1;
    }
    // Compact vertical metrics: TextEditor uses GetTextLineHeightWithSpacing(),
    // which includes ItemSpacing.y from the scaled UI chrome.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 1.f));
    ImFont* code = mm2CodeFont();
    if (code)
        ImGui::PushFont(code, code->LegacySize);
    else
        ImGui::PushFont(nullptr, 20.f);
    const size_t before = editor_.GetUndoIndex();
    editor_.Render(title, size, true);
    const bool changed = editor_.GetUndoIndex() != before;
    ImGui::PopFont();
    ImGui::PopStyleVar();
    return changed;
}

}  // namespace mm2
