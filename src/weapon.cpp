// BoxDead - Weapon implementation
#include "boxdead/weapon.hpp"

namespace bd {

// Fields, in order:
//   kind, name, cooldown, speed, count, spread, damage, ammo,
//   blast_radius, blast_damage, fuse, stun, lure_radius, lure_seconds
WeaponSpec weapon_spec(WeaponKind k) {
    switch (k) {
        case WeaponKind::Shotgun:
            return {k, "Shotgun", 0.65f, 520.0f, 5, 24.0f, 1, 6,
                    0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f};
        case WeaponKind::MachineGun:
            return {k, "Machine Gun", 0.09f, 720.0f, 1, 6.0f, 1, 30,
                    0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f};
        // The heavy hitter: detonates on the first thing it touches, and the
        // only thing in the game that out-damages a barrel.
        case WeaponKind::RocketLauncher:
            return {k, "Rocket Launcher", 0.95f, 430.0f, 1, 0.0f, 3, 5,
                    130.0f, 8, 0.0f, 0.0f, 0.0f, 0.0f};
        // Thrown, ~1.1s fuse, a little weaker than a barrel.
        case WeaponKind::Grenade:
            return {k, "Grenade", 0.80f, 300.0f, 1, 0.0f, 0, 6,
                    105.0f, 5, 1.10f, 0.0f, 0.0f, 0.0f};
        // Wide but almost harmless: the point is the 3s freeze, not the damage.
        case WeaponKind::Concussion:
            return {k, "Concussion", 0.80f, 300.0f, 1, 0.0f, 0, 4,
                    135.0f, 1, 1.10f, 3.0f, 0.0f, 0.0f};
        // Gathers a crowd from a long way off, then pops with a *small* blast:
        // the lure radius is deliberately far wider than the kill radius, so it
        // groups enemies up for something else rather than wiping them itself.
        case WeaponKind::Lure:
            return {k, "Lure Grenade", 0.90f, 280.0f, 1, 0.0f, 0, 4,
                    60.0f, 2, 2.20f, 0.0f, 300.0f, 3.5f};
        case WeaponKind::Pistol:
        default:
            return {k, "Pistol", 0.22f, 600.0f, 1, 0.0f, 1, -1,
                    0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f};
    }
}

bool is_thrown(WeaponKind kind) { return weapon_spec(kind).fuse > 0.0f; }

}  // namespace bd
