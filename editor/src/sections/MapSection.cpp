#include "sections/MapSection.h"

#include <cstdio>

#include "core/AreaNames.h"
#include "core/Cartography.h"
#include "imgui.h"
#include "widgets/Texture.h"
#include "widgets/UiLayout.h"

namespace mm2 {

MapSection::~MapSection() { releaseTextures(); }

void MapSection::Sheet::release() {
    for (unsigned int t : tex) freeTexture(t);
    tex.clear();
    img = GfxImage{};
}

void MapSection::releaseTextures() {
    outb_.release();
    townb_.release();
    sky_.release();
    floorTown_.release();
    floorCave_.release();
    floorCastle_.release();
    floorOut_.release();
    wallTown_.release();
    wallCave_.release();
    wallCastle_.release();
}

void MapSection::loadTilesets(const std::string& dataDir) {
    releaseTextures();
    auto loadSheet = [&](const std::string& name, Sheet& s) {
        if (!gfxLoad(dataDir + "/" + name, false, s.img)) return;
        for (const GfxFrame& fr : s.img.frames)
            s.tex.push_back(makeTextureRGBA(fr.rgba.data(), fr.width, fr.height));
    };
    loadSheet("outb.32", outb_);
    loadSheet("townb.32", townb_);
    loadSheet("sky.32", sky_);
    loadSheet("townf.32", floorTown_);
    loadSheet("cavef.32", floorCave_);
    loadSheet("castlef.32", floorCastle_);
    loadSheet("outf.32", floorOut_);
    loadSheet("town.32", wallTown_);
    loadSheet("cave.32", wallCave_);
    loadSheet("castle.32", wallCastle_);
    loadSheet("townt.32", torchTown_);
    loadSheet("cavet.32", torchCave_);
    loadSheet("castlet.32", torchCastle_);
}

bool MapSection::load(const std::string& dataDir) {
    loaded = file_.load(dataDir + "/" + fileName());
    attribLoaded_ = attrib_.load(dataDir + "/attrib.dat");
    loadTilesets(dataDir);
    dirty = false;
    return loaded;
}

bool MapSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

bool MapSection::isOutdoor(int screen) const {
    // Elemental planes (41..44) always render with the outdoor tileset.
    if (screen >= 41 && screen <= 44) return true;
    if (attribLoaded_) return attrib_.screens[screen].isOutside();
    // Without attrib data, fall back to the named interior areas (towns,
    // caverns, castles and dungeons all carry real names); unnamed indices are
    // overland surface tiles.
    return areaNameRaw(screen)[0] == '\0';
}

MapSection::Env MapSection::envOf(int screen) const {
    if (isOutdoor(screen)) return Env::Outdoor;
    if (attribLoaded_) {
        // attrib +0x01 map category: 2 cavern, 4 castle; +0x03 env type:
        // 18 cavern, 19/20 castle. Everything else (towns, dungeons) renders
        // with the town backdrop set, matching the engine's env dispatcher.
        uint8_t cat = attrib_.screens[screen].mapCategory();
        uint8_t env = attrib_.screens[screen].envType();
        if (cat == 2 || env == 18) return Env::Cavern;
        if (cat == 4 || env == 19 || env == 20) return Env::Castle;
    }
    return Env::Town;
}

void MapSection::drawWindow() {
    Env env = envOf(screen_);
    const Sheet* floor = &floorTown_;
    const char* floorName = "townf.32";
    switch (env) {
        case Env::Cavern:  floor = &floorCave_;   floorName = "cavef.32";   break;
        case Env::Castle:  floor = &floorCastle_; floorName = "castlef.32"; break;
        case Env::Outdoor: floor = &floorOut_;    floorName = "outf.32";    break;
        case Env::Town:    break;
    }
    const char* envName = env == Env::Cavern ? "Cavern"
                          : env == Env::Castle ? "Castle"
                          : env == Env::Outdoor ? "Outdoor" : "Town";

    // The engine composites the scene from two 208x60 backdrops: sky.32 (the
    // ceiling/sky, top) over the per-environment floor image (bottom), then
    // paints perspective walls on top. We reproduce the backdrop here.
    const float W = 208.0f, H = 60.0f;
    float z = viewZoom_;

    ImGui::Text("Backdrop: sky.32 (top) + %s (bottom)  -  %s", floorName, envName);
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("View zoom", &viewZoom_, 1.0f, 4.0f, "%.0fx");
    if (sky_.tex.size() > 1) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
        ImGui::SliderInt("Sky frame", &skyFrame_, 0, static_cast<int>(sky_.tex.size()) - 1);
    }

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto blit = [&](const Sheet& s, int frame, float x, float y) {
        if (frame < 0 || frame >= static_cast<int>(s.tex.size())) return;
        const GfxFrame& fr = s.img.frames[frame];
        ImVec2 a(origin.x + x * z, origin.y + y * z);
        ImVec2 b(a.x + fr.width * z, a.y + fr.height * z);
        dl->AddImage(static_cast<ImTextureID>(s.tex[frame]), a, b);
    };

