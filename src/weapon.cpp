// BoxDead - Weapon implementation
#include "boxdead/weapon.hpp"

namespace bd {

WeaponSpec weapon_spec(WeaponKind k) {
    switch (k) {
        case WeaponKind::Shotgun:
            return {k, "Shotgun", 0.65f, 520.0f, 5, 24.0f, 1, 6};
        case WeaponKind::MachineGun:
            return {k, "Machine Gun", 0.09f, 720.0f, 1, 6.0f, 1, 30};
        case WeaponKind::Pistol:
        default:
            return {k, "Pistol", 0.22f, 600.0f, 1, 0.0f, 1, -1};
    }
}

}  // namespace bd
