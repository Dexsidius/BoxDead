// BoxDead - Tilemap: loads scene/level tilesets exported by the LevelEdit++
// editor (".mx" files). An .mx file is JSON produced by the editor's
// `ToJson::ExportMX`:
//
//   {
//     "formatVersion": 2,
//     "name": "Courtyard",
//     "tiles": {
//       "Ground": {
//         "filepath": "assets/Ground.bmp",
//         "locations": [[x, y, w, h, elevation], ...]
//       },
//       "Wall": { "filepath": "assets/Wall.bmp", "locations": [...] }
//     }
//   }
//
// Each tile entry points at a .bmp image (relative to the .mx file) and lists
// every placement as [x, y, w, h] in world pixels, optionally followed by the
// height it stands off the floor.
//
// That fifth element arrived with format version 2. Version 1 files have four
// and load flat, and a reader that only knows the four-element form ignores
// anything past it, so the two versions interoperate in both directions. A
// raised tile draws its top face lifted by the elevation with a shaded side
// face filling the gap down to the footprint - the footprint, and so the
// collision, is exactly where it would be if the tile were flat.
//
// BoxDead loads the .bmp for
// each tile once and renders every placement. Tiles whose name contains a
// solid keyword ("wall", "block", "rock", "stone", "barrier", "fence",
// "crate", "pillar", "obstacle") are treated as collidable.
//
// A tile entry may carry an optional "collision": [x, y, w, h] footprint in
// tile-local pixels, which is what blocks movement instead of the whole tile.
// LevelEdit++ ignores keys it does not know, so a map carrying one still opens
// in the editor.
//
// Tiles whose name contains an explosive keyword ("barrel", "drum",
// "explosive", "tnt") are not drawn or collided as tiles at all: their
// placements are handed to the Game, which spawns a destructible Barrel
// entity at each one so they can be shot, explode, and chain-react.
#pragma once

#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

namespace bd {

class Tilemap {
public:
    Tilemap() = default;
    ~Tilemap();

    Tilemap(const Tilemap&) = delete;
    Tilemap& operator=(const Tilemap&) = delete;

    // Load + parse a ".mx" tileset. `mx_path` is the path to the .mx file;
    // tile .bmp paths inside are resolved relative to the .mx file's
    // directory (exactly how the editor writes them). Returns false (and
    // leaves the tilemap empty) on any parse/load error so the caller can
    // fall back to a solid-color floor.
    bool load(SDL_Renderer* r, const std::string& mx_path);

    // Release all loaded tile textures + placements.
    void clear();

    // Draw every visible tile placement (floor + walls). Call after
    // SDL_RenderClear. `cam_x/cam_y` subtract the camera position so a world
    // larger than the viewport scrolls: screen = world - camera. Tiles outside
    // the `view_w` x `view_h` viewport are skipped, which is what keeps very
    // large maps (thousands of tiles) from spending the frame on draw calls
    // for tiles nobody can see.
    //
    // This draws the floor and then every raised tile, so entities drawn
    // afterwards all end up in front of the walls. Use render_floor() plus
    // render_raised() instead to interleave entities with raised tiles by
    // depth, which is what the game does.
    void render(SDL_Renderer* r, float cam_x, float cam_y, float view_w,
                float view_h) const;

    // Just the flat tiles. These sit on the ground, so nothing ever goes
    // behind them and they can all be drawn in one pass up front.
    void render_floor(SDL_Renderer* r, float cam_x, float cam_y, float view_w,
                      float view_h) const;

    // Raised tiles, ordered back to front by the line where they meet the
    // ground. The caller walks these alongside its own entities, drawing
    // whichever comes first, so a character behind a wall is covered by it and
    // one in front of the wall is drawn over it.
    int raised_count() const { return static_cast<int>(raised_.size()); }
    float raised_ground_line(int index) const;
    void render_raised(SDL_Renderer* r, int index, float cam_x, float cam_y,
                       float view_w, float view_h) const;

    // Where the level wants an explosive barrel. These come from tiles named
    // with an explosive keyword and are NOT part of the drawn/collided tile
    // list — the Game turns each one into a Barrel entity on load.
    struct Spawn {
        float cx, cy;  // centre of the placement, in world pixels
        float w, h;
    };
    const std::vector<Spawn>& explosive_spawns() const { return explosives_; }

    // Bounding box of every placed tile (max x+w, max y+h). The game uses this
    // as the world size for a level (maps can be bigger than the viewport).
    void world_bounds(float& out_w, float& out_h) const;

    // True if a world-space point lies inside any solid tile. Used by the
    // player/enemy movement to block walking through walls.
    bool is_solid(float px, float py) const;

    bool loaded() const { return loaded_; }
    int tile_count() const { return static_cast<int>(tiles_.size()); }

private:
    struct Placement {
        SDL_Texture* tex;  // non-owning (owned by textures_)
        int x, y, w, h;
        // How far this tile stands off the floor, in pixels (the editor calls
        // it "Stands Up"). 0 is flat ground. A raised tile draws its top face
        // lifted by this much with a shaded side face filling the gap down to
        // the footprint, which is what gives the top-down view its 2.5D look.
        // The footprint - and so the collision - stays exactly where it is.
        int elevation;
        bool solid;
        // What actually blocks movement, in world pixels. A prop is drawn on a
        // full tile but only occupies part of it, so blocking the whole tile
        // put an invisible wall around every rock and tree. Defaults to the
        // draw rect when the tile declares no "collision" footprint.
        int sx, sy, sw, sh;
    };

    // Draw one placement with its lift and side face. Shared by every render
    // path so the floor pass, the interleaved pass and the all-in-one render()
    // cannot drift apart.
    void draw_placement(SDL_Renderer* r, const Placement& t, float cam_x,
                        float cam_y, float view_w, float view_h) const;

    // Indices into tiles_ of everything with elevation > 0, sorted back to
    // front by ground line. Built once at load rather than per frame.
    void build_raised_index();
    std::vector<int> raised_;

    // Uniform grid over the map holding, per cell, the indices of the solid
    // placements overlapping it. is_solid() then tests a handful of rects
    // instead of every tile in the level, which matters once a map runs to
    // thousands of tiles and every entity queries it twice a frame.
    void build_solid_index();
    static constexpr float kGridCell = 64.0f;
    std::vector<std::vector<int>> solid_cells_;
    int grid_cols_ = 0;
    int grid_rows_ = 0;

    std::vector<std::unique_ptr<Texture>> textures_;  // owns the .bmp textures
    std::vector<Placement> tiles_;
    std::vector<Spawn> explosives_;
    bool loaded_ = false;
};

}  // namespace bd