    // Reserve the canvas and draw a border, then the two backdrop halves.
    ImVec2 canvas(W * z, (H * 2) * z);
    dl->AddRectFilled(origin, ImVec2(origin.x + canvas.x, origin.y + canvas.y),
                      IM_COL32(0, 0, 0, 255));
    int sf = sky_.tex.empty() ? -1 : (skyFrame_ < static_cast<int>(sky_.tex.size()) ? skyFrame_ : 0);
    blit(sky_, sf, 0.0f, 0.0f);     // ceiling / sky
    blit(*floor, 0, 0.0f, H);       // floor
    dl->AddRect(origin, ImVec2(origin.x + canvas.x, origin.y + canvas.y),
                IM_COL32(120, 120, 120, 255));
    ImGui::Dummy(canvas);

    if (sky_.tex.empty() || floor->tex.empty())
        ImGui::TextDisabled("(sky.32 / floor sheet not found in data folder)");
    ImGui::TextDisabled(
        "Walls (town/cave/castle.32) draw over this per the depth tables; see "
        "docs/15-3d-view-and-game-screen.md.");
}

void MapSection::stepCameraInDirection(int dir) {
    static const int kDx[] = {0, 1, 0, -1};
    static const int kDy[] = {1, 0, -1, 0};

    int nx = camera_.x + kDx[dir & 3];
    int ny = camera_.y + kDy[dir & 3];
    int ns = screen_;

    if (attribLoaded_) {
        const AttribScreen& rec = attrib_.screens[static_cast<size_t>(screen_)];
        if (nx < 0) {
            int west = rec.neighbor(3);
            if (west >= 0 && west < kMapScreens) {
                ns = west;
                nx = kMapGridDim - 1;
            }
        } else if (nx >= kMapGridDim) {
            int east = rec.neighbor(1);
            if (east >= 0 && east < kMapScreens) {
                ns = east;
                nx = 0;
            }
        }
        if (ny < 0) {
            int south = rec.neighbor(2);
            if (south >= 0 && south < kMapScreens) {
                ns = south;
                ny = kMapGridDim - 1;
            }
        } else if (ny >= kMapGridDim) {
            int north = rec.neighbor(0);
            if (north >= 0 && north < kMapScreens) {
                ns = north;
                ny = 0;
            }
        }
    }

    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= kMapGridDim) nx = kMapGridDim - 1;
    if (ny >= kMapGridDim) ny = kMapGridDim - 1;
    screen_ = ns;
    camera_.x = nx;
    camera_.y = ny;
}

void MapSection::handleView3DKeyboardInput() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    if (ImGui::IsAnyItemActive()) return;
    if (ImGui::GetIO().WantTextInput) return;

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) ||
        ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        camera_.facing = (camera_.facing + 3) & 3;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) ||
        ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        camera_.facing = (camera_.facing + 1) & 3;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) ||
        ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        stepCameraInDirection(camera_.facing);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false) ||
        ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        stepCameraInDirection((camera_.facing + 2) & 3);
    }
}

