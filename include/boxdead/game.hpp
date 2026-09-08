// BoxDead - Game: owns the window, renderer, entities, and main loop
#pragma once

#include "boxdead/entity.hpp"
#include "boxdead/enemy.hpp"
#include "boxdead/player.hpp"
#include "boxdead/projectile.hpp"
#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

namespace bd {

class Game {
public:
    explicit Game(bool smoke_test = false);
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Create window, renderer, sprite textures, and the player.
    bool init();

    // Run the main loop until the window closes or Esc is pressed.
    void run();

    // Release window/renderer resources.
    void shutdown();

private:
    void handle_events(bool& running);
    void update(float dt);
    void render();
    void spawn_enemy(const GameContext& ctx);
    void fire_projectile(float dt, const GameContext& ctx);
    void check_collisions();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<Texture> player_tex_;
    std::unique_ptr<Texture> enemy_tex_;
    std::unique_ptr<Texture> projectile_tex_;
    std::vector<std::unique_ptr<Entity>> entities_;
    Player* player_ = nullptr;  // non-owning; lives in entities_
    float spawn_timer_ = 0.0f;
    float fire_cooldown_ = 0.0f;
    bool smoke_test_;
};

}  // namespace bd
