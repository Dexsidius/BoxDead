// BoxDead - Game implementation: owns the main loop, menu, spawning, firing,
// collisions, item pickups, and rendering. main.cpp is a thin bootstrap.
#define _USE_MATH_DEFINES
#include "boxdead/game.hpp"

#include "boxdead/item.hpp"
#include "boxdead/sprite.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace bd {

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "BoxDead";
constexpr float kSpawnInterval = 1.2f;    // seconds between enemy spawns
constexpr size_t kMaxEnemies = 50;
constexpr float kFireInterval = 0.18f;    // fallback cooldown (unused now)
constexpr float kProjectileSpeed = 600.0f;
constexpr float kInvulnDuration = 1.2f;   // seconds of i-frames after a hit
constexpr int kPlayerMaxHp = 5;
constexpr float kItemSpawnInterval = 12.0f;  // seconds between floor item spawns
constexpr int kMaxFloorItems = 3;

const std::vector<std::string> kMainMenuItems = {"New Game", "Options", "Exit"};
}  // namespace

// Scene definitions: each map loads a LevelEdit++ ".mx" tileset and has a
// fallback floor color if the tileset is missing. Levels cycle (wrap around)
// so the game always has somewhere to go next.
const Game::Level Game::kLevels[] = {
    {"The Courtyard",  "assets/maps/Courtyard/Courtyard.mx",  {18, 18, 24, 255},  {40, 44, 54, 255}},
    {"The Asylum",     "assets/maps/Asylum/Asylum.mx",        {24, 18, 18, 255},  {58, 40, 40, 255}},
    {"The Sewers",     "assets/maps/Sewers/Sewers.mx",       {14, 22, 20, 255},  {36, 54, 48, 255}},
    {"The Graveyard",  "assets/maps/Graveyard/Graveyard.mx", {20, 20, 28, 255},  {48, 46, 64, 255}},
    {"Hell's Gate",     "assets/maps/Hells Gate/Hells Gate.mx",{28, 14, 14, 255},  {70, 30, 30, 255}},
};
const int Game::kLevelCount =
    sizeof(Game::kLevels) / sizeof(Game::kLevels[0]);

Game::Game(bool smoke_test, std::string screenshot_path)
    : smoke_test_(smoke_test),
      screenshot_mode_(!screenshot_path.empty()),
      screenshot_path_(std::move(screenshot_path)) {}

Game::~Game() { shutdown(); }

