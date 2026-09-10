// BoxDead - Tilemap implementation (loads LevelEdit++ ".mx" tilesets).
#include "boxdead/tilemap.hpp"

#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>


using json = nlohmann::json;

namespace bd {

namespace {

// Candidate on-disk paths for a tile image, in priority order.
//
// LevelEdit++ writes `filepath` relative to *its own* working directory
// ("exports/<Level>/assets/Tile.bmp"), while BoxDead loads a map from wherever
// the .mx happens to live. Trying the basename under the .mx's own assets/
// directory as a fallback means one .mx file works unmodified in both: the
// editor resolves the literal path, the game finds the same file next to the
// map it just opened.
std::vector<std::string> resolve_candidates(const std::string& mx_path,
                                            const std::string& asset_path) {
    auto pos = mx_path.find_last_of("/\\");
    const std::string base =
        (pos == std::string::npos) ? std::string{} : mx_path.substr(0, pos + 1);
    auto slash = asset_path.find_last_of("/\\");
    const std::string leaf = (slash == std::string::npos)
                                 ? asset_path
                                 : asset_path.substr(slash + 1);
    return {base + asset_path, base + "assets/" + leaf, asset_path};
}

// Case-insensitive "name contains keyword" test for solid tiles.
bool name_is_solid(const std::string& name) {
    static const char* kSolidKeywords[] = {
        "wall", "block", "rock", "stone", "barrier",
        "fence", "crate", "pillar", "obstacle"};
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(c)));
    for (const char* kw : kSolidKeywords) {
        std::string k(kw);
        if (lower.find(k) != std::string::npos) return true;
    }
    return false;
}

// Case-insensitive test for tiles the level author meant as explosive props.
// Checked before the solid test so "Explosive Barrel" becomes a barrel rather
// than a wall.
bool name_is_explosive(const std::string& name) {
    static const char* kExplosiveKeywords[] = {"barrel", "drum", "explosive",
                                               "tnt"};
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(c)));
    for (const char* kw : kExplosiveKeywords) {
        if (lower.find(kw) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

Tilemap::~Tilemap() { clear(); }

void Tilemap::clear() {
    tiles_.clear();
    explosives_.clear();
    textures_.clear();
    solid_cells_.clear();
    raised_.clear();
    grid_cols_ = 0;
    grid_rows_ = 0;
    loaded_ = false;
}

void Tilemap::build_solid_index() {
    solid_cells_.clear();
    float w = 0.0f;
    float h = 0.0f;
    world_bounds(w, h);
    grid_cols_ = std::max(1, static_cast<int>(w / kGridCell) + 1);
    grid_rows_ = std::max(1, static_cast<int>(h / kGridCell) + 1);
    solid_cells_.assign(static_cast<size_t>(grid_cols_) * grid_rows_, {});
    for (size_t i = 0; i < tiles_.size(); ++i) {
        const Placement& t = tiles_[i];
        if (!t.solid) continue;
        const int c0 = std::max(0, static_cast<int>(t.sx / kGridCell));
        const int c1 = std::min(grid_cols_ - 1,
                                static_cast<int>((t.sx + t.sw - 1) / kGridCell));
        const int r0 = std::max(0, static_cast<int>(t.sy / kGridCell));
        const int r1 = std::min(grid_rows_ - 1,
                                static_cast<int>((t.sy + t.sh - 1) / kGridCell));
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                solid_cells_[static_cast<size_t>(r) * grid_cols_ + c]
                    .push_back(static_cast<int>(i));
            }
        }
    }
}

