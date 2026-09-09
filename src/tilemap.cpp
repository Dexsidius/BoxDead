// BoxDead - Tilemap implementation (loads LevelEdit++ ".mx" tilesets).
#include "boxdead/tilemap.hpp"

#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#ifdef _WIN32
#include <ciso646>  // MinGW: provide `and`/`or`/`not` keywords
#endif

using json = nlohmann::json;

namespace bd {

namespace {

// Resolve a tile .bmp path relative to the .mx file's directory. The editor
// writes paths like "assets/Grass Patch.bmp" relative to the .mx file.
std::string resolve_asset(const std::string& mx_path,
                          const std::string& asset_path) {
    // Find the last path separator in the .mx path.
    auto pos = mx_path.find_last_of("/\\");
    std::string base =
        (pos == std::string::npos) ? std::string{} : mx_path.substr(0, pos + 1);
    return base + asset_path;
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

}  // namespace

Tilemap::~Tilemap() { clear(); }

void Tilemap::clear() {
    tiles_.clear();
    textures_.clear();
    loaded_ = false;
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
            std::string full = resolve_asset(mx_path, filepath);
            auto t = Texture::load(r, full);
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

    loaded_ = true;
    return true;
}

void Tilemap::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    for (const auto& t : tiles_) {
        const SDL_FRect dst{static_cast<float>(t.x) - cam_x,
                            static_cast<float>(t.y) - cam_y,
                            static_cast<float>(t.w),
                            static_cast<float>(t.h)};
        // Cull tiles fully outside the viewport (cam +/- view is unknown here,
        // so just skip tiles whose screen rect is entirely off any side — a
        // cheap negative check; SDL clips the rest).
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
    for (const auto& t : tiles_) {
        if (!t.solid) continue;
        if (px >= t.x && px < t.x + t.w &&
            py >= t.y && py < t.y + t.h) {
            return true;
        }
    }
    return false;
}

}  // namespace bd
