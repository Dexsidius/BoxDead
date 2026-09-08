// BoxDead - Enemy entity (chases the player). Two kinds share the same chase
// AI: the green Zombie (general enemy) and the red Devil (a tougher special
// enemy that inherits the zombie's behavior).
#pragma once

#include "boxdead/animated_entity.hpp"

namespace bd {

enum class EnemyKind { Zombie, Devil };

class Enemy : public AnimatedEntity {
public:
    Enemy(EnemyKind kind, float x, float y);

    EnemyKind kind() const { return kind_; }

    void update(float dt, const GameContext& ctx) override;

private:
    EnemyKind kind_;
    float speed_;
};

}  // namespace bd