bool Game::init() {
    window_ = SDL_CreateWindow(kWindowTitle, kWindowWidth, kWindowHeight,
                               SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        return false;
    }

    player_walk_sheet_ = make_walk_sheet_texture(
        renderer_, SDL_Color{60, 160, 255, 255}, 4, 32, true);
    player_idle_sheet_ = make_walk_sheet_texture(
        renderer_, SDL_Color{60, 160, 255, 255}, 2, 32, false);

    // Boxhead-style zombie: pale green skin, dark hair, white shirt with a
    // red tie, blood splatter.
    CreaturePalette zombie_pal;
    zombie_pal.skin = SDL_Color{158, 194, 90, 255};
    zombie_pal.hair = SDL_Color{51, 51, 51, 255};
    zombie_pal.torso = SDL_Color{232, 232, 232, 255};
    zombie_pal.tie = SDL_Color{198, 40, 40, 255};
    zombie_pal.pants = SDL_Color{58, 58, 58, 255};
    zombie_pal.blood = true;
    zombie_walk_sheet_ = make_creature_sheet_texture(
        renderer_, zombie_pal, 4, 40, true);

    // Boxhead-style red devil: red skin, dark red torso, two horns. Inherits
    // the zombie's behavior but is tougher (2 HP).
    CreaturePalette devil_pal;
    devil_pal.skin = SDL_Color{211, 47, 47, 255};
    devil_pal.torso = SDL_Color{139, 26, 26, 255};
    devil_pal.pants = SDL_Color{74, 16, 16, 255};
    devil_pal.horn = SDL_Color{26, 16, 16, 255};
    devil_walk_sheet_ = make_creature_sheet_texture(
        renderer_, devil_pal, 4, 40, true);

    projectile_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{255, 220, 60, 255}, 8);
    health_tex_ = make_cross_sprite_texture(
        renderer_, SDL_Color{40, 170, 70, 255}, SDL_Color{255, 255, 255, 255},
        22);
    weapon_tex_pistol_ = make_solid_sprite_texture(
        renderer_, SDL_Color{200, 200, 210, 255}, 22);
    weapon_tex_shotgun_ = make_solid_sprite_texture(
        renderer_, SDL_Color{230, 140, 40, 255}, 22);
    weapon_tex_machinegun_ = make_solid_sprite_texture(
        renderer_, SDL_Color{70, 200, 230, 255}, 22);

    // In-hand gun sprites (side view, muzzle +x); one per weapon kind.
    gun_hand_tex_[0] = make_gun_texture(renderer_, WeaponKind::Pistol);
    gun_hand_tex_[1] = make_gun_texture(renderer_, WeaponKind::Shotgun);
    gun_hand_tex_[2] = make_gun_texture(renderer_, WeaponKind::MachineGun);

    if (!TTF_Init()) {
        std::cerr << "TTF_Init failed: " << SDL_GetError() << '\n';
        return false;
    }
    // assets/dejavu-sans.ttf is resolved relative to the executable. Try a
    // few candidate locations so it works from the build dir and a deployed
    // layout next to the executable.
    const char* base = SDL_GetBasePath();
    const std::string base_dir = base ? std::string(base) : std::string("");
    if (base) SDL_free(const_cast<char*>(base));
    const std::vector<std::string> font_candidates = {
        base_dir + "assets/dejavu-sans.ttf",
        base_dir + "../assets/dejavu-sans.ttf",
        base_dir + "../../assets/dejavu-sans.ttf",
        "assets/dejavu-sans.ttf",
        "../assets/dejavu-sans.ttf",
    };
    bool font_ok = false;
    std::string font_path;
    for (const std::string& p : font_candidates) {
        if (font_.load(renderer_, p, 24)) {
            font_ok = true;
            font_path = p;
            break;
        }
    }
    if (!font_ok) {
        std::cerr << "Font load failed (tried " << font_candidates.size()
                  << " paths under " << base_dir << ")\n";
        return false;
    }

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    menu_.set_items(kMainMenuItems);
    state_ = (smoke_test_ || screenshot_mode_) ? GameState::Playing : GameState::MainMenu;

    auto player =
        std::make_unique<Player>(kWindowWidth * 0.5f, kWindowHeight * 0.5f);
    player->add_animation("walk", player_walk_sheet_.get(), 4, 32, 0.12f, true);
    player->add_animation("idle", player_idle_sheet_.get(), 2, 32, 0.28f, true);
    player->play_animation("idle");
    player_ = player.get();
    apply_gun_textures(*player_);
    if (smoke_test_ || screenshot_mode_) player_->health = 1000;  // survive the whole smoke run
    entities_.push_back(std::move(player));
    // Smoke/screenshot skip the main menu, so they never go through reset();
    // load the first scene's tileset here so the floor renders and collision
    // is exercised from the first frame.
    if (smoke_test_ || screenshot_mode_) load_current_tilemap();
    return true;
}

void Game::run() {
    if (smoke_test_ || screenshot_mode_) {
        // Drive a short fixed-step scenario so combat, enemy drops, and item
        // pickups are all exercised. In screenshot mode this runs under a real
        // renderer (e.g. Xvfb) so the framebuffer can be captured.
        bool running = true;
        const float dt = 1.0f / 60.0f;
        const int frames = screenshot_mode_ ? 220 : 900;
        for (int i = 0; i < frames && running; ++i) {
            process_input(running);
            update(dt);
            render();
            // For screenshot verification: force a scene transition at frame
            // 100 so the fade + map swap + banner can be captured (the real
            // game triggers this every 15 waves automatically).
            if (screenshot_mode_ && i == 100) {
                begin_level_transition((level_ + 1) % kLevelCount);
            }
            // Capture a few frames in screenshot mode so at least one lands
            // with live enemies on screen (they spawn and die quickly).
            if (screenshot_mode_ && (i == 90 || i == 110 || i == 160 || i == 185)) {
                const std::string p =
                    screenshot_path_ + "." + std::to_string(i) + ".bmp";
                capture_screenshot_to(p);
            }
        }
        if (screenshot_mode_) {
            capture_screenshot();
        } else {
            std::cerr << "smoke: kills=" << score_ << " wave=" << wave_
                      << " hp=" << player_->health
                      << " weapon=" << player_->weapon_name()
                      << " pickups=" << pickups_collected_;
            // Report the highest enemy animation frame seen during the run to
            // prove the animator ticked (robust even if all enemies die late).
            std::cerr << " enemy_frame=" << max_enemy_anim_frame_ << '\n';
        }
        return;
    }

    Uint64 last_time = SDL_GetTicksNS();
    bool running = true;
    while (running) {
        const Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - last_time) / 1.0e9f;
        last_time = now;
        if (dt > 0.25f) dt = 0.25f;  // avoid spiral of death after stalls

        process_input(running);
        update(dt);
        render();

        SDL_Delay(1);  // yield CPU
    }
}

