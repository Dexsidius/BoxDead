// BoxDead - Sprite: a renderable quad (texture region + color fallback)
#pragma once

#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>

#include <memory>

namespace bd {

// A renderable sprite: an optional texture region plus a color tint.
struct Sprite {
    Texture* texture = nullptr;            // optional; if null, draws color
    SDL_FRect src{};                        // texture region (0 = whole tex)
    SDL_Color color{255, 255, 255, 255};    // tint / fallback color
};

// Draw a sprite centered at (cx, cy) with the given world size. When a texture
// is set, the color tints it via SDL_SetTextureColorMod (white = no tint); this
// is what makes the player flash red during i-frames even with a real texture.
void draw_sprite(SDL_Renderer* r, const Sprite& s, float cx, float cy, float w,
                 float h);

// Procedurally generate a solid-color sprite texture with a darker border.
std::unique_ptr<Texture> make_solid_sprite_texture(SDL_Renderer* r,
                                                   SDL_Color base,
                                                   int size = 32);

// Procedurally generate a colored box with a plus/cross glyph (health pickups).
std::unique_ptr<Texture> make_cross_sprite_texture(SDL_Renderer* r,
                                                    SDL_Color base,
                                                    SDL_Color cross,
                                                    int size = 22);

// Procedurally generate a horizontal sprite sheet of `frame_count` walk/idle
// frames for a boxman character. When `moving`, the two legs alternate height
// per frame to simulate a walk cycle; when idle, the legs stand still and the
// body bobs. Pixels outside the body are transparent (blend mode is set by
// Texture::from_pixels). Each frame is `frame_size` pixels wide.
std::unique_ptr<Texture> make_walk_sheet_texture(SDL_Renderer* r,
                                                 SDL_Color base,
                                                 int frame_count,
                                                 int frame_size,
                                                 bool moving);

}  // namespace bd
