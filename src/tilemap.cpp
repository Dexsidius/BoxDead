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
        const int c0 = std::max(0, static_cast<int>(t.x / kGridCell));
        const int c1 = std::min(grid_cols_ - 1,
                                static_cast<int>((t.x + t.w - 1) / kGridCell));
        const int r0 = std::max(0, static_cast<int>(t.y / kGridCell));
        const int r1 = std::min(grid_rows_ - 1,
                                static_cast<int>((t.y + t.h - 1) / kGridCell));
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

        // Each location is [x, y, w, h]; default to 32x32 if missing/short.
        const auto& locations = entry.value("locations", json::array());
        if (!locations.is_array()) continue;
        for (const auto& loc : locations) {
            if (!loc.is_array() || loc.size() < 2) continue;
            int x = loc[0].get<int>();
            int y = loc[1].get<int>();
            int w = (loc.size() > 2) ? loc[2].get<int>() : 32;
            int h = (loc.size() > 3) ? loc[3].get<int>() : 32;
            tiles_.push_back(Placement{tex->get(), x, y, w, h, solid});
        }
    }

    build_solid_index();
    loaded_ = true;
    return true;
}

void Tilemap::render(SDL_Renderer* r, float cam_x, float cam_y, float view_w,
                    float view_h) const {
    for (const auto& t : tiles_) {
        const SDL_FRect dst{static_cast<float>(t.x) - cam_x,
                            static_cast<float>(t.y) - cam_y,
                            static_cast<float>(t.w),
                            static_cast<float>(t.h)};
        // Skip anything entirely off-screen rather than handing SDL a draw
        // call to clip away.
        if (dst.x + dst.w <= 0.0f || dst.y + dst.h <= 0.0f ||
            dst.x >= view_w || dst.y >= view_h) {
            continue;
        }
        SDL_RenderTexture(r, t.tex, nullptr, &dst);
    }
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
        if (px >= t.x && px < t.x + t.w && py >= t.y && py < t.y + t.h) {
            return true;
        }
    }
    return false;
}

}  // namespace bd
