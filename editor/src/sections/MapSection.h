#pragma once
#include <vector>

#include "app/Section.h"
#include "core/AttribFile.h"
#include "core/Gfx.h"
#include "core/MapFile.h"
#include "core/View3D.h"

namespace mm2 {

class MapSection : public Section {
public:
    const char* title() const override { return "Maps"; }
    const char* fileName() const override { return "map.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;
    ~MapSection() override;

    // The four backdrop environments the engine selects between.
    enum class Env { Town, Cavern, Castle, Outdoor };

    void drawGrid(const char* id, std::array<uint8_t, kMapPageSize>& page, int& selTile,
                  bool markEvents = false);
    void drawCartoGrid(const char* id, std::array<uint8_t, kMapPageSize>& page, int& selTile,
                       bool forceTownb = false, bool markEvents = false);
    void drawVisualDecode(uint8_t cell);
    void drawCollisionDecode(uint8_t cell);
    void drawWindow();
    void drawView3D();
    void drawMinimap();  // top-down minimap overlay used by drawView3D
    void loadTilesets(const std::string& dataDir);
    void releaseTextures();
    // True when the screen renders with the outdoor tileset (outb.32).
    bool isOutdoor(int screen) const;
    Env envOf(int screen) const;

    // One decoded sheet plus its uploaded GL textures (one per frame).
    struct Sheet {
        GfxImage img;
        std::vector<unsigned int> tex;
        void release();
    };

    MapFile file_;
    AttribFile attrib_;
    bool attribLoaded_ = false;

    Sheet outb_;   // outdoor auto-map tiles
    Sheet townb_;  // indoor auto-map tiles
    Sheet sky_;    // shared ceiling/sky backdrop (sky.32, 2 frames)
    Sheet floorTown_, floorCave_, floorCastle_, floorOut_;   // per-env floor
    Sheet wallTown_, wallCave_, wallCastle_;                  // per-env walls
    Sheet torchTown_, torchCave_, torchCastle_;               // per-env torches

    int screen_ = 0;
    View3DCamera camera_{8, 8, 0};
    int selVisual_ = 0;
    int selCollision_ = 0;
    bool graphical_ = true;
    float zoom_ = 3.0f;
    float viewZoom_ = 2.0f;
    int skyFrame_ = 0;
};

}  // namespace mm2
