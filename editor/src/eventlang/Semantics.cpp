#include "eventlang/Semantics.h"

#include <cctype>
#include <cstdio>
#include <optional>
#include <utility>

namespace mm2::eventlang {

TriggerCond triggerCondFromByte(uint8_t raw) {
    switch (raw) {
        case 0x10: return TriggerCond::Always;
        case 0x20: return TriggerCond::FromNorth;
        case 0x40: return TriggerCond::DirSpecial;
        case 0x80: return TriggerCond::Enter;
        case 0xA0: return TriggerCond::FacingNs;
        case 0xC0: return TriggerCond::EnterSpecial;
        case 0xF0: return TriggerCond::AnyDirection;
        default: return TriggerCond::Raw;
    }
}

uint8_t triggerCondByte(TriggerCond cond, uint8_t rawFallback) {
    switch (cond) {
        case TriggerCond::Always: return 0x10;
        case TriggerCond::Enter: return 0x80;
        case TriggerCond::FromNorth: return 0x20;
        case TriggerCond::DirSpecial: return 0x40;
        case TriggerCond::AnyDirection: return 0xF0;
        case TriggerCond::FacingNs: return 0xA0;
        case TriggerCond::EnterSpecial: return 0xC0;
        default: return rawFallback;
    }
}

bool isCondSetOp(uint8_t op) {
    switch (op) {
        case 0x09:
        case 0x0A:
        case 0x15:  // OP_15 apply_party test → cond_flag (not masked write)
        case 0x16:
        case 0x17:
        case 0x19:  // give_item → cond=1 if placed on a member
        case 0x1B:
        case 0x1C:  // rng roll → raw cond byte
        case 0x1F:  // party_effect add → cond=1, underflow N/A
        case 0x20:  // party_effect sub → cond=0 on underflow (EventPartyEffects)
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:  // pay gems → cond
        case 0x28:  // consume backpack item → cond
        case 0x2D:
        case 0x30:
        case 0x32:
            return true;
        default:
            return false;
    }
}

std::string partyMemberToken(int memberSpec) {
    if (memberSpec == 0) return "all";
    if (memberSpec == 9) return "selected";
    return "member=" + std::to_string(memberSpec);
}

const char* partyFieldName(int fieldSel) {
    switch (fieldSel & 0x7F) {
        case 0x6D: return "skills";       // sel→rec+0x50 dual skill nibbles
        case 0x74: return "quest_flags";  // sel→rec+0x79 class/quest/guild mask
        case 0x75: return "quest_byte";
        case 0x76: return "nordon_flags";
        case 0x7B: return "travel_flags";
        default: return nullptr;
    }
}

int partyFieldByName(const std::string& name) {
    std::string lower;
    for (char c : name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "skills") return 0x6D;
    if (lower == "quest_flags") return 0x74;
    if (lower == "quest_byte") return 0x75;
    if (lower == "nordon_flags") return 0x76;
    if (lower == "travel_flags") return 0x7B;
    return -1;
}

const char* skillNibbleName(int nibble) {
    switch (nibble & 0x0F) {
        case 0x03: return "cartographer";
        case 0x0B: return "mountaineering";
        case 0x0D: return "pathfinder";
        default: return nullptr;
    }
}

const char* selectorHandlerLabel(int sel) {
    // Explicit OP_0E cases @ asm 0x160C2 / EventTownServices (not default-range).
    switch (sel) {
        case 0x01: return "inn (stay / registry)";
        case 0x02: return "training hall (XP level-up)";
        case 0x03: return "tavern / pub";
        case 0x04: return "temple";
        case 0x05: return "mages guild (spell shop)";
        case 0x06: return "blacksmith";
        case 0x07: return "general store (skill convert)";
        case 0x08: return "arena games (ticket combat)";
        case 0x64: return "circus (attr game)";
        case 0x7E: return "free teleport UI";
        case 0x7F: return "seeded combat encounter";
        case 0x80: return "intra-map portal / slide trap";
        case 0x81: return "found item (tier 0)";
        case 0x82: return "found item (tier 1)";
        case 0x83: return "found item (tier 2)";
        case 0xC9: return "quest encode / tip";
        case 0xCA: return "tavern drink";
        case 0xCB: return "quest handler 0xCB";
        case 0xCC: return "quest handler 0xCC";
        case 0xCD: return "quest handler 0xCD";
        case 0xCE: return "quest handler 0xCE";
        case 0xCF: return "quest handler 0xCF";
        case 0xE2: return "special 0xE2";
        case 0xFD: return "special 0xFD";
        default: return nullptr;
    }
}

namespace {

const char* overlayBankTitle(int loc) {
    switch (loc) {
        case 60: return "Nordon/Nordonna/Corak quest bank";
        case 61: return "spell/hireling / class-quest tips";
        case 62: return "Chris / Gertrude side quests";
        case 63: return "castle overlay A";
        case 64: return "Lord Haart heirloom";
        case 65: return "castle overlay B";
        case 66: return "endgame Corak/Murray/Horvath";
        case 67: return "Hall of Spells pool";
        case 68: return "castle overlay C";
        case 69: return "Queen Lamanda";
        case 70: return "HoS / bishops / puzzles";
        default: return "overlay bank";
    }
}

struct SelBin {
    int lo, hi, cat, sub;
};
constexpr SelBin kSelectorBins[] = {
    {0x09, 0x10, 0x3C, 0x08}, {0x11, 0x37, 0x3D, 0x10}, {0x38, 0x4B, 0x3E, 0x37},
    {0x4C, 0x54, 0x3F, 0x4B}, {0x56, 0x5B, 0x40, 0x55}, {0x5C, 0x5E, 0x41, 0x5B},
    {0x65, 0x69, 0x42, 0x64}, {0x6A, 0x7C, 0x43, 0x69}, {0x97, 0x98, 0x44, 0x96},
    {0xE3, 0xF3, 0x45, 0xE2}, {0xF4, 0xFB, 0x46, 0xF3},
};

}  // namespace

std::optional<std::pair<int, int>> binExecSelector(int sel) {
    const int s = sel & 0xFF;
    // Explicit handlers are NOT default-range (ASM switch before 0x15EDC).
    if (selectorHandlerLabel(s)) return std::nullopt;
    for (const auto& r : kSelectorBins) {
        if (s >= r.lo && s <= r.hi) return std::make_pair(r.cat, s - r.sub);
    }
    return std::nullopt;
}

int selectorFromOverlayBin(int overlayLoc, int eventIndex) {
    for (const auto& r : kSelectorBins) {
        if (r.cat != overlayLoc) continue;
        const int sel = eventIndex + r.sub;
        if (sel >= r.lo && sel <= r.hi && !selectorHandlerLabel(sel)) return sel;
    }
    return -1;
}

std::string formatSelectorDsl(int sel) {
    const int s = sel & 0xFF;
    char hex[8];
    std::snprintf(hex, sizeof(hex), "0x%02X", s);

    // Named town services (explicit OP_0E cases @ 0x160C2).
    if (s == 0x01) return "shop inn";
    if (s == 0x02) return "shop training";
    if (s == 0x03) return "shop tavern";
    if (s == 0x04) return "shop temple";
    if (s == 0x05) return "shop mages_guild";
    if (s == 0x06) return "shop blacksmith";
    if (s == 0x07) return "shop general_store";
    if (s == 0x08) return "shop arena";
    if (s == 0x64) return "shop circus";

    if (const char* lab = selectorHandlerLabel(s)) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "engine %s  # %s", hex, lab);
        return buf;
    }

    // Default-range @ 0x15EDC → re-enter event.dat overlay bank.
    if (auto bin = binExecSelector(s)) {
        const char* tip = nullptr;
        if (bin->first == 60) {
            if (bin->second == 1) tip = "Corak intro";
            else if (bin->second == 2) tip = "Nordon goblet quest";
            else if (bin->second == 3) tip = "Nordonna";
            else if (bin->second == 5) tip = "mages guild enroll tip";
            else if (bin->second == 6) tip = "Feldecarb Fountain";
        }
        char buf[160];
        if (tip) {
            std::snprintf(buf, sizeof(buf),
                          "overlay %d event_%02d  # selector %s — %s", bin->first, bin->second, hex,
                          tip);
        } else {
            std::snprintf(buf, sizeof(buf),
                          "overlay %d event_%02d  # selector %s — %s", bin->first, bin->second, hex,
                          overlayBankTitle(bin->first));
        }
        return buf;
    }

    char buf[48];
    std::snprintf(buf, sizeof(buf), "selector %s  # unknown OP_0E", hex);
    return buf;
}

