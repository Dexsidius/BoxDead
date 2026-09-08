// BoxDead - Texture implementation
#include "boxdead/texture.hpp"

namespace bd {

Texture::Texture(SDL_Texture* t) : tex_(t) {
    if (t) SDL_GetTextureSize(t, &w_, &h_);
}

Texture::~Texture() {
    if (tex_) SDL_DestroyTexture(tex_);
}

std::unique_ptr<Texture> Texture::from_pixels(SDL_Renderer* r,
                                               const void* pixels, int w,
                                               int h) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                        SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, pixels, w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return std::make_unique<Texture>(t);
}

std::unique_ptr<Texture> Texture::load(SDL_Renderer* r,
                                       const std::string& path) {
    SDL_Surface* s = SDL_LoadBMP(path.c_str());
    if (!s) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_DestroySurface(s);
    return t ? std::make_unique<Texture>(t) : nullptr;
}

}  // namespace bd
