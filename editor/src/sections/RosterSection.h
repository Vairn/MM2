#pragma once
#include "app/Section.h"
#include "core/RosterFile.h"
#include "widgets/UiLayout.h"

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
    void drawHirelingUnlocks();

    // Event bank g=0x00..0x17 in the roster global tail (A4-$798B).
    uint8_t hirelingUnlockByte(int letterIndex) const;
    void setHirelingUnlock(int letterIndex, bool found);

    RosterFile file_;
    int selected_ = 0;
    ui::MasterDetail layout_;
};

}  // namespace mm2
