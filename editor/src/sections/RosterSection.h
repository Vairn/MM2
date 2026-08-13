#pragma once
#include "app/Section.h"
#include "core/RosterFile.h"
#include "widgets/UiLayout.h"

namespace mm2 {

class RosterSection : public Section {
public:
    DocKind docKind() const override { return DocKind::Roster; }
    const char* fileName() const override { return "roster.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;

private:
    void drawCharacterSheet(RosterRecord& r);
    void drawStats(RosterRecord& r);
    void drawEquipment(App& app, RosterRecord& r);
    void drawSpells(RosterRecord& r);
    void drawGlobalOverlay();
    void drawHirelingUnlocks();
    void drawRosterDetail(App& app);

    uint8_t hirelingUnlockByte(int letterIndex) const;
    void setHirelingUnlock(int letterIndex, bool found);

    RosterFile file_;
    int selected_ = 0;
    ui::MasterDetail layout_;
};

}  // namespace mm2
