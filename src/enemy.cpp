// BoxDead - Enemy implementation
#include "boxdead/enemy.hpp"
#include "boxdead/iso_sprite.hpp"
#include "boxdead/tilemap.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace bd {

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
        // Slide along walls: block each axis if it enters a solid tile.
        if (ctx.tilemap) {
            if (!ctx.tilemap->is_solid(pos.x + sx, pos.y)) pos.x += sx;
            if (!ctx.tilemap->is_solid(pos.x, pos.y + sy)) pos.y += sy;
        } else {
            pos.x += sx;
            pos.y += sy;
        }
        facing_ = d;  // face the player
    }

    // Enemies are always chasing, so the walk cycle runs continuously.
    walk_phase_ += dt * 8.0f;
    play_animation("walk");
    update_animator(dt);
}

void Enemy::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    IsoCharStyle style;
    if (kind_ == EnemyKind::Devil) {
        style = IsoCharStyle{
            SDL_Color{235, 90, 90, 255},
            SDL_Color{211, 47, 47, 255},
            SDL_Color{150, 30, 30, 255},
            SDL_Color{245, 110, 110, 255},
            SDL_Color{220, 60, 60, 255},
            SDL_Color{150, 30, 30, 255},
            SDL_Color{74, 16, 16, 255},
            SDL_Color{0, 0, 0, 90},
        };
    } else {
        style = IsoCharStyle{
            SDL_Color{190, 210, 120, 255},
            SDL_Color{158, 194, 90, 255},
            SDL_Color{110, 150, 60, 255},
            SDL_Color{200, 215, 130, 255},
            SDL_Color{170, 200, 100, 255},
            SDL_Color{120, 160, 70, 255},
            SDL_Color{58, 58, 58, 255},
            SDL_Color{0, 0, 0, 90},
        };
    }
    draw_iso_character(r, pos.x - cam_x, pos.y + size.y * 0.25f - cam_y, size.x, size.y,
                       facing_.x, facing_.y, walk_phase_, style, nullptr,
                       0.0f);
}

}  // namespace bd