void MapSection::drawView3D() {
    Env env = envOf(screen_);
    if (env == Env::Outdoor) {
        ImGui::TextDisabled("Outdoor areas use layered horizon tiles (outdoor1-3.32); "
                            "interior 3D view applies to town/cavern/castle maps.");
        drawWindow();
        return;
    }

    const Sheet* floor = &floorTown_;
    const Sheet* walls = &wallTown_;
    const char* wallName = "town.32";
    switch (env) {
        case Env::Cavern:
            floor = &floorCave_;
            walls = &wallCave_;
            wallName = "cave.32";
            break;
        case Env::Castle:
            floor = &floorCastle_;
            walls = &wallCastle_;
            wallName = "castle.32";
            break;
        default:
            break;
    }

    // Keep editor render aligned with tools/view3d_indoor.py:
    // indoor path uses sky frame 0 and does not apply roof-bit switching.
    int skyFrame = 0;

    static const char* kFacing[] = {"North", "East", "South", "West"};
    ImGui::Text("Camera  (%d, %d)  facing %s", camera_.x, camera_.y,
                kFacing[camera_.facing & 3]);

    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("X##cam", &camera_.x);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Y##cam", &camera_.y);
    if (camera_.x < 0) camera_.x = 0;
    if (camera_.y < 0) camera_.y = 0;
    if (camera_.x >= kMapGridDim) camera_.x = kMapGridDim - 1;
    if (camera_.y >= kMapGridDim) camera_.y = kMapGridDim - 1;

    ImGui::SameLine();
    if (ImGui::Button("Turn L")) camera_.facing = (camera_.facing + 3) & 3;
    ImGui::SameLine();
    if (ImGui::Button("Turn R")) camera_.facing = (camera_.facing + 1) & 3;
    ImGui::SameLine();
    if (ImGui::Button("Step F")) stepCameraInDirection(camera_.facing);
    ImGui::SameLine();
    if (ImGui::Button("Step B")) stepCameraInDirection((camera_.facing + 2) & 3);

    handleView3DKeyboardInput();

    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("View zoom##3d", &viewZoom_, 1.0f, 4.0f, "%.0fx");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Map zoom##mini", &zoom_, 1.0f, 4.0f, "%.0fx");

    const float W = static_cast<float>(kView3DViewportW);
    const float H = static_cast<float>(kView3DFloorY + 60 - kView3DSkyY);
    float z = viewZoom_;

    View3DMapBuffers bufs =
        buildMapBuffers(file_, screen_, attribLoaded_ ? &attrib_ : nullptr);
    View3DScene scene = buildView3DScene(bufs, camera_);

    ImGui::BeginGroup();

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto blit = [&](const Sheet& sheet, int frame, float x, float y) {
        if (frame < 0 || frame >= static_cast<int>(sheet.tex.size())) return;
        const GfxFrame& fr = sheet.img.frames[frame];
        ImVec2 a(origin.x + x * z, origin.y + y * z);
        ImVec2 b(a.x + fr.width * z, a.y + fr.height * z);
        dl->AddImage(static_cast<ImTextureID>(sheet.tex[frame]), a, b);
    };

    ImVec2 canvas(W * z, H * z);
    dl->AddRectFilled(origin, ImVec2(origin.x + canvas.x, origin.y + canvas.y),
                      IM_COL32(0, 0, 0, 255));

    // 1. floor backdrop — townf.32 frame 0 at (8, 68)
    blit(*floor, 0, static_cast<float>(kView3DOriginX), static_cast<float>(kView3DFloorY));
    // 2. sky backdrop — fixed frame 0 to match the Python indoor viewer
    blit(sky_, skyFrame, static_cast<float>(kView3DOriginX), static_cast<float>(kView3DSkyY));
    // 3. walls (town/cave/castle.32) — painted over sky/floor like view_3d_master @0x2ECE
    for (const View3DBlit& wb : scene.blits)
        blit(*walls, wb.frame, static_cast<float>(wb.x), static_cast<float>(wb.y));

    dl->AddRect(origin, ImVec2(origin.x + canvas.x, origin.y + canvas.y),
                IM_COL32(120, 120, 120, 255));
    ImGui::Dummy(canvas);

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 14.0f);
    ImGui::BeginGroup();
    ImGui::TextDisabled("Minimap");
    ImGui::Spacing();
    drawMinimap();
    ImGui::EndGroup();

    if (walls->tex.empty())
        ImGui::TextDisabled("(%s not found in data folder)", wallName);
    ImGui::TextDisabled("%zu wall sprite(s)  |  visual page  |  click Tiles tab to move camera",
                        scene.blits.size());
    if (ImGui::TreeNode("Wall blits (latX, row)")) {
        for (const View3DBlit& b : scene.blits)
            ImGui::Text("(%d,%d) %s  f%d @ (%d,%d)", b.latX, b.latRow, view3dLatXName(b.latX),
                        b.frame, b.x, b.y);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Frustum debug (map read)")) {
        static const char* kSlot[] = {
            "F20", "F1F", "F1E", "F1D", "F1C", "F1B", "F1A", "F19", "F18", "F17",
            "F16", "F15", "F14", "F13", "F12", "F11", "F10", "F0F", "F0E", "F0D",
        };
        static const char* kHood[] = {
            "C0@", "C1", "C2", "C3", "L0", "L1", "L2", "L3", "R0", "R1", "R2", "R3", "R4",
        };
        auto wallName = [](int v) -> const char* {
            switch (v) {
                case 0: return "open";
                case 1: return "wall";
                case 2: return "torch";
                case 3: return "door";
                default: return "?";
            }
        };
        ImGui::TextDisabled("Hood = page-0 visual bytes (N@0 E@2 S@4 W@6)");
        for (int i = 0; i < static_cast<int>(scene.hood.size()); ++i) {
            const uint8_t v = scene.hood[static_cast<size_t>(i)];
            ImGui::Text("[%2d] %s  %02X  N=%d E=%d S=%d W=%d", i, kHood[i], v,
                        visualWallN(v), visualWallE(v), visualWallS(v), visualWallW(v));
        }
        ImGui::Separator();
        ImGui::TextDisabled("Non-zero frustum slots → blits above");
        for (int i = 0; i < static_cast<int>(scene.slots.size()); ++i) {
            const uint8_t s = scene.slots[static_cast<size_t>(i)];
            if (s != 0)
                ImGui::Text("%s = %u (%s)", kSlot[i], s, wallName(s));
        }
        ImGui::TreePop();
    }
}

