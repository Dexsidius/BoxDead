// BoxDead - Enemy implementation
#include "boxdead/enemy.hpp"

#include <cmath>

namespace bd {

Enemy::Enemy(float x, float y)
    : AnimatedEntity(x, y, 28.0f, 28.0f), speed_(120.0f) {
    sprite_.color = SDL_Color{255, 255, 255, 255};  // no tint (texture is red)
}

void Enemy::update(float dt, const GameContext& ctx) {
    Vec2 d{ctx.player_pos.x - pos.x, ctx.player_pos.y - pos.y};
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > 1.0f) {
        d.x /= len;
        d.y /= len;
        pos.x += d.x * speed_ * dt;
        pos.y += d.y * speed_ * dt;
    }

    // Enemies are always chasing, so the walk cycle runs continuously.
    play_animation("walk");
    update_animator(dt);
}

}  // namespace bd