void Game::process_input(bool& running) {
    int win_w = kWindowWidth;
    int win_h = kWindowHeight;
    SDL_GetWindowSize(window_, &win_w, &win_h);
    const float ww = static_cast<float>(win_w);
    const float wh = static_cast<float>(win_h);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
            return;
        }
        switch (state_) {
            case GameState::MainMenu: {
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    running = false;
                    break;
                }
                int c = menu_.handle_event(event, ww, wh);
                if (c >= 0) on_main_menu_select(c, running);
                break;
            }
            case GameState::Options: {
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    return_to_menu();
                    break;
                }
                int c = menu_.handle_event(event, ww, wh);
                if (c >= 0) on_options_select(c);
                break;
            }
            case GameState::Playing:
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    return_to_menu();
                    break;
                }
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    const SDL_Keycode k = event.key.key;
                    if (k == SDLK_1) player_->switch_to(WeaponKind::Pistol);
                    else if (k == SDLK_2) player_->switch_to(WeaponKind::Shotgun);
                    else if (k == SDLK_3) player_->switch_to(WeaponKind::MachineGun);
                    else if (k == SDLK_Q) player_->cycle(-1);
                    else if (k == SDLK_E) player_->cycle(1);
                }
                break;
            case GameState::GameOver:
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_ESCAPE) {
                        return_to_menu();
                    } else if (event.key.key == SDLK_R) {
                        reset();
                    }
                }
                break;
        }
    }
}

void Game::on_main_menu_select(int index, bool& running) {
    if (index == 0) {
        reset();  // New Game
    } else if (index == 1) {
        enter_options();
    } else if (index == 2) {
        running = false;  // Exit
    }
}

void Game::enter_options() {
    state_ = GameState::Options;
    menu_.set_items({"Difficulty: " + difficulty_label(),
                     "Aim: " + aim_label(),
                     "Back"});
    menu_.reset();
}

void Game::on_options_select(int index) {
    if (index == 0) {
        difficulty_ = (difficulty_ + 1) % 3;  // cycle Easy/Normal/Hard
    } else if (index == 1) {
        aim_mode_ = (aim_mode_ == AimMode::FaceMouse ? AimMode::FaceMovement
                                                    : AimMode::FaceMouse);
    } else if (index == 2) {
        return_to_menu();
    }
    if (index == 0 || index == 1) {
        menu_.set_items({"Difficulty: " + difficulty_label(),
                         "Aim: " + aim_label(),
                         "Back"});
    }
}

void Game::return_to_menu() {
    state_ = GameState::MainMenu;
    menu_.set_items(kMainMenuItems);
    menu_.reset();
}

std::string Game::difficulty_label() {
    switch (difficulty_) {
        case 0: return "Easy";
        case 2: return "Hard";
        default: return "Normal";
    }
}

std::string Game::aim_label() {
    return aim_mode_ == AimMode::FaceMouse ? "Mouse" : "Movement";
}

float Game::difficulty_factor() {
    switch (difficulty_) {
        case 0: return 1.4f;  // Easy: slower spawns
        case 2: return 0.7f;  // Hard: faster spawns
        default: return 1.0f;
    }
}

void Game::update_player_aim(const GameContext& ctx) {
    float dx = 0.0f;
    float dy = -1.0f;
    if (smoke_test_ || screenshot_mode_) {
        bool found = false;
        float best = 1e30f;
        for (const auto& e : entities_) {
            auto* en = dynamic_cast<Enemy*>(e.get());
            if (!en || !en->alive) continue;
            const float ax = en->pos.x - player_->pos.x;
            const float ay = en->pos.y - player_->pos.y;
            const float dd = ax * ax + ay * ay;
            if (dd < best) {
                best = dd;
                dx = ax;
                dy = ay;
                found = true;
            }
        }
        const float l = std::sqrt(dx * dx + dy * dy);
        if (found && l > 0.001f) {
            dx /= l;
            dy /= l;
        } else {
            dx = 0.0f;
            dy = -1.0f;
        }
    } else if (aim_mode_ == AimMode::FaceMovement) {
        // Aim where the player is heading (last non-zero movement dir).
        dx = player_->last_move_dir().x;
        dy = player_->last_move_dir().y;
        const float l = std::sqrt(dx * dx + dy * dy);
        if (l < 0.001f) {
            dx = 0.0f;
            dy = -1.0f;
        } else {
            dx /= l;
            dy /= l;
        }
    } else {
        // FaceMouse: aim from the player toward the cursor.
        int win_w = kWindowWidth;
        int win_h = kWindowHeight;
        SDL_GetWindowSize(window_, &win_w, &win_h);
        float mx = 0.0f;
        float my = 0.0f;
        SDL_GetMouseState(&mx, &my);
        const float sx = ctx.world_w / static_cast<float>(win_w);
        const float sy = ctx.world_h / static_cast<float>(win_h);
        dx = mx * sx - player_->pos.x;
        dy = my * sy - player_->pos.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) {
            dx = 0.0f;
            dy = -1.0f;
        } else {
            dx /= len;
            dy /= len;
        }
    }
    player_->set_facing(dx, dy);
}

