// BoxDead - Enemy entity (chases the player). Every kind shares the same chase
// AI and differs only in stats and palette:
//
//   Zombie       - the general enemy. 1 HP, base speed.
//   Devil        - tougher special enemy, lobs a fireball at point-blank range.
//   YellowDemon  - the every-5th-wave mini-boss. Faster than the rest and
//                  shoots fireballs from well outside melee range.
//   Boss         - the end-of-level boss. Big, slow, heavy health pool, fires a
//                  spread of fireballs, and carries a unique name that the HUD
//                  shows over a Dark-Souls-style bar.
#pragma once

#include "boxdead/animated_entity.hpp"

#include <algorithm>
#include <string>

namespace bd {

enum class EnemyKind { Zombie, Devil, YellowDemon, Boss };

class Enemy : public AnimatedEntity {
public:
    Enemy(EnemyKind kind, float x, float y);

    EnemyKind kind() const { return kind_; }
    bool is_boss() const { return kind_ == EnemyKind::Boss; }

    // --- Ranged attack profile (per kind) ---------------------------------
    // Kinds that lob fireballs at the player when they have line of sight.
    bool can_shoot() const { return kind_ != EnemyKind::Zombie; }
    float fire_range() const;      // world pixels
    float fire_interval() const;   // seconds between volleys
    int fire_count() const;        // fireballs per volley
    float fire_spread_deg() const; // cone the volley is spread across

    // --- Crowd control -----------------------------------------------------
    // A concussion blast freezes an enemy in place; a lure blast makes it walk
    // to a point instead of chasing the player. Both simply run down a timer.
    void stun(float seconds) { stun_ = std::max(stun_, seconds); }
    bool stunned() const { return stun_ > 0.0f; }
    void lure_to(Vec2 point, float seconds) {
        lure_pos_ = point;
        lure_ = std::max(lure_, seconds);
    }
    bool lured() const { return lure_ > 0.0f; }

    float ranged_cooldown() const { return ranged_cd_; }
    void reset_ranged_cooldown(float cd) { ranged_cd_ = cd; }
    void tick_ranged_cooldown(float dt) {
        if (ranged_cd_ > 0.0f) ranged_cd_ -= dt;
    }

    // --- Boss identity ----------------------------------------------------
    // Health it spawned with, so the HUD can draw a proportional bar.
    int max_health() const { return max_health_; }
    // Unique name shown above the boss and over its health bar. Empty for
    // ordinary enemies.
    const std::string& name() const { return name_; }
    void set_name(std::string n) { name_ = std::move(n); }

    void update(float dt, const GameContext& ctx) override;
    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

private:
    EnemyKind kind_;
    float speed_;
    int max_health_;
    std::string name_;
    float walk_phase_ = 0.0f;
    float ranged_cd_ = 0.0f;
    float stun_ = 0.0f;
    float lure_ = 0.0f;
    Vec2 lure_pos_{};
    Vec2 facing_{0.0f, 1.0f};  // toward whatever it is walking at
};

}  // namespace bd
