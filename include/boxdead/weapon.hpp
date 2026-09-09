// BoxDead - Weapon: value-type weapon profiles (no polymorphism needed yet).
#pragma once

namespace bd {

// Order is the inventory order, and the HUD binds slot keys 1..N to it.
// `Count` must stay last: it sizes the player's inventory arrays.
enum class WeaponKind {
    Pistol,
    Shotgun,
    MachineGun,
    RocketLauncher,
    Grenade,
    Concussion,
    Lure,
    Count,
};

// Static description of a weapon. Looked up via weapon_spec(kind). The active
// weapon state (kind + ammo) lives on the Player; Game asks the player for the
// current spec when firing, so there is one firing path instead of a switch.
//
// Explosives are described by the same struct rather than a subclass: a shot
// carries a payload, and `blast_radius > 0` is what makes it explode. That
// keeps one firing path for bullets, rockets and thrown grenades alike.
struct WeaponSpec {
    WeaponKind kind;
    const char* name;
    float cooldown;          // seconds between shots
    float projectile_speed;  // px/s
    int projectile_count;    // projectiles emitted per shot
    float spread_degrees;    // total width of the fire cone
    int damage;              // direct hit damage per projectile
    int ammo;                // -1 = infinite

    // --- payload (blast_radius == 0 means a plain bullet) -----------------
    float blast_radius;   // world px
    int blast_damage;     // applied to everything inside the radius
    // >0 makes the shot *thrown*: it slows to a stop and detonates when the
    // fuse burns out. <=0 detonates on the first thing it touches.
    float fuse;
    float stun_seconds;   // concussion: enemies caught are frozen this long
    float lure_radius;    // lure: enemies this far away are drawn to the blast
    float lure_seconds;   // how long they stay lured
};

WeaponSpec weapon_spec(WeaponKind kind);

// True if the weapon lobs a fused projectile rather than firing one.
bool is_thrown(WeaponKind kind);

}  // namespace bd