void Game::update(float dt) {
    if (state_ != GameState::Playing) return;

    int draw_w = kWindowWidth;
    int draw_h = kWindowHeight;
    SDL_GetCurrentRenderOutputSize(renderer_, &draw_w, &draw_h);

    GameContext ctx;
    ctx.keys = SDL_GetKeyboardState(nullptr);
    ctx.world_w = static_cast<float>(draw_w);
    ctx.world_h = static_cast<float>(draw_h);
    ctx.tilemap = &tilemap_;

    // Update the player first so enemies can chase its new position.
    player_->update(dt, ctx);
    ctx.player_pos = player_->pos;

    // Compute the player's aim direction once: the gun render and the firing
    // path both read this stored vector, so they can never diverge.
    update_player_aim(ctx);

    // When the player is dead, freeze the world (no spawns, movement, etc.).
    if (game_over_) return;

    // Tick a level transition (fade to black, swap map at the midpoint, fade
    // back in). While fading we hold spawns so the new scene starts clean.
    const bool transitioning = transition_timer_ > 0.0f;
    if (transitioning) {
        const float before = transition_timer_;
        transition_timer_ = std::max(0.0f, transition_timer_ - dt);
        // Swap at the midpoint (when crossing below half the duration).
        if (pending_level_ >= 0 &&
            before > kTransitionDur * 0.5f &&
            transition_timer_ <= kTransitionDur * 0.5f) {
            apply_level_swap(ctx);
        }
    }
    if (banner_timer_ > 0.0f) banner_timer_ = std::max(0.0f, banner_timer_ - dt);

    // Advance waves: each wave raises the spawn rate (shorter interval).
    wave_timer_ += dt;
    if (wave_timer_ >= wave_duration_) {
        wave_timer_ = 0.0f;
        ++wave_;
        // Every kWavesPerLevel waves, transition to the next map/scene.
        const int new_level = level_for_wave(wave_);
        if (new_level != level_ && !transitioning) {
            begin_level_transition(new_level);
        }
    }

    // Spawn enemies on a timer (rate scales with wave + difficulty).
    const float spawn_interval =
        std::max(0.35f, (kSpawnInterval -
                         0.08f * static_cast<float>(wave_ - 1)) *
                        difficulty_factor());
    spawn_timer_ += dt;
    if (spawn_timer_ >= spawn_interval && !transitioning) {
        spawn_timer_ = 0.0f;
        size_t enemy_count = 0;
        for (const auto& e : entities_)
            if (dynamic_cast<Enemy*>(e.get())) ++enemy_count;
        if (enemy_count < kMaxEnemies) spawn_enemy(ctx);
    }

    fire_projectile(dt, ctx);

    // Update everything except the player (already updated).
    for (auto& e : entities_) {
        if (e.get() != player_) e->update(dt, ctx);
    }

    // In smoke, record the highest enemy animation frame seen so the summary
    // proves the animator ticked even if every enemy dies before the run ends.
    if (smoke_test_) {
        for (const auto& e : entities_) {
            auto* en = dynamic_cast<Enemy*>(e.get());
            if (en && en->alive) {
                const int f = static_cast<int>(en->animator_frame_index());
                if (f > max_enemy_anim_frame_) max_enemy_anim_frame_ = f;
            }
        }
    }

    // Tick the player's post-hit invulnerability window.
    if (invuln_timer_ > 0.0f) {
        invuln_timer_ -= dt;
        if (invuln_timer_ < 0.0f) invuln_timer_ = 0.0f;
    }

    check_collisions();
    check_item_pickups();

    // In smoke, force items onto the player periodically so the pickup path
    // (health + weapons) is exercised even though the player never moves.
    if (smoke_test_ || screenshot_mode_) {
        ++smoke_frame_;
        if (smoke_frame_ % 90 == 0) spawn_item_on_player();
    }

    // Occasionally drop a fresh item on the floor.
    item_spawn_timer_ += dt;
    if (item_spawn_timer_ >= kItemSpawnInterval) {
        item_spawn_timer_ = 0.0f;
        spawn_floor_item(ctx);
    }

    // Fade the pickup toast.
    if (pickup_toast_timer_ > 0.0f) {
        pickup_toast_timer_ -= dt;
        if (pickup_toast_timer_ < 0.0f) pickup_toast_timer_ = 0.0f;
    }

    // Reap dead entities (projectiles that hit/expired, dead enemies, used
    // items).
    entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                   [](const std::unique_ptr<Entity>& e) {
                                       return !e->alive;
                                   }),
                    entities_.end());

    if (game_over_) state_ = GameState::GameOver;
}

