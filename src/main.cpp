// BoxDead - SDL3 base template
// Launchable SDL3 window with a sprite + entity system: a controllable
// player and enemies that spawn at the edges and chase the player.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include "entity.hpp"
#include "sprite.hpp"

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "BoxDead";

constexpr float kSpawnInterval = 1.2f;   // seconds between enemy spawns
constexpr size_t kMaxEnemies = 50;

struct SdlInitGuard {
    ~SdlInitGuard() { SDL_Quit(); }
};

// Spawn an enemy at a random point along the screen edge.
std::unique_ptr<Enemy> spawn_edge_enemy(float world_w, float world_h,
                                        Texture* tex) {
    const int edge = std::rand() % 4;
    float x = 0.0f;
    float y = 0.0f;
    const float margin = 20.0f;
    switch (edge) {
        case 0:  // top
            x = static_cast<float>(std::rand() % static_cast<int>(world_w));
            y = margin;
            break;
        case 1:  // bottom
            x = static_cast<float>(std::rand() % static_cast<int>(world_w));
            y = world_h - margin;
            break;
        case 2:  // left
            x = margin;
            y = static_cast<float>(std::rand() % static_cast<int>(world_h));
            break;
        default:  // right
            x = world_w - margin;
            y = static_cast<float>(std::rand() % static_cast<int>(world_h));
            break;
    }
    auto e = std::make_unique<Enemy>(x, y);
    e->set_texture(tex);
    return e;
}
}  // namespace

int main(int argc, char* argv[]) {
    const bool smoke_test =
        argc > 1 && std::string_view(argv[1]) == "--smoke-test";

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SdlInitGuard quit_guard;

    using WindowPtr =
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    using RendererPtr =
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

    WindowPtr window(
        SDL_CreateWindow(kWindowTitle, kWindowWidth, kWindowHeight,
                         SDL_WINDOW_RESIZABLE),
        SDL_DestroyWindow);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return 1;
    }

    RendererPtr renderer(SDL_CreateRenderer(window.get(), nullptr),
                         SDL_DestroyRenderer);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        return 1;
    }

    // Build procedural sprite textures (swap make_solid_sprite_texture for
    // Texture::load(...) to use real art assets).
    auto player_tex = make_solid_sprite_texture(
        renderer.get(), SDL_Color{60, 160, 255, 255}, 32);
    auto enemy_tex = make_solid_sprite_texture(
        renderer.get(), SDL_Color{220, 45, 45, 255}, 32);

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Entities: player first, enemies after.
    std::vector<std::unique_ptr<Entity>> entities;
    auto player = std::make_unique<Player>(kWindowWidth * 0.5f,
                                           kWindowHeight * 0.5f);
    player->set_texture(player_tex.get());
    Player* player_ptr = player.get();
    entities.push_back(std::move(player));

    int draw_w = kWindowWidth;
    int draw_h = kWindowHeight;
    SDL_GetCurrentRenderOutputSize(renderer.get(), &draw_w, &draw_h);

    float spawn_timer = 0.0f;
    Uint64 last_time = SDL_GetTicksNS();
    bool running = true;

    while (running) {
        const Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - last_time) / 1.0e9f;
        last_time = now;
        if (dt > 0.25f) dt = 0.25f;

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

        SDL_GetCurrentRenderOutputSize(renderer.get(), &draw_w, &draw_h);

        GameContext ctx;
        ctx.keys = SDL_GetKeyboardState(nullptr);
        ctx.world_w = static_cast<float>(draw_w);
        ctx.world_h = static_cast<float>(draw_h);

        // Update the player first so enemies can chase its new position.
        player_ptr->update(dt, ctx);
        ctx.player_pos = player_ptr->pos;

        // Spawn enemies on a timer.
        spawn_timer += dt;
        if (spawn_timer >= kSpawnInterval) {
            spawn_timer = 0.0f;
            size_t enemy_count = 0;
            for (const auto& e : entities)
                if (dynamic_cast<Enemy*>(e.get())) ++enemy_count;
            if (enemy_count < kMaxEnemies) {
                entities.push_back(spawn_edge_enemy(
                    ctx.world_w, ctx.world_h, enemy_tex.get()));
            }
        }

        // Update everything except the player (already updated).
        for (auto& e : entities) {
            if (e.get() != player_ptr) e->update(dt, ctx);
        }

        // Remove dead entities (currently none die; hook for combat later).
        entities.erase(
            std::remove_if(entities.begin(), entities.end(),
                           [](const std::unique_ptr<Entity>& e) {
                               return !e->alive;
                           }),
            entities.end());

        // Render
        SDL_SetRenderDrawColor(renderer.get(), 18, 18, 24, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer.get());
        for (const auto& e : entities) e->render(renderer.get());
        SDL_RenderPresent(renderer.get());

        if (smoke_test) running = false;
        SDL_Delay(1);
    }

    return 0;
}
