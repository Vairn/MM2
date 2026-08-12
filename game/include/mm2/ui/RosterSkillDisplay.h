#pragma once

#include "mm2_roster_codec.h"

namespace mm2::ui {

/** A4-$8720[0] empty-skill glyph run printed by LAB_38EA fields $14/$15. */
inline constexpr char kRosterEmptySkillSlot[] = "............";

/** Thievery % for character-sheet draw (roster+$1E @ character_sheet_draw). */
uint8_t rosterDisplayThievery(const Mm2RosterRecord &rec);

/** A4-$8720[id] for LAB_38EA — id 0 is the 12-dot empty slot. */
const char *rosterSheetSkillName(uint8_t id);

/** Decode roster secondary_skills nibbles into display names (max 6). */
int collectRosterSkillNames(const Mm2RosterRecord &rec, const char **names, int max_names);

/** FAQ hireling preset skills for roster slots >= kRosterHirelingPageOffset. */
int collectHirelingSkillNames(int hireling_index, const char **names, int max_names);

}  // namespace mm2::ui
