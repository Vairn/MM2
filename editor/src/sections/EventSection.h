#pragma once
// Event scripting toolkit: outline | .mm2evt editor | inspector.

#include <string>
#include <vector>

#include "app/Section.h"
#include "core/EventFile.h"
#include "eventlang/Ast.h"
#include "widgets/Mm2EvtEditor.h"

namespace mm2 {

class App;

class EventSection : public Section {
public:
    const char* title() const override { return "Events"; }
    const char* fileName() const override { return "event.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

private:
    void refreshAst();
    void syncScriptBuffer();
    bool compileFromScript(App& app);
    void exportDsl(App& app);
    void importDsl(App& app);
    void applyAstToFile(App& app);
    void ensureSelectedEvent();
    void selectEvent(int eventId);
    /** Switch location + event to an overlay bank target (discards unsaved buffer). */
    void openOverlay(int locId, int eventId);
    void jumpToError();
    void tryOpenOverlayFromEditorClick();
    void drawToolbar(App& app);
    void drawOutline();
    void drawEditor();
    void drawInspector();
    void drawStmtTree(const std::vector<eventlang::Stmt>& stmts, int depth);
    void drawProblems();

    EventFile file_;
    eventlang::Location ast_;
    std::string scriptBuf_;
    std::string compileError_;
    std::string compileOkMsg_;
    Mm2EvtEditor scriptEditor_;
    int selectedLoc_ = 0;
    int astLoc_ = -1;
    int selectedEvent_ = -1;
    int compileErrorLine_ = -1;  // 0-based
    bool scriptDirty_ = false;
    bool outlineFilterScripts_ = true;
    float outlineW_ = 0.f;     // 0 => default em width
    float inspectorW_ = 0.f;   // 0 => default em width
    float problemsH_ = 0.f;    // 0 => auto when visible
};

}  // namespace mm2
