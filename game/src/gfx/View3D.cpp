#include "mm2/gfx/View3D.h"

#include "mm2/CppStdCompat.h"

namespace mm2::gfx {

namespace {

constexpr int kFrustumSlots = 20;
constexpr int kHoodBytes = 13;

enum HoodIdx {
    H_C0 = 0, H_C1, H_C2, H_C3, H_C4,
    H_L1 = 5, H_L2, H_L3,
    H_R0 = 8, H_R1, H_R2, H_R3, H_R4,
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
    const View3DMapBuffers &bufs;

    uint8_t at(int x, int y) const {
        const std::array<uint8_t, kMapPageSize> *page = &bufs.center;
        int lx = x;
        int ly = y;
        if (x > 0x0F && x < 0x14) {
            page = &bufs.east;
            lx = x - 0x10;
        } else if (x < 0) {
            page = &bufs.west;
            lx = x + 0x10;
        } else if (x >= 0xFC) {
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
        if (lx < 0 || ly < 0 || lx >= kMapGridDim || ly >= kMapGridDim) {
            return 0;
        }
        return (*page)[static_cast<size_t>(ly * kMapGridDim + lx)];
    }
};

struct FacingCtx {
    uint8_t d1 = 0xC0;
    uint8_t d4 = 0x03;
    uint8_t d5 = 0x30;
    int shFwd = 6;
    int shD4 = 0;
    int shD5 = 4;
};

FacingCtx facingContext(int facing) {
    FacingCtx f{};
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

const int8_t *directionBundle(int facing) {
    static const int8_t *kAll[4] = {kBundleN, kBundleE, kBundleS, kBundleW};
    return kAll[facing & 3];
}

void refreshHood(const MapGrid &map, int px, int py, int facing, std::array<uint8_t, kHoodBytes> &hood) {
    hood.fill(0);
    const int8_t *b = directionBundle(facing);
    const int dx = b[0];
    const int dy = b[1];

    auto rowSampler = [&](int sx, int sy, int outOff) {
        int x = sx;
        int y = sy;
        for (int i = 0; i < 5; ++i) {
            hood[static_cast<size_t>(outOff + i)] = map.at(x, y);
            x += dx;
            y += dy;
        }
    };

    rowSampler(px, py, H_C0);
    rowSampler(px + b[2], py + b[3], H_C4);
    rowSampler(px + b[4], py + b[5], H_R0);
}

void assignSlot(std::array<uint8_t, kFrustumSlots> &slots, int idx, uint8_t cell, uint8_t mask, int shift) {
    if ((cell & mask) == 0) {
        return;
    }
    const uint8_t code = static_cast<uint8_t>((cell >> shift) & 3);
    if (code == 0) {
        return;
    }
    slots[static_cast<size_t>(idx)] = code;
}

void buildFrustum(const std::array<uint8_t, kHoodBytes> &hood, const FacingCtx &f,
                  std::array<uint8_t, kFrustumSlots> &slots) {
    slots.fill(0);
    const uint8_t d1 = f.d1;
    const uint8_t d4 = f.d4;
    const uint8_t d5 = f.d5;
    const int shFwd = f.shFwd;
    const int shD4 = f.shD4;
    const int shD5 = f.shD5;

    auto b = [&](int idx) -> uint8_t { return (idx >= 0 && idx < kHoodBytes) ? hood[static_cast<size_t>(idx)] : 0; };

    assignSlot(slots, S_F20, b(H_C0), d4, shD4);
    if (slots[S_F20] == 0) {
        assignSlot(slots, S_F14, b(H_C4), d1, shFwd);
    }
    assignSlot(slots, S_F1C, b(H_C0), d5, shD5);
    if (slots[S_F1C] == 0) {
        assignSlot(slots, S_F10, b(H_R0), d1, shFwd);
    }

    if (b(H_C0) & d1) {
        assignSlot(slots, S_F18, b(H_C0), d1, shFwd);
        if (slots[S_F20] == 0 && slots[S_F14] == 0) {
            assignSlot(slots, S_F13, b(H_L1), d1, shFwd);
        }
        if (slots[S_F1C] == 0 && slots[S_F10] == 0) {
            assignSlot(slots, S_F0F, b(H_R1), d1, shFwd);
        }
    } else {
        if (b(H_C1) & d4) {
            assignSlot(slots, S_F1F, b(H_C1), d4, shD4);
        } else {
            assignSlot(slots, S_F13, b(H_L1), d1, shFwd);
        }

        if (b(H_C1) & d5) {
            assignSlot(slots, S_F1B, b(H_C1), d5, shD5);
        } else {
            assignSlot(slots, S_F0F, b(H_R1), d1, shFwd);
        }

        assignSlot(slots, S_F17, b(H_C1), d1, shFwd);

        if (slots[S_F1F] == 0 && slots[S_F13] == 0) {
            assignSlot(slots, S_F12, b(H_L2), d1, shFwd);
        }
        if (slots[S_F1B] == 0 && slots[S_F0F] == 0) {
            assignSlot(slots, S_F0E, b(H_R2), d1, shFwd);
        }

        if ((b(H_C1) & d1) == 0) {
            if (b(H_C2) & d4) {
                assignSlot(slots, S_F1E, b(H_C2), d4, shD4);
            } else {
                assignSlot(slots, S_F12, b(H_L2), d1, shFwd);
            }

            if (b(H_C2) & d5) {
                assignSlot(slots, S_F1A, b(H_C2), d5, shD5);
            } else {
                assignSlot(slots, S_F0E, b(H_R2), d1, shFwd);
            }

            assignSlot(slots, S_F16, b(H_C2), d1, shFwd);

            if ((b(H_C2) & d1) == 0) {
                if (b(H_C3) & d4) {
                    assignSlot(slots, S_F1D, b(H_C3), d4, shD4);
                } else {
                    assignSlot(slots, S_F11, b(H_L3), d1, shFwd);
                }

                if (b(H_C3) & d5) {
                    assignSlot(slots, S_F19, b(H_C3), d5, shD5);
                } else {
                    assignSlot(slots, S_F0D, b(H_R3), d1, shFwd);
                }

                assignSlot(slots, S_F15, b(H_C3), d1, shFwd);
            }
        }
    }

    auto norm = [&](int a, int bb) {
        if (slots[a] != 0 && slots[bb] == static_cast<uint8_t>(WallField::Door)) {
            slots[bb] = static_cast<uint8_t>(WallField::Wall);
        }
    };
    norm(S_F20, S_F13);
    norm(S_F1C, S_F0F);
    norm(S_F1F, S_F12);
    norm(S_F1B, S_F0E);
}

bool view3dPaintLatticeCell(View3DScene &scene, int latX, int latRow, uint8_t rawCode) {
    if (rawCode == 0) {
        return false;
    }
    if (latX < kView3DLatXMin || latX > kView3DLatXMax) {
        return false;
    }
    if (latRow < kView3DLatRowMin || latRow > kView3DLatRowMax) {
        return false;
    }

    const int depth = view3dDepthFromRow(latRow);
    if (depth < 0 || depth >= kView3DDepthBands) {
        return false;
    }

    const uint8_t code = static_cast<uint8_t>(rawCode & 3);
    if (code == 0) {
        return false;
    }
    const bool isDoor = (code == static_cast<uint8_t>(WallField::Door));

    using T = View3DWallTables;
    int frame = 0;
    int x = 0;
    int y = 0;

    if (latX == 0) {
        x = T::kFrontX[depth];
        y = T::kFrontY[depth] + (depth == 0 ? 1 : 0);
        frame = depth;
        if (isDoor) {
            frame += 0x10;
        }
    } else if (latX == -2) {
        frame = T::kLeftFarFrame[depth];
        x = T::kLeftFarX[depth];
        y = T::kLeftFarY[depth];
        if (isDoor) {
            frame += 0x10;
        }
    } else if (latX == -1) {
        x = T::kLeftNearX[depth];
        y = T::kLeftNearY[depth];
        frame = depth + 4;
        if (isDoor) {
            frame += 0x10;
        }
    } else if (latX == 1) {
        x = T::kRightNearX[depth];
        y = T::kRightNearY[depth];
        frame = depth + 8;
        if (isDoor) {
            frame += 0x10;
        }
    } else {
        frame = T::kRightFarFrame[depth];
        x = T::kRightFarX[depth];
        y = T::kRightFarY[depth];
        if (isDoor) {
            frame += 0x10;
        }
    }

    if (frame < 0) {
        return false;
    }
    if (scene.num_blits < static_cast<int>(scene.blits.size())) {
        scene.blits[scene.num_blits++] = {frame, x, y, latX, latRow, code};
        if (code == kMapVisualWallTorch && depth <= 2 &&
            scene.num_torch_blits < static_cast<int>(scene.torch_blits.size())) {
            scene.torch_blits[scene.num_torch_blits++] =
                scene.blits[static_cast<size_t>(scene.num_blits - 1)];
        }
    }
    return true;
}

void paintFrustumCell(View3DScene &scene, int latX, int paintDepth, uint8_t rawCode) {
    const int depth = (paintDepth & 0x7F) - 1;
    if (depth < 0 || depth >= kView3DDepthBands) {
        return;
    }
    view3dPaintLatticeCell(scene, latX, view3dRowFromDepth(depth), rawCode);
}

void collectBlits(const std::array<uint8_t, kFrustumSlots> &s, View3DScene &scene) {
    scene.num_blits = 0;
    scene.num_torch_blits = 0;
    const auto sl = [&](int i) -> uint8_t { return s[static_cast<size_t>(i)]; };

    paintFrustumCell(scene, -1, 4, sl(S_F1D));
    paintFrustumCell(scene, -2, 0x84, sl(S_F11));
    paintFrustumCell(scene, 1, 4, sl(S_F19));
    paintFrustumCell(scene, 2, 0x84, sl(S_F0D));
    paintFrustumCell(scene, 0, 4, sl(S_F15));
    paintFrustumCell(scene, -1, 3, sl(S_F1E));
    paintFrustumCell(scene, -2, 0x83, sl(S_F12));
    paintFrustumCell(scene, 1, 3, sl(S_F1A));
    paintFrustumCell(scene, 2, 0x83, sl(S_F0E));
    paintFrustumCell(scene, 0, 3, sl(S_F16));
    paintFrustumCell(scene, -1, 2, sl(S_F1F));
    paintFrustumCell(scene, -2, 0x82, sl(S_F13));
    paintFrustumCell(scene, 1, 2, sl(S_F1B));
    paintFrustumCell(scene, 2, 0x82, sl(S_F0F));
    paintFrustumCell(scene, 0, 2, sl(S_F17));
    paintFrustumCell(scene, -1, 1, sl(S_F20));
    paintFrustumCell(scene, -2, 0x81, sl(S_F14));
    paintFrustumCell(scene, 1, 1, sl(S_F1C));
    paintFrustumCell(scene, 2, 0x81, sl(S_F10));
    paintFrustumCell(scene, 0, 1, sl(S_F18));
}

}  // namespace

View3DMapBuffers buildView3DMapBuffers(const uint8_t center_page[kMapPageSize]) {
    View3DMapBuffers bufs{};
    if (center_page) {
        ::memcpy(bufs.center.begin(), center_page, kMapPageSize);
    }
    return bufs;
}

bool view3dTorchBlitFor(const View3DBlit &wb, int phase, View3DTorchBlit *out) {
    if (!out || wb.code != kMapVisualWallTorch) {
        return false;
    }
    const int depth = view3dDepthFromRow(wb.latRow);
    if (depth < 0 || depth > 2) {
        return false;
    }

    const int flicker =
        ((phase % kView3DTorchPhaseCount) + kView3DTorchPhaseCount) % kView3DTorchPhaseCount;
    const int baseFrame = static_cast<int>(wb.frame);

    if (wb.latX == 0) {
        static constexpr int kX[3] = {105, 108, 107};
        static constexpr int kY[3] = {44, 52, 60};
        *out = {0x12 + depth * 3 + flicker, kX[depth], kY[depth]};
        return true;
    }
    if (wb.latX == -1) {
        if (wb.frame == 12 || wb.frame == 14 || wb.frame == 2 || wb.frame == 3 ||
            baseFrame == 12 || baseFrame == 14 || baseFrame == 2 || baseFrame == 3) {
            if (depth == 0) {
                return false;
            }
            static constexpr int kX[3] = {8, 16, 64};
            static constexpr int kY[3] = {44, 52, 60};
            *out = {0x12 + depth * 3 + flicker, kX[depth], kY[depth]};
            return true;
        }
        static constexpr int kX[3] = {8, 43, 73};
        static constexpr int kY[3] = {49, 55, 59};
        *out = {depth * 3 + flicker, kX[depth], kY[depth]};
        return true;
    }
    if (wb.latX == -2) {
        if (depth == 0) {
            return false;
        }
        static constexpr int kX[3] = {8, 16, 64};
        static constexpr int kY[3] = {44, 52, 60};
        *out = {0x12 + depth * 3 + flicker, kX[depth], kY[depth]};
        return true;
    }
    if (wb.latX == 1) {
        if (wb.frame == 13 || wb.frame == 15 || wb.frame == 2 || wb.frame == 3 ||
            baseFrame == 13 || baseFrame == 15 || baseFrame == 2 || baseFrame == 3) {
            if (depth == 0) {
                return false;
            }
            static constexpr int kX[3] = {202, 199, 152};
            static constexpr int kY[3] = {44, 52, 60};
            *out = {0x12 + depth * 3 + flicker, kX[depth], kY[depth]};
            return true;
        }
        static constexpr int kX[3] = {196, 166, 142};
        static constexpr int kY[3] = {49, 55, 59};
        *out = {9 + depth * 3 + flicker, kX[depth], kY[depth]};
        return true;
    }
    if (wb.latX == 2) {
        if (depth == 0) {
            return false;
        }
        static constexpr int kX[3] = {202, 199, 152};
        static constexpr int kY[3] = {44, 52, 60};
        *out = {0x12 + depth * 3 + flicker, kX[depth], kY[depth]};
        return true;
    }
    return false;
}

View3DScene buildView3DScene(const View3DMapBuffers &bufs, const View3DCamera &cam) {
    MapGrid grid{bufs};
    std::array<uint8_t, kHoodBytes> hood{};
    std::array<uint8_t, kFrustumSlots> slots{};

    refreshHood(grid, cam.x, cam.y, cam.facing, hood);
    buildFrustum(hood, facingContext(cam.facing), slots);

    View3DScene scene;
    scene.hood = hood;
    scene.slots = slots;
    collectBlits(slots, scene);
    return scene;
}

}  // namespace mm2::gfx
