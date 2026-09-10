// BoxDead - Spark implementation
#include "boxdead/spark.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace bd {

namespace {
constexpr float kPi = 3.14159265358979323846f;
// Particles slow down fast, so the burst reads as a chip rather than a firework.
constexpr float kDrag = 6.0f;

float frand() { return static_cast<float>(std::rand()) / RAND_MAX; }
}  // namespace

Spark::Spark(float x, float y, Vec2 dir, SDL_Color colour, int count,
             float speed, float life)
    : Entity(x, y, 2.0f, 2.0f),
      count_(std::clamp(count, 1, kMaxParticles)),
      colour_(colour),
      life_(life),
      max_life_(life) {
    // Spray back along the incoming direction: a bullet hitting a wall throws
    // chips toward the shooter, not through the wall.
    float base = 0.0f;
    if (std::abs(dir.x) + std::abs(dir.y) > 0.001f) {
        base = std::atan2(-dir.y, -dir.x);
    } else {
        base = frand() * 2.0f * kPi;
    }
    for (int i = 0; i < count_; ++i) {
        const float a = base + (frand() - 0.5f) * kPi;  // +/-90 degree fan
        const float s = speed * (0.45f + 0.75f * frand());
        parts_[i].pos = Vec2{x, y};
        parts_[i].vel = Vec2{std::cos(a) * s, std::sin(a) * s};
    }
}

void Spark::update(float dt, const GameContext& /*ctx*/) {
    life_ -= dt;
    if (life_ <= 0.0f) {
        alive = false;
        return;
    }
    const float drag = std::max(0.0f, 1.0f - kDrag * dt);
    for (int i = 0; i < count_; ++i) {
        parts_[i].pos.x += parts_[i].vel.x * dt;
        parts_[i].pos.y += parts_[i].vel.y * dt;
        parts_[i].vel.x *= drag;
        parts_[i].vel.y *= drag;
    }
}

void Spark::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    const float t = std::clamp(life_ / max_life_, 0.0f, 1.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, colour_.r, colour_.g, colour_.b,
                           static_cast<Uint8>(colour_.a * t));
    for (int i = 0; i < count_; ++i) {
        // Each particle is a short streak along its own velocity, which reads
        // better at this size than a dot.
        const float px = parts_[i].pos.x - cam_x;
        const float py = parts_[i].pos.y - cam_y;
        const float tail = 0.035f;
        SDL_RenderLine(r, px, py, px - parts_[i].vel.x * tail,
                       py - parts_[i].vel.y * tail);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

}  // namespace bd