std::string formatSelectorSummary(int sel) {
    const int s = sel & 0xFF;
    if (s == 0x01) return "shop inn";
    if (s == 0x02) return "shop training";
    if (s == 0x03) return "shop tavern";
    if (s == 0x04) return "shop temple";
    if (s == 0x05) return "shop mages_guild";
    if (s == 0x06) return "shop blacksmith";
    if (s == 0x07) return "shop general_store";
    if (s == 0x08) return "shop arena";
    if (s == 0x64) return "shop circus";
    if (const char* lab = selectorHandlerLabel(s)) return lab;
    if (auto bin = binExecSelector(s)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "overlay %d.%02d", bin->first, bin->second);
        return buf;
    }
    char buf[24];
    std::snprintf(buf, sizeof(buf), "selector 0x%02X", s);
    return buf;
}

std::string autoScriptName(int eventId) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "event_%02d", eventId);
    return buf;
}

std::string autoStringName(int index) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "s%d", index);
    return buf;
}

uint8_t sayOpForVariant(const std::string& variant) {
    if (variant == "basic") return 0x01;
    if (variant == "block") return 0x02;
    if (variant == "door") return 0x04;
    if (variant == "popup_a") return 0x05;
    if (variant == "popup_b") return 0x06;
    if (variant == "say_door") return 0x04;
    if (variant == "say_block") return 0x02;
    if (variant == "say_popup") return 0x05;
    return 0x03;
}

