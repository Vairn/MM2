#pragma once
// .mm2evt script editor with syntax highlighting (goossens ImGuiColorTextEdit).

#include <cstddef>
#include <string>

#include "TextEditor.h"

namespace mm2 {

class Mm2EvtEditor {
public:
    Mm2EvtEditor();

    void setText(const std::string& text, int locId);
    std::string getText() const { return editor_.GetText(); }
    size_t undoIndex() const { return editor_.GetUndoIndex(); }

    /** Jump to 0-based line and focus caret (call before draw). */
    void goToLine(int line0);
    /** Find first line containing needle; returns 0-based line or -1. */
    int findLine(const std::string& needle) const;
    void clearErrorMarkers();
    void markErrorLine(int line0, const char* tip = nullptr);

    bool draw(const char* title, const ImVec2& size);

    TextEditor& raw() { return editor_; }

private:
    TextEditor editor_;
    int syncedLoc_ = -1;
    int pendingGoToLine_ = -1;
    static const TextEditor::Language* language();
};

}  // namespace mm2
