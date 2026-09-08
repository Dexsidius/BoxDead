// BoxDead - Weapon: value-type weapon profiles (no polymorphism needed yet).
#pragma once

namespace bd {

enum class WeaponKind { Pistol, Shotgun, MachineGun };

// Static description of a weapon. Looked up via weapon_spec(kind). The active
// weapon state (kind + ammo) lives on the Player; Game asks the player for the
// current spec when firing, so there is one firing path instead of a switch.
struct WeaponSpec {
    WeaponKind kind;
    const char* name;
    float cooldown;          // seconds between shots
    float projectile_speed;  // px/s
    int projectile_count;    // projectiles emitted per shot
    float spread_degrees;    // total width of the fire cone
    int damage;              // hit damage per projectile
    int ammo;                // -1 = infinite
};

WeaponSpec weapon_spec(WeaponKind kind);

}  // namespace bd
