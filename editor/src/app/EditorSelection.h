#pragma once

#include <string>

#include "app/DocKind.h"

namespace mm2 {

struct EditorSelection {
    enum class Kind {
        None = 0,
        Item,
        Monster,
        Spell,
        RosterChar,
        StringLine,
        MapScreen,
        MapTile,
        AttribScreen,
        EventLoc,
        EventNode,      // script: index = eventId
        EventTrigger,   // index = trigger index in location AST
        EventString,    // index = string index in location AST
        GfxFile,
    };

    DocKind doc = DocKind::None;
    Kind kind = Kind::None;
    int index = -1;       // primary id (item #, screen #, event id, …)
    int secondary = -1;   // e.g. tile index, overlay event
    std::string requestInnerTab;

    void Clear() {
        doc = DocKind::None;
        kind = Kind::None;
        index = -1;
        secondary = -1;
        requestInnerTab.clear();
    }

    void Select(DocKind d, Kind k, int idx, int sec = -1) {
        doc = d;
        kind = k;
        index = idx;
        secondary = sec;
    }

    void RequestTab(const char* tab) { requestInnerTab = tab ? tab : ""; }

    bool empty() const { return kind == Kind::None; }
};

}  // namespace mm2
