// BoxDead - Item: pickups the player touches to collect.
#pragma once

#include "boxdead/entity.hpp"
#include "boxdead/weapon.hpp"

namespace bd {

class Game;
class Player;

// Base class for collectible items. Items are static entities; the Game runs a
// pickup collision pass and calls on_pickup() on contact, then removes the
// item. Side effects (healing, equipping weapons, toast feedback) live in the
// concrete subclass, not in Game.
class Item : public Entity {
public:
    Item(float x, float y, float w, float h) : Entity(x, y, w, h) {
        sprite_.color = SDL_Color{255, 255, 255, 255};  // no tint
    }

    void set_texture(Texture* tex) { sprite_.texture = tex; }
    void update(float /*dt*/, const GameContext& /*ctx*/) override {}
    virtual void on_pickup(Game& game, Player& player) = 0;
};

// Restores 2 HP (capped at the player max).
class HealthPickup final : public Item {
public:
    explicit HealthPickup(float x, float y);
    void on_pickup(Game& game, Player& player) override;
};

// Equips a weapon (with finite ammo, or -1 for the pistol).
class WeaponPickup final : public Item {
public:
    WeaponPickup(float x, float y, WeaponKind kind, int ammo);
    void on_pickup(Game& game, Player& player) override;

private:
    WeaponKind kind_;
    int ammo_;
};

}  // namespace bd
