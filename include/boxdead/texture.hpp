// BoxDead - Texture: RAII wrapper around SDL_Texture
#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace bd {

// Owns an SDL_Texture and releases it on destruction. Non-copyable.
class Texture {
public:
    Texture() = default;
    explicit Texture(SDL_Texture* t);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    SDL_Texture* get() const { return tex_; }
    float w() const { return w_; }
    float h() const { return h_; }

    // Build a texture from raw RGBA32 pixel data.
    static std::unique_ptr<Texture> from_pixels(SDL_Renderer* r,
                                               const void* pixels, int w,
                                               int h);

    // Load an image file (BMP out of the box; add SDL_image for PNG/JPG).
    static std::unique_ptr<Texture> load(SDL_Renderer* r,
                                        const std::string& path);

private:
    SDL_Texture* tex_ = nullptr;
    float w_ = 0.0f;
    float h_ = 0.0f;
};

}  // namespace bd
