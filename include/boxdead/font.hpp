// BoxDead - Font: RAII wrapper around TTF_Font with cached text textures.
#pragma once

#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace bd {

// Owns a TTF_Font and caches rendered text strings as SDL textures, so
// repeated text (score, labels, menu items) is not re-rendered every frame.
class Font {
public:
    Font() = default;
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // Load a TrueType font at the given pixel size.
    bool load(SDL_Renderer* r, const std::string& path, int size);

    // Width in pixels of `text` at the current font size (0 if not loaded).
    int text_width(const std::string& text);

    // Render `text` (top-left at x,y) in the given color.
    void draw(const std::string& text, float x, float y,
              SDL_Color color = SDL_Color{255, 255, 255, 255});

    // Drop the cached text textures and close the font. Must be called while
    // the renderer and SDL_ttf are still alive — the destructor alone is too
    // late, because a Font member outlives the Game::shutdown() that tears
    // those down. Safe to call more than once.
    void release();

private:
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Texture>> cache_;
};

}  // namespace bd