bool Tilemap::load(SDL_Renderer* r, const std::string& mx_path) {
    clear();

    json root;
    {
        std::ifstream f(mx_path);
        if (!f.is_open()) return false;
        try {
            f >> root;
        } catch (const std::exception&) {
            return false;  // malformed JSON
        }
    }
    if (!root.is_object()) return false;

    const auto& tiles = root.value("tiles", json::object());
    if (!tiles.is_object()) return false;

    // Cache loaded textures by asset path so repeated tiles share one texture.
    std::vector<std::pair<std::string, Texture*>> cache;

    for (auto it = tiles.begin(); it != tiles.end(); ++it) {
        const std::string tile_name = it.key();
        const json& entry = it.value();
        if (!entry.is_object()) continue;

        // Explosive tiles never become tiles: collect their placements for
        // the Game to spawn Barrel entities from, and skip loading their .bmp
        // (barrels are drawn as 3D props, not flat tile images).
        if (name_is_explosive(tile_name)) {
            const auto& locs = entry.value("locations", json::array());
            if (!locs.is_array()) continue;
            for (const auto& loc : locs) {
                if (!loc.is_array() || loc.size() < 2) continue;
                const float x = static_cast<float>(loc[0].get<int>());
                const float y = static_cast<float>(loc[1].get<int>());
                const float w =
                    (loc.size() > 2) ? static_cast<float>(loc[2].get<int>()) : 32.0f;
                const float h =
                    (loc.size() > 3) ? static_cast<float>(loc[3].get<int>()) : 32.0f;
                explosives_.push_back(
                    Spawn{x + w * 0.5f, y + h * 0.5f, w, h});
            }
            continue;
        }

        std::string filepath =
            entry.value("filepath", std::string{});
        if (filepath.empty()) continue;

        // Find or load this tile's texture (resolved relative to the .mx dir).
        Texture* tex = nullptr;
        for (const auto& c : cache) {
            if (c.first == filepath) {
                tex = c.second;
                break;
            }
        }
        if (!tex) {
            std::unique_ptr<Texture> t;
            for (const std::string& cand :
                 resolve_candidates(mx_path, filepath)) {
                t = Texture::load(r, cand);
                if (t) break;
            }
            if (!t) continue;  // skip tiles whose .bmp can't be loaded
            tex = t.get();
            cache.emplace_back(filepath, tex);
            textures_.push_back(std::move(t));
        }

        const bool solid = name_is_solid(tile_name);

        // Optional tight collision footprint, in tile-local pixels. Without it
        // a prop blocks its whole tile, which reads in-game as an invisible
        // wall around the visible art.
        int cx = 0;
        int cy = 0;
        int cw = -1;
        int ch = -1;
        const auto& col = entry.value("collision", json::array());
        if (col.is_array() && col.size() >= 4) {
            cx = col[0].get<int>();
            cy = col[1].get<int>();
            cw = col[2].get<int>();
            ch = col[3].get<int>();
        }

        // Each location is [x, y, w, h, elevation]; default to 32x32 if
        // missing/short. The fifth element arrived with .mx format version 2;
        // a map written before that has four and loads flat.
        const auto& locations = entry.value("locations", json::array());
        if (!locations.is_array()) continue;
        for (const auto& loc : locations) {
            if (!loc.is_array() || loc.size() < 2) continue;
            int x = loc[0].get<int>();
            int y = loc[1].get<int>();
            int w = (loc.size() > 2) ? loc[2].get<int>() : 32;
            int h = (loc.size() > 3) ? loc[3].get<int>() : 32;
            int elevation = (loc.size() > 4) ? loc[4].get<int>() : 0;
            if (elevation < 0) elevation = 0;
            const bool has_box = (cw > 0 && ch > 0);
            tiles_.push_back(Placement{tex->get(), x, y, w, h, elevation, solid,
                                       has_box ? x + cx : x,
                                       has_box ? y + cy : y,
                                       has_box ? cw : w,
                                       has_box ? ch : h});
        }
    }

    build_solid_index();
    build_raised_index();
    loaded_ = true;
    return true;
}

void Tilemap::build_raised_index() {
    raised_.clear();
    for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
        if (tiles_[static_cast<size_t>(i)].elevation > 0) raised_.push_back(i);
    }
    // Back to front by where each tile meets the ground, so a wall covers
    // whatever stands behind it. Sorted once here instead of every frame.
    // Stable, so tiles sharing a ground line keep their file order rather than
    // trading places.
    std::stable_sort(raised_.begin(), raised_.end(), [this](int a, int b) {
        const Placement& pa = tiles_[static_cast<size_t>(a)];
        const Placement& pb = tiles_[static_cast<size_t>(b)];
        return (pa.y + pa.h) < (pb.y + pb.h);
    });
}

