// BoxDead - Explosion: the purely visual fireball left behind by a barrel.
//
// Damage is applied once, by the Game, at the instant of detonation; this
// entity only plays the animation out and then removes itself. It has no
// collision and never blocks movement.
#pragma once

#include "boxdead/entity.hpp"

namespace bd {

class Explosion final : public Entity {
public:
    // `radius` is the blast radius in world pixels (matches the damage radius
    // so the fireball covers exactly what it hurt).
    Explosion(float x, float y, float radius);

    void update(float dt, const GameContext& ctx) override;
    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

private:
    static constexpr float kDuration = 0.45f;  // seconds
    float radius_;
    float age_ = 0.0f;
};

}  // namespace bd
