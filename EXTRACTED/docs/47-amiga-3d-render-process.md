# Amiga 3D Render Process (Might and Magic 2)

Based on the reverse-engineering documentation (`15-3d-view-and-game-screen.md`), the 3D first-person view on the Amiga version of Might and Magic 2 does not use modern polygon rendering or flat-color fills. Instead, it composites every frame entirely by blitting (pasting) pre-rendered 2D image sprites back-to-front (painter's algorithm).

Here is the detailed step-by-step breakdown of how a frame is constructed.

## 1. Environment Dispatch & Setup

When loading into an area, an environment dispatcher (`0x1786..0x187E`) loads specific graphics sheets into global memory pointers based on the area type (town, cavern, castle, or outdoors). Each sheet carries its own 32-entry Amiga color palette.

- **Walls:** e.g., `town.32`, `cave.32`, `castle.32` (contains multi-resolution sprites for depth scaling).
- **Floor:** e.g., `townf.32`, `cavef.32`.
- **Ceiling:** e.g., `townt.32`, `cavet.32`.
- **Outdoor Sheets:** `outdoor1.32`, `outdoor2.32`, etc.

Because there are no flat-color polygons, the "color" of an area (e.g., inside vs. outside) is simply determined by which set of image sheets is currently loaded.

## 2. The Indoor Render Pipeline (`0x2ECE`)

When rendering an indoor dungeon or town, the game builds a 208x120 pixel viewport (stacked as a 60px ceiling/sky and a 60px floor) through the following sequence:

1. **Input Latch Clear:** The engine clears the input latch so it doesn't process stale input during the draw cycle.
2. **Floor Backdrop:** The engine blits the appropriate floor sheet (`townf.32`, etc.) at the bottom of the viewport (`y=68, x=8`).
3. **Ceiling/Sky Backdrop:** The engine blits the ceiling or sky sheet at the top (`y=8, x=8`). It chooses frame 0 or 1 of the sky sheet based on a flag that indicates if the ceiling is closed or open.
4. **Frustum Clear:** The engine clears out an array of 20 "frustum" slots in memory, which represent the visible cells in front of the player.
5. **Cell Builder (`0x2900`):** The engine reads the raw `map.dat` (page 0 visual bytes) around the player's current position and facing. It extracts the 2-bit wall codes (open, wall, wall+torch, door) for the surrounding grid and populates the 20 frustum slots.
6. **Wall Painting (Back-to-Front):** 
   - The engine loops through the populated frustum slots from furthest depth to closest (painter's algorithm).
   - For each valid wall, it calls one of three specific wall-piece primitives: **front**, **left side**, or **right side**.
   - These primitives use hardcoded lookup tables based on depth (`0..3`) to determine the exact screen `x, y` coordinates and which size frame to pick from the multi-resolution wall sheet (e.g. `town.32`). Deeper cells automatically pick smaller frames.
   - For instance, the absolute nearest front wall (depth 0) is a 160x92 sprite placed exactly at `(32, 23)`.
7. **Flush & Advance:** The renderer flushes the draw queue and increments the animation frame counter.

## 3. The Outdoor Render Pipeline (`0x18870`)

The outdoor surface world is drawn via a completely separate rendering path. It doesn't use the indoor wall frustum, but rather layers horizon bands and terrain decor.

1. **Backdrops:** It paints the outdoor floor (`outf.32`) and the sky sheet, similar to the indoor pipeline.
2. **Floor Decor:** It reads the map to determine the biome (desert, swamp, tundra, ocean) and blits foreground details (grass, water edges, wedges) over the bottom of the floor using the `desert.32` (or equivalent) sheet. 
3. **Horizon Layers (3 Passes):**
   - The engine samples three rows of map data ahead of the player.
   - It converts the raw map bytes into terrain IDs, which act as indices to select between the three loaded outdoor sheets (`outdoor1.32`, `outdoor2.32`, `outdoor3.32`).
   - It blits these terrain sprites in three distinct overlapping passes (L1, L2, L3) using specialized coordinate lookup tables to layer distant mountains, forests, or plains over one another.

## 4. The Blitting Primitive

Every single 3D element is pasted using the exact same assembly function (`JSR -$7C20(A4)`), which takes 4 arguments:
```
blit(sheet_ptr, frame_index, y_coordinate, x_coordinate)
```
The screen coordinates are always absolute positions relative to the top-left of the 208x120 viewport area. 

## 5. Final Screen Compositing

Once the 3D viewport is complete, the engine layers on the rest of the UI:

- **Monsters:** If there is an encounter, monster sprites are placed over the 3D viewport based on a 24-entry placement table.
- **Chrome / UI:** The text, borders, character names, HP, and the command lists are **not** a single background image. They are dynamically drawn over a 40x24 text grid using a custom font engine that draws characters and box-drawing line glyphs (e.g., lines, corners) to build the UI frames on the fly.

## 6. The C++ Implementation in the Remake

In accordance with the 100% ASM fidelity rule, the remake's C++ rendering implementation in the `game/src/gfx/` directory mimics the logic, limits, and memory layout of the original Amiga executable. It builds pure data structures containing lists of blits, which are subsequently rendered by the graphics backend.

### Indoor Rendering (`game/src/gfx/View3D.cpp`)
The indoor master routine is implemented as `buildView3DScene()`. It follows the exact same flow as the original `0x2ECE` sequence:
1. **Hood Generation:** `refreshHood()` gathers a 13-byte neighborhood (the "hood") from the central and adjacent `MapGrid` pages (`kMapPageSize`), mimicking the original map sampling.
2. **Frustum Building:** `buildFrustum()` applies directional masks (N, E, S, W) mapping to the 2-bit field codes (Wall, Torch, Door) to construct the 20-slot frustum.
3. **Blit Collection:** `collectBlits()` walks through the frustum slots back-to-front (e.g. depth 4 to 0) and issues calls to `paintFrustumCell()` / `view3dPaintLatticeCell()`.
4. **Coordinate Tables:** The raw Amiga DATA segment depth tables (like `-$75AE`) are preserved as static compile-time constants (e.g., `T::kFrontX`, `T::kLeftFarY`) inside `View3DWallTables`.

### Outdoor Rendering (`game/src/gfx/OutdoorView3D.cpp`)
The outdoor master routine (`0x18870`) is handled by `buildOutdoorScene()`:
1. **Screen Resolution:** Unlike indoor dungeons, the outdoor `OutdoorMapGrid` must cross 16x16 screen boundaries. It uses `neighborScreen()` parsing `Mm2AttribFile` data to correctly wrap and fetch tiles from adjacent overland sectors.
2. **Terrain Lookup:** `processTerrainRows()` runs the raw map bytes through a 256-byte lookup array `kTerrainLookup` (replicating the original `0x9524` translation table) to resolve terrain IDs.
3. **Horizon Passes:** `buildHorizonBlits()` handles the three L1, L2, and L3 horizon terrain passes, pushing `OutdoorSpriteBlit` instructions pointing to `Outdoor1`, `Outdoor2`, or `Outdoor3` sheets.
4. **Floor Decor:** `buildDecorBlits()` layers the foreground wedges/grass according to the sector's biome (Ocean, Tundra, Swamp, Desert) using `biomeForCell()`, placing them at the bottom of the viewport using `T::kDecorY`.