float Tilemap::raised_ground_line(int index) const {
    if (index < 0 || index >= static_cast<int>(raised_.size())) return 0.0f;
    const Placement& t = tiles_[static_cast<size_t>(raised_[static_cast<size_t>(index)])];
    return static_cast<float>(t.y + t.h);
}

void Tilemap::draw_placement(SDL_Renderer* r, const Placement& t, float cam_x,
                             float cam_y, float view_w, float view_h) const {
    // The footprint is where the tile sits on the floor; the top face is that
    // same rect lifted by the elevation. This matches what the editor draws, so
    // a level looks the same in the game as it did while it was being built.
    const SDL_FRect dst{static_cast<float>(t.x) - cam_x,
                        static_cast<float>(t.y) - cam_y -
                            static_cast<float>(t.elevation),
                        static_cast<float>(t.w),
                        static_cast<float>(t.h)};

    // Skip anything entirely off-screen rather than handing SDL a draw
    // call to clip away. The bottom edge test uses the foot of the side face,
    // not the top face, or a tall tile would vanish while its side was still
    // on screen.
    if (dst.x + dst.w <= 0.0f || dst.x >= view_w || dst.y >= view_h ||
        dst.y + dst.h + static_cast<float>(t.elevation) <= 0.0f) {
        return;
    }

    // Side face: fills the gap between the lifted top face and the ground,
    // drawn from the tile's own texture with a darker colour mod so existing
    // tile art gets a shaded side without anyone authoring new images.
    if (t.elevation > 0) {
        const SDL_FRect side{dst.x, dst.y + dst.h, dst.w,
                             static_cast<float>(t.elevation)};
        SDL_SetTextureColorMod(t.tex, 150, 150, 150);
        SDL_RenderTexture(r, t.tex, nullptr, &side);
        // Textures are shared between placements, so put the colour back.
        SDL_SetTextureColorMod(t.tex, 255, 255, 255);
    }

    SDL_RenderTexture(r, t.tex, nullptr, &dst);
}

void Tilemap::render(SDL_Renderer* r, float cam_x, float cam_y, float view_w,
                    float view_h) const {
    render_floor(r, cam_x, cam_y, view_w, view_h);
    for (int i = 0; i < raised_count(); ++i) {
        render_raised(r, i, cam_x, cam_y, view_w, view_h);
    }
}

void Tilemap::render_floor(SDL_Renderer* r, float cam_x, float cam_y,
                           float view_w, float view_h) const {
    for (const auto& t : tiles_) {
        if (t.elevation > 0) continue;  // drawn later, interleaved with entities
        draw_placement(r, t, cam_x, cam_y, view_w, view_h);
    }
}

void Tilemap::render_raised(SDL_Renderer* r, int index, float cam_x,
                            float cam_y, float view_w, float view_h) const {
    if (index < 0 || index >= static_cast<int>(raised_.size())) return;
    draw_placement(r, tiles_[static_cast<size_t>(raised_[static_cast<size_t>(index)])],
                   cam_x, cam_y, view_w, view_h);
}

void Tilemap::world_bounds(float& out_w, float& out_h) const {
    float maxw = 0.0f;
    float maxh = 0.0f;
    for (const auto& t : tiles_) {
        const float rx = static_cast<float>(t.x + t.w);
        const float ry = static_cast<float>(t.y + t.h);
        if (rx > maxw) maxw = rx;
        if (ry > maxh) maxh = ry;
    }
    out_w = maxw;
    out_h = maxh;
}

bool Tilemap::is_solid(float px, float py) const {
    if (solid_cells_.empty()) return false;
    if (px < 0.0f || py < 0.0f) return false;
    const int c = static_cast<int>(px / kGridCell);
    const int r = static_cast<int>(py / kGridCell);
    if (c < 0 || c >= grid_cols_ || r < 0 || r >= grid_rows_) return false;
    // Only the solid tiles sharing this grid cell can contain the point.
    for (int i : solid_cells_[static_cast<size_t>(r) * grid_cols_ + c]) {
        const Placement& t = tiles_[static_cast<size_t>(i)];
        if (px >= t.sx && px < t.sx + t.sw &&
            py >= t.sy && py < t.sy + t.sh) {
            return true;
        }
    }
    return false;
}

}  // namespace bd
