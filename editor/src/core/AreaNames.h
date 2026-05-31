#pragma once
// MM2 area (map screen) names, transcribed from b3dmm2/mm2ed.bb (the Select
// area / Case block around line 3145). There are 60 map screens (0..59);
// unnamed indices are overland surface areas the legacy tool labels from
// attrib.dat at runtime, so we fall back to "Area NN".
//
// The legacy .bb had many placeholder/misspelled names; all entries were
// verified against the game-data map index table at
// dungeoncrawl-classics.com/might-and-magic/mm2/mm2-map-details and the
// GameFAQs walkthrough's canonical location names. Corrections applied:
//   - town spellings: Tundra->Tundara, Volcania->Vulcania (+ their caverns)
//   - punctuation/typos: Corak's, Sarakin's, Druid's, Forbidden, Dragon's,
//     Dawn's, Gemmaker's Cave, Nomadic Rift, "Plain"->"Plane"
//   - wrong locations: 23 Square Isle->Square Lake Cave, 24 Tundra Cave->Ice
//     Cavern, 26 Murry's Resort->Murray's Cave
//   - castle basements 45-52 (were generic "dungeon Level N") and Castle
//     Xabran (59, was "the One in 800")
//   - Hillstone/Woodhaven main indices 55/56 were swapped in the legacy .bb.
// Each castle's two basements sit at a fixed offset from its main index:
//   55 Hillstone -> 45/46, 56 Woodhaven -> 47/48,
//   57 Pinehurst -> 49/50, 58 Luxus    -> 51/52.
//
// Environment-type decode (also from mm2ed.bb):
//   attrib +0x04 > 0            -> outside area (name "Outside Area: XX" via +0x15)
//   else attrib +0x03 == 17     -> town
//                       == 18     -> cavern
//                       == 19/20  -> castle
//   areas 41..44 are forced outside (the elemental planes).

#include <cstdint>
#include <cstdio>
#include <string>

namespace mm2 {

constexpr int kAreaCount = 60;

inline const char* areaNameRaw(int area) {
    switch (area) {
        case 0:  return "Middlegate";
        case 1:  return "Atlantium";
        case 2:  return "Tundara";
        case 3:  return "Vulcania";
        case 4:  return "Sandsobar";
        case 17: return "Middlegate Cavern";
        case 18: return "Atlantium Cavern";
        case 19: return "Tundara Cavern";
        case 20: return "Vulcania Cavern";
        case 21: return "Sandsobar Cavern";
        case 22: return "Corak's Cave";
        case 23: return "Square Lake Cave";
        case 24: return "Ice Cavern";
        case 25: return "Sarakin's Mine";
        case 26: return "Murray's Cave";
        case 27: return "Druid's Cave";
        case 28: return "Forbidden Forest Cavern";
        case 29: return "Dragon's Dominion";
        case 30: return "Dawn's Mist Bog";
        case 31: return "Gemmaker's Cave";
        case 32: return "Nomadic Rift";
        case 41: return "Elemental Plane of Air";
        case 42: return "Elemental Plane of Fire";
        case 43: return "Elemental Plane of Earth";
        case 44: return "Elemental Plane of Water";
        case 45: return "Hillstone Dungeon Level 1";
        case 46: return "Hillstone Dungeon Level 2";
        case 47: return "Woodhaven Dungeon Level 1";
        case 48: return "Woodhaven Dungeon Level 2";
        case 49: return "Pinehurst Dungeon Level 1";
        case 50: return "Pinehurst Dungeon Level 2";
        case 51: return "Luxus Palace Dungeon Level 1";
        case 52: return "Luxus Palace Dungeon Level 2";
        case 53: return "Ancients (Good)";
        case 54: return "Ancients (Evil)";
        case 55: return "Hillstone";
        case 56: return "Woodhaven";
        case 57: return "Pinehurst";
        case 58: return "Luxus Palace Royale";
        case 59: return "Castle Xabran";
        default: return "";
    }
}

// Display label: real name when known, else "Area NN".
inline std::string areaLabel(int area) {
    const char* n = areaNameRaw(area);
    if (n[0]) return std::string(n);
    char buf[16];
    snprintf(buf, sizeof(buf), "Area %d", area);
    return buf;
}

// Decode environment type from an attrib.dat record's +0x03 byte.
inline const char* envTypeName(uint8_t attribByte03) {
    switch (attribByte03) {
        case 17: return "Town";
        case 18: return "Cavern";
        case 19: return "Castle";
        case 20: return "Castle";
        default: return "Outside / other";
    }
}

}  // namespace mm2
