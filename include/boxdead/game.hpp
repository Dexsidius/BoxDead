#pragma once

#include "boxdead/barrel.hpp"
#include "boxdead/entity.hpp"
#include "boxdead/enemy.hpp"
#include "boxdead/font.hpp"
#include "boxdead/iso_sprite.hpp"
#include "boxdead/menu.hpp"
#include "boxdead/player.hpp"
#include "boxdead/projectile.hpp"
#include "boxdead/texture.hpp"
#include "boxdead/tilemap.hpp"
#include "boxdead/weapon.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

namespace bd {

// Logical render size (the "camera" viewport). The window can be resized or
// fullscreened; SDL scales this fixed view to the window with letterboxing, so
// gameplay coordinates stay stable regardless of window size.
inline constexpr int kViewWidth = 1280;
inline constexpr int kViewHeight = 720;

// A scrollable camera. Worlds (maps) may be larger than the viewport; the
// camera follows the player and clamps to the world bounds. Everything world-
// space is drawn at (world - camera) to land on screen.
struct Camera {
    float x = 0.0f;
    float y = 0.0f;
};

// High-level game state. The run loop dispatches input/update/render based
// on the current state, so the menu, options, and gameplay never overlap.
enum class GameState { MainMenu, CharacterSelect, Options, Playing, GameOver };

class Game {
public:
    // `smoke_frames` overrides the length of a --smoke-test run (0 = default).
    // Waves are cleared rather than timed, so reaching the boss wave takes a
    // few thousand frames; short runs only cover the first couple of waves.
    explicit Game(bool smoke_test = false, std::string screenshot_path = "",
                 bool menu_shot = false, int smoke_frames = 0,
                 int start_level = 0);
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
    // Pick a spawn position along the current view edge that is clear of walls
    // and barrels. Shared by the horde and the boss.
    void pick_spawn_point(const GameContext& ctx, float& out_x, float& out_y);

    // --- Bosses ------------------------------------------------------------
    // Every 5th wave, each spawn has a 1-in-kDemonOdds chance of being a
    // yellow demon instead of an ordinary enemy.
    static const int kDemonWaveInterval = 5;
    static const int kDemonOdds = 5;
    // The end-of-level boss spawns on every kWavesPerLevel-th wave and holds
    // the scene transition until it is dead.
    void spawn_boss(const GameContext& ctx);
    // The living boss, or nullptr. Scanned rather than cached so there is no
    // pointer to dangle when the boss dies and is reaped.
    Enemy* find_boss() const;
    // Unique boss name for a scene (cycles with the levels).
    static const char* boss_name(int level);
    // Dark-Souls-style name + health bar across the bottom of the screen.
    void render_boss_bar();
    void fire_projectile(float dt, const GameContext& ctx);
    void capture_screenshot();
    void capture_screenshot_to(const std::string& path);
    void check_collisions();

    // --- Explosive barrels -------------------------------------------------
    // Barrels come from the level tileset (tiles named "Barrel"/"Explosive").
    // They block movement, take bullet damage, and detonate on a short fuse.
    void spawn_barrels_from_map();
    // Refresh the dynamic-obstacle list handed to entities each frame so the
    // player and enemies collide with living barrels.
    void rebuild_obstacles();
    // Detonate any barrel whose fuse burned out this frame.
    void update_barrels();
    // Apply one blast at `pos`: damages enemies and the player inside the
    // radius, lights the fuse on other barrels (chain reaction), and spawns
    // the fireball. Never called while iterating entities_.
    void detonate(Vec2 pos);
    std::vector<Obstacle> obstacles_;  // living barrels, rebuilt each frame

    void check_item_pickups();
    void maybe_drop_item(Vec2 pos);
    void spawn_floor_item(const GameContext& ctx);
    void spawn_item_on_player();
    void reset();

    // Scene / level system. Every 15 waves the world transitions to the next
    // map (fade to black, reset positions, swap floor palette + banner).
    struct Level {
        const char* name;
        const char* map_path;  // LevelEdit++ ".mx" tileset to load for this scene
        SDL_Color floor;      // fallback floor color if the map fails to load
        SDL_Color grid;
    };
    static const Level kLevels[];
    static const int kLevelCount;
    // Waves per scene. The last wave of each block is the boss wave.
    static const int kWavesPerLevel = 13;
    static constexpr float kTransitionDur = 1.2f;  // seconds (fade out + in)
    int level_ = 0;
    float transition_timer_ = 0.0f;   // >0 while a level transition is playing
    int pending_level_ = -1;         // level to swap to at the fade midpoint
    float banner_timer_ = 0.0f;      // shows the level name briefly after a swap
    const Level& current_level() const;
    int level_for_wave(int wave) const;
    void begin_level_transition(int new_level);
    void apply_level_swap(const GameContext& ctx);
    // Loads the current scene's ".mx" tileset into tilemap_. Returns false
    // (and leaves the floor as a solid color) if the file is missing/broken.
    void load_current_tilemap();

    // Menu / state transitions.
    void on_main_menu_select(int index, bool& running);
    void on_character_select(int index);
    void enter_character_select();
    void on_options_select(int index);
    void enter_options();
    void return_to_menu();
    std::string difficulty_label();
    std::string aim_label();
    float difficulty_factor();

