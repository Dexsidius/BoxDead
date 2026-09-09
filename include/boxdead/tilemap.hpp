// BoxDead - Tilemap: loads scene/level tilesets exported by the LevelEdit++
// editor (".mx" files). An .mx file is JSON produced by the editor's
// `ToJson::ExportMX`:
//
//   {
//     "name": "Courtyard",
//     "tiles": {
//       "Ground": {
//         "filepath": "assets/Ground.bmp",
//         "locations": [[x, y, w, h], ...]
//       },
//       "Wall": { "filepath": "assets/Wall.bmp", "locations": [...] }
//     }
//   }
//
// Each tile entry points at a .bmp image (relative to the .mx file) and lists
// every placement as [x, y, w, h] in world pixels. BoxDead loads the .bmp for
// each tile once and renders every placement. Tiles whose name contains a
// solid keyword ("wall", "block", "rock", "stone", "barrier", "fence",
// "crate", "pillar", "obstacle") are treated as collidable.
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
    void render(SDL_Renderer* r, float cam_x, float cam_y, float view_w,
                float view_h) const;

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
        bool solid;
    };

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