const char* sayVariantForOp(uint8_t op) {
    switch (op) {
        case 0x01: return "basic";
        case 0x02: return "block";
        case 0x03: return "";
        case 0x04: return "door";
        case 0x05: return "popup_a";
        case 0x06: return "popup_b";
        default: return "";
    }
}

int classFieldByName(const std::string& name) {
    std::string lower;
    for (char c : name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "knight") return 0x00;
    if (lower == "paladin") return 0x01;
    if (lower == "archer") return 0x02;
    if (lower == "cleric") return 0x03;
    if (lower == "sorcerer") return 0x04;
    if (lower == "robber") return 0x05;
    if (lower == "ninja") return 0x06;
    if (lower == "barbarian") return 0x07;
    return 0;
}

int selectorByShopOrQuest(const std::string& kind, const std::string& name) {
    std::string n;
    for (char c : name) n += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (kind == "shop") {
        if (n == "inn") return 0x01;
        if (n == "training") return 0x02;
        if (n == "tavern") return 0x03;
        if (n == "temple") return 0x04;
        if (n == "mages_guild") return 0x05;
        if (n == "blacksmith") return 0x06;
        if (n == "general_store") return 0x07;
        if (n == "arena_shop" || n == "arena") return 0x08;
        if (n == "enroll_mages") return 0x0D;
        if (n == "circus") return 0x64;
        return -1;  // unknown shop — do not invent a selector
    }
    if (kind == "quest") {
        if (n == "goblet") return 0x0A;
        if (n == "portal_travel") return 0x64;
        return -1;
    }
    return -1;
}

}  // namespace mm2::eventlang
