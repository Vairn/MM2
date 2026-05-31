#pragma once
// A Section is a self-contained editor for one MM2 data file (or topic).
// To add a new data type: subclass Section, implement the virtuals, and
// register it in App::registerSections().

#include <string>

namespace mm2 {

class App;

class Section {
public:
    virtual ~Section() = default;

    // Sidebar label, e.g. "Items".
    virtual const char* title() const = 0;
    // Backing file name relative to the data folder, e.g. "items.dat".
    // Return "" for sections with no single backing file.
    virtual const char* fileName() const = 0;

    // Load/save the backing file from/to dataDir. Return false on failure.
    virtual bool load(const std::string& dataDir) = 0;
    virtual bool save(const std::string& dataDir) = 0;

    // Render the section's main panel content (inside an existing window).
    virtual void draw(App& app) = 0;

    // Release GPU resources queued for end-of-frame deletion (no-op by default).
    virtual void flushPending() {}

    bool loaded = false;
    bool dirty = false;
};

}  // namespace mm2
