// BoxDead - Barrel: an explosive prop placed by the level tileset.
//
// Barrels block movement like a wall until they are shot. Taking enough damage
// lights a short fuse (the barrel flashes white), and when it burns out the
// Game detonates it: everything inside the blast radius is damaged, including
// other barrels, which light their own fuses and chain the explosion outward.
//
// The Game owns the blast itself (it is the only thing that can see every
// entity); a Barrel just tracks its own health and fuse.
#pragma once

#include "boxdead/entity.hpp"
#include "boxdead/iso_sprite.hpp"

namespace bd {

class Barrel final : public Entity {
public:
    // Shots a barrel survives before its fuse lights.
    static constexpr int kMaxHealth = 3;
    // Seconds between the fuse lighting and the blast (the flash window).
    static constexpr float kFuseTime = 0.22f;
    // Blast radius in world pixels and the damage dealt inside it.
    static constexpr float kBlastRadius = 110.0f;
    static constexpr int kBlastDamage = 5;

    Barrel(float x, float y, float w, float h);

    // Apply damage from a bullet or a nearby blast. Once the fuse is lit
    // further damage is ignored (it is already going to explode).
    void hit(int amount);

    bool fuse_lit() const { return fuse_ >= 0.0f; }
    // True on the frame the fuse burns out. The Game consumes this, kills the
    // barrel, and spawns the explosion.
    bool ready_to_explode() const { return fuse_lit() && fuse_ <= 0.0f; }

    void update(float dt, const GameContext& ctx) override;
    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

private:
    float fuse_ = -1.0f;   // <0 = unlit
    float height_;         // standing height in pixels (taller than the tile)
};

}  // namespace bd
