#include "mm2/gfx/OutdoorView3D.h"

#include "mm2/world/MapWorld.h"

namespace mm2::gfx {

namespace {

constexpr int kHoodBytes = 13;

constexpr int8_t kBundleN[6] = {0, 1, -1, 0, 1, 0};
constexpr int8_t kBundleE[6] = {1, 0, 0, 1, 0, -1};
constexpr int8_t kBundleS[6] = {0, -1, 1, 0, -1, 0};
constexpr int8_t kBundleW[6] = {-1, 0, 0, -1, 0, 1};

/* @0x9524 terrain lookup table (A4-$720A), from EXTRACTED/ghidra/mm2_data_00.bin. */
constexpr uint8_t kTerrainLookup[256] = {
    0x00, 0x01, 0x01, 0x02, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x3C, 0x0F, 0x5C, 0x12, 0x86, 0x18, 0x44, 0x1E, 0x80, 0xFD, 0x00,
    0x00, 0x40, 0x00, 0x20, 0xFF, 0xFF, 0x30, 0x30, 0x2E, 0x61, 0x6E, 0x6D, 0x00, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x30, 0x30, 0x30, 0x4B, 0x00, 0x00, 0x00, 0x9C, 0xAE, 0x00, 0x00, 0x9C, 0xCC,
    0x00, 0x00, 0x9C, 0xEB, 0x00, 0x00, 0x9D, 0x12, 0x00, 0x00, 0x9D, 0x39, 0x00, 0x00, 0x9D, 0x54,
    0x00, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x05, 0xDC, 0x00, 0x00,
    0x01, 0xF4, 0x00, 0x00, 0x07, 0xD0, 0x00, 0x00, 0x13, 0x88, 0x00, 0x00, 0x0B, 0xB8, 0x00, 0x00,
    0x1B, 0x58, 0x00, 0x00, 0x3A, 0x98, 0x00, 0x00, 0x27, 0x10, 0x00, 0x00, 0x4E, 0x20, 0x00, 0x00,
    0xC3, 0x50, 0x00, 0x00, 0x75, 0x30, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x40, 0x01, 0x02, 0x04, 0x08, 0x0A, 0x11,
    0x18, 0x0A, 0x11, 0x18, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x05, 0x09, 0x0C, 0x0F, 0x06, 0x0A,
    0x0D, 0x10, 0x07, 0x0B, 0x0E, 0x26, 0x08, 0x22, 0x24, 0x27, 0x21, 0x23, 0x25, 0x28, 0x00, 0x14,
    0x00, 0x28, 0x00, 0x3C, 0x00, 0x50, 0x00, 0x5D, 0x00, 0x5E, 0x00, 0x64, 0x00, 0x78, 0x00, 0x82,
};

const int8_t *directionBundle(int facing)
{
    static const int8_t *kAll[4] = {kBundleN, kBundleE, kBundleS, kBundleW};
    return kAll[facing & 3];
}

uint8_t facingMask(int facing)
{
    static constexpr uint8_t kMask[4] = {0xC0, 0x30, 0x0C, 0x03};
    return kMask[facing & 3];
}

int neighborScreen(const Mm2AttribFile &attrib, int screen, int slot)
{
    const Mm2AttribRecord &rec = attrib.records[screen];
    const int n = mm2_attrib_neighbor(&rec, slot);
    return (n >= 0 && n < MM2_MAP_SCREEN_COUNT) ? n : screen;
}

struct OutdoorMapGrid {
    const Mm2MapFile &map;
    const Mm2AttribFile &attrib;
    int screen = 0;

    int screenIdAt(int x, int y) const
    {
        return resolveCell(x, y).screen;
    }

    uint8_t at(int x, int y) const
    {
        const ResolvedCell r = resolveCell(x, y);
        if (r.lx < 0 || r.ly < 0 || r.lx >= kMapGridDim || r.ly >= kMapGridDim) {
            return 0;
        }
        if (r.screen < 0 || r.screen >= MM2_MAP_SCREEN_COUNT) {
            return 0;
        }
        return map.screens[r.screen].visual[(r.ly << 4) | r.lx];
    }

private:
    struct ResolvedCell {
        int screen = 0;
        int lx = 0;
        int ly = 0;
    };

