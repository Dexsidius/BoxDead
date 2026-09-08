// BoxDead - Sprite: a renderable quad (texture region + color fallback)
#pragma once

#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>

#include <memory>

namespace bd {

// A renderable sprite: an optional texture region plus a fallback color.
struct Sprite {
    Texture* texture = nullptr;            // optional; if null, draws color
    SDL_FRect src{};                        // texture region (0 = whole tex)
    SDL_Color color{255, 255, 255, 255};    // tint / fallback color
};

// Draw a sprite centered at (cx, cy) with the given world size.
void draw_sprite(SDL_Renderer* r, const Sprite& s, float cx, float cy, float w,
                 float h);

// Procedurally generate a solid-color sprite texture with a darker border,
// so the placeholder art looks like a framed sprite rather than a flat box.
std::unique_ptr<Texture> make_solid_sprite_texture(SDL_Renderer* r,
                                                   SDL_Color base,
                                                   int size = 32);

}  // namespace bd
