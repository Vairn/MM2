#pragma once
// Event scripting toolkit: outline | .mm2evt editor | inspector.

#include <string>
#include <vector>

#include "app/Section.h"
#include "core/EventFile.h"
#include "eventlang/Ast.h"
#include "sections/eventgraph/EventGraph.h"
#include "sections/eventwizard/EventWizard.h"
#include "widgets/Mm2EvtEditor.h"

namespace mm2 {

class App;

class EventSection : public Section {
public:
    DocKind docKind() const override { return DocKind::Events; }
    const char* fileName() const override { return "event.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;
    bool hasUnsavedBuffer() const override { return scriptDirty_; }
    const char* bufferStatus() const override;
    void focusIndex(int index) override;
    void setApp(App* app) { app_ = app; }

private:
    void refreshAst();
    void syncScriptBuffer();
    bool compileFromScript(App& app);
    void exportDsl(App& app);
    void importDsl(App& app);
    void applyAstToFile(App& app);
    void ensureSelectedEvent();
    void selectEvent(int eventId);
    /** Switch location + event to an overlay bank target. */
    void openOverlay(int locId, int eventId);
    bool confirmDiscardBuffer(const char* action);
    void jumpToError();
    void tryOpenOverlayFromEditorClick();
    void drawToolbar(App& app);
    void drawOutline(EditorSelection& sel);
    void drawEditor();
    void drawGraph(App& app, EditorSelection& sel);
    void rebuildGraph(App& app);
    void commitAstEdit(App& app);
    void drawOpcodeInspector(App& app, EditorSelection& sel);
    void drawScriptSummary(App& app);
    void drawStmtTree(const std::vector<eventlang::Stmt>& stmts, int depth);
    void drawProblems();
    void drawWizard(App& app);
    void openWizardModal();
    void drawTriggerEditor(EditorSelection& sel);
    void drawStringEditor(EditorSelection& sel);
    bool structuredEditsAllowed() const;
    void regenerateFromAst();
    eventlang::Script* selectedScript();
    eventlang::Script* scriptByEvent(int eventId);
    /** Select event_MM in Outline and focus the Graph tab. */
    void jumpToEvent(int eventId);

    EventFile file_;
    eventlang::Location ast_;
    std::string scriptBuf_;
    std::string compileError_;
    std::string compileOkMsg_;
    Mm2EvtEditor scriptEditor_;
    EventGraph graph_;
    App* app_ = nullptr;
    int selectedLoc_ = 0;
    int astLoc_ = -1;
    int selectedEvent_ = -1;
    int compileErrorLine_ = -1;  // 0-based
    bool scriptDirty_ = false;
    bool outlineFilterScripts_ = true;
    float problemsH_ = 0.f;
    bool wizardOpen_ = false;
    WizardState wizard_;
    int wizardTargetEvent_ = -1;     // jump target after wizard insert (-1 none)
    int propsStringEditIdx_ = -1;      // string being edited in Properties (-1 none)
    std::string propsStringBuf_;       // edit buffer for propsStringEditIdx_
};

}  // namespace mm2
