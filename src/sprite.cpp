// BoxDead - Sprite implementation
#include "boxdead/sprite.hpp"

#include <vector>

namespace bd {

void draw_sprite(SDL_Renderer* r, const Sprite& s, float cx, float cy, float w,
                 float h) {
    const SDL_FRect dst{cx - w * 0.5f, cy - h * 0.5f, w, h};
    if (s.texture && s.texture->get()) {
        SDL_FRect src = s.src;
        if (src.w == 0.0f || src.h == 0.0f) {
            src = SDL_FRect{0.0f, 0.0f, s.texture->w(), s.texture->h()};
        }
        SDL_RenderTexture(r, s.texture->get(), &src, &dst);
    } else {
        SDL_SetRenderDrawColor(r, s.color.r, s.color.g, s.color.b, s.color.a);
        SDL_RenderFillRect(r, &dst);
    }
}

std::unique_ptr<Texture> make_solid_sprite_texture(SDL_Renderer* r,
                                                   SDL_Color base,
                                                   int size) {
    std::vector<Uint32> pixels(static_cast<size_t>(size) * size);
    const SDL_Color dark{static_cast<Uint8>(base.r * 0.55f),
                         static_cast<Uint8>(base.g * 0.55f),
                         static_cast<Uint8>(base.b * 0.55f), base.a};
    const auto pack = [](SDL_Color c) {
        return static_cast<Uint32>(c.r) | (static_cast<Uint32>(c.g) << 8) |
               (static_cast<Uint32>(c.b) << 16) |
               (static_cast<Uint32>(c.a) << 24);
    };
    const int b = 4;  // border thickness
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool border = x < b || x >= size - b || y < b ||
                                y >= size - b;
            pixels[static_cast<size_t>(y) * size + x] =
                pack(border ? dark : base);
        }
    }
    return Texture::from_pixels(r, pixels.data(), size, size);
}

}  // namespace bd
