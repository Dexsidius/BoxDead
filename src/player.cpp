// BoxDead - Player implementation
#include "boxdead/player.hpp"

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
    pos.x += dx * speed_ * dt;
    pos.y += dy * speed_ * dt;

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

void Player::render(SDL_Renderer* r) const {
    // Body (legacy textured sprite; the isometric renderer replaces this
    // in a later step).
    AnimatedEntity::render(r);

    // Draw the currently-equipped gun in the hand, rotated to the aim angle.
    // The gun art points +x (right) at angle 0; the grip sits near the
    // lower-left, so we rotate around that pivot to keep the hand stable.
    Texture* gun = current_gun_texture();
    if (gun && gun->get()) {
        constexpr double kPi = 3.14159265358979323846;
        const double angle_deg = gun_angle_ * 180.0 / kPi;
        const float scale = 1.2f;
        const float gw = static_cast<float>(gun->w()) * scale;
        const float gh = static_cast<float>(gun->h()) * scale;
        const float px_frac = 0.15f;
        const float py_frac = 0.8f;
        const SDL_FPoint center{px_frac * gw, py_frac * gh};
        const SDL_FRect dst{pos.x - center.x, pos.y - center.y, gw, gh};
        SDL_RenderTextureRotated(r, gun->get(), nullptr, &dst, angle_deg,
                                 &center, SDL_FLIP_NONE);
    }
}

}  // namespace bd
