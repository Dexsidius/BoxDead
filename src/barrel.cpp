// BoxDead - Barrel implementation.
#include "boxdead/barrel.hpp"

#include <algorithm>
#include <cmath>

namespace bd {

Barrel::Barrel(float x, float y, float w, float h)
    : Entity(x, y, w, h), height_(h) {
    // The collision box is the full tile (w x h) so barrels tile flush with
    // the level grid, but the drum is drawn a little narrower than that so it
    // reads as a prop standing on the tile rather than filling it.
    health = kMaxHealth;
}

void Barrel::hit(int amount) {
    if (fuse_lit()) return;  // already committed to blowing up
    health -= amount;
    if (health <= 0) {
        health = 0;
        fuse_ = kFuseTime;
    }
}

void Barrel::update(float dt, const GameContext& /*ctx*/) {
    if (!fuse_lit()) return;
    fuse_ = std::max(0.0f, fuse_ - dt);
    // At 0 the barrel is "ready_to_explode"; the Game reaps it that frame.
}

void Barrel::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    IsoBarrelStyle style;
    if (fuse_lit()) {
        // Flash toward white as the fuse burns down, accelerating near the
        // end so the player gets a clear "get clear" tell.
        const float t = 1.0f - fuse_ / kFuseTime;
        const float f = 0.35f + 0.65f * t;  // hotter as the fuse burns down
        const bool on = std::fmod(t * 6.0f, 1.0f) < 0.5f;
        if (on) {
            const auto lerp8 = [f](int a, int b) {
                return static_cast<Uint8>(a + (b - a) * f);
            };
            style.body = SDL_Color{255, lerp8(170, 240), lerp8(80, 220), 255};
            style.band = SDL_Color{255, lerp8(200, 245), lerp8(150, 235), 255};
            style.lid = SDL_Color{255, 248, 226, 255};
        }
    }
    // Feet of the barrel sit slightly below the tile centre so it plants on
    // the floor like the characters do.
    draw_iso_barrel(r, pos.x - cam_x, pos.y + size.y * 0.25f - cam_y,
                    size.x * 0.88f, height_, style);
}

}  // namespace bd
