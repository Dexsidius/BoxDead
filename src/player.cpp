// BoxDead - Player implementation
#include "boxdead/player.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace bd {

Player::Player(float x, float y)
    : Entity(x, y, 32.0f, 32.0f), speed_(320.0f) {
    health = 5;  // player survives 5 enemy hits
    sprite_.color = SDL_Color{255, 255, 255, 255};  // no tint by default
}

void Player::equip_weapon(WeaponKind kind, int ammo) {
    weapon_kind_ = kind;
    weapon_ammo_ = ammo;
}

WeaponSpec Player::current_weapon() const {
    return weapon_spec(weapon_kind_);
}

bool Player::consume_ammo() {
    if (weapon_ammo_ < 0) return true;  // infinite ammo
    if (weapon_ammo_ == 0) {
        weapon_kind_ = WeaponKind::Pistol;
        weapon_ammo_ = -1;
        return true;
    }
    --weapon_ammo_;
    if (weapon_ammo_ == 0) {
        weapon_kind_ = WeaponKind::Pistol;
        weapon_ammo_ = -1;
    }
    return true;
}

std::string Player::weapon_name() const {
    return weapon_spec(weapon_kind_).name;
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
    // Normalize diagonals so diagonal speed matches cardinal speed.
    if (dx != 0.0f && dy != 0.0f) {
        const float inv = 1.0f / std::sqrt(2.0f);
        dx *= inv;
        dy *= inv;
    }
    pos.x += dx * speed_ * dt;
    pos.y += dy * speed_ * dt;

    // Clamp to the world bounds.
    const float hw = size.x * 0.5f;
    const float hh = size.y * 0.5f;
    pos.x = std::clamp(pos.x, hw, ctx.world_w - hw);
    pos.y = std::clamp(pos.y, hh, ctx.world_h - hh);
}

}  // namespace bd
