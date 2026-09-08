// Unit test for Player weapon state (equip / consume / auto-revert).
// No SDL runtime needed: Player construction and ammo logic are pure.
#include "boxdead/player.hpp"
#include "boxdead/weapon.hpp"

#include <cassert>
#include <cstdio>

int main() {
    bd::Player p(100.0f, 100.0f);

    // Defaults: pistol with infinite ammo.
    assert(p.weapon_name() == "Pistol");
    assert(p.weapon_ammo() == -1);
    assert(p.current_weapon().cooldown == 0.22f);
    assert(p.consume_ammo());  // infinite -> always fires

    // Equip shotgun (6 rounds) and empty it; should revert to pistol.
    p.equip_weapon(bd::WeaponKind::Shotgun, 6);
    assert(p.weapon_name() == "Shotgun");
    assert(p.weapon_ammo() == 6);
    for (int i = 0; i < 6; ++i) assert(p.consume_ammo());
    assert(p.weapon_name() == "Pistol");
    assert(p.weapon_ammo() == -1);

    // Machine gun (30 rounds): emptying reverts to pistol too.
    p.equip_weapon(bd::WeaponKind::MachineGun, 30);
    assert(p.weapon_ammo() == 30);
    for (int i = 0; i < 30; ++i) assert(p.consume_ammo());
    assert(p.weapon_name() == "Pistol");
    assert(p.weapon_ammo() == -1);

    std::printf("weapon logic OK\n");
    return 0;
}
