#pragma once
// Outdoor first-person view (outdoor_3d_master @0x18870).
//
// Pipeline (see EXTRACTED/docs/15-3d-view-and-game-screen.md):
//   1. refreshOutdoorHood   — page-0 rows -> -$55D4 / -$55D0 / -$55CC
//   2. processTerrainRows   — @0x9544 terrain lookup -> -$55C6 / -$55C2 / -$55BE
//   3. buildDecorBlits      — @0x182D8 floor decor on biome sheet (-$7A0A)
//   4. buildHorizonBlits    — @0x1877A horizon from outdoor1/2/3.32
//
// Kept in sync with editor/src/core/Outdoor3D.{h,cpp} and tools/view3d_outdoor.py.

#include "mm2/CppStdCompat.h"

#include "mm2/gfx/View3D.h"

#include "mm2_attrib_codec.h"
#include "mm2_map_codec.h"

namespace mm2::world {
class MapWorld;
}

namespace mm2::gfx {

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

    static constexpr int16_t kDecorY[4] = {108, 93, 78, 68};
    static constexpr int kDecorX = 8;
    static constexpr int kDecorX112 = 0x70;
    static constexpr int16_t kDecorXBde[4] = {184, 160, 136, 112};
};

struct OutdoorSpriteBlit {
    enum class Kind : uint8_t { Decor, Horizon };

    Kind kind = Kind::Horizon;
    OutdoorBiome biome = OutdoorBiome::Desert;
    OutdoorHorizonSheet horizon = OutdoorHorizonSheet::Outdoor1;
    int frame = 0;
    int x = 0;
    int y = 0;
};

/* Night sky + stars — outdoor_3d_master @0x18870 calls the night routine
 * (thunk -$7e78 -> @0x0687C) instead of the daytime sky.32 blit whenever
 * MM2_GS_TIME_SUBDAY (-$79b4, word, 256 = 1 day) >= 0x80 (the second half of
 * the day). The routine fills the sky band black then plots 7 star pixels. */

/* Night is the second half of the day cycle (cmpi.w #$80,-$79b4 @0x18898). */
constexpr int kOutdoorNightSubdayThreshold = 0x80;

/* Black sky fill — fill-rect primitive @0x217ba paints horizontal lines from
 * x=8..215 over rows y=8..67 inclusive, i.e. the 208x60 sky.32 footprint. */
constexpr int kOutdoorSkyFillX = kView3DOriginX;   // 8
constexpr int kOutdoorSkyFillY = kView3DSkyY;      // 8
constexpr int kOutdoorSkyFillW = kView3DViewportW; // 208 (cols 8..215)
constexpr int kOutdoorSkyFillH = 60;               // rows 8..67
constexpr uint8_t kOutdoorSkyFillPen = 0;          // pen 0 (black) @0x6888

/* Star palette pens (indices into the active sky.32 palette): color A from
 * -$7a4d (ghidra/mm2_data_00.bin @0x5B1 = 20), color B from -$7a52 (@0x5AC = 1).
 * The plot loop toggles A/B per star starting on A. In sky.32 pen 20 is light
 * blue, pen 1 is white. */
constexpr uint8_t kOutdoorStarPenA = 20;
constexpr uint8_t kOutdoorStarPenB = 1;
constexpr int kOutdoorStarCount = 7;

struct OutdoorStarBlit {
    int x = 0;
    int y = 0;
    uint8_t pen = 0;
};

/* Replicates the star plot loop @0x68ea: starting table index = facing*4
 * (N=0,W=4,S=8,E=12 from facing key char -$79b1), 7 consecutive entries with
 * the @0x6946 wrap-to-0, screen x = X[idx]*8 + 8 (+12 columns for stars 4..6
 * @0x691e), y = Y[idx]. Returns kOutdoorStarCount. `facing` uses the engine
 * 0=N,1=E,2=S,3=W convention. */
int buildOutdoorStars(int facing, std::array<OutdoorStarBlit, kOutdoorStarCount> &out);

struct OutdoorScene {
    std::array<std::array<uint8_t, kOutdoorHoodRowLen>, 3> rowsRaw{};
    std::array<uint8_t, kOutdoorHoodRowLen> laneC6{};
    std::array<uint8_t, kOutdoorHoodRowLen> laneC2{};
    std::array<uint8_t, kOutdoorHoodRowLen> laneBe{};
    std::array<OutdoorBiome, kOutdoorLaneCols> columnBiomes{};
    std::array<OutdoorSpriteBlit, 16> decor{};
    int num_decor = 0;
    std::array<OutdoorSpriteBlit, 16> horizon{};
    int num_horizon = 0;
};

const char *outdoorBiomeFilename(OutdoorBiome biome);
const char *outdoorHorizonFilename(OutdoorHorizonSheet sheet);

OutdoorScene buildOutdoorScene(const world::MapWorld &world, const View3DCamera &cam);

}  // namespace mm2::gfx
