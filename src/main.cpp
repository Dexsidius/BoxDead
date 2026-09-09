// BoxDead - entry point. Initializes SDL, runs the Game, cleans up.
#include "boxdead/game.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    bool smoke_test = false;
    bool menu_shot = false;
    std::string screenshot_path;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--smoke-test") smoke_test = true;
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshot_path = argv[++i];
        else if (arg == "--menu-shot" && i + 1 < argc) {
            menu_shot = true;
            screenshot_path = argv[++i];
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    int rc = 0;
    {
        bd::Game game(smoke_test, screenshot_path, menu_shot);
        if (!game.init()) {
            rc = 1;
        } else {
            game.run();
        }
    }

    SDL_Quit();
    return rc;
}
