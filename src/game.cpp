// BoxDead - Game implementation: owns the main loop, spawning, firing,
// collisions, and rendering. main.cpp is a thin bootstrap around this class.
#include "boxdead/game.hpp"

#include "boxdead/sprite.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>

namespace bd {

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "BoxDead";
constexpr float kSpawnInterval = 1.2f;    // seconds between enemy spawns
constexpr size_t kMaxEnemies = 50;
constexpr float kFireInterval = 0.18f;    // seconds between shots
constexpr float kProjectileSpeed = 600.0f;
constexpr float kInvulnDuration = 1.2f;  // seconds of i-frames after a hit
}  // namespace

Game::Game(bool smoke_test) : smoke_test_(smoke_test) {}

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

    player_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{60, 160, 255, 255}, 32);
    enemy_tex_ =
        make_solid_sprite_texture(renderer_, SDL_Color{220, 45, 45, 255}, 32);
    projectile_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{255, 220, 60, 255}, 8);

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    auto player =
        std::make_unique<Player>(kWindowWidth * 0.5f, kWindowHeight * 0.5f);
    player->set_texture(player_tex_.get());
    player_ = player.get();
    entities_.push_back(std::move(player));
    return true;
}

void Game::run() {
    Uint64 last_time = SDL_GetTicksNS();
    bool running = true;
    while (running) {
        const Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - last_time) / 1.0e9f;
        last_time = now;
        if (dt > 0.25f) dt = 0.25f;  // avoid spiral of death after stalls

        handle_events(running);
        update(dt);
        render();

        if (smoke_test_ || game_over_) running = false;  // one frame, for CI
        SDL_Delay(1);                       // yield CPU
    }
}

void Game::handle_events(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) running = false;
                break;
            default:
                break;
        }
    }
}

void Game::update(float dt) {
    int draw_w = kWindowWidth;
    int draw_h = kWindowHeight;
    SDL_GetCurrentRenderOutputSize(renderer_, &draw_w, &draw_h);

    GameContext ctx;
    ctx.keys = SDL_GetKeyboardState(nullptr);
    ctx.world_w = static_cast<float>(draw_w);
    ctx.world_h = static_cast<float>(draw_h);

    // Update the player first so enemies can chase its new position.
    player_->update(dt, ctx);
    ctx.player_pos = player_->pos;

    // When the player is dead, freeze the world (no spawns, movement, etc.).
    if (game_over_) return;

    // Spawn enemies on a timer.
    spawn_timer_ += dt;
    if (spawn_timer_ >= kSpawnInterval) {
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

    // Tick the player's post-hit invulnerability window.
    if (invuln_timer_ > 0.0f) {
        invuln_timer_ -= dt;
        if (invuln_timer_ < 0.0f) invuln_timer_ = 0.0f;
    }

    check_collisions();

    // Reap dead entities (projectiles that hit/expired and dead enemies).
    entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                   [](const std::unique_ptr<Entity>& e) {
                                       return !e->alive;
                                   }),
                    entities_.end());
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
    auto e = std::make_unique<Enemy>(x, y);
    e->set_texture(enemy_tex_.get());
    entities_.push_back(std::move(e));
}

void Game::fire_projectile(float dt, const GameContext& ctx) {
    fire_cooldown_ -= dt;
    const bool want_fire =
        (ctx.keys && ctx.keys[SDL_SCANCODE_SPACE]) ||
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);
    if (!want_fire || fire_cooldown_ > 0.0f) return;
    fire_cooldown_ = kFireInterval;

    // Scale mouse coords from window space to render output space (HiDPI).
    int win_w = kWindowWidth;
    int win_h = kWindowHeight;
    SDL_GetWindowSize(window_, &win_w, &win_h);
    float mx = 0.0f;
    float my = 0.0f;
    SDL_GetMouseState(&mx, &my);
    const float sx = ctx.world_w / static_cast<float>(win_w);
    const float sy = ctx.world_h / static_cast<float>(win_h);
    float dx = mx * sx - player_->pos.x;
    float dy = my * sy - player_->pos.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) {
        dx = 0.0f;
        dy = -1.0f;
    } else {
        dx /= len;
        dy /= len;
    }
    auto proj = std::make_unique<Projectile>(
        player_->pos.x, player_->pos.y, dx * kProjectileSpeed,
        dy * kProjectileSpeed);
    proj->set_texture(projectile_tex_.get());
    entities_.push_back(std::move(proj));
}

void Game::check_collisions() {
    // Projectile vs enemy: the projectile is destroyed and the enemy takes
    // a hit (dies in one shot for now).
    for (auto& a : entities_) {
        auto* proj = dynamic_cast<Projectile*>(a.get());
        if (!proj || !proj->alive) continue;
        for (auto& b : entities_) {
            auto* en = dynamic_cast<Enemy*>(b.get());
            if (!en || !en->alive) continue;
            if (entities_overlap(*proj, *en)) {
                proj->alive = false;
                en->damage(1);
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

void Game::render() {
    SDL_SetRenderDrawColor(renderer_, 18, 18, 24, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer_);

    // Flash the player red while invulnerable (just took a hit).
    const bool flashing = invuln_timer_ > 0.0f &&
                          static_cast<int>(invuln_timer_ * 10.0f) % 2 == 0;
    if (flashing) player_->set_color_override(SDL_Color{220, 60, 60, 255});

    for (const auto& e : entities_) e->render(renderer_);

    if (flashing) player_->set_color_override(SDL_Color{60, 160, 255, 255});

    render_hud();

    SDL_RenderPresent(renderer_);
}

void Game::render_hud() {
    // Health bar at the top-left.
    constexpr float bar_x = 16.0f;
    constexpr float bar_y = 16.0f;
    constexpr float bar_w = 180.0f;
    constexpr float bar_h = 18.0f;
    constexpr int max_hp = 5;
    const int hp = std::clamp(player_->health, 0, max_hp);
    const float fill_w = bar_w * (static_cast<float>(hp) / max_hp);

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

    if (game_over_) {
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, SDL_ALPHA_OPAQUE);
        const SDL_FRect panel{0.0f, 0.0f, 1280.0f, 720.0f};
        SDL_RenderRect(renderer_, &panel);  // placeholder; text needs SDL_ttf
    }
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
}

}  // namespace bd