void MapSection::drawGrid(const char* id, std::array<uint8_t, kMapPageSize>& page, int& selTile,
                          bool markEvents) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cell = 28.0f;
    // On-disk row 0 is the SOUTH (bottom) row, so draw it last: screen row y
    // shows data row (15 - y). Matches the game/b3dmm2 editor which iterate
    // rows 15->0 and index with c=((15-y)*16)+x.
    for (int y = 0; y < kMapGridDim; ++y) {
        for (int x = 0; x < kMapGridDim; ++x) {
            int idx = (kMapGridDim - 1 - y) * kMapGridDim + x;
            uint8_t v = page[idx];
            // Tint by byte value so patterns are visible.
            float h = (v / 255.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(h, 0.45f, 0.55f).Value);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(h, 0.6f, 0.7f).Value);
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%02X", v);
            ImGui::PushID(idx);
            if (ImGui::Button(lbl, ImVec2(cell, cell))) {
                selTile = idx;
                camera_.x = x;
                camera_.y = kMapGridDim - 1 - y;
            }
            if (markEvents && collisionHasEvent(v)) {
                ImVec2 mn = ImGui::GetItemRectMin();
                dl->AddCircleFilled(ImVec2(mn.x + 5.0f, mn.y + 5.0f), 3.0f,
                                    IM_COL32(255, 80, 80, 255));
            }
            ImGui::PopID();
            ImGui::PopStyleColor(2);
            if (x != kMapGridDim - 1) ImGui::SameLine();
        }
    }
    ImGui::Text("Selected tile (%d,%d) = 0x%02X", selTile % kMapGridDim, selTile / kMapGridDim,
                page[selTile]);
    int v = page[selTile];
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Tile value", &v)) {
        page[selTile] = static_cast<uint8_t>(v & 0xFF);
        dirty = true;
    }
    if (markEvents)
        drawCollisionDecode(page[selTile]);
    else
        drawVisualDecode(page[selTile]);
    ImGui::PopID();
}

