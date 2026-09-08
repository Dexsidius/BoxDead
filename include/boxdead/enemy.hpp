// BoxDead - Enemy entity (chases the player)
#pragma once

#include "boxdead/entity.hpp"

namespace bd {

class Enemy : public Entity {
public:
    explicit Enemy(float x, float y);

    void set_texture(Texture* tex) { sprite_.texture = tex; }
    void update(float dt, const GameContext& ctx) override;

private:
    float speed_;
};

}  // namespace bd
