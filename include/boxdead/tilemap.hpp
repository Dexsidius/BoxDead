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

    // Draw every tile placement (floor + walls). Call after SDL_RenderClear.
    void render(SDL_Renderer* r) const;

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

    std::vector<std::unique_ptr<Texture>> textures_;  // owns the .bmp textures
    std::vector<Placement> tiles_;
    bool loaded_ = false;
};

}  // namespace bd