void Game::spawn_enemy(const GameContext& ctx) {
    const int edge = std::rand() % 4;
    const float margin = 20.0f;
    float x = 0.0f;
    float y = 0.0f;
    switch (edge) {
        case 0:  // top
            x = static_cast<float>(std::rand() % static_cast<int>(ctx.world_w));
            y = margin;
            break;
        case 1:  // bottom
            x = static_cast<float>(std::rand() % static_cast<int>(ctx.world_w));
            y = ctx.world_h - margin;
            break;
        case 2:  // left
            x = margin;
            y = static_cast<float>(std::rand() % static_cast<int>(ctx.world_h));
            break;
        default:  // right
            x = ctx.world_w - margin;
            y = static_cast<float>(std::rand() % static_cast<int>(ctx.world_h));
            break;
    }
    // Pick the enemy kind: zombies always; the tougher red Devil appears
    // from wave 2 onward (25% chance) as a special enemy.
    const EnemyKind kind =
        (wave_ >= 2 && (std::rand() % 4 == 0)) ? EnemyKind::Devil
                                              : EnemyKind::Zombie;
    auto e = std::make_unique<Enemy>(kind, x, y);
    const Texture* sheet = (kind == EnemyKind::Devil) ? devil_walk_sheet_.get()
                                                       : zombie_walk_sheet_.get();
    e->add_animation("walk", const_cast<Texture*>(sheet), 4, 40, 0.14f, true);
    e->play_animation("walk");
    entities_.push_back(std::move(e));
}

void Game::fire_projectile(float dt, const GameContext& ctx) {
    fire_cooldown_ -= dt;
    bool want_fire =
        (ctx.keys && ctx.keys[SDL_SCANCODE_SPACE]) ||
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);
    // Smoke-test auto-fire so combat + pickups are exercised headlessly.
    if ((smoke_test_ || screenshot_mode_) && !want_fire) want_fire = true;
    if (!want_fire || fire_cooldown_ > 0.0f) return;

    // Use the player's current weapon profile (one firing path, no switches).
    const WeaponSpec w = player_->current_weapon();
    if (!player_->consume_ammo()) return;
    fire_cooldown_ = w.cooldown;

    // Aim direction comes from the shared aim state computed in update()
    // (smoke targets the nearest enemy; otherwise mouse or movement).
    const Vec2 aim = player_->facing();
    const float dx = aim.x;
    const float dy = aim.y;

    // Emit N projectiles spread across the fire cone.
    const int n = w.projectile_count;
    const float spread_rad = w.spread_degrees * (static_cast<float>(M_PI) / 180.0f);
    for (int i = 0; i < n; ++i) {
        float angle = 0.0f;
        if (n > 1) angle = -spread_rad * 0.5f + spread_rad * (i / (n - 1.0f));
        const float ca = std::cos(angle);
        const float sa = std::sin(angle);
        const float dirx = dx * ca - dy * sa;
        const float diry = dx * sa + dy * ca;
        auto proj = std::make_unique<Projectile>(
            player_->pos.x, player_->pos.y,
            dirx * w.projectile_speed, diry * w.projectile_speed, w.damage);
        proj->set_texture(projectile_tex_.get());
        entities_.push_back(std::move(proj));
    }
}

void Game::check_collisions() {
    // Projectile vs enemy: the projectile is destroyed and the enemy takes
    // hit damage from the weapon. Dead enemies may drop an item.
    for (auto& a : entities_) {
        auto* proj = dynamic_cast<Projectile*>(a.get());
        if (!proj || !proj->alive) continue;
        for (auto& b : entities_) {
            auto* en = dynamic_cast<Enemy*>(b.get());
            if (!en || !en->alive) continue;
            if (entities_overlap(*proj, *en)) {
                proj->alive = false;
                const Vec2 drop_pos = en->pos;
                en->damage(proj->damage_amount);
                if (!en->alive) {
                    ++score_;  // count the kill
                    maybe_drop_item(drop_pos);
                }
                break;
            }
        }
    }

    // Enemy vs player: contact damage on a short cooldown.
    if (invuln_timer_ <= 0.0f && player_->alive) {
        for (auto& e : entities_) {
            auto* en = dynamic_cast<Enemy*>(e.get());
            if (!en || !en->alive) continue;
            if (entities_overlap(*en, *player_)) {
                player_->damage(1);
                invuln_timer_ = kInvulnDuration;
                if (!player_->alive) game_over_ = true;
                break;
            }
        }
    }
}

