#pragma once
#include "app/Section.h"
#include "core/RosterFile.h"

namespace mm2 {

class RosterSection : public Section {
public:
    const char* title() const override { return "Roster / Party"; }
    const char* fileName() const override { return "roster.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

private:
    void drawCharacterSheet(RosterRecord& r);
    void drawStats(RosterRecord& r);
    void drawEquipment(App& app, RosterRecord& r);
    void drawSpells(RosterRecord& r);
    void drawGlobalOverlay();

    RosterFile file_;
    int selected_ = 0;
};

}  // namespace mm2
