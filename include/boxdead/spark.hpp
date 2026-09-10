// BoxDead - Spark: the short-lived burst left where something got hit.
//
// Purely cosmetic and collision-free, like Explosion, but small and cheap: a
// handful of particles thrown out from the impact point that fade over a
// couple of hundred milliseconds. The colour carries the meaning - pale yellow
// chips off a wall, red spray off an enemy.
#pragma once

#include "boxdead/entity.hpp"

namespace bd {

class Spark final : public Entity {
public:
    // `dir` is the direction the shot was travelling; particles fan out around
    // the reflection of it so a wall hit sprays back off the surface.
    Spark(float x, float y, Vec2 dir, SDL_Color colour, int count = 7,
          float speed = 150.0f, float life = 0.22f);

    void update(float dt, const GameContext& ctx) override;
    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

private:
    static constexpr int kMaxParticles = 10;
    struct Particle {
        Vec2 pos;
        Vec2 vel;
    };
    Particle parts_[kMaxParticles];
    int count_;
    SDL_Color colour_;
    float life_;
    float max_life_;
};

}  // namespace bd
