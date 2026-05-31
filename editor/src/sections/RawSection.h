#pragma once
// Generic hex/raw editor for formats not yet structurally edited here.
// Used for spells.dat (layout unknown) and event.dat (container decoded by
// tools/decode_event.py, but edited as raw bytes in the GUI for now).

#include "app/Section.h"
#include "core/RawFile.h"

namespace mm2 {

class RawSection : public Section {
public:
    RawSection(const char* title, const char* fileName, const char* note)
        : title_(title), fileName_(fileName), note_(note) {}

    const char* title() const override { return title_; }
    const char* fileName() const override { return fileName_; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

private:
    const char* title_;
    const char* fileName_;
    const char* note_;
    RawFile file_;
};

}  // namespace mm2
