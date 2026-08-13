#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm2_roster_codec.h"

// Party status line layout traced from party_status_panel_redraw @ 0x6150:
//   putchar ' ' + digit + ')' + ' ' + win_print(name until NUL) + ' ' + '/' + HP.
//   Name is NOT padded to 11 — short names shift '/' and HP left (byte-exact).
//   HP word roster +$5E (0x624C); CMPI #$03E7 @ 0x6252 → "+++" @ 0x62C4 else
//   3-cell decimal @ 0x6266..0x629A (left-aligned; trailing spaces when HP < 100 / < 10).
// Combat strip (combat_message_helper @ 0x12848) differs: prefix glyph 0x17
// (check) when slot < front-rank cutoff A4-$5E4D (0x12892) else space; HP
// digits stay left-aligned with trailing spaces (0x1291E..0x12956).

namespace mm2::gfx {

/** Max roster name bytes (field size); party strip prints until first NUL. */
constexpr int kPartyNameFieldWidth = MM2_ROSTER_NAME_SIZE;

/** HP > 999 → "+++"; else 3-char decimal field left-aligned (e.g. "7  ", "16 ", "999"). */
void formatPartyHpField(uint16_t hp, char *out, size_t cap);

enum class PartyStatusPrefix : uint8_t {
    Exploration,     /* " n)" @ 0x6204 — leading space, digit, ')' */
    CombatFrontRank, /* glyph 0x17 check — slot < A4-$5E4D (0x12898) */
    CombatBackRank,  /* space — combat strip, not in melee reach (0x128A4) */
};

/** Full line: prefix + name until NUL + " /" + HP field. Returns strlen(out).
 *  Condition inverse (0x623A) is applied at draw time to the spaces around the name. */
size_t formatPartyStatusLine(char *out, size_t cap, int slot_index, const char *name, uint16_t hp,
                             PartyStatusPrefix prefix_style = PartyStatusPrefix::Exploration);

/** print_number @ 0x22480 / -$7BDE: left-pad to `width` (clamped 1..24) with `pad`.
 *  Extra digits are kept (width is a minimum, not a truncate). */
void formatPrintNumber(uint32_t value, char *out, size_t cap, int width, char pad = ' ');

/** Quick Ref + character sheet: right-align current in [0, field_width) for a fixed '/' column. */
void formatSlashStatCurrent(uint16_t value, char *out, size_t cap, int field_width);

/** Write "current/max" with aligned slash (no space after '/'). Returns strlen(out). */
size_t formatSlashStatPair(char *out, size_t cap, uint16_t current, uint16_t max, int field_width);

}  // namespace mm2::gfx
