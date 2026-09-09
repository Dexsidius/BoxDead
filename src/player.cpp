// BoxDead - Player implementation
#include "boxdead/player.hpp"
#include "boxdead/iso_sprite.hpp"
#include "boxdead/tilemap.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace bd {

Player::Player(float x, float y)
    : AnimatedEntity(x, y, 32.0f, 32.0f), speed_(320.0f) {
    health = 5;  // player survives 5 enemy hits
    sprite_.color = SDL_Color{255, 255, 255, 255};  // no tint by default
}

void Player::acquire_weapon(WeaponKind k, int ammo) {
    const int i = static_cast<int>(k);
    if (!owned_[i]) {
        owned_[i] = true;
        ammo_[i] = ammo;
        current_ = k;
    } else if (ammo_[i] >= 0) {
        // Already owned: refill ammo (capped to keep the HUD sane).
        ammo_[i] = std::min(60, ammo_[i] + ammo);
    }
    // Infinite-ammo weapons (pistol) keep -1.
}

void Player::switch_to(WeaponKind k) {
    if (owned_[static_cast<int>(k)]) current_ = k;
}

void Player::cycle(int dir) {
    for (int step = 1; step <= kSlotCount; ++step) {
        const int i = (static_cast<int>(current_) +
                       step * (dir > 0 ? 1 : kSlotCount - 1)) % kSlotCount;
        if (owned_[i]) {
            current_ = static_cast<WeaponKind>(i);
            return;
        }
    }
}

WeaponSpec Player::current_weapon() const {
    return weapon_spec(current_);
}

bool Player::consume_ammo() {
    const int i = static_cast<int>(current_);
    if (ammo_[i] < 0) return true;  // infinite
    if (ammo_[i] <= 0) return false;  // empty, can't fire
    --ammo_[i];
    // Auto-fall back to the pistol when a finite weapon runs dry.
    if (ammo_[i] == 0 && current_ != WeaponKind::Pistol) {
        current_ = WeaponKind::Pistol;
    }
    return true;
}

std::string Player::weapon_name() const {
    return weapon_spec(current_).name;
}

void Player::set_moving(bool moving, float dx, float dy) {
    if (moving) {
        last_move_dir_.x = dx;
        last_move_dir_.y = dy;
    }
    // Advance the walk phase while moving; the iso renderer samples it.
    if (moving) {
        walk_phase_ += 0.0f;  // advanced by dt in update()
    }
}

void Player::update(float dt, const GameContext& ctx) {
    float dx = 0.0f;
    float dy = 0.0f;
    if (ctx.keys) {
        if (ctx.keys[SDL_SCANCODE_A] || ctx.keys[SDL_SCANCODE_LEFT]) dx -= 1.0f;
        if (ctx.keys[SDL_SCANCODE_D] || ctx.keys[SDL_SCANCODE_RIGHT]) dx += 1.0f;
        if (ctx.keys[SDL_SCANCODE_W] || ctx.keys[SDL_SCANCODE_UP]) dy -= 1.0f;
        if (ctx.keys[SDL_SCANCODE_S] || ctx.keys[SDL_SCANCODE_DOWN]) dy += 1.0f;
    }
    const bool moving = (dx != 0.0f || dy != 0.0f);
    // Normalize diagonals so diagonal speed matches cardinal speed.
    if (moving) {
        const float inv = 1.0f / std::sqrt(dx * dx + dy * dy);
        dx *= inv;
        dy *= inv;
        last_move_dir_.x = dx;
        last_move_dir_.y = dy;
    }
    // Move per-axis so the player slides along walls and barrels instead of
    // sticking. Each axis is blocked if the destination centre lands inside a
    // solid tile or a dynamic obstacle.
    const float step_x = dx * speed_ * dt;
    const float step_y = dy * speed_ * dt;
    if (!ctx.blocked(pos.x + step_x, pos.y)) pos.x += step_x;
    if (!ctx.blocked(pos.x, pos.y + step_y)) pos.y += step_y;

    // Clamp to the world bounds.
    const float hw = size.x * 0.5f;
    const float hh = size.y * 0.5f;
    pos.x = std::clamp(pos.x, hw, ctx.world_w - hw);
    pos.y = std::clamp(pos.y, hh, ctx.world_h - hh);

    // Advance the walk phase while moving (for the iso leg animation).
    if (moving) walk_phase_ += dt * 8.0f;

    // Keep the legacy animator ticking (used by the smoke metric).
    play_animation(moving ? "walk" : "idle");
    update_animator(dt);
}

void Player::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    // Isometric character: shaded, outlined box body + head that yaws to face
    // the aim direction, animated legs, arms holding the current gun. The
    // palette comes from the character-select screen (default blue).
    IsoCharStyle style = style_;
    // sprite_.color carries the i-frame flash the Game sets on a hit; feed it
    // in as the palette tint so every part of the figure flashes together.
    style.tint = sprite_.color;
    // Drawn larger than the collision box on purpose: a Boxhead survivor
    // reads as about one and a half tiles tall, but a hitbox that size would
    // make the player impossible to squeeze between obstacles.
    constexpr float kDrawScale = 1.4f;
    draw_iso_character(r, pos.x - cam_x, pos.y + size.y * 0.25f - cam_y,
                       size.x * kDrawScale, size.y * kDrawScale,
                       facing_.x, facing_.y, walk_phase_, style,
                       current_gun_texture(), gun_angle_);
}

}  // namespace bd
