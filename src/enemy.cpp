// BoxDead - Enemy implementation
#include "boxdead/enemy.hpp"
#include "boxdead/iso_sprite.hpp"
#include "boxdead/tilemap.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace bd {

namespace {
// Boxhead zombie: sickly green skin, black hair, bloodied white office shirt
// with a red tie, dark slacks.
IsoCharStyle zombie_style() {
    IsoCharStyle s;
    s.skin = SDL_Color{150, 186, 96, 255};
    s.hair = SDL_Color{34, 30, 30, 255};
    s.shirt = SDL_Color{226, 226, 220, 255};
    s.pants = SDL_Color{54, 54, 62, 255};
    s.shoe = SDL_Color{28, 26, 30, 255};
    s.tie = SDL_Color{176, 38, 38, 255};
    s.eye = SDL_Color{28, 40, 26, 255};
    s.blood = true;
    return s;
}

// Red devil: red hide, no hair, two black horns, dark red body.
IsoCharStyle devil_style() {
    IsoCharStyle s;
    s.skin = SDL_Color{206, 58, 48, 255};
    s.hair = SDL_Color{0, 0, 0, 0};  // bald, so the horns read clearly
    s.shirt = SDL_Color{138, 30, 28, 255};
    s.pants = SDL_Color{86, 20, 20, 255};
    s.shoe = SDL_Color{40, 14, 14, 255};
    s.horn = SDL_Color{32, 22, 24, 255};
    s.eye = SDL_Color{252, 214, 96, 255};  // glowing eyes
    return s;
}
}  // namespace

Enemy::Enemy(EnemyKind kind, float x, float y)
    : AnimatedEntity(x, y, 36.0f, 36.0f), kind_(kind), speed_(120.0f) {
    // The Devil inherits the zombie's behavior but is a tougher "special"
    // enemy (2 HP instead of 1). Same speed and chase AI.
    health = (kind_ == EnemyKind::Devil) ? 2 : 1;
    sprite_.color = SDL_Color{255, 255, 255, 255};  // no tint (texture carries color)
}

void Enemy::update(float dt, const GameContext& ctx) {
    Vec2 d{ctx.player_pos.x - pos.x, ctx.player_pos.y - pos.y};
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > 1.0f) {
        d.x /= len;
        d.y /= len;
        const float sx = d.x * speed_ * dt;
        const float sy = d.y * speed_ * dt;
        // Slide along walls and barrels: block each axis that enters one.
        if (!ctx.blocked(pos.x + sx, pos.y)) pos.x += sx;
        if (!ctx.blocked(pos.x, pos.y + sy)) pos.y += sy;
        facing_ = d;  // face the player
    }

    // Enemies are always chasing, so the walk cycle runs continuously.
    walk_phase_ += dt * 8.0f;
    play_animation("walk");
    update_animator(dt);
}

void Enemy::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    IsoCharStyle style =
        (kind_ == EnemyKind::Devil) ? devil_style() : zombie_style();
    style.tint = sprite_.color;  // flashes when the Game tints a hit enemy
    // Same as the player: the figure is drawn bigger than its hitbox so the
    // characters read at Boxhead scale without making the horde unfair.
    constexpr float kDrawScale = 1.3f;
    draw_iso_character(r, pos.x - cam_x, pos.y + size.y * 0.25f - cam_y,
                       size.x * kDrawScale, size.y * kDrawScale,
                       facing_.x, facing_.y, walk_phase_, style, nullptr, 0.0f);
}

}  // namespace bd
