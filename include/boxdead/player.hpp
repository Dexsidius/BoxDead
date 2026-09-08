// BoxDead - Player entity (keyboard-driven)
#pragma once

#include "boxdead/entity.hpp"

namespace bd {

class Player : public Entity {
public:
    explicit Player(float x, float y);

    void set_texture(Texture* tex) { sprite_.texture = tex; }
    void update(float dt, const GameContext& ctx) override;

private:
    float speed_;
};

}  // namespace bd
