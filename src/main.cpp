// BoxDead - entry point. Initializes SDL, runs the Game, cleans up.
#include "boxdead/game.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    const bool smoke_test =
        argc > 1 && std::string_view(argv[1]) == "--smoke-test";

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    int rc = 0;
    {
        // Game is destroyed before SDL_Quit so it can release SDL resources.
        bd::Game game(smoke_test);
        if (!game.init()) {
            rc = 1;
        } else {
            game.run();
        }
    }

    SDL_Quit();
    return rc;
}
