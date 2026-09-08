// BoxDead - Projectile entity (player-fired bullet)
#pragma once

#include "boxdead/entity.hpp"

namespace bd {

class Projectile : public Entity {
public:
    Projectile(float x, float y, float vx, float vy, int dmg = 1);

    void set_texture(Texture* tex) { sprite_.texture = tex; }
    void update(float dt, const GameContext& ctx) override;

    int damage_amount = 1;  // applied to enemies on hit

private:
    float life_;
};

}  // namespace bd
