// BoxDead - Font implementation
#include "boxdead/font.hpp"

namespace bd {

Font::~Font() {
    if (font_) TTF_CloseFont(font_);
}

bool Font::load(SDL_Renderer* r, const std::string& path, int size) {
    renderer_ = r;
    font_ = TTF_OpenFont(path.c_str(), size);
    return font_ != nullptr;
}

int Font::text_width(const std::string& text) {
    if (!font_ || text.empty()) return 0;
    int w = 0;
    int h = 0;
    TTF_GetStringSize(font_, text.c_str(), text.size(), &w, &h);
    return w;
}

void Font::draw(const std::string& text, float x, float y, SDL_Color color) {
    if (!font_ || text.empty()) return;
    auto it = cache_.find(text);
    if (it == cache_.end()) {
        SDL_Surface* s = TTF_RenderText_Blended(font_, text.c_str(),
                                                text.size(), color);
        if (!s) return;
        cache_[text] = Texture::from_pixels(renderer_, s->pixels, s->w, s->h);
        SDL_DestroySurface(s);
        it = cache_.find(text);
        if (it == cache_.end()) return;
    }
    Texture* tex = it->second.get();
    SDL_FRect dst{x, y, tex->w(), tex->h()};
    SDL_RenderTexture(renderer_, tex->get(), nullptr, &dst);
}

}  // namespace bd
