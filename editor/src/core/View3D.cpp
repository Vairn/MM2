#include "core/View3D.h"

#include <cstdio>
#include <vector>

namespace mm2 {

namespace {

constexpr int kFrustumSlots = 20;
constexpr int kHoodBytes = 13;

// Hood indices match A4-$55D4..-$55C9 after map_neighbourhood_refresh (3x5, overlapping).
enum HoodIdx {
    H_C0 = 0, H_C1, H_C2, H_C3, H_C4,  // center; [4] shared with left[0]
    H_L1 = 5, H_L2, H_L3,              // left row (overwrites center[4])
    H_R0 = 8, H_R1, H_R2, H_R3, H_R4,  // right row (overwrites left[4])
};

enum Slot {
    S_F20 = 0, S_F1F, S_F1E, S_F1D, S_F1C, S_F1B, S_F1A, S_F19,
    S_F18, S_F17, S_F16, S_F15, S_F14, S_F13, S_F12, S_F11,
    S_F10, S_F0F, S_F0E, S_F0D,
};

constexpr int8_t kBundleN[6] = {0, 1, -1, 0, 1, 0};
constexpr int8_t kBundleE[6] = {1, 0, 0, 1, 0, -1};
constexpr int8_t kBundleS[6] = {0, -1, 1, 0, -1, 0};
constexpr int8_t kBundleW[6] = {-1, 0, 0, -1, 0, 1};

struct MapGrid {
    const View3DMapBuffers& bufs;

