#pragma once
// One panel per MM2 data file (or topic). Registered in App::registerSections().

#include <string>

#include "app/DocKind.h"
#include "app/EditorSelection.h"

namespace mm2 {

class App;

class Section {
public:
    virtual ~Section() = default;

    virtual DocKind docKind() const = 0;

    // Sidebar / tab label, e.g. "Items".
    virtual const char* title() const { return DocKindTitle(docKind()); }
    // Backing file name relative to the data folder, e.g. "items.dat".
    // Return "" for sections with no single backing file.
    virtual const char* fileName() const = 0;

    virtual bool isReadOnly() const { return DocKindIsReadOnly(docKind()); }

    // Load/save the backing file from/to dataDir. Return false on failure.
    virtual bool load(const std::string& dataDir) = 0;
    virtual bool save(const std::string& dataDir) = 0;

    // Workspace content (lists, canvas, script editor, inner tabs).
    virtual void drawWorkspace(App& app, EditorSelection& sel) = 0;
    // Selection-driven inspector (Properties dock). Default: empty hint.
    virtual void drawProperties(App& app, EditorSelection& sel) {
        (void)app;
        (void)sel;
    }

    // True when in-memory edit buffer differs from compiled/saved form (Events).
    virtual bool hasUnsavedBuffer() const { return false; }
    // Extra status for the status bar (e.g. "script dirty").
    virtual const char* bufferStatus() const { return nullptr; }

    // Release GPU resources queued for end-of-frame deletion (no-op by default).
    virtual void flushPending() {}

    // Jump helpers used by cross-doc links (Map → Events).
    virtual void focusIndex(int index) { (void)index; }

    bool loaded = false;
    bool dirty = false;
};

}  // namespace mm2
