// Unit test for Player weapon inventory (acquire / switch / consume / auto-fallback).
// No SDL runtime needed: Player construction and ammo logic are pure.
#include "boxdead/player.hpp"
#include "boxdead/weapon.hpp"

#include <cassert>
#include <cstdio>

int main() {
    bd::Player p(100.0f, 100.0f);

    // Defaults: pistol only, owned, infinite ammo.
    assert(p.weapon_name() == "Pistol");
    assert(p.weapon_ammo() == -1);
    assert(p.owns(bd::WeaponKind::Pistol));
    assert(!p.owns(bd::WeaponKind::Shotgun));
    assert(p.current_weapon().cooldown == 0.22f);
    assert(p.consume_ammo());  // infinite -> always fires

    // Acquire shotgun (6 rounds): added to inventory and selected.
    p.acquire_weapon(bd::WeaponKind::Shotgun, 6);
    assert(p.owns(bd::WeaponKind::Shotgun));
    assert(p.weapon_name() == "Shotgun");
    assert(p.weapon_ammo() == 6);

    // Switch back to pistol, then back to shotgun; ammo persists.
    p.switch_to(bd::WeaponKind::Pistol);
    assert(p.weapon_name() == "Pistol");
    p.switch_to(bd::WeaponKind::Shotgun);
    assert(p.weapon_name() == "Shotgun");
    assert(p.weapon_ammo() == 6);

    // Empty the shotgun: stays owned at 0, auto-falls back to pistol.
    for (int i = 0; i < 6; ++i) assert(p.consume_ammo());
    assert(p.weapon_name() == "Pistol");
    assert(p.owns(bd::WeaponKind::Shotgun));
    assert(p.ammo(bd::WeaponKind::Shotgun) == 0);

    // Re-acquiring the shotgun refills its ammo.
    p.acquire_weapon(bd::WeaponKind::Shotgun, 6);
    assert(p.ammo(bd::WeaponKind::Shotgun) == 6);

    // Cycling wraps around owned weapons.
    p.switch_to(bd::WeaponKind::Shotgun);
    p.cycle(1);
    assert(p.current_kind() == bd::WeaponKind::Pistol);

    // Machine gun (30 rounds): emptying reverts to pistol too.
    p.acquire_weapon(bd::WeaponKind::MachineGun, 30);
    assert(p.weapon_ammo() == 30);
    for (int i = 0; i < 30; ++i) assert(p.consume_ammo());
    assert(p.weapon_name() == "Pistol");

    std::printf("weapon inventory OK\n");
    return 0;
}
