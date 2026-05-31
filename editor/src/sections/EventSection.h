#pragma once
// Structured node-graph editor for event.dat, built on imnodes.
//
// Each location is a graph of: trigger nodes (tiles that fire an event id) ->
// event-script nodes (the decoded opcode chain, with editable argument bytes)
// -> string nodes (text-table entries referenced by show_text opcodes).
// Argument-byte edits write back into the in-memory file (length-preserving)
// and persist on Save.

#include <string>

#include "app/Section.h"
#include "core/EventFile.h"

namespace mm2 {

class EventSection : public Section {
public:
    const char* title() const override { return "Events"; }
    const char* fileName() const override { return "event.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

private:
    void drawGraph(App& app, EventLocation& loc);

    EventFile file_;
    int selectedLoc_ = 0;
    int layoutForLoc_ = -1;   // location whose initial node layout was applied
    bool showStrings_ = true;
};

}  // namespace mm2
