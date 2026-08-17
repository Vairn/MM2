#pragma once
#include <vector>

#include "app/Section.h"
#include "core/AttribFile.h"
#include "core/Gfx.h"
#include "core/MapFile.h"
#include "core/Outdoor3D.h"
#include "core/View3D.h"

namespace mm2 {

class MapSection : public Section {
public:
    DocKind docKind() const override { return DocKind::Map; }
    const char* title() const override { return "Maps"; }
    const char* fileName() const override { return "map.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;
    void focusIndex(int index) override;
    /** Open screen and select tile at game (y,x). */
    void focusTile(int screen, int tileY, int tileX);
    void flushPending() override;
    ~MapSection() override;

    int currentScreen() const { return screen_; }

    // The four backdrop environments the engine selects between.
    enum class Env { Town, Cavern, Castle, Outdoor };

    // Manual override for 3D indoor wall/floor/torch sheets (Auto follows attrib).
    enum class WallsetOverride { Auto = 0, Town, Cavern, Castle };

    void drawGrid(const char* id, std::array<uint8_t, kMapPageSize>& page, int& selTile,
                  bool markEvents = false);
    void drawCartoGrid(const char* id, std::array<uint8_t, kMapPageSize>& page, int& selTile,
                       bool forceTownb = false, bool markEvents = false);
    void drawVisualDecode(uint8_t cell);
    void drawCollisionDecode(uint8_t cell);
    void drawWindow();
    void drawView3D();
    void drawOutdoorView3D();
    void drawMinimap();  // top-down minimap overlay used by drawView3D
    void handleView3DKeyboardInput();
    void stepCameraInDirection(int dir);
    void loadTilesets(const std::string& dataDir);
    void ensureOutdoorGfxLoaded();
    void releaseOutdoorGfx(std::vector<unsigned int>* deferGl = nullptr);
    void releaseTextures(bool deferGl = false);
    void flushPendingTextures();
    // True when the screen renders with the outdoor tileset (outb.32).
    bool isOutdoor(int screen) const;
    bool usesOutbCarto(int screen) const;
    Env envOf(int screen) const;

    // One uploaded GL texture per frame; decoded RGBA is not retained after upload.
    struct Sheet {
        bool ok = false;
        std::vector<int> frameW;
        std::vector<int> frameH;
        std::vector<unsigned int> tex;

        void release(std::vector<unsigned int>* deferGl = nullptr);
        void loadFile(const std::string& path, const uint8_t (*palette_override)[4] = nullptr);
        uint8_t palette[32][4] = {};
    };

    MapFile file_;
    AttribFile attrib_;
    bool attribLoaded_ = false;

    Sheet outb_;   // outdoor auto-map tiles
    Sheet townb_;  // indoor auto-map (townb.32 through town.32 palette)
    Sheet townbCave_;    // townb.32 through cave.32 palette
    Sheet townbCastle_;  // townb.32 through castle.32 palette
    Sheet sky_;    // shared ceiling/sky backdrop (sky.32, 2 frames)
    Sheet floorTown_, floorCave_, floorCastle_, floorOut_;   // per-env floor
    Sheet wallTown_, wallCave_, wallCastle_;                  // per-env walls
    Sheet torchTown_, torchCave_, torchCastle_;               // per-env torches
    Sheet horizon1_, horizon2_, horizon3_;                // outdoor1/2/3.32
    Sheet biomeDesert_, biomeOcean_, biomeTundra_, biomeSwamp_;

    const Sheet* biomeSheet(OutdoorBiome biome) const;
    const Sheet* horizonSheet(OutdoorHorizonSheet sheet) const;
    const Sheet& indoorCartoSheet() const;

    std::string dataDir_;
    bool outdoorGfxLoaded_ = false;
    std::vector<unsigned int> pendingFreeTextures_;

    int screen_ = 0;
    View3DCamera camera_{8, 8, 0};
    int selVisual_ = 0;
    int selCollision_ = 0;
    bool graphical_ = true;
    float zoom_ = 3.0f;
    float viewZoom_ = 2.0f;
    int skyFrame_ = 0;
    int wallsetOverride_ = 0;  // WallsetOverride as int for ImGui::Combo
};

}  // namespace mm2
