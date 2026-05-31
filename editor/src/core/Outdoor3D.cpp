#include "core/Outdoor3D.h"

#include <cstdio>
#include <unordered_map>

namespace mm2 {

namespace {

constexpr int kHoodBytes = 13;

constexpr int8_t kBundleN[6] = {0, 1, -1, 0, 1, 0};
constexpr int8_t kBundleE[6] = {1, 0, 0, 1, 0, -1};
constexpr int8_t kBundleS[6] = {0, -1, 1, 0, -1, 0};
constexpr int8_t kBundleW[6] = {-1, 0, 0, -1, 0, 1};

constexpr int kStepDx[4] = {0, 1, 0, -1};
constexpr int kStepDy[4] = {1, 0, -1, 0};

const int8_t* directionBundle(int facing) {
    static const int8_t* kAll[4] = {kBundleN, kBundleE, kBundleS, kBundleW};
    return kAll[facing & 3];
}

uint8_t facingMask(int facing) {
    static constexpr uint8_t kMask[4] = {0xC0, 0x30, 0x0C, 0x03};
    return kMask[facing & 3];
}

int neighborScreen(const AttribFile& attrib, int screen, int slot) {
    const int n = attrib.screens[static_cast<size_t>(screen)].neighbor(slot);
    return (n >= 0 && n < kMapScreens) ? n : screen;
}

struct ResolvedCell {
    int screen = 0;
    int lx = 0;
    int ly = 0;
};

ResolvedCell resolveCell(const MapFile& map, const AttribFile& attrib, int screen, int x,
                         int y) {
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
    (void)map;
    return r;
}

std::array<uint8_t, 256> gTerrainLookup{};
bool gTerrainLookupLoaded = false;

OutdoorBiome biomeFromSurfaceFlag(uint8_t sf) {
    if (sf == 0xCC) return OutdoorBiome::Ocean;
    if (sf == 0x99) return OutdoorBiome::Tundra;
    if (sf == 0xBB) return OutdoorBiome::Swamp;
    return OutdoorBiome::Desert;
}

void refreshOutdoorHood(const OutdoorMapGrid& grid, int px, int py, int facing,
                        std::array<uint8_t, kHoodBytes>& hood) {
    hood.fill(0);
    const int8_t* b = directionBundle(facing);
    const int dx = b[0], dy = b[1];

    auto rowSampler = [&](int sx, int sy, int outOff) {
        int x = sx, y = sy;
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

void processTerrainRows(const std::array<std::array<uint8_t, kOutdoorHoodRowLen>, 3>& rows,
                        uint8_t currentCell, int facing,
                        const std::array<uint8_t, 256>& lookup,
                        std::array<uint8_t, kOutdoorHoodRowLen>& c6,
                        std::array<uint8_t, kOutdoorHoodRowLen>& c2,
                        std::array<uint8_t, kOutdoorHoodRowLen>& be) {
    for (int i = 0; i < kOutdoorHoodRowLen; ++i) {
        c6[static_cast<size_t>(i)] = lookup[rows[0][static_cast<size_t>(i)] & 0x1F];
        c2[static_cast<size_t>(i)] = lookup[rows[1][static_cast<size_t>(i)] & 0x1F];
        be[static_cast<size_t>(i)] = lookup[rows[2][static_cast<size_t>(i)] & 0x1F];
    }

    const uint8_t near = c6[0];
    if (near >= 1 && near <= 3) {
        const uint8_t fwd = facingMask(facing);
        const uint8_t left =
            (fwd == 0xC0) ? static_cast<uint8_t>(0x03) : static_cast<uint8_t>((fwd << 2) & 0xFF);
        const uint8_t right = (fwd == 0x03) ? static_cast<uint8_t>(0xC0) : (fwd >> 2);
        if ((currentCell & left & 0x55) != 0) c2[0] = near;
        if ((currentCell & right & 0x55) != 0) be[0] = near;
        if ((currentCell & fwd & 0x55) == 0) c6[0] = 0;
    }
}

std::unordered_map<uint8_t, OutdoorBiome> buildSectorLabelBiomes(const AttribFile& attrib) {
    std::unordered_map<uint8_t, OutdoorBiome> out;
    out[0xC3] = OutdoorBiome::Ocean;
    for (const AttribScreen& rec : attrib.screens) {
        if (!rec.isOutside()) continue;
        const uint8_t label = rec.outsideLabel();
        if (out.find(label) != out.end()) continue;
        out[label] = biomeFromSurfaceFlag(rec.outsideFlag());
    }
    return out;
}

OutdoorBiome biomeForCell(uint8_t mapByte, uint8_t sectorLabel,
                          const std::unordered_map<uint8_t, OutdoorBiome>& sectorTable) {
    const uint8_t tid = mapByte & 0x1F;
    if (tid == 2 || tid == 3) return remapOutdoorBiome(OutdoorBiome::Ocean, sectorLabel);
    auto it = sectorTable.find(sectorLabel);
    const OutdoorBiome base =
        (it != sectorTable.end()) ? it->second : OutdoorBiome::Desert;
    return remapOutdoorBiome(base, sectorLabel);
}

void columnBiomes(const OutdoorMapGrid& grid, int px, int py, int facing,
                  const AttribFile& attrib,
                  const std::unordered_map<uint8_t, OutdoorBiome>& sectorTable,
                  std::array<OutdoorBiome, kOutdoorLaneCols>& out) {
    const int8_t* b = directionBundle(facing);
    const int dx = b[0], dy = b[1];
    for (int col = 0; col < kOutdoorLaneCols; ++col) {
        const int wx = px + col * dx;
        const int wy = py + col * dy;
        const int sid = grid.screenIdAt(wx, wy);
        const uint8_t cell = grid.at(wx, wy);
        const uint8_t label = attrib.screens[static_cast<size_t>(sid)].outsideLabel();
        out[static_cast<size_t>(col)] = biomeForCell(cell, label, sectorTable);
    }
}

uint8_t horizonIndex(uint8_t terrainClass) {
    if (terrainClass == 0 || terrainClass > 3) return kOutdoorHorizonNone;
    return static_cast<uint8_t>(terrainClass - 1);
}

int horizonStartCol(const std::array<uint8_t, kOutdoorLaneCols>& l1) {
    for (int col = 0; col < kOutdoorLaneCols; ++col) {
        if (l1[static_cast<size_t>(col)] != kOutdoorHorizonNone) return col;
    }
    return kOutdoorLaneCols;
}

void appendHorizon(std::vector<OutdoorSpriteBlit>& blits, uint8_t idx, int frame, int x, int y) {
    if (idx == kOutdoorHorizonNone || idx > 2) return;
    blits.push_back({OutdoorSpriteBlit::Kind::Horizon,
                     OutdoorBiome::Desert,
                     static_cast<OutdoorHorizonSheet>(idx),
                     frame,
                     x,
                     y});
}

void buildDecorBlits(const std::array<uint8_t, kOutdoorLaneCols>& c6,
                     const std::array<uint8_t, kOutdoorLaneCols>& laneL2,
                     const std::array<uint8_t, kOutdoorLaneCols>& laneL3,
                     const std::array<OutdoorBiome, kOutdoorLaneCols>& colBiomes,
                     std::vector<OutdoorSpriteBlit>& blits) {
    using T = OutdoorHorizonTables;
    for (int col = kOutdoorLaneCols - 1; col >= 0; --col) {
        const size_t ci = static_cast<size_t>(col);
        const OutdoorBiome biome = colBiomes[ci];
        const bool mainA = c6[ci] > 3;
        const bool mainB = laneL2[ci] > 3;
        const bool mainC = laneL3[ci] > 3;
        const int y = T::kDecorY[col];

        auto pushDecor = [&](int frame, int x) {
            blits.push_back(
                {OutdoorSpriteBlit::Kind::Decor, biome, OutdoorHorizonSheet::Outdoor1, frame, x, y});
        };

        if (mainA) {
            pushDecor(col, T::kDecorX);
            if (mainB) pushDecor(col + 12, T::kDecorX);
            if (mainC) pushDecor(col + 16, T::kDecorXBde[col] - kView3DOriginX);
        } else {
            if (mainB) pushDecor(col + 4, T::kDecorX);
            if (mainC) pushDecor(col + 8, T::kDecorX112);
        }
    }
}

void buildHorizonBlits(std::array<uint8_t, kOutdoorLaneCols> c6,
                       std::array<uint8_t, kOutdoorLaneCols> laneL2,
                       std::array<uint8_t, kOutdoorLaneCols> laneL3,
                       std::vector<OutdoorSpriteBlit>& blits) {
    using T = OutdoorHorizonTables;
    std::array<uint8_t, kOutdoorLaneCols> l1{}, l2{}, l3{};
    for (int col = 0; col < kOutdoorLaneCols; ++col) {
        const size_t i = static_cast<size_t>(col);
        l1[i] = horizonIndex(c6[i]);
        l2[i] = horizonIndex(laneL2[i]);
        l3[i] = horizonIndex(laneL3[i]);
    }

    const int start = horizonStartCol(l1);
    int pivot = (start == 3 && l1[3] == kOutdoorHorizonNone) ? 3 : start;

    if (start < kOutdoorLaneCols && l1[static_cast<size_t>(start)] != kOutdoorHorizonNone) {
        appendHorizon(blits, l1[static_cast<size_t>(start)], T::kL1Frame[start],
                      T::kL1X[start], T::kL1Y[start]);
    } else if (start == kOutdoorLaneCols) {
        pivot = 3;
    }

    auto passL2 = [&](int col, int piv) {
        const bool special = (col != 0 && col == piv && l1[static_cast<size_t>(col)] != kOutdoorHorizonNone);
        if (special && l2[static_cast<size_t>(col - 1)] != kOutdoorHorizonNone)
            l2[static_cast<size_t>(col)] = kOutdoorHorizonNone;
        if (l2[static_cast<size_t>(col)] == kOutdoorHorizonNone) return;

        int frame = T::kL2Frame[col];
        int x = T::kL2X[col];
        int y = T::kL2Y[col];
        if (special && l2[static_cast<size_t>(piv)] != kOutdoorHorizonNone) {
            frame = T::kL2Frame[piv - 1];
            x = T::kL2X[piv - 1];
            y = T::kL1Y[piv];
        }
        if (col == 1 && l2[0] == kOutdoorHorizonNone) x = 8;
        if (col == 1 || (col == 2 && col == piv)) x = 8;
        appendHorizon(blits, l2[static_cast<size_t>(col)], frame, x, y);
    };

    auto passL3 = [&](int col, int piv) {
        const bool special = (col != 0 && col == piv && l1[static_cast<size_t>(col)] != kOutdoorHorizonNone);
        if (special && l3[static_cast<size_t>(col - 1)] != kOutdoorHorizonNone)
            l3[static_cast<size_t>(col)] = kOutdoorHorizonNone;
        if (l3[static_cast<size_t>(col)] == kOutdoorHorizonNone) return;

        int frame = T::kL3Frame[col];
        int x = T::kL3X[col];
        int y = T::kL3Y[col];
        if (special && l3[static_cast<size_t>(piv)] != kOutdoorHorizonNone) {
            frame = T::kL3Frame[piv - 1];
            x = T::kL3X[piv - 1];
            y = T::kL1Y[piv];
        }
        if (col == 1 && l3[0] == kOutdoorHorizonNone) x = (col == piv) ? 0xB0 : 0x98;
        appendHorizon(blits, l3[static_cast<size_t>(col)], frame, x, y);
    };

    for (int col = pivot; col >= 0; --col) passL2(col, pivot);
    for (int col = pivot; col >= 0; --col) passL3(col, pivot);
}

}  // namespace

const char* outdoorBiomeFilename(OutdoorBiome biome) {
    switch (biome) {
        case OutdoorBiome::Ocean:  return "ocean.32";
        case OutdoorBiome::Tundra: return "tundra.32";
        case OutdoorBiome::Swamp:  return "swamp.32";
        default:                    return "desert.32";
    }
}

const char* outdoorHorizonFilename(OutdoorHorizonSheet sheet) {
    switch (sheet) {
        case OutdoorHorizonSheet::Outdoor2: return "outdoor2.32";
        case OutdoorHorizonSheet::Outdoor3: return "outdoor3.32";
        default:                          return "outdoor1.32";
    }
}

bool loadOutdoorTerrainLookup(const std::string& dataBinPath, std::array<uint8_t, 256>& out) {
    Bytes blob;
    if (!readFile(dataBinPath, blob)) return false;
    constexpr size_t kA4 = 0x7FFE;
    constexpr size_t kOff = kA4 - 0x720A;
    if (blob.size() < kOff + 256) return false;
    std::copy(blob.begin() + static_cast<std::ptrdiff_t>(kOff),
              blob.begin() + static_cast<std::ptrdiff_t>(kOff + 256), out.begin());
    return true;
}

bool initOutdoorTerrainLookup(const std::string& dataBinPath) {
    if (!loadOutdoorTerrainLookup(dataBinPath, gTerrainLookup)) return false;
    gTerrainLookupLoaded = true;
    return true;
}

const std::array<uint8_t, 256>& outdoorTerrainLookup() {
    if (!gTerrainLookupLoaded)
        initOutdoorTerrainLookup("EXTRACTED/ghidra/mm2_data_00.bin");
    return gTerrainLookup;
}

OutdoorBiome remapOutdoorBiome(OutdoorBiome sheet, uint8_t sectorLabel) {
    if (sectorLabel == 0xC3) return sheet;
    switch (sheet) {
        case OutdoorBiome::Desert: return OutdoorBiome::Ocean;
        case OutdoorBiome::Ocean:  return OutdoorBiome::Tundra;
        case OutdoorBiome::Tundra: return OutdoorBiome::Desert;
        default:                  return sheet;
    }
}

int OutdoorMapGrid::screenIdAt(int x, int y) const {
    return resolveCell(map, attrib, screen, x, y).screen;
}

uint8_t OutdoorMapGrid::at(int x, int y) const {
    const ResolvedCell r = resolveCell(map, attrib, screen, x, y);
    if (r.lx < 0 || r.ly < 0 || r.lx >= kMapGridDim || r.ly >= kMapGridDim) return 0;
    if (r.screen < 0 || r.screen >= kMapScreens) return 0;
    return map.screens[static_cast<size_t>(r.screen)].visualAt(r.lx, r.ly);
}

OutdoorScene buildOutdoorScene(const OutdoorMapGrid& grid, int px, int py, int facing,
                               const AttribFile& attrib) {
    std::array<uint8_t, kHoodBytes> hood{};
    refreshOutdoorHood(grid, px, py, facing, hood);

    OutdoorScene scene{};
    for (int i = 0; i < kOutdoorHoodRowLen; ++i) {
        scene.rowsRaw[0][static_cast<size_t>(i)] = hood[static_cast<size_t>(i)];
        scene.rowsRaw[1][static_cast<size_t>(i)] = hood[static_cast<size_t>(4 + i)];
        scene.rowsRaw[2][static_cast<size_t>(i)] = hood[static_cast<size_t>(8 + i)];
    }

    const auto& lookup = outdoorTerrainLookup();
    processTerrainRows(scene.rowsRaw, grid.at(px, py), facing, lookup, scene.laneC6, scene.laneC2,
                       scene.laneBe);

    std::array<uint8_t, kOutdoorLaneCols> laneL2{}, laneL3{};
    for (int i = 0; i < kOutdoorLaneCols; ++i) {
        laneL2[static_cast<size_t>(i)] = scene.laneC2[static_cast<size_t>(i)];
        laneL3[static_cast<size_t>(i)] = scene.laneBe[static_cast<size_t>(i)];
    }

    const auto sectorTable = buildSectorLabelBiomes(attrib);
    columnBiomes(grid, px, py, facing, attrib, sectorTable, scene.columnBiomes);

    std::array<uint8_t, kOutdoorLaneCols> c6{};
    for (int i = 0; i < kOutdoorLaneCols; ++i)
        c6[static_cast<size_t>(i)] = scene.laneC6[static_cast<size_t>(i)];

    buildDecorBlits(c6, laneL2, laneL3, scene.columnBiomes, scene.decor);
    buildHorizonBlits(c6, laneL2, laneL3, scene.horizon);
    return scene;
}

void stepOutdoorParty(int facing, int& x, int& y, int& screen, const AttribFile& attrib) {
    x += kStepDx[facing & 3];
    y += kStepDy[facing & 3];
    const AttribScreen& rec = attrib.screens[static_cast<size_t>(screen)];
    if (x < 0) {
        const int n = rec.neighbor(3);
        if (n >= 0 && n < kMapScreens) {
            screen = n;
            x = 15;
        }
    } else if (x >= kMapGridDim) {
        const int n = rec.neighbor(1);
        if (n >= 0 && n < kMapScreens) {
            screen = n;
            x = 0;
        }
    }
    if (y < 0) {
        const int n = rec.neighbor(2);
        if (n >= 0 && n < kMapScreens) {
            screen = n;
            y = 15;
        }
    } else if (y >= kMapGridDim) {
        const int n = rec.neighbor(0);
        if (n >= 0 && n < kMapScreens) {
            screen = n;
            y = 0;
        }
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= kMapGridDim) x = kMapGridDim - 1;
    if (y >= kMapGridDim) y = kMapGridDim - 1;
}

void dumpOutdoorTrace(const OutdoorScene& scene, int screen, int x, int y, int facing, FILE* out) {
    static const char* kFace[] = {"N", "E", "S", "W"};
    std::fprintf(out, "screen %d  (%d,%d)  facing %s\n", screen, x, y, kFace[facing & 3]);
    std::fprintf(out, "  terrain C6:");
    for (int i = 0; i < kOutdoorLaneCols; ++i) std::fprintf(out, " %u", scene.laneC6[i]);
    std::fprintf(out, "  C2:");
    for (int i = 0; i < kOutdoorLaneCols; ++i) std::fprintf(out, " %u", scene.laneC2[i]);
    std::fprintf(out, "  BE:");
    for (int i = 0; i < kOutdoorLaneCols; ++i) std::fprintf(out, " %u", scene.laneBe[i]);
    std::fprintf(out, "\n");
    std::fprintf(out, "  decor blits: %zu\n", scene.decor.size());
    for (const OutdoorSpriteBlit& b : scene.decor)
        std::fprintf(out, "    %s f%d @ (%d,%d)\n", outdoorBiomeFilename(b.biome), b.frame, b.x,
                     b.y);
    std::fprintf(out, "  horizon blits: %zu\n", scene.horizon.size());
    for (const OutdoorSpriteBlit& b : scene.horizon)
        std::fprintf(out, "    %s f%d @ (%d,%d)\n", outdoorHorizonFilename(b.horizon), b.frame,
                     b.x, b.y);
}

}  // namespace mm2
