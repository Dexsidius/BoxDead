// BoxDead - Sprite system
// Texture: RAII wrapper around SDL_Texture (create from pixels or load a file).
// Sprite: a renderable quad - a texture region with an optional color fallback.
#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

// Owns an SDL_Texture and releases it on destruction.
class Texture {
public:
    Texture() : tex_(nullptr, SDL_DestroyTexture) {}
    explicit Texture(SDL_Texture* t) : tex_(t, SDL_DestroyTexture) {
        if (t) SDL_GetTextureSize(t, &w_, &h_);
    }

    SDL_Texture* get() const { return tex_.get(); }
    float w() const { return w_; }
    float h() const { return h_; }

    // Build a texture from raw RGBA32 pixel data.
    static std::unique_ptr<Texture> from_pixels(SDL_Renderer* r,
                                                const void* pixels, int w,
                                                int h) {
        SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STATIC, w, h);
        if (!t) return nullptr;
        SDL_UpdateTexture(t, nullptr, pixels, w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        return std::make_unique<Texture>(t);
    }

    // Load an image file (BMP out of the box; add SDL_image for PNG/JPG).
    static std::unique_ptr<Texture> load(SDL_Renderer* r,
                                        const std::string& path) {
        SDL_Surface* s = SDL_LoadBMP(path.c_str());
        if (!s) return nullptr;
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
        return t ? std::make_unique<Texture>(t) : nullptr;
    }

private:
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> tex_;
    float w_ = 0.0f;
    float h_ = 0.0f;
};

// A renderable sprite: an optional texture region plus a fallback color.
struct Sprite {
    Texture* texture = nullptr;            // optional; if null, draws color
    SDL_FRect src{};                        // texture region (0 = whole tex)
    SDL_Color color{255, 255, 255, 255};    // tint / fallback color
};

// Draw a sprite centered at (cx, cy) with the given world size.
inline void draw_sprite(SDL_Renderer* r, const Sprite& s, float cx, float cy,
                        float w, float h) {
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

// Procedurally generate a solid-color sprite texture with a darker border,
// so the placeholder art looks like a framed sprite rather than a flat box.
inline std::unique_ptr<Texture> make_solid_sprite_texture(SDL_Renderer* r,
                                                          SDL_Color base,
                                                          int size = 32) {
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
