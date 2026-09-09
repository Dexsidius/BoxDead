// BoxDead - Explosion implementation.
#include "boxdead/explosion.hpp"

#include "boxdead/iso_sprite.hpp"

namespace bd {

Explosion::Explosion(float x, float y, float radius)
    : Entity(x, y, radius * 2.0f, radius * 2.0f), radius_(radius) {}

void Explosion::update(float dt, const GameContext& /*ctx*/) {
    age_ += dt;
    if (age_ >= kDuration) alive = false;
}

void Explosion::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    draw_explosion(r, pos.x - cam_x, pos.y - cam_y, radius_,
                   age_ / kDuration);
}

}  // namespace bd
