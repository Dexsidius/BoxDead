// BoxDead - Player entity (keyboard-driven, holds the active weapon)
#pragma once

#include "boxdead/entity.hpp"
#include "boxdead/weapon.hpp"

#include <string>

namespace bd {

class Player : public Entity {
public:
    explicit Player(float x, float y);

    void set_texture(Texture* tex) { sprite_.texture = tex; }
    void set_color_override(SDL_Color c) { sprite_.color = c; }
    void update(float dt, const GameContext& ctx) override;

    // Weapon state.
    void equip_weapon(WeaponKind kind, int ammo);
    WeaponSpec current_weapon() const;
    bool consume_ammo();          // true if a shot can be fired; reverts to
                                  // pistol when a finite-ammo weapon runs dry
    std::string weapon_name() const;
    int weapon_ammo() const { return weapon_ammo_; }

private:
    float speed_;
    WeaponKind weapon_kind_ = WeaponKind::Pistol;
    int weapon_ammo_ = -1;  // -1 = infinite
};

}  // namespace bd
