// BoxDead - Player entity (keyboard-driven, holds an inventory of weapons)
#pragma once

#include "boxdead/animated_entity.hpp"
#include "boxdead/iso_sprite.hpp"
#include "boxdead/weapon.hpp"
#include "boxdead/math.hpp"

#include <cmath>
#include <string>

namespace bd {

enum class AimMode { FaceMouse, FaceMovement };

class Player : public AnimatedEntity {
public:
    explicit Player(float x, float y);

    // --- Inventory ---------------------------------------------------------
    // Three fixed slots indexed by WeaponKind: [Pistol, Shotgun, MachineGun].
    // The pistol is always owned with infinite ammo. Acquired weapons persist
    // in the inventory even when empty (shown greyed in the HUD); the player can
    // always fall back to the pistol.
    static constexpr int kSlotCount = 3;

    bool owns(WeaponKind k) const { return owned_[static_cast<int>(k)]; }
    int ammo(WeaponKind k) const { return ammo_[static_cast<int>(k)]; }
    WeaponKind current_kind() const { return current_; }
    int current_ammo() const { return ammo_[static_cast<int>(current_)]; }

    // Acquire a weapon (from a pickup). If new, adds it and switches to it;
    // if already owned, refills its ammo.
    void acquire_weapon(WeaponKind k, int ammo);
    // Switch to a weapon slot if owned.
    void switch_to(WeaponKind k);
    // Cycle to the next owned weapon (dir = +1 or -1).
    void cycle(int dir);

    WeaponSpec current_weapon() const;
    bool consume_ammo();          // true if a shot can be fired now
    std::string weapon_name() const;
    int weapon_ammo() const { return current_ammo(); }

    // --- Gun sprites (non-owning, owned by Game) --------------------------
    void set_gun_texture(WeaponKind k, Texture* t) {
        gun_tex_[static_cast<int>(k)] = t;
    }
    Texture* gun_texture(WeaponKind k) const {
        return gun_tex_[static_cast<int>(k)];
    }
    Texture* current_gun_texture() const {
        return gun_tex_[static_cast<int>(current_)];
    }

    // --- Facing / aim / walk state ----------------------------------------
    // Set the normalized aim direction the body + gun face this frame.
    void set_facing(float dx, float dy) {
        facing_.x = dx;
        facing_.y = dy;
        gun_angle_ = std::atan2(dy, dx);
    }
    Vec2 facing() const { return facing_; }
    float gun_angle() const { return gun_angle_; }

    void set_moving(bool moving, float dx, float dy);
    float walk_phase() const { return walk_phase_; }
    Vec2 last_move_dir() const { return last_move_dir_; }

    void set_color_override(SDL_Color c) { sprite_.color = c; }
    // Set the character's color palette (from the character-select screen).
    void set_style(const IsoCharStyle& s) { style_ = s; }
    const IsoCharStyle& style() const { return style_; }
    void update(float dt, const GameContext& ctx) override;
    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

private:
    IsoCharStyle style_{};  // skin/hair/shirt/pants palette (character skin)
    float speed_;
    bool owned_[kSlotCount] = {true, false, false};
    int ammo_[kSlotCount] = {-1, 0, 0};   // -1 = infinite
    WeaponKind current_ = WeaponKind::Pistol;

    Texture* gun_tex_[kSlotCount] = {};

    Vec2 facing_{0.0f, -1.0f};
    Vec2 last_move_dir_{0.0f, -1.0f};
    float walk_phase_ = 0.0f;
    float gun_angle_ = 0.0f;   // radians
};

}  // namespace bd