void Game::check_item_pickups() {
    if (!player_->alive) return;
    for (auto& e : entities_) {
        auto* item = dynamic_cast<Item*>(e.get());
        if (!item || !item->alive) continue;
        if (entities_overlap(*item, *player_)) {
            item->on_pickup(*this, *player_);
            item->alive = false;
            ++pickups_collected_;
        }
    }
}

void Game::maybe_drop_item(Vec2 pos) {
    const int r = std::rand() % 100;
    if (r < 15) {
        auto it = std::make_unique<HealthPickup>(pos.x, pos.y);
        it->set_texture(health_tex_.get());
        entities_.push_back(std::move(it));
    } else if (r < 25) {
        // Only drop real upgrades, never the default pistol.
        const WeaponKind kinds[2] = {WeaponKind::Shotgun,
                                    WeaponKind::MachineGun};
        const WeaponKind k = kinds[std::rand() % 2];
        const int ammo = weapon_spec(k).ammo;
        auto it = std::make_unique<WeaponPickup>(pos.x, pos.y, k, ammo);
        it->set_texture(weapon_pickup_tex(k));
        entities_.push_back(std::move(it));
    }
}

void Game::spawn_floor_item(const GameContext& ctx) {
    int count = 0;
    for (const auto& e : entities_)
        if (dynamic_cast<Item*>(e.get())) ++count;
    if (count >= kMaxFloorItems) return;

    const float x = 60.0f + static_cast<float>(
                               std::rand() % static_cast<int>(ctx.world_w - 120));
    const float y = 60.0f + static_cast<float>(
                               std::rand() % static_cast<int>(ctx.world_h - 120));
    if (std::rand() % 2 == 0) {
        auto it = std::make_unique<HealthPickup>(x, y);
        it->set_texture(health_tex_.get());
        entities_.push_back(std::move(it));
    } else {
        const WeaponKind kinds[2] = {WeaponKind::Shotgun,
                                      WeaponKind::MachineGun};
        const WeaponKind k = kinds[std::rand() % 2];
        const int ammo = weapon_spec(k).ammo;
        auto it = std::make_unique<WeaponPickup>(x, y, k, ammo);
        it->set_texture(weapon_pickup_tex(k));
        entities_.push_back(std::move(it));
    }
}

void Game::spawn_item_on_player() {
    // Alternate health and weapon pickups directly on the player.
    if (smoke_frame_ % 180 == 0) {
        auto it = std::make_unique<HealthPickup>(player_->pos.x, player_->pos.y);
        it->set_texture(health_tex_.get());
        entities_.push_back(std::move(it));
    } else {
        const WeaponKind kinds[2] = {WeaponKind::Shotgun,
                                    WeaponKind::MachineGun};
        const WeaponKind k = kinds[std::rand() % 2];
        const int ammo = weapon_spec(k).ammo;
        auto it =
            std::make_unique<WeaponPickup>(player_->pos.x, player_->pos.y,
                                           k, ammo);
        it->set_texture(weapon_pickup_tex(k));
        entities_.push_back(std::move(it));
    }
}

Texture* Game::weapon_pickup_tex(WeaponKind k) {
    switch (k) {
        case WeaponKind::Shotgun: return weapon_tex_shotgun_.get();
        case WeaponKind::MachineGun: return weapon_tex_machinegun_.get();
        case WeaponKind::Pistol:
        default: return weapon_tex_pistol_.get();
    }
}

