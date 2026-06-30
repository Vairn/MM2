#pragma once
// First-person dungeon view for the map editor.
//
// Pipeline (see View3D.cpp):
//   1. refreshHood    — page-0 visual neighbourhood (hood @0x19D6)
//   2. buildFrustum   — frustum slots @0x2900
//   3. collectBlits   — wall_piece_* paint pass @0x2F7E
//
// Viewport lattice (ref-layout-*-coords.png):
//   latX  -2 far L, -1 near L, 0 front, +1 near R, +2 far R
//   latRow -3 (far) .. 0 (near); depth index d = -latRow (0..3)

#include <array>
#include <cstdint>
#include <vector>

#include "core/AttribFile.h"
#include "core/MapFile.h"

namespace mm2 {

constexpr int kView3DDepthBands = 4;
constexpr int kView3DViewportW = 208;
// Backdrop sheets only (view_3d_master @0x2EE2 hardcodes x=8, y=8/68).
// Wall/outdoor tables store absolute viewport coordinates — do not add this offset.
constexpr int kView3DOriginX = 8;
constexpr int kView3DSkyY = 8;
constexpr int kView3DFloorY = 68;

// Lateral lattice column (not screen px).
constexpr int kView3DLatXMin = -2;
constexpr int kView3DLatXMax = 2;
constexpr int kView3DLatRowMin = -3;
constexpr int kView3DLatRowMax = 0;

inline int view3dDepthFromRow(int latRow) { return -latRow; }
inline int view3dRowFromDepth(int depth) { return -depth; }
inline bool view3dLatXValid(int latX) {
    return latX >= kView3DLatXMin && latX <= kView3DLatXMax;
}
inline bool view3dLatRowValid(int latRow) {
    return latRow >= kView3DLatRowMin && latRow <= kView3DLatRowMax;
}

enum class WallField : uint8_t {
    Open  = 0,
    Wall  = 1,
    Door  = 2,
    Torch = 3,
};

// DATA hunk wall tables (A4-$75xx); shared with tools/render_view_refs.py
struct View3DWallTables {
    static constexpr int16_t kFrontX[4] = {32, 64, 88, 104};   // -$75B6
    static constexpr int16_t kFrontY[4] = {22, 40, 54, 62};    // -$75AE
    static constexpr int16_t kLeftNearX[4]  = {8, 32, 64, 88};
    static constexpr int16_t kLeftNearY[4]  = {8, 22, 40, 54};
    static constexpr int16_t kLeftFarX[4]   = {8, 8, 40, 88};
    static constexpr int16_t kLeftFarY[4]   = {22, 40, 54, 62};
    static constexpr int16_t kRightNearX[4] = {192, 160, 136, 120};
    static constexpr int16_t kRightNearY[4] = {8, 22, 40, 54};
    static constexpr int16_t kRightFarX[4]  = {192, 160, 136, 120};
    static constexpr int16_t kRightFarY[4]  = {22, 40, 54, 62};
    static constexpr uint8_t kLeftFarFrame[4]  = {12, 14, 2, 3};   // -$75A6
    static constexpr uint8_t kRightFarFrame[4] = {13, 15, 2, 3};  // -$7582
    static constexpr uint8_t kDoorFrontFrame[4] = {4, 5, 6, 7};
};

struct View3DCamera {
    int x = 8;
    int y = 8;
    int facing = 0;  // 0=N 1=E 2=S 3=W
};

struct View3DBlit {
    int frame = 0;
    int x = 0;       // screen top-left (viewport px)
    int y = 0;
    int latX = 0;    // lattice column -2..2
    int latRow = 0;  // lattice row -3..0
    uint8_t code = 0;  // map.dat page-0 nibble: 1 wall, 2 torch, 3 door
};

struct View3DScene {
    std::vector<View3DBlit> blits;  // back → front paint order
    std::array<uint8_t, 13> hood{};   // page-0 samples @ map_neighbourhood_refresh
    std::array<uint8_t, 20> slots{};  // frustum codes before paint
};

struct View3DMapBuffers {
    std::array<uint8_t, kMapPageSize> center{};
    std::array<uint8_t, kMapPageSize> north{};
    std::array<uint8_t, kMapPageSize> east{};
    std::array<uint8_t, kMapPageSize> south{};
    std::array<uint8_t, kMapPageSize> west{};
};

// Paint one lattice cell if rawCode is a visible wall (ASM wall_piece_*).
bool view3dPaintLatticeCell(std::vector<View3DBlit>& out, int latX, int latRow,
                             uint8_t rawCode);

View3DMapBuffers buildMapBuffers(const MapFile& map, int screenIdx, const AttribFile* attrib);

View3DScene buildView3DScene(const View3DMapBuffers& bufs, const View3DCamera& cam);

inline View3DScene buildView3DScene(const MapScreen& screen, const View3DCamera& cam) {
    View3DMapBuffers bufs{};
    bufs.center = screen.visual;
    return buildView3DScene(bufs, cam);
}

void stepForward(int facing, int& x, int& y);
void stepBackward(int facing, int& x, int& y);

const char* view3dLatXName(int latX);

void dumpView3DTrace(const View3DScene& scene, int camX, int camY, int facing, FILE* out);

}  // namespace mm2
