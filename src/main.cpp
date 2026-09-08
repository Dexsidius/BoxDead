// BoxDead - SDL3 base template
// Launchable SDL3 window with frame-rate-independent timing and keyboard movement.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "BoxDead";

// Player movement speed in pixels per second.
constexpr float kPlayerSpeed = 320.0f;
constexpr float kPlayerSize = 80.0f;

struct SdlInitGuard {
    ~SdlInitGuard() { SDL_Quit(); }
};

// A simple movable player rendered as a colored square.
struct Player {
    float x = 0.0f;
    float y = 0.0f;

    void center(float w, float h) {
        x = (w - kPlayerSize) * 0.5f;
        y = (h - kPlayerSize) * 0.5f;
    }
};

// Read directional input into a normalized vector (units per second).
void read_input(const bool* keys, float& dx, float& dy) {
    dx = 0.0f;
    dy = 0.0f;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) dx -= 1.0f;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) dx += 1.0f;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) dy -= 1.0f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) dy += 1.0f;
    // Normalize diagonals so diagonal speed isn't ~1.41x faster.
    if (dx != 0.0f && dy != 0.0f) {
        const float inv = 1.0f / std::sqrt(2.0f);
        dx *= inv;
        dy *= inv;
    }
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

    // Center the player in the initial drawable area.
    int draw_w = kWindowWidth;
    int draw_h = kWindowHeight;
    SDL_GetCurrentRenderOutputSize(renderer.get(), &draw_w, &draw_h);
    Player player;
    player.center(static_cast<float>(draw_w), static_cast<float>(draw_h));

    // Frame-rate-independent loop using high-resolution ticks (nanoseconds).
    Uint64 last_time = SDL_GetTicksNS();
    bool running = true;

    while (running) {
        const Uint64 now = SDL_GetTicksNS();
        // Delta time in seconds, clamped to avoid the "spiral of death"
        // after a long stall (e.g. window dragged / breakpoint).
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

        // Continuous keyboard movement (polled state, not per-press events).
        const bool* keys = SDL_GetKeyboardState(nullptr);
        float dx = 0.0f;
        float dy = 0.0f;
        read_input(keys, dx, dy);
        player.x += dx * kPlayerSpeed * dt;
        player.y += dy * kPlayerSpeed * dt;

        // Keep the player inside the current drawable area (resizable window).
        SDL_GetCurrentRenderOutputSize(renderer.get(), &draw_w, &draw_h);
        const float max_x = static_cast<float>(draw_w) - kPlayerSize;
        const float max_y = static_cast<float>(draw_h) - kPlayerSize;
        player.x = std::clamp(player.x, 0.0f, max_x);
        player.y = std::clamp(player.y, 0.0f, max_y);

        // Render
        SDL_SetRenderDrawColor(renderer.get(), 18, 18, 24, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer.get());

        SDL_SetRenderDrawColor(renderer.get(), 220, 45, 45, SDL_ALPHA_OPAQUE);
        const SDL_FRect player_rect{player.x, player.y, kPlayerSize,
                                    kPlayerSize};
        SDL_RenderFillRect(renderer.get(), &player_rect);

        SDL_RenderPresent(renderer.get());

        if (smoke_test) running = false;  // one frame then exit, for CI
        SDL_Delay(1);                     // yield CPU, vsync handles the cap
    }

    return 0;
}