void Game::render() {
    int win_w = kWindowWidth;
    int win_h = kWindowHeight;
    SDL_GetWindowSize(window_, &win_w, &win_h);
    const float ww = static_cast<float>(win_w);
    const float wh = static_cast<float>(win_h);

    const Level& lvl = current_level();
    SDL_SetRenderDrawColor(renderer_, lvl.floor.r, lvl.floor.g, lvl.floor.b,
                          SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer_);

    // Draw the scene tileset (LevelEdit++ ".mx"). On load failure the floor
    // stays the solid fallback color above.
    if (tilemap_.loaded()) tilemap_.render(renderer_);

    if (state_ == GameState::MainMenu) {
        menu_.render(font_, "BOXDEAD", ww, wh);
        SDL_RenderPresent(renderer_);
        return;
    }
    if (state_ == GameState::Options) {
        menu_.render(font_, "OPTIONS", ww, wh);
        SDL_RenderPresent(renderer_);
        return;
    }

    // Playing / GameOver: world + HUD.
    const bool flashing = invuln_timer_ > 0.0f &&
                          static_cast<int>(invuln_timer_ * 10.0f) % 2 == 0;
    if (flashing) player_->set_color_override(SDL_Color{220, 60, 60, 255});

    // Draw entities back-to-front by ground position so closer (lower)
    // characters correctly overlap further (higher) ones in the iso view.
    std::vector<Entity*> order;
    order.reserve(entities_.size());
    for (const auto& e : entities_) order.push_back(e.get());
    std::sort(order.begin(), order.end(), [](const Entity* a, const Entity* b) {
        return (a->pos.y + a->size.y * 0.5f) < (b->pos.y + b->size.y * 0.5f);
    });
    for (const auto* e : order) e->render(renderer_);

    if (flashing) player_->set_color_override(SDL_Color{255, 255, 255, 255});

    render_hud();

    // Level banner (shown briefly after entering a scene).
    if (banner_timer_ > 0.0f) {
        const std::string label = std::string(current_level().name) +
                                  "  -  Level " + std::to_string(level_ + 1);
        const float tw = static_cast<float>(font_.text_width(label));
        const float x = (ww - tw) * 0.5f;
        const float y = wh * 0.22f;
        const float alpha =
            std::min(1.0f, banner_timer_ * 1.5f) * 255.0f;  // fades out at the end
        font_.draw(label, x, y,
                   SDL_Color{235, 235, 235, static_cast<Uint8>(alpha)});
    }

    // Level-transition fade overlay (covers everything during the swap).
    if (transition_timer_ > 0.0f) {
        // 0 -> midpoint: fade to black; midpoint -> end: fade back in.
        const float t = 1.0f - (transition_timer_ / kTransitionDur);
        const float a = 1.0f - std::abs(2.0f * t - 1.0f);  // triangle, peaks at 0.5
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0,
                               static_cast<Uint8>(a * 255.0f));
        SDL_RenderFillRect(renderer_, nullptr);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    }

    SDL_RenderPresent(renderer_);
}

void Game::render_hud() {
    // Health bar at the top-left.
    constexpr float bar_x = 16.0f;
    constexpr float bar_y = 44.0f;
    constexpr float bar_w = 180.0f;
    constexpr float bar_h = 18.0f;
    const int hp = std::clamp(player_->health, 0, kPlayerMaxHp);
    const float fill_w = bar_w * (static_cast<float>(hp) / kPlayerMaxHp);

    font_.draw("HP", bar_x, 16.0f);

    // Background.
    SDL_SetRenderDrawColor(renderer_, 60, 60, 70, SDL_ALPHA_OPAQUE);
    SDL_FRect bg{bar_x, bar_y, bar_w, bar_h};
    SDL_RenderFillRect(renderer_, &bg);
    // Fill.
    SDL_SetRenderDrawColor(renderer_, 220, 45, 45, SDL_ALPHA_OPAQUE);
    SDL_FRect fill{bar_x, bar_y, fill_w, bar_h};
    SDL_RenderFillRect(renderer_, &fill);
    // Border.
    SDL_SetRenderDrawColor(renderer_, 200, 200, 210, SDL_ALPHA_OPAQUE);
    SDL_FRect border{bar_x, bar_y, bar_w, bar_h};
    SDL_RenderRect(renderer_, &border);

    // Score + wave, top-right.
    font_.draw("Kills: " + std::to_string(score_), 980.0f, 16.0f);
    font_.draw("Wave: " + std::to_string(wave_), 980.0f, 40.0f);

    // Inventory: one row per owned weapon slot. The selected weapon is
    // highlighted; empty (finite) weapons are dimmed. Keys 1/2/3 select,
    // Q/E cycle.
    static const char* kSlotKey[3] = {"1", "2", "3"};
    float iy = 64.0f;
    for (int i = 0; i < Player::kSlotCount; ++i) {
        const WeaponKind wk = static_cast<WeaponKind>(i);
        if (!player_->owns(wk)) continue;  // hide unowned slots
        const std::string name = weapon_spec(wk).name;
        const int ammo = player_->ammo(wk);
        std::string label = std::string("[") + kSlotKey[i] + "] " + name;
        if (ammo < 0) label += " (inf)";
        else if (ammo == 0) label += " (empty)";
        else label += " x" + std::to_string(ammo);
        const bool sel = (wk == player_->current_kind());
        const bool empty = (ammo == 0);
        SDL_Color c = sel ? SDL_Color{255, 220, 60, 255}
                          : (empty ? SDL_Color{110, 110, 120, 255}
                                   : SDL_Color{200, 200, 210, 255});
        if (sel) label = "> " + label;
        font_.draw(label, 980.0f, iy, c);
        iy += 22.0f;
    }

    // Pickup toast, centered near the top.
    if (pickup_toast_timer_ > 0.0f && !pickup_toast_.empty()) {
        const int tw = font_.text_width(pickup_toast_);
        font_.draw(pickup_toast_, (1280 - tw) * 0.5f, 110.0f,
                   SDL_Color{255, 230, 120, 255});
    }

    if (game_over_) {
        // Dim the screen.
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 160);
        const SDL_FRect dim{0.0f, 0.0f, 1280.0f, 720.0f};
        SDL_RenderFillRect(renderer_, &dim);
        // GAME OVER + score + restart prompt, centered.
        font_.draw("GAME OVER", 560.0f, 320.0f,
                   SDL_Color{220, 45, 45, 255});
        font_.draw("Kills: " + std::to_string(score_), 568.0f, 360.0f);
        font_.draw("Press R to restart, Esc for menu", 500.0f, 400.0f);
    }
}

