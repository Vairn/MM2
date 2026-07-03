#pragma once

#include "mm2_roster_codec.h"

namespace mm2::ui {

/** Empty secondary-skill row on the character sheet (no spaces between dots). */
inline constexpr char kRosterEmptySkillSlot[] = "............";

/** Thievery % for character-sheet draw (roster+$1E @ character_sheet_draw). */
uint8_t rosterDisplayThievery(const Mm2RosterRecord &rec);

/** Decode roster secondary_skills nibbles into display names (max 6). */
int collectRosterSkillNames(const Mm2RosterRecord &rec, const char **names, int max_names);

/** FAQ hireling preset skills for roster slots >= kRosterHirelingPageOffset. */
int collectHirelingSkillNames(int hireling_index, const char **names, int max_names);

}  // namespace mm2::ui
