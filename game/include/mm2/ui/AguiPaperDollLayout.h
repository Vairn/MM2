#pragma once

// Agui paper-doll on the in-game sheet (replaces equipped/backpack rows $0C..$12).

#include <cstdint>

#include "mm2_roster_codec.h"

namespace mm2::ui::agui_doll {

constexpr int kIcon = 12;

constexpr int kBodyX = 40;
constexpr int kBodyY = 100;
constexpr int kBodyW = 32;
constexpr int kBodyH = 48;

enum class Body : uint8_t {
    Head = 0,
    Torso = 1,
    MainHand = 2,
    OffHand = 3,
    Missile = 4,
    Trinket = 5,
};

struct SlotPx {
    int x;
    int y;
};

constexpr SlotPx kHead{48, 100};
constexpr SlotPx kMissile{88, 100};
constexpr SlotPx kMainHand{16, 116};
constexpr SlotPx kTorso{48, 116};
constexpr SlotPx kOffHand{88, 116};

constexpr int kTrinketX0 = 16;
constexpr int kTrinketY = 136;
constexpr int kTrinketStep = 16;
constexpr int kTrinketCap = 6;

constexpr int kPackLabelX = 168;
constexpr int kPackIconX0 = 184;
constexpr int kPackY0 = 104;
constexpr int kPackColW = 56;
constexpr int kPackRowH = 16;

constexpr SlotPx kBodySlots[5] = {kHead, kTorso, kMainHand, kOffHand, kMissile};

inline bool itemIsTwoHandedMelee(uint8_t id)
{
    return id >= 0x42 && id <= 0x5B;
}

inline Body bodyForItemId(uint8_t id)
{
    if (id >= 0x9B && id <= 0x9F) {
        return Body::Head;
    }
    if (id >= 0x7F && id <= 0x9A) {
        return Body::Torso;
    }
    /* Cloaks / robes / loincloth sit on the torso visually. */
    if (id == 0xB6 || id == 0xC0 || id == 0xCC || id == 0xCE || id == 0xE1) {
        return Body::Torso;
    }
    if (id >= 0x01 && id <= 0x5B) {
        return Body::MainHand;
    }
    if (id >= 0x73 && id <= 0x7E) {
        return Body::OffHand;
    }
    /* Keys 0x6F..0x72 share the missile id band in combat; doll treats them as trinkets. */
    if (id >= 0x5C && id <= 0x6E) {
        return Body::Missile;
    }
    return Body::Trinket;
}

struct EquipView {
    uint8_t body_id[5];
    uint8_t body_slot[5]; /* equipped index 0..5, 0xFF empty */
    uint8_t trinket_id[kTrinketCap];
    uint8_t trinket_slot[kTrinketCap];
    int trinket_count;
};

inline EquipView assignEquip(const uint8_t equipped_id[MM2_ROSTER_ITEM_SLOTS])
{
    EquipView v{};
    for (int i = 0; i < 5; ++i) {
        v.body_id[i] = 0;
        v.body_slot[i] = 0xFF;
    }
    for (int i = 0; i < kTrinketCap; ++i) {
        v.trinket_id[i] = 0;
        v.trinket_slot[i] = 0xFF;
    }
    v.trinket_count = 0;
    if (!equipped_id) {
        return v;
    }
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        const uint8_t id = equipped_id[i];
        if (id == 0) {
            continue;
        }
        const Body b = bodyForItemId(id);
        const int bi = static_cast<int>(b);
        if (b != Body::Trinket && v.body_id[bi] == 0) {
            v.body_id[bi] = id;
            v.body_slot[bi] = static_cast<uint8_t>(i);
            if (b == Body::MainHand && itemIsTwoHandedMelee(id) && v.body_id[static_cast<int>(Body::OffHand)] == 0) {
                v.body_id[static_cast<int>(Body::OffHand)] = id;
                v.body_slot[static_cast<int>(Body::OffHand)] = static_cast<uint8_t>(i);
            }
        } else if (v.trinket_count < kTrinketCap) {
            v.trinket_id[v.trinket_count] = id;
            v.trinket_slot[v.trinket_count] = static_cast<uint8_t>(i);
            ++v.trinket_count;
        }
    }
    return v;
}

inline SlotPx packSlot(int i)
{
    /* 2×3 grid, A top-left, B top-right, … F bottom-right. */
    SlotPx s{};
    s.x = kPackIconX0 + (i % 2) * kPackColW;
    s.y = kPackY0 + (i / 2) * kPackRowH;
    return s;
}

}  // namespace mm2::ui::agui_doll