void Game::show_toast(const std::string& text) {
    pickup_toast_ = text;
    pickup_toast_timer_ = 1.2f;
}

void Game::reset() {
    entities_.clear();
    spawn_timer_ = 0.0f;
    fire_cooldown_ = 0.0f;
    invuln_timer_ = 0.0f;
    game_over_ = false;
    score_ = 0;
    wave_ = 1;
    wave_timer_ = 0.0f;
    item_spawn_timer_ = 0.0f;
    pickups_collected_ = 0;
    max_enemy_anim_frame_ = -1;
    pickup_toast_timer_ = 0.0f;
    pickup_toast_.clear();
    state_ = GameState::Playing;

    auto player =
        std::make_unique<Player>(kWindowWidth * 0.5f, kWindowHeight * 0.5f);
    player->add_animation("walk", player_walk_sheet_.get(), 4, 32, 0.12f, true);
    player->add_animation("idle", player_idle_sheet_.get(), 2, 32, 0.28f, true);
    player->play_animation("idle");
    player_ = player.get();
    apply_gun_textures(*player_);
    entities_.push_back(std::move(player));

    // Fresh run: back to the first scene, no transition in flight.
    level_ = 0;
    transition_timer_ = 0.0f;
    pending_level_ = -1;
    banner_timer_ = 1.6f;  // greet the player with the level name
    load_current_tilemap();
}

const Game::Level& Game::current_level() const {
    return kLevels[static_cast<size_t>(level_) % kLevelCount];
}

int Game::level_for_wave(int wave) const {
    // Waves are 1-based; every kWavesPerLevel waves advances to the next map.
    return ((wave - 1) / kWavesPerLevel) % kLevelCount;
}

void Game::begin_level_transition(int new_level) {
    pending_level_ = new_level;
    transition_timer_ = kTransitionDur;
}

void Game::apply_level_swap(const GameContext& ctx) {
    level_ = pending_level_;
    pending_level_ = -1;
    banner_timer_ = 2.2f;
    // New map: clear enemies and projectiles, recentre the player, load the
    // new scene's tileset.
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                       [](const std::unique_ptr<Entity>& e) {
                           return dynamic_cast<Enemy*>(e.get()) ||
                                  dynamic_cast<Projectile*>(e.get());
                       }),
        entities_.end());
    if (player_) {
        player_->pos = {ctx.world_w * 0.5f, ctx.world_h * 0.5f};
    }
    spawn_timer_ = 0.0f;
    load_current_tilemap();
}

void Game::load_current_tilemap() {
    // Load the ".mx" tileset for the current scene; if it fails the floor
    // falls back to the solid level color (render() checks tilemap_.loaded()).
    tilemap_.clear();
    tilemap_.load(renderer_, current_level().map_path);
}

void Game::apply_gun_textures(Player& p) {
    p.set_gun_texture(WeaponKind::Pistol, gun_hand_tex_[0].get());
    p.set_gun_texture(WeaponKind::Shotgun, gun_hand_tex_[1].get());
    p.set_gun_texture(WeaponKind::MachineGun, gun_hand_tex_[2].get());
}

void Game::capture_screenshot() { capture_screenshot_to(screenshot_path_); }

void Game::capture_screenshot_to(const std::string& path) {
    SDL_Surface* surf = SDL_RenderReadPixels(renderer_, nullptr);
    if (!surf) return;
    SDL_SaveBMP(surf, path.c_str());
    SDL_DestroySurface(surf);
}

void Game::shutdown() {
    entities_.clear();
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    TTF_Quit();
}

}  // namespace bd
