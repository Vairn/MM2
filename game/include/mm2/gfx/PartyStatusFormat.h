#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm2_roster_codec.h"

// Party status line layout traced from draw_party_status_panel @ 0x6150:
//   prefix " n)" (0x6204..0x6226) + 11-byte roster name field + " /" (0x6254..0x6266)
//   + HP word roster +$5E (0x6274); CMPI #$03E7 @ 0x627A → "+++" @ 0x62EC else
//   3-cell decimal @ 0x628E..0x62C2 (right-aligned; leading spaces when HP < 100 / < 10).
// Combat monster-list check glyph 0x7E is drawn only in the right-column roster
// via combat_monster_line @ 0x1374A (A4-$702a), not on the party panel @ 0x6150.

namespace mm2::gfx {

constexpr int kPartyNameFieldWidth = MM2_ROSTER_NAME_SIZE;

/** HP > 999 → "+++"; else 3-char decimal field right-aligned (e.g. "  7", " 16", "999"). */
void formatPartyHpField(uint16_t hp, char *out, size_t cap);

enum class PartyStatusPrefix : uint8_t {
    Exploration,     /* " n)" @ 0x6204 — leading space, digit, ')' */
    CombatCheckmark, /* glyph 0x7E — combat_monster_line @ 0x1374A (monster rows only) */
};

/** Full line: prefix + name padded/truncated to 11 + " /" + HP field. Returns strlen(out). */
size_t formatPartyStatusLine(char *out, size_t cap, int slot_index, const char *name, uint16_t hp,
                             PartyStatusPrefix prefix_style = PartyStatusPrefix::Exploration);

/** Quick Ref + character sheet: right-align current in [0, field_width) for a fixed '/' column. */
void formatSlashStatCurrent(uint16_t value, char *out, size_t cap, int field_width);

/** Write "current/max" with aligned slash (no space after '/'). Returns strlen(out). */
size_t formatSlashStatPair(char *out, size_t cap, uint16_t current, uint16_t max, int field_width);

}  // namespace mm2::gfx
