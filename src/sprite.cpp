// BoxDead - Sprite implementation
#include "boxdead/sprite.hpp"

#include <algorithm>
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
        // Tint the texture by the sprite color (white = no change).
        SDL_SetTextureColorMod(s.texture->get(), s.color.r, s.color.g,
                               s.color.b);
        SDL_RenderTexture(r, s.texture->get(), &src, &dst);
    } else {
        SDL_SetRenderDrawColor(r, s.color.r, s.color.g, s.color.b, s.color.a);
        SDL_RenderFillRect(r, &dst);
    }
}

namespace {
inline Uint32 pack_color(SDL_Color c) {
    return static_cast<Uint32>(c.r) | (static_cast<Uint32>(c.g) << 8) |
           (static_cast<Uint32>(c.b) << 16) |
           (static_cast<Uint32>(c.a) << 24);
}
}  // namespace

std::unique_ptr<Texture> make_solid_sprite_texture(SDL_Renderer* r,
                                                   SDL_Color base,
                                                   int size) {
    std::vector<Uint32> pixels(static_cast<size_t>(size) * size);
    const SDL_Color dark{static_cast<Uint8>(base.r * 0.55f),
                         static_cast<Uint8>(base.g * 0.55f),
                         static_cast<Uint8>(base.b * 0.55f), base.a};
    const int b = 4;  // border thickness
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool border = x < b || x >= size - b || y < b ||
                                y >= size - b;
            pixels[static_cast<size_t>(y) * size + x] =
                pack_color(border ? dark : base);
        }
    }
    return Texture::from_pixels(r, pixels.data(), size, size);
}

std::unique_ptr<Texture> make_cross_sprite_texture(SDL_Renderer* r,
                                                   SDL_Color base,
                                                   SDL_Color cross,
                                                   int size) {
    std::vector<Uint32> pixels(static_cast<size_t>(size) * size);
    const SDL_Color dark{static_cast<Uint8>(base.r * 0.55f),
                         static_cast<Uint8>(base.g * 0.55f),
                         static_cast<Uint8>(base.b * 0.55f), base.a};
    const int b = 3;                 // border thickness
    const int t = 3;                 // cross thickness
    const int c = size / 2;          // center
    const int lo = b + 1;            // inner padding
    const int hi = size - b - 1;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool border = x < b || x >= size - b || y < b ||
                                y >= size - b;
            const bool on_cross =
                ((y >= c - t / 2 && y < c + (t + 1) / 2) ||
                 (x >= c - t / 2 && x < c + (t + 1) / 2)) &&
                x >= lo && x < hi && y >= lo && y < hi;
            pixels[static_cast<size_t>(y) * size + x] =
                pack_color(border ? dark
                                  : (on_cross ? cross : base));
        }
    }
    return Texture::from_pixels(r, pixels.data(), size, size);
}

std::unique_ptr<Texture> make_walk_sheet_texture(SDL_Renderer* r,
                                                 SDL_Color base,
                                                 int frame_count,
                                                 int frame_size,
                                                 bool moving) {
    const int sheet_w = frame_count * frame_size;
    const int sheet_h = frame_size;
    std::vector<Uint32> pixels(static_cast<size_t>(sheet_w) * sheet_h, 0);

    const SDL_Color dark{static_cast<Uint8>(base.r * 0.5f),
                         static_cast<Uint8>(base.g * 0.5f),
                         static_cast<Uint8>(base.b * 0.5f), 255};
    const SDL_Color leg{static_cast<Uint8>(base.r * 0.65f),
                        static_cast<Uint8>(base.g * 0.65f),
                        static_cast<Uint8>(base.b * 0.65f), 255};

    const int b = std::max(2, frame_size / 12);  // border thickness
    const int body_top = b;
    const int body_h = frame_size * 7 / 10;    // body occupies the top 70%
    const int leg_top = body_h;
    const int leg_region = frame_size - leg_top - b;
    const int leg_w = std::max(3, frame_size / 7);
    const int leg_x[2] = {frame_size * 3 / 10 - leg_w / 2,
                          frame_size * 7 / 10 - leg_w / 2};

    auto fill_rect = [&](int ox, int x0, int y0, int w0, int h0,
                         Uint32 col) {
        for (int yy = 0; yy < h0; ++yy) {
            for (int xx = 0; xx < w0; ++xx) {
                const int px = ox + x0 + xx;
                const int py = y0 + yy;
                if (px >= 0 && px < sheet_w && py >= 0 && py < sheet_h)
                    pixels[static_cast<size_t>(py) * sheet_w + px] = col;
            }
        }
    };

    for (int f = 0; f < frame_count; ++f) {
        const int ox = f * frame_size;  // frame origin x
        const int bob = moving ? 0 : ((f % 2 == 0) ? 0 : 1);

        // Body: dark border + base fill, centered, with a small bob.
        fill_rect(ox, b, body_top + bob, frame_size - 2 * b, body_h - 2 * b,
                  pack_color(dark));
        fill_rect(ox, b + 1, body_top + bob + 1, frame_size - 2 * b - 2,
                  body_h - 2 * b - 2, pack_color(base));

        // Two legs; heights alternate per frame when walking.
        for (int side = 0; side < 2; ++side) {
            int lh = leg_region;
            if (moving) {
                const bool forward = (f % 2 == 0) == (side == 0);
                lh = leg_region * (forward ? 1.0f : 0.55f);
            }
            fill_rect(ox, leg_x[side], leg_top + (leg_region - lh), leg_w, lh,
                      pack_color(leg));
        }
    }

    return Texture::from_pixels(r, pixels.data(), sheet_w, sheet_h);
}

}  // namespace bd
