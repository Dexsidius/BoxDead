#pragma once

#include "boxdead/entity.hpp"
#include "boxdead/enemy.hpp"
#include "boxdead/font.hpp"
#include "boxdead/menu.hpp"
#include "boxdead/player.hpp"
#include "boxdead/projectile.hpp"
#include "boxdead/texture.hpp"
#include "boxdead/weapon.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

namespace bd {

// High-level game state. The run loop dispatches input/update/render based
// on the current state, so the menu, options, and gameplay never overlap.
enum class GameState { MainMenu, Options, Playing, GameOver };

class Game {
public:
    explicit Game(bool smoke_test = false, std::string screenshot_path = "");
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    bool init();
    void run();
    void shutdown();

    // Called by Item::on_pickup implementations to show a toast message.
    void show_toast(const std::string& text);

private:
    // Loop phases (dispatched by `state_`).
    void process_input(bool& running);
    void update(float dt);
    void render();
    void render_hud();

    // Gameplay systems.
    void spawn_enemy(const GameContext& ctx);
    void fire_projectile(float dt, const GameContext& ctx);
    void capture_screenshot();
    void check_collisions();
    void check_item_pickups();
    void maybe_drop_item(Vec2 pos);
    void spawn_floor_item(const GameContext& ctx);
    void spawn_item_on_player();
    void reset();

    // Menu / state transitions.
    void on_main_menu_select(int index, bool& running);
    void on_options_select(int index);
    void enter_options();
    void return_to_menu();
    std::string difficulty_label();
    std::string aim_label();
    float difficulty_factor();

    // Picks the pickup texture for a given weapon kind.
    Texture* weapon_pickup_tex(WeaponKind k);

    // Combat / i-frame state.
    float invuln_timer_ = 0.0f;
    bool game_over_ = false;

    // Score + progression.
    int score_ = 0;
    int wave_ = 1;
    float wave_timer_ = 0.0f;
    float wave_duration_ = 15.0f;
    int difficulty_ = 1;  // 0 Easy, 1 Normal, 2 Hard
    AimMode aim_mode_ = AimMode::FaceMouse;  // FaceMouse or FaceMovement

    // Computes the player's aim direction this frame (used by both the gun
    // render and firing, so the visual and bullet dir never diverge).
    void update_player_aim(const GameContext& ctx);
    float item_spawn_timer_ = 0.0f;
    int pickups_collected_ = 0;
    int smoke_frame_ = 0;
    int max_enemy_anim_frame_ = -1;  // highest enemy anim frame seen this run
    std::string pickup_toast_;
    float pickup_toast_timer_ = 0.0f;

    // Top-level state + UI.
    GameState state_ = GameState::MainMenu;
    Menu menu_;
    Font font_;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<Texture> player_walk_sheet_;
    std::unique_ptr<Texture> player_idle_sheet_;
    std::unique_ptr<Texture> zombie_walk_sheet_;
    std::unique_ptr<Texture> devil_walk_sheet_;
    std::unique_ptr<Texture> projectile_tex_;
    std::unique_ptr<Texture> health_tex_;
    std::unique_ptr<Texture> weapon_tex_pistol_;
    std::unique_ptr<Texture> weapon_tex_shotgun_;
    std::unique_ptr<Texture> weapon_tex_machinegun_;
    std::unique_ptr<Texture> gun_hand_tex_[3];  // in-hand gun sprite per weapon kind
    void apply_gun_textures(Player& p);  // hand the gun sprites to a player
    std::vector<std::unique_ptr<Entity>> entities_;
    Player* player_ = nullptr;
    float spawn_timer_ = 0.0f;
    float fire_cooldown_ = 0.0f;
    bool smoke_test_;
    bool screenshot_mode_ = false;
    std::string screenshot_path_;
};

}  // namespace bd