void MapSection::drawCartoGrid(const char* id, std::array<uint8_t, kMapPageSize>& page,
                               int& selTile, bool forceTownb, bool markEvents) {
    bool useTownb = forceTownb || !isOutdoor(screen_);
    const std::vector<unsigned int>& tex = useTownb ? townb_.tex : outb_.tex;
    if (tex.empty()) {  // tileset missing -> fall back to the hex grid
        drawGrid(id, page, selTile, markEvents);
        return;
    }

    const int frameCount = static_cast<int>(tex.size());
    const float cw = 14.0f * zoom_;
    const float ch = 11.0f * zoom_;
    ImGui::TextDisabled("%s.32  (%d frames)", useTownb ? "townb" : "outb", frameCount);

    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // On-disk row 0 is south (bottom): draw it last so screen row y shows data
    // row (15 - y), matching the game's north-up view.
    for (int y = 0; y < kMapGridDim; ++y) {
        for (int x = 0; x < kMapGridDim; ++x) {
            int idx = (kMapGridDim - 1 - y) * kMapGridDim + x;
            // Mask the event flag so the auto-map tile reflects walls only.
            uint8_t cellByte = markEvents ? (page[idx] & kCollisionWallMask) : page[idx];
            int frame = cartoFrame(screen_, cellByte, !useTownb);
            if (frame < 0 || frame >= frameCount) frame = 0;
            unsigned int t = tex[frame];
            ImGui::PushID(idx);
            if (ImGui::ImageButton("##t", static_cast<ImTextureID>(t), ImVec2(cw, ch))) {
                selTile = idx;
                camera_.x = x;
                camera_.y = kMapGridDim - 1 - y;
            }
            if (markEvents && collisionHasEvent(page[idx])) {
                ImVec2 mn = ImGui::GetItemRectMin();
                dl->AddCircleFilled(ImVec2(mn.x + 4.0f, mn.y + 4.0f), 3.0f,
                                    IM_COL32(255, 80, 80, 255));
            }
            if (idx == selTile)
                dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                            IM_COL32(255, 220, 0, 255), 0.0f, 0, 2.0f);
            ImGui::PopID();
            if (x != kMapGridDim - 1) ImGui::SameLine();
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::PopID();

    uint8_t v = page[selTile];
    ImGui::Text("Selected tile (%d,%d) = 0x%02X -> frame %d", selTile % kMapGridDim,
                selTile / kMapGridDim, v, cartoFrame(screen_, v, !useTownb));
    int iv = v;
    ImGui::SetNextItemWidth(120);
    ImGui::PushID(id);
    if (ImGui::InputInt("Tile value", &iv)) {
        page[selTile] = static_cast<uint8_t>(iv & 0xFF);
        dirty = true;
    }
    ImGui::PopID();
    if (markEvents) drawCollisionDecode(page[selTile]);
}

void MapSection::drawMinimap() {
    // Auto-map uses page 0 (visual tiles) — same path as drawCartoGrid on the
    // Visual tab.  Event dots still come from page 1 (collision 0x80 flag).
    const bool  outdoor    = isOutdoor(screen_);
    const bool  useTownb   = !outdoor;
    const std::vector<unsigned int>& tex = useTownb ? townb_.tex : outb_.tex;
    const int   dim        = kMapGridDim;
    const float tw         = 14.0f * zoom_;
    const float th         = 11.0f * zoom_;
    const float mapW       = dim * tw;
    const float mapH       = dim * th;
    const int   frameCount = static_cast<int>(tex.size());
    const bool  useTiles   = !tex.empty();

    const auto& vis = file_.screens[static_cast<size_t>(screen_)].visual;
    const auto& col = file_.screens[static_cast<size_t>(screen_)].collision;

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      origin = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(origin, ImVec2(origin.x + mapW, origin.y + mapH),
                      IM_COL32(12, 12, 22, 255));

    for (int cy = 0; cy < dim; ++cy) {      // cy=0 = north (top of minimap)
        for (int cx = 0; cx < dim; ++cx) {  // cx=0 = west (left of minimap)
            // Disk row 0 = south → screen bottom; cy=0 ↔ disk row 15 (north).
            int     diskY = dim - 1 - cy;
            int     idx   = diskY * dim + cx;
            uint8_t v     = vis[static_cast<size_t>(idx)];
            float   px    = origin.x + cx * tw;
            float   py    = origin.y + cy * th;

            if (useTiles) {
                int frame = cartoFrame(screen_, v, outdoor);
                if (frame >= 0 && frame < frameCount)
                    dl->AddImage(static_cast<ImTextureID>(tex[frame]),
                                 ImVec2(px, py), ImVec2(px + tw, py + th));
            } else {
                // Fallback: page-0 wall edges (2-bit fields per direction).
                dl->AddRectFilled(ImVec2(px, py), ImVec2(px + tw, py + th),
                                  IM_COL32(30, 30, 45, 255));
                ImU32 wc = IM_COL32(210, 215, 220, 255);
                float t  = 1.5f;
                if (v & 0xC0) dl->AddLine(ImVec2(px,      py),      ImVec2(px + tw, py),      wc, t);
                if (v & 0x30) dl->AddLine(ImVec2(px + tw, py),      ImVec2(px + tw, py + th), wc, t);
                if (v & 0x0C) dl->AddLine(ImVec2(px,      py + th), ImVec2(px + tw, py + th), wc, t);
                if (v & 0x03) dl->AddLine(ImVec2(px,      py),      ImVec2(px,      py + th), wc, t);
            }

            // Event dot (red) — collision page 0x80 flag only.
            if (collisionHasEvent(col[static_cast<size_t>(idx)]))
                dl->AddCircleFilled(ImVec2(px + tw * 0.5f, py + th * 0.5f), 2.0f * zoom_,
                                    IM_COL32(255, 65, 65, 200));
        }
    }

    // Player cell — highlight + direction arrow.
    // Cartography.h documents direction-arrow frames 0x20..0x23 (N/S/E/W) in townb.32.
    int   screenCY = dim - 1 - camera_.y;
    float pcx      = origin.x + camera_.x * tw;
    float pcy      = origin.y + screenCY   * th;

    // Frame order from Cartography.h comment: N=0x20, S=0x21, E=0x22, W=0x23
    static const int kDirFrame[] = {0x20, 0x22, 0x21, 0x23};  // indexed by facing 0..3
    int  dirFrame      = kDirFrame[camera_.facing & 3];
    bool useArrowTile  = useTiles && (dirFrame < frameCount);

    if (useArrowTile) {
        // Yellow selection highlight behind the arrow tile
        dl->AddRectFilled(ImVec2(pcx, pcy), ImVec2(pcx + tw, pcy + th),
                          IM_COL32(255, 220, 0, 55));
        dl->AddRect(ImVec2(pcx, pcy), ImVec2(pcx + tw, pcy + th),
                    IM_COL32(255, 220, 0, 200), 0.0f, 0, 1.5f);
        dl->AddImage(static_cast<ImTextureID>(townb_.tex[dirFrame]),
                     ImVec2(pcx, pcy), ImVec2(pcx + tw, pcy + th));
    } else {
        // Primitive fallback: yellow circle + white facing triangle
        float cx2 = pcx + tw * 0.5f, cy2 = pcy + th * 0.5f;
        static const float kArrDx[] = { 0.0f, 1.0f,  0.0f, -1.0f};
        static const float kArrDy[] = {-1.0f, 0.0f,  1.0f,  0.0f};
        int   f    = camera_.facing & 3;
        float arrL = th * 0.5f, arrW = tw * 0.28f;
        float tipX = cx2 + kArrDx[f] * arrL, tipY = cy2 + kArrDy[f] * arrL;
        float perpX = -kArrDy[f], perpY = kArrDx[f];
        dl->AddCircleFilled(ImVec2(cx2, cy2), tw * 0.35f, IM_COL32(255, 220, 0, 255));
        dl->AddTriangleFilled(ImVec2(tipX, tipY),
                              ImVec2(cx2 + perpX * arrW, cy2 + perpY * arrW),
                              ImVec2(cx2 - perpX * arrW, cy2 - perpY * arrW),
                              IM_COL32(255, 255, 255, 230));
    }

    // Compass "N" above the map centre
    dl->AddText(ImVec2(origin.x + mapW * 0.5f - 3.0f, origin.y - 13.0f),
                IM_COL32(160, 160, 180, 200), "N");

    dl->AddRect(origin, ImVec2(origin.x + mapW, origin.y + mapH),
                IM_COL32(130, 130, 145, 255));

    ImGui::Dummy(ImVec2(mapW, mapH));
}

void MapSection::drawVisualDecode(uint8_t cell) {
    static const char* kWall[] = {"open", "wall", "door", "wall"};
    auto name = [&](int v) -> const char* {
        return (v >= 0 && v <= 3) ? kWall[v] : "?";
    };
    ImGui::Text("N:%s  E:%s  S:%s  West:%s", name(visualWallN(cell)), name(visualWallE(cell)),
                name(visualWallS(cell)), name(visualWallW(cell)));
    ImGui::TextDisabled("visual page: 2 bits per side, 0-3 (no event bit)");
}

void MapSection::drawCollisionDecode(uint8_t cell) {
    // Each direction field = (dark<<1)|wall. Low bit = wall present, high bit =
    // darkness (drains one party light factor on entry; see Cleric Light/Lasting
    // Light). North/East/South keep their darkness bit (0x02/0x08/0x20); West's
    // high bit (0x80) is reused as the event marker instead.
    auto fieldDesc = [](int f, bool westNoDark) -> const char* {
        bool wall = f & 1;
        bool dark = (f & 2) && !westNoDark;
        if (wall && dark) return "wall+dark";
        if (wall) return "wall";
        if (dark) return "dark";
        return "open";
    };
    int n = cell & 3, e = (cell >> 2) & 3, s = (cell >> 4) & 3, w = (cell >> 6) & 3;
    ImGui::Text("N:%s  E:%s  S:%s  W:%s", fieldDesc(n, false), fieldDesc(e, false),
                fieldDesc(s, false), fieldDesc(w, true));
    ImGui::TextDisabled("dark squares cost 1 light factor on entry");
    if (collisionHasEvent(cell))
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "Event flag (0x80) SET - this tile has an event.dat entry");
    else
        ImGui::TextDisabled("No event flag (0x80 clear)");
}

void MapSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ImGui::TextDisabled("map.dat not loaded.");
        return;
    }

    ui::SetFieldWide();
    std::string cur = std::to_string(screen_) + ": " + areaLabel(screen_);
    if (ImGui::BeginCombo("Screen", cur.c_str())) {
        for (int i = 0; i < kMapScreens; ++i) {
            std::string lbl = std::to_string(i) + ": " + areaLabel(i);
            if (ImGui::Selectable(lbl.c_str(), screen_ == i)) screen_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (screen_ > 0) {
        if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) screen_--;
        ImGui::SameLine();
    }
    if (screen_ < kMapScreens - 1) {
        if (ImGui::ArrowButton("##next", ImGuiDir_Right)) screen_++;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", isOutdoor(screen_) ? "outdoor" : "indoor");

    MapScreen& s = file_.screens[screen_];

    if (ImGui::BeginTabBar("map_tabs")) {
        if (ImGui::BeginTabItem("Tiles")) {
            ImGui::Checkbox("Tile graphics", &graphical_);
            if (graphical_) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(160);
                ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 8.0f, "%.0fx");
            }
            if (ImGui::BeginTable("map_pages", 2, ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SeparatorText("Visual tiles (page 0)");
                if (graphical_)
                    drawCartoGrid("visual", s.visual, selVisual_);
                else
                    drawGrid("visual", s.visual, selVisual_);
                ImGui::TableNextColumn();
                ImGui::SeparatorText("Collision + events (page 1)");
                // Collision always renders with the indoor townb.32 cartography
                // tiles (even on outdoor screens); falls back to hex if missing.
                // Event-flagged cells (0x80) get a red marker.
                if (graphical_ && !townb_.tex.empty())
                    drawCartoGrid("collision", s.collision, selCollision_, /*forceTownb=*/true,
                                  /*markEvents=*/true);
                else
                    drawGrid("collision", s.collision, selCollision_, /*markEvents=*/true);
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Window (3D backdrop)")) {
            drawWindow();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("3D View")) {
            drawView3D();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

}  // namespace mm2
