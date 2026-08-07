#pragma once
// Location-level opcode graph: one horizontal chain per script.
// Flow is left → right: trigger → header → opcodes. If-branches drop the
// else arm below (then continues on the main row). Scripts stack vertically.
// Cross-script overlay/selector edges connect nodes across chains.

#include <functional>
#include <string>
#include <vector>

#include "eventlang/Ast.h"
#include "imgui.h"

namespace mm2 {

class App;

enum class StmtListKind : int { Body = 0, Then = 1, Else = 2 };

struct StmtPathElem {
    StmtListKind list = StmtListKind::Body;
    int index = 0;
};

using StmtPath = std::vector<StmtPathElem>;

struct GraphLink {
    int id = 0;
    int fromPin = 0;
    int toPin = 0;
};

enum class GraphNodeCategory {
    Dialogue,
    Economy,
    Combat,
    Control,
    World,
    Other,
    Trigger,
    Header,
};

struct GraphNode {
    int id = 0;
    StmtPath path;
    std::string kind;
    std::string title;
    std::string subtitle;
    GraphNodeCategory category = GraphNodeCategory::Other;
    ImVec2 pos{0, 0};
    ImVec2 size{200.f, 80.f};  // estimated or measured (ImNodes::GetNodeDimensions)
    int inPin = 0;
    int outPin = 0;       // sequential / then
    int elseOutPin = -1;  // if nodes only
    bool isIf = false;
    // Chain/script this node belongs to (for column layout & Properties).
    int eventId = -1;
    int chainIndex = -1;  // vertical row index in the location graph
    // If >=0 this is a trigger-header node; eventId is the target script.
    bool isTrigger = false;
    // target overlay location/event for cross-script links (-1 = none).
    int overlayLoc = -1;
    int overlayEvent = -1;
};

struct EventGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphLink> links;
    int selectedNodeId = -1;
    int builtForEvent = -1;  // focused script (single)
    int builtForLoc = -1;
    int lastFocusedEvent = -1;
    bool layoutDirty = true;
    int layoutRefinePasses = 0;
    int nextLinkId = 1;
    // Per-script caches for rebuild reuse.
    float columnX[8];
    int scriptColumn[8];

    void clear();
    void rebuild(const eventlang::Location& loc, int locId, int focusEvent,
                 const std::vector<int>& scriptOrder, App* app);
    /** Re-pack node positions using measured/estimated `GraphNode::size`. */
    void reflow(const eventlang::Location& loc);
    const GraphNode* findNode(int id) const;
    GraphNode* findNode(int id);
    const GraphNode* findNodeForEvent(int eventId) const;
    const GraphNode* findEventHeader(int eventId) const;
    GraphNode* findNodeByPath(int eventId, const StmtPath& path);

    // Resolve a mutable Stmt* for a path within `sc`. Null if invalid.
    static eventlang::Stmt* resolveStmt(eventlang::Script& sc, const StmtPath& path);
    static const eventlang::Stmt* resolveStmt(const eventlang::Script& sc, const StmtPath& path);

    static bool insertAfter(eventlang::Script& sc, const StmtPath& path,
                            const eventlang::Stmt& stub);
    static bool deleteAt(eventlang::Script& sc, const StmtPath& path);
    /** Append a statement to the end of the script body. */
    static bool appendStmt(eventlang::Script& sc, const eventlang::Stmt& stub);
    /** Build a default stub for the Add Node menu. */
    static eventlang::Stmt makeStub(const char* kind);

    /** Pending cross-UI action set by the context menu (cleared by EventSection). */
    enum class PendingAction {
        None = 0,
        ShowOnMap,
        NavigateOverlay,
        JumpEvent,
    };
    PendingAction pendingAction = PendingAction::None;
    int pendingLoc = -1;
    int pendingEvent = -1;
    int pendingTileY = -1;
    int pendingTileX = -1;

    /** Draw the imnodes canvas.
     *  selectionChanged / astMutated report what happened this frame.
     *  focusedEvent is updated on click so Properties follows.
     *  loc non-const because the context menu mutates scripts. */
    void draw(App& app, eventlang::Location& loc, eventlang::Script* focusScript,
              bool readOnly, bool allowMutate, int* focusedEvent, bool* selectionChanged,
              bool* astMutated);
};

GraphNodeCategory categorizeStmt(const std::string& kind);
ImU32 categoryTitleColor(GraphNodeCategory c);
ImU32 categoryBgColor(GraphNodeCategory c);

/** Human label + resolved subtitle (item names, string previews). */
void describeStmt(const eventlang::Stmt& st, const eventlang::Location& loc, App* app,
                  std::string* titleOut, std::string* subtitleOut);

/** One-line summary of a whole script for header nodes (title + subtitle). */
void summarizeScript(const eventlang::Script& sc, const eventlang::Location& loc, App* app,
                     std::string* titleOut, std::string* subtitleOut);

/** Decode fight / fight_b spawn list into human title + detail.
 *  Title is "Fixed fight" or "Random pool"; detail lists counts × names. */
void describeFightEncounter(const eventlang::Stmt& st, App* app, std::string* titleOut,
                            std::string* detailOut);

}  // namespace mm2