    // Picks the pickup texture for a given weapon kind.
    Texture* weapon_pickup_tex(WeaponKind k);

    // Paused with P during play: the world stops updating and a dim overlay
    // goes up, but the scene stays drawn behind it.
    bool paused_ = false;

    // Combat / i-frame state.
    float invuln_timer_ = 0.0f;
    bool game_over_ = false;

    // Score + progression.
    int score_ = 0;
    // Waves are cleared, not timed: a wave spawns a fixed roster and the next
    // one does not begin until every enemy from it is dead.
    int wave_ = 1;
    int wave_spawns_left_ = 0;    // still to spawn from this wave's roster
    float wave_break_timer_ = 0.0f;   // breather between a clear and the next wave
    float wave_banner_timer_ = 0.0f;  // "WAVE N" announcement
    static constexpr float kWaveBreak = 2.5f;   // seconds between waves
    static constexpr int kWaveBaseEnemies = 5;  // roster size on wave 1
    static constexpr int kWaveEnemyStep = 2;    // added per wave
    static constexpr int kWaveEnemyCap = 40;    // roster ceiling
    // Roster size for a wave (the boss on a boss wave is extra).
    static int enemies_for_wave(int wave);
    // Start `wave`: set the roster, announce it, spawn the boss if it is a
    // boss wave.
    void begin_wave(int wave, const GameContext& ctx);
    // Enemies currently on the field.
    int living_enemies() const;
    // Enemies from this wave still to come (unspawned + alive).
    int wave_enemies_left() const;
    int difficulty_ = 1;  // 0 Easy, 1 Normal, 2 Hard
    AimMode aim_mode_ = AimMode::FaceMouse;  // FaceMouse or FaceMovement
    // Selected character skin (0=Blue, 1=Green, 2=Red). Chosen on the
    // character-select screen before a New Game starts.
    int selected_character_ = 0;
    // Palettes for the playable characters (preview + applied to the player).
    static IsoCharStyle character_style(int index);
    static const char* character_name(int index);

    // Computes the player's aim direction this frame (used by both the gun
    // render and firing, so the visual and bullet dir never diverge).
    void update_player_aim(const GameContext& ctx);
    float item_spawn_timer_ = 0.0f;
    int pickups_collected_ = 0;
    int smoke_frame_ = 0;
    int max_enemy_anim_frame_ = -1;  // highest enemy anim frame seen this run
    int barrels_exploded_ = 0;       // barrels detonated this run (smoke metric)
    int blast_kills_ = 0;            // enemies killed by explosions (smoke metric)
    int demons_spawned_ = 0;         // yellow demons spawned (smoke metric)
    int bosses_spawned_ = 0;         // level bosses spawned (smoke metric)
    // Times a wave began while enemies from the previous one were still alive.
    // Must stay 0: that is the whole point of clearing waves rather than
    // timing them, so the smoke summary reports it as a regression check.
    int wave_gate_violations_ = 0;
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
    std::unique_ptr<Texture> fireball_tex_;  // devil ranged attack sprite
    std::unique_ptr<Texture> health_tex_;
    std::unique_ptr<Texture> weapon_tex_pistol_;
    std::unique_ptr<Texture> weapon_tex_shotgun_;
    std::unique_ptr<Texture> weapon_tex_machinegun_;
    std::unique_ptr<Texture> gun_hand_tex_[3];  // in-hand gun sprite per weapon kind
    void apply_gun_textures(Player& p);  // hand the gun sprites to a player
    // Scene tilemap (LevelEdit++ ".mx" tileset) for the current level.
    Tilemap tilemap_;
    // World size for the current level (from the tilemap bounds, or the view
    // size if no map loaded). Decoupled from the window so maps can be larger
    // than the screen and scroll under the camera.
    float world_w_ = static_cast<float>(kViewWidth);
    float world_h_ = static_cast<float>(kViewHeight);
    Camera camera_;
    void update_camera();  // follow the player, clamp to world bounds
    // True if no solid tile blocks the line from `a` to `b` (devil sight line).
    bool line_of_solid_clear(float ax, float ay, float bx, float by) const;
    // Every shooting kind (devil, yellow demon, boss) fires at the player
    // when in range with a clear line of sight.
    void update_enemy_ranged_attacks(float dt, const GameContext& ctx);
    std::vector<std::unique_ptr<Entity>> entities_;
    Player* player_ = nullptr;
    float spawn_timer_ = 0.0f;
    float fire_cooldown_ = 0.0f;
    bool smoke_test_;
    int smoke_frames_ = 0;
    // Scene the headless/screenshot modes open on (--level N), so each theme
    // can be captured without playing through the wave ladder to reach it.
    int start_level_ = 0;
    bool screenshot_mode_ = false;
    bool menu_shot_ = false;
    std::string screenshot_path_;
    // When set, render() captures the framebuffer to this path RIGHT BEFORE
    // SDL_RenderPresent (post-present readback is unreliable/blank on some
    // drivers, so we read the back buffer while it still holds the frame).
    std::string pending_capture_;
};

}  // namespace bd
