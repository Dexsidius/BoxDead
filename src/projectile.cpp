// BoxDead - Projectile implementation
#include "boxdead/projectile.hpp"

namespace bd {

Projectile::Projectile(float x, float y, float vx, float vy)
    : Entity(x, y, 6.0f, 6.0f), life_(2.5f) {
    vel.x = vx;
    vel.y = vy;
    sprite_.color = SDL_Color{255, 220, 60, 255};
}

void Projectile::update(float dt, const GameContext& ctx) {
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
    life_ -= dt;
    const float hw = size.x * 0.5f;
    const float hh = size.y * 0.5f;
    if (life_ <= 0.0f || pos.x < -hw || pos.x > ctx.world_w + hw ||
        pos.y < -hh || pos.y > ctx.world_h + hh) {
        alive = false;
    }
}

}  // namespace bd
