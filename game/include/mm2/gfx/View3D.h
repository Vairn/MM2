#pragma once
// First-person dungeon view — ported from editor/src/core/View3D.{h,cpp}
// (frustum @ 0x2900, paint @ 0x2F7E). See EXTRACTED/docs/15-3d-view-and-game-screen.md.

#include "mm2/CppStdCompat.h"

namespace mm2::gfx {

constexpr int kMapGridDim = 16;
constexpr int kMapPageSize = 256;

constexpr int kView3DDepthBands = 4;
constexpr int kView3DViewportW = 208;
constexpr int kView3DOriginX = 8;
constexpr int kView3DSkyY = 8;
constexpr int kView3DFloorY = 68;

constexpr int kView3DLatXMin = -2;
constexpr int kView3DLatXMax = 2;
constexpr int kView3DLatRowMin = -3;
constexpr int kView3DLatRowMax = 0;

/** map.dat page-0 visual field: 3 = wall+torch (doc 15 / hood @0x2900). */
constexpr uint8_t kMapVisualWallTorch = 3;

/** key_read_3d @0x1E9CE — A4-$667A torch flicker phase 0..2 (wraps at 3). */
constexpr int kView3DTorchPhaseCount = 3;

inline int view3dDepthFromRow(int latRow) { return -latRow; }
inline int view3dRowFromDepth(int depth) { return -depth; }

enum class WallField : uint8_t { Open = 0, Wall = 1, Door = 2, Torch = 3 };

struct View3DWallTables {
    static constexpr int16_t kFrontX[4] = {32, 64, 88, 104};
    static constexpr int16_t kFrontY[4] = {22, 40, 54, 62};
    static constexpr int16_t kLeftNearX[4] = {8, 32, 64, 88};
    static constexpr int16_t kLeftNearY[4] = {8, 22, 40, 54};
    static constexpr int16_t kLeftFarX[4] = {8, 8, 40, 88};
    static constexpr int16_t kLeftFarY[4] = {22, 40, 54, 62};
    static constexpr int16_t kRightNearX[4] = {192, 160, 136, 120};
    static constexpr int16_t kRightNearY[4] = {8, 22, 40, 54};
    static constexpr int16_t kRightFarX[4] = {192, 160, 136, 120};
    static constexpr int16_t kRightFarY[4] = {22, 40, 54, 62};
    static constexpr uint8_t kLeftFarFrame[4] = {12, 14, 2, 3};
    static constexpr uint8_t kRightFarFrame[4] = {13, 15, 2, 3};
};

struct View3DCamera {
    int x = 8;
    int y = 8;
    int facing = 0;  // 0=N 1=E 2=S 3=W
};

struct View3DBlit {
    int frame = 0;
    int x = 0;
    int y = 0;
    int latX = 0;
    int latRow = 0;
    uint8_t code = 0;  // map.dat page-0 nibble: 1 wall, 2 torch, 3 door
};

struct View3DTorchBlit {
    int frame = 0;
    int x = 0;
    int y = 0;
};

/** Returns false when this wall slot has no torch overlay (townt/cavet/castlet.32). */
bool view3dTorchBlitFor(const View3DBlit &wb, int phase, View3DTorchBlit *out);

struct View3DScene {
    std::array<View3DBlit, 20> blits{};
    int num_blits = 0;
    /** Field 2 only — populated at paint time (ASM -$4F62/-$4F76/-$4F8A). */
    std::array<View3DBlit, 12> torch_blits{};
    int num_torch_blits = 0;
    std::array<uint8_t, 13> hood{};
    std::array<uint8_t, 20> slots{};
};

struct View3DMapBuffers {
    std::array<uint8_t, kMapPageSize> center{};
    std::array<uint8_t, kMapPageSize> north{};
    std::array<uint8_t, kMapPageSize> east{};
    std::array<uint8_t, kMapPageSize> south{};
    std::array<uint8_t, kMapPageSize> west{};
};

View3DMapBuffers buildView3DMapBuffers(const uint8_t center_page[kMapPageSize]);
View3DScene buildView3DScene(const View3DMapBuffers &bufs, const View3DCamera &cam);

}  // namespace mm2::gfx
