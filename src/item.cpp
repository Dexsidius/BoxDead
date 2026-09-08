// BoxDead - Item implementation
#include "boxdead/item.hpp"

#include "boxdead/game.hpp"
#include "boxdead/player.hpp"

#include <algorithm>
#include <string>

namespace bd {

namespace {
constexpr int kPlayerMaxHp = 5;
}

HealthPickup::HealthPickup(float x, float y) : Item(x, y, 22.0f, 22.0f) {}

void HealthPickup::on_pickup(Game& game, Player& player) {
    player.health = std::min(player.health + 2, kPlayerMaxHp);
    game.show_toast("+2 HP");
}

WeaponPickup::WeaponPickup(float x, float y, WeaponKind kind, int ammo)
    : Item(x, y, 22.0f, 22.0f), kind_(kind), ammo_(ammo) {}

void WeaponPickup::on_pickup(Game& game, Player& player) {
    player.acquire_weapon(kind_, ammo_);
    game.show_toast(std::string(weapon_spec(kind_).name) + " acquired");
}

}  // namespace bd
