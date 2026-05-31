#pragma once
// Outdoor first-person view (outdoor_3d_master @0x18870).
//
// Pipeline (see EXTRACTED/docs/15-3d-view-and-game-screen.md):
//   1. refreshOutdoorHood   — page-0 rows -> -$55D4 / -$55D0 / -$55CC
//   2. processTerrainRows   — @0x9544 terrain lookup -> -$55C6 / -$55C2 / -$55BE
//   3. buildDecorBlits      — @0x182D8 floor decor on biome sheet (-$7A0A)
//   4. buildHorizonBlits    — @0x1877A horizon from outdoor1/2/3.32
//
// Kept in sync with tools/view3d_outdoor.py.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/AttribFile.h"
#include "core/MapFile.h"
#include "core/View3D.h"

namespace mm2 {

enum class OutdoorBiome : uint8_t {
    Desert = 0,
    Ocean,
    Tundra,
    Swamp,
};

enum class OutdoorHorizonSheet : uint8_t {
    Outdoor1 = 0,
    Outdoor2,
    Outdoor3,
};

constexpr int kOutdoorLaneCols = 4;
constexpr int kOutdoorHoodRowLen = 5;
constexpr uint8_t kOutdoorHorizonNone = 0xFF;

struct OutdoorHorizonTables {
    static constexpr int16_t kL1Y[4] = {21, 21, 42, 50};
    static constexpr int16_t kL1X[4] = {40, 40, 64, 88};
    static constexpr uint8_t kL1Frame[4] = {0, 0, 1, 2};

    static constexpr int16_t kL2Y[4] = {36, 46, 50, 58};
    static constexpr int16_t kL2X[4] = {8, 16, 32, 88};
    static constexpr uint8_t kL2Frame[4] = {4, 5, 2, 3};

    static constexpr int16_t kL3Y[4] = {36, 46, 50, 58};
    static constexpr int16_t kL3X[4] = {176, 152, 136, 120};
    static constexpr uint8_t kL3Frame[4] = {6, 7, 2, 3};

    static constexpr int16_t kDecorY[4] = {108, 93, 78, 68};  // $80 - 6BD6
    static constexpr int kDecorX = 8;
    static constexpr int kDecorX112 = 0x70;
    static constexpr int16_t kDecorXBde[4] = {184, 160, 136, 112};
};

struct OutdoorSpriteBlit {
    enum class Kind : uint8_t { Decor, Horizon };

    Kind kind = Kind::Horizon;
    OutdoorBiome biome = OutdoorBiome::Desert;          // decor only
    OutdoorHorizonSheet horizon = OutdoorHorizonSheet::Outdoor1;  // horizon only
    int frame = 0;
    int x = 0;
    int y = 0;
};

struct OutdoorScene {
    std::array<std::array<uint8_t, kOutdoorHoodRowLen>, 3> rowsRaw{};
    std::array<uint8_t, kOutdoorHoodRowLen> laneC6{};
    std::array<uint8_t, kOutdoorHoodRowLen> laneC2{};
    std::array<uint8_t, kOutdoorHoodRowLen> laneBe{};
    std::array<OutdoorBiome, kOutdoorLaneCols> columnBiomes{};
    std::vector<OutdoorSpriteBlit> decor;
    std::vector<OutdoorSpriteBlit> horizon;
};

// Stitched page-0 sampler (center + N/E/S/W neighbors, cross-screen steps).
struct OutdoorMapGrid {
    const MapFile& map;
    const AttribFile& attrib;
    int screen = 0;

    OutdoorMapGrid(const MapFile& m, const AttribFile& a, int scr)
        : map(m), attrib(a), screen(scr) {}

    int screenIdAt(int x, int y) const;
    uint8_t at(int x, int y) const;
};

const char* outdoorBiomeFilename(OutdoorBiome biome);
const char* outdoorHorizonFilename(OutdoorHorizonSheet sheet);

bool loadOutdoorTerrainLookup(const std::string& dataBinPath,
                              std::array<uint8_t, 256>& out);
bool initOutdoorTerrainLookup(const std::string& dataBinPath);
const std::array<uint8_t, 256>& outdoorTerrainLookup();

OutdoorBiome remapOutdoorBiome(OutdoorBiome sheet, uint8_t sectorLabel);

OutdoorScene buildOutdoorScene(const OutdoorMapGrid& grid, int px, int py, int facing,
                               const AttribFile& attrib);

void stepOutdoorParty(int facing, int& x, int& y, int& screen, const AttribFile& attrib);

void dumpOutdoorTrace(const OutdoorScene& scene, int screen, int x, int y, int facing,
                      FILE* out);

}  // namespace mm2