    uint8_t at(int x, int y) const {
        const std::array<uint8_t, kMapPageSize>* page = &bufs.center;
        int lx = x;
        int ly = y;
        if (x > 0x0F && x < 0x14) {
            page = &bufs.east;
            lx = x - 0x10;
        } else if (x < 0) {
            page = &bufs.west;
            lx = x + 0x10;
        } else if (x >= 0xFC) {
            // Preserve 8-bit wrapped coordinate handling used by ASM traces.
            page = &bufs.west;
            lx = x - 0xF0;
        }
        if (y >= kMapGridDim) {
            page = &bufs.north;
            ly = y - kMapGridDim;
        } else if (y < 0) {
            page = &bufs.south;
            ly = y + kMapGridDim;
        } else if (y >= 0xFC) {
            page = &bufs.south;
            ly = y - 0xF0;
        }
        if (lx < 0 || ly < 0 || lx >= kMapGridDim || ly >= kMapGridDim) return 0;
        return (*page)[static_cast<size_t>(ly * kMapGridDim + lx)];
    }
};

// Masks/shifts from frustum_wall_cell_builder @0x2900 (-$55D8, -$75C9/$75CA, -$55D7).
struct FacingCtx {
    uint8_t d1 = 0xC0;
    uint8_t d4 = 0x03;
    uint8_t d5 = 0x30;
    int     shFwd = 6;
    int     shD4 = 0;
    int     shD5 = 4;
};

FacingCtx facingContext(int facing) {
    FacingCtx f{};
    // Keep exactly in sync with tools/view3d_trace.py:set_facing.
    static constexpr uint8_t kDirMask[4] = {0xC0, 0x30, 0x0C, 0x03};
    const int i = facing & 3;
    f.d1 = kDirMask[i];
    f.shFwd = (f.d1 == 0x03) ? 0 : (6 - 2 * i);
    f.shD4 = (f.shFwd + 2) & 7;
    f.shD5 = (f.shFwd == 0) ? 6 : (f.shFwd - 2);
    f.d5 = (f.d1 == 0x03) ? 0xC0 : static_cast<uint8_t>(f.d1 >> 2);
    f.d4 = (f.d1 == 0xC0) ? 0x03 : static_cast<uint8_t>((f.d1 << 2) & 0xFF);
    return f;
}

const int8_t* directionBundle(int facing) {
    static const int8_t* kAll[4] = {kBundleN, kBundleE, kBundleS, kBundleW};
    return kAll[facing & 3];
}

void refreshHood(const MapGrid& map, int px, int py, int facing,
                 std::array<uint8_t, kHoodBytes>& hood) {
    hood.fill(0);
    const int8_t* b = directionBundle(facing);
    const int dx = b[0], dy = b[1];

    auto rowSampler = [&](int sx, int sy, int outOff) {
        int x = sx, y = sy;
        for (int i = 0; i < 5; ++i) {
            hood[static_cast<size_t>(outOff + i)] = map.at(x, y);
            x += dx;
            y += dy;
        }
    };

    // Keep exactly in sync with tools/view3d_indoor.py:refresh_hood.
    rowSampler(px, py, H_C0);
    rowSampler(px + b[2], py + b[3], H_C4);
    rowSampler(px + b[4], py + b[5], H_R0);
}

void assignSlot(std::array<uint8_t, kFrustumSlots>& slots, int idx, uint8_t cell,
                uint8_t mask, int shift) {
    if ((cell & mask) == 0) return;
    // ASM uses lsr.b and stores the shifted byte directly (no &3 clamp).
    const uint8_t code = static_cast<uint8_t>((cell >> shift) & 3);
    if (code == 0) return;
    slots[static_cast<size_t>(idx)] = code;
}

void buildFrustum(const std::array<uint8_t, kHoodBytes>& hood, const FacingCtx& f,
                  std::array<uint8_t, kFrustumSlots>& slots) {
    slots.fill(0);
    const uint8_t d1 = f.d1, d4 = f.d4, d5 = f.d5;
    const int shFwd = f.shFwd, shD4 = f.shD4, shD5 = f.shD5;

    auto b = [&](int idx) -> uint8_t {
        return (idx >= 0 && idx < kHoodBytes) ? hood[static_cast<size_t>(idx)] : 0;
    };

    // 2964..29ac — bootstrap side slots from player / far columns
    assignSlot(slots, S_F20, b(H_C0), d4, shD4);
    if (slots[S_F20] == 0) assignSlot(slots, S_F14, b(H_C4), d1, shFwd);
    assignSlot(slots, S_F1C, b(H_C0), d5, shD5);
    if (slots[S_F1C] == 0) assignSlot(slots, S_F10, b(H_R0), d1, shFwd);

    if (b(H_C0) & d1) {
        // 29b0..2a02 — player cell blocks forward on facing mask
        assignSlot(slots, S_F18, b(H_C0), d1, shFwd);
        if (slots[S_F20] == 0 && slots[S_F14] == 0)
            assignSlot(slots, S_F13, b(H_L1), d1, shFwd);
        if (slots[S_F1C] == 0 && slots[S_F10] == 0)
            assignSlot(slots, S_F0F, b(H_R1), d1, shFwd);
    } else {
        // 2a06..2aa8 — forward row 1 (hood C1), always when player forward open
        if (b(H_C1) & d4)
            assignSlot(slots, S_F1F, b(H_C1), d4, shD4);
        else
            assignSlot(slots, S_F13, b(H_L1), d1, shFwd);

        if (b(H_C1) & d5)
            assignSlot(slots, S_F1B, b(H_C1), d5, shD5);
        else
            assignSlot(slots, S_F0F, b(H_R1), d1, shFwd);

        assignSlot(slots, S_F17, b(H_C1), d1, shFwd);

        if (slots[S_F1F] == 0 && slots[S_F13] == 0)
            assignSlot(slots, S_F12, b(H_L2), d1, shFwd);
        if (slots[S_F1B] == 0 && slots[S_F0F] == 0)
            assignSlot(slots, S_F0E, b(H_R2), d1, shFwd);

        if ((b(H_C1) & d1) == 0) {
            // 2aac..2b0a — forward row 2 (hood C2)
            if (b(H_C2) & d4)
                assignSlot(slots, S_F1E, b(H_C2), d4, shD4);
            else
                assignSlot(slots, S_F12, b(H_L2), d1, shFwd);

            if (b(H_C2) & d5)
                assignSlot(slots, S_F1A, b(H_C2), d5, shD5);
            else
                assignSlot(slots, S_F0E, b(H_R2), d1, shFwd);

            assignSlot(slots, S_F16, b(H_C2), d1, shFwd);

            if ((b(H_C2) & d1) == 0) {
                // 2b0c..2b6a — forward row 3 (hood C3)
                if (b(H_C3) & d4)
                    assignSlot(slots, S_F1D, b(H_C3), d4, shD4);
                else
                    assignSlot(slots, S_F11, b(H_L3), d1, shFwd);

                if (b(H_C3) & d5)
                    assignSlot(slots, S_F19, b(H_C3), d5, shD5);
                else
                    assignSlot(slots, S_F0D, b(H_R3), d1, shFwd);

                assignSlot(slots, S_F15, b(H_C3), d1, shFwd);
            }
        }
    }

    // 2b6a..2bbc — door adjacent to open normalises to plain wall
    auto norm = [&](int a, int bb) {
        if (slots[a] != 0 && slots[bb] == static_cast<uint8_t>(WallField::Door))
            slots[bb] = static_cast<uint8_t>(WallField::Wall);
    };
    norm(S_F20, S_F13);
    norm(S_F1C, S_F0F);
    norm(S_F1F, S_F12);
    norm(S_F1B, S_F0E);
}

int wallFrame(WallField field, int baseFrame) {
    switch (field) {
        case WallField::Open:  return -1;
        case WallField::Wall:  return baseFrame;
        case WallField::Torch: return baseFrame;
        case WallField::Door:  return baseFrame;
    }
    return baseFrame;
}

void paintFrustumCell(std::vector<View3DBlit>& out, int latX, int paintDepth, uint8_t rawCode) {
    const int depth = (paintDepth & 0x7F) - 1;
    if (depth < 0 || depth >= kView3DDepthBands) return;
    view3dPaintLatticeCell(out, latX, view3dRowFromDepth(depth), rawCode);
}

void collectBlits(const std::array<uint8_t, kFrustumSlots>& s, std::vector<View3DBlit>& out) {
    out.clear();
    const auto sl = [&](int i) -> uint8_t { return s[static_cast<size_t>(i)]; };

    paintFrustumCell(out, -1, 4, sl(S_F1D));
    paintFrustumCell(out, -2, 0x84, sl(S_F11));
    paintFrustumCell(out, 1, 4, sl(S_F19));
    paintFrustumCell(out, 2, 0x84, sl(S_F0D));
    paintFrustumCell(out, 0, 4, sl(S_F15));
    paintFrustumCell(out, -1, 3, sl(S_F1E));
    paintFrustumCell(out, -2, 0x83, sl(S_F12));
    paintFrustumCell(out, 1, 3, sl(S_F1A));
    paintFrustumCell(out, 2, 0x83, sl(S_F0E));
    paintFrustumCell(out, 0, 3, sl(S_F16));
    paintFrustumCell(out, -1, 2, sl(S_F1F));
    paintFrustumCell(out, -2, 0x82, sl(S_F13));
    paintFrustumCell(out, 1, 2, sl(S_F1B));
    paintFrustumCell(out, 2, 0x82, sl(S_F0F));
    paintFrustumCell(out, 0, 2, sl(S_F17));
    paintFrustumCell(out, -1, 1, sl(S_F20));
    paintFrustumCell(out, -2, 0x81, sl(S_F14));
    paintFrustumCell(out, 1, 1, sl(S_F1C));
    paintFrustumCell(out, 2, 0x81, sl(S_F10));
    paintFrustumCell(out, 0, 1, sl(S_F18));
}

}  // namespace

bool view3dPaintLatticeCell(std::vector<View3DBlit>& out, int latX, int latRow,
                             uint8_t rawCode) {
    if (rawCode == 0) return false;
    if (!view3dLatXValid(latX) || !view3dLatRowValid(latRow)) return false;

    const int depth = view3dDepthFromRow(latRow);
    if (depth < 0 || depth >= kView3DDepthBands) return false;

    const auto field = static_cast<WallField>(rawCode & 3);
    if (field == WallField::Open) return false;
    const bool isDoor = (field == WallField::Door);

    using T = View3DWallTables;
    int frame = 0;
    int x = 0;
    int y = 0;

    if (latX == 0) {
        x = T::kFrontX[depth];
        y = T::kFrontY[depth] + (depth == 0 ? 1 : 0);
        frame = wallFrame(field, depth);
        if (isDoor) frame += 0x10;
    } else if (latX == -2) {
        frame = T::kLeftFarFrame[depth];
        x = T::kLeftFarX[depth];
        y = T::kLeftFarY[depth];
        frame = wallFrame(field, frame);
        if (isDoor) frame += 0x10;
    } else if (latX == -1) {
        x = T::kLeftNearX[depth];
        y = T::kLeftNearY[depth];
        frame = wallFrame(field, depth + 4);
        if (isDoor) frame += 0x10;
    } else if (latX == 1) {
        x = T::kRightNearX[depth];
        y = T::kRightNearY[depth];
        frame = wallFrame(field, depth + 8);
        if (isDoor) frame += 0x10;
    } else {
        frame = T::kRightFarFrame[depth];
        x = T::kRightFarX[depth];
        y = T::kRightFarY[depth];
        frame = wallFrame(field, frame);
        if (isDoor) frame += 0x10;
    }

    if (frame < 0) return false;
    out.push_back({frame, x, y, latX, latRow, static_cast<uint8_t>(field)});
    return true;
}

const char* view3dLatXName(int latX) {
    switch (latX) {
        case -2: return "far L";
        case -1: return "near L";
        case 0:  return "front";
        case 1:  return "near R";
        case 2:  return "far R";
        default: return "?";
    }
}

View3DMapBuffers buildMapBuffers(const MapFile& map, int screenIdx, const AttribFile* attrib) {
    View3DMapBuffers bufs{};
    if (screenIdx < 0 || screenIdx >= kMapScreens) return bufs;

    bufs.center = map.screens[static_cast<size_t>(screenIdx)].visual;

    if (!attrib) return bufs;
    auto loadNeighbor = [&](int slot, std::array<uint8_t, kMapPageSize>& page) {
        page.fill(0xFF);
        const int n = attrib->screens[static_cast<size_t>(screenIdx)].neighbor(slot);
        if (n >= 0 && n < kMapScreens && n != screenIdx)
            page = map.screens[static_cast<size_t>(n)].visual;
    };
    loadNeighbor(0, bufs.north);
    loadNeighbor(1, bufs.east);
    loadNeighbor(2, bufs.south);
    loadNeighbor(3, bufs.west);
    return bufs;
}

View3DScene buildView3DScene(const View3DMapBuffers& bufs, const View3DCamera& cam) {
    MapGrid grid{bufs};
    std::array<uint8_t, kHoodBytes> hood{};
    std::array<uint8_t, kFrustumSlots> slots{};

    refreshHood(grid, cam.x, cam.y, cam.facing, hood);
    buildFrustum(hood, facingContext(cam.facing), slots);

    View3DScene scene;
    scene.hood = hood;
    scene.slots = slots;
    collectBlits(slots, scene.blits);
    return scene;
}

void stepForward(int facing, int& x, int& y) {
    static const int kDx[] = {0, 1, 0, -1};
    static const int kDy[] = {1, 0, -1, 0};
    x += kDx[facing & 3];
    y += kDy[facing & 3];
}

void stepBackward(int facing, int& x, int& y) {
    stepForward((facing + 2) & 3, x, y);
}

void dumpView3DTrace(const View3DScene& scene, int camX, int camY, int facing, FILE* out) {
    static const char* kFace[] = {"N", "E", "S", "W"};
    std::fprintf(out, "(%d,%d) facing %s\n", camX, camY, kFace[facing & 3]);
    std::fprintf(out, "%zu blit(s), paint order (latX,latRow) screen:\n", scene.blits.size());
    for (const View3DBlit& b : scene.blits)
        std::fprintf(out, "  (%d,%d) %s  frame=%d @ (%d,%d)\n", b.latX, b.latRow,
                     view3dLatXName(b.latX), b.frame, b.x, b.y);
}

}  // namespace mm2
