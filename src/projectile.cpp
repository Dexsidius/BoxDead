// BoxDead - Projectile implementation
#include "boxdead/projectile.hpp"
#include "boxdead/tilemap.hpp"

#include <algorithm>
#include <cmath>

namespace bd {

namespace {
// Thrown grenades shed speed so they come to rest near where they land rather
// than sliding across the whole map while the fuse burns.
constexpr float kThrownDrag = 1.9f;   // per second
// Movement is stepped at no more than this, so a fast shot cannot tunnel
// through a 32px wall in a single frame.
constexpr float kMaxStep = 8.0f;
}  // namespace

Projectile::Projectile(float x, float y, float vx, float vy, int dmg)
    : Entity(x, y, 6.0f, 6.0f), damage_amount(dmg), life_(2.5f) {
    vel.x = vx;
    vel.y = vy;
    sprite_.color = SDL_Color{255, 255, 255, 255};
}

void Projectile::arm(const Payload& p, float fuse) {
    payload = p;
    fuse_ = fuse;
    if (fuse_ > 0.0f) {
        // A grenade lives as long as its fuse; a rocket keeps the default.
        life_ = fuse_ + 0.1f;
        size = Vec2{10.0f, 10.0f};
    } else {
        size = Vec2{9.0f, 9.0f};
    }
}

void Projectile::on_hit() {
    if (armed()) {
        blast_pending = true;
    } else {
        alive = false;
    }
}

void Projectile::update(float dt, const GameContext& ctx) {
    if (thrown()) {
        const float drag = std::max(0.0f, 1.0f - kThrownDrag * dt);
        vel.x *= drag;
        vel.y *= drag;
        fuse_ -= dt;
        if (fuse_ <= 0.0f) {
            blast_pending = true;
            return;
        }
    }

    // Step the movement so a fast projectile tests every wall it passes
    // through. Only *tile* solidity stops a shot: barrels are handled by the
    // Game's overlap pass so that shooting one damages it instead of the
    // bullet stopping dead against an obstacle it never hurt.
    const float dxt = vel.x * dt;
    const float dyt = vel.y * dt;
    const float dist = std::sqrt(dxt * dxt + dyt * dyt);
    const int steps = std::max(1, static_cast<int>(dist / kMaxStep) + 1);
    for (int i = 0; i < steps; ++i) {
        pos.x += dxt / steps;
        pos.y += dyt / steps;
        if (ctx.tilemap && ctx.tilemap->is_solid(pos.x, pos.y)) {
            const float vlen = std::sqrt(vel.x * vel.x + vel.y * vel.y);
            if (vlen > 0.001f) hit_dir = Vec2{vel.x / vlen, vel.y / vlen};
            hit_wall = true;
            if (thrown()) {
                // Grenades bonk off the wall and drop: back the step out and
                // stop moving, but keep counting down.
                pos.x -= dxt / steps;
                pos.y -= dyt / steps;
                vel.x = 0.0f;
                vel.y = 0.0f;
                hit_wall = false;  // a bounce, not an impact
                return;
            }
            on_hit();  // bullets die here, rockets detonate against the wall
            return;
        }
    }

    life_ -= dt;
    const float hw = size.x * 0.5f;
    const float hh = size.y * 0.5f;
    if (pos.x < -hw || pos.x > ctx.world_w + hw ||
        pos.y < -hh || pos.y > ctx.world_h + hh) {
        alive = false;
        return;
    }
    if (life_ <= 0.0f) {
        // A rocket that simply ran out of range still goes off.
        if (armed()) blast_pending = true;
        else alive = false;
    }
}

}  // namespace bd
