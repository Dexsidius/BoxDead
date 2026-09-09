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

    // Cooldown for the Devil's ranged fireball attack (seconds).
    float ranged_cooldown() const { return ranged_cd_; }
    void reset_ranged_cooldown(float cd) { ranged_cd_ = cd; }
    void tick_ranged_cooldown(float dt) {
        if (ranged_cd_ > 0.0f) ranged_cd_ -= dt;
    }

    void update(float dt, const GameContext& ctx) override;
    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

private:
    EnemyKind kind_;
    float speed_;
    float walk_phase_ = 0.0f;
    float ranged_cd_ = 0.0f;
    Vec2 facing_{0.0f, 1.0f};  // toward the player
};

}  // namespace bd
