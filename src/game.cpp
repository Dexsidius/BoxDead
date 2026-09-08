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

        if (smoke_test_) running = false;  // one frame, for CI
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
    for (auto& a : entities_) {
        auto* proj = dynamic_cast<Projectile*>(a.get());
        if (!proj || !proj->alive) continue;
        for (auto& b : entities_) {
            auto* en = dynamic_cast<Enemy*>(b.get());
            if (!en || !en->alive) continue;
            if (entities_overlap(*proj, *en)) {
                proj->alive = false;
                en->alive = false;
                break;
            }
        }
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer_, 18, 18, 24, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer_);
    for (const auto& e : entities_) e->render(renderer_);
    SDL_RenderPresent(renderer_);
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
