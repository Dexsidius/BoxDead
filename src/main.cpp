// BoxDead - SDL3 base template
// A minimal, launchable SDL3 window with a clean event loop and shutdown.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>
#include <memory>
#include <string_view>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "BoxDead";

// Runs SDL_Quit() at scope exit so we never leak SDL global state,
// even on early-return error paths.
struct SdlInitGuard {
    ~SdlInitGuard() { SDL_Quit(); }
};
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

    bool running = true;
    while (running) {
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

        // Clear to a dark background.
        SDL_SetRenderDrawColor(renderer.get(), 18, 18, 24, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer.get());

        // Placeholder player square.
        SDL_SetRenderDrawColor(renderer.get(), 220, 45, 45, SDL_ALPHA_OPAQUE);
        const SDL_FRect player{600.0f, 320.0f, 80.0f, 80.0f};
        SDL_RenderFillRect(renderer.get(), &player);

        SDL_RenderPresent(renderer.get());

        if (smoke_test) running = false;  // one frame then exit, for CI
        SDL_Delay(16);                    // ~60 fps cap
    }

    return 0;
}