    ResolvedCell resolveCell(int x, int y) const
    {
        ResolvedCell r{};
        r.screen = screen;
        r.lx = x;
        r.ly = y;
        if (r.lx < 0) {
            r.screen = neighborScreen(attrib, r.screen, 3);
            r.lx += kMapGridDim;
        } else if (r.lx >= kMapGridDim) {
            r.screen = neighborScreen(attrib, r.screen, 1);
            r.lx -= kMapGridDim;
        }
        if (r.ly < 0) {
            r.screen = neighborScreen(attrib, r.screen, 2);
            r.ly += kMapGridDim;
        } else if (r.ly >= kMapGridDim) {
            r.screen = neighborScreen(attrib, r.screen, 0);
            r.ly -= kMapGridDim;
        }
        return r;
    }
};

OutdoorBiome biomeFromSurfaceFlag(uint8_t sf)
{
    if (sf == 0xCC) {
        return OutdoorBiome::Ocean;
    }
    if (sf == 0x99) {
        return OutdoorBiome::Tundra;
    }
    if (sf == 0xBB) {
        return OutdoorBiome::Swamp;
    }
    return OutdoorBiome::Desert;
}

OutdoorBiome remapOutdoorBiome(OutdoorBiome sheet, uint8_t sectorLabel)
{
    if (sectorLabel == 0xC3) {
        return sheet;
    }
    switch (sheet) {
        case OutdoorBiome::Desert:
            return OutdoorBiome::Ocean;
        case OutdoorBiome::Ocean:
            return OutdoorBiome::Tundra;
        case OutdoorBiome::Tundra:
            return OutdoorBiome::Desert;
        default:
            return sheet;
    }
}

void refreshOutdoorHood(const OutdoorMapGrid &grid, int px, int py, int facing,
                        std::array<uint8_t, kHoodBytes> &hood)
{
    hood.fill(0);
    const int8_t *b = directionBundle(facing);
    const int dx = b[0];
    const int dy = b[1];

    auto rowSampler = [&](int sx, int sy, int outOff) {
        int x = sx;
        int y = sy;
        for (int i = 0; i < kOutdoorHoodRowLen; ++i) {
            hood[static_cast<size_t>(outOff + i)] = grid.at(x, y);
            x += dx;
            y += dy;
        }
    };

    rowSampler(px, py, 0);
    rowSampler(px + b[2], py + b[3], 4);
    rowSampler(px + b[4], py + b[5], 8);
}

void processTerrainRows(const std::array<std::array<uint8_t, kOutdoorHoodRowLen>, 3> &rows,
                        uint8_t currentCell, int facing,
                        std::array<uint8_t, kOutdoorHoodRowLen> &c6,
                        std::array<uint8_t, kOutdoorHoodRowLen> &c2,
                        std::array<uint8_t, kOutdoorHoodRowLen> &be)
{
    for (int i = 0; i < kOutdoorHoodRowLen; ++i) {
        c6[static_cast<size_t>(i)] = kTerrainLookup[rows[0][static_cast<size_t>(i)] & 0x1F];
        c2[static_cast<size_t>(i)] = kTerrainLookup[rows[1][static_cast<size_t>(i)] & 0x1F];
        be[static_cast<size_t>(i)] = kTerrainLookup[rows[2][static_cast<size_t>(i)] & 0x1F];
    }

    const uint8_t near = c6[0];
    if (near >= 1 && near <= 3) {
        const uint8_t fwd = facingMask(facing);
        const uint8_t left =
            (fwd == 0xC0) ? static_cast<uint8_t>(0x03) : static_cast<uint8_t>((fwd << 2) & 0xFF);
        const uint8_t right = (fwd == 0x03) ? static_cast<uint8_t>(0xC0) : (fwd >> 2);
        if ((currentCell & left & 0x55) != 0) {
            c2[0] = near;
        }
        if ((currentCell & right & 0x55) != 0) {
            be[0] = near;
        }
        if ((currentCell & fwd & 0x55) == 0) {
            c6[0] = 0;
        }
    }
}

OutdoorBiome sectorBiome(const Mm2AttribFile &attrib, uint8_t sectorLabel)
{
    if (sectorLabel == 0xC3) {
        return OutdoorBiome::Ocean;
    }
    for (int i = 0; i < MM2_ATTRIB_RECORD_COUNT; ++i) {
        const Mm2AttribRecord &rec = attrib.records[i];
        if (!mm2_attrib_is_outdoor(&rec)) {
            continue;
        }
        if (rec.raw[MM2_ATTRIB_OFF_COMPLEX_ID] == sectorLabel) {
            return biomeFromSurfaceFlag(mm2_attrib_surface_flag(&rec));
        }
    }
    return OutdoorBiome::Desert;
}

OutdoorBiome biomeForCell(uint8_t mapByte, uint8_t sectorLabel, const Mm2AttribFile &attrib)
{
    const uint8_t tid = mapByte & 0x1F;
    if (tid == 2 || tid == 3) {
        return remapOutdoorBiome(OutdoorBiome::Ocean, sectorLabel);
    }
    return remapOutdoorBiome(sectorBiome(attrib, sectorLabel), sectorLabel);
}

void columnBiomes(const OutdoorMapGrid &grid, int px, int py, int facing, const Mm2AttribFile &attrib,
                  std::array<OutdoorBiome, kOutdoorLaneCols> &out)
{
    const int8_t *b = directionBundle(facing);
    const int dx = b[0];
    const int dy = b[1];
    for (int col = 0; col < kOutdoorLaneCols; ++col) {
        const int wx = px + col * dx;
        const int wy = py + col * dy;
        const int sid = grid.screenIdAt(wx, wy);
        const uint8_t cell = grid.at(wx, wy);
        const uint8_t label = attrib.records[sid].raw[MM2_ATTRIB_OFF_COMPLEX_ID];
        out[static_cast<size_t>(col)] = biomeForCell(cell, label, attrib);
    }
}

uint8_t horizonIndex(uint8_t terrainClass)
{
    if (terrainClass == 0 || terrainClass > 3) {
        return kOutdoorHorizonNone;
    }
    return static_cast<uint8_t>(terrainClass - 1);
}

int horizonStartCol(const std::array<uint8_t, kOutdoorLaneCols> &l1)
{
    for (int col = 0; col < kOutdoorLaneCols; ++col) {
        if (l1[static_cast<size_t>(col)] != kOutdoorHorizonNone) {
            return col;
        }
    }
    return kOutdoorLaneCols;
}

void appendHorizon(OutdoorScene &scene, uint8_t idx, int frame, int x, int y)
{
    if (idx == kOutdoorHorizonNone || idx > 2) {
        return;
    }
    if (scene.num_horizon < static_cast<int>(scene.horizon.size())) {
        scene.horizon[scene.num_horizon++] = {OutdoorSpriteBlit::Kind::Horizon, OutdoorBiome::Desert,
                         static_cast<OutdoorHorizonSheet>(idx), frame, x, y};
    }
}

void buildDecorBlits(const std::array<uint8_t, kOutdoorLaneCols> &c6,
                     const std::array<uint8_t, kOutdoorLaneCols> &laneL2,
                     const std::array<uint8_t, kOutdoorLaneCols> &laneL3,
                     const std::array<OutdoorBiome, kOutdoorLaneCols> &colBiomes,
                     OutdoorScene &scene)
{
    using T = OutdoorHorizonTables;
    for (int col = kOutdoorLaneCols - 1; col >= 0; --col) {
        const size_t ci = static_cast<size_t>(col);
        const OutdoorBiome biome = colBiomes[ci];
        const bool mainA = c6[ci] > 3;
        const bool mainB = laneL2[ci] > 3;
        const bool mainC = laneL3[ci] > 3;
        const int y = T::kDecorY[col];

        auto pushDecor = [&](int frame, int x) {
            if (scene.num_decor < static_cast<int>(scene.decor.size())) {
                scene.decor[scene.num_decor++] = {OutdoorSpriteBlit::Kind::Decor, biome, OutdoorHorizonSheet::Outdoor1,
                                 frame, x, y};
            }
        };

        if (mainA) {
            pushDecor(col, T::kDecorX);
            if (mainB) {
                pushDecor(col + 12, T::kDecorX);
            }
            if (mainC) {
                pushDecor(col + 16, T::kDecorXBde[col] - kView3DOriginX);
            }
        } else {
            if (mainB) {
                pushDecor(col + 4, T::kDecorX);
            }
            if (mainC) {
                pushDecor(col + 8, T::kDecorX112);
            }
        }
    }
}

void buildHorizonBlits(std::array<uint8_t, kOutdoorLaneCols> c6,
                       std::array<uint8_t, kOutdoorLaneCols> laneL2,
                       std::array<uint8_t, kOutdoorLaneCols> laneL3,
                       OutdoorScene &scene)
{
    using T = OutdoorHorizonTables;
    std::array<uint8_t, kOutdoorLaneCols> l1{};
    std::array<uint8_t, kOutdoorLaneCols> l2{};
    std::array<uint8_t, kOutdoorLaneCols> l3{};
    for (int col = 0; col < kOutdoorLaneCols; ++col) {
        const size_t i = static_cast<size_t>(col);
        l1[i] = horizonIndex(c6[i]);
        l2[i] = horizonIndex(laneL2[i]);
        l3[i] = horizonIndex(laneL3[i]);
    }

    const int start = horizonStartCol(l1);
    int pivot = (start == 3 && l1[3] == kOutdoorHorizonNone) ? 3 : start;

    if (start < kOutdoorLaneCols && l1[static_cast<size_t>(start)] != kOutdoorHorizonNone) {
        appendHorizon(scene, l1[static_cast<size_t>(start)], T::kL1Frame[start], T::kL1X[start],
                      T::kL1Y[start]);
    } else if (start == kOutdoorLaneCols) {
        pivot = 3;
    }

    auto passL2 = [&](int col, int piv) {
        const bool special =
            (col != 0 && col == piv && l1[static_cast<size_t>(col)] != kOutdoorHorizonNone);
        if (special && l2[static_cast<size_t>(col - 1)] != kOutdoorHorizonNone) {
            l2[static_cast<size_t>(col)] = kOutdoorHorizonNone;
        }
        if (l2[static_cast<size_t>(col)] == kOutdoorHorizonNone) {
            return;
        }

        int frame = T::kL2Frame[col];
        int x = T::kL2X[col];
        int y = T::kL2Y[col];
        if (special && l2[static_cast<size_t>(piv)] != kOutdoorHorizonNone) {
            frame = T::kL2Frame[piv - 1];
            x = T::kL2X[piv - 1];
            y = T::kL1Y[piv];
        }
        if (col == 1 && l2[0] == kOutdoorHorizonNone) {
            x = 8;
        }
        if (col == 1 || (col == 2 && col == piv)) {
            x = 8;
        }
        appendHorizon(scene, l2[static_cast<size_t>(col)], frame, x, y);
    };

    auto passL3 = [&](int col, int piv) {
        const bool special =
            (col != 0 && col == piv && l1[static_cast<size_t>(col)] != kOutdoorHorizonNone);
        if (special && l3[static_cast<size_t>(col - 1)] != kOutdoorHorizonNone) {
            l3[static_cast<size_t>(col)] = kOutdoorHorizonNone;
        }
        if (l3[static_cast<size_t>(col)] == kOutdoorHorizonNone) {
            return;
        }

        int frame = T::kL3Frame[col];
        int x = T::kL3X[col];
        int y = T::kL3Y[col];
        if (special && l3[static_cast<size_t>(piv)] != kOutdoorHorizonNone) {
            frame = T::kL3Frame[piv - 1];
            x = T::kL3X[piv - 1];
            y = T::kL1Y[piv];
        }
        if (col == 1 && l3[0] == kOutdoorHorizonNone) {
            x = (col == piv) ? 0xB0 : 0x98;
        }
        appendHorizon(scene, l3[static_cast<size_t>(col)], frame, x, y);
    };

    for (int col = pivot; col >= 0; --col) {
        passL2(col, pivot);
    }
    for (int col = pivot; col >= 0; --col) {
        passL3(col, pivot);
    }
}

}  // namespace

const char *outdoorBiomeFilename(OutdoorBiome biome)
{
    switch (biome) {
        case OutdoorBiome::Ocean:
            return "ocean.32";
        case OutdoorBiome::Tundra:
            return "tundra.32";
        case OutdoorBiome::Swamp:
            return "swamp.32";
        default:
            return "desert.32";
    }
}

const char *outdoorHorizonFilename(OutdoorHorizonSheet sheet)
{
    switch (sheet) {
        case OutdoorHorizonSheet::Outdoor2:
            return "outdoor2.32";
        case OutdoorHorizonSheet::Outdoor3:
            return "outdoor3.32";
        default:
            return "outdoor1.32";
    }
}

OutdoorScene buildOutdoorScene(const world::MapWorld &world, const View3DCamera &cam)
{
    OutdoorMapGrid grid{world.mapFile(), world.attribFile(), world.currentScreen()};

    std::array<uint8_t, kHoodBytes> hood{};
    refreshOutdoorHood(grid, cam.x, cam.y, cam.facing, hood);

    OutdoorScene scene{};
    for (int i = 0; i < kOutdoorHoodRowLen; ++i) {
        scene.rowsRaw[0][static_cast<size_t>(i)] = hood[static_cast<size_t>(i)];
        scene.rowsRaw[1][static_cast<size_t>(i)] = hood[static_cast<size_t>(4 + i)];
        scene.rowsRaw[2][static_cast<size_t>(i)] = hood[static_cast<size_t>(8 + i)];
    }

    processTerrainRows(scene.rowsRaw, grid.at(cam.x, cam.y), cam.facing, scene.laneC6, scene.laneC2,
                       scene.laneBe);

    std::array<uint8_t, kOutdoorLaneCols> laneL2{};
    std::array<uint8_t, kOutdoorLaneCols> laneL3{};
    for (int i = 0; i < kOutdoorLaneCols; ++i) {
        laneL2[static_cast<size_t>(i)] = scene.laneC2[static_cast<size_t>(i)];
        laneL3[static_cast<size_t>(i)] = scene.laneBe[static_cast<size_t>(i)];
    }

    columnBiomes(grid, cam.x, cam.y, cam.facing, world.attribFile(), scene.columnBiomes);

    std::array<uint8_t, kOutdoorLaneCols> c6{};
    for (int i = 0; i < kOutdoorLaneCols; ++i) {
        c6[static_cast<size_t>(i)] = scene.laneC6[static_cast<size_t>(i)];
    }

    buildDecorBlits(c6, laneL2, laneL3, scene.columnBiomes, scene);
    buildHorizonBlits(c6, laneL2, laneL3, scene);
    return scene;
}

}  // namespace mm2::gfx
