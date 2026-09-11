// BoxDead - Projectile entity (bullets, rockets and thrown grenades).
//
// One entity covers all three because they only differ in what they carry and
// what ends their flight:
//
//   bullet   - no payload; dies on a wall, an enemy, or when its life runs out
//   rocket   - payload with no fuse; detonates on the first thing it touches
//   grenade  - payload with a fuse; slows to a stop and detonates on the timer
//
// The Game owns detonation (it is the only thing that can see every entity), so
// a projectile just raises `blast_pending` and waits to be reaped.
#pragma once

#include "boxdead/entity.hpp"

namespace bd {

// What a projectile leaves behind when it goes off. A zero radius means the
// projectile is a plain bullet and simply disappears.
struct Payload {
    float blast_radius = 0.0f;
    int blast_damage = 0;
    float stun_seconds = 0.0f;   // concussion: freezes enemies caught in it
    float lure_radius = 0.0f;    // lure: pulls enemies toward the blast point
    float lure_seconds = 0.0f;

    bool explodes() const { return blast_radius > 0.0f; }
};

class Projectile : public Entity {
public:
    Projectile(float x, float y, float vx, float vy, int dmg = 1);

    void set_texture(Texture* tex) { sprite_.texture = tex; }
    void update(float dt, const GameContext& ctx) override;
    // Drawn at the texture's own size and rotated to the direction of travel,
    // so a bullet sprite points where it is going instead of flying sideways.
    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

    // Arm this projectile with an explosive payload. `fuse` > 0 makes it a
    // thrown grenade (drags to a stop, detonates on the timer); otherwise it
    // detonates on first contact.
    void arm(const Payload& p, float fuse);

    // Called by the Game when this projectile hits something it should go off
    // on. Bullets just die; armed projectiles raise blast_pending.
    void on_hit();

    int damage_amount = 1;  // applied to enemies on a direct hit
    // Hostile projectiles (devil/demon/boss fireballs) damage the player;
    // player-fired shots are not hostile and damage enemies.
    bool hostile = false;
    // Set when this projectile should detonate; the Game consumes it, kills the
    // projectile and applies the blast.
    bool blast_pending = false;
    // Set on the frame this shot stopped against level geometry. The Game
    // consumes it to throw a spark and play the impact sound - a projectile
    // cannot spawn entities or reach the mixer itself.
    bool hit_wall = false;
    // Travel direction at the moment of impact, so the spark sprays back off
    // the surface instead of firing in an arbitrary direction.
    Vec2 hit_dir{};
    Payload payload;

    bool armed() const { return payload.explodes(); }
    bool thrown() const { return fuse_ > 0.0f; }

private:
    float life_;
    float fuse_ = -1.0f;  // <=0 = not fused (detonates on impact instead)
};

}  // namespace bd
