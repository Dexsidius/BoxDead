// BoxDead - Projectile entity (player-fired bullet)
#pragma once

#include "boxdead/entity.hpp"

namespace bd {

class Projectile : public Entity {
public:
    Projectile(float x, float y, float vx, float vy);

    void set_texture(Texture* tex) { sprite_.texture = tex; }
    void update(float dt, const GameContext& ctx) override;

private:
    float life_;
};

}  // namespace bd
