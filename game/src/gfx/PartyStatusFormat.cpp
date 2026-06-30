#include "mm2/gfx/PartyStatusFormat.h"

#include "mm2/CppStdCompat.h"

namespace mm2::gfx {

void formatPartyHpField(uint16_t hp, char *out, size_t cap)
{
    if (!out || cap == 0) {
        return;
    }
    if (hp > 999) {
        std::snprintf(out, cap, "+++");
        return;
    }

    /* draw_party_status_panel @ 0x628E..0x62C2: 3-cell decimal field after " /".
     * Digits are right-aligned (leading spaces when HP < 100 / < 10). */
    std::snprintf(out, cap, "%3u", static_cast<unsigned>(hp));
}

size_t formatPartyStatusLine(char *out, size_t cap, int slot_index, const char *name, uint16_t hp,
                             PartyStatusPrefix prefix_style)
{
    if (!out || cap == 0) {
        return 0;
    }

    char hp_field[8];
    if (hp > 999) {
        hp_field[0] = '\0';
    } else {
        formatPartyHpField(hp, hp_field, sizeof(hp_field));
    }

    char name_field[kPartyNameFieldWidth + 1];
    std::snprintf(name_field, sizeof(name_field), "%-*.*s", kPartyNameFieldWidth, kPartyNameFieldWidth,
                  name ? name : "");

    const int slot_digit = (slot_index >= 0 && slot_index < 8) ? slot_index + 1 : 1;

    if (prefix_style == PartyStatusPrefix::CombatCheckmark) {
        if (hp > 999) {
            std::snprintf(out, cap, "%c%d)%-*s /+++", static_cast<char>(0x7E), slot_digit, kPartyNameFieldWidth,
                          name_field);
        } else {
            std::snprintf(out, cap, "%c%d)%-*s /%s", static_cast<char>(0x7E), slot_digit, kPartyNameFieldWidth,
                          name_field, hp_field);
        }
    } else if (hp > 999) {
        std::snprintf(out, cap, " %d)%-*s /+++", slot_digit, kPartyNameFieldWidth, name_field);
    } else {
        std::snprintf(out, cap, " %d)%-*s /%s", slot_digit, kPartyNameFieldWidth, name_field, hp_field);
    }

    return std::strlen(out);
}

void formatSlashStatCurrent(uint16_t value, char *out, size_t cap, int field_width)
{
    if (!out || cap == 0 || field_width <= 0) {
        return;
    }
    std::snprintf(out, cap, "%*u", field_width, static_cast<unsigned>(value));
}

size_t formatSlashStatPair(char *out, size_t cap, uint16_t current, uint16_t max, int field_width)
{
    if (!out || cap == 0 || field_width <= 0) {
        return 0;
    }
    char cur[kPartyNameFieldWidth + 8];
    formatSlashStatCurrent(current, cur, sizeof(cur), field_width);
    std::snprintf(out, cap, "%s/%u", cur, static_cast<unsigned>(max));
    return std::strlen(out);
}

}  // namespace mm2::gfx